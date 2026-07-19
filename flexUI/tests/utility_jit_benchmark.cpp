#include <flexUI/box.h>
#include <flexUI/element.h>

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;

struct Baseline {
  std::size_t elements = 0;
  double first_update_ms = 0.0;
  double attribute_update_ms = 0.0;
  double utility_update_ms = 0.0;
  std::size_t active_tokens = 0;
  std::size_t stylesheet_bytes = 0;
};

template <typename Fn>
double elapsed_ms(Fn&& fn) {
  const auto start = Clock::now();
  fn();
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Baseline measure(std::size_t element_count) {
  flexUI::Box box(nullptr);
  auto* root = box.create("div", "root");
  root->add_utilities("flex flex-col gap-2 p-4 bg-background text-foreground");

  flexUI::Element* first_child = nullptr;
  for (std::size_t i = 0; i < element_count; ++i) {
    auto* child = box.create("div", "item-" + std::to_string(i));
    child->add_utilities(i % 2 == 0
                             ? "flex items-center h-10 w-full rounded-md border"
                             : "flex items-center h-10 w-full rounded-md shadow-sm");
    root->append(child);
    if (!first_child) {
      first_child = child;
    }
  }

  box.set_root(root);
  box.set_viewport(1280.0f, 720.0f);

  Baseline result;
  result.elements = element_count + 1;
  result.first_update_ms = elapsed_ms([&] { box.update(); });
  const auto initial_revision = box.utility_jit_revision();

  first_child->set_attribute("data-state", "selected");
  result.attribute_update_ms = elapsed_ms([&] { box.update(); });
  if (box.utility_jit_revision() != initial_revision) {
    throw std::runtime_error("attribute-only update changed JIT revision");
  }

  first_child->add_utility("opacity-90");
  result.utility_update_ms = elapsed_ms([&] { box.update(); });
  if (box.utility_jit_revision() != initial_revision + 1) {
    throw std::runtime_error("utility update did not advance JIT revision once");
  }

  result.active_tokens = box.active_utility_count();
  result.stylesheet_bytes = box.utility_stylesheet_size();
  return result;
}

void print(const char* scenario, const Baseline& result) {
  std::cout << std::fixed << std::setprecision(3)
            << scenario << ": elements=" << result.elements
            << " active_tokens=" << result.active_tokens
            << " stylesheet_bytes=" << result.stylesheet_bytes
            << " first_update_ms=" << result.first_update_ms
            << " attribute_update_ms=" << result.attribute_update_ms
            << " utility_update_ms=" << result.utility_update_ms << '\n';
}

}  // namespace

int main() {
  try {
    print("typical", measure(500));
    print("large", measure(5000));
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "utility_jit_benchmark failed: " << ex.what() << '\n';
    return 1;
  }
}
