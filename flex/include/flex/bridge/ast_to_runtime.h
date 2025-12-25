/*
 * Flex Engine - AST to Runtime Converter (Header)
 * Converts parsed AST objects to runtime objects
 */

#pragma once

#include "flex/compiler/flex_ast.h"
#include <memory>

namespace flex {

// Forward declarations
class Definition;
class Artboard;
class Node;
class Timeline;
class RuntimeStateMachine;

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

  // Convert AST program to runtime objects
  void convert(const parser::AstProgram &program);

private:
  void *impl_;

  // Scene conversion
  std::shared_ptr<Artboard> convert_scene(const std::shared_ptr<parser::AstScene> &scene);
  std::shared_ptr<Node> convert_node(const std::shared_ptr<parser::AstNode> &ast_node);

  // Machine conversion
  void convert_machines(const std::vector<std::shared_ptr<parser::AstMachine>> &machines);
  std::shared_ptr<RuntimeStateMachine> convert_machine(const parser::AstMachine &machine);
  void convert_layer(RuntimeStateMachine *machine, const parser::AstLayer &layer);

  // Animation conversion
  void convert_animations(const std::vector<std::shared_ptr<parser::AstAnim>> &animations);
  std::shared_ptr<Timeline> convert_animation(const parser::AstAnim &anim);

  // Asset conversion
  void convert_assets(const parser::AstAssets &assets);

  // Utility
  enum class LoopMode parse_loop_mode(const std::string &mode_str);
  uint32_t parse_color_rgba(const std::string &color_str);
};

} // namespace flex
