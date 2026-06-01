# DSView Stream 模式磁盘缓存 — 设计规格 (Spec)

> 状态：已批准  
> 日期：2026-06-01  
> 作者：Brainstorming session  
> 关联问题文档：`docs/stream-disk-cached-questions.md`

---

## 1. 目标与约束

### 1.1 目标
在 Stream 模式下，通过将「内存 → 磁盘」的 Spill 层透明地插入数据路径，将可采集深度从受 `sw_depth`（8/16 GB RAM）限制提升到 `disk_depth`（可达 TB 级），同时保持：
- 采集吞吐率不下降（队列异步写盘）
- 波形浏览流畅（LRU 读缓存 + 预取）
- 解码器正常运行（block 级顺序读）
- 非磁盘缓存模式的全部功能和行为**完全不变**

### 1.2 非目标（首版不做）
- 采集中途解码（单次模式 Q1-C 的「边采边解码」留作 Phase 2）
- LZ4/zstd 压缩（Phase 2 扩展项）
- 会话持久化（关闭后重载 spill 文件）
- DSO / Analog 数据类型的磁盘缓存（仅 Logic）

### 1.3 硬约束
- **不更改**磁盘缓存关闭时任何现有代码路径的行为
- 全部新代码用 `if (spill_manager_)` 守护；OFF = 指针为 `nullptr`，短路返回
- 跨平台：Windows + Linux（不使用 mmap）

---

## 2. 用户决策摘要

| 问题 | 选择 |
|------|------|
| Q1 访问模式 | 单次→B（边采边翻，首版；C 解码预留 Phase 2）；重复/滚动→B |
| Q2 触发策略 | D：`sw_depth` 变为 RAM 热区上限；新增 `disk_depth` 为总采样深度 |
| Q3 磁盘格式 | B：每通道独立 spill 文件，读写速度优先；无压缩（首版） |
| Q4 生命周期 | A：临时文件，采集结束或退出时删除 |
| Q5 适用范围 | C 变种：Logic + Loop；Loop 模式不覆盖数据，改为记录显示窗口起始偏移 |

---

## 3. 架构总览

### 3.1 数据流（磁盘缓存开启时）

```
硬件包
  └→ SigSession::feed_in_logic()
       └→ LogicSnapshot::append_payload()
            └→ append_cross_payload()
                 ├─ [正常] 写入 lbp[k] = malloc(LeafBlockSpace)
                 └─ [RAM 达上限] SpillManager::enqueue_spill(ch, blk_id, ptr)
                                    → 队列 → I/O 线程 → per-channel spill 文件
                                    → lbp[k] = SPILLED_SENTINEL (0x1)
                                    → 释放 RAM

渲染/解码 线程
  └→ get_sample() / get_nxt_edge_unlock()
       └─ lbp[k] == SPILLED_SENTINEL?
            是 → SpillManager::load_block(ch, blk_id) → LRU cache → 文件读
            否 → 直接使用 RAM 指针（现有路径，零开销）
```

### 3.2 Loop 模式变更

**当前行为（磁盘缓存关闭）：**
```
_loop_offset >= RootNodeSamples → move_first_node_to_last() → 丢弃老数据
```

**新行为（磁盘缓存开启 + Loop 模式）：**
```
所有 leaf block 写满后均 enqueue_spill() 落盘（不调 move_first_node_to_last）
UI 维护 _display_window_start = 最新样本 - 屏幕宽度对应样本数
用户手动拖动改变 _display_window_start，数据在 spill 文件中永久保留
```

---

## 4. 新增类：`SpillManager`

**文件：** `DSView/pv/data/spillmanager.h` / `spillmanager.cpp`

### 4.1 接口

