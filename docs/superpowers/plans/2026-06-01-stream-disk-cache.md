# Stream 磁盘缓存 (Disk-Spill) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Stream 模式下将 Logic 采样数据从内存透明溢出（spill）至硬盘，使采集深度从 RAM 上限（8/16 GB）扩展到 `disk_depth`（TB 级），同时保持现有所有功能不变。

**Architecture:** 新增 `SpillManager` 类（纯 C++ 标准库，无 Qt）管理 per-channel 临时文件、后台 I/O 线程、写队列（背压机制）和 128 槽 LRU 读缓存。`LogicSnapshot` 在 leaf block 填满后判断是否超过 `ram_limit`，若是则将该 block 的副本入队落盘，原指针在 IO 线程写盘完成后由 `check_pending_spills()` 释放并改写为 `SPILLED_SENTINEL(0x1)`。读取时遇到 sentinel 则通过 `SpillManager::load_block()` 从磁盘加载到 LRU 缓存。磁盘缓存开关为可选，关闭时所有新代码路径均被 `if (_spill_manager)` 短路，零开销。

**Tech Stack:** C++17（std::thread, std::mutex, std::condition_variable, std::atomic, std::unordered_map）, Qt5（仅 UI 部分）, 标准 C 文件 I/O（fopen/fwrite/fread/fseek），QSettings（配置持久化）。

**Spec:** `docs/superpowers/specs/2026-06-01-stream-disk-cache-design.md`

---

## 文件清单

| 路径 | 类型 | 负责 |
|------|------|------|
| `DSView/pv/data/spillmanager.h` | **新增** | SpillManager 全部声明 |
| `DSView/pv/data/spillmanager.cpp` | **新增** | SpillManager 实现（I/O 线程、LRU、文件管理） |
| `DSView/pv/utility/diskcachesettings.h` | **新增** | DiskCacheSettings 结构体 + QSettings 持久化辅助 |
| `DSView/test/test_spillmanager.cpp` | **新增** | SpillManager 单元测试（无 Qt 依赖） |
| `DSView/pv/data/logicsnapshot.h` | 修改 | 新增成员、set_spill_manager()、spill_oldest_block()、check_pending_spills() |
| `DSView/pv/data/logicsnapshot.cpp` | 修改 | 读写路径 sentinel 检查、RAM 计费、loop 分支 |
| `DSView/pv/sigsession.h` | 修改 | DiskCacheSettings 成员、SpillManager* 成员 |
| `DSView/pv/sigsession.cpp` | 修改 | 采集开始/结束时构造/销毁 SpillManager，注入 LogicSnapshot |
| `DSView/main.cpp` | 修改 | 启动时清理残留 spill 文件 |
| `DSView/pv/dialogs/diskcachedialog.h` | **新增** | 磁盘缓存配置对话框声明 |
| `DSView/pv/dialogs/diskcachedialog.cpp` | **新增** | 对话框实现（开关+depth+路径） |
| `DSView/pv/mainwindow.h` | 修改 | 添加 disk cache 菜单项 |
| `DSView/pv/mainwindow.cpp` | 修改 | 连接菜单到对话框 |
| `DSView/CMakeLists.txt` | 修改 | 添加 spillmanager.cpp、对话框文件、test target |

---

## Task 1: CMakeLists + SpillManager 骨架

**Files:**
- Create: `DSView/pv/data/spillmanager.h`
- Create: `DSView/pv/data/spillmanager.cpp`
- Modify: `DSView/CMakeLists.txt`

- [ ] **Step 1: 创建 spillmanager.h（完整声明）**

```cpp
// DSView/pv/data/spillmanager.h
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace pv {
namespace data {

class SpillManager {
public:
    // 当 lbp[] 已落盘时，指针槽设为此 sentinel（malloc 永远不会返回 0x1）
    static constexpr uintptr_t SPILLED_SENTINEL = 0x1;
    static constexpr int MAX_QUEUE_DEPTH = 32;   // 写队列最大条目数（背压上限）
    static constexpr int LRU_SLOTS = 128;         // LRU 缓存槽数（≈256MB @ 2MB/slot）

    // ram_limit_bytes: LogicSnapshot 已分配 leaf block RAM 上限（超过则触发落盘）
    // disk_limit_bytes: 允许的最大 spill 文件总字节数（0 = 无限制）
    // spill_dir: 临时文件目录（如 /tmp 或 %TEMP%）
    SpillManager(uint64_t ram_limit_bytes,
                 uint64_t disk_limit_bytes,
                 const std::string& spill_dir);
    ~SpillManager();

    // 采集开始前调用：为每通道创建 spill 文件
    // session_id: 随机字符串，用于文件命名以避免冲突
    bool init_channels(uint16_t channel_count, const std::string& session_id);

    // 将 data（size 字节，即 LeafBlockSpace）复制进写队列（异步落盘）
    // 若队列满则阻塞（背压），保证不丢数据
    // 调用方在返回后可立即继续使用原始 ptr（已复制），但不要 free，
    // 等 drain_completed() 通知后由 LogicSnapshot 自己 free。
    bool enqueue_spill(uint16_t channel, uint64_t block_id,
                       const void* data, size_t size);

    // 采集线程调用：将 IO 线程已写完的 (channel, block_id) 对从完成队列取出
    // 供 LogicSnapshot 根据这些信息 free 原始 ptr 并设 SENTINEL
    void drain_completed(std::vector<std::pair<uint16_t, uint64_t>>& out);

    // 同步加载（带 LRU 缓存），返回指针在下一次 load_block() 之前有效
    // 失败时返回 nullptr
    const void* load_block(uint16_t channel, uint64_t block_id);

    // 将 block 数据拷贝至调用方 out_buf（out_buf 须为 size 字节）
    bool read_block_into(uint16_t channel, uint64_t block_id,
                         void* out_buf, size_t size);

    bool is_spilled(uint16_t channel, uint64_t block_id) const;

    // LogicSnapshot 每次分配新 leaf block 后调用
    void notify_ram_usage(uint64_t bytes_in_ram);
    bool should_spill() const;
    bool disk_full() const;

    // 采集结束时打印一次统计摘要（通过 dsv_info）
    void log_stats_summary();

    // 应用启动时清理残留的 dsview_spill_* 文件
    static void cleanup_stale_files(const std::string& spill_dir);

private:
    struct CacheEntry {
        uint16_t  channel  = 0;
        uint64_t  block_id = UINT64_MAX;
        std::vector<uint8_t> data;   // LeafBlockSpace 字节
        uint64_t  last_tick = 0;
    };

    struct WriteJob {
        uint16_t  channel;
        uint64_t  block_id;
        std::vector<uint8_t> data;   // 已复制的 leaf block 内容
    };

    // make_key: 将 (channel, block_id) 压缩为 map key
    // channel < 65536, block_id < 2^40 (覆盖 ~16PB 每通道，足够)
    static uint64_t make_key(uint16_t ch, uint64_t blk_id) {
        return (static_cast<uint64_t>(ch) << 40) | (blk_id & 0xFFFFFFFFFFULL);
    }

    void   io_thread_func();
    bool   write_block_to_file(WriteJob& job);
    CacheEntry* lru_evict_slot();
    void   prefetch_async(uint16_t channel, uint64_t block_id);

    uint64_t     ram_limit_;
    uint64_t     disk_limit_;
    std::string  spill_dir_;
    uint16_t     channel_count_ = 0;

    std::vector<FILE*>       spill_files_;    // per-channel FILE* (opened in init_channels)
    std::vector<std::string> spill_paths_;    // per-channel 文件路径（析构时删除）

    // (channel, block_id) → 文件内字节偏移
    std::unordered_map<uint64_t, uint64_t> block_offsets_;
    mutable std::mutex index_mutex_;

    // 写队列
    std::deque<WriteJob>    write_queue_;
    std::mutex              queue_mutex_;
    std::condition_variable queue_cv_;        // IO 线程等待新任务
    std::condition_variable queue_space_cv_;  // 采集线程等待队列空位
    bool                    stop_io_ = false;
    std::thread             io_thread_;

    // IO 线程写完后放入此队列供采集线程 drain_completed() 消费
    std::deque<std::pair<uint16_t, uint64_t>> completed_spills_;
    std::mutex                                 completed_mutex_;

    // LRU 读缓存（128 固定槽）
    std::vector<CacheEntry> lru_;
    std::mutex              lru_mutex_;
    std::atomic<uint64_t>   lru_tick_{0};

    // 性能计数器（原子，热路径无锁）
    std::atomic<uint64_t> stat_spilled_{0};
    std::atomic<uint64_t> stat_bytes_written_{0};
    std::atomic<uint64_t> stat_lru_hits_{0};
    std::atomic<uint64_t> stat_lru_misses_{0};
    std::atomic<uint64_t> stat_total_read_us_{0};
    std::atomic<uint64_t> stat_queue_peak_{0};
    std::atomic<uint64_t> stat_stalls_{0};

    std::atomic<uint64_t> ram_usage_{0};
    std::atomic<uint64_t> disk_usage_{0};
};

} // namespace data
} // namespace pv
```

