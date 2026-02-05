/*
 * Flex Engine - Debug and Profiling System
 *
 *
 * Features:
 *   - Profile data collection in background thread (sink mode)
 *   - Runtime control via environment variables
 *
 * Environment variables:
 *   - FLEX_DEBUG=1     : Enable debug logging at runtime
 *   - FLEX_PROFILE=1   : Enable profiling at runtime
 *   - FLEX_LOG_LEVEL=DBG|INF|WRN|ERR : Set log level
 *
 * The code is always compiled in, but checks env vars at runtime.
 * This allows enabling debug/profile without recompiling.
 */

#pragma once
 
#include <tlog.h>
#include <cstdlib>

namespace flex {
namespace debug {

// Runtime flags (checked once at startup via env vars)
inline bool is_debug_enabled() {
  static bool enabled = [] {
    const char* env = std::getenv("FLEX_DEBUG");
    return env && (env[0] == '1' || env[0] == 't' || env[0] == 'T');
  }();
  return enabled;
}

inline bool is_profile_enabled() {
  static bool enabled = [] {
    const char* env = std::getenv("FLEX_PROFILE");
    return env && (env[0] == '1' || env[0] == 't' || env[0] == 'T');
  }();
  return enabled;
}

inline turbo_log_level_t get_log_level() {
  static turbo_log_level_t level = [] {
    const char* env = std::getenv("FLEX_LOG_LEVEL");
    if (!env) return TURBO_LOG_LEVEL_DEBUG;
    if (env[0] == 'E' || env[0] == 'e') return TURBO_LOG_LEVEL_ERROR;
    if (env[0] == 'W' || env[0] == 'w') return TURBO_LOG_LEVEL_WARN;
    if (env[0] == 'I' || env[0] == 'i') return TURBO_LOG_LEVEL_INFO;
    return TURBO_LOG_LEVEL_DEBUG;
  }();
  return level;
}

} // namespace debug
} // namespace flex

// ============================================================================
// Logging Macros (runtime-controlled via FLEX_DEBUG env var)
// ============================================================================

