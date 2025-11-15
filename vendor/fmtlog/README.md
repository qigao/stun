# fmtlog (vendored copy)

`fmtlog` is a high-performance asynchronous logger built on top of
[fmt](https://github.com/fmtlib/fmt). Each producer thread writes to its own
lock-free ring buffer, while a background consumer thread merges log events,
formats them with `fmt`, and flushes to disk.  

## Key Features
- **Low-overhead logging macros**: `logd`, `logi`, `logw`, `loge` expand to
  `FMTLOG(...)` and only format when the message passes the current log level.
- **Asynchronous batching**: messages are enqueued via SPSC queues and formatted
  in bulk by the background polling thread.
- **Rate limiting helpers**: `FMTLOG_ONCE` and `FMTLOG_LIMIT` guard noisy log
  sites.
- **Thread naming**: call `fmtlog::setThreadName()` so the consumer prints human
  friendly identifiers instead of raw IDs.
- **Flexible flushing**: configure flush delays, minimum flush level, or trigger
  manual flushes for crash-safe checkpoints.

## Basic Usage

```cpp
#include "fmtlog.h"

int main() {
  fmtlog::setLogLevel(fmtlog::INF);               // optional, defaults to INF
  fmtlog::setLogFile("app.log", /*truncate*/true);

  fmtlog::setThreadName("main");
  logi("Application started, pid={}", ::GetCurrentProcessId());

  // logd/logw/loge behave the same; use logio/logwo/logeo for "once".
  logw("Unexpected input '{}'", "foo");

  // Optional: wait for all pending logs before exit.
  fmtlog::shutdown();
}
```

NanoGUI’s build already compiles the bundled `fmtlog.cpp`, so you only need to
include `fmtlog.h`. If you prefer header-only integration in other projects,
define `FMTLOG_HEADER_ONLY` before including the header.

## Configuration Reference

Compile-time options (set before including `fmtlog.h`):

| Macro                | Default              | Description |
|----------------------|----------------------|-------------|
| `FMTLOG_QUEUE_SIZE`  | `1 << 20`            | Queue capacity per producer thread. |
| `FMTLOG_BLOCK`       | `0`                  | `1` to block when queues are full instead of dropping messages. |
| `FMTLOG_ACTIVE_LEVEL`| `FMTLOG_LEVEL_DBG`   | Compile-time log level filter. |

Runtime configuration helpers:

- `fmtlog::setLogLevel(level)`, `fmtlog::getLogLevel()`
- `fmtlog::setLogFile(const char* filename, bool truncate)` or
  `fmtlog::setLogFile(FILE* fp, bool manage)`
- `fmtlog::setFlushDelay(ns)`, `fmtlog::setFlushBufSize(bytes)`,
  `fmtlog::flushOn(level)`, `fmtlog::flush(forceFlush)` for manual draining
- `fmtlog::startPollingThread(intervalNs)` if you need to override the default
  interval (the thread is auto-started on first log call).
- `fmtlog::shutdown()` stops the polling thread, flushes remaining records, and
  closes the active log file.

## Polling Model

Each thread allocates its `ThreadBuffer` on demand. The first call that needs
the buffer also ensures the global polling thread is running. That background
thread repeatedly merges pending queues into a min-heap ordered by timestamp,
formats headers via `fmt`, and appends the payload to an in-memory buffer before
writing to the destination file. If you need deterministic flushing (for crash
repro steps, log snapshots, etc.), call `fmtlog::flush(true)` to have the worker
process all queues immediately. For full shutdown, use `fmtlog::shutdown()`.

## License

 MIT license (see headers).