- [ ] **Step 2: 创建 spillmanager.cpp（骨架，仅让代码能编译）**

```cpp
// DSView/pv/data/spillmanager.cpp
#ifdef SPILL_TESTING
  // 测试模式：不依赖 Qt log 系统
  #include <cstdio>
  #define dsv_info(fmt, ...)  printf("[SPILL INFO] " fmt "\n", ##__VA_ARGS__)
  #define dsv_err(fmt, ...)   fprintf(stderr, "[SPILL ERR]  " fmt "\n", ##__VA_ARGS__)
  #define dsv_warn(fmt, ...)  printf("[SPILL WARN] " fmt "\n", ##__VA_ARGS__)
#else
  #include "../log.h"
#endif

#include "spillmanager.h"
#include <cerrno>
#include <cstring>
#include <cassert>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
#else
  #include <sys/stat.h>
  #include <dirent.h>
  #include <unistd.h>
#endif

namespace pv {
namespace data {

SpillManager::SpillManager(uint64_t ram_limit_bytes,
                           uint64_t disk_limit_bytes,
                           const std::string& spill_dir)
    : ram_limit_(ram_limit_bytes)
    , disk_limit_(disk_limit_bytes)
    , spill_dir_(spill_dir)
{
    lru_.resize(LRU_SLOTS);
}

SpillManager::~SpillManager()
{
    // 停止 IO 线程
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        stop_io_ = true;
    }
    queue_cv_.notify_all();
    if (io_thread_.joinable())
        io_thread_.join();

    // 关闭并删除所有 spill 文件
    for (size_t i = 0; i < spill_files_.size(); ++i) {
        if (spill_files_[i]) {
            fclose(spill_files_[i]);
            spill_files_[i] = nullptr;
        }
        if (!spill_paths_[i].empty()) {
            remove(spill_paths_[i].c_str());
            dsv_info("[SpillMgr] Deleted spill file: %s", spill_paths_[i].c_str());
        }
    }
}

bool SpillManager::init_channels(uint16_t channel_count, const std::string& session_id)
{
    channel_count_ = channel_count;
    spill_files_.resize(channel_count, nullptr);
    spill_paths_.resize(channel_count);

    for (uint16_t ch = 0; ch < channel_count; ++ch) {
        char fname[512];
        snprintf(fname, sizeof(fname), "%s/dsview_spill_%s_ch%u.bin",
                 spill_dir_.c_str(), session_id.c_str(), (unsigned)ch);
        spill_paths_[ch] = fname;

        FILE* f = fopen(fname, "wb+");   // 读写，截断
        if (!f) {
            dsv_err("[SpillMgr] Cannot create spill file %s: %s",
                    fname, strerror(errno));
            return false;
        }
        spill_files_[ch] = f;
        dsv_info("[SpillMgr] Spill file created: %s", fname);
    }

    // 启动 IO 线程
    io_thread_ = std::thread(&SpillManager::io_thread_func, this);

    dsv_info("[SpillMgr] Initialized %u channels, ram_limit=%llu MB, disk_limit=%llu MB",
             (unsigned)channel_count,
             (unsigned long long)(ram_limit_ >> 20),
             (unsigned long long)(disk_limit_ >> 20));
    return true;
}

bool SpillManager::enqueue_spill(uint16_t channel, uint64_t block_id,
                                 const void* data, size_t size)
{
    WriteJob job;
    job.channel  = channel;
    job.block_id = block_id;
    job.data.assign(static_cast<const uint8_t*>(data),
                    static_cast<const uint8_t*>(data) + size);

    std::unique_lock<std::mutex> lk(queue_mutex_);
    if (write_queue_.size() >= static_cast<size_t>(MAX_QUEUE_DEPTH)) {
        stat_stalls_++;
        dsv_warn("[SpillMgr] Write queue full (%zu), capture stalled!",
                 write_queue_.size());
        queue_space_cv_.wait(lk, [this]{
            return write_queue_.size() < static_cast<size_t>(MAX_QUEUE_DEPTH);
        });
    }
    write_queue_.push_back(std::move(job));
    uint64_t depth = write_queue_.size();
    uint64_t prev  = stat_queue_peak_.load(std::memory_order_relaxed);
    if (depth > prev) stat_queue_peak_.store(depth, std::memory_order_relaxed);
    queue_cv_.notify_one();
    return true;
}

void SpillManager::drain_completed(std::vector<std::pair<uint16_t, uint64_t>>& out)
{
    std::lock_guard<std::mutex> lk(completed_mutex_);
    for (auto& p : completed_spills_)
        out.push_back(p);
    completed_spills_.clear();
}

void SpillManager::io_thread_func()
{
    while (true) {
        WriteJob job;
        {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_cv_.wait(lk, [this]{
                return stop_io_ || !write_queue_.empty();
            });
            if (write_queue_.empty() && stop_io_) break;
            job = std::move(write_queue_.front());
            write_queue_.pop_front();
            queue_space_cv_.notify_one();
        }
        write_block_to_file(job);
    }
    // 停止前排空队列
    std::unique_lock<std::mutex> lk(queue_mutex_);
    while (!write_queue_.empty()) {
        WriteJob job2 = std::move(write_queue_.front());
        write_queue_.pop_front();
        write_block_to_file(job2);
    }
}

bool SpillManager::write_block_to_file(WriteJob& job)
{
    if (job.channel >= spill_files_.size()) return false;
    FILE* f = spill_files_[job.channel];
    if (!f) return false;

    // 追加到文件末尾，记录偏移
    if (fseek(f, 0, SEEK_END) != 0) {
        dsv_err("[SpillMgr] fseek failed ch%u: %s", job.channel, strerror(errno));
        return false;
    }
    long offset = ftell(f);
    if (offset < 0) {
        dsv_err("[SpillMgr] ftell failed ch%u: %s", job.channel, strerror(errno));
        return false;
    }

    size_t written = fwrite(job.data.data(), 1, job.data.size(), f);
    if (written != job.data.size()) {
        dsv_err("[SpillMgr] Short write ch%u block#%llu: wrote %zu of %zu: %s",
                job.channel, (unsigned long long)job.block_id,
                written, job.data.size(), strerror(errno));
        return false;
    }
    fflush(f);

    // 更新索引
    {
        std::lock_guard<std::mutex> lk(index_mutex_);
        block_offsets_[make_key(job.channel, job.block_id)] = static_cast<uint64_t>(offset);
    }

    disk_usage_ += written;
    stat_spilled_++;
    stat_bytes_written_ += written;

    if (stat_spilled_ % 100 == 0) {
        dsv_info("[SpillMgr] %llu blocks spilled (%llu MB written)",
                 (unsigned long long)stat_spilled_.load(),
                 (unsigned long long)(stat_bytes_written_.load() >> 20));
    }

    // 通知采集线程此 block 已写完
    {
        std::lock_guard<std::mutex> lk(completed_mutex_);
        completed_spills_.push_back({job.channel, job.block_id});
    }
    return true;
}

const void* SpillManager::load_block(uint16_t channel, uint64_t block_id)
{
    auto t0 = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lk(lru_mutex_);

    // 查 LRU
    for (auto& e : lru_) {
        if (e.channel == channel && e.block_id == block_id && !e.data.empty()) {
            e.last_tick = ++lru_tick_;
            stat_lru_hits_++;
            return e.data.data();
        }
    }

    // LRU miss: 找一个槽位加载
    CacheEntry* slot = lru_evict_slot();
    slot->channel  = channel;
    slot->block_id = block_id;

    uint64_t key, file_offset;
    {
        std::lock_guard<std::mutex> ilk(index_mutex_);
        auto it = block_offsets_.find(make_key(channel, block_id));
        if (it == block_offsets_.end()) {
            dsv_err("[SpillMgr] load_block: ch%u block#%llu not in index",
                    channel, (unsigned long long)block_id);
            slot->block_id = UINT64_MAX;  // 标记无效
            return nullptr;
        }
        file_offset = it->second;
    }

    if (channel >= spill_files_.size() || !spill_files_[channel]) {
        return nullptr;
    }
    FILE* f = spill_files_[channel];

    // 确保 slot.data 大小足够（首次使用时分配）
    if (slot->data.empty()) {
        // 取第一个非空 slot 的大小或使用固定大小
        // LeafBlockSpace 大小在此处无法直接获取；使用实际块大小
        // 从 block_offsets_ 推算相邻 block 的大小不可靠
        // 改为写入时保存大小，简化：直接读到文件末尾的字节数
        // 实际上所有 block 大小固定（LeafBlockSpace），在 enqueue_spill 时已固定
        // 这里取写队列里 job.data.size() —— 由调用者在 write_block_to_file 时的写入大小
        // 简单方案：从文件中读到下一个 block 偏移（或文件末尾）
        // 最简方案：从文件中从 offset 读取固定大小
        // 注意：写入大小 = job.data.size() = LeafBlockSpace（由 enqueue_spill 调用者传入 size 参数）
        // 这里无法知道 LeafBlockSpace，但实际上写入的每个 block 都是同样大小。
        // 解决：在 enqueue_spill 中记录 block_size_。
        // 修复见下方 block_size_ 成员（需在 init_channels 后由第一次 enqueue_spill 设置）
        
        // 使用已记录的 block_size_（见 block_size_ 成员）
        if (block_size_ == 0) {
            dsv_err("[SpillMgr] load_block: block_size unknown");
            return nullptr;
        }
        slot->data.resize(block_size_);
    }

    if (fseek(f, static_cast<long>(file_offset), SEEK_SET) != 0) {
        dsv_err("[SpillMgr] fseek failed on read: %s", strerror(errno));
        return nullptr;
    }
    size_t read_bytes = fread(slot->data.data(), 1, slot->data.size(), f);
    if (read_bytes != slot->data.size()) {
        dsv_err("[SpillMgr] Short read ch%u block#%llu: %zu of %zu",
                channel, (unsigned long long)block_id,
                read_bytes, slot->data.size());
        return nullptr;
    }

    slot->last_tick = ++lru_tick_;
    stat_lru_misses_++;

    auto t1 = std::chrono::steady_clock::now();
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    stat_total_read_us_ += us;
    if (us > 5000)
        dsv_warn("[SpillMgr] Slow load: ch%u block#%llu took %llu us",
                 channel, (unsigned long long)block_id, (unsigned long long)us);

    return slot->data.data();
}

bool SpillManager::read_block_into(uint16_t channel, uint64_t block_id,
                                   void* out_buf, size_t size)
{
    const void* p = load_block(channel, block_id);
    if (!p) return false;
    memcpy(out_buf, p, size);
    return true;
}

SpillManager::CacheEntry* SpillManager::lru_evict_slot()
{
    // 找 last_tick 最小的槽（最久未使用）
    CacheEntry* oldest = &lru_[0];
    for (auto& e : lru_) {
        if (e.block_id == UINT64_MAX) return &e;  // 空槽，直接使用
        if (e.last_tick < oldest->last_tick)
            oldest = &e;
    }
    return oldest;
}

bool SpillManager::is_spilled(uint16_t channel, uint64_t block_id) const
{
    std::lock_guard<std::mutex> lk(index_mutex_);
    return block_offsets_.count(make_key(channel, block_id)) > 0;
}

void SpillManager::notify_ram_usage(uint64_t bytes_in_ram)
{
    ram_usage_.store(bytes_in_ram, std::memory_order_relaxed);
}

bool SpillManager::should_spill() const
{
    return ram_usage_.load(std::memory_order_relaxed) >= ram_limit_;
}

bool SpillManager::disk_full() const
{
    if (disk_limit_ == 0) return false;
    return disk_usage_.load(std::memory_order_relaxed) >= disk_limit_;
}

void SpillManager::log_stats_summary()
{
    uint64_t hits   = stat_lru_hits_.load();
    uint64_t misses = stat_lru_misses_.load();
    uint64_t total  = hits + misses;
    double hit_rate = total > 0 ? 100.0 * hits / total : 100.0;
    double avg_ms   = misses > 0
        ? (double)stat_total_read_us_.load() / misses / 1000.0
        : 0.0;

    dsv_info("[SpillMgr] Session end: spilled=%llu blocks / %llu MB, "
             "lru_hit=%.1f%%, disk_reads=%llu (avg %.2fms), "
             "stalls=%llu, queue_peak=%llu",
             (unsigned long long)stat_spilled_.load(),
             (unsigned long long)(stat_bytes_written_.load() >> 20),
             hit_rate,
             (unsigned long long)misses,
             avg_ms,
             (unsigned long long)stat_stalls_.load(),
             (unsigned long long)stat_queue_peak_.load());
}

void SpillManager::cleanup_stale_files(const std::string& spill_dir)
{
#ifdef _WIN32
    std::string pattern = spill_dir + "\\dsview_spill_*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::string path = spill_dir + "\\" + ffd.cFileName;
        remove(path.c_str());
        dsv_info("[SpillMgr] Cleaned stale file: %s", path.c_str());
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
#else
    DIR* dir = opendir(spill_dir.c_str());
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, "dsview_spill_", 13) == 0) {
            std::string path = spill_dir + "/" + ent->d_name;
            remove(path.c_str());
            dsv_info("[SpillMgr] Cleaned stale file: %s", path.c_str());
        }
    }
    closedir(dir);
#endif
}

} // namespace data
} // namespace pv
```

