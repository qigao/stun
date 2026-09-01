#include "window_listener_state.hpp"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<gcanvas::WindowListenerSubscription>);
static_assert(!std::is_copy_assignable_v<gcanvas::WindowListenerSubscription>);
static_assert(std::is_nothrow_move_constructible_v<gcanvas::WindowListenerSubscription>);
static_assert(std::is_nothrow_move_assignable_v<gcanvas::WindowListenerSubscription>);

int main()
{
    auto listeners = std::make_shared<gcanvas::detail::WindowListenerState>();

    int persistent_calls = 0;
    listeners->add_resize([&](gcanvas::resize_event) { ++persistent_calls; });
    listeners->publish(gcanvas::resize_event{64, 48});
    if (persistent_calls != 1)
        return 1;

    int scoped_calls = 0;
    {
        auto subscription =
            listeners->subscribe_resize([&](gcanvas::resize_event) { ++scoped_calls; });
        if (!subscription.active())
            return 2;
        auto moved = std::move(subscription);
        if (subscription.active() || !moved.active())
            return 3;
        listeners->publish(gcanvas::resize_event{80, 60});
    }
    listeners->publish(gcanvas::resize_event{96, 72});
    if (scoped_calls != 1 || persistent_calls != 3)
        return 4;

    int removed_later_calls = 0;
    gcanvas::WindowListenerSubscription later;
    auto earlier = listeners->subscribe_focus([&](gcanvas::focus_event) { later.reset(); });
    later = listeners->subscribe_focus([&](gcanvas::focus_event) { ++removed_later_calls; });
    listeners->publish(gcanvas::focus_event{true});
    if (!earlier.active() || later.active() || removed_later_calls != 0)
        return 5;

    int self_calls = 0;
    gcanvas::WindowListenerSubscription self;
    self = listeners->subscribe_close([&](gcanvas::close_event) {
        ++self_calls;
        self.reset();
    });
    listeners->publish(gcanvas::close_event{});
    listeners->publish(gcanvas::close_event{});
    if (self.active() || self_calls != 1)
        return 6;

    int added_during_delivery_calls = 0;
    gcanvas::WindowListenerSubscription added_during_delivery;
    auto adding = listeners->subscribe_scroll([&](gcanvas::scroll_event) {
        if (!added_during_delivery.active())
        {
            added_during_delivery = listeners->subscribe_scroll(
                [&](gcanvas::scroll_event) { ++added_during_delivery_calls; });
        }
    });
    listeners->publish(gcanvas::scroll_event{0.0, 1.0});
    if (added_during_delivery_calls != 0)
        return 7;
    listeners->publish(gcanvas::scroll_event{0.0, 1.0});
    if (!adding.active() || added_during_delivery_calls != 1)
        return 8;

    int reset_calls = 0;
    listeners->add_resize([&](gcanvas::resize_event) { ++reset_calls; });
    listeners->add_mouse_move([&](gcanvas::mouse_move_event) { ++reset_calls; });
    listeners->add_mouse_button([&](gcanvas::mouse_button_event) { ++reset_calls; });
    listeners->add_key([&](gcanvas::key_event) { ++reset_calls; });
    listeners->add_char([&](gcanvas::char_event) { ++reset_calls; });
    listeners->add_scroll([&](gcanvas::scroll_event) { ++reset_calls; });
    listeners->add_focus([&](gcanvas::focus_event) { ++reset_calls; });
    listeners->add_close([&](gcanvas::close_event) { ++reset_calls; });
    listeners->reset();
    listeners->publish(gcanvas::resize_event{1, 1});
    listeners->publish(gcanvas::mouse_move_event{1.0, 1.0});
    listeners->publish(gcanvas::mouse_button_event{});
    listeners->publish(gcanvas::key_event{});
    listeners->publish(gcanvas::char_event{});
    listeners->publish(gcanvas::scroll_event{});
    listeners->publish(gcanvas::focus_event{false});
    listeners->publish(gcanvas::close_event{});
    if (reset_calls != 0 || earlier.active() || adding.active() || added_during_delivery.active())
        return 9;

    int reset_during_delivery_calls = 0;
    auto resetting = listeners->subscribe_focus([&](gcanvas::focus_event) {
        ++reset_during_delivery_calls;
        listeners->reset();
    });
    auto reset_before_call =
        listeners->subscribe_focus([&](gcanvas::focus_event) { ++reset_during_delivery_calls; });
    listeners->publish(gcanvas::focus_event{false});
    listeners->publish(gcanvas::focus_event{true});
    if (reset_during_delivery_calls != 1 || resetting.active() || reset_before_call.active())
        return 10;

    bool rejected_empty_callback = false;
    try
    {
        listeners->subscribe_close({});
    }
    catch (const std::invalid_argument&)
    {
        rejected_empty_callback = true;
    }
    if (!rejected_empty_callback)
        return 11;

    gcanvas::WindowListenerSubscription orphan;
    {
        auto temporary = std::make_shared<gcanvas::detail::WindowListenerState>();
        orphan = temporary->subscribe_close([](gcanvas::close_event) {});
        if (!orphan.active())
            return 12;
    }
    if (orphan.active())
        return 13;
    orphan.reset();

    return 0;
}
