#include "native_timeline_widget.h"
#include <charts/ui/graph_canvas_widget.h>
#include <charts/ui/graph_edit_document.h>

#include <flexUI/application_turboscript.h>
#include <flexUI/gcanvas_plugin_window_host.h>

#include <cmeta_fs.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <new>
#include <memory>
#include <stdexcept>
#include <utility>
#include <string>
#include <string_view>
#include <thread>

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

flexUI::GCanvasWindowHostConfig window_config(bool smoke, bool async_routing) {
  flexUI::GCanvasWindowHostConfig config;
  config.window.title = "FlexUI Desktop Editor";
  config.window.width = kWindowWidth;
  config.window.height = kWindowHeight;
  config.window.visible = !smoke;
  config.window.decorated = !smoke;
  config.window.resizeable = !smoke;
  config.window.vsync = !smoke;
  config.window.backend = gcanvas::Backend::OpenGL;
  // An opt-in async graph must be pumped without waiting for a new OS event.
  // Preserve the established event-driven default for synchronous canvases.
  config.frame_mode = async_routing
      ? flexUI::GCanvasWindowHostFrameMode::Continuous
      : flexUI::GCanvasWindowHostFrameMode::EventDriven;
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
    SourceBundle sources, std::shared_ptr<flexui_demo::TimelineState> timeline_model,
    std::shared_ptr<charts::ui::GraphEditDocument> graph_model,
    bool async_routing) {
  auto registry = flexUI::WidgetRegistry::builtins();
  if (auto error = flexui_demo::register_timeline_widget(registry, std::move(timeline_model))) {
    throw std::runtime_error(error.message);
  }
  if (auto error = charts::ui::register_graph_canvas_widget(
          registry, std::move(graph_model),
          async_routing ? charts::ui::GraphCanvasRoutingMode::AsyncCommit
                        : charts::ui::GraphCanvasRoutingMode::Synchronous)) {
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
              const std::shared_ptr<flexui_demo::TimelineState> &timeline_model,
              const std::shared_ptr<charts::ui::GraphEditDocument> &graph_model,
              bool async_routing, bool group_smoke) {
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

  // Drive the real linked GraphCanvas through precisely the same application
  // EventDispatcher/Box as Timeline, TurboScript, and PluginHost. Ports are
  // measured on node boundaries, and a connection is only published after
  // actual Stun::GraphOrthogonal routing + independent geometry validation.
  auto *graph = host.application().box().get_by_id("graph");
  if (!graph || !graph->widget) {
    return fail("native graph lookup", "registered GraphCanvas is absent");
  }
  const float gx = graph->absolute_x(), gy = graph->absolute_y();
  auto connect_down = flexUI::Event::mouse_down(gx + 180.0F, gy + 72.0F);
  if (!host.application().dispatch_event(connect_down) || !connect_down.handled ||
      host.application().box().capturing_element() != graph) {
    return fail("graph port down", "source port did not capture the connection");
  }
  auto connect_move = flexUI::Event::mouse_move(gx + 440.0F, gy + 72.0F);
  auto connect_up = flexUI::Event::mouse_up(gx + 440.0F, gy + 72.0F);
  if (!host.application().dispatch_event(connect_move) || !connect_move.handled ||
      !host.application().dispatch_event(connect_up) || !connect_up.handled ||
      host.application().box().capturing_element() != nullptr ||
      graph_model->snapshot().edges.size() != 1 ||
      graph_model->snapshot().routes.size() != 1) {
    return fail("graph connect", "measured native ports did not produce a committed route");
  }

  const auto original_revision = graph_model->snapshot().revision;
  auto drag_down = flexUI::Event::mouse_down(gx + 95.0F, gy + 56.0F);
  if (!host.application().dispatch_event(drag_down) || !drag_down.handled ||
      host.application().box().capturing_element() != graph) {
    return fail("graph move down", "node did not capture its drag");
  }
  auto drag_move = flexUI::Event::mouse_move(gx + 127.0F, gy + 70.0F);
  if (!host.application().dispatch_event(drag_move) || !drag_move.handled ||
      !graph_model->has_preview() ||
      graph_model->snapshot().revision != original_revision ||
      graph_model->snapshot().nodes.front().x != 55.0) {
    return fail("graph preview", "drag changed committed geometry before MouseUp");
  }
  auto drag_up = flexUI::Event::mouse_up(gx + 127.0F, gy + 70.0F);
  if (!host.application().dispatch_event(drag_up) || !drag_up.handled ||
      host.application().box().capturing_element() != nullptr) {
    return fail("graph release", "pointer capture remained after MouseUp");
  }
  if (async_routing) {
    if (graph_model->snapshot().revision != original_revision ||
        graph_model->snapshot().nodes.front().x != 55.0 ||
        graph_model->has_preview() ||
        !graph->attribute("data-routing") ||
        *graph->attribute("data-routing") != "pending") {
      return fail("graph async snapshot", "MouseUp committed before owner accepted worker");
    }
    // Actual OpenGL/PluginHost frames drive the attached Widget's frame
    // callback, not a test-only direct call to GraphRouteWorker::poll().
    constexpr int kMaximumAsyncPumps = 160;
    for (int i = 0; i < kMaximumAsyncPumps &&
         graph_model->snapshot().revision == original_revision; ++i) {
      pumped = host.pump_once(0.0);
      if (!pumped) return fail("graph async host pump", pumped.error.message);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (graph_model->snapshot().revision != original_revision + 1 ||
        !graph->attribute("data-routing") ||
        *graph->attribute("data-routing") != "committed") {
      return fail("graph async publish", "owner frame did not publish exact worker result");
    }
    // Semantic node identity belongs to the SAME Box and is updated on
    // the frame after the frozen worker result was committed.
    bool node_painted_from_result = false;
    for (auto *child : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(child);
      const auto *name = part->attribute("part");
      if (name && *name == "node:alpha" && part->x() == 87.0f)
        node_painted_from_result = true;
    }
    if (!node_painted_from_result)
      return fail("graph async frame geometry", "semantic node was not updated by owner frame");
  }
  if (graph_model->snapshot().nodes.front().x != 87.0 ||
      graph_model->snapshot().revision != original_revision + 1)
    return fail("graph commit", "drag did not commit exactly one versioned node edit");
  auto undo = flexUI::Event::key_down(flexUI::KeyCode::Z,
                                     static_cast<int>(flexUI::KeyMod::Control));
  auto redo = flexUI::Event::key_down(flexUI::KeyCode::Y,
                                     static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(undo) || !undo.handled ||
      graph_model->snapshot().nodes.front().x != 55.0 ||
      !host.application().dispatch_event(redo) || !redo.handled ||
      graph_model->snapshot().nodes.front().x != 87.0 ||
      graph_model->snapshot().routes.size() != 1) {
    return fail("graph Undo/Redo", "committed route and node history did not replay");
  }

  auto cancel_down = flexUI::Event::mouse_down(gx + 120.0F, gy + 70.0F);
  auto cancel_move = flexUI::Event::mouse_move(gx + 145.0F, gy + 76.0F);
  if (!host.application().dispatch_event(cancel_down) ||
      !host.application().dispatch_event(cancel_move) ||
      !graph_model->has_preview()) {
    return fail("graph cancel setup", "active drag preview not created");
  }
  auto escape = flexUI::Event::key_down(flexUI::KeyCode::Escape);
  if (!host.application().dispatch_event(escape) || !escape.handled ||
      graph_model->has_preview() ||
      host.application().box().capturing_element() != nullptr ||
      graph_model->snapshot().nodes.front().x != 87.0) {
    return fail("graph Escape", "cancel did not restore immutable committed geometry");
  }
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("native graph repaint", pumped.error.message);

  if (group_smoke) {
    // Run the GROUP gesture in the real OpenGL/PluginHost/Box owner. This is
    // independent of the existing native ChartUiTest and screenshot suite.
    // Ordinary graph editing above leaves alpha moved to (87,50).
    if (graph_model->snapshot().groups.size() != 1 ||
        graph_model->snapshot().groups[0].collapsed) {
      return fail("group source", "measured compound group was not mounted");
    }
    const auto revision = graph_model->snapshot().revision;
    auto group_down = flexUI::Event::mouse_down(gx + 60.0F, gy + 30.0F);
    auto group_up = flexUI::Event::mouse_up(gx + 60.0F, gy + 30.0F);
    if (!host.application().dispatch_event(group_down) || !group_down.handled ||
        host.application().box().capturing_element() != graph ||
        !host.application().dispatch_event(group_up) || !group_up.handled ||
        host.application().box().capturing_element() != nullptr ||
        graph_model->snapshot().revision != revision + 1 ||
        !graph_model->snapshot().groups[0].collapsed ||
        graph_model->snapshot().edges[0].source_port != "source") {
      return fail("group collapse", "native click failed to commit stable proxy");
    }
    if (graph_model->snapshot().routes.size() != 1 ||
        graph_model->snapshot().routes[0].points.front().x != 210.0) {
      return fail("group boundary", "collapsed route ignored measured proxy port");
    }
    const auto collapsed_revision = graph_model->snapshot().revision;
    auto group_drag_down = flexUI::Event::mouse_down(gx + 85.0F, gy + 91.0F);
    auto group_drag_move = flexUI::Event::mouse_move(gx + 109.0F, gy + 103.0F);
    auto group_drag_up = flexUI::Event::mouse_up(gx + 109.0F, gy + 103.0F);
    if (!host.application().dispatch_event(group_drag_down) ||
        !group_drag_down.handled ||
        host.application().box().capturing_element() != graph ||
        !host.application().dispatch_event(group_drag_move) ||
        !group_drag_move.handled ||
        !graph_model->has_preview() ||
        graph_model->snapshot().revision != collapsed_revision) {
      return fail("group preview", "real pointer capture did not preserve frozen geometry");
    }
    double preview_x = 0.0, preview_y = 0.0;
    if (!graph_model->preview_position("alpha", preview_x, preview_y) ||
        preview_x != 111.0 || preview_y != 62.0 ||
        !host.application().dispatch_event(group_drag_up) ||
        !group_drag_up.handled ||
        host.application().box().capturing_element() != nullptr ||
        graph_model->has_preview() ||
        graph_model->snapshot().revision != collapsed_revision + 1 ||
        graph_model->snapshot().groups[0].x != 64.0 ||
        graph_model->snapshot().groups[0].y != 36.0 ||
        graph_model->snapshot().nodes.front().x != 111.0 ||
        graph_model->snapshot().nodes.back().x != 440.0 ||
        graph_model->snapshot().edges[0].source_port != "source" ||
        graph_model->snapshot().routes[0].points.front().x != 234.0) {
      return fail("group commit", "compound geometry or logical endpoints were not atomic");
    }
    pumped = host.pump_once(0.0);
    if (!pumped) return fail("group owner repaint", pumped.error.message);
    bool proxy_semantics_updated = false;
    for (auto *child : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(child);
      const auto *name = part->attribute("part");
      if (name && *name == "node:group@pack" && part->x() == 64.0f &&
          part->attribute("aria-hidden") &&
          *part->attribute("aria-hidden") == "false") {
        proxy_semantics_updated = true;
      }
    }
    if (!proxy_semantics_updated)
      return fail("group owner semantics", "real Box did not publish the moved proxy");

    // One final click expands, without changing original member/edge IDs.
    auto expand_down = flexUI::Event::mouse_down(gx + 100.0F, gy + 110.0F);
    auto expand_up = flexUI::Event::mouse_up(gx + 100.0F, gy + 110.0F);
    if (!host.application().dispatch_event(expand_down) ||
        !host.application().dispatch_event(expand_up) ||
        !expand_up.handled || graph_model->snapshot().groups[0].collapsed ||
        graph_model->snapshot().nodes.front().x != 111.0 ||
        graph_model->snapshot().edges[0].source_port != "source")
      return fail("group expand", "expansion damaged logical identities");
    pumped = host.pump_once(0.0);
    if (!pumped) return fail("group expand repaint", pumped.error.message);
  }

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

// M2c: real linked OpenGL/PluginHost/Box, not a mock group graph or second
// standalone consumer. This follows one native frame and EventDispatcher
// through parent-local nested collapse, group drag, owner semantic updates,
// Undo/Redo and the same application Save service.
int run_hierarchy_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("hierarchy initial frame", pumped.error.message);
  auto *graph = host.application().box().get_by_id("graph");
  if (!graph || !graph->widget)
    return fail("hierarchy lookup", "real GraphCanvas is not mounted");
  const float gx = graph->absolute_x(), gy = graph->absolute_y();
  const auto find_node = [&](std::string_view id)
      -> const charts::ui::GraphEditNode * {
    for (const auto &node : model->snapshot().nodes)
      if (node.id == id) return &node;
    return nullptr;
  };
  const auto find_group = [&](std::string_view id)
      -> const charts::ui::GraphEditGroup * {
    for (const auto &group : model->snapshot().groups)
      if (group.id == id) return &group;
    return nullptr;
  };
  if (model->snapshot().groups.size() != 3 ||
      model->snapshot().routes.size() != 3)
    return fail("hierarchy source", "three-level original source did not route");
  charts::ui::GraphEditNode inner;
  if (!model->group_world_rect("inner", inner) ||
      inner.x != 70.0 || inner.y != 70.0)
    return fail("hierarchy local transform", "nested world resolution mismatch");
  const auto first_revision = model->snapshot().revision;

  // Choose the *inner* header. Its ancestors are expanded, but their larger
  // rectangles must not steal this child's capture.
  auto down = flexUI::Event::mouse_down(gx + 90.0F, gy + 78.0F);
  auto move = flexUI::Event::mouse_move(gx + 114.0F, gy + 86.0F);
  auto up = flexUI::Event::mouse_up(gx + 114.0F, gy + 86.0F);
  if (!host.application().dispatch_event(down) || !down.handled ||
      host.application().box().capturing_element() != graph ||
      !host.application().dispatch_event(move) || !move.handled ||
      !model->has_preview() ||
      model->snapshot().revision != first_revision)
    return fail("hierarchy preview", "nested child failed real Box capture");
  double px = 0, py = 0;
  if (!model->preview_position("group@inner", px, py) ||
      px != 94.0 || py != 78.0 ||
      !model->preview_position("a", px, py) ||
      px != 104.0 || py != 103.0 ||
      !host.application().dispatch_event(up) || !up.handled ||
      host.application().box().capturing_element() != nullptr ||
      model->has_preview() ||
      model->snapshot().revision != first_revision + 1)
    return fail("hierarchy child commit", "single-group motion was not atomic");
  const auto *inner_group = find_group("inner");
  const auto *middle_group = find_group("middle");
  const auto *outer_group = find_group("outer");
  if (!inner_group || !middle_group || !outer_group ||
      inner_group->x != 44.0 || inner_group->y != 28.0 ||
      middle_group->x != 30.0 || outer_group->x != 20.0 ||
      !find_node("a") || find_node("a")->x != 104.0 ||
      !find_node("b") || find_node("b")->x != 194.0 ||
      !find_node("c") || find_node("c")->x != 340.0)
    return fail("hierarchy child isolation", "parent-local coordinates mutated");
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("hierarchy child frame", pumped.error.message);

  // Collapse child; (a,b) and logical ab persist, but bc departs from the
  // precisely measured inner proxy boundary at x=294 (original x=270+24).
  auto child_down = flexUI::Event::mouse_down(gx + 110.0F, gy + 80.0F);
  auto child_up = flexUI::Event::mouse_up(gx + 110.0F, gy + 80.0F);
  if (!host.application().dispatch_event(child_down) ||
      !host.application().dispatch_event(child_up) || !child_up.handled ||
      !find_group("inner")->collapsed ||
      model->snapshot().routes.size() != 2 ||
      model->snapshot().routes[0].points.front().x != 294.0 ||
      model->snapshot().edges.size() != 3 ||
      model->snapshot().edges[1].source_port != "b-out")
    return fail("hierarchy inner collapse", "stable boundary remap failed");
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("hierarchy inner frame", pumped.error.message);
  bool has_inner_proxy = false;
  for (auto *child : graph->children()) {
    auto *part = static_cast<flexUI::Element *>(child);
    const auto *name = part->attribute("part");
    if (name && *name == "node:group@inner" &&
        part->x() == 94.0f && part->attribute("aria-hidden") &&
        *part->attribute("aria-hidden") == "false")
      has_inner_proxy = true;
  }
  if (!has_inner_proxy)
    return fail("hierarchy semantic proxy", "real Box did not publish child proxy");

  // Collapse outer: the child proxy must no longer be visible and logical
  // cd must originate from outer's explicit c-out mapping.
  auto outer_down = flexUI::Event::mouse_down(gx + 36.0F, gy + 28.0F);
  auto outer_up = flexUI::Event::mouse_up(gx + 36.0F, gy + 28.0F);
  if (!host.application().dispatch_event(outer_down) ||
      !host.application().dispatch_event(outer_up) || !outer_up.handled ||
      !find_group("outer")->collapsed ||
      model->group_is_visible("inner") ||
      model->snapshot().routes.size() != 1 ||
      model->snapshot().routes[0].edge_id != "cd" ||
      model->snapshot().routes[0].points.front().x != 440.0)
    return fail("hierarchy outer collapse", "outermost proxy choice failed");

  // The expanded descendants cannot steal a drag from the collapsed outer
  // proxy. Move the parent and verify all original descendants plus route.
  auto parent_down = flexUI::Event::mouse_down(gx + 80.0F, gy + 55.0F);
  auto parent_move = flexUI::Event::mouse_move(gx + 96.0F, gy + 63.0F);
  auto parent_up = flexUI::Event::mouse_up(gx + 96.0F, gy + 63.0F);
  const auto collapsed_revision = model->snapshot().revision;
  if (!host.application().dispatch_event(parent_down) ||
      !host.application().dispatch_event(parent_move) ||
      !model->has_preview() ||
      model->snapshot().revision != collapsed_revision ||
      !host.application().dispatch_event(parent_up) || !parent_up.handled ||
      model->has_preview() ||
      model->snapshot().revision != collapsed_revision + 1 ||
      find_group("outer")->x != 36.0 ||
      find_group("middle")->x != 30.0 ||
      find_group("inner")->x != 44.0 ||
      find_node("a")->x != 120.0 ||
      find_node("b")->x != 210.0 ||
      find_node("c")->x != 356.0 ||
      find_node("d")->x != 560.0 ||
      model->snapshot().routes[0].points.front().x != 456.0)
    return fail("hierarchy parent drag", "descendant world motion was not atomic");
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("hierarchy parent frame", pumped.error.message);
  bool has_outer_proxy = false;
  for (auto *child : graph->children()) {
    auto *part = static_cast<flexUI::Element *>(child);
    const auto *name = part->attribute("part");
    if (name && *name == "node:group@outer" &&
        part->x() == 36.0f && part->attribute("aria-hidden") &&
        *part->attribute("aria-hidden") == "false")
      has_outer_proxy = true;
  }
  if (!has_outer_proxy)
    return fail("hierarchy outer semantics", "real owner frame missed proxy motion");

  // Undo parent drag and both collapses, then child movement. All source
  // topology and local/world coordinates return exactly to the baseline.
  for (int i = 0; i < 4; ++i) {
    const auto undone = model->undo();
    if (!undone) return fail("hierarchy Undo", undone.message);
  }
  if (model->snapshot().nodes.size() != 4 ||
      model->snapshot().edges.size() != 3 ||
      model->snapshot().routes.size() != 3 ||
      find_group("inner")->collapsed ||
      find_group("outer")->collapsed ||
      find_group("inner")->x != 20.0 ||
      find_group("outer")->x != 20.0 ||
      find_node("a")->x != 80.0 ||
      find_node("c")->x != 340.0)
    return fail("hierarchy history", "exact original topology was not restored");

  // Retain the genuine PluginHost document Save check on this new source.
  auto *save = host.application().box().get_by_id("save");
  if (!save) return fail("hierarchy Save lookup", "no real #save element");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("hierarchy Save click", clicked.error.message);
  std::string pump_error;
  if (!pump_until_saved(host, *save, pump_error))
    return fail("hierarchy Save service", pump_error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("hierarchy shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor hierarchy smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// Qualify post-mount semantic leaf reconciliation against the ACTUAL
// linked OpenGL/PluginHost/Box desktop application, not a mock consumer.
int run_semantic_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("semantic first frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget || graph->child_count() != 4)
    return fail("semantic initial host", "real mounted graph lacks original parts");

  const auto count = box.owned_element_count();
  const auto find_part = [&](std::string_view key) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(raw);
      const auto *name = part->attribute("part");
      if (name && *name == key) return part;
    }
    return nullptr;
  };
  auto *alpha = find_part("node:alpha");
  if (!alpha) return fail("semantic base", "stable alpha part is missing");

  // A real document Save/Reopen only owns logical IDs; the host semantic
  // tree is a derived Box-owned projection, never serialized as pointers.
  std::string original;
  if (!model->save_json(original))
    return fail("semantic source", "logical v1 graph JSON unavailable");
  auto extended = nlohmann::json::parse(original);
  extended["nodes"].push_back({{"id", "gamma"}, {"x", 740.0},
      {"y", 38.0}, {"width", 80.0}, {"height", 64.0},
      {"locked", false}});
  extended["ports"].push_back({{"id", "gamma-output"},
      {"node", "gamma"}, {"side", "east"}, {"x", 80.0},
      {"y", 32.0}, {"output", true}});
  const auto expanded = extended.dump();

  if (!model->load_json(expanded))
    return fail("semantic add", "new original node/port was not accepted");
  box.update(); // The exact Box owner safe boundary performs reconciliation.
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped || box.owned_element_count() != count + 2 ||
      graph->child_count() != 6 || !find_part("node:gamma") ||
      !find_part("port:gamma-output") || find_part("node:alpha") != alpha ||
      box.last_widget_part_error() != flexUI::BoxWidgetPartError::None)
    return fail("semantic add host", "native Box did not mount two new owned parts");

  // A borrowed/focused part must NOT be destroyed while still addressable.
  auto *gamma = find_part("node:gamma");
  box.set_focus(gamma);
  if (!model->load_json(original))
    return fail("semantic remove source", "cannot restore original graph");
  box.update();
  if (box.last_widget_part_error() != flexUI::BoxWidgetPartError::BorrowedPart ||
      box.owned_element_count() != count + 2 ||
      box.focused_element() != gamma || gamma->parent_elem() != graph)
    return fail("semantic focus lease", "borrowed group part was incorrectly freed");

  box.set_focus(nullptr);
  box.update();
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped || box.owned_element_count() != count ||
      graph->child_count() != 4 || find_part("node:gamma") ||
      find_part("port:gamma-output") ||
      find_part("node:alpha") != alpha ||
      box.last_widget_part_error() != flexUI::BoxWidgetPartError::None)
    return fail("semantic physical retire", "Box failed to reclaim old parts");

  // One GroupId and its named proxy PortId are created and deleted repeatedly
  // AFTER the XML widget was mounted. Reusing the same stable key must never
  // grow the Box-owned Element arena.
  for (int i = 0; i < 6; ++i) {
    charts::ui::GraphEditGroup group;
    group.id = "pack";
    group.x = 40; group.y = 24; group.width = 170; group.height = 130;
    group.member_node_ids = {"alpha"};
    group.proxy_ports.push_back(
        {"pack-out", "source", stun::graphlayout::Side::East, 170, 48});
    if (!model->add_group(std::move(group)))
      return fail("semantic group source", "measured group was rejected");
    box.update();
    if (graph->child_count() != 6 ||
        box.owned_element_count() != count + 2 ||
        !find_part("node:group@pack") || !find_part("port:pack-out") ||
        find_part("node:alpha") != alpha)
      return fail("semantic group insert", "group proxy missing on owner Box");
    if (!model->remove_group("pack"))
      return fail("semantic group remove", "logical group could not be removed");
    box.update();
    if (graph->child_count() != 4 || box.owned_element_count() != count ||
        find_part("node:group@pack") || find_part("port:pack-out") ||
        find_part("node:alpha") != alpha)
      return fail("semantic group retire", "repeated group proxy leaked");
  }
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("semantic final frame", pumped.error.message);

  // Service ownership and Save must continue to work after dynamic changes.
  auto *save = box.get_by_id("save");
  if (!save) return fail("semantic Save lookup", "real Save action is absent");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("semantic Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("semantic Save service", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("semantic host shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor semantic smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// M2f: real installed-style desktop host, no mock GraphCanvas or second
// document model. Exercise *in-place* topology transactions rather than
// replacing the whole JSON document and clearing Undo history.
int run_topology_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("topology initial frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget || graph->child_count() != 4)
    return fail("topology host", "original linked GraphCanvas not mounted");

  const auto find_part = [&](std::string_view name) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(raw);
      const auto *key = part->attribute("part");
      if (key && *key == name) return part;
    }
    return nullptr;
  };
  auto *alpha = find_part("node:alpha");
  if (!alpha) return fail("topology original node", "alpha identity missing");
  const auto baseline_parts = graph->child_count();
  const auto baseline_owned = box.owned_element_count();
  const auto baseline_revision = model->snapshot().revision;
  if (!model->add_node(
          {"gamma", 740.0, 36.0, 80.0, 72.0, false},
          {{"gamma-in", "gamma", stun::graphlayout::Side::West,
            0.0, 36.0, false}}))
    return fail("topology insert", "one atomic original Node+Port rejected");
  box.update();
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped || model->snapshot().revision != baseline_revision + 1 ||
      model->undo_count() != 1 ||
      graph->child_count() != baseline_parts + 2 ||
      box.owned_element_count() != baseline_owned + 2 ||
      !find_part("node:gamma") || !find_part("port:gamma-in") ||
      find_part("node:alpha") != alpha)
    return fail("topology box insertion", "new original semantic leaves not published");

  if (!model->add_port(
        {"beta-out", "beta", stun::graphlayout::Side::East,
         120.0, 36.0, true}) ||
      !model->connect("beta-out", "gamma-in", "bridge"))
    return fail("topology connect", "native measured endpoint route rejected");
  box.update();
  if (graph->child_count() != baseline_parts + 3 ||
      model->snapshot().routes.size() != 1 ||
      model->undo_count() != 3)
    return fail("topology connection", "one native GraphLayout route not published");

  const auto route = model->snapshot().routes.front();
  if (!model->remove_node("gamma"))
    return fail("topology delete", "cannot atomically prune original node");
  box.update();
  if (graph->child_count() != baseline_parts + 1 ||
      box.owned_element_count() != baseline_owned + 1 ||
      model->snapshot().nodes.size() != 2 ||
      model->snapshot().edges.size() != 0 ||
      model->snapshot().routes.size() != 0 ||
      model->undo_count() != 4 ||
      find_part("node:alpha") != alpha)
    return fail("topology cascade", "incident edge or Box parts leaked");

  if (!model->undo())
    return fail("topology Undo", "cannot restore one original node transaction");
  box.update();
  if (graph->child_count() != baseline_parts + 3 ||
      box.owned_element_count() != baseline_owned + 3 ||
      model->snapshot().edges.size() != 1 ||
      model->snapshot().routes.size() != 1 ||
      model->snapshot().routes[0].edge_id != route.edge_id ||
      find_part("node:alpha") != alpha)
    return fail("topology undo owner", "route or semantic identity not restored");
  if (!model->redo()) return fail("topology Redo", "cannot replay deletion");
  box.update();
  if (box.owned_element_count() != baseline_owned + 1 ||
      model->snapshot().nodes.size() != 2)
    return fail("topology redo owner", "removed node leaked on replay");

  if (!model->remove_port("beta-out"))
    return fail("topology remove port", "cannot retire original port schema");
  box.update();
  if (graph->child_count() != baseline_parts ||
      box.owned_element_count() != baseline_owned ||
      model->snapshot().ports.size() != 2 ||
      box.last_widget_part_error() != flexUI::BoxWidgetPartError::None)
    return fail("topology final owner", "live edit failed to restore bounded Box");

  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("topology paint", pumped.error.message);
  auto *save = box.get_by_id("save");
  if (!save) return fail("topology Save lookup", "document Save not mounted");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("topology Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("topology Save", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("topology shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor topology smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// M2g: user-facing keyboard deletion on the same real OpenGL/PluginHost
// desktop editor, not a separate Widget/GraphLayout or test executable.
int run_delete_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("delete initial frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget || graph->child_count() != 4 ||
      model->snapshot().edges.size() != 1 ||
      model->snapshot().routes.size() != 1)
    return fail("delete source", "connected native GraphCanvas was not mounted");
  const auto find_part = [&](std::string_view key) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(raw);
      const auto *name = part->attribute("part");
      if (name && *name == key) return part;
    }
    return nullptr;
  };
  auto *beta = find_part("node:beta");
  if (!beta) return fail("delete baseline", "stable surviving node not found");
  const auto original_owned = box.owned_element_count();
  const auto original_revision = model->snapshot().revision;
  const float gx = graph->absolute_x(), gy = graph->absolute_y();

  const auto select_alpha = [&]() -> bool {
    auto down = flexUI::Event::mouse_down(gx + 85.0F, gy + 56.0F);
    auto up = flexUI::Event::mouse_up(gx + 85.0F, gy + 56.0F);
    return host.application().dispatch_event(down) && down.handled &&
           host.application().dispatch_event(up) && up.handled &&
           box.capturing_element() == nullptr;
  };
  if (!select_alpha())
    return fail("delete select", "native pointer event did not select alpha");
  auto del = flexUI::Event::key_down(flexUI::KeyCode::Delete);
  if (!host.application().dispatch_event(del) || !del.handled)
    return fail("delete keyboard", "Delete was not handled by GraphCanvas");
  box.update();
  if (model->snapshot().revision != original_revision + 1 ||
      model->snapshot().nodes.size() != 1 ||
      model->snapshot().ports.size() != 1 ||
      !model->snapshot().edges.empty() || !model->snapshot().routes.empty() ||
      model->undo_count() != 1 ||
      graph->child_count() != 2 ||
      box.owned_element_count() != original_owned - 2 ||
      find_part("node:beta") != beta)
    return fail("delete atomic topology", "Delete lost original graph identity");

  auto undo = flexUI::Event::key_down(
      flexUI::KeyCode::Z, static_cast<int>(flexUI::KeyMod::Control));
  auto redo = flexUI::Event::key_down(
      flexUI::KeyCode::Y, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(undo) || !undo.handled)
    return fail("delete Undo input", "Ctrl-Z not dispatched");
  box.update();
  if (model->snapshot().nodes.size() != 2 ||
      model->snapshot().ports.size() != 2 ||
      model->snapshot().edges.size() != 1 ||
      model->snapshot().routes.size() != 1 ||
      graph->child_count() != 4 ||
      box.owned_element_count() != original_owned ||
      find_part("node:beta") != beta)
    return fail("delete Undo restore", "native graph route/semantics not restored");
  if (!host.application().dispatch_event(redo) || !redo.handled)
    return fail("delete Redo input", "Ctrl-Y not dispatched");
  box.update();
  if (model->snapshot().nodes.size() != 1 ||
      graph->child_count() != 2 ||
      box.owned_element_count() != original_owned - 2)
    return fail("delete Redo", "native deletion was not replayed");
  if (!model->undo()) return fail("delete restore", "Undo after Redo failed");
  box.update();
  if (!select_alpha())
    return fail("delete Backspace select", "pointer could not select restored node");
  auto backspace = flexUI::Event::key_down(flexUI::KeyCode::Backspace);
  if (!host.application().dispatch_event(backspace) || !backspace.handled)
    return fail("delete Backspace", "Backspace was not consumed");
  box.update();
  if (model->snapshot().nodes.size() != 1 ||
      model->snapshot().edges.size() != 0 ||
      box.owned_element_count() != original_owned - 2 ||
      find_part("node:beta") != beta)
    return fail("delete Backspace result", "second gesture leaked semantics");

  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("delete paint", pumped.error.message);
  auto *save = box.get_by_id("save");
  if (!save) return fail("delete Save lookup", "real service UI absent");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("delete Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("delete Save service", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("delete shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor delete smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// M2h: the exact same production OpenGL/PluginHost/Box application
// exercises genuine nested Ungroup, keyboard selection, full Undo/Redo,
// physical semantic retirement and a real native document Save.
int run_ungroup_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("ungroup initial frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget || graph->child_count() != 16 ||
      model->snapshot().nodes.size() != 4 ||
      model->snapshot().edges.size() != 3 ||
      model->snapshot().groups.size() != 3)
    return fail("ungroup mount", "three-level source was not mounted");
  const auto *shortcuts = graph->attribute("aria-keyshortcuts");
  if (!shortcuts || shortcuts->find("Control+Shift+G") == std::string::npos)
    return fail("ungroup shortcuts", "native Widget metadata is incomplete");
  const auto part = [&](std::string_view key) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *element = static_cast<flexUI::Element *>(raw);
      const auto *name = element->attribute("part");
      if (name && *name == key) return element;
    }
    return nullptr;
  };
  const auto group = [&](std::string_view id)
      -> const charts::ui::GraphEditGroup * {
    for (const auto &g : model->snapshot().groups)
      if (g.id == id) return &g;
    return nullptr;
  };
  auto *inner_semantic = part("node:group@inner");
  if (!inner_semantic || !part("node:group@middle") ||
      !part("port:middle-out"))
    return fail("ungroup group parts", "group semantic identity is missing");
  const auto owned_before = box.owned_element_count();
  const auto first_revision = model->snapshot().revision;
  const float gx = graph->absolute_x(), gy = graph->absolute_y();

  // A click on the middle header selects its stable GroupId and collapses it.
  // Ctrl+Shift+G must then remove the selected group, without deleting its
  // nested inner group, moving any original leaf, or rewriting any EdgeId.
  auto click_down = flexUI::Event::mouse_down(gx + 80.0F, gy + 58.0F);
  auto click_up = flexUI::Event::mouse_up(gx + 80.0F, gy + 58.0F);
  if (!host.application().dispatch_event(click_down) || !click_down.handled ||
      box.capturing_element() != graph ||
      !host.application().dispatch_event(click_up) || !click_up.handled ||
      box.capturing_element() != nullptr ||
      !group("middle") || !group("middle")->collapsed ||
      model->snapshot().revision != first_revision + 1 ||
      model->snapshot().routes.size() != 2)
    return fail("ungroup selection", "middle header did not capture and collapse");
  const int mods = static_cast<int>(flexUI::KeyMod::Control) |
                   static_cast<int>(flexUI::KeyMod::Shift);
  auto shortcut = flexUI::Event::key_down(flexUI::KeyCode::G, mods);
  if (!host.application().dispatch_event(shortcut) || !shortcut.handled ||
      group("middle") != nullptr ||
      !group("inner") || !group("outer") ||
      group("inner")->parent_group_id != "outer" ||
      group("inner")->x != 50.0 || group("inner")->y != 50.0 ||
      model->snapshot().revision != first_revision + 2 ||
      model->snapshot().nodes.size() != 4 ||
      model->snapshot().ports.size() != 6 ||
      model->snapshot().edges.size() != 3 ||
      model->snapshot().routes.size() != 3)
    return fail("ungroup keyboard", "one native group transaction failed");
  box.update();
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped || box.owned_element_count() != owned_before - 2 ||
      graph->child_count() != 14 ||
      part("node:group@inner") != inner_semantic ||
      part("node:group@middle") || part("port:middle-out"))
    return fail("ungroup physical parts", "Box did not reclaim only middle group leaves");
  charts::ui::GraphEditNode inner_world;
  if (!model->group_world_rect("inner", inner_world) ||
      inner_world.x != 70.0 || inner_world.y != 70.0)
    return fail("ungroup world geometry", "inner GroupId moved unexpectedly");

  auto undo = flexUI::Event::key_down(
      flexUI::KeyCode::Z, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(undo) || !undo.handled)
    return fail("ungroup Undo keyboard", "Ctrl-Z was not consumed");
  box.update();
  if (!group("middle") || !group("middle")->collapsed ||
      group("inner")->parent_group_id != "middle" ||
      group("inner")->x != 20.0 ||
      model->snapshot().routes.size() != 2 ||
      graph->child_count() != 16 ||
      box.owned_element_count() != owned_before ||
      part("node:group@inner") != inner_semantic)
    return fail("ungroup Undo", "hierarchy or Box leaves were not restored");

  auto redo = flexUI::Event::key_down(
      flexUI::KeyCode::Y, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(redo) || !redo.handled)
    return fail("ungroup Redo keyboard", "Ctrl-Y was not consumed");
  box.update();
  if (group("middle") || group("inner")->parent_group_id != "outer" ||
      model->snapshot().routes.size() != 3 ||
      box.owned_element_count() != owned_before - 2)
    return fail("ungroup Redo", "single recorded transaction was not replayed");

  // Removing a root parent additionally promotes the remaining child group
  // to world coordinates, while c is left completely ungrouped.
  if (!model->remove_group("outer"))
    return fail("ungroup root source", "root Ungroup was rejected");
  box.update();
  if (group("outer") || !group("inner") ||
      group("inner")->parent_group_id != "" ||
      group("inner")->x != 70.0 || group("inner")->y != 70.0 ||
      model->snapshot().edges.size() != 3 ||
      model->snapshot().routes.size() != 3 ||
      graph->child_count() != 12 ||
      box.owned_element_count() != owned_before - 4 ||
      part("node:group@inner") != inner_semantic)
    return fail("ungroup root promotion", "world-origin promotion was not atomic");

  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("ungroup final frame", pumped.error.message);
  auto *save = box.get_by_id("save");
  if (!save) return fail("ungroup Save lookup", "real service action missing");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("ungroup Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("ungroup Save service", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("ungroup shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor nested Ungroup smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// M2i: real OpenGL/PluginHost/Box multi-selection, no mock graph runtime.
// Shift+Click selects ORIGINAL NodeIds. Ctrl+G creates one measured group
// without synthesizing external boundary ports, and Undo/Redo remain native.
int run_group_selection_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("selection first frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget || graph->child_count() != 4 ||
      model->snapshot().edges.size() != 1)
    return fail("selection mount", "original connected graph is not mounted");
  const auto find_part = [&](std::string_view name) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(raw);
      const auto *key = part->attribute("part");
      if (key && *key == name) return part;
    }
    return nullptr;
  };
  auto *alpha = find_part("node:alpha");
  auto *beta = find_part("node:beta");
  if (!alpha || !beta)
    return fail("selection parts", "original stable node semantic parts missing");
  const float gx = graph->absolute_x(), gy = graph->absolute_y();
  const auto base_owned = box.owned_element_count();
  const auto base_revision = model->snapshot().revision;
  const auto original_edge = model->snapshot().edges.front();
  const auto original_route = model->snapshot().routes.front();

  auto down = flexUI::Event::mouse_down(gx + 85.0F, gy + 56.0F);
  auto up = flexUI::Event::mouse_up(gx + 85.0F, gy + 56.0F);
  if (!host.application().dispatch_event(down) || !down.handled ||
      !host.application().dispatch_event(up) || !up.handled)
    return fail("selection click", "first Box node was not selected");
  auto second = flexUI::Event::mouse_down(gx + 480.0F, gy + 56.0F);
  second.mods = static_cast<int>(flexUI::KeyMod::Shift);
  auto second_up = flexUI::Event::mouse_up(gx + 480.0F, gy + 56.0F);
  second_up.mods = second.mods;
  if (!host.application().dispatch_event(second) || !second.handled ||
      !host.application().dispatch_event(second_up))
    return fail("selection Shift+Click", "second original NodeId not admitted");
  box.update();
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("marquee committed frame",
                           pumped.error.window_error.message);
  if (host.window_host().has_native_pointer_capture())
    return fail("marquee native release",
                "X11 pointer grab survived the completed MouseUp");
  if (!alpha->attribute("data-selected") ||
      *alpha->attribute("data-selected") != "true" ||
      !beta->attribute("data-selected") ||
      *beta->attribute("data-selected") != "true" ||
      model->snapshot().revision != base_revision ||
      box.capturing_element() != nullptr)
    return fail("selection state", "multi-selection mutated graph or lost UI IDs");

  auto create = flexUI::Event::key_down(
      flexUI::KeyCode::G, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(create) || !create.handled ||
      model->snapshot().groups.size() != 1 ||
      model->snapshot().nodes.size() != 2 ||
      model->snapshot().edges.size() != 1 ||
      model->snapshot().edges[0].id != original_edge.id ||
      model->snapshot().edges[0].source_port != original_edge.source_port ||
      model->undo_count() != 1 ||
      model->snapshot().revision != base_revision + 1)
    return fail("selection create", "Ctrl+G did not publish one compound Undo");
  const auto group = model->snapshot().groups.front();
  if (group.member_node_ids.size() != 2 ||
      group.proxy_ports.size() != 0 ||
      group.x != 39.0 || group.y != 20.0 ||
      group.width != 537.0 || group.height != 104.0)
    return fail("selection measured group", "group geometry was not measured from two nodes");
  box.update();
  if (graph->child_count() != 5 ||
      box.owned_element_count() != base_owned + 1 ||
      find_part("node:alpha") != alpha ||
      find_part("node:beta") != beta ||
      !find_part("node:group@" + group.id))
    return fail("selection semantic group", "Box did not create one stable proxy");

  // Both original leaves are internal to the new group. Collapse only hides
  // the visual route; the original edge and its measured endpoints survive.
  auto group_down = flexUI::Event::mouse_down(gx + 60.0F, gy + 30.0F);
  auto group_up = flexUI::Event::mouse_up(gx + 60.0F, gy + 30.0F);
  if (!host.application().dispatch_event(group_down) || !group_down.handled ||
      !host.application().dispatch_event(group_up) || !group_up.handled ||
      !model->snapshot().groups[0].collapsed ||
      model->snapshot().routes.size() != 0 ||
      model->snapshot().edges.size() != 1)
    return fail("selection collapse", "internal logical edge was not preserved");

  const int shortcut_mods = static_cast<int>(flexUI::KeyMod::Control) |
                            static_cast<int>(flexUI::KeyMod::Shift);
  auto ungroup = flexUI::Event::key_down(flexUI::KeyCode::G, shortcut_mods);
  if (!host.application().dispatch_event(ungroup) || !ungroup.handled ||
      !model->snapshot().groups.empty() ||
      model->snapshot().routes.size() != 1 ||
      model->snapshot().edges.front().id != original_edge.id ||
      model->undo_count() != 3)
    return fail("selection Ungroup", "one original group was not retired");
  box.update();
  if (graph->child_count() != 4 ||
      box.owned_element_count() != base_owned ||
      find_part("node:alpha") != alpha ||
      find_part("node:beta") != beta)
    return fail("selection retire", "Box lost stable originals or leaked proxy");
  if (model->snapshot().routes[0].edge_id != original_route.edge_id ||
      model->snapshot().routes[0].points.size() != original_route.points.size())
    return fail("selection original route", "Ungroup changed logical topology");

  auto undo = flexUI::Event::key_down(
      flexUI::KeyCode::Z, static_cast<int>(flexUI::KeyMod::Control));
  auto redo = flexUI::Event::key_down(
      flexUI::KeyCode::Y, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(undo) || !undo.handled)
    return fail("selection Undo", "Ctrl+Z did not restore collapsed group");
  box.update();
  if (model->snapshot().groups.size() != 1 ||
      !model->snapshot().groups[0].collapsed ||
      graph->child_count() != 5)
    return fail("selection Undo state", "original group hierarchy lost");
  if (!host.application().dispatch_event(redo) || !redo.handled)
    return fail("selection Redo", "Ctrl+Y did not reapply Ungroup");
  box.update();
  if (model->snapshot().groups.size() != 0 ||
      graph->child_count() != 4 ||
      box.owned_element_count() != base_owned)
    return fail("selection Redo owner", "Box failed to reclaim proxy again");

  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("selection repaint", pumped.error.message);
  auto *save = box.get_by_id("save");
  if (!save) return fail("selection Save", "real Save service missing");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("selection Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("selection Save service", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("selection shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor selection smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// M2j: the real linked OpenGL/PluginHost/Box editor measures a group
// boundary from the user's pointer release, never a fabricated PortId/route.
int run_boundary_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("boundary initial frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget || graph->child_count() != 5 ||
      model->snapshot().edges.size() != 1 ||
      model->snapshot().groups.size() != 1 ||
      !model->snapshot().groups[0].proxy_ports.empty())
    return fail("boundary source", "unbound original group not mounted");
  const auto group = [&]() -> const charts::ui::GraphEditGroup & {
    return model->snapshot().groups.front();
  };
  const auto find_part = [&](std::string_view key) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(raw);
      const auto *name = part->attribute("part");
      if (name && *name == key) return part;
    }
    return nullptr;
  };
  auto *alpha = find_part("node:alpha");
  if (!alpha) return fail("boundary stable leaf", "original node not mounted");
  const auto baseline_owned = box.owned_element_count();
  const auto original_revision = model->snapshot().revision;
  const float gx = graph->absolute_x(), gy = graph->absolute_y();
  const auto click_header = [&]() -> bool {
    auto down = flexUI::Event::mouse_down(gx + 60.0F, gy + 30.0F);
    auto up = flexUI::Event::mouse_up(gx + 60.0F, gy + 30.0F);
    return host.application().dispatch_event(down) && down.handled &&
           host.application().dispatch_event(up) && up.handled;
  };

  // Collapse without an explicit original source boundary is invalid.
  // The pointer click still selects the stable GroupId for authoring.
  if (!click_header() || group().collapsed ||
      model->snapshot().revision != original_revision ||
      model->undo_count() != 0)
    return fail("boundary missing binding", "unmeasured collapse silently committed");

  const int mods = static_cast<int>(flexUI::KeyMod::Control) |
                   static_cast<int>(flexUI::KeyMod::Alt);
  const auto author = [&](float end_x, float end_y) -> bool {
    auto down = flexUI::Event::mouse_down(gx + 180.0F, gy + 72.0F);
    down.mods = mods;
    auto move = flexUI::Event::mouse_move(gx + end_x, gy + end_y);
    move.mods = mods;
    auto up = flexUI::Event::mouse_up(gx + end_x, gy + end_y);
    up.mods = mods;
    if (!host.application().dispatch_event(down) || !down.handled ||
        box.capturing_element() != graph ||
        !host.application().dispatch_event(move) || !move.handled ||
        !host.application().dispatch_event(up) || !up.handled ||
        box.capturing_element() != nullptr)
      return false;
    return true;
  };
  if (!author(210.0F, 72.0F) ||
      group().proxy_ports.size() != 1 ||
      group().proxy_ports[0].original_port_id != "source" ||
      group().proxy_ports[0].side != stun::graphlayout::Side::East ||
      group().proxy_ports[0].local_x != 170.0 ||
      group().proxy_ports[0].local_y != 48.0 ||
      model->undo_count() != 1 ||
      model->snapshot().edges[0].source_port != "source" ||
      model->snapshot().revision != original_revision + 1)
    return fail("boundary measurement", "user-directed native port not committed");
  const auto stable_proxy = group().proxy_ports[0].id;
  box.update();
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped || graph->child_count() != 6 ||
      box.owned_element_count() != baseline_owned + 1 ||
      find_part("node:alpha") != alpha ||
      !find_part("port:" + stable_proxy))
    return fail("boundary semantic owner", "new measured Box port was not published");

  // Releasing INSIDE the group does not change the measured attachment,
  // even if the original PortId and selected GroupId remain valid.
  const auto before_invalid = model->snapshot().revision;
  if (!author(145.0F, 72.0F) ||
      model->snapshot().revision != before_invalid ||
      model->undo_count() != 1 ||
      group().proxy_ports[0].id != stable_proxy ||
      group().proxy_ports[0].local_x != 170.0)
    return fail("boundary invalid interior", "port was inferred from interior release");

  if (!click_header() || !group().collapsed ||
      model->snapshot().routes.size() != 1 ||
      model->snapshot().routes[0].points.front().x != 210.0 ||
      model->snapshot().edges[0].source_port != "source" ||
      model->undo_count() != 2)
    return fail("boundary native collapse", "real measured proxy was not routed");
  charts::ui::GraphEditSnapshot visible;
  if (!model->visible_snapshot(visible) ||
      visible.edges.size() != 1 ||
      visible.edges[0].source_port != stable_proxy)
    return fail("boundary visible mapping", "logical source port was overwritten");

  // Every key press is a distinct Event instance. The real dispatcher
  // marks an accepted event handled and non-propagating; re-dispatching the
  // same mutable Event would silently skip the GraphCanvas keyboard owner.
  const auto press_history = [&](flexUI::KeyCode key) {
    auto event = flexUI::Event::key_down(
        key, static_cast<int>(flexUI::KeyMod::Control));
    return host.application().dispatch_event(event) && event.handled;
  };
  if (!press_history(flexUI::KeyCode::Z) || group().collapsed)
    return fail("boundary Undo collapse", "native first Undo did not expand");
  if (!press_history(flexUI::KeyCode::Z) ||
      !group().proxy_ports.empty())
    return fail("boundary Undo binding", "native second Undo did not retire port");
  box.update();
  if (graph->child_count() != 5 ||
      box.owned_element_count() != baseline_owned)
    return fail("boundary port retirement", "removed proxy semantic leaf leaked");
  if (!press_history(flexUI::KeyCode::Y) ||
      group().proxy_ports.size() != 1 ||
      group().proxy_ports[0].id != stable_proxy)
    return fail("boundary Redo binding", "native first Redo did not restore PortId");
  if (!press_history(flexUI::KeyCode::Y) || !group().collapsed)
    return fail("boundary Redo collapse", "native second Redo did not collapse");
  box.update();
  if (graph->child_count() != 6 ||
      box.owned_element_count() != baseline_owned + 1 ||
      !find_part("port:" + stable_proxy) ||
      find_part("node:alpha") != alpha)
    return fail("boundary Redo semantics", "one Box ownership was lost");

  auto *save = box.get_by_id("save");
  if (!save) return fail("boundary Save lookup", "real Save action missing");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("boundary Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("boundary Save service", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("boundary shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor boundary smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

// M2k: actual linked OpenGL/PluginHost/Box host validates a bounded
// rubber-band selection from blank canvas. A marquee is Widget-only UI
// state until Ctrl+G publishes one native GraphEditDocument transaction.
int run_marquee_smoke(
    flexUI::GCanvasPluginWindowHost &host,
    const std::shared_ptr<charts::ui::GraphEditDocument> &model) {
  auto pumped = host.pump_once(0.0);
  if (!pumped) return fail("marquee initial frame", pumped.error.message);
  auto &box = host.application().box();
  auto *graph = box.get_by_id("graph");
  if (!graph || !graph->widget ||
      graph->child_count() != 4 ||
      model->snapshot().nodes.size() != 2 ||
      model->snapshot().edges.size() != 1 ||
      !model->snapshot().groups.empty())
    return fail("marquee initial source", "real linked original graph is absent");

  const auto find_part = [&](std::string_view key) -> flexUI::Element * {
    for (auto *raw : graph->children()) {
      auto *part = static_cast<flexUI::Element *>(raw);
      const auto *name = part->attribute("part");
      if (name && *name == key) return part;
    }
    return nullptr;
  };
  auto *alpha = find_part("node:alpha");
  auto *beta = find_part("node:beta");
  if (!alpha || !beta)
    return fail("marquee stable IDs", "both original semantic leaves are required");

  const auto before_revision = model->snapshot().revision;
  const auto before_owned = box.owned_element_count();
  const float gx = graph->absolute_x(), gy = graph->absolute_y();

  const auto down = [&] {
    return flexUI::Event::mouse_down(gx + 20.0F, gy + 125.0F);
  };
  const auto move = [&] {
    return flexUI::Event::mouse_move(gx + 600.0F, gy + 20.0F);
  };
  const auto up = [&] {
    return flexUI::Event::mouse_up(gx + 600.0F, gy + 20.0F);
  };

  // The canvas captures input before any document edit. The owner can paint
  // the selection preview while a native gesture is still in flight.
  auto pressed = down();
  if (!host.application().dispatch_event(pressed) || !pressed.handled ||
      box.capturing_element() != graph)
    return fail("marquee pointer capture", "blank drag lacks an owner lease");
  auto dragging = move();
  if (!host.application().dispatch_event(dragging) || !dragging.handled ||
      box.capturing_element() != graph ||
      model->snapshot().revision != before_revision ||
      model->undo_count() != 0)
    return fail("marquee uncommitted", "selection preview mutated the graph");
  // This is the real X11 OS/window capture acceptance, not merely the
  // Box's borrowed Element pointer. Drive a production GPU frame WHILE the
  // user gesture is still active. GLFW/X11 must own its native pointer grab
  // across that frame; neither cursor confinement nor synthetic "success"
  // is an acceptable substitute.
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped)
    return fail("marquee native capture frame",
                pumped.error.window_error.message);
  if (!host.window_host().has_native_pointer_capture() ||
      box.capturing_element() != graph ||
      model->snapshot().revision != before_revision)
    return fail("marquee native capture lease",
                "GLFW/X11 did not retain the actual pointer grab during the frame");
  // A SECOND frame validates actual X11 capture across frame boundaries,
  // including the same native window's idempotent re-acquisition.
  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped || !host.window_host().has_native_pointer_capture())
    return fail("marquee second native frame",
                pumped ? "native X11 grab was not retained" :
                         pumped.error.window_error.message);

  auto released = up();
  if (!host.application().dispatch_event(released) || !released.handled ||
      box.capturing_element() != nullptr)
    return fail("marquee release", "owner failed to release pointer on MouseUp");
  box.update();
  if (!alpha->attribute("data-selected") ||
      *alpha->attribute("data-selected") != "true" ||
      !beta->attribute("data-selected") ||
      *beta->attribute("data-selected") != "true" ||
      model->snapshot().revision != before_revision ||
      model->undo_count() != 0 ||
      box.owned_element_count() != before_owned)
    return fail("marquee two original nodes", "rubber-band selection not committed");

  auto group = flexUI::Event::key_down(
      flexUI::KeyCode::G, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(group) || !group.handled ||
      model->snapshot().groups.size() != 1 ||
      model->snapshot().edges.size() != 1 ||
      model->snapshot().routes.size() != 1 ||
      model->snapshot().revision != before_revision + 1 ||
      model->undo_count() != 1)
    return fail("marquee Ctrl+G", "one measured source GroupId not published");
  const auto authored = model->snapshot().groups[0];
  if (authored.x != 39.0 || authored.y != 20.0 ||
      authored.width != 537.0 || authored.height != 104.0 ||
      authored.member_node_ids.size() != 2 ||
      !authored.proxy_ports.empty())
    return fail("marquee measured bounds", "group boundary was not measured");
  box.update();
  if (graph->child_count() != 5 ||
      box.owned_element_count() != before_owned + 1 ||
      find_part("node:alpha") != alpha ||
      find_part("node:beta") != beta ||
      !find_part("node:group@" + authored.id))
    return fail("marquee Box owner", "one stable group leaf missing");

  auto ungroup = flexUI::Event::key_down(
      flexUI::KeyCode::G, static_cast<int>(flexUI::KeyMod::Control) |
                          static_cast<int>(flexUI::KeyMod::Shift));
  if (!host.application().dispatch_event(ungroup) || !ungroup.handled ||
      !model->snapshot().groups.empty() ||
      model->snapshot().edges.size() != 1 ||
      model->undo_count() != 2)
    return fail("marquee Ungroup", "original source graph could not be restored");
  box.update();
  if (graph->child_count() != 4 ||
      box.owned_element_count() != before_owned ||
      find_part("node:alpha") != alpha ||
      find_part("node:beta") != beta)
    return fail("marquee physical group retire", "Box retained orphan proxy");

  // Each keystroke owns a new Event, never reuse an already-handled object.
  auto undo = flexUI::Event::key_down(
      flexUI::KeyCode::Z, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(undo) || !undo.handled ||
      model->snapshot().groups.size() != 1)
    return fail("marquee Undo", "nested group history was not restored");
  box.update();
  auto redo = flexUI::Event::key_down(
      flexUI::KeyCode::Y, static_cast<int>(flexUI::KeyMod::Control));
  if (!host.application().dispatch_event(redo) || !redo.handled ||
      !model->snapshot().groups.empty())
    return fail("marquee Redo", "native history was not replayed");
  box.update();
  if (graph->child_count() != 4 || box.owned_element_count() != before_owned)
    return fail("marquee Redo Box", "group semantic leaf leaked");

  box.invalidate();
  pumped = host.pump_once(0.0);
  if (!pumped) return fail("marquee final frame", pumped.error.message);
  auto *save = box.get_by_id("save");
  if (!save) return fail("marquee Save lookup", "native Save service not mounted");
  const auto clicked = dispatch_save_click(host.application(), *save);
  if (!clicked) return fail("marquee Save click", clicked.error.message);
  std::string error;
  if (!pump_until_saved(host, *save, error))
    return fail("marquee Save service", error);
  const auto stopped = host.shutdown(kShutdownTimeout);
  if (!stopped) return fail("marquee host shutdown", stopped.error.message);
  std::cout << "FlexUI desktop editor marquee smoke passed: "
            << save->text() << '\n';
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char **argv) {
  bool smoke = false;
  bool async_routing = false;
  bool group_smoke = false;
  bool hierarchy_smoke = false;
  bool semantic_smoke = false;
  bool topology_smoke = false;
  bool delete_smoke = false;
  bool ungroup_smoke = false;
  bool selection_smoke = false;
  bool boundary_smoke = false;
  bool marquee_smoke = false;
  if (argc == 2 && std::string_view(argv[1]) == "--smoke") {
    smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-async") {
    smoke = true;
    async_routing = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-group") {
    smoke = true;
    group_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-hierarchy") {
    smoke = true;
    hierarchy_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-semantic") {
    smoke = true;
    semantic_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-topology") {
    smoke = true;
    topology_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-delete") {
    smoke = true;
    delete_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-ungroup") {
    smoke = true;
    ungroup_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-selection") {
    smoke = true;
    selection_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-boundary") {
    smoke = true;
    boundary_smoke = true;
  } else if (argc == 2 && std::string_view(argv[1]) == "--smoke-marquee") {
    smoke = true;
    marquee_smoke = true;
  } else if (argc != 1) {
    return fail("arguments",
                "usage: flexui_desktop_editor [--smoke|--smoke-async|--smoke-group|--smoke-hierarchy|--smoke-semantic|--smoke-topology|--smoke-delete|--smoke-ungroup|--smoke-selection|--smoke-boundary|--smoke-marquee]");
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
  auto graph_model = std::make_shared<charts::ui::GraphEditDocument>();
  charts::ui::GraphEditSnapshot initial_graph;
  initial_graph.nodes = {{"beta", 440.0, 36.0, 120.0, 72.0, false},
                         {"alpha", 55.0, 36.0, 125.0, 72.0, false}};
  initial_graph.ports = {
      {"sink", "beta", stun::graphlayout::Side::West, 0.0, 36.0, false},
      {"source", "alpha", stun::graphlayout::Side::East, 125.0, 36.0, true}};
  if (delete_smoke || selection_smoke || boundary_smoke || marquee_smoke)
    initial_graph.edges.push_back({"edge-1", "source", "sink"});
  if (group_smoke || boundary_smoke) {
    charts::ui::GraphEditGroup group;
    group.id = "pack";
    group.x = 40.0; group.y = 24.0;
    group.width = 170.0; group.height = 130.0;
    group.member_node_ids = {"alpha"};
    if (group_smoke)
      group.proxy_ports.push_back(
          {"pack-out", "source", stun::graphlayout::Side::East, 170.0, 48.0});
    initial_graph.groups.push_back(std::move(group));
  }
  if (hierarchy_smoke || ungroup_smoke) {
    // All original leaf nodes/ports remain world-space; nested groups are
    // parent-local. These are the SAME bounded C++ source types that the
    // existing ChartUiTest uses, not a parallel smoke-only graph runtime.
    using stun::graphlayout::Side;
    initial_graph.nodes = {{"a", 80, 95, 48, 36, false},
                           {"b", 170, 95, 48, 36, false},
                           {"c", 340, 95, 48, 36, false},
                           {"d", 560, 95, 48, 36, false}};
    initial_graph.ports = {
        {"a-out", "a", Side::East, 48, 18, true},
        {"b-in", "b", Side::West, 0, 18, false},
        {"b-out", "b", Side::East, 48, 18, true},
        {"c-in", "c", Side::West, 0, 18, false},
        {"c-out", "c", Side::East, 48, 18, true},
        {"d-in", "d", Side::West, 0, 18, false}};
    initial_graph.edges = {{"ab", "a-out", "b-in"},
                           {"bc", "b-out", "c-in"},
                           {"cd", "c-out", "d-in"}};
    charts::ui::GraphEditGroup outer;
    outer.id = "outer";
    outer.x = 20; outer.y = 20; outer.width = 420; outer.height = 240;
    outer.member_node_ids = {"c"};
    outer.proxy_ports.push_back(
        {"outer-out", "c-out", Side::East, 420, 93});
    charts::ui::GraphEditGroup middle;
    middle.id = "middle";
    middle.parent_group_id = "outer";
    middle.x = 30; middle.y = 30; middle.width = 270; middle.height = 180;
    middle.proxy_ports.push_back(
        {"middle-out", "b-out", Side::East, 270, 63});
    charts::ui::GraphEditGroup inner;
    inner.id = "inner";
    inner.parent_group_id = "middle";
    inner.x = 20; inner.y = 20; inner.width = 200; inner.height = 115;
    inner.member_node_ids = {"a", "b"};
    inner.proxy_ports.push_back(
        {"inner-out", "b-out", Side::East, 200, 43});
    initial_graph.groups = {std::move(outer), std::move(middle),
                            std::move(inner)};
  }
  if (const auto status = graph_model->initialize(std::move(initial_graph)); !status) {
    return fail("graph source", status.message);
  }
  auto built = flexUI::GCanvasPluginWindowHost::create(
      window_config(smoke, async_routing),
      application_builder(std::move(sources), timeline_model, graph_model,
                          async_routing),
      std::move(plugins),
      capability_manifest(), kShutdownTimeout);
  if (!built) {
    return fail("runtime composition", built.error.message);
  }

  if (marquee_smoke)
    return run_marquee_smoke(*built.host, graph_model);
  if (boundary_smoke)
    return run_boundary_smoke(*built.host, graph_model);
  if (selection_smoke)
    return run_group_selection_smoke(*built.host, graph_model);
  if (ungroup_smoke) return run_ungroup_smoke(*built.host, graph_model);
  if (delete_smoke) return run_delete_smoke(*built.host, graph_model);
  if (topology_smoke) return run_topology_smoke(*built.host, graph_model);
  if (semantic_smoke) return run_semantic_smoke(*built.host, graph_model);
  if (hierarchy_smoke) return run_hierarchy_smoke(*built.host, graph_model);
  if (smoke) {
    return run_smoke(*built.host, timeline_model, graph_model,
                     async_routing, group_smoke);
  }
  const auto ran = built.host->run();
  return ran ? EXIT_SUCCESS : fail("desktop loop", ran.error.message);
}