> **注意：** 上面的 `load_block()` 引用了 `block_size_` 成员，但 Step 1 的头文件声明中还没有。Step 3 会修正这个问题。

- [ ] **Step 3: 在头文件 `spillmanager.h` 的 private 区末尾添加 `block_size_` 成员**

在 `spillmanager.h` 的 `std::atomic<uint64_t> disk_usage_{0};` 之后添加：

```cpp
    uint64_t block_size_ = 0;  // 首次 enqueue_spill 时记录，所有 block 大小固定
```

并在 `spillmanager.cpp` 的 `enqueue_spill()` 函数中，在 `job.data.assign(...)` 之后添加：

```cpp
    if (block_size_ == 0) block_size_ = size;  // 记录一次即可
```

- [ ] **Step 4: 修改 CMakeLists.txt，添加 spillmanager.cpp**

打开 `DSView/CMakeLists.txt`，在列出 `pv/data/logicsnapshot.cpp` 的位置附近添加：

```cmake
    pv/data/spillmanager.cpp
```

同时在文件末尾（或测试目标区域）添加测试可执行文件（Task 7 要用）：

```cmake
# SpillManager unit tests (no Qt dependency)
add_executable(test_spillmanager
    test/test_spillmanager.cpp
    pv/data/spillmanager.cpp
)
target_compile_definitions(test_spillmanager PRIVATE SPILL_TESTING=1)
target_include_directories(test_spillmanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(test_spillmanager)
else()
    target_link_libraries(test_spillmanager pthread)
endif()
```

- [ ] **Step 5: 确认编译通过（不运行）**

```bash
cmake --build <your-build-dir> --target DSView 2>&1 | tail -20
```

期望：无 error（允许有关 "block_size_" 的 warning，Step 3 已修复）。

- [ ] **Step 6: Commit**

```bash
git add DSView/pv/data/spillmanager.h DSView/pv/data/spillmanager.cpp DSView/CMakeLists.txt
git commit -m "feat(spill): add SpillManager skeleton + CMakeLists entry"
```

---

## Task 2: DiskCacheSettings 配置结构体

**Files:**
- Create: `DSView/pv/utility/diskcachesettings.h`

- [ ] **Step 1: 创建 diskcachesettings.h**

```cpp
// DSView/pv/utility/diskcachesettings.h
#pragma once
#include <cstdint>
#include <QString>
#include <QSettings>
#include <QStandardPaths>

namespace pv {
namespace utility {

struct DiskCacheSettings {
    bool     enabled       = false;
    uint64_t ram_limit_gb  = 4;       // RAM 热区上限（GB）
    uint64_t disk_limit_gb = 256;     // 磁盘总深度（GB）；0 = 无限制
    QString  spill_dir;               // 空字符串 = 系统 temp 目录

    // 返回实际 spill 目录（系统 temp 作为默认）
    QString effective_spill_dir() const {
        if (!spill_dir.isEmpty()) return spill_dir;
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    uint64_t ram_limit_bytes()  const { return ram_limit_gb  * 1024ULL * 1024 * 1024; }
    uint64_t disk_limit_bytes() const { return disk_limit_gb * 1024ULL * 1024 * 1024; }

    void save() const {
        QSettings s;
        s.beginGroup("DiskCache");
        s.setValue("enabled",       enabled);
        s.setValue("ram_limit_gb",  (qulonglong)ram_limit_gb);
        s.setValue("disk_limit_gb", (qulonglong)disk_limit_gb);
        s.setValue("spill_dir",     spill_dir);
        s.endGroup();
    }

    void load() {
        QSettings s;
        s.beginGroup("DiskCache");
        enabled       = s.value("enabled",       false).toBool();
        ram_limit_gb  = s.value("ram_limit_gb",  4ULL).toULongLong();
        disk_limit_gb = s.value("disk_limit_gb", 256ULL).toULongLong();
        spill_dir     = s.value("spill_dir",     "").toString();
        s.endGroup();
    }
};

} // namespace utility
} // namespace pv
```

- [ ] **Step 2: Commit**

```bash
git add DSView/pv/utility/diskcachesettings.h
git commit -m "feat(spill): add DiskCacheSettings config struct"
```

