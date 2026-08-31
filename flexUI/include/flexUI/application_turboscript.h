#pragma once

#include "flexUI/application.h"
#include "flexUI/controller_turboscript.h"

namespace flexUI {

/// Returns a source-backed TurboScript factory for DesktopApplicationBuilder.
/// Native TurboScript plugins remain denied by the controller adapter.
ScriptModuleFactory
make_turboscript_desktop_module_factory(TurboScriptControllerOptions options = {});

} // namespace flexUI
