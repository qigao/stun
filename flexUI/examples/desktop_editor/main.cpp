#include "native_timeline_widget.h"

#include <flexUI/application_turboscript.h>
#include <flexUI/gcanvas_plugin_window_host.h>

#include <cmeta_fs.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <memory>
#include <stdexcept>
#include <utility>
#include <string>
#include <string_view>

#ifndef FLEXUI_DESKTOP_EDITOR_ASSET_DIRECTORY_NAME
  #error FLEXUI_DESKTOP_EDITOR_ASSET_DIRECTORY_NAME must name the editor asset directory
#endif

#ifndef FLEXUI_DESKTOP_EDITOR_PLUGIN_FILE_NAME
  #error FLEXUI_DESKTOP_EDITOR_PLUGIN_FILE_NAME must name the document service plugin
#endif

namespace {

constexpr std::uint64_t kMaximumAssetBytes = 1024U * 1024U;
constexpr int kWindowWidth = 928;
constexpr int kWindowHeight = 608;
constexpr std::size_t kSmokePumpLimit = 8;
constexpr std::string_view kSavedText = "Saved by native document service";
constexpr auto kShutdownTimeout = std::chrono::seconds(1);

struct SourceBundle {
  std::string xml;
  std::string css;
  std::string script;
};

int fail(std::string_view stage, std::string_view message) {
  std::cerr << "FlexUI desktop editor failed during " << stage << ": " << message << '\n';
  return EXIT_FAILURE;
}

bool read_bounded_file(const std::filesystem::path &path, std::string &output, std::string &error) {
  const auto native_path = path.generic_string();
  cmeta_fs_stat_t metadata{};
  if (cmeta_fs_stat(native_path.c_str(), &metadata) != 0 || !metadata.is_file) {
    error = "asset is not a readable regular file: " + native_path;
    return false;
  }
  if (metadata.size > kMaximumAssetBytes) {
    error = "asset exceeds the 1 MiB source limit: " + native_path;
    return false;
  }

  cmeta_fs_buf_t bytes{};
  if (cmeta_fs_read_file(native_path.c_str(), &bytes) != 0) {
    error = "could not read asset: " + native_path;
    return false;
  }
  if (bytes.len > kMaximumAssetBytes || (bytes.len != 0U && bytes.base == nullptr)) {
    cmeta_fs_buf_free(&bytes);
    error = "asset changed or became invalid while being read: " + native_path;
    return false;
  }
  try {
    if (bytes.len == 0U) {
      output.clear();
    } else {
      output.assign(bytes.base, bytes.len);
    }
  } catch (const std::bad_alloc &) {
    cmeta_fs_buf_free(&bytes);
    error = "could not allocate storage for asset: " + native_path;
    return false;
  }
  cmeta_fs_buf_free(&bytes);
  return true;
}

bool load_sources(const std::filesystem::path &root, SourceBundle &sources, std::string &error) {
  return read_bounded_file(root / "editor.xml", sources.xml, error) &&
         read_bounded_file(root / "editor.css", sources.css, error) &&
         read_bounded_file(root / "editor.tbs", sources.script, error);
}

flexUI::GCanvasWindowHostConfig window_config(bool smoke) {
  flexUI::GCanvasWindowHostConfig config;
  config.window.title = "FlexUI Desktop Editor";
  config.window.width = kWindowWidth;
  config.window.height = kWindowHeight;
  config.window.visible = !smoke;
  config.window.decorated = !smoke;
  config.window.resizeable = !smoke;
  config.window.vsync = !smoke;
  config.window.backend = gcanvas::Backend::OpenGL;
  config.frame_mode = flexUI::GCanvasWindowHostFrameMode::EventDriven;
  return config;
}

flexUI::PluginHostBuildResult build_plugins(const std::filesystem::path &plugin_path) {
  flexUI::PluginHostBuilder builder;
  const auto loaded = builder.load_plugin(plugin_path);
  if (!loaded) {
    flexUI::PluginHostBuildResult failed;
    failed.error = loaded.error;
    return failed;
  }
  return builder.build();
}

flexUI::DesktopApplicationBuilder application_builder(
    SourceBundle sources, std::shared_ptr<flexui_demo::TimelineState> timeline_model) {
  auto registry = flexUI::WidgetRegistry::builtins();
  if (auto error = flexui_demo::register_timeline_widget(registry, std::move(timeline_model))) {
    throw std::runtime_error(error.message);
  }
  flexUI::TurboScriptControllerOptions script_options;
  script_options.execution_mode = flexUI::TurboScriptExecutionMode::Jit;

  flexUI::DesktopApplicationBuilder builder;
  builder.xml_entry(std::move(sources.xml))
      .widget_registry(std::move(registry))
      .stylesheet(std::move(sources.css))
      .script(std::move(sources.script),
              flexUI::make_turboscript_desktop_module_factory(script_options), "desktop-editor");
  return builder;
}

flexUI::ApplicationCapabilityManifest capability_manifest() {
  return {{"document.save/1"}, {"document.save/1"}};
}

flexUI::DesktopApplicationResult dispatch_save_click(flexUI::DesktopApplication &application,
                                                     const flexUI::Element &save) {
  const float click_x = save.absolute_x() + save.width() * 0.5F;
  const float click_y = save.absolute_y() + save.height() * 0.5F;
  auto down = flexUI::Event::mouse_down(click_x, click_y);
  auto up = flexUI::Event::mouse_up(click_x, click_y);
  const auto pressed = application.dispatch_event(down);
  return pressed ? application.dispatch_event(up) : pressed;
}

bool pump_until_saved(flexUI::GCanvasPluginWindowHost &host, const flexUI::Element &save,
                      std::string &error) {
  for (std::size_t attempt = 0; attempt < kSmokePumpLimit && save.text() != kSavedText; ++attempt) {
    const auto pumped = host.pump_once(0.0);
    if (!pumped) {
      error = pumped.error.message;
      return false;
    }
  }
  if (save.text() != kSavedText) {
    error = "document service did not update the Save button";
    return false;
  }
  return true;
}

int run_smoke(flexUI::GCanvasPluginWindowHost &host,
              const std::shared_ptr<flexui_demo::TimelineState> &timeline_model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) {
    return fail("initial frame", pumped.error.message);
  }