---

## Task 3: SpillManager 单元测试程序

**Files:**
- Create: `DSView/test/test_spillmanager.cpp`

- [ ] **Step 1: 创建 test_spillmanager.cpp**

```cpp
// DSView/test/test_spillmanager.cpp
// 编译：cmake --build <build-dir> --target test_spillmanager
// 运行：<build-dir>/test_spillmanager
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>

// SPILL_TESTING 宏让 spillmanager.cpp 使用 printf 而非 dsv_info
#include "../pv/data/spillmanager.h"

using namespace pv::data;

static int g_pass = 0, g_fail = 0;

#define ASSERT_TRUE(expr) do { \
    if (expr) { ++g_pass; } \
    else { fprintf(stderr, "FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++g_fail; } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static std::string get_tmp_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    return std::string(buf);
#else
    return "/tmp";
#endif
}

// ─── Test 1: 生命周期：init_channels + 析构删文件 ───────────────────────────
void test_lifecycle() {
    printf("\n--- test_lifecycle ---\n");
    std::string dir = get_tmp_dir();
    {
        SpillManager sm(4ULL << 30, 0, dir);
        bool ok = sm.init_channels(2, "lifecycle_test");
        ASSERT_TRUE(ok);
        // 文件应存在
        std::string f0 = dir + "/dsview_spill_lifecycle_test_ch0.bin";
        std::string f1 = dir + "/dsview_spill_lifecycle_test_ch1.bin";
#ifndef _WIN32
        ASSERT_TRUE(access(f0.c_str(), F_OK) == 0);
        ASSERT_TRUE(access(f1.c_str(), F_OK) == 0);
#endif
    }  // 析构 → 删除文件
#ifndef _WIN32
    {
        std::string f0 = dir + "/dsview_spill_lifecycle_test_ch0.bin";
        ASSERT_TRUE(access(f0.c_str(), F_OK) != 0);  // 已删除
    }
#endif
}

// ─── Test 2: 写入 + is_spilled + 读回一致性 ─────────────────────────────────
void test_write_read() {
    printf("\n--- test_write_read ---\n");
    std::string dir = get_tmp_dir();
    // LeafBlockSpace ≈ 2MB；这里用小块（4KB）模拟
    const size_t BLOCK_SIZE = 4096;
    SpillManager sm(4ULL << 30, 0, dir);
    ASSERT_TRUE(sm.init_channels(2, "wr_test"));

    // 准备测试数据
    std::vector<uint8_t> data0(BLOCK_SIZE, 0xAA);
    std::vector<uint8_t> data1(BLOCK_SIZE, 0xBB);

    // 写入
    ASSERT_TRUE(sm.enqueue_spill(0, 7, data0.data(), BLOCK_SIZE));
    ASSERT_TRUE(sm.enqueue_spill(1, 7, data1.data(), BLOCK_SIZE));

    // 等待 IO 线程完成（简单等待）
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // is_spilled
    ASSERT_TRUE(sm.is_spilled(0, 7));
    ASSERT_TRUE(sm.is_spilled(1, 7));
    ASSERT_TRUE(!sm.is_spilled(0, 8));

    // 读回
    std::vector<uint8_t> out0(BLOCK_SIZE, 0), out1(BLOCK_SIZE, 0);
    ASSERT_TRUE(sm.read_block_into(0, 7, out0.data(), BLOCK_SIZE));
    ASSERT_TRUE(sm.read_block_into(1, 7, out1.data(), BLOCK_SIZE));
    ASSERT_EQ(memcmp(out0.data(), data0.data(), BLOCK_SIZE), 0);
    ASSERT_EQ(memcmp(out1.data(), data1.data(), BLOCK_SIZE), 0);
}

// ─── Test 3: 多 block 写入 + LRU 命中 ───────────────────────────────────────
void test_multi_block_lru() {
    printf("\n--- test_multi_block_lru ---\n");
    const size_t BLOCK_SIZE = 1024;
    std::string dir = get_tmp_dir();
    SpillManager sm(4ULL << 30, 0, dir);
    ASSERT_TRUE(sm.init_channels(1, "lru_test"));

    const int N = 10;
    std::vector<std::vector<uint8_t>> blocks(N, std::vector<uint8_t>(BLOCK_SIZE));
    for (int i = 0; i < N; ++i) {
        memset(blocks[i].data(), (uint8_t)i, BLOCK_SIZE);
        ASSERT_TRUE(sm.enqueue_spill(0, (uint64_t)i, blocks[i].data(), BLOCK_SIZE));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 读取所有 block，验证数据
    for (int i = 0; i < N; ++i) {
        std::vector<uint8_t> out(BLOCK_SIZE, 0xFF);
        ASSERT_TRUE(sm.read_block_into(0, (uint64_t)i, out.data(), BLOCK_SIZE));
        for (size_t j = 0; j < BLOCK_SIZE; ++j) {
            ASSERT_EQ(out[j], (uint8_t)i);
        }
    }
}

// ─── Test 4: should_spill 触发 ───────────────────────────────────────────────
void test_should_spill() {
    printf("\n--- test_should_spill ---\n");
    SpillManager sm(100 * 1024, 0, get_tmp_dir());  // ram_limit = 100KB
    sm.init_channels(1, "spill_trigger_test");

    sm.notify_ram_usage(50 * 1024);
    ASSERT_TRUE(!sm.should_spill());

    sm.notify_ram_usage(100 * 1024);
    ASSERT_TRUE(sm.should_spill());

    sm.notify_ram_usage(101 * 1024);
    ASSERT_TRUE(sm.should_spill());
}

// ─── Test 5: cleanup_stale_files ─────────────────────────────────────────────
void test_cleanup_stale() {
    printf("\n--- test_cleanup_stale ---\n");
    std::string dir = get_tmp_dir();
    // 手动创建一个 stale 文件
    std::string stale = dir + "/dsview_spill_stale_ch0.bin";
    FILE* f = fopen(stale.c_str(), "wb");
    if (f) { fclose(f); }
    SpillManager::cleanup_stale_files(dir);
#ifndef _WIN32
    ASSERT_TRUE(access(stale.c_str(), F_OK) != 0);
#endif
}

int main() {
    printf("=== SpillManager Unit Tests ===\n");
    test_lifecycle();
    test_write_read();
    test_multi_block_lru();
    test_should_spill();
    test_cleanup_stale();
    printf("\n=== Results: %d PASS, %d FAIL ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
```

- [ ] **Step 2: 编译测试目标**

```bash
cmake --build <build-dir> --target test_spillmanager 2>&1 | tail -20
```

期望：编译成功。

- [ ] **Step 3: 运行测试（预期部分失败，因实现骨架尚不完整）**

```bash
<build-dir>/test_spillmanager
```

期望：`test_lifecycle` 应 PASS（文件创建/删除已实现），其余可能 FAIL（等 Task 1 骨架的 write/read 就绪）。

- [ ] **Step 4: Commit（骨架就绪，测试框架到位）**

```bash
git add DSView/test/test_spillmanager.cpp
git commit -m "test(spill): add SpillManager unit test program"
```

---

## Task 4: 运行测试并修复 SpillManager 直至全部通过

**Files:**
- Modify: `DSView/pv/data/spillmanager.cpp`（修复 write/read 逻辑）

- [ ] **Step 1: 运行测试**

```bash
<build-dir>/test_spillmanager
```

- [ ] **Step 2: 修复 test_write_read 失败**

最常见原因：`load_block()` 中 `block_size_` 为 0。确认 `enqueue_spill()` 里有：

```cpp
if (block_size_ == 0) block_size_ = size;
```

且 `load_block()` 在 `slot->data.empty()` 时执行：

```cpp
if (slot->data.empty()) {
    slot->data.resize(block_size_);
}
```

- [ ] **Step 3: 修复 test_multi_block_lru 失败**

若 LRU 的 `lru_evict_slot()` 找到空槽时 `data` 未正确 resize，补充：

```cpp
CacheEntry* SpillManager::lru_evict_slot()
{
    CacheEntry* oldest = nullptr;
    for (auto& e : lru_) {
        if (e.block_id == UINT64_MAX) return &e;   // 空槽
        if (!oldest || e.last_tick < oldest->last_tick)
            oldest = &e;
    }
    oldest->block_id = UINT64_MAX;  // 清空旧 block_id，data 在 load_block 中 resize
    oldest->data.clear();           // 清空旧数据（eviction）
    return oldest;
}
```

- [ ] **Step 4: 重新编译并运行测试，确认全部通过**

```bash
cmake --build <build-dir> --target test_spillmanager && <build-dir>/test_spillmanager
```

期望输出：

```
=== SpillManager Unit Tests ===
--- test_lifecycle ---
--- test_write_read ---
--- test_multi_block_lru ---
--- test_should_spill ---
--- test_cleanup_stale ---
=== Results: N PASS, 0 FAIL ===
```

