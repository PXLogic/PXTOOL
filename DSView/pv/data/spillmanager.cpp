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
    , active_writes_(0)
    , lru_tick_(0)
    , stat_spilled_(0)
    , stat_bytes_written_(0)
    , stat_lru_hits_(0)
    , stat_lru_misses_(0)
    , stat_queue_peak_(0)
    , stat_stalls_(0)
    , speed_window_start_ms_(0)
    , speed_window_bytes_(0)
    , speed_bytes_per_sec_(0)
    , first_write_ms_(0)
    , last_progress_log_ms_(0)
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
    file_buffers_.clear();
    file_buffers_.resize(channel_count_);

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
        file_buffers_[ch].resize(FILE_BUFFER_BYTES);
        if (setvbuf(spill_files_[ch], file_buffers_[ch].data(), _IOFBF, file_buffers_[ch].size()) != 0)
            dsv_warn("[SpillMgr] setvbuf failed for %s", spill_paths_[ch].c_str());
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
    job.data = static_cast<const uint8_t*>(data);
    job.size = size;

    std::unique_lock<std::mutex> lk(queue_mutex_);
    if (write_queue_.size() >= static_cast<size_t>(MAX_QUEUE_DEPTH)) {
        stat_stalls_++;
        uint64_t stalls = stat_stalls_.load(std::memory_order_relaxed);
        if (stalls <= 5 || (stalls % 100) == 0) {
            dsv_warn("[SpillMgr] Write queue full: depth=%zu active=%d stalls=%llu",
                     write_queue_.size(), active_writes_, (unsigned long long)stalls);
        }
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

void SpillManager::wait_for_idle()
{
    std::unique_lock<std::mutex> lk(queue_mutex_);
    while (!write_queue_.empty() || active_writes_ != 0) {
        dsv_info("[SpillMgr] wait_for_idle: queue=%zu active=%d",
                 write_queue_.size(), active_writes_);
        idle_cv_.wait_for(lk, std::chrono::milliseconds(500));
    }
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

uint64_t SpillManager::ram_usage_bytes() const
{
    return ram_usage_.load(std::memory_order_relaxed);
}

uint64_t SpillManager::disk_usage_bytes() const
{
    return disk_usage_.load(std::memory_order_relaxed);
}

uint64_t SpillManager::recent_write_bytes_per_sec() const
{
    return speed_bytes_per_sec_.load(std::memory_order_relaxed);
}

void SpillManager::log_stats_summary()
{
    const uint64_t first_ms = first_write_ms_.load(std::memory_order_relaxed);
    const uint64_t last_ms = last_progress_log_ms_.load(std::memory_order_relaxed);
    const uint64_t elapsed_ms = (first_ms > 0 && last_ms > first_ms) ? (last_ms - first_ms) : 0;
    const uint64_t bytes_written = stat_bytes_written_.load(std::memory_order_relaxed);
    const uint64_t avg_bps = elapsed_ms > 0 ? (bytes_written * 1000 / elapsed_ms) : 0;

    dsv_info("[SpillMgr] Session end: spilled=%llu blocks written=%lluMB ram_usage=%lluMB limit=%lluMB avg_write=%lluMB/s last_write=%lluMB/s lru_hit=%llu lru_miss=%llu queue_peak=%llu stalls=%llu",
             (unsigned long long)stat_spilled_.load(),
             (unsigned long long)(bytes_written >> 20),
             (unsigned long long)(ram_usage_.load() >> 20),
             (unsigned long long)(ram_limit_ >> 20),
             (unsigned long long)(avg_bps >> 20),
             (unsigned long long)(speed_bytes_per_sec_.load(std::memory_order_relaxed) >> 20),
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
            active_writes_++;
        }
        queue_space_cv_.notify_one();
        write_block_to_file(job);
        {
            std::lock_guard<std::mutex> lk(queue_mutex_);
            active_writes_--;
            if (write_queue_.empty() && active_writes_ == 0)
                idle_cv_.notify_all();
        }
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

        written = fwrite(job.data, 1, job.size, f);
    }
    if (written != job.size) {
        dsv_err("[SpillMgr] Short write ch%u block#%llu: wrote %zu of %zu",
                job.channel, (unsigned long long)job.block_id, written, job.size);
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

    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    uint64_t expected_first = 0;
    if (first_write_ms_.compare_exchange_strong(expected_first, now_ms, std::memory_order_relaxed)) {
        dsv_info("[SpillMgr] First spill write: ch%u block#%llu size=%zu",
                 job.channel, (unsigned long long)job.block_id, written);
    }
    uint64_t start_ms = speed_window_start_ms_.load(std::memory_order_relaxed);
    if (start_ms == 0) {
        speed_window_start_ms_.store(now_ms, std::memory_order_relaxed);
        speed_window_bytes_.store(written, std::memory_order_relaxed);
    } else {
        const uint64_t elapsed = now_ms > start_ms ? now_ms - start_ms : 0;
        const uint64_t window_bytes =
            speed_window_bytes_.fetch_add(written, std::memory_order_relaxed) + written;
        if (elapsed >= 1000) {
            const uint64_t bps = window_bytes * 1000 / elapsed;
            speed_bytes_per_sec_.store(bps, std::memory_order_relaxed);
            speed_window_start_ms_.store(now_ms, std::memory_order_relaxed);
            speed_window_bytes_.store(0, std::memory_order_relaxed);

            dsv_info("[SpillMgr] Write progress: written=%lluMB speed=%lluMB/s ram=%lluMB queue_peak=%llu stalls=%llu",
                     (unsigned long long)(stat_bytes_written_.load(std::memory_order_relaxed) >> 20),
                     (unsigned long long)(bps >> 20),
                     (unsigned long long)(ram_usage_.load(std::memory_order_relaxed) >> 20),
                     (unsigned long long)stat_queue_peak_.load(std::memory_order_relaxed),
                     (unsigned long long)stat_stalls_.load(std::memory_order_relaxed));
        }
    }
    last_progress_log_ms_.store(now_ms, std::memory_order_relaxed);

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
