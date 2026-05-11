# 并行解码器线程方案

## 背景

DSView 支持同时添加多个 Python 协议解码器（Protocol Decoder，PD）。在改动之前，所有解码器共用一个全局 `std::thread`，通过任务队列依次执行——第一个解码器跑完之后，才开始第二个。当用户添加多个解码器时，等待时间随数量线性增长。

## 问题根因

`SigSession` 维护了一个单线程串行队列：

```
// 改动前（sigsession.cpp）
void SigSession::decode_task_proc() {
    while (task = get_top_decode_task()) {
        task->decoder()->begin_decode_work();  // 阻塞，等待整个解码完成
        ...
    }
    _is_decoding = false;
}
```

`begin_decode_work()` 是同步阻塞的，导致所有解码器串行执行。

## 解决方案

### 核心思路：每个 DecoderStack 拥有独立线程

参考 atk-logic 的设计（每个 `DecodeController` 拥有一个独立的 `QThread`），给每个 `DecoderStack` 分配自己的 `std::thread`，让多个解码器并发运行。

### 架构图

```
改动前：

SigSession._decode_thread
  └─ decode_task_proc()
       ├─ DecoderStack A (begin_decode_work → 阻塞) ──→ done
       ├─ DecoderStack B (begin_decode_work → 阻塞) ──→ done   ← 串行
       └─ DecoderStack C (begin_decode_work → 阻塞) ──→ done

改动后：

SigSession.add_decode_task()
  ├─ DecoderStack A._own_thread ──────────────────────→ done ─┐
  ├─ DecoderStack B._own_thread ──────────────────────→ done ─┤ 并行
  └─ DecoderStack C._own_thread ──────────────────────→ done ─┘
       所有线程完成后：_running_decoder_count 归零 → decode_end()
```

---

## 关键改动

### 1. `DecoderStack`：拥有独立线程

**文件：** `DSView/pv/data/decoderstack.h` / `decoderstack.cpp`

```cpp
// 新增成员
std::atomic<decode_state>  _decode_state;    // 原为 plain enum，改为原子变量
std::atomic<bool>          _task_active{false}; // 防止计数器双重递减
std::thread                _own_thread;         // 每个 DecoderStack 独占线程
```

`begin_decode_work()` 从同步改为异步：

```cpp
void DecoderStack::begin_decode_work() {
    assert(_decode_state == Stopped);
    join_own_thread();                  // 等待上次运行彻底结束
    _decode_state = Running;
    _own_thread = std::thread([this]() {
        do_decode_work();
        _decode_state = Stopped;
        bool was_cancelled = _stask_stauts && _stask_stauts->_bStop;
        if (!was_cancelled && !_session->is_closed()) {
            emit decode_done();         // 只在正常完成时发信号
        }
    });
    // 立即返回，解码在 _own_thread 后台运行
}
```

`stop_decode_work()` 和析构函数均 join 线程，确保不存在悬空线程：

```cpp
void DecoderStack::stop_decode_work() {
    if (_stask_stauts) _stask_stauts->_bStop = true;
    _decode_state = Stopped;
    join_own_thread();   // 等待线程真正退出
}

DecoderStack::~DecoderStack() {
    join_own_thread();   // 析构前必须 join
    ...
}
```

### 2. `SigSession`：移除串行队列，直接并行分发

**文件：** `DSView/pv/sigsession.h` / `sigsession.cpp`

移除的内容：
- `std::thread _decode_thread`（全局串行解码线程）
- `std::deque _decode_tasks`（任务队列）
- `decode_task_proc()`（串行循环函数）
- `get_top_decode_task()`
- `volatile bool _is_decoding`（不再精确）

新增内容：

```cpp
// sigsession.h
std::atomic<int>  _running_decoder_count{0};   // 当前活跃解码器数量
QHash<pv::data::DecoderStack*, QMetaObject::Connection>
                  _decode_connections;          // 精确管理信号连接

bool is_decoding() { return _running_decoder_count.load() > 0; }
```

`add_decode_task()` 从管理线程生命周期简化为：

