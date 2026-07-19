/*
 * Flex DSL Parser Driver
 * Connects re2c lexer with lemon parser, implements AST-to-Runtime conversion
 *
 * Design: Lemon parser only validates syntax. AST construction happens here
 * using a token history buffer to track semantic values.
 */

#include "flex/dsl/flex_ast.h"
#include "flex/dsl/flex_parser.h"
#include "flex/dsl/flex_token.h"
#include "flex/core/expr.h"


#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <set>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <vector>

namespace flex {
namespace parser {

// ============================================================================
// Global Error State
// ============================================================================

static std::string g_last_error;
static int g_last_error_line = 0;
static int g_last_error_column = 0;

// ============================================================================
// Token History for Semantic Value Tracking
// ============================================================================

struct TokenHistory {
  std::deque<Token> tokens;
  size_t max_size = 32;

  void push(const Token &tok) {
    tokens.push_back(tok);
    if (tokens.size() > max_size) {
      tokens.pop_front();
    }
  }

  // Get token by offset from current (0 = most recent)
  const Token &get(size_t offset = 0) const {
    if (offset >= tokens.size()) {
      static Token empty{TOK_ERROR, "", 0, 0};
      return empty;
    }
    return tokens[tokens.size() - 1 - offset];
  }

  // Find most recent token of given type
  const Token *find_recent(int type, size_t max_lookback = 10) const {
    for (size_t i = 0; i < (std::min)(max_lookback, tokens.size()); i++) {
      const auto &tok = tokens[tokens.size() - 1 - i];
      if (tok.type == type) {
        return &tok;
      }
    }
    return nullptr;
  }
};

// ============================================================================
// Parse Context (must match lemon grammar definition)
// ============================================================================

struct ParseContext {
  AstProgram *program;
  std::stack<std::shared_ptr<AstNode>> node_stack;
  std::shared_ptr<AstScene> current_scene;
  std::shared_ptr<AstAnim> current_anim;
  AstTrack current_track;
  std::shared_ptr<AstMachine> current_machine;
  AstLayer current_layer;
  AstState current_state;
  AstTransition current_transition;
  std::stack<AstValue> value_stack;
  std::string error_message;
  int error_line = 0;
  int error_column = 0;

  // Token history for semantic value tracking
  TokenHistory *token_history = nullptr;

  ParseContext(AstProgram *p) : program(p), current_track("") {}
};

} // namespace parser
} // namespace flex

// Lemon parser declarations (C++ linkage, generated from flex_parser_gen.cpp)
void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *p, void (*freeProc)(void *));
void Parse(void *yyp, int yymajor, int yyminor, flex::parser::ParseContext *ctx);

namespace flex {
namespace parser {

// ============================================================================
// Event-Driven AST Builder
// ============================================================================

class UiPropertySeparatorValidator {
public:
  bool accept(const Token &token) {
    if (ui_brace_depth_ == 0) {
      if (token.type == TOK_UI) {
        header_state_ = HeaderState::Name;
      } else if (header_state_ == HeaderState::Name) {
        header_state_ = token.type == TOK_IDENTIFIER ? HeaderState::Brace : HeaderState::None;
      } else if (header_state_ == HeaderState::Brace) {
        if (token.type == TOK_LBRACE) {
          ui_brace_depth_ = 1;
        }
        header_state_ = HeaderState::None;
      }
      return true;
    }

    if (in_property_value_) {
      if (token.type == TOK_COMMA) {
        in_property_value_ = false;
      } else if (token.type == TOK_RBRACE) {
        in_property_value_ = false;
        --ui_brace_depth_;
      } else if (token.type == TOK_COLON || token.type == TOK_LBRACE) {
        error_line_ = token.line;
        error_column_ = token.column;
        return false;
      }
      return true;
    }

    if (token.type == TOK_COLON) {
      in_property_value_ = true;
    } else if (token.type == TOK_LBRACE) {
      ++ui_brace_depth_;
    } else if (token.type == TOK_RBRACE) {
      --ui_brace_depth_;
    }
    return true;
  }

  int error_line() const { return error_line_; }
  int error_column() const { return error_column_; }

private:
  enum class HeaderState { None, Name, Brace };

  HeaderState header_state_ = HeaderState::None;
  int ui_brace_depth_ = 0;
  bool in_property_value_ = false;
  int error_line_ = 0;
  int error_column_ = 0;
};

class AstBuilder {
public:
  AstBuilder(AstProgram *program) : program_(program) {}

  // Symbol table for constants and variables (name -> value)
  std::unordered_map<std::string, AstValue> symbols_;

