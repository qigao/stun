#ifndef GCANVAS_NATIVE_CALLBACK_ERROR_STATE_HPP
#define GCANVAS_NATIVE_CALLBACK_ERROR_STATE_HPP

#include <exception>
#include <utility>

namespace gcanvas::detail
{
    class NativeCallbackErrorState final
    {
    public:
        void capture(std::exception_ptr error) noexcept
        {
            if (_pending == nullptr && error != nullptr)
                _pending = std::move(error);
        }

        void capture_current() noexcept
        {
            capture(std::current_exception());
        }

        bool pending() const noexcept
        {
            return _pending != nullptr;
        }

        void rethrow_pending()
        {
            std::exception_ptr error = std::exchange(_pending, nullptr);
            if (error != nullptr)
                std::rethrow_exception(error);
        }

    private:
        std::exception_ptr _pending;
    };
} // namespace gcanvas::detail

#endif // GCANVAS_NATIVE_CALLBACK_ERROR_STATE_HPP
