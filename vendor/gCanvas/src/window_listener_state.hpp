#ifndef GCANVAS_WINDOW_LISTENER_STATE_HPP
#define GCANVAS_WINDOW_LISTENER_STATE_HPP

#include "gcanvas/window.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace gcanvas::detail
{
    template <typename Event>
    class WindowCallbackRegistry
    {
    public:
        using Callback = std::function<void(Event)>;

        void add_persistent(Callback callback)
        {
            validate(callback);
            _entries.push_back({0, std::move(callback), true});
        }

        void add_scoped(std::uint64_t id, Callback callback)
        {
            validate(callback);
            _entries.push_back({id, std::move(callback), true});
        }

        bool remove(std::uint64_t id) noexcept
        {
            if (id == 0)
                return false;
            for (Entry& entry : _entries)
            {
                if (entry.id == id && entry.active)
                {
                    entry.active = false;
                    if (_dispatch_depth == 0)
                        compact();
                    return true;
                }
            }
            return false;
        }

        bool contains(std::uint64_t id) const noexcept
        {
            if (id == 0)
                return false;
            for (const Entry& entry : _entries)
            {
                if (entry.id == id && entry.active)
                    return true;
            }
            return false;
        }

        void reset() noexcept
        {
            if (_dispatch_depth == 0)
            {
                _entries.clear();
                return;
            }
            for (Entry& entry : _entries)
                entry.active = false;
        }

        void publish(Event event)
        {
            ++_dispatch_depth;
            const std::size_t delivery_count = _entries.size();
            try
            {
                for (std::size_t index = 0; index < delivery_count; ++index)
                {
                    if (!_entries[index].active)
                        continue;
                    _entries[index].callback(event);
                }
            }
            catch (...)
            {
                finish_dispatch();
                throw;
            }
            finish_dispatch();
        }

    private:
        struct Entry
        {
            std::uint64_t id;
            Callback callback;
            bool active;
        };

        static void validate(const Callback& callback)
        {
            if (!callback)
                throw std::invalid_argument("gCanvas window listener callback must not be empty");
        }

        void finish_dispatch() noexcept
        {
            --_dispatch_depth;
            if (_dispatch_depth == 0)
                compact();
        }

        void compact() noexcept
        {
            _entries.erase(std::remove_if(_entries.begin(), _entries.end(),
                                          [](const Entry& entry) { return !entry.active; }),
                           _entries.end());
        }

        // End insertion keeps references stable while a callback registers
        // another listener. Erasure is deferred until the outer dispatch ends.
        std::deque<Entry> _entries;
        std::size_t _dispatch_depth = 0;
    };

    class WindowListenerState final : public std::enable_shared_from_this<WindowListenerState>
    {
    public:
        void add_resize(std::function<void(resize_event)> callback)
        {
            _resize.add_persistent(std::move(callback));
        }
        void add_mouse_move(std::function<void(mouse_move_event)> callback)
        {
            _mouse_move.add_persistent(std::move(callback));
        }
        void add_mouse_button(std::function<void(mouse_button_event)> callback)
        {
            _mouse_button.add_persistent(std::move(callback));
        }
        void add_key(std::function<void(key_event)> callback)
        {
            _key.add_persistent(std::move(callback));
        }
        void add_char(std::function<void(char_event)> callback)
        {
            _char.add_persistent(std::move(callback));
        }
        void add_scroll(std::function<void(scroll_event)> callback)
        {
            _scroll.add_persistent(std::move(callback));
        }
        void add_focus(std::function<void(focus_event)> callback)
        {
            _focus.add_persistent(std::move(callback));
        }
        void add_close(std::function<void(close_event)> callback)
        {
            _close.add_persistent(std::move(callback));
        }

        WindowListenerSubscription subscribe_resize(std::function<void(resize_event)> callback)
        {
            return subscribe(_resize, std::move(callback));
        }
        WindowListenerSubscription subscribe_mouse_move(
            std::function<void(mouse_move_event)> callback)
        {
            return subscribe(_mouse_move, std::move(callback));
        }
        WindowListenerSubscription subscribe_mouse_button(
            std::function<void(mouse_button_event)> callback)
        {
            return subscribe(_mouse_button, std::move(callback));
        }
        WindowListenerSubscription subscribe_key(std::function<void(key_event)> callback)
        {
            return subscribe(_key, std::move(callback));
        }
        WindowListenerSubscription subscribe_char(std::function<void(char_event)> callback)
        {
            return subscribe(_char, std::move(callback));
        }
        WindowListenerSubscription subscribe_scroll(std::function<void(scroll_event)> callback)
        {
            return subscribe(_scroll, std::move(callback));
        }
        WindowListenerSubscription subscribe_focus(std::function<void(focus_event)> callback)
        {
            return subscribe(_focus, std::move(callback));
        }
        WindowListenerSubscription subscribe_close(std::function<void(close_event)> callback)
        {
            return subscribe(_close, std::move(callback));
        }

        void publish(resize_event event)
        {
            _resize.publish(event);
        }
        void publish(mouse_move_event event)
        {
            _mouse_move.publish(event);
        }
        void publish(mouse_button_event event)
        {
            _mouse_button.publish(event);
        }
        void publish(key_event event)
        {
            _key.publish(event);
        }
        void publish(char_event event)
        {
            _char.publish(event);
        }
        void publish(scroll_event event)
        {
            _scroll.publish(event);
        }
        void publish(focus_event event)
        {
            _focus.publish(event);
        }
        void publish(close_event event)
        {
            _close.publish(event);
        }

        bool remove(std::uint64_t id) noexcept
        {
            return _resize.remove(id) || _mouse_move.remove(id) || _mouse_button.remove(id) ||
                   _key.remove(id) || _char.remove(id) || _scroll.remove(id) || _focus.remove(id) ||
                   _close.remove(id);
        }

        bool contains(std::uint64_t id) const noexcept
        {
            return _resize.contains(id) || _mouse_move.contains(id) || _mouse_button.contains(id) ||
                   _key.contains(id) || _char.contains(id) || _scroll.contains(id) ||
                   _focus.contains(id) || _close.contains(id);
        }

        void reset() noexcept
        {
            _resize.reset();
            _mouse_move.reset();
            _mouse_button.reset();
            _key.reset();
            _char.reset();
            _scroll.reset();
            _focus.reset();
            _close.reset();
        }

    private:
        template <typename Event>
        WindowListenerSubscription subscribe(WindowCallbackRegistry<Event>& registry,
                                             std::function<void(Event)> callback)
        {
            if (_next_id == 0)
                throw std::overflow_error("gCanvas window listener ID space exhausted");
            const std::uint64_t id = _next_id++;
            registry.add_scoped(id, std::move(callback));
            return WindowListenerSubscription(weak_from_this(), id);
        }

        std::uint64_t _next_id = 1;
        WindowCallbackRegistry<resize_event> _resize;
        WindowCallbackRegistry<mouse_move_event> _mouse_move;
        WindowCallbackRegistry<mouse_button_event> _mouse_button;
        WindowCallbackRegistry<key_event> _key;
        WindowCallbackRegistry<char_event> _char;
        WindowCallbackRegistry<scroll_event> _scroll;
        WindowCallbackRegistry<focus_event> _focus;
        WindowCallbackRegistry<close_event> _close;
    };
} // namespace gcanvas::detail

#endif // GCANVAS_WINDOW_LISTENER_STATE_HPP