  void on_token(const Token &tok, const Token *prev_tok) {
    // Flush pending const/var value when encountering a new top-level block or EOF
    if (expecting_const_value_ && !pending_value_tokens_.empty()) {
      bool should_flush = (tok.type == TOK_SCENE || tok.type == TOK_UI ||
                           tok.type == TOK_ARTBOARD ||
                           tok.type == TOK_CONST || tok.type == TOK_VAR ||
                           tok.type == TOK_DATA || tok.type == TOK_ANIM ||
                           tok.type == TOK_MACHINE || tok.type == TOK_COMPONENT ||
                           tok.type == TOK_ASSETS || tok.type == TOK_IMPORT ||
                           tok.type == TOK_EOF);
      if (should_flush) {
        flush_const_value();
      }
    }

    // Build AST based on token sequence
    switch (tok.type) {
    case TOK_SCENE:
    case TOK_ARTBOARD:
      // Next IDENTIFIER will be scene name
      expecting_scene_name_ = true;
      break;

    case TOK_UI:
      expecting_ui_name_ = true;
      break;

    case TOK_IDENTIFIER:
      handle_identifier(tok, prev_tok);
      break;

    case TOK_NODE_TYPE:
      // In assets block, image/svg are asset types
      if (in_assets_block_ && (tok.value == "image" || tok.value == "svg")) {
        pending_asset_type_ = tok.value;
        expecting_asset_id_ = true;
      } else {
        // Next IDENTIFIER will be node ID
        pending_node_type_ = tok.value;
        expecting_node_id_ = true;
      }
      break;

    case TOK_COMMA:
      // Flush pending value before processing next property
      if (expecting_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      if (in_anim_params_block_) {
        expecting_anim_param_key_ = true;
      }
      break;

    case TOK_LBRACE:
      handle_lbrace(tok);
      break;

    case TOK_RBRACE:
      handle_rbrace(tok);
      break;

    case TOK_COLON:
      if (!pending_prop_key_.empty() && !expecting_value_) {
        expecting_value_ = true;
        break;
      }
      // A new anim param may start immediately after the previous value.
      if (in_anim_params_block_ && expecting_anim_param_value_ && prev_tok &&
          prev_tok->type == TOK_IDENTIFIER && !pending_value_tokens_.empty()) {
        if (pending_value_tokens_.back().type == TOK_IDENTIFIER &&
            pending_value_tokens_.back().value == prev_tok->value) {
          pending_value_tokens_.pop_back();
        }
        flush_pending_value();
        pending_anim_param_key_ = prev_tok->value;
        expecting_anim_param_key_ = false;
        expecting_value_ = true;
        expecting_anim_param_value_ = true;
        break;
      }
      // After SET #node.property, expect value expression
      if (!pending_set_property_.empty()) {
        expecting_value_ = true;
        break;
      }
      // After anim param key, expect value expression
      if (!pending_anim_param_key_.empty() && !expecting_anim_param_value_) {
        expecting_value_ = true;
        expecting_anim_param_value_ = true;
        break;
      }
      // play: audioId (backward compat — PLAY followed by COLON)
      if (expecting_play_string_) {
        // "play" was treated as PLAY token, but it's actually "play: audioId"
        // Revert: treat as property key
        expecting_play_string_ = false;
        pending_prop_key_ = "play";
        expecting_value_ = true;
        break;
      }
      // Previous IDENTIFIER / DATA token is a property key
      if (prev_tok && (prev_tok->type == TOK_IDENTIFIER ||
                       prev_tok->type == TOK_NODE_TYPE ||
          (prev_tok->type == TOK_DATA &&
           (!node_stack_.empty() || in_pseudo_block_ || in_data_item_ ||
            (current_component_ && component_brace_depth_ > 0))))) {
        // If we were building a value for a previous property, finish it now
        if (expecting_value_ && !pending_value_tokens_.empty()) {
            // The previous token (which is the new key) may have been buffered as part of the prior value.
            // We must remove it from the value buffer of the *previous* property.
            if ((pending_value_tokens_.back().type == TOK_IDENTIFIER ||
                 pending_value_tokens_.back().type == TOK_NODE_TYPE ||
                 pending_value_tokens_.back().type == TOK_DATA) &&
                pending_value_tokens_.back().value == prev_tok->value) {
                pending_value_tokens_.pop_back();
            }

            flush_pending_value();
        }

        pending_prop_key_ = prev_tok->value;
        expecting_value_ = true;
      }
      // Clear any pending component type/id since this is a property, not a node
      pending_node_type_.clear();
      pending_node_id_.clear();
      expecting_node_id_ = false;
      expecting_template_suffix_ = false;
      break;

    case TOK_NUMBER:
    case TOK_STRING:
    case TOK_COLOR:
    case TOK_BOOL:
    case TOK_BINDING:
    case TOK_STAR:
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_SLASH:
    case TOK_LPAREN:
    case TOK_RPAREN:
      // Handle binding expression as transition condition: when ${expr}
      if (expecting_condition_var_ && tok.type == TOK_BINDING) {
        current_transition_.condition_expr = normalize_binding_expression(tok.value);
        expecting_condition_var_ = false;
        break;
      }
      // Handle import path (STRING token like "components/button.flex")
      if (expecting_import_path_ && tok.type == TOK_STRING) {
        program_->imports.emplace_back(tok.value, tok.line, tok.column);
        expecting_import_path_ = false;
      }
      // Handle asset path (STRING token like "sounds/click.wav")
      else if (expecting_asset_path_ && tok.type == TOK_STRING) {
        pending_asset_path_ = tok.value;
        expecting_asset_path_ = false;
        // Create and finalize the asset entry
        AstAsset asset(pending_asset_type_, pending_asset_id_, pending_asset_path_);
        if (!current_assets_) {
          current_assets_ = std::make_shared<AstAssets>();
        }
        current_assets_->assets.push_back(asset);
        pending_asset_type_.clear();
        pending_asset_id_.clear();
        pending_asset_path_.clear();
        // Note: If there's an options block after this, it will be handled in handle_lbrace
      }
      // Handle asset option value
      else if (expecting_asset_opt_value_) {
        // Option values don't support math yet
        if (!current_assets_ || current_assets_->assets.empty())
          break;
        auto &asset = current_assets_->assets.back();
        AstValue value;
        if (tok.type == TOK_NUMBER) {
          value = std::stof(tok.value);
        } else if (tok.type == TOK_BOOL) {
          value = (tok.value == "true");
        } else {
          value = tok.value;
        }
        asset.options[pending_asset_opt_key_] = value;
        pending_asset_opt_key_.clear();
        expecting_asset_opt_value_ = false;
        expecting_asset_opt_key_ = true; // Ready for next option
      }
      // Handle repeat count 
       else if (expecting_repeat_count_ && tok.type == TOK_NUMBER) {
        pending_repeat_count_ = static_cast<int>(std::stof(tok.value));
        expecting_repeat_count_ = false;
      }
      // Handle animation name
      else if (expecting_anim_name_ && tok.type == TOK_STRING) {
        current_anim_ = std::make_shared<parser::AstAnim>();
        current_anim_->name = tok.value;
        expecting_anim_name_ = false;
      }
      // Handle play "animName" (PLAY STRING)
      else if (expecting_play_string_ && tok.type == TOK_STRING) {
        current_state_.animation = tok.value;
        expecting_play_string_ = false;
        // Next might be WITH or end of statement
      }
      // Handle track property path
      else if (expecting_track_name_ && tok.type == TOK_STRING) {
        current_track_ = parser::AstTrack(tok.value);
        in_track_ = true;
        expecting_track_name_ = false;
      }
      // Handle const/var value expression
      else if (expecting_const_value_) {
        pending_value_tokens_.push_back(tok);
      }
      // Handle keyframe value
      else if (expecting_keyframe_value_) {
        // Keyframes now support buffering too
        pending_value_tokens_.push_back(tok);
      }
      // Handle property value or condition value
      else if (expecting_value_ || expecting_condition_val_) {
        // BUFFER the token
        pending_value_tokens_.push_back(tok);
      }
      // Handle transition condition op
      else if (tok.type == TOK_GT || tok.type == TOK_LT || tok.type == TOK_EQ || tok.type == TOK_NEQ) {
         // handled elsewhere
      }
      break;

    case TOK_ANIM:
      expecting_anim_name_ = true;
      break;

    case TOK_TRACK:
      expecting_track_name_ = true;
      break;

    case TOK_KEYFRAME:
      // Flush any pending keyframe value from previous keyframe
      if (expecting_keyframe_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      expecting_keyframe_time_ = true;
      break;

    case TOK_ARROW:
      if (expecting_keyframe_time_ && prev_tok && prev_tok->type == TOK_NUMBER) {
        pending_keyframe_time_ = std::stof(prev_tok->value);
        expecting_keyframe_time_ = false;
        expecting_keyframe_value_ = true;
      }
      break;

    case TOK_MACHINE:
      expecting_machine_name_ = true;
      break;

    case TOK_LAYER:
      expecting_layer_name_ = true;
      break;

    case TOK_STATE:
      expecting_state_name_ = true;
      break;

    case TOK_TRANSITION:
      // Flush any pending condition value from previous transition
      if (expecting_condition_val_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      // Push any pending transition before starting a new one
      if (has_pending_transition_ && in_layer_) {
        current_layer_.transitions.push_back(current_transition_);
        has_pending_transition_ = false;
      }
      expecting_transition_from_ = true;
      current_transition_ = AstTransition(); // Reset for new transition
      has_pending_transition_ = true;
      break;

    case TOK_WHEN:
      expecting_condition_var_ = true;
      break;

    case TOK_GT:
    case TOK_LT:
    case TOK_EQ:
    case TOK_NEQ:
      if (tok.type == TOK_GT)
        pending_condition_op_ = ">";
      else if (tok.type == TOK_LT)
        pending_condition_op_ = "<";
      else if (tok.type == TOK_EQ)
        pending_condition_op_ = "==";
      else if (tok.type == TOK_NEQ)
        pending_condition_op_ = "!=";
      expecting_condition_val_ = true;
      break;

    case TOK_COMPONENT:
      expecting_component_name_ = true;
      break;

    case TOK_REPEAT:
      expecting_repeat_count_ = true;
      break;

    case TOK_DATA:
      if (node_stack_.empty() && !in_pseudo_block_ && !in_data_item_ &&
          !(current_component_ && component_brace_depth_ > 0)) {
        expecting_data_name_ = true;
      }
      break;

    case TOK_ASSETS:
      expecting_assets_block_ = true;
      break;

    case TOK_AUDIO:
      if (in_assets_block_) {
        pending_asset_type_ = "audio";
        expecting_asset_id_ = true;
      }
      break;

    case TOK_FONT:
      if (in_assets_block_) {
        pending_asset_type_ = "font";
        expecting_asset_id_ = true;
      }
      break;

    case TOK_FOR:
      expecting_for_iterator_ = true;
      break;

    case TOK_CONST:
      expecting_const_name_ = true;
      pending_is_var_ = false;
      break;

    case TOK_VAR:
      expecting_const_name_ = true;
      pending_is_var_ = true;
      break;

    case TOK_ASSIGN:
      // After const/var name, expect expression
      if (!pending_const_name_.empty()) {
        expecting_const_value_ = true;
      }
      break;

    case TOK_IMPORT:
      expecting_import_path_ = true;
      break;

    case TOK_SET:
      // Flush any pending value from previous property (e.g., initial: true)
      if (expecting_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      // set #node.prop: value_expr
      expecting_set_hash_ = true;
      break;

    case TOK_PLAY:
      // Flush any pending value from previous property (e.g., initial: true)
      if (expecting_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      // play "anim" with { ... } OR play: audioId (backward compat handled via COLON)
      expecting_play_string_ = true;
      break;

    case TOK_WITH:
      // with { param: value, ... }
      expecting_anim_params_block_ = true;
      break;

    case TOK_HASH:
      if (expecting_set_hash_) {
        expecting_set_hash_ = false;
        expecting_set_node_id_ = true;
      }
      break;

    case TOK_IN:
      // Previous identifier was iterator name, next is data source
      expecting_for_source_ = true;
      break;

    case TOK_DOT:
      // Mark that we're expecting a property access after dot
      if (expecting_set_dot_) {
        expecting_set_dot_ = false;
        expecting_set_prop_name_ = true;
      } else if (!expecting_value_ && prev_tok && prev_tok->type == TOK_IDENTIFIER) {
        pending_prop_key_prefix_ = prev_tok->value;
        expecting_prop_key_suffix_ = true;
      } else if (prev_tok && prev_tok->type == TOK_IDENTIFIER) {
        pending_dot_object_ = prev_tok->value;
        expecting_dot_property_ = true;
      }
      break;

    case TOK_AT:
      // Check if this is part of a template ID (item@index)
      if (!pending_node_id_.empty()) {
        pending_node_id_ += "@";
        expecting_template_suffix_ = true;
      } else {
        // @index reference - next IDENTIFIER is the variable name
        expecting_at_var_ = true;
      }
      break;

    default:
      break;
    }
  }

  AstProgram *program() { return program_; }

  void finalize() {
    if (program_->scene) {
      expand_for_loops(program_->scene->children);
    }
    for (auto &ui_document : program_->ui_documents) {
      if (ui_document) {
        expand_for_loops(ui_document->children);
      }
    }
    for (auto &component : program_->components) {
      if (component) {
        expand_for_loops(component->children);
      }
    }
  }

private:
  std::string normalize_binding_expression(const std::string &value) const {
    std::string raw = value;
    if (raw.size() >= 3 && raw[0] == '$' && raw[1] == '{' && raw.back() == '}') {
      raw = raw.substr(2, raw.size() - 3);
    } else if (raw.size() >= 3 && raw[0] == '$' && raw[1] == '(' && raw.back() == ')') {
      raw = raw.substr(2, raw.size() - 3);
    }

    std::string cleaned;
    cleaned.reserve(raw.size());
    for (size_t ci = 0; ci < raw.size(); ++ci) {
      if (raw[ci] == '$') {
        size_t cj = ci + 1;
        if (cj < raw.size() &&
            (std::isalpha(static_cast<unsigned char>(raw[cj])) || raw[cj] == '_')) {
          continue;
        }
      }
      cleaned.push_back(raw[ci]);
    }
    return cleaned;
  }

  void handle_identifier(const Token &tok, const Token *prev_tok) {
    if (!tok.value.empty() && tok.value[0] == ':' && !node_stack_.empty()) {
      if (expecting_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      pending_pseudo_class_ = tok.value;
      expecting_value_ = false;
      return;
    }

    if (expecting_prop_key_suffix_) {
      pending_prop_key_ = pending_prop_key_prefix_ + "." + tok.value;
      pending_prop_key_prefix_.clear();
      expecting_prop_key_suffix_ = false;
      return;
    }

    // Handle const/var name
    if (expecting_const_name_) {
      pending_const_name_ = tok.value;
      expecting_const_name_ = false;
      return;
    }
    // Handle const/var value expression
    if (expecting_const_value_) {
      pending_value_tokens_.push_back(tok);
      return;
    }
    if (expecting_asset_id_) {
      pending_asset_id_ = tok.value;
      expecting_asset_id_ = false;
      expecting_asset_path_ = true;
    } else if (expecting_asset_opt_key_) {
      pending_asset_opt_key_ = tok.value;
      expecting_asset_opt_key_ = false;
      expecting_asset_opt_value_ = true;
    } else if (expecting_scene_name_) {
      current_scene_ = std::make_shared<AstScene>(tok.value);
      expecting_scene_name_ = false;
    } else if (expecting_ui_name_) {
      current_ui_ = std::make_shared<AstUiDocument>(tok.value);
      expecting_ui_name_ = false;
    } else if (expecting_template_suffix_) {
      // Append suffix to template ID (e.g., "item@" + "index" = "item@index")
      pending_node_id_ += tok.value;
      expecting_template_suffix_ = false;
    } else if (expecting_node_id_) {
      // Store node ID but don't create node yet - wait for LBRACE
      // This allows us to capture template IDs like "item@index"
      pending_node_id_ = tok.value;
      expecting_node_id_ = false;
    } else if (expecting_anim_name_) {
      // Animation name comes as STRING, not IDENTIFIER
    } else if (expecting_machine_name_) {
      current_machine_ = std::make_shared<AstMachine>(tok.value);
      expecting_machine_name_ = false;
    } else if (expecting_layer_name_) {
      current_layer_ = AstLayer(tok.value);
      expecting_layer_name_ = false;
    } else if (expecting_state_name_) {
      current_state_ = AstState(tok.value);
      expecting_state_name_ = false;
    } else if (expecting_transition_from_) {
      current_transition_.from_state = tok.value;
      expecting_transition_from_ = false;
      expecting_transition_to_ = true;
    } else if (expecting_transition_to_) {
      current_transition_.to_state = tok.value;
      expecting_transition_to_ = false;
    } else if (expecting_component_name_) {
      current_component_ = std::make_shared<AstComponent>(tok.value);
      expecting_component_name_ = false;
    } else if (expecting_condition_var_) {
      // BINDING token as condition expression: when ${speed > 5 and health < 50}
      if (tok.type == TOK_BINDING) {
        current_transition_.condition_expr = normalize_binding_expression(tok.value);
        expecting_condition_var_ = false;
      } else {
        // Legacy: identifier as condition variable name
        pending_condition_var_ = tok.value;
        expecting_condition_var_ = false;
      }
    } else if (expecting_at_var_) {
      // @variable reference - store as "@variable" string
      set_property(AstValue("@" + tok.value));
      expecting_at_var_ = false;
      expecting_value_ = false;
    } else if (expecting_set_node_id_) {
      pending_set_node_id_ = tok.value;
      expecting_set_node_id_ = false;
      expecting_set_dot_ = true;
    } else if (expecting_set_prop_name_) {
      pending_set_property_ = tok.value;
      expecting_set_prop_name_ = false;
      // Next token should be COLON then value_expr — handled by expecting_value_ flow
    } else if (expecting_anim_param_key_) {
      pending_anim_param_key_ = tok.value;
      expecting_anim_param_key_ = false;
      // Next token should be COLON then value_expr
    } else if (expecting_data_name_) {
      // Start a new data block
      current_data_ = std::make_shared<AstData>(tok.value);
      expecting_data_name_ = false;
    } else if (expecting_data_item_key_) {
      // New item in data block
      current_data_item_ = AstDataItem(tok.value);
      expecting_data_item_key_ = false;
    } else if (expecting_for_iterator_) {
      // Store iterator name for for loop
      pending_for_iterator_ = tok.value;
      expecting_for_iterator_ = false;
    } else if (expecting_for_source_) {
      // Store data source for for loop
      pending_for_source_ = tok.value;
      expecting_for_source_ = false;
    } else if (expecting_dot_property_) {
      // Handle item.property - store as expression placeholder
      std::string expr = "{" + pending_dot_object_ + "." + tok.value + "}";
      if (expecting_value_) {
        set_property(AstValue(expr));
        expecting_value_ = false;
      }
      expecting_dot_property_ = false;
      pending_dot_object_.clear();
    } else if (expecting_value_) {
        // Identifier can be part of an expression or an Enum value
        // Buffer it
        pending_value_tokens_.push_back(tok);
      } else if (prev_tok && prev_tok->type == TOK_IDENTIFIER) {
        // Two identifiers in a row...
        pending_node_type_ = prev_tok->value;
        pending_node_id_ = tok.value;
      }
  }

  // Evaluate a math expression from tokens through the shared MIR backend,
  // substituting numeric constants from the parser symbol table.
  // Returns true if successfully evaluated as a number, false if should keep as string
  bool try_evaluate_expression(const std::vector<Token>& tokens, float& result) {
    // Check if this is a pure math expression (numbers, operators, parens, and known symbols)
    bool has_binding = false;
    bool has_unknown_identifier = false;

    for (const auto& tok : tokens) {
      if (tok.type == TOK_BINDING) {
        has_binding = true;
        break;
      }
      if (tok.type == TOK_IDENTIFIER) {
        // Check if it's a known constant
        if (symbols_.find(tok.value) == symbols_.end()) {
          has_unknown_identifier = true;
        }
      }
    }

    // If has binding like ${m.rot}, can't evaluate at parse time
    if (has_binding || has_unknown_identifier) {
      return false;
    }

    // Build expression string, substituting constants
    std::string expr;
    for (const auto& tok : tokens) {
      if (tok.type == TOK_IDENTIFIER) {
        auto it = symbols_.find(tok.value);
        if (it != symbols_.end()) {
          if (auto* fval = std::get_if<float>(&it->second)) {
            expr += std::to_string(*fval);
          } else {
            return false; // Non-numeric constant
          }
        }
      } else if (tok.type == TOK_NUMBER || tok.type == TOK_PLUS ||
                 tok.type == TOK_MINUS || tok.type == TOK_STAR ||
                 tok.type == TOK_SLASH || tok.type == TOK_LPAREN ||
                 tok.type == TOK_RPAREN) {
        expr += tok.value;
      } else {
        return false; // Unknown token type for math
      }
      expr += " ";
    }

    Expr evaluator;
    result = static_cast<float>(evaluator.eval(expr));
    return true;
  }

  // Flush a const/var declaration
  void flush_const_value() {
    if (pending_const_name_.empty() || pending_value_tokens_.empty()) {
      pending_const_name_.clear();
      pending_value_tokens_.clear();
      expecting_const_value_ = false;
      return;
    }

    // Try to evaluate as numeric expression
    float numeric_result;
    AstValue value;

    if (pending_value_tokens_.size() == 1) {
      const auto& tok = pending_value_tokens_[0];
      if (tok.type == TOK_NUMBER) {
        value = std::stof(tok.value);
      } else if (tok.type == TOK_BOOL) {
        value = (tok.value == "true");
      } else if (tok.type == TOK_STRING) {
        value = tok.value;
      } else if (tok.type == TOK_IDENTIFIER) {
        // Reference to another constant
        auto it = symbols_.find(tok.value);
        if (it != symbols_.end()) {
          value = it->second;
        } else {
          value = tok.value;
        }
      } else {
        value = tok.value;
      }
    } else if (try_evaluate_expression(pending_value_tokens_, numeric_result)) {
      value = numeric_result;
    } else {
      // Keep as string expression
      std::string expr;
      for (size_t i = 0; i < pending_value_tokens_.size(); i++) {
        if (i > 0) expr += " ";
        expr += pending_value_tokens_[i].value;
      }
      value = expr;
    }

    // Store in symbol table
    symbols_[pending_const_name_] = value;

    // Store in AST
    program_->constants.emplace_back(pending_const_name_, value, pending_is_var_);

    pending_const_name_.clear();
    pending_value_tokens_.clear();
    expecting_const_value_ = false;
  }

  void flush_pending_value() {
     if (pending_value_tokens_.empty()) return;

     AstValue value;

     // Try to evaluate as numeric expression first
     float numeric_result;
     if (pending_value_tokens_.size() == 1 && pending_value_tokens_[0].type != TOK_BINDING) {
         // Simple single token
         const auto& tok = pending_value_tokens_[0];
         switch (tok.type) {
            case TOK_NUMBER: value = std::stof(tok.value); break;
            case TOK_BOOL: value = (tok.value == "true"); break;
            case TOK_IDENTIFIER: {
              // Check if it's a constant reference
              auto it = symbols_.find(tok.value);
              if (it != symbols_.end()) {
                value = it->second;
              } else {
                value = tok.value;
              }
              break;
            }
            default: value = tok.value; break;
         }
     } else if (try_evaluate_expression(pending_value_tokens_, numeric_result)) {
         // Successfully evaluated math expression
         value = numeric_result;
     } else {
         // Expression or Binding -> Stringify
         std::string expr;
         for (size_t i = 0; i < pending_value_tokens_.size(); i++) {
             if (i > 0) expr += " ";
             expr += pending_value_tokens_[i].value;
         }
         value = expr;
     }

     if (expecting_keyframe_value_) {
         current_track_.keyframes.emplace_back(pending_keyframe_time_, value);
         expecting_keyframe_value_ = false;
     } else if (expecting_condition_val_) {
         // Synthesize condition_expr from legacy var op val
         if (auto* fval = std::get_if<float>(&value)) {
             current_transition_.condition_expr = pending_condition_var_ + " " + pending_condition_op_ + " " + std::to_string(*fval);
         }
         pending_condition_var_.clear();
         pending_condition_op_.clear();
         expecting_condition_val_ = false;
     } else if (!pending_set_property_.empty()) {
         // State action: set #node.prop: value_expr
         AstStateAction action;
         action.node_id = pending_set_node_id_;
         action.property = pending_set_property_;
         // For expression values, store the expression string
         if (auto* sval = std::get_if<std::string>(&value)) {
             action.expression = normalize_binding_expression(*sval);
         } else if (auto* fval = std::get_if<float>(&value)) {
             action.expression = std::to_string(*fval);
         }
         current_state_.actions.push_back(action);
         pending_set_node_id_.clear();
         pending_set_property_.clear();
     } else if (expecting_anim_param_value_) {
         // Animation parameter value
         if (auto* sval = std::get_if<std::string>(&value)) {
             current_state_.animation_params[pending_anim_param_key_] =
                 normalize_binding_expression(*sval);
         } else if (auto* fval = std::get_if<float>(&value)) {
             current_state_.animation_params[pending_anim_param_key_] = std::to_string(*fval);
         }
         pending_anim_param_key_.clear();
         expecting_anim_param_value_ = false;
         if (in_anim_params_block_) {
             expecting_anim_param_key_ = true;
         }
     } else {
         set_property(value);
     }

     pending_value_tokens_.clear();
     expecting_value_ = false;
  }

  void append_completed_node(const std::shared_ptr<AstNode>& node) {
    if (!node) {
      return;
    }
    if (!node_stack_.empty()) {
      node_stack_.top()->children.push_back(node);
    } else if (current_scene_) {
      current_scene_->children.push_back(node);
    } else if (current_ui_) {
      current_ui_->children.push_back(node);
    } else if (current_component_ && component_brace_depth_ > 0) {
      current_component_->children.push_back(node);
    }
  }

  void handle_lbrace(const Token &tok) {
    // Flush any pending value before entering block
    if (expecting_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
    }

    brace_depth_++;

    if (!pending_pseudo_class_.empty()) {
      in_pseudo_block_ = true;
      pseudo_brace_depth_ = brace_depth_;
      current_pseudo_class_ = pending_pseudo_class_;
      current_pseudo_props_.clear();
      pending_pseudo_class_.clear();
      return;
    }

    // Track entering assets block
    if (expecting_assets_block_) {
      in_assets_block_ = true;
      assets_brace_depth_ = brace_depth_;
      expecting_assets_block_ = false;
      if (!current_assets_) {
        current_assets_ = std::make_shared<AstAssets>();
      }
      return;
    }

    // Track entering anim params block (play "anim" with { ... })
    if (expecting_anim_params_block_) {
      in_anim_params_block_ = true;
      anim_params_brace_depth_ = brace_depth_;
      expecting_anim_params_block_ = false;
      expecting_anim_param_key_ = true;
      return;
    }

    // Track entering asset options block (nested brace inside assets block)
    if (in_assets_block_ && !in_asset_opts_ && current_assets_ && !current_assets_->assets.empty()) {
      in_asset_opts_ = true;
      asset_opts_brace_depth_ = brace_depth_;
      expecting_asset_opt_key_ = true;
      return;
    }

    // Track entering repeat block
    if (pending_repeat_count_ > 0 && repeat_brace_depth_ == 0) {
      repeat_brace_depth_ = brace_depth_;
      // Create a virtual "repeat" node to collect template children
      auto repeat_node = std::make_shared<AstNode>("repeat", "");
      repeat_node->repeat_count = pending_repeat_count_;
      node_stack_.push(repeat_node);
      pending_repeat_count_ = 0;
    }
    // Track entering data block
    else if (current_data_ && data_brace_depth_ == 0) {
      data_brace_depth_ = brace_depth_;
      expecting_data_item_key_ = true;
    }
    // Track entering data item (nested brace inside data block)
    else if (in_data_item_ == false && data_brace_depth_ > 0 &&
             current_data_item_.key.length() > 0) {
      in_data_item_ = true;
      data_item_brace_depth_ = brace_depth_;
      // Clear expecting_value_ since the COLON after item key is not a property assignment
      expecting_value_ = false;
      pending_prop_key_.clear();
    }
    // Track entering for loop block
    else if (!pending_for_source_.empty() && for_brace_depth_ == 0) {
      for_brace_depth_ = brace_depth_;
      // Create a virtual "for" node to collect template children
      auto for_node = std::make_shared<AstNode>("for", "");
      // Store iterator and source in properties for later expansion
      for_node->properties["__iterator"] = pending_for_iterator_;
      for_node->properties["__source"] = pending_for_source_;
      node_stack_.push(for_node);
      pending_for_iterator_.clear();
      pending_for_source_.clear();
    }
    // Track entering animation
    else if (current_anim_ && anim_brace_depth_ == 0) {
      anim_brace_depth_ = brace_depth_;
    }
    // Track entering machine
    else if (current_machine_ && machine_brace_depth_ == 0) {
      machine_brace_depth_ = brace_depth_;
    }
    // Track entering component
    else if (current_component_ && component_brace_depth_ == 0) {
      component_brace_depth_ = brace_depth_;
    }

    // Create pending node if we have a node type (deferred from handle_identifier)
    if (!pending_node_type_.empty()) {
      auto node = std::make_shared<AstNode>(pending_node_type_, pending_node_id_);
      node_stack_.push(node);
      pending_node_type_.clear();
      pending_node_id_.clear();
    }

    // Track entering layer
    if (expecting_layer_name_ == false && current_layer_.name.length() > 0 && !in_layer_) {
      in_layer_ = true;
      layer_brace_depth_ = brace_depth_;
    }
    // Track entering state
    if (expecting_state_name_ == false && current_state_.name.length() > 0 && !in_state_) {
      in_state_ = true;
      state_brace_depth_ = brace_depth_;
    }
  }

  // Helper: substitute @index in node IDs and properties
  void substitute_index(std::shared_ptr<AstNode> &node, int index) {
    // Substitute in ID
    size_t pos;
    while ((pos = node->id.find("@index")) != std::string::npos) {
      node->id.replace(pos, 6, std::to_string(index));
    }

    // Substitute in string properties, converting to float if result is numeric
    for (auto &[key, value] : node->properties) {
      if (auto *sval = std::get_if<std::string>(&value)) {
        bool had_substitution = false;
        while ((pos = sval->find("@index")) != std::string::npos) {
          sval->replace(pos, 6, std::to_string(index));
          had_substitution = true;
        }

        // If we did substitution and the result is a pure number, convert to float
        if (had_substitution) {
          try {
            size_t processed = 0;
            float fval = std::stof(*sval, &processed);
            if (processed == sval->length()) {
              // Entire string was a number - convert to float
              value = fval;
            }
          } catch (...) {
            // Not a number, keep as string
          }
        }
      }
    }

    // Recurse into children
    for (auto &child : node->children) {
      substitute_index(child, index);
    }
  }

  // Helper: substitute $(iterator.property) expressions in for loop
  void substitute_for_item(std::shared_ptr<AstNode> &node, const std::string &iterator,
                           const AstDataItem &item, int index) {
    // Substitute in ID: e.g., "item" -> "item0" (only if exact match)
    size_t pos;
    if (node->id == iterator) {
      node->id = iterator + std::to_string(index);
    }

    // Substitute in string properties
    for (auto &[key, value] : node->properties) {
      if (auto *sval = std::get_if<std::string>(&value)) {
        std::string result = *sval;
        auto resolve_item_value = [&](const std::string& prop_name) -> std::string {
          auto it = item.properties.find(prop_name);
          if (it == item.properties.end()) {
            return "";
          }
          if (auto *fval = std::get_if<float>(&it->second)) {
            std::ostringstream oss;
            oss << *fval;
            return oss.str();
          }
          if (auto *str = std::get_if<std::string>(&it->second)) {
            return *str;
          }
          if (auto *bval = std::get_if<bool>(&it->second)) {
            return *bval ? "true" : "false";
          }
          return "";
        };

        auto replace_binding = [&](const std::string& prefix, const std::string& suffix) {
          size_t start = 0;
          while ((pos = result.find(prefix, start)) != std::string::npos) {
            size_t end = result.find(suffix, pos + prefix.length());
            if (end == std::string::npos) {
              break;
            }

            std::string prop_name = result.substr(
                pos + prefix.length(), end - pos - prefix.length());
            std::string replacement = resolve_item_value(prop_name);
            result.replace(pos, end - pos + suffix.length(), replacement);
            start = pos + replacement.length();
          }
        };

        replace_binding("${" + iterator + ".", "}");
        replace_binding("$(" + iterator + ".", ")");

        while ((pos = result.find("${index}")) != std::string::npos) {
          result.replace(pos, 8, std::to_string(index));
        }
        while ((pos = result.find("$(index)")) != std::string::npos) {
          result.replace(pos, 8, std::to_string(index));
        }

        // --- MATH EVALUATION ---
        if (result.find('*') != std::string::npos || result.find('/') != std::string::npos ||
            result.find('+') != std::string::npos || result.find('-') != std::string::npos) {
             try {
                 bool is_math = true;
                 for (char c : result) {
                     if (!isdigit(c) && c != '.' && c != ' ' && c != '*' && c != '/' && c != '+' && c != '-' && c != '(' && c != ')') {
                         is_math = false; break;
                     }
                 }

                 if (is_math) {
                     // Recursive descent parser for math with parentheses
                     std::function<float(const char*&)> parse_expr;
                     std::function<float(const char*&)> parse_term;
                     std::function<float(const char*&)> parse_factor;

                     parse_factor = [&](const char*& p) -> float {
                         while (*p == ' ') p++;
                         if (*p == '(') {
                             p++;
                             float val = parse_expr(p);
                             while (*p == ' ') p++;
                             if (*p == ')') p++;
                             return val;
                         }
                         char* end;
                         float val = strtof(p, &end);
                         p = end;
                         return val;
                     };

                     parse_term = [&](const char*& p) -> float {
                         float left = parse_factor(p);
                         while (true) {
                             while (*p == ' ') p++;
                             if (*p == '*') { p++; left *= parse_factor(p); }
                             else if (*p == '/') { p++; left /= parse_factor(p); }
                             else break;
                         }
                         return left;
                     };

                     parse_expr = [&](const char*& p) -> float {
                         float left = parse_term(p);
                         while (true) {
                             while (*p == ' ') p++;
                             if (*p == '+') { p++; left += parse_term(p); }
                             else if (*p == '-') { p++; left -= parse_term(p); }
                             else break;
                         }
                         return left;
                     };

                     const char* p = result.c_str();
                     float computed = parse_expr(p);
                     while (*p == ' ') p++;
                     if (*p == '\0') {
                         std::ostringstream oss;
                         oss << computed;
                         result = oss.str();
                     }
                 }
             } catch (...) {}
        }

        // If result is a pure number, convert to float
        if (result != *sval) {
          *sval = result;
          try {
            size_t processed = 0;
            float fval = std::stof(result, &processed);
            if (processed == result.length()) {
              value = fval;
            }
          } catch (...) {
            // Keep as string
          }
        }
      }
    }

    // Recurse into children
    for (auto &child : node->children) {
      substitute_for_item(child, iterator, item, index);
    }
  }

  AstData *find_data_block(const std::string &source) {
    for (const auto &db : program_->data_blocks) {
      if (db && db->name == source) {
        return db.get();
      }
    }
    return nullptr;
  }

  void expand_for_node(const std::shared_ptr<AstNode> &for_node,
                       std::vector<std::shared_ptr<AstNode>> &output_nodes) {
    if (!for_node) {
      return;
    }

    auto iterator_it = for_node->properties.find("__iterator");
    auto source_it = for_node->properties.find("__source");
    if (iterator_it == for_node->properties.end() ||
        source_it == for_node->properties.end()) {
      return;
    }

    auto iterator = std::get_if<std::string>(&iterator_it->second);
    auto source = std::get_if<std::string>(&source_it->second);
    if (!iterator || !source) {
      return;
    }

    AstData *data = find_data_block(*source);
    if (!data) {
      return;
    }

    int index = 0;
    for (const auto &item : data->items) {
      for (const auto &template_child : for_node->children) {
        auto cloned = template_child->clone();
        substitute_for_item(cloned, *iterator, item, index);
        expand_for_loops(cloned->children);
        output_nodes.push_back(cloned);
      }
      index++;
    }
  }

  void expand_for_loops(std::vector<std::shared_ptr<AstNode>> &nodes) {
    std::vector<std::shared_ptr<AstNode>> expanded_nodes;
    expanded_nodes.reserve(nodes.size());

    for (auto &node : nodes) {
      if (!node) {
        continue;
      }

      if (node->type == "for") {
        expand_for_node(node, expanded_nodes);
        continue;
      }

      expand_for_loops(node->children);
      expanded_nodes.push_back(node);
    }

    nodes = std::move(expanded_nodes);
  }

  void handle_rbrace(const Token &tok) {
    // Flush any pending value before processing closing brace
    if (expecting_value_ && !pending_value_tokens_.empty()) {
      flush_pending_value();
    }

    brace_depth_--;

    if (in_pseudo_block_ && brace_depth_ == pseudo_brace_depth_ - 1) {
      if (!node_stack_.empty()) {
        node_stack_.top()->pseudo_classes[current_pseudo_class_] = current_pseudo_props_;
      }
      current_pseudo_class_.clear();
      current_pseudo_props_.clear();
      in_pseudo_block_ = false;
      pseudo_brace_depth_ = 0;
      return;
    }

    // Check for asset options block completion
    if (in_asset_opts_ && brace_depth_ == asset_opts_brace_depth_ - 1) {
      in_asset_opts_ = false;
      expecting_asset_opt_key_ = false;
      return;
    }

    // Check for anim params block completion (play "anim" with { ... })
    if (in_anim_params_block_ && brace_depth_ == anim_params_brace_depth_ - 1) {
      in_anim_params_block_ = false;
      expecting_anim_param_key_ = false;
      return;
    }

    // Check for assets block completion
    if (in_assets_block_ && brace_depth_ == assets_brace_depth_ - 1) {
      program_->assets = current_assets_;
      current_assets_ = nullptr;
      in_assets_block_ = false;
      assets_brace_depth_ = 0;
      return;
    }

    // Check for repeat block completion - expand template N times
    if (repeat_brace_depth_ > 0 && brace_depth_ == repeat_brace_depth_ - 1) {
      if (!node_stack_.empty() && node_stack_.top()->type == "repeat") {
        auto repeat_node = node_stack_.top();
        node_stack_.pop();

        // Expand: clone template children N times with @index substitution
        for (int i = 0; i < repeat_node->repeat_count; i++) {
          for (const auto &template_child : repeat_node->children) {
            auto cloned = template_child->clone();
            substitute_index(cloned, i);
            append_completed_node(cloned);
          }
        }
      }
      repeat_brace_depth_ = 0;
      return; // Don't process as regular node close
    }

    // Check for data item completion
    if (in_data_item_ && brace_depth_ == data_item_brace_depth_ - 1) {
      current_data_->items.push_back(current_data_item_);
      current_data_item_ = AstDataItem();
      in_data_item_ = false;
      expecting_data_item_key_ = true; // Ready for next item
      return;
    }

    // Check for data block completion
    if (current_data_ && brace_depth_ == data_brace_depth_ - 1) {
      program_->data_blocks.push_back(current_data_);
      current_data_ = nullptr;
      data_brace_depth_ = 0;
      expecting_data_item_key_ = false;
      return;
    }

    // Check for for loop completion - expand template for each data item
    if (for_brace_depth_ > 0 && brace_depth_ == for_brace_depth_ - 1) {
      if (!node_stack_.empty() && node_stack_.top()->type == "for") {
        auto for_node = node_stack_.top();
        node_stack_.pop();

        // Get iterator name and data source
        std::string iterator = std::get<std::string>(for_node->properties["__iterator"]);
        std::string source = std::get<std::string>(for_node->properties["__source"]);

        // Find the data block
        AstData *data = find_data_block(source);

        if (data) {
          // Expand: clone template children for each data item
          int index = 0;
          for (const auto &item : data->items) {
            for (const auto &template_child : for_node->children) {
              auto cloned = template_child->clone();
              substitute_for_item(cloned, iterator, item, index);
              append_completed_node(cloned);
            }
            index++;
          }
        } else {
          // Defer expansion until the full source has been parsed so a later
          // top-level data block can still satisfy the loop.
          append_completed_node(for_node);
        }
      }
      for_brace_depth_ = 0;
      return;
    }

    // Pop completed constructs
    if (!node_stack_.empty() && node_stack_.size() > static_cast<size_t>(scene_node_depth_)) {
      auto node = node_stack_.top();
      node_stack_.pop();
      append_completed_node(node);
    }

    // Check for scene completion
    if (current_scene_ && brace_depth_ == 0 && node_stack_.empty()) {
      program_->scene = current_scene_;
      current_scene_ = nullptr;
    }

    if (current_ui_ && brace_depth_ == 0 && node_stack_.empty()) {
      program_->ui_documents.push_back(current_ui_);
      current_ui_ = nullptr;
    }

    // Check for animation completion
    // Track closes when we drop back to anim level (from depth N to N-1, where N was track depth)
    if (current_anim_ && in_track_ && brace_depth_ == anim_brace_depth_) {
      // Flush any pending keyframe value before closing track
      if (expecting_keyframe_value_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      current_anim_->tracks.push_back(current_track_);
      current_track_ = AstTrack("");
      in_track_ = false;
    } else if (current_anim_ && brace_depth_ == anim_brace_depth_ - 1) {
      program_->animations.push_back(current_anim_);
      current_anim_ = nullptr;
      anim_brace_depth_ = 0;
    }

    // Check for state machine completion
    // State closes when we drop back to layer level
    if (in_state_ && brace_depth_ == state_brace_depth_ - 1) {
      current_layer_.states.push_back(current_state_);
      current_state_ = AstState();
      in_state_ = false;
    } else if (in_layer_ && brace_depth_ == layer_brace_depth_ - 1) {
      // Flush any pending condition value before closing
      if (expecting_condition_val_ && !pending_value_tokens_.empty()) {
        flush_pending_value();
      }
      // Push any pending transition before closing the layer
      if (has_pending_transition_) {
        current_layer_.transitions.push_back(current_transition_);
        has_pending_transition_ = false;
        current_transition_ = AstTransition();
      }
      if (current_machine_) {
        current_machine_->layers.push_back(current_layer_);
      }
      current_layer_ = AstLayer();
      in_layer_ = false;
    } else if (current_machine_ && brace_depth_ == machine_brace_depth_ - 1) {
      program_->machines.push_back(current_machine_);
      current_machine_ = nullptr;
      machine_brace_depth_ = 0;
    }

    // Check for component completion
    if (current_component_ && brace_depth_ == component_brace_depth_ - 1) {
      program_->components.push_back(current_component_);
      current_component_ = nullptr;
      component_brace_depth_ = 0;
    }
  }

    void handle_value(const Token &tok) {
        // Legacy: Just push to buffer now.
        // This function is only kept if other parts call it directly (none found), 
        // or for simple cases. 
        // We moved logic to flush_pending_value().
    }

    /*
    AstValue value;

    switch (tok.type) {
    case TOK_NUMBER:
      value = std::stof(tok.value);
      break;
    ...
    case TOK_STRING:
      value = tok.value;
      break;
    case TOK_COLOR:
      value = tok.value;
      break;
    case TOK_BOOL:
      value = (tok.value == "true");
      break;
    case TOK_BINDING:
      // Store binding expression as string (e.g., "$(pos.x)")
      value = tok.value;
      break;
    default:
      value = tok.value;
      break;
    }

    if (expecting_keyframe_value_) {
      current_track_.keyframes.emplace_back(pending_keyframe_time_, value);
      expecting_keyframe_value_ = false;
    } else if (expecting_condition_val_) {
      if (auto *fval = std::get_if<float>(&value)) {
        current_transition_.condition_val = *fval;
      }
      expecting_condition_val_ = false;
    } else {
      set_property(value);
    }

    expecting_value_ = false;
  }
  */

  void set_property(const AstValue &value) {
    if (pending_prop_key_.empty())
      return;

    // Set on appropriate context
    // Data item properties have highest priority
    if (in_data_item_) {
      current_data_item_.properties[pending_prop_key_] = value;
    } else if (in_pseudo_block_) {
      current_pseudo_props_[pending_prop_key_] = value;
    } else if (!node_stack_.empty()) {
      node_stack_.top()->properties[pending_prop_key_] = value;
    } else if (current_scene_) {
      if (pending_prop_key_ == "width") {
        if (auto *fval = std::get_if<float>(&value)) {
          current_scene_->width = *fval;
        }
      } else if (pending_prop_key_ == "height") {
        if (auto *fval = std::get_if<float>(&value)) {
          current_scene_->height = *fval;
        }
      }
    } else if (current_component_ && component_brace_depth_ > 0) {
      current_component_->default_props[pending_prop_key_] = value;
    } else if (current_anim_) {
      if (pending_prop_key_ == "duration") {
        if (auto *fval = std::get_if<float>(&value)) {
          current_anim_->duration = *fval;
        }
      } else if (pending_prop_key_ == "loop") {
        if (auto *sval = std::get_if<std::string>(&value)) {
          current_anim_->loop_mode = *sval;
        }
      }
    } else if (in_state_) {
      if (pending_prop_key_ == "initial") {
        if (auto *bval = std::get_if<bool>(&value)) {
          current_state_.initial = *bval;
        }
      } else if (pending_prop_key_ == "animation") {
        if (auto *sval = std::get_if<std::string>(&value)) {
          current_state_.animation = *sval;
        }
      } else if (pending_prop_key_ == "play") {
        // play: can be string or identifier (asset ID)
        if (auto *sval = std::get_if<std::string>(&value)) {
          current_state_.play_audio = *sval;
        }
      } else if (pending_prop_key_ == "stop") {
        // stop: can be string or identifier (asset ID)
        if (auto *sval = std::get_if<std::string>(&value)) {
          current_state_.stop_audio = *sval;
        }
      }
    }

    pending_prop_key_.clear();
  }

  AstProgram *program_;

  // Scene building
  std::shared_ptr<AstScene> current_scene_;
  std::shared_ptr<AstUiDocument> current_ui_;
  std::stack<std::shared_ptr<AstNode>> node_stack_;
  int scene_node_depth_ = 0;

  // Animation building
  std::shared_ptr<AstAnim> current_anim_;
  AstTrack current_track_{""};
  int anim_brace_depth_ = 0;
  bool in_track_ = false;
  float pending_keyframe_time_ = 0;

  // State machine building
  std::shared_ptr<AstMachine> current_machine_;
  AstLayer current_layer_;
  AstState current_state_;
  AstTransition current_transition_;
  int machine_brace_depth_ = 0;
  int layer_brace_depth_ = 0;
  int state_brace_depth_ = 0;
  bool in_layer_ = false;
  bool in_state_ = false;

  // Component building
  std::shared_ptr<AstComponent> current_component_;
  int component_brace_depth_ = 0;

  // Repeat block building
  int repeat_brace_depth_ = 0;
  int pending_repeat_count_ = 0;

  // Data block building
  std::shared_ptr<AstData> current_data_;
  AstDataItem current_data_item_;
  int data_brace_depth_ = 0;
  int data_item_brace_depth_ = 0;
  bool in_data_item_ = false;

  // For loop building
  int for_brace_depth_ = 0;
  std::string pending_for_iterator_;
  std::string pending_for_source_;
  std::string pending_dot_object_;

  // Parser state
  int brace_depth_ = 0;
  std::string pending_node_type_;
  std::string pending_node_id_;
  std::string pending_prop_key_;

  // Expectation flags
  bool expecting_scene_name_ = false;
  bool expecting_ui_name_ = false;
  bool expecting_node_id_ = false;
  bool expecting_template_suffix_ = false;
  bool expecting_value_ = false;
  std::vector<Token> pending_value_tokens_; // Added for buffering
  bool expecting_anim_name_ = false;
  bool expecting_track_name_ = false;
  bool expecting_keyframe_time_ = false;
  bool expecting_keyframe_value_ = false;
  bool expecting_machine_name_ = false;
  bool expecting_layer_name_ = false;
  bool expecting_state_name_ = false;
  bool expecting_transition_from_ = false;
  bool expecting_transition_to_ = false;
  bool expecting_component_name_ = false;
  bool expecting_condition_var_ = false;
  bool expecting_condition_val_ = false;
  bool has_pending_transition_ = false;
  bool expecting_repeat_count_ = false;
  bool expecting_at_var_ = false;

  // Backward compat: pending condition var/op for legacy when var > val
  std::string pending_condition_var_;
  std::string pending_condition_op_;

  // SET action parsing: set #node.prop: value_expr
  bool expecting_set_hash_ = false;
  bool expecting_set_node_id_ = false;
  bool expecting_set_dot_ = false;
  bool expecting_set_prop_name_ = false;
  std::string pending_set_node_id_;
  std::string pending_set_property_;

  // PLAY parsing: play "anim" with { param: value }
  bool expecting_play_string_ = false;

  // WITH { ... } anim params block
  bool expecting_anim_params_block_ = false;
  bool in_anim_params_block_ = false;
  int anim_params_brace_depth_ = 0;
  bool expecting_anim_param_key_ = false;
  bool expecting_anim_param_value_ = false;
  std::string pending_anim_param_key_;

  // Import flag
  bool expecting_import_path_ = false;

  // Data and for loop flags
  bool expecting_data_name_ = false;
  bool expecting_data_item_key_ = false;
  bool expecting_for_iterator_ = false;
  bool expecting_for_source_ = false;
  bool expecting_dot_property_ = false;
  bool expecting_prop_key_suffix_ = false;
  std::string pending_prop_key_prefix_;

  // Pseudo-class block building
  bool in_pseudo_block_ = false;
  int pseudo_brace_depth_ = 0;
  std::string pending_pseudo_class_;
  std::string current_pseudo_class_;
  AstProps current_pseudo_props_;

  // Assets block building
  std::shared_ptr<AstAssets> current_assets_;
  int assets_brace_depth_ = 0;
  int asset_opts_brace_depth_ = 0;
  bool in_assets_block_ = false;
  bool in_asset_opts_ = false;
  bool expecting_assets_block_ = false;
  bool expecting_asset_id_ = false;
  bool expecting_asset_path_ = false;
  bool expecting_asset_opt_key_ = false;
  bool expecting_asset_opt_value_ = false;
  std::string pending_asset_type_;
  std::string pending_asset_id_;
  std::string pending_asset_path_;
  std::string pending_asset_opt_key_;

  // Const/var building
  bool expecting_const_name_ = false;
  bool expecting_const_value_ = false;
  bool pending_is_var_ = false;
  std::string pending_const_name_;
};

// ============================================================================
// Parsing
// ============================================================================

std::shared_ptr<AstProgram> parse(const char *source) {
  if (!source || source[0] == '\0') {
    g_last_error = "Empty source";
    g_last_error_line = 0;
    g_last_error_column = 0;
    return nullptr;
  }

  auto program = std::make_shared<AstProgram>();
  ParseContext ctx(program.get());
  TokenHistory history;
  ctx.token_history = &history;

  // Create AST builder
  AstBuilder builder(program.get());
  UiPropertySeparatorValidator ui_separator_validator;

  // Create lexer and parser
  LexerState *lexer = lexer_create(source);
  void *parser = ParseAlloc(malloc);

  // Feed tokens to parser
  Token tok;
  Token prev_tok{TOK_ERROR, "", 0, 0};

  do {
    tok = lex_next_token(lexer);

    if (tok.type == TOK_ERROR) {
      g_last_error = "Lexer error: invalid character '";
      g_last_error += tok.value;
      g_last_error += "'";
      g_last_error_line = tok.line;
      g_last_error_column = tok.column;
      ParseFree(parser, free);
      lexer_destroy(lexer);
      return nullptr;
    }

    if (!ui_separator_validator.accept(tok)) {
      g_last_error = "UI properties must be separated by commas";
      g_last_error_line = ui_separator_validator.error_line();
      g_last_error_column = ui_separator_validator.error_column();
      ParseFree(parser, free);
      lexer_destroy(lexer);
      return nullptr;
    }

    // Build AST based on token stream
    builder.on_token(tok, prev_tok.type != TOK_ERROR ? &prev_tok : nullptr);

    // Feed token to lemon for syntax validation
    Parse(parser, tok.type, 0, &ctx);

    if (!ctx.error_message.empty()) {
      g_last_error = ctx.error_message;
      g_last_error_line = tok.line;
      g_last_error_column = tok.column;
      ParseFree(parser, free);
      lexer_destroy(lexer);
      return nullptr;
    }

    history.push(tok);
    prev_tok = tok;

  } while (tok.type != TOK_EOF);

  // Cleanup
  ParseFree(parser, free);
  lexer_destroy(lexer);

  builder.finalize();

  return program;
}

// ============================================================================
// Error Handling
// ============================================================================

const char *get_error() { return g_last_error.c_str(); }

int get_error_line() { return g_last_error_line; }

int get_error_column() { return g_last_error_column; }

// ============================================================================
// AST to Runtime Conversion
// ============================================================================

} // namespace parser
} // namespace flex