- [ ] **Step 5: Commit**

```bash
git add DSView/pv/data/spillmanager.cpp
git commit -m "fix(spill): SpillManager write/read/LRU all unit tests passing"
```

---

## Task 5: LogicSnapshot — SENTINEL 安全清理（free 路径）

**Files:**
- Modify: `DSView/pv/data/logicsnapshot.h`
- Modify: `DSView/pv/data/logicsnapshot.cpp`

> 这是最安全的第一步改动：只在 free 路径跳过 sentinel，不改变任何读写逻辑。

- [ ] **Step 1: 在 logicsnapshot.h 添加新成员和接口**

在 `class LogicSnapshot` 的 `private:` 区（与 `_is_loop`、`_loop_offset` 同级）添加：

```cpp
    // Disk cache spill support
    SpillManager*  _spill_manager      = nullptr;  // nullptr = disk cache off
    uint64_t       _ram_usage_bytes    = 0;        // 当前已分配 leaf block RAM 字节
    uint64_t       _oldest_spill_root  = 0;        // 待落盘的最老 root_index
    uint64_t       _oldest_spill_lbp   = 0;        // 待落盘的最老 lbp_index
```

在 `public:` 区添加：

```cpp
    // 由 SigSession 在采集开始前注入（nullptr = 关闭磁盘缓存）
    inline void set_spill_manager(SpillManager* mgr) { _spill_manager = mgr; }

    // 声明（实现在 Task 6）
    void spill_oldest_block();
    void check_pending_spills();
```

在头文件 include 区域（文件顶部）添加：

```cpp
#include "spillmanager.h"
```

- [ ] **Step 2: 修改 free_data()，跳过 SENTINEL**

找到 `LogicSnapshot::free_data()` 中的：

```cpp
for (unsigned int k = 0; k < Scale; k++){
    if (iter_rn.lbp[k] != NULL)
        free(iter_rn.lbp[k]);
}
```

改为：

```cpp
for (unsigned int k = 0; k < Scale; k++){
    void* p = iter_rn.lbp[k];
    if (p != NULL && p != (void*)SpillManager::SPILLED_SENTINEL)
        free(p);
}
```

- [ ] **Step 3: 修改 move_first_node_to_last()，跳过 SENTINEL**

找到：

```cpp
if (rn.lbp[x] != NULL){
    free(rn.lbp[x]);
    rn.lbp[x] = NULL;
}
```

改为：

```cpp
void* p = rn.lbp[x];
if (p != NULL && p != (void*)SpillManager::SPILLED_SENTINEL){
    free(p);
    rn.lbp[x] = NULL;
}
else if (p == (void*)SpillManager::SPILLED_SENTINEL) {
    rn.lbp[x] = NULL;  // 清除 sentinel（该 root node 循环利用）
}
```

- [ ] **Step 4: 修改 free_head_blocks()，跳过 SENTINEL**

找到：

```cpp
if (_ch_data[i][0].lbp[j] != NULL){
    free(_ch_data[i][0].lbp[j]);
    _ch_data[i][0].lbp[j] = NULL;
}
```

改为：

```cpp
void* p = _ch_data[i][0].lbp[j];
if (p != NULL && p != (void*)SpillManager::SPILLED_SENTINEL){
    free(p);
    _ch_data[i][0].lbp[j] = NULL;
}
else if (p == (void*)SpillManager::SPILLED_SENTINEL) {
    _ch_data[i][0].lbp[j] = NULL;
}
```

- [ ] **Step 5: 修改 free_decode_lpb()，跳过 SENTINEL**

找到 `LogicSnapshot::free_decode_lpb(void *lbp)` 中的 `free(lbp)` 调用，在其前加条件：

```cpp
void LogicSnapshot::free_decode_lpb(void *lbp)
{
    if (lbp == nullptr || lbp == (void*)SpillManager::SPILLED_SENTINEL)
        return;
    // ... 原有逻辑
}
```

- [ ] **Step 6: 重置 _oldest_spill_root/_lbp 在 first_payload()**

在 `LogicSnapshot::first_payload()` 中 `_sample_count = 0;` 附近添加：

```cpp
    _oldest_spill_root = 0;
    _oldest_spill_lbp  = 0;
    _ram_usage_bytes   = 0;
```

- [ ] **Step 7: 编译，确认无 error**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep -E "error:|warning:" | head -20
```

- [ ] **Step 8: Commit**

```bash
git add DSView/pv/data/logicsnapshot.h DSView/pv/data/logicsnapshot.cpp
git commit -m "feat(spill): LogicSnapshot SENTINEL-safe free paths + new members"
```

---

## Task 6: LogicSnapshot — 写路径（spill_oldest_block + check_pending_spills）

**Files:**
- Modify: `DSView/pv/data/logicsnapshot.cpp`

- [ ] **Step 1: 实现 check_pending_spills()**

在 `logicsnapshot.cpp` 末尾添加：

```cpp
void LogicSnapshot::check_pending_spills()
{
    if (!_spill_manager) return;

    std::vector<std::pair<uint16_t, uint64_t>> completed;
    _spill_manager->drain_completed(completed);
    if (completed.empty()) return;

    std::lock_guard<std::mutex> lock(_mutex);
    for (auto& [ch, blk_id] : completed) {
        if (ch >= _channel_num) continue;
        uint64_t root    = blk_id / Scale;
        uint64_t lbp_idx = blk_id % Scale;
        if (root >= _ch_data[ch].size()) continue;
        void* ptr = _ch_data[ch][root].lbp[lbp_idx];
        if (ptr != nullptr && ptr != (void*)SpillManager::SPILLED_SENTINEL) {
            free(ptr);
            _ch_data[ch][root].lbp[lbp_idx] = (void*)SpillManager::SPILLED_SENTINEL;
            _ram_usage_bytes = (_ram_usage_bytes >= LeafBlockSpace)
                                   ? _ram_usage_bytes - LeafBlockSpace
                                   : 0;
        }
    }
}
```

- [ ] **Step 2: 实现 spill_oldest_block()**

```cpp
void LogicSnapshot::spill_oldest_block()
{
    if (!_spill_manager) return;
    if (_channel_num == 0 || _ch_data.empty()) return;

    // 当前写入位置（以通道 0 为代表，cross-format 中所有通道同步推进）
    uint64_t cur_root = _cur_ref_block_indexs[0].root_index;
    uint64_t cur_lbp  = _cur_ref_block_indexs[0].lbp_index;

    // 从 _oldest_spill_root/_lbp 开始向前搜索第一个可落盘的 block
    while (true) {
        // 不能落盘当前正在写入的 block
        if (_oldest_spill_root > cur_root) break;
        if (_oldest_spill_root == cur_root && _oldest_spill_lbp >= cur_lbp) break;

        void* ptr0 = _ch_data[0][_oldest_spill_root].lbp[_oldest_spill_lbp];
        if (ptr0 != nullptr && ptr0 != (void*)SpillManager::SPILLED_SENTINEL) {
            // 该 block 有数据且在 RAM → 对所有通道落盘
            uint64_t blk_id = _oldest_spill_root * Scale + _oldest_spill_lbp;
            for (uint16_t ch = 0; ch < _channel_num; ++ch) {
                void* ptr = _ch_data[ch][_oldest_spill_root].lbp[_oldest_spill_lbp];
                if (ptr == nullptr || ptr == (void*)SpillManager::SPILLED_SENTINEL)
                    continue;
                // enqueue 复制数据（原 ptr 继续有效，check_pending_spills 会 free）
                _spill_manager->enqueue_spill(ch, blk_id, ptr, LeafBlockSpace);
            }

            // 推进 spill 指针
            if (++_oldest_spill_lbp >= Scale) {
                _oldest_spill_lbp = 0;
                ++_oldest_spill_root;
            }
            return;  // 每次只落盘一个 lbp_index（跨所有通道）
        }

        // 跳过空或已 sentinel 的 slot
        if (++_oldest_spill_lbp >= Scale) {
            _oldest_spill_lbp = 0;
            ++_oldest_spill_root;
        }
    }
}
```

- [ ] **Step 3: 修改 append_payload() — 在加锁前 drain 完成队列**

找到：

```cpp
void LogicSnapshot::append_payload(const sr_datafeed_logic &logic)
{
    std::lock_guard<std::mutex> lock(_mutex);
    append_cross_payload(logic);
}
```

改为：

```cpp
void LogicSnapshot::append_payload(const sr_datafeed_logic &logic)
{
    check_pending_spills();  // 在 _mutex 外调用；内部自己加锁
    std::lock_guard<std::mutex> lock(_mutex);
    append_cross_payload(logic);
}
```

- [ ] **Step 4: 在 append_cross_payload() 新分配 lbp 后插入 RAM 计费**

在 `append_cross_payload()` 中，找到每处：

```cpp
lbp = malloc(LeafBlockSpace);
if (lbp == NULL){
    dsv_err("LogicSnapshot::append_cross_payload, Malloc memory failed!");
    return;
}
_ch_data[_ch_fraction][index0].lbp[index1] = lbp;
memset(lbp, 0, LeafBlockSpace);
```

在 `memset` 之后追加（注意同一函数可能有多处分配，每处都要加）：

```cpp
// [DISK CACHE] 记账并触发落盘
if (_spill_manager) {
    _ram_usage_bytes += LeafBlockSpace;
    _spill_manager->notify_ram_usage(_ram_usage_bytes);
    if (_spill_manager->should_spill()) {
        spill_oldest_block();
    }
}
```

- [ ] **Step 5: 编译确认**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep "error:" | head -10
```