```cpp
class SpillManager {
public:
    // 构造：ram_limit=RAM热区字节上限；disk_limit=总磁盘字节上限；dir=临时目录
    SpillManager(uint64_t ram_limit_bytes,
                 uint64_t disk_limit_bytes,
                 const std::string& spill_dir);
    ~SpillManager();  // 自动删除所有 spill 文件

    // 初始化通道（采集开始时调用，创建每通道 spill 文件）
    bool init_channels(uint16_t channel_count, const std::string& session_id);

    // 异步写盘：将 data（LeafBlockSpace 字节）加入写队列
    // 成功 enqueue 后调用者应将 lbp 标记为 SPILLED_SENTINEL 并释放
    // 若队列满（背压）则阻塞，直到有空位
    bool enqueue_spill(uint16_t channel, uint64_t block_id,
                       const void* data, size_t size);

    // 同步读取（LRU 缓存）：返回指向 LRU 缓存槽的只读指针
    // 指针在下一次 load_block() 前有效（不跨线程缓存）
    const void* load_block(uint16_t channel, uint64_t block_id);

    // 批量读取接口（供解码器使用）
    // 将 block_id 对应数据拷贝到 out_buf（调用者分配 LeafBlockSpace 字节）
    bool read_block_into(uint16_t channel, uint64_t block_id, void* out_buf);

    // 查询：某 block 是否已在 spill 文件中
    bool is_spilled(uint16_t channel, uint64_t block_id) const;

    // 查询当前 RAM 使用量（由 LogicSnapshot 通知）
    void notify_ram_usage(uint64_t bytes_in_ram);

    // 检查是否达到 RAM 上限
    bool should_spill() const;

    // 检查是否达到磁盘上限
    bool disk_full() const;

    // 性能统计摘要（采集结束时调用，打印到 dsv_info）
    void log_stats_summary();

    // 静态：启动时清理残留 spill 文件
    static void cleanup_stale_files(const std::string& spill_dir);

    static constexpr uintptr_t SPILLED_SENTINEL = 0x1;

private:
    // LRU 缓存条目
    struct CacheEntry {
        uint16_t channel;
        uint64_t block_id;
        std::vector<uint8_t> data;  // LeafBlockSpace 字节
        uint64_t last_access_tick;
    };

    // 写队列条目
    struct WriteJob {
        uint16_t channel;
        uint64_t block_id;
        std::vector<uint8_t> data;
    };

    void io_thread_func();          // 后台 I/O 线程主函数
    bool write_block_to_file(const WriteJob& job);
    uint64_t get_file_offset(uint16_t channel, uint64_t block_id) const;
    CacheEntry* lru_find_or_evict(uint16_t channel, uint64_t block_id);
    void prefetch_async(uint16_t channel, uint64_t block_id);

    uint64_t ram_limit_;            // RAM 热区字节上限
    uint64_t disk_limit_;           // 磁盘字节上限
    std::string spill_dir_;
    uint16_t channel_count_ = 0;

    // per-channel spill 文件（FILE* 而非 fstream，确保跨平台 fseek/fwrite）
    std::vector<FILE*>    spill_files_;
    std::vector<std::string> spill_paths_;

    // 每 channel、每 block_id → 文件偏移索引
    // key = (channel << 32) | block_id；value = file_offset
    std::unordered_map<uint64_t, uint64_t> block_offsets_;
    mutable std::mutex    index_mutex_;

    // 写队列
    std::deque<WriteJob>  write_queue_;
    std::mutex            queue_mutex_;
    std::condition_variable queue_cv_;
    bool                  stop_io_thread_ = false;
    std::thread           io_thread_;

    // LRU 读缓存（固定 128 槽，256MB @ 2MB/slot）
    static constexpr int  LRU_SLOTS = 128;
    std::vector<CacheEntry> lru_cache_;
    std::mutex            lru_mutex_;
    uint64_t              lru_tick_ = 0;

    // 原子性能计数器（热路径无锁）
    std::atomic<uint64_t> stat_blocks_spilled_{0};
    std::atomic<uint64_t> stat_bytes_written_{0};
    std::atomic<uint64_t> stat_lru_hits_{0};
    std::atomic<uint64_t> stat_lru_misses_{0};
    std::atomic<uint64_t> stat_total_read_us_{0};
    std::atomic<uint64_t> stat_queue_peak_{0};
    std::atomic<uint64_t> stat_write_stalls_{0};
    std::atomic<uint64_t> ram_bytes_in_use_{0};
};
```

### 4.2 文件布局（per-channel spill 文件）

```
spill_dir/dsview_spill_{session_id}_ch{N}.bin
```

每个文件：追加写入，每次追加恰好 `LeafBlockSpace` 字节（不压缩，保持 mipmap 完整）。
`block_offsets_[key] = file_offset` 记录 block_id 在文件中的起始位置，用于随机读。

---

## 5. `LogicSnapshot` 改动

**文件：** `DSView/pv/data/logicsnapshot.h` / `logicsnapshot.cpp`

