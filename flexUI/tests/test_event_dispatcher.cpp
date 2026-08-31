#include <flexUI/box.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/widget.h>

#include <tinytest.hpp>

#include <vector>

namespace {

class CaptureProbeWidget final : public flexUI::Widget {
public:
  explicit CaptureProbeWidget(bool consume) : consume_(consume) {}

  void emit_render_commands(const flexUI::Element &, flexUI::RenderCommandList &) override {}

  bool handle_event(const flexUI::Event &event, flexUI::Element &) override {
    calls.push_back(event.type);
    if (event.type == flexUI::EventType::MouseDown) {
      capturing = true;
    } else if (event.type == flexUI::EventType::MouseUp) {
      capturing = false;
    }
    return consume_;
  }

  const char *type_name() const override { return "CaptureProbeWidget"; }
  bool wants_mouse_capture() const override { return capturing; }

  bool capturing = false;
  std::vector<flexUI::EventType> calls;

private:
  bool consume_ = false;
};

struct CaptureHarness {
  flexUI::Box box{nullptr};
  flexUI::Element *root = nullptr;
  flexUI::Element *capture = nullptr;
  flexUI::Element *physical = nullptr;
  CaptureProbeWidget *probe = nullptr;

  explicit CaptureHarness(bool consume) {
    root = box.create("div", "root");
    capture = box.create_widget<CaptureProbeWidget>("div", "capture", consume);
    physical = box.create("div", "physical");
    probe = static_cast<CaptureProbeWidget *>(capture->widget);
    root->append(capture);
    root->append(physical);
    box.set_root(root);

    root->set_layout_bounds(0.0F, 0.0F, 300.0F, 100.0F);
    capture->set_layout_bounds(0.0F, 0.0F, 100.0F, 100.0F);
    physical->set_layout_bounds(150.0F, 0.0F, 100.0F, 100.0F);
  }
};

} // namespace

spec("EventDispatcher routes captured pointer input exactly once") {
  it("retargets to capture while preserving the physical hover target") {
    CaptureHarness harness(false);
    bool clicked = false;
    harness.capture->on_click([&] { clicked = true; });

    auto down = flexUI::Event::mouse_down(20.0F, 20.0F);
    harness.box.dispatch_event(down);
    check(harness.box.capturing_element() == harness.capture);

    harness.probe->calls.clear();
    auto move = flexUI::Event::mouse_move(200.0F, 20.0F);
    harness.box.dispatch_event(move);

    check(move.target == harness.capture);
    check(harness.box.hovered_element() == harness.physical);
    check_equal(harness.probe->calls.size(), std::size_t{1});
    check(harness.probe->calls.front() == flexUI::EventType::MouseMove);

    auto up = flexUI::Event::mouse_up(200.0F, 20.0F);
    harness.box.dispatch_event(up);

    check(up.target == harness.capture);
    check_false(clicked);
    check(harness.box.capturing_element() == nullptr);
    check_equal(harness.probe->calls.size(), std::size_t{2});
    check(harness.probe->calls.back() == flexUI::EventType::MouseUp);
  }

  it("reconciles capture after a widget consumes mouse up") {
    CaptureHarness harness(true);

    auto down = flexUI::Event::mouse_down(20.0F, 20.0F);
    harness.box.dispatch_event(down);
    check_true(down.handled);
    check(harness.box.capturing_element() == harness.capture);

    harness.probe->calls.clear();
    auto up = flexUI::Event::mouse_up(200.0F, 20.0F);
    harness.box.dispatch_event(up);

    check_true(up.handled);
    check(up.target == harness.capture);
    check(harness.box.capturing_element() == nullptr);
    check_equal(harness.probe->calls.size(), std::size_t{1});
    check(harness.probe->calls.front() == flexUI::EventType::MouseUp);
  }
}