- [ ] **Step 6: Commit**

```bash
git add DSView/pv/data/logicsnapshot.cpp
git commit -m "feat(spill): LogicSnapshot spill_oldest_block + check_pending_spills + RAM accounting"
```

---

## Task 7: LogicSnapshot — 读路径 Sentinel 检查

**Files:**
- Modify: `DSView/pv/data/logicsnapshot.cpp`

> **目标：** 所有访问 `lbp[k]` 并解引用的地方，若 `_spill_manager` 存在且值为 SENTINEL，则通过 `load_block()` 从磁盘加载。

- [ ] **Step 1: 修改 get_sample_self()**

找到：

```cpp
uint64_t *lbp = (uint64_t*)_ch_data[order][index0].lbp[index1];
return *(lbp + ((index & LeafMask) >> ScalePower)) & index_mask;
```

改为：

```cpp
void* raw = _ch_data[order][index0].lbp[index1];
if (_spill_manager && raw == (void*)SpillManager::SPILLED_SENTINEL) {
    raw = const_cast<void*>(
        _spill_manager->load_block(static_cast<uint16_t>(order),
                                   index0 * Scale + index1));
    if (!raw) return false;
}
uint64_t *lbp = (uint64_t*)raw;
return *(lbp + ((index & LeafMask) >> ScalePower)) & index_mask;
```

- [ ] **Step 2: 修改 get_sample_unlock()**（如存在类似模式）

在 `get_sample_unlock()` 中找到所有 `uint64_t *lbp = (uint64_t*)_ch_data[...][...].lbp[...]` 形式，应用同样的 sentinel 检查模式（与 Step 1 完全相同的替换）。

- [ ] **Step 3: 修改 get_nxt_edge_unlock() 中调用 block_pre_edge() 之前**

`block_pre_edge(uint64_t *lbp, ...)` 的第一个参数是已解析的 lbp 指针。在 `get_nxt_edge_unlock()` 中，找到每处这样的模式：

```cpp
uint64_t *lbp = (uint64_t*)_ch_data[order][index0].lbp[index1];
// ... 然后调用 block_pre_edge(lbp, ...) 或直接访问 *lbp
```

在取出 lbp 之前，插入 sentinel 检查：

```cpp
void* raw_lbp = _ch_data[order][index0].lbp[index1];
if (_spill_manager && raw_lbp == (void*)SpillManager::SPILLED_SENTINEL) {
    raw_lbp = const_cast<void*>(
        _spill_manager->load_block(static_cast<uint16_t>(order),
                                   index0 * Scale + index1));
    if (!raw_lbp) {
        // 读盘失败：跳过此 block，将 index 推进到下一 block 边界
        index = (index0 * Scale + index1 + 1) * LeafBlockSamples;
        continue;
    }
}
uint64_t *lbp = (uint64_t*)raw_lbp;
```

> **注意：** `get_nxt_edge_unlock()` 的完整源码需要你在编辑器中打开 `logicsnapshot.cpp` 查看（约 800+ 行处）。对所有直接使用 `lbp[index1]` 的地方，应用上述 sentinel 模式。

