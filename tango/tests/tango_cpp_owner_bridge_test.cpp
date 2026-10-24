// Real C++17 consumer of the C11 Tango owner/wakeup ABI. Not linked by the
// production Tango library; this must work on Linux, macOS and Windows.
#include "tui.h"
#include "tui_app_internal.h"
#include "tui_async_wait_internal.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#ifdef _WIN32
constexpr intptr_t kNoInputHandle = 0;
#else
constexpr intptr_t kNoInputHandle = -1;
#endif

struct OwnerModel {
    int updates = 0;
    uint32_t latest = 0;
};

extern "C" bool on_keyboard(int, void*) { return true; }
extern "C" int map_keyboard(const tui_event_t*) { return -1; }
extern "C" void on_view(const void*, tui_buffer_t*) {}

extern "C" bool on_async(const tui_async_event_t* event,
                          const void* payload, void* state) {
    auto* model = static_cast<OwnerModel*>(state);
    if (!event || !payload || event->payload_size != sizeof(uint32_t))
        return false;
    uint32_t value = 0;
    // Access the copied event payload as bytes; no cross-language ownership.
    const auto* bytes = static_cast<const unsigned char*>(payload);
    for (size_t i = 0; i < sizeof(value); ++i)
        reinterpret_cast<unsigned char*>(&value)[i] = bytes[i];
    ++model->updates;
    model->latest = value;
    return true;
}

int main() {
    // Link and execute the native C owner-dispatch symbol from a C++ client.
    if (tui_app_dispatch_once(nullptr)) return 1;

    OwnerModel model{};
    tui_app_t* app = tui_app_create(
        &model, on_keyboard, on_view, map_keyboard);
    if (!app) return 2;
    const bool admitted = tui_app_set_async_update(app, on_async);
    tui_async_queue_t* queue = tui_app_async_queue(app);
    if (!admitted || !queue) {
        tui_app_destroy(app);
        return 3;
    }
    if (!tui_async_queue_is_owner(queue)) {
        tui_app_destroy(app);
        return 4;
    }

    const uint32_t message = 0x2468ace0u;
    tui_async_status_t posted = TUI_ASYNC_INVALID_ARGUMENT;
    std::thread producer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        posted = tui_async_queue_post(
            queue, 1u, 1u, &message, sizeof(message), nullptr);
    });

    // The platform wait must be interrupted by an actual worker event.
    const auto wake = tui_async_wait_io(queue, kNoInputHandle, 1000);
    producer.join();
    const bool delivered = (wake == TUI_ASYNC_WAIT_MESSAGE) &&
                           (posted == TUI_ASYNC_OK);
    const bool pumped = (tui_app_pump_messages(app, 4u) == 1u);
    const bool content_ok = (model.updates == 1 &&
                             model.latest == message);
    const bool wake_retired =
        (tui_async_wait_io(queue, kNoInputHandle, 0) ==
         TUI_ASYNC_WAIT_TIMEOUT);
    const bool idle = tui_app_pump_messages(app, 4u) == 0u;

    // Cross-platform C++ callers may only borrow the queue. Shutdown is
    // owned by the Tango UI thread after the producer has joined.
    tui_app_destroy(app);
    if (!delivered || !pumped || !content_ok || !wake_retired || !idle) {
        std::fprintf(stderr,
            "C++ Tango owner ABI regression: delivered=%d pumped=%d"
            " content=%d retired=%d idle=%d\n",
            delivered, pumped, content_ok, wake_retired, idle);
        return 5;
    }
    return 0;
}