  // Verify a linked native widget inside the real XML/CSS/Script/PluginHost
  // application. Drag Clip A five frames; its end must magnetically snap to
  // the start of Clip B (frame 24), i.e. Clip A starts at frame 14.
  auto *timeline = host.application().box().get_by_id("timeline");
  if (!timeline || !timeline->widget) {
    return fail("native timeline lookup", "registered widget is absent");
  }
  const float x = timeline->absolute_x() + 20.0F + 8.0F * 12.0F + 30.0F;
  const float y = timeline->absolute_y() + 92.0F;
  auto down = flexUI::Event::mouse_down(x, y);
  if (!host.application().dispatch_event(down) || !down.handled ||
      host.application().box().capturing_element() != timeline) {
    return fail("native timeline pointer down", "widget did not capture the drag");
  }
  auto move = flexUI::Event::mouse_move(x + 5.0F * 12.0F, y);
  if (!host.application().dispatch_event(move) || !move.handled) {
    return fail("native timeline pointer move", "widget did not process the drag");
  }
  auto up = flexUI::Event::mouse_up(x + 5.0F * 12.0F, y);
  if (!host.application().dispatch_event(up) || !up.handled ||
      host.application().box().capturing_element() != nullptr) {
    return fail("native timeline pointer up", "widget did not retire capture");
  }
  if (timeline_model->clips[0].start_frame != 14 ||
      timeline_model->clips[0].length_frames != 10) {
    return fail("native timeline snap", "five-frame drag did not snap to Clip B");
  }
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("native timeline repaint", pumped.error.message);

  auto *save = host.application().box().get_by_id("save");
  if (save == nullptr) {
    return fail("smoke lookup", "compiled UI did not publish #save");
  }
  const auto first_click = dispatch_save_click(host.application(), *save);
  const auto duplicate_click =
      first_click ? dispatch_save_click(host.application(), *save) : first_click;
  if (!duplicate_click) {
    return fail("smoke click", duplicate_click.error.message);
  }

  std::string pump_error;
  if (!pump_until_saved(host, *save, pump_error)) {
    return fail("service completion", pump_error);
  }
  auto statistics = host.plugins().statistics();
  if (statistics.submitted != 1 || statistics.completed != 1) {
    return fail("duplicate save gate", "concurrent Save clicks did not collapse into one request");
  }

  const auto repeated_click = dispatch_save_click(host.application(), *save);
  if (!repeated_click) {
    return fail("repeat save click", repeated_click.error.message);
  }
  if (!pump_until_saved(host, *save, pump_error)) {
    return fail("repeat save completion", pump_error);
  }
  statistics = host.plugins().statistics();
  if (statistics.submitted != 2 || statistics.completed != 2) {
    return fail("repeat save", "completed Save request did not become reusable");
  }

  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) {
    return fail("shutdown", stopped.error.message);
  }
  std::cout << "FlexUI desktop editor smoke test passed: " << save->text() << '\n';
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char **argv) {
  bool smoke = false;
  if (argc == 2 && std::string_view(argv[1]) == "--smoke") {
    smoke = true;
  } else if (argc != 1) {
    return fail("arguments", "usage: flexui_desktop_editor [--smoke]");
  }

  std::error_code path_error;
  const auto executable = std::filesystem::absolute(argv[0], path_error);
  if (path_error || !executable.has_parent_path()) {
    return fail("runtime location", "could not resolve the executable directory");
  }
  const auto runtime_root = executable.parent_path();

  SourceBundle sources;
  std::string source_error;
  if (!load_sources(runtime_root / FLEXUI_DESKTOP_EDITOR_ASSET_DIRECTORY_NAME, sources,
                    source_error)) {
    return fail("source loading", source_error);
  }

  auto plugins = build_plugins(runtime_root / FLEXUI_DESKTOP_EDITOR_PLUGIN_FILE_NAME);
  if (!plugins) {
    return fail(plugins.error.stage, plugins.error.message);
  }

  auto timeline_model = std::make_shared<flexui_demo::TimelineState>();
  auto built = flexUI::GCanvasPluginWindowHost::create(
      window_config(smoke), application_builder(std::move(sources), timeline_model),
      std::move(plugins),
      capability_manifest(), kShutdownTimeout);
  if (!built) {
    return fail("runtime composition", built.error.message);
  }

  if (smoke) {
    return run_smoke(*built.host, timeline_model);
  }
  const auto ran = built.host->run();
  return ran ? EXIT_SUCCESS : fail("desktop loop", ran.error.message);
}