#define FLEX_LOGD(fmt, ...) \
  do { if (::flex::debug::is_debug_enabled()) TLOG_DEBUG("[flex] " fmt, ##__VA_ARGS__); } while(0)

#define FLEX_LOGI(fmt, ...) \
  do { if (::flex::debug::is_debug_enabled()) TLOG_INFO("[flex] " fmt, ##__VA_ARGS__); } while(0)

#define FLEX_LOGW(fmt, ...) \
  do { if (::flex::debug::is_debug_enabled()) TLOG_WARN("[flex] " fmt, ##__VA_ARGS__); } while(0)

#define FLEX_LOGE(fmt, ...) \
  do { if (::flex::debug::is_debug_enabled()) TLOG_ERROR("[flex] " fmt, ##__VA_ARGS__); } while(0)

#define FLEX_DEBUG_BLOCK(code) \
  do { if (::flex::debug::is_debug_enabled()) { code; } } while(0)

// ============================================================================
// Profiling System with Background Sink (runtime-controlled via FLEX_PROFILE env var)
// ============================================================================

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace flex {
namespace profile {

// Profile event types
enum class EventType : uint8_t {
  ScopeBegin,
  ScopeEnd,
  Counter,
  FrameBegin,
  FrameEnd
};

// Profile event (lock-free submission to sink)
struct ProfileEvent {
  EventType type;
  const char* name;
  int64_t timestamp_ns;
  int64_t value;  // duration_ns for scope, value for counter
  uint32_t thread_id;
};

// ============================================================================
// Profile Sink (background thread for data collection)
// ============================================================================

class ProfileSink {
public:
  using Callback = std::function<void(const std::vector<ProfileEvent>&)>;

  static ProfileSink& instance() {
    static ProfileSink sink;
    return sink;
  }

  // Start background collection thread
  void start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this] { worker_loop(); });
  }

  // Stop and flush
  void stop() {
    if (!running_.exchange(false)) return;
    cv_.notify_one();
    if (worker_.joinable()) worker_.join();
    flush_buffer();
  }

  // Submit event (called from any thread)
  void submit(const ProfileEvent& event) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    event_queue_.push(event);
    if (event_queue_.size() >= flush_threshold_) {
      cv_.notify_one();
    }
  }

  // Set callback for processed events (e.g., send to UI, write to file)
  void set_callback(Callback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = std::move(cb);
  }

  // Get aggregated statistics
  struct ScopeStats {
    int64_t count = 0;
    int64_t total_ns = 0;
    int64_t min_ns = INT64_MAX;
    int64_t max_ns = 0;
    float avg_ns() const { return count > 0 ? static_cast<float>(total_ns) / count : 0; }
  };

  std::unordered_map<std::string, ScopeStats> get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return scope_stats_;
  }

  void reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    scope_stats_.clear();
    frame_stats_ = FrameStats{};
  }

  // Frame statistics
  struct FrameStats {
    int64_t frame_count = 0;
    int64_t total_ns = 0;
    int64_t min_ns = INT64_MAX;
    int64_t max_ns = 0;
    float avg_fps() const {
      if (frame_count == 0) return 0;
      float avg_ns = static_cast<float>(total_ns) / frame_count;
      return 1e9f / avg_ns;
    }
  };

  FrameStats get_frame_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return frame_stats_;
  }

  void set_flush_threshold(size_t n) { flush_threshold_ = n; }
  void set_poll_interval_ms(int ms) { poll_interval_ms_ = ms; }

private:
  ProfileSink() = default;
  ~ProfileSink() { stop(); }

  void worker_loop() {
    while (running_) {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      cv_.wait_for(lock, std::chrono::milliseconds(poll_interval_ms_),
                   [this] { return !running_ || event_queue_.size() >= flush_threshold_; });

      if (!event_queue_.empty()) {
        std::vector<ProfileEvent> batch;
        batch.reserve(event_queue_.size());
        while (!event_queue_.empty()) {
          batch.push_back(event_queue_.front());
          event_queue_.pop();
        }
        lock.unlock();
        process_batch(batch);
      }
    }
  }

  void process_batch(const std::vector<ProfileEvent>& batch) {
    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      for (const auto& e : batch) {
        if (e.type == EventType::ScopeEnd && e.name) {
          auto& stats = scope_stats_[e.name];
          stats.count++;
          stats.total_ns += e.value;
          stats.min_ns = (std::min)(stats.min_ns, e.value);
          stats.max_ns = (std::max)(stats.max_ns, e.value);
        } else if (e.type == EventType::FrameEnd) {
          frame_stats_.frame_count++;
          frame_stats_.total_ns += e.value;
          frame_stats_.min_ns = (std::min)(frame_stats_.min_ns, e.value);
          frame_stats_.max_ns = (std::max)(frame_stats_.max_ns, e.value);
        }
      }
    }

    Callback cb;
    {
      std::lock_guard<std::mutex> lock(callback_mutex_);
      cb = callback_;
    }
    if (cb) {
      cb(batch);
    }
  }

  void flush_buffer() {
    std::vector<ProfileEvent> remaining;
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      while (!event_queue_.empty()) {
        remaining.push_back(event_queue_.front());
        event_queue_.pop();
      }
    }
    if (!remaining.empty()) {
      process_batch(remaining);
    }
  }

  std::atomic<bool> running_{false};
  std::thread worker_;
  std::mutex queue_mutex_;
  std::condition_variable cv_;
  std::queue<ProfileEvent> event_queue_;

  mutable std::mutex stats_mutex_;
  std::unordered_map<std::string, ScopeStats> scope_stats_;
  FrameStats frame_stats_;

  std::mutex callback_mutex_;
  Callback callback_;

  size_t flush_threshold_ = 100;
  int poll_interval_ms_ = 16;  // ~60fps
};

// ============================================================================
// Scope Timer (RAII, submits to sink if profiling enabled)
// ============================================================================

class ScopeTimer {
public:
  ScopeTimer(const char* name)
      : name_(name), enabled_(::flex::debug::is_profile_enabled()) {
    if (enabled_) {
      start_ = std::chrono::high_resolution_clock::now();
    }
  }

  ~ScopeTimer() {
    if (!enabled_) return;

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();

    ProfileEvent event;
    event.type = EventType::ScopeEnd;
    event.name = name_;
    event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end.time_since_epoch()).count();
    event.value = ns;
    event.thread_id = static_cast<uint32_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));

    ProfileSink::instance().submit(event);
  }

