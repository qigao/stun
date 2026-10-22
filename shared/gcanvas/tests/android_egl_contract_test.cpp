#include "gcanvas/platform/android_egl.hpp"

#include <memory>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<gcanvas::android::AndroidEglHost>);
static_assert(!std::is_copy_assignable_v<gcanvas::android::AndroidEglHost>);
static_assert(!std::is_move_constructible_v<gcanvas::android::AndroidEglHost>);
static_assert(std::is_same_v<
              decltype(std::declval<gcanvas::android::AndroidEglHost&>().host_callbacks()),
              gcanvas::opengles::Host>);
static_assert(std::is_same_v<
              decltype(gcanvas::android::AndroidEglHost::create(
                  std::declval<ANativeWindow*>())),
              std::unique_ptr<gcanvas::android::AndroidEglHost>>);
static_assert(std::is_same_v<
              decltype(std::declval<gcanvas::android::AndroidEglHost&>().suspend(
                  std::declval<gcanvas::Context&>())),
              void>);
static_assert(std::is_same_v<
              decltype(std::declval<gcanvas::android::AndroidEglHost&>().resume(
                  std::declval<gcanvas::Context&>(), std::declval<ANativeWindow*>())),
              void>);

int main()
{
    return 0;
}