- [ ] **Step 4: 编译确认**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep "error:" | head -10
```

- [ ] **Step 5: Commit**

```bash
git add DSView/pv/data/logicsnapshot.cpp
git commit -m "feat(spill): add SPILLED_SENTINEL checks to all LogicSnapshot read paths"
```

---

## Task 8: LogicSnapshot — Loop 模式磁盘缓存分支

**Files:**
- Modify: `DSView/pv/data/logicsnapshot.cpp`

> **设计：** 当磁盘缓存开启 + Loop 模式时，`first_payload()` 不调用 `set_loop(true)`（因此 `_is_loop` 为 false），避免 ring 覆盖逻辑触发。显示窗口跟随最新数据（Phase 1 简化方案）。

- [ ] **Step 1: 在 first_payload() 中，仅在磁盘缓存关闭时执行 is_loop 检查**

找到 `first_payload()` 中：

```cpp
if (total_sample_count != _total_sample_count
    || channel_num != _channel_num
    || channel_changed
    || _is_loop) {
```

注意 `_is_loop` 由调用者（SigSession）通过 `set_loop(is_loop_mode())` 传入。Task 9 会在 SigSession 侧控制：当磁盘缓存开启时传入 `false`。此处 `logicsnapshot.cpp` 无需额外改动。

- [ ] **Step 2: 确认 append_cross_payload() 的 loop 分支在 _is_loop=false 时被绕过**

```cpp
if (_is_loop)    // <── 当 SigSession 不调用 set_loop(true) 时，此分支不执行
{
    if (_loop_offset >= LeafBlockSamples * Scale){        
        move_first_node_to_last();
        ...
    }
    ...
}
```

磁盘缓存开启时，`_is_loop = false`，整个 loop 分支跳过。✓  
RAM 超限的处理由 Task 6 的 `spill_oldest_block()` 接管。

- [ ] **Step 3: 编译，确认无 error**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep "error:" | head -5
```

- [ ] **Step 4: Commit**

```bash
git commit -m "feat(spill): loop mode bypassed when disk cache enabled (handled via spill)"
# 本 task 可能无需新增改动，仅验证
```

---

## Task 9: SigSession — SpillManager 生命周期

**Files:**
- Modify: `DSView/pv/sigsession.h`
- Modify: `DSView/pv/sigsession.cpp`

- [ ] **Step 1: 修改 sigsession.h**

在 `sigsession.h` 的 include 区添加：

```cpp
#include "data/spillmanager.h"
#include "utility/diskcachesettings.h"
```

在 `private:` 区（与 `_is_saving`、`_is_working` 同级）添加：

```cpp
    // Disk cache
    pv::utility::DiskCacheSettings _disk_cache_settings;
    pv::data::SpillManager*        _spill_manager = nullptr;
```

添加 public 接口：

```cpp
    void set_disk_cache_settings(const pv::utility::DiskCacheSettings& s) {
        _disk_cache_settings = s;
    }
    const pv::utility::DiskCacheSettings& disk_cache_settings() const {
        return _disk_cache_settings;
    }
```

- [ ] **Step 2: 在 sigsession.cpp 添加辅助函数**

在 `sigsession.cpp` 顶部匿名 namespace 或文件作用域添加：

```cpp
static std::string generate_session_id() {
    // timestamp + 4位随机后缀
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    unsigned rnd = (unsigned)(now & 0xFFFF);
    char buf[64];
    snprintf(buf, sizeof(buf), "%llu_%04x",
             (unsigned long long)now, rnd);
    return buf;
}
```

（需要在 sigsession.cpp 顶部确认 `#include <chrono>` 已存在，否则添加。）

- [ ] **Step 3: 在 feed_in_logic() 的 first_payload 分支中创建/注入 SpillManager**

找到 `SigSession::feed_in_logic()` 中：

```cpp
if (_capture_data->get_logic()->last_ended())
{
    _capture_data->get_logic()->set_loop(is_loop_mode());

    bool bNotFree = is_decoding() && _view_data == _capture_data;

    _capture_data->get_logic()->first_payload(o, 
                    _device_agent.get_sample_limit(),
                    _device_agent.get_channels(),
                    !bNotFree);
```

改为：

```cpp
if (_capture_data->get_logic()->last_ended())
{
    // [DISK CACHE] 当磁盘缓存开启时：
    // 1. 不启用 ring-loop（_is_loop=false），由 SpillManager 接管深度
    // 2. total_sample_count 扩展为 disk_depth 对应样本数
    bool use_disk_cache = _disk_cache_settings.enabled && !_spill_manager;
    uint64_t sample_limit = _device_agent.get_sample_limit();

    if (use_disk_cache) {
        // 统计已启用的 logic 通道数
        int ch_count = 0;
        for (const GSList *l = _device_agent.get_channels(); l; l = l->next) {
            sr_channel* probe = (sr_channel*)l->data;
            if (probe->type == SR_CHANNEL_LOGIC && probe->enabled) ++ch_count;
        }
        if (ch_count > 0 && _disk_cache_settings.disk_limit_bytes() > 0) {
            sample_limit = _disk_cache_settings.disk_limit_bytes() * 8 / (uint64_t)ch_count;
        }

        // 创建 SpillManager
        std::string dir = _disk_cache_settings.effective_spill_dir().toStdString();
        _spill_manager = new pv::data::SpillManager(
            _disk_cache_settings.ram_limit_bytes(),
            _disk_cache_settings.disk_limit_bytes(),
            dir
        );
        if (!_spill_manager->init_channels((uint16_t)ch_count, generate_session_id())) {
            dsv_err("SpillManager init failed; falling back to RAM-only mode");
            delete _spill_manager;
            _spill_manager = nullptr;
        } else {
            dsv_info("[SigSession] Disk cache enabled: ram=%llu MB, disk=%llu MB",
                     (unsigned long long)(_disk_cache_settings.ram_limit_bytes() >> 20),
                     (unsigned long long)(_disk_cache_settings.disk_limit_bytes() >> 20));
        }
    }

    // 磁盘缓存开启时不设 loop（避免 ring 覆盖）
    bool set_loop = is_loop_mode() && !_spill_manager;
    _capture_data->get_logic()->set_loop(set_loop);
    _capture_data->get_logic()->set_spill_manager(_spill_manager);

    bool bNotFree = is_decoding() && _view_data == _capture_data;
    _capture_data->get_logic()->first_payload(o,
                    sample_limit,
                    _device_agent.get_channels(),
                    !bNotFree);
```

- [ ] **Step 4: 在采集结束时销毁 SpillManager**

在 SigSession 中找到采集结束回调（session_error、stop、frame_ended 等）。通常在 `sessioncallback.cpp` 的 `SigSession::feed_in_frame_end()` 或等价函数。找到采集结束的主要路径，添加：

```cpp
if (_spill_manager) {
    _spill_manager->log_stats_summary();
    // SpillManager 析构时自动删除 spill 文件
    _capture_data->get_logic()->set_spill_manager(nullptr);
    delete _spill_manager;
    _spill_manager = nullptr;
}
```

> **提示：** 在 `sigsession.cpp` 搜索 `_is_working = false` 或 `_callback->session_stopped` 找到采集结束点。

- [ ] **Step 5: SigSession 析构函数中确保 SpillManager 被清理**

在 `SigSession::~SigSession()` 中添加：

```cpp
if (_spill_manager) {
    delete _spill_manager;
    _spill_manager = nullptr;
}
```

- [ ] **Step 6: 编译确认**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep "error:" | head -10
```

- [ ] **Step 7: Commit**

```bash
git add DSView/pv/sigsession.h DSView/pv/sigsession.cpp
git commit -m "feat(spill): SigSession creates/destroys SpillManager; injects into LogicSnapshot"
```

---

## Task 10: 启动时清理残留 spill 文件

**Files:**
- Modify: `DSView/main.cpp`

- [ ] **Step 1: 在 main.cpp 加入启动清理**

打开 `DSView/main.cpp`，找到 `main()` 函数的入口（在 `QApplication` 初始化之后，主窗口创建之前）。添加：

```cpp
// 顶部 include 区添加：
#include "pv/data/spillmanager.h"
#include <QStandardPaths>

// main() 函数内，QApplication 实例化之后：
{
    QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    pv::data::SpillManager::cleanup_stale_files(tmpDir.toStdString());
}
```

- [ ] **Step 2: 编译确认**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep "error:" | head -5
```

- [ ] **Step 3: Commit**

```bash
git add DSView/main.cpp
git commit -m "feat(spill): cleanup stale dsview_spill_* files at startup"
```

---

## Task 11: UI — 磁盘缓存配置对话框

**Files:**
- Create: `DSView/pv/dialogs/diskcachedialog.h`
- Create: `DSView/pv/dialogs/diskcachedialog.cpp`
- Modify: `DSView/CMakeLists.txt`

- [ ] **Step 1: 创建 diskcachedialog.h**

```cpp
// DSView/pv/dialogs/diskcachedialog.h
#pragma once
#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include "../utility/diskcachesettings.h"

namespace pv {
namespace dialogs {

class DiskCacheDialog : public QDialog {
    Q_OBJECT
public:
    explicit DiskCacheDialog(QWidget* parent = nullptr);

    // 用对话框中的设置填充 s
    void get_settings(pv::utility::DiskCacheSettings& s) const;
    // 用 s 初始化控件
    void set_settings(const pv::utility::DiskCacheSettings& s);

private slots:
    void on_browse_dir();
    void on_enabled_changed(int state);

private:
    QCheckBox    *_chk_enabled;
    QComboBox    *_cmb_ram;       // 1/2/4/8/16 GB
    QComboBox    *_cmb_disk;      // 64/128/256/512GB/1TB/Unlimited
    QLineEdit    *_edt_dir;
    QPushButton  *_btn_browse;
};

} // namespace dialogs
} // namespace pv
```

- [ ] **Step 2: 创建 diskcachedialog.cpp**

```cpp
// DSView/pv/dialogs/diskcachedialog.cpp
#include "diskcachedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QStandardPaths>

namespace pv {
namespace dialogs {

static const uint64_t RAM_GB[]  = {1, 2, 4, 8, 16};
static const uint64_t DISK_GB[] = {64, 128, 256, 512, 1024, 0}; // 0 = unlimited

DiskCacheDialog::DiskCacheDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Disk Cache Settings"));
    setMinimumWidth(420);

    _chk_enabled = new QCheckBox(tr("Enable disk cache (extends capture depth beyond RAM)"));
    _cmb_ram  = new QComboBox;
    _cmb_disk = new QComboBox;
    _edt_dir  = new QLineEdit;
    _btn_browse = new QPushButton(tr("Browse..."));

    for (uint64_t gb : RAM_GB)
        _cmb_ram->addItem(QString("%1 GB").arg(gb), QVariant((qulonglong)gb));
    _cmb_ram->setCurrentIndex(2);  // 默认 4GB

    for (uint64_t gb : DISK_GB) {
        if (gb == 0)
            _cmb_disk->addItem(tr("Unlimited"), QVariant((qulonglong)0));
        else
            _cmb_disk->addItem(QString("%1 GB").arg(gb >= 1024 ? gb/1024 : gb)
                               + (gb >= 1024 ? " TB" : " GB"),
                               QVariant((qulonglong)gb));
    }
    _cmb_disk->setCurrentIndex(2);  // 默认 256GB

    _edt_dir->setPlaceholderText(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    auto* form = new QFormLayout;
    form->addRow(tr("RAM hot window:"), _cmb_ram);
    form->addRow(tr("Disk total depth:"), _cmb_disk);

    auto* dir_row = new QHBoxLayout;
    dir_row->addWidget(_edt_dir);
    dir_row->addWidget(_btn_browse);
    form->addRow(tr("Cache directory:"), dir_row);

    auto* grp = new QGroupBox;
    grp->setLayout(form);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(_chk_enabled);
    main_layout->addWidget(grp);
    main_layout->addWidget(btns);

    connect(_chk_enabled, &QCheckBox::stateChanged,
            this, &DiskCacheDialog::on_enabled_changed);
    connect(_btn_browse, &QPushButton::clicked,
            this, &DiskCacheDialog::on_browse_dir);

    on_enabled_changed(Qt::Unchecked);
}

void DiskCacheDialog::on_enabled_changed(int state)
{
    bool en = (state == Qt::Checked);
    _cmb_ram->setEnabled(en);
    _cmb_disk->setEnabled(en);
    _edt_dir->setEnabled(en);
    _btn_browse->setEnabled(en);
}

void DiskCacheDialog::on_browse_dir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select cache directory"), _edt_dir->text());
    if (!dir.isEmpty())
        _edt_dir->setText(dir);
}

void DiskCacheDialog::get_settings(pv::utility::DiskCacheSettings& s) const
{
    s.enabled       = _chk_enabled->isChecked();
    s.ram_limit_gb  = _cmb_ram->currentData().toULongLong();
    s.disk_limit_gb = _cmb_disk->currentData().toULongLong();
    s.spill_dir     = _edt_dir->text().trimmed();
}

void DiskCacheDialog::set_settings(const pv::utility::DiskCacheSettings& s)
{
    _chk_enabled->setChecked(s.enabled);
    for (int i = 0; i < _cmb_ram->count(); ++i)
        if (_cmb_ram->itemData(i).toULongLong() == s.ram_limit_gb)
            { _cmb_ram->setCurrentIndex(i); break; }
    for (int i = 0; i < _cmb_disk->count(); ++i)
        if (_cmb_disk->itemData(i).toULongLong() == s.disk_limit_gb)
            { _cmb_disk->setCurrentIndex(i); break; }
    _edt_dir->setText(s.spill_dir);
    on_enabled_changed(s.enabled ? Qt::Checked : Qt::Unchecked);
}

} // namespace dialogs
} // namespace pv
```

- [ ] **Step 3: 在 CMakeLists.txt 添加 dialog 源文件**

找到 `pv/dialogs/` 源文件列表，添加：

```cmake
    pv/dialogs/diskcachedialog.cpp
```

- [ ] **Step 4: 编译确认**

```bash
cmake --build <build-dir> --target DSView 2>&1 | grep "error:" | head -10
```

- [ ] **Step 5: Commit**

```bash
git add DSView/pv/dialogs/diskcachedialog.h DSView/pv/dialogs/diskcachedialog.cpp DSView/CMakeLists.txt
git commit -m "feat(spill): add DiskCacheDialog for user configuration"
```

---

## Task 12: UI — 主窗口菜单接入

**Files:**
- Modify: `DSView/pv/mainwindow.h`
- Modify: `DSView/pv/mainwindow.cpp`

- [ ] **Step 1: 在 mainwindow.h 添加 slot 和成员**

在 `private slots:` 区添加：

```cpp
    void on_disk_cache_settings();
```

- [ ] **Step 2: 在 mainwindow.cpp 找到 File/Edit/View/Help 菜单，添加 Tools 菜单项**

搜索 `menuBar()` 或现有菜单创建逻辑。添加：

```cpp
// include 区：
#include "dialogs/diskcachedialog.h"

// 在菜单构建处：
QAction* disk_cache_action = new QAction(tr("Disk Cache Settings..."), this);
connect(disk_cache_action, &QAction::triggered, this, &MainWindow::on_disk_cache_settings);
// 将 disk_cache_action 添加到合适的菜单（如 Tools 或 Settings 菜单）
// 若无 Tools 菜单，可添加到 menuBar() 的最后：
QMenu* tools_menu = menuBar()->addMenu(tr("Tools"));
tools_menu->addAction(disk_cache_action);
```

- [ ] **Step 3: 实现 on_disk_cache_settings()**

```cpp
void MainWindow::on_disk_cache_settings()
{
    pv::dialogs::DiskCacheDialog dlg(this);
    // 从 session 读取当前设置
    dlg.set_settings(_session->disk_cache_settings());
    if (dlg.exec() == QDialog::Accepted) {
        pv::utility::DiskCacheSettings s;
        dlg.get_settings(s);
        s.save();
        _session->set_disk_cache_settings(s);
    }
}
```

- [ ] **Step 4: 在 SigSession 构造或初始化时加载设置**

在 `SigSession` 构造函数末尾添加：

```cpp
_disk_cache_settings.load();
```

- [ ] **Step 5: 编译运行，确认菜单项可点击，对话框可打开**

```bash
cmake --build <build-dir> --target DSView && <build-dir>/DSView  # 或等价的运行命令
```

期望：「Tools → Disk Cache Settings...」菜单可见，点击打开对话框，复选框/下拉框可操作，OK 后重新打开仍保持上次设置。

- [ ] **Step 6: Commit**

```bash
git add DSView/pv/mainwindow.h DSView/pv/mainwindow.cpp DSView/pv/sigsession.cpp
git commit -m "feat(spill): wire DiskCacheDialog into main menu; load settings on start"
```

---

## Task 13: SamplingBar — 双深度标签

**Files:**
- Modify: `DSView/pv/toolbars/samplingbar.cpp`

- [ ] **Step 1: 修改 update_buffer_label()**

找到 `SamplingBar::update_buffer_label(bool stream_mode)` 函数。在该函数末尾（设置 label 文字的地方），添加条件：

```cpp
// 在原有 label 文字设置代码之前/之后添加：
if (stream_mode) {
    const auto& dc = _session->disk_cache_settings();
    if (dc.enabled) {
        QString ram_str  = dc.ram_limit_gb  >= 1024
            ? QString("%1 TB").arg(dc.ram_limit_gb / 1024)
            : QString("%1 GB").arg(dc.ram_limit_gb);
        QString disk_str = dc.disk_limit_gb == 0
            ? tr("Unlimited")
            : (dc.disk_limit_gb >= 1024
               ? QString("%1 TB").arg(dc.disk_limit_gb / 1024)
               : QString("%1 GB").arg(dc.disk_limit_gb));
        // 将 label 设置为 "RAM: Xgb / Disk: Ygb"
        // 具体 label widget 变量名请查阅 samplingbar.h 中的 _lbl_buffer
        if (_lbl_buffer)
            _lbl_buffer->setText(QString("RAM: %1 / Disk: %2").arg(ram_str, disk_str));
        return;
    }
}
// 原有标签设置代码（不变）
```

- [ ] **Step 2: 编译并肉眼检查标签显示**

启动 DSView，打开 Disk Cache Settings，勾选启用，OK，再看 SamplingBar 中的 buffer 标签是否变为 "RAM: X GB / Disk: Y GB"。

- [ ] **Step 3: Commit**

```bash
git add DSView/pv/toolbars/samplingbar.cpp
git commit -m "feat(spill): SamplingBar shows RAM+Disk dual-depth label when disk cache enabled"
```

---

## Task 14: 集成测试清单

> 以下手动测试必须在代码完成后逐条执行并记录结果。

- [ ] **测试 1: 磁盘缓存关闭 → 现有功能不变**
  - 启动 DSView，确认 Disk Cache Settings 未勾选
  - 以单次/重复/滚动模式各采集一次（使用已有设备或 Demo 设备）
  - 期望：行为与未修改前完全一致，无 crash，波形正常显示，解码正常

- [ ] **测试 2: 磁盘缓存开启 → RAM 限制小于实际深度（触发落盘）**
  - 打开 Disk Cache Settings，勾选启用，设 RAM=1 GB，Disk=8 GB
  - 确认临时目录可写（默认系统 temp）
  - 采集（单次模式），持续到停止
  - 检查日志（dsv log）：应有 "[SpillMgr] Spill file created: ..." 和落盘进度日志
  - 停止后检查 spill 文件已删除（`ls /tmp/dsview_spill_*` 应无）

- [ ] **测试 3: 大深度采集不 OOM**
  - 设 ram_limit=2GB, disk_depth=16GB，采集至深度满
  - 期望：不出现 Malloc_err，`_memory_failed` 不置位

- [ ] **测试 4: 采集中浏览历史波形（边采边翻，B 模式）**
  - 采集进行中（单次模式），已落盘后尝试拖动波形到更早区域
  - 期望：波形渲染正常（可能有短暂加载延迟），不 crash

- [ ] **测试 5: 采集结束后保存 .dsv**
  - 磁盘缓存开启采集，停止后执行「Save」
  - 期望：.dsv 文件生成，重新加载后数据完整

- [ ] **测试 6: Loop 模式 + 磁盘缓存**
  - 切换 Loop 模式，开启磁盘缓存，采集 5 分钟
  - 停止后拖动波形到最开头
  - 期望：全程数据保留（不覆盖），可查看历史

- [ ] **测试 7: 崩溃残留清理**
  - 手动在 temp 目录创建 `dsview_spill_fake_ch0.bin`
  - 重启 DSView
  - 期望：日志显示 "[SpillMgr] Cleaned stale file: ..."，文件已删除

- [ ] **测试 8: 磁盘空间日志摘要**
  - 开启磁盘缓存，进行一次触发落盘的采集，停止
  - 查阅日志输出
  - 期望：看到 "[SpillMgr] Session end: spilled=..." 摘要行

---

## Task 15: 最终提交与 PR 准备

- [ ] **Step 1: 运行所有单元测试**

```bash
cmake --build <build-dir> --target test_spillmanager && <build-dir>/test_spillmanager
```

期望：`0 FAIL`

- [ ] **Step 2: 确认 DSView 完整编译**

```bash
cmake --build <build-dir> --target DSView 2>&1 | tail -5
```

期望：无 error。

- [ ] **Step 3: 执行集成测试清单（Task 14 的所有测试）**

逐条执行并打勾。

- [ ] **Step 4: 最终 commit（如有未提交内容）**

```bash
git add -A
git status  # 确认无意外文件
git commit -m "feat(spill): stream disk cache Phase 1 complete"
```

---

## 附录：常见问题排查

| 现象 | 排查点 |
|------|--------|
| 采集时 OOM (`Malloc_err`) | `_ram_usage_bytes` 是否正确计费；`should_spill()` 是否返回 true；`spill_oldest_block()` 是否找到可落盘 block |
| 波形显示乱码（已落盘区域） | `load_block()` 是否返回正确数据；LRU 槽 resize 是否为 `block_size_`；sentinel 检查是否覆盖所有读路径 |
| 写盘速度不够（stalls > 0） | 检查 SSD 类型（SATA vs NVMe）；考虑减小 ram_limit（给 IO 线程更多时间） |
| 临时文件未删除 | 确认析构函数路径被执行；检查 `spill_paths_` 是否正确填充 |
| 磁盘缓存关闭时行为改变 | 检查所有 `if (_spill_manager)` 分支是否正确短路；不应影响 `_is_loop` 值（仅在 disk cache 开启时传 false） |
