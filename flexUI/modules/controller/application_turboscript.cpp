#include "flexUI/application_turboscript.h"

#include <utility>

namespace flexUI {

ScriptModuleFactory make_turboscript_desktop_module_factory(TurboScriptControllerOptions options) {
  return [options](std::string_view source, std::string_view module_name) mutable {
    auto created = create_turboscript_controller_module(source, module_name, options);
    return ScriptModuleFactoryResult{std::move(created.module), std::move(created.error)};
  };
}

} // namespace flexUI
