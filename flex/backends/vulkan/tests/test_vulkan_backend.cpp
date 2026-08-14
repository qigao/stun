#include "backends/vulkan/init.h"
#include "tinytest.h"

#include <cstdint>
using namespace flex;

suite("flex::vulkan_backend") {
  it("loads the Vulkan API without requiring a physical device") {
    std::uint32_t api_version = VK_API_VERSION_1_0;

    check_int_eq(vkEnumerateInstanceVersion(&api_version), VK_SUCCESS);
    check_int_ge(static_cast<int>(VK_API_VERSION_MAJOR(api_version)), 1);
  }

  it("rejects an incomplete Vulkan canvas without touching a device") {
    vulkan_backend::VulkanCanvas canvas;

    check_false(vulkan_backend::is_valid_canvas(canvas));
    check_null(vulkan_backend::create_renderer(&canvas).get());
    check_null(create_vulkan_renderer(nullptr).get());
  }

  it("registers the Vulkan factory independently of GPU discovery") {
    const auto previous = renderer_backend_factory(RendererBackend::Vulkan);

    check_true(vulkan_backend::register_backend());
    check_true(is_renderer_backend_available(RendererBackend::Vulkan));
    check_str_eq(renderer_backend_name(RendererBackend::Vulkan), "Vulkan");

    register_renderer_backend(RendererBackend::Vulkan, previous);
  }

  it("accepts the documented native RGBA and BGRA attachment formats") {
    check_true(vulkan_backend::is_supported_format(VK_FORMAT_B8G8R8A8_UNORM));
    check_true(vulkan_backend::is_supported_format(VK_FORMAT_B8G8R8A8_SRGB));
    check_true(vulkan_backend::is_supported_format(VK_FORMAT_R8G8B8A8_UNORM));
    check_true(vulkan_backend::is_supported_format(VK_FORMAT_R8G8B8A8_SRGB));
    check_false(vulkan_backend::is_supported_format(VK_FORMAT_R16G16B16A16_SFLOAT));
  }
}