### 5.1 成员变量（新增）

```cpp
// logicsnapshot.h — private 区
SpillManager* _spill_manager = nullptr;   // 注入；nullptr = 磁盘缓存关闭
uint64_t      _ram_usage_bytes = 0;       // 当前已 malloc 的 leaf block 字节数
```

### 5.2 `append_cross_payload()` 改动

在每个 leaf block `malloc` 成功后，在现有逻辑末尾加：

```cpp
// 新 leaf block 分配完成后记账
if (_spill_manager) {
    _ram_usage_bytes += LeafBlockSpace;
    _spill_manager->notify_ram_usage(_ram_usage_bytes);

    // 若超过 RAM 上限，淘汰最老的已填满 block
    if (_spill_manager->should_spill()) {
        spill_oldest_block();   // 见 5.3
    }
}
```

### 5.3 新增 `spill_oldest_block()`

```cpp
void LogicSnapshot::spill_oldest_block() {
    // 遍历 _ch_data，找到全局最老的已满 leaf block（最小 (root_idx, lbp_idx)）
    // 对每个 channel 的同一 block_id 落盘（保持多通道同步）
    for (uint16_t ch = 0; ch < _channel_num; ch++) {
        auto& rn = _ch_data[ch][oldest_root];
        void* ptr = rn.lbp[oldest_lbp];
        if (ptr == nullptr || ptr == (void*)SpillManager::SPILLED_SENTINEL)
            continue;

        uint64_t blk_id = oldest_root * Scale + oldest_lbp;
        _spill_manager->enqueue_spill(ch, blk_id, ptr, LeafBlockSpace);

        free(ptr);
        rn.lbp[oldest_lbp] = (void*)SpillManager::SPILLED_SENTINEL;
        _ram_usage_bytes -= LeafBlockSpace;
    }
}
```

### 5.4 `get_sample_unlock()` 改动

**改动前：**
```cpp
uint64_t *lbp = (uint64_t*)_ch_data[order][index0].lbp[index1];
return *(lbp + ...) & index_mask;
```

**改动后：**
```cpp
void* raw_ptr = _ch_data[order][index0].lbp[index1];
if (_spill_manager && raw_ptr == (void*)SpillManager::SPILLED_SENTINEL) {
    uint64_t blk_id = index0 * Scale + index1;
    raw_ptr = const_cast<void*>(_spill_manager->load_block(order, blk_id));
    if (!raw_ptr) return false;  // 读盘失败，返回默认值
}
uint64_t *lbp = (uint64_t*)raw_ptr;
return *(lbp + ...) & index_mask;
```

同样的改动适用于：`get_nxt_edge_unlock()`、`block_pre_edge()`、`calc_mipmap()`（每处访问 `lbp[k]` 之前加 sentinel 检查）。

### 5.5 Loop 模式分支

在 `append_cross_payload()` 的 loop 分支：

```cpp
if (_is_loop) {
    if (_spill_manager) {
        // 磁盘缓存开启：不覆盖，通过 spill_oldest_block() 控制RAM压力
        // 更新显示窗口偏移而非环形写入（由外部 View 负责）
        // 不调用 move_first_node_to_last() 也不调用 free_head_blocks()
    } else {
        // 磁盘缓存关闭：现有行为完全不变
        if (_loop_offset >= LeafBlockSamples * Scale) {
            move_first_node_to_last();
            ...
        }
    }
}
```

### 5.6 注入接口

```cpp
// logicsnapshot.h — public
inline void set_spill_manager(SpillManager* mgr) {
    _spill_manager = mgr;
}
```

---

## 6. `SigSession` 改动

**文件：** `DSView/pv/sigsession.h` / `sigsession.cpp`

### 6.1 成员变量

```cpp
// sigsession.h — private
SpillManager* _spill_manager = nullptr;
bool          _disk_cache_enabled = false;
uint64_t      _disk_cache_ram_limit = 0;   // bytes
uint64_t      _disk_cache_disk_limit = 0;  // bytes
std::string   _disk_cache_dir;             // 临时文件目录
```

### 6.2 采集开始时

在 `feed_in_logic()` 调用 `first_payload()` 之前：