```cpp
void SigSession::add_decode_task(view::DecodeTrace *trace) {
    _running_decoder_count.fetch_add(1);
    trace->decoder()->set_task_active(true);

    // 连接完成信号（精确存储句柄，避免泄漏）
    auto *ds = trace->decoder();
    _decode_connections[ds] = connect(ds, &DecoderStack::decode_done,
        this, [this, ds]() {
            if (ds->clear_task_active()) {
                if (_running_decoder_count.fetch_sub(1) == 1) {
                    // 最后一个解码器完成
                    _view_data->get_logic()->decode_end();
                    _callback->decode_done();
                }
            }
        }, Qt::QueuedConnection);

    trace->decoder()->begin_decode_work();  // 非阻塞，立即返回
}
```

---

## 线程安全设计细节

### `_task_active` 原子标志：防止计数器双重递减

解码完成有两条路径：
1. **正常完成**：`_own_thread` lambda 触发 `decode_done` 信号 → slot 递减计数器
2. **强制取消**：`remove_decode_task()` 调用 `stop_decode_work()` join 线程 → 直接递减计数器

两条路径都可能同时发生（lambda 已入 Qt 事件队列，同时 remove 也在运行）。`_task_active.exchange(false)` 的原子交换保证只有**第一个到达的路径**能得到 `true`，另一条路径得到 `false` 后跳过递减：

```cpp
// 两条路径都执行相同的 CAS 操作
if (ds->clear_task_active()) {   // atomic exchange(false)，返回旧值
    _running_decoder_count.fetch_sub(1);
    if (count == 1) decode_end();
}
```

### `_running_decoder_count`：用原子整数替代 vector 遍历

旧的 `is_decoding()` 遍历 `_decode_traces` vector，而该 vector 可能在 UI 线程被修改，存在 data race。新方案用 `std::atomic<int>` 一次原子 load 完成判断，无需锁，任意线程调用均安全。

### `bNotFree` 缓冲区保护

在 `first_payload()` 前有一个关键安全检查：

```cpp
bool bNotFree = is_decoding() && _view_data == _capture_data;
```

只要有任何解码器仍在运行（`_running_decoder_count > 0`），采集缓冲区就不会被释放，避免解码线程出现 use-after-free。

### 信号连接管理

每次 `add_decode_task()` 连接一个 lambda 到 `decode_done` 信号。通过 `QHash<DecoderStack*, QMetaObject::Connection>` 存储连接句柄，确保：
- `remove_decode_task()`：只断开 per-task 的计数 lambda，保留 `DecodeTrace::on_decode_done` 的 UI 更新连接
- `clear_all_decode_task()`：批量断开所有 lambda 连接后再 join，防止残留 lambda 在 stop 后修改计数器

---

## Python GIL 的影响

libsigrokdecode 的 Python 解码器受 Python GIL（全局解释器锁）约束。多线程下 Python 代码在微观上是时间片轮转（非真正并行），但：

1. **喂数据阶段（C 层循环）**：`srd_session_send()` 在传递采样数据时处于 C 层，GIL 可被释放，各解码器的数据处理阶段**真正并行**
2. **Python decode() 函数**：GIL 时间片共享，多解码器交替运行，总吞吐量优于串行
3. **libsigrokdecode 线程模型**：`srd_init()` 调用 `PyEval_InitThreads()` + `PyEval_SaveThread()`，每个 `srd_session` 的 `di_thread` 使用 `PyGILState_Ensure()` 独立管理 GIL，多 session 并发是安全的

实际效果：对于 CPU 密集型或数据量大的解码器（如 USB、SPI 等），并行运行有明显提速；对于轻量解码器，提升较小但不退步。

---

## 涉及文件

| 文件 | 改动 |
|------|------|
| `DSView/pv/data/decoderstack.h` | 新增 `_own_thread`, `_task_active`, atomic `_decode_state` |
| `DSView/pv/data/decoderstack.cpp` | `begin_decode_work` 异步化，`stop_decode_work`/析构加 join |
| `DSView/pv/sigsession.h` | 移除串行队列成员，新增 `_running_decoder_count`, `_decode_connections` |
| `DSView/pv/sigsession.cpp` | 移除 `decode_task_proc`，简化 `add/remove_decode_task` |

