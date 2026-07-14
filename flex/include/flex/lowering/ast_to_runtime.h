/*
 * Flex Engine - AST to Runtime Lowering
 *
 * Preferred semantic path for lowering parsed DSL AST into executable core
 * objects and complete Definition/Instance state.
 *
 * Legacy alias: #include "flex/bridge/ast_to_runtime.h"
 */

#pragma once

#include "flex/dsl/flex_ast.h"
#include "flex/core/runtime_machine.h"
#include "flex/core/timeline.h"
#include <memory>

namespace flex {

// Forward declarations
class Definition;
class Scene;
class Node;

namespace parser {
struct AstProgram;
struct AstScene;
struct AstNode;
struct AstMachine;
struct AstLayer;
struct AstAnim;
struct AstAssets;
}

// ============================================================================
// AST to Runtime Converter
// ============================================================================

class AstToRuntimeConverter {
public:
  // Uses void* to avoid exposing Definition::Impl (private nested struct)
  AstToRuntimeConverter(void *definition_impl);
  ~AstToRuntimeConverter() = default;

  // Lower parsed AST program into executable objects owned by Definition::Impl.
  void convert(const parser::AstProgram &program);

private:
  void *impl_;

  // Scene conversion
  Scene* convert_scene(const std::shared_ptr<parser::AstScene> &scene);
  Node* convert_node(const std::shared_ptr<parser::AstNode> &ast_node);

  // Machine conversion
  void convert_machines(const std::vector<std::shared_ptr<parser::AstMachine>> &machines);
  RuntimeStateMachine::SharedPtr convert_machine(const parser::AstMachine &machine);
  void convert_layer(RuntimeStateMachine *machine, const parser::AstLayer &layer);

  // Animation conversion
  void convert_animations(const std::vector<std::shared_ptr<parser::AstAnim>> &animations);
  Timeline::SharedPtr convert_animation(const parser::AstAnim &anim);

  // Asset conversion
  void convert_assets(const parser::AstAssets &assets);

  // Utility
  LoopMode parse_loop_mode(const std::string &mode_str);
  uint32_t parse_color_rgba(const std::string &color_str);
};

} // namespace flex