```cpp
if (_disk_cache_enabled && !_spill_manager) {
    std::string session_id = generate_session_id();  // timestamp + random
    _spill_manager = new SpillManager(
        _disk_cache_ram_limit,
        _disk_cache_disk_limit,
        _disk_cache_dir
    );
    if (!_spill_manager->init_channels(channel_count, session_id)) {
        dsv_err("SpillManager: failed to init, disk cache disabled for this session");
        delete _spill_manager;
        _spill_manager = nullptr;
    }
}
if (_spill_manager)
    _capture_data->get_logic()->set_spill_manager(_spill_manager);
```

### 6.3 采集结束时

在 `session_error()` 和正常结束回调处：

```cpp
if (_spill_manager) {
    _spill_manager->log_stats_summary();
    // SpillManager 析构时自动删除 spill 文件（Q4-A：临时文件策略）
    delete _spill_manager;
    _spill_manager = nullptr;
    _capture_data->get_logic()->set_spill_manager(nullptr);
}
```

### 6.4 应用启动时

在 `AppControl` 或 `main()` 初始化处：

```cpp
SpillManager::cleanup_stale_files(get_temp_dir());
```

---

## 7. UI 改动

### 7.1 设备选项对话框（deviceoptions）

在「Device Options」对话框新增「磁盘缓存」区块：

| 控件 | 类型 | 说明 |
|------|------|------|
| 启用磁盘缓存 | QCheckBox | 默认关闭 |
| RAM 热区大小 | QComboBox | 原 sw_depth 选项（含义变为 RAM 热区上限） |
| 磁盘总深度 | QComboBox | 64GB / 128GB / 256GB / 512GB / 1TB / 无限制 |
| 缓存目录 | QLineEdit + 浏览按钮 | 默认系统 temp 目录；显示当前可用空间 |

磁盘缓存关闭时，上述三个控件 disable（灰色），行为与现在完全一致。

### 7.2 SamplingBar 标签

磁盘缓存开启时，buffer/depth 标签显示为：
```
RAM: 4GB / Disk: 256GB
```
磁盘缓存关闭时标签保持不变：
```
Buffer: 4GB
```

**改动范围：** `SamplingBar::update_buffer_label()` 加条件分支。

---

## 8. 性能保障措施

### 8.1 写路径（不阻塞采集线程）

- 写队列最大容量：32 个 WriteJob（可配置）
- 当队列满时：`enqueue_spill()` **阻塞**采集线程（比丢数据更可接受）
- 每次阻塞记录 `stat_write_stalls_++`
- SSD写速 < 数据率时，背压会将采集帧率降低，避免 OOM

### 8.2 读路径（LRU + 预取）

- LRU 固定 128 槽 × LeafBlockSpace ≈ **256MB** 读缓存
- 每次 `load_block(ch, blk_id)` 后，异步预取 `(ch, blk_id+1)`（投机预取）
- LRU 替换策略：LFU-tiny（least recently used）
- `load_block()` 在 UI 线程调用时，若 miss 则同步阻塞：NVMe ≈ 0.5ms/block，SATA ≈ 2ms

### 8.3 磁盘空间检查

- 采集开始前检查 `disk_depth` 对应目录可用空间
- 若不足，弹出警告对话框并降级（仅 RAM 模式）
- 采集中若 `disk_full()`，停止落盘并发送 `session_error()`

---

## 9. 日志策略

使用现有 `dsv_info()` / `dsv_err()` / `dsv_warn()`（`log.h`）。

| 日志点 | 级别 | 频率 |
|--------|------|------|
| SpillManager 创建（路径、RAM限制、Disk限制） | INFO | 一次 |
| 第一个 block 落盘触发 | INFO | 一次 |
| 每 100 个 block 落盘进度 | INFO | 每100块 |
| 写队列背压触发 | WARN | 每次 |
| 磁盘读 miss 且 > 5ms | WARN | 每次 |
| 采集结束性能摘要 | INFO | 一次 |
| 临时文件创建 / 删除 | INFO | 一次 |
| 写盘 I/O 错误 | ERROR | 每次 |
| 磁盘满 | ERROR | 一次 |
| 残留文件清理 | INFO | 启动时 |

**热路径规则：** `append_cross_payload()` 和 `get_sample_unlock()` 中**只做原子计数器操作**，不调用任何日志函数。

**调试构建：** 定义 `SPILL_DEBUG=1` 启用每块落盘/加载详细日志，Release 完全编译掉。

---

## 10. 错误处理