private:
  const char* name_;
  bool enabled_;
  std::chrono::high_resolution_clock::time_point start_;
};

// ============================================================================
// Frame Timer
// ============================================================================

class FrameTimer {
public:
  void begin() {
    if (::flex::debug::is_profile_enabled()) {
      start_ = std::chrono::high_resolution_clock::now();
    }
  }

  void end() {
    if (!::flex::debug::is_profile_enabled()) return;

    auto end_time = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_).count();

    ProfileEvent event;
    event.type = EventType::FrameEnd;
    event.name = "frame";
    event.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time.time_since_epoch()).count();
    event.value = ns;
    event.thread_id = 0;

    ProfileSink::instance().submit(event);
  }

private:
  std::chrono::high_resolution_clock::time_point start_;
};

// Global frame timer
inline FrameTimer& frame_timer() {
  static FrameTimer timer;
  return timer;
}

} // namespace profile
} // namespace flex

// ============================================================================
// Profile Macros (runtime-controlled via FLEX_PROFILE env var)
// ============================================================================

#define FLEX_PROFILE_SCOPE(name) \
  ::flex::profile::ScopeTimer _flex_scope_##__LINE__(name)

#define FLEX_PROFILE_FUNC() \
  ::flex::profile::ScopeTimer _flex_func_(__func__)

#define FLEX_PROFILE_FRAME_BEGIN() \
  ::flex::profile::frame_timer().begin()

#define FLEX_PROFILE_FRAME_END() \
  ::flex::profile::frame_timer().end()

#define FLEX_PROFILE_COUNTER(name, value) do { \
    if (::flex::debug::is_profile_enabled()) { \
      ::flex::profile::ProfileEvent _e; \
      _e.type = ::flex::profile::EventType::Counter; \
      _e.name = name; \
      _e.value = static_cast<int64_t>(value); \
      _e.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>( \
          std::chrono::high_resolution_clock::now().time_since_epoch()).count(); \
      ::flex::profile::ProfileSink::instance().submit(_e); \
    } \
  } while(0)

// ============================================================================
// Debug System Initialization
// ============================================================================

namespace flex {
namespace debug {

// Initialize debug/profile system (call once at startup)
// Initialize debug/profile system (call once at startup)
inline void init(const char* log_file = nullptr) {
  if (is_debug_enabled()) {
    static bool initialized = false;
    if (!initialized) {
        tlog_config_t config = {};
        config.min_level = get_log_level();
        config.async_mode = 1;
        config.buffer_size = 64 * 1024;

        tlog_t* logger = tlog_create(&config);
        if (logger) {
            turbo_console_sink_opts_t copts = {};
            copts.output = stdout;
            copts.use_colors = 1;
            tlog_add_sink(logger, turbo_sink_console_create(&copts));

            if (log_file) {
                turbo_file_sink_opts_t fopts = {};
                fopts.path = log_file;
                fopts.max_size = 10 * 1024 * 1024;
                fopts.max_files = 3;
                tlog_add_sink(logger, turbo_sink_file_create(&fopts));
            }
            tlog_set_default(logger);
            initialized = true;
        }
    }
    TLOG_INFO("[flex] Debug mode enabled (FLEX_DEBUG=1)");
  }

  if (is_profile_enabled()) {
    profile::ProfileSink::instance().start();
    TLOG_INFO("[flex] Profiling enabled (FLEX_PROFILE=1)");
  }
}

// Shutdown - stop sink thread, flush logs
inline void shutdown() {
  if (is_profile_enabled()) {
    profile::ProfileSink::instance().stop();
  }
  if (is_debug_enabled() || is_profile_enabled()) {
    tlog_flush(tlog_get_default());
  }
}

// Get profile statistics
inline auto get_scope_stats() {
  return profile::ProfileSink::instance().get_stats();
}

inline auto get_frame_stats() {
  return profile::ProfileSink::instance().get_frame_stats();
}

inline void reset_profile_stats() {
  profile::ProfileSink::instance().reset_stats();
}

// Set callback for real-time profile data
inline void set_profile_callback(profile::ProfileSink::Callback cb) {
  profile::ProfileSink::instance().set_callback(std::move(cb));
}

} // namespace debug
} // namespace flex
