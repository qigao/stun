/*
 * Flex Engine - Runtime Module
 *
 * Scene graph, animation, state machines, rendering
 * Independent of compiler/parser
 *
 * Usage:
 *   #include "flex/runtime.h"
 *   auto artboard = flex::Artboard::create(800, 600);
 *   auto shape = flex::Shape::create();
 *
 * For backend initialization (ThorVG example):
 *   #include "flex/backends/thorvg/init.h"
 *   flex::init();
 *   // ... use flex
 *   flex::shutdown();
 */

#pragma once

// Runtime core
#include "flex/runtime/allocator.h"
#include "flex/runtime/types.h"
#include "flex/runtime/node.h"
#include "flex/runtime/renderer.h"
#include "flex/runtime/instance_context.h"

// Scene graph nodes
#include "flex/runtime/artboard.h"
#include "flex/runtime/group.h"
#include "flex/runtime/shape.h"
#include "flex/runtime/text.h"
#include "flex/runtime/image.h"
#include "flex/runtime/svg.h"
#include "flex/runtime/path.h"
#include "flex/runtime/instance.h"
#include "flex/runtime/solo.h"

// Animation system
#include "flex/runtime/timeline.h" 

// State machine
#include "flex/runtime/fsm.h"
#include "flex/runtime/runtime_machine.h"

// Layout system
#include "flex/runtime/layout.h"
#include "flex/runtime/geometry.h"

// Data binding & scripting
#include "flex/runtime/binding.h"
#include "flex/runtime/state_binding.h"
#include "flex/runtime/script.h"

// Asset management
#include "flex/runtime/asset.h"

// Event system
#include "flex/runtime/event.h"

// Physics (optional)
#include "flex/runtime/physics.h"

// Component system
#include "flex/runtime/component.h"
#include "flex/runtime/instance.h"

// Utilities
#include "flex/runtime/solo.h"
#include "flex/runtime/debug.h"

// Standard library
#include <memory>
#include <string>
#include <vector>
