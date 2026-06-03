#pragma once

#include <cstdint>
#include <cstdio>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pv {
namespace data {

class SpillManager {
public:
    static constexpr uintptr_t SPILLED_SENTINEL = 0x1;

    SpillManager(uint64_t ram_limit_bytes, uint64_t disk_limit_bytes, const std::string& spill_dir);
    ~SpillManager();

    bool init_channels(uint16_t channel_count, const std::string& session_id);
    bool enqueue_spill(uint16_t channel, uint64_t block_id, const void* data, size_t size);
    void wait_for_idle();
    void drain_completed(std::vector<std::pair<uint16_t, uint64_t>>& out);
    const void* load_block(uint16_t channel, uint64_t block_id);
    bool read_block_into(uint16_t channel, uint64_t block_id, void* out_buf, size_t size);
    bool is_spilled(uint16_t channel, uint64_t block_id) const;
    void notify_ram_usage(uint64_t bytes_in_ram);
    bool should_spill() const;
    bool disk_full() const;
    uint64_t ram_usage_bytes() const;
    uint64_t disk_usage_bytes() const;
    uint64_t recent_write_bytes_per_sec() const;
    void log_stats_summary();
    static void cleanup_stale_files(const std::string& spill_dir);

private:
    struct CacheEntry {
        uint16_t channel = 0;
        uint64_t block_id = UINT64_MAX;
        std::vector<uint8_t> data;
        uint64_t last_tick = 0;
    };

    struct WriteJob {
        uint16_t channel = 0;
        uint64_t block_id = 0;
        const uint8_t* data = nullptr;
        size_t size = 0;
    };

    static constexpr int MAX_QUEUE_DEPTH = 128;
    static constexpr int LRU_SLOTS = 128;
    static constexpr size_t FILE_BUFFER_BYTES = 4 * 1024 * 1024;

    static uint64_t make_key(uint16_t ch, uint64_t block_id)
    {
        return (static_cast<uint64_t>(ch) << 40) | (block_id & 0xFFFFFFFFFFULL);
    }

    void io_thread_func();
    bool write_block_to_file(WriteJob& job);
    CacheEntry* lru_evict_slot();

    uint64_t ram_limit_;
    uint64_t disk_limit_;
    std::string spill_dir_;
    uint16_t channel_count_;
    std::atomic<uint64_t> ram_usage_;
    std::atomic<uint64_t> disk_usage_;

    std::vector<FILE*> spill_files_;
    std::vector<std::string> spill_paths_;
    std::vector<std::vector<char>> file_buffers_;
    std::mutex file_mutex_;

    std::unordered_map<uint64_t, uint64_t> block_offsets_;
    std::unordered_map<uint64_t, size_t> block_sizes_;
    mutable std::mutex index_mutex_;

    std::deque<WriteJob> write_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable queue_space_cv_;
    std::condition_variable idle_cv_;
    bool stop_io_;
    std::thread io_thread_;
    int active_writes_;

    std::deque<std::pair<uint16_t, uint64_t>> completed_spills_;
    std::mutex completed_mutex_;

    std::vector<CacheEntry> lru_;
    std::mutex lru_mutex_;
    std::atomic<uint64_t> lru_tick_;

    std::atomic<uint64_t> stat_spilled_;
    std::atomic<uint64_t> stat_bytes_written_;
    std::atomic<uint64_t> stat_lru_hits_;
    std::atomic<uint64_t> stat_lru_misses_;
    std::atomic<uint64_t> stat_queue_peak_;
    std::atomic<uint64_t> stat_stalls_;
    std::atomic<uint64_t> speed_window_start_ms_;
    std::atomic<uint64_t> speed_window_bytes_;
    std::atomic<uint64_t> speed_bytes_per_sec_;
    std::atomic<uint64_t> first_write_ms_;
    std::atomic<uint64_t> last_progress_log_ms_;
};

} // namespace data
} // namespace pv