| 场景 | 处理 |
|------|------|
| spill 文件创建失败（目录无写权限） | `init_channels()` 返回 false → 该次采集降级为 RAM 模式 |
| 写盘 I/O 错误 | 停止 I/O 线程 → 通知 `SigSession::session_error()` → 停止采集 |
| 磁盘满 | 同上 |
| `load_block()` 读失败 | 返回 `nullptr`，调用方返回默认值（false/0），波形可能出现空洞 |
| 崩溃残留文件 | 启动时 `cleanup_stale_files()` 按文件名模式删除 |

---

## 11. 测试计划

### 11.1 功能正确性

1. **数据一致性**：同设备同参数，RAM 模式 vs. 磁盘缓存模式采集相同深度 → 逐样本 bit-identical
2. **Loop 模式历史保留**：Loop 采集 10 分钟，stop 后拖回最开头 → 波形完整
3. **超 RAM 深度**：`ram_depth=1GB, disk_depth=8GB`，采集 8G 样本 → 不 OOM
4. **采集中浏览**：单次模式采集中滚动到已落盘区域 → 波形正常
5. **StoreSession 保存**：采集完成后另存 .dsv → 重新加载数据完整
6. **解码验证**：UART/SPI 解码在磁盘缓存模式下结果与 RAM 模式一致

### 11.2 性能基准

| 指标 | 目标 |
|------|------|
| 采集数据率（16ch × 500MHz） | 磁盘缓存模式下 ≥ RAM 模式的 95% |
| LRU 命中率（典型波形平移） | > 95% |
| 磁盘读 P99（NVMe） | < 2ms/block |
| 写背压触发次数（NVMe）| 0（正常采集） |

### 11.3 回归测试

- 磁盘缓存 **关闭** → 运行单次/重复/滚动模式，与现有基准一致
- 磁盘缓存 **开启** + `disk_depth < sw_depth`（无实际落盘）→ 行为同关闭

---

## 12. 改动文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `DSView/pv/data/spillmanager.h` | 新增 | SpillManager 声明 |
| `DSView/pv/data/spillmanager.cpp` | 新增 | SpillManager 实现（I/O 线程、LRU、文件管理） |
| `DSView/pv/data/logicsnapshot.h` | 修改 | 新增 `_spill_manager`、`set_spill_manager()`、`spill_oldest_block()` |
| `DSView/pv/data/logicsnapshot.cpp` | 修改 | `append_cross_payload()` 淘汰逻辑；`get_sample_unlock()` / `get_nxt_edge_unlock()` / `block_pre_edge()` sentinel 检查；loop 模式分支 |
| `DSView/pv/sigsession.h` | 修改 | 新增 SpillManager 成员和磁盘缓存配置字段 |
| `DSView/pv/sigsession.cpp` | 修改 | 采集开始/结束时构造/销毁 SpillManager；注入 LogicSnapshot |
| `DSView/pv/dialogs/deviceoptions.h/.cpp` | 修改 | 新增「磁盘缓存」UI 区块 |
| `DSView/pv/toolbars/samplingbar.cpp` | 修改 | `update_buffer_label()` 显示 RAM/Disk 双深度标签 |
| `DSView/main.cpp` 或 `appcontrol.cpp` | 修改 | 启动时调用 `SpillManager::cleanup_stale_files()` |
| `DSView/CMakeLists.txt` | 修改 | 新增 `spillmanager.cpp`；可选 `SPILL_DEBUG` 选项 |

---

## 13. 阶段划分

| 阶段 | 内容 | 完成标志 |
|------|------|---------|
| Phase 1（本次） | SpillManager 框架 + LogicSnapshot 集成 + SigSession 生命周期 | 功能测试 1-3 通过 |
| Phase 2 | 解码器 block 级 API；LZ4 压缩；单次模式边采边解码 | 解码验证测试通过 |
| Phase 3 | UI 完善：磁盘空间预警、实时 spill 进度显示 | UX 验收 |

---

## 14. 记录

| 字段 | 值 |
|------|-----|
| 创建 | 2026-06-01 |
| 关联代码 | `LogicSnapshot`, `SigSession::feed_in_logic`, `StoreSession`, `SamplingBar` |
| 关联问题 | `docs/stream-disk-cached-questions.md` |
| 关联旧 brainstorm | `docs/superpowers/specs/2026-05-18-stream-disk-cache-brainstorm-questions.md` |
