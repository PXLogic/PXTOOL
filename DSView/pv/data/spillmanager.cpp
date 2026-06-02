#include "spillmanager.h"
#include "../log.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <QDir>
#include <QFile>
#include <QFileInfoList>

namespace pv {
namespace data {

SpillManager::SpillManager(uint64_t ram_limit_bytes, uint64_t disk_limit_bytes, const std::string& spill_dir)
    : ram_limit_(ram_limit_bytes)
    , disk_limit_(disk_limit_bytes)
    , spill_dir_(spill_dir)
    , channel_count_(0)
    , ram_usage_(0)
    , disk_usage_(0)
    , stop_io_(false)
    , lru_tick_(0)
    , stat_spilled_(0)
    , stat_bytes_written_(0)
    , stat_lru_hits_(0)
    , stat_lru_misses_(0)
    , stat_queue_peak_(0)
    , stat_stalls_(0)
{
    lru_.resize(LRU_SLOTS);
}

SpillManager::~SpillManager()
{
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        stop_io_ = true;
    }
    queue_cv_.notify_all();
    if (io_thread_.joinable())
        io_thread_.join();

    for (size_t i = 0; i < spill_files_.size(); ++i) {
        if (spill_files_[i]) {
            fclose(spill_files_[i]);
            spill_files_[i] = nullptr;
        }
        if (i < spill_paths_.size() && !spill_paths_[i].empty()) {
            if (QFile::remove(QString::fromStdString(spill_paths_[i])))
                dsv_info("[SpillMgr] Deleted spill file: %s", spill_paths_[i].c_str());
        }
    }
}

bool SpillManager::init_channels(uint16_t channel_count, const std::string& session_id)
{
    channel_count_ = channel_count;
    spill_files_.assign(channel_count_, nullptr);
    spill_paths_.assign(channel_count_, std::string());

    QDir dir(QString::fromStdString(spill_dir_));
    if (!dir.exists() && !dir.mkpath(".")) {
        dsv_err("[SpillMgr] Cannot create spill dir: %s", spill_dir_.c_str());
        return false;
    }

    for (uint16_t ch = 0; ch < channel_count_; ++ch) {
        QString path = dir.absoluteFilePath(
            QString("dsview_spill_%1_ch%2.bin")
                .arg(QString::fromStdString(session_id))
                .arg((unsigned)ch));
        spill_paths_[ch] = path.toStdString();
        spill_files_[ch] = fopen(spill_paths_[ch].c_str(), "w+b");
        if (!spill_files_[ch]) {
            dsv_err("[SpillMgr] Cannot create spill file %s: %s",
                    spill_paths_[ch].c_str(), strerror(errno));
            return false;
        }
        dsv_info("[SpillMgr] Spill file created: %s", spill_paths_[ch].c_str());
    }

    stop_io_ = false;
    io_thread_ = std::thread(&SpillManager::io_thread_func, this);

    dsv_info("[SpillMgr] init channels=%u session=%s ram_limit=%lluMB disk_limit=%lluMB dir=%s",
             (unsigned)channel_count_,
             session_id.c_str(),
             (unsigned long long)(ram_limit_ >> 20),
             (unsigned long long)(disk_limit_ >> 20),
             spill_dir_.c_str());
    return true;
}

bool SpillManager::enqueue_spill(uint16_t channel, uint64_t block_id, const void* data, size_t size)
{
    if (!data || channel >= channel_count_ || disk_full())
        return false;

    WriteJob job;
    job.channel = channel;
    job.block_id = block_id;
    job.data.assign(static_cast<const uint8_t*>(data),
                    static_cast<const uint8_t*>(data) + size);

    std::unique_lock<std::mutex> lk(queue_mutex_);
    if (write_queue_.size() >= static_cast<size_t>(MAX_QUEUE_DEPTH)) {
        stat_stalls_++;
        queue_space_cv_.wait(lk, [this] {
            return stop_io_ || write_queue_.size() < static_cast<size_t>(MAX_QUEUE_DEPTH);
        });
        if (stop_io_)
            return false;
    }

    write_queue_.push_back(std::move(job));
    uint64_t depth = write_queue_.size();
    uint64_t peak = stat_queue_peak_.load(std::memory_order_relaxed);
    while (depth > peak &&
           !stat_queue_peak_.compare_exchange_weak(peak, depth, std::memory_order_relaxed)) {
    }
    lk.unlock();
    queue_cv_.notify_one();
    return true;
}

void SpillManager::drain_completed(std::vector<std::pair<uint16_t, uint64_t>>& out)
{
    std::lock_guard<std::mutex> lk(completed_mutex_);
    out.insert(out.end(), completed_spills_.begin(), completed_spills_.end());
    completed_spills_.clear();
}

const void* SpillManager::load_block(uint16_t channel, uint64_t block_id)
{
    {
        std::lock_guard<std::mutex> lk(lru_mutex_);
        for (auto& e : lru_) {
            if (e.channel == channel && e.block_id == block_id && !e.data.empty()) {
                e.last_tick = ++lru_tick_;
                stat_lru_hits_++;
                return e.data.data();
            }
        }
    }

    stat_lru_misses_++;
    uint64_t offset = 0;
    size_t size = 0;
    {
        std::lock_guard<std::mutex> lk(index_mutex_);
        const uint64_t key = make_key(channel, block_id);
        auto off_it = block_offsets_.find(key);
        auto size_it = block_sizes_.find(key);
        if (off_it == block_offsets_.end() || size_it == block_sizes_.end())
            return nullptr;
        offset = off_it->second;
        size = size_it->second;
    }

    if (channel >= spill_files_.size() || !spill_files_[channel])
        return nullptr;

    std::lock_guard<std::mutex> lk(lru_mutex_);
    CacheEntry* slot = lru_evict_slot();
    slot->channel = channel;
    slot->block_id = block_id;
    slot->data.resize(size);

    size_t read = 0;
    {
        std::lock_guard<std::mutex> file_lk(file_mutex_);
        FILE* f = spill_files_[channel];
        if (fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
            return nullptr;
        read = fread(slot->data.data(), 1, size, f);
    }
    if (read != size) {
        slot->data.clear();
        slot->block_id = UINT64_MAX;
        dsv_err("[SpillMgr] Short read ch%u block#%llu: read %zu of %zu",
                channel, (unsigned long long)block_id, read, size);
        return nullptr;
    }
    slot->last_tick = ++lru_tick_;
    return slot->data.data();
}

bool SpillManager::read_block_into(uint16_t channel, uint64_t block_id, void* out_buf, size_t size)
{
    const void* src = load_block(channel, block_id);
    if (!src || !out_buf)
        return false;

    size_t stored = 0;
    {
        std::lock_guard<std::mutex> lk(index_mutex_);
        auto it = block_sizes_.find(make_key(channel, block_id));
        if (it == block_sizes_.end())
            return false;
        stored = it->second;
    }
    if (size > stored)
        return false;
    memcpy(out_buf, src, size);
    return true;
}

bool SpillManager::is_spilled(uint16_t channel, uint64_t block_id) const
{
    std::lock_guard<std::mutex> lk(index_mutex_);
    return block_offsets_.find(make_key(channel, block_id)) != block_offsets_.end();
}

void SpillManager::notify_ram_usage(uint64_t bytes_in_ram)
{
    ram_usage_ = bytes_in_ram;
}

bool SpillManager::should_spill() const
{
    return ram_limit_ > 0 && ram_usage_.load(std::memory_order_relaxed) > ram_limit_;
}

bool SpillManager::disk_full() const
{
    return disk_limit_ > 0 && disk_usage_.load(std::memory_order_relaxed) >= disk_limit_;
}

void SpillManager::log_stats_summary()
{
    dsv_info("[SpillMgr] Session end: spilled=%llu blocks written=%lluMB ram_usage=%lluMB limit=%lluMB lru_hit=%llu lru_miss=%llu queue_peak=%llu stalls=%llu",
             (unsigned long long)stat_spilled_.load(),
             (unsigned long long)(stat_bytes_written_.load() >> 20),
             (unsigned long long)(ram_usage_.load() >> 20),
             (unsigned long long)(ram_limit_ >> 20),
             (unsigned long long)stat_lru_hits_.load(),
             (unsigned long long)stat_lru_misses_.load(),
             (unsigned long long)stat_queue_peak_.load(),
             (unsigned long long)stat_stalls_.load());
}

void SpillManager::io_thread_func()
{
    while (true) {
        WriteJob job;
        {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_cv_.wait(lk, [this] {
                return stop_io_ || !write_queue_.empty();
            });
            if (write_queue_.empty() && stop_io_)
                break;
            job = std::move(write_queue_.front());
            write_queue_.pop_front();
        }
        queue_space_cv_.notify_one();
        write_block_to_file(job);
    }
}

bool SpillManager::write_block_to_file(WriteJob& job)
{
    if (job.channel >= spill_files_.size() || !spill_files_[job.channel])
        return false;

    long offset = 0;
    size_t written = 0;
    {
        std::lock_guard<std::mutex> file_lk(file_mutex_);
        FILE* f = spill_files_[job.channel];
        if (fseek(f, 0, SEEK_END) != 0)
            return false;

        offset = ftell(f);
        if (offset < 0)
            return false;

        written = fwrite(job.data.data(), 1, job.data.size(), f);
        fflush(f);
    }
    if (written != job.data.size()) {
        dsv_err("[SpillMgr] Short write ch%u block#%llu: wrote %zu of %zu",
                job.channel, (unsigned long long)job.block_id, written, job.data.size());
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(index_mutex_);
        const uint64_t key = make_key(job.channel, job.block_id);
        block_offsets_[key] = static_cast<uint64_t>(offset);
        block_sizes_[key] = written;
    }

    disk_usage_ += written;
    stat_spilled_++;
    stat_bytes_written_ += written;

    {
        std::lock_guard<std::mutex> lk(completed_mutex_);
        completed_spills_.push_back(std::make_pair(job.channel, job.block_id));
    }
    return true;
}

SpillManager::CacheEntry* SpillManager::lru_evict_slot()
{
    auto best = lru_.begin();
    for (auto it = lru_.begin(); it != lru_.end(); ++it) {
        if (it->data.empty())
            return &(*it);
        if (it->last_tick < best->last_tick)
            best = it;
    }
    return &(*best);
}

void SpillManager::cleanup_stale_files(const std::string& spill_dir)
{
    QDir dir(QString::fromStdString(spill_dir));
    if (!dir.exists())
        return;

    QFileInfoList files = dir.entryInfoList(QStringList() << "dsview_spill_*",
                                            QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &fi : files) {
        QFile::remove(fi.absoluteFilePath());
        dsv_info("[SpillMgr] Cleaned stale file: %s", fi.absoluteFilePath().toUtf8().constData());
    }
}

} // namespace data
} // namespace pv
