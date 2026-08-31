#include "flexUI/controller_turboscript.h"

#include <turbo_script.h>

#include <array>
#include <cmath>
#include <exception>
#include <initializer_list>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace flexUI {
namespace {

turbo_script_string_view_t string_view(std::string_view value) noexcept {
  return {value.data(), value.size()};
}

turbo_script_value_view_t integer_value(std::int64_t value) noexcept {
  turbo_script_value_view_t result{};
  result.kind = TURBO_SCRIPT_VALUE_INT64;
  result.as.integer = value;
  return result;
}

turbo_script_value_view_t number_value(double value) noexcept {
  turbo_script_value_view_t result{};
  result.kind = TURBO_SCRIPT_VALUE_NUMBER;
  result.as.number = value;
  return result;
}

turbo_script_value_view_t bool_value(bool value) noexcept {
  turbo_script_value_view_t result{};
  result.kind = TURBO_SCRIPT_VALUE_BOOL;
  result.as.boolean = value ? 1 : 0;
  return result;
}

turbo_script_value_view_t text_value(std::string_view value) noexcept {
  turbo_script_value_view_t result{};
  result.kind = TURBO_SCRIPT_VALUE_STRING;
  result.as.string = string_view(value);
  return result;
}

turbo_script_record_entry_view_t record_entry(std::string_view key,
                                              turbo_script_value_view_t value) noexcept {
  return {string_view(key), value};
}

template <typename T> void override_if_nonzero(T &target, T requested) noexcept {
  if (requested != 0) {
    target = requested;
  }
}

const char *event_kind_name(UiEventKind event) noexcept {
  switch (event) {
  case UiEventKind::Click:
    return "click";
  case UiEventKind::MouseMove:
    return "mouse_move";
  case UiEventKind::MouseDown:
    return "mouse_down";
  case UiEventKind::MouseUp:
    return "mouse_up";
  case UiEventKind::MouseWheel:
    return "mouse_wheel";
  case UiEventKind::KeyDown:
    return "key_down";
  case UiEventKind::KeyUp:
    return "key_up";
  case UiEventKind::TextInput:
    return "text_input";
  case UiEventKind::FocusIn:
    return "focus_in";
  case UiEventKind::FocusOut:
    return "focus_out";
  case UiEventKind::CompositionStart:
    return "composition_start";
  case UiEventKind::CompositionUpdate:
    return "composition_update";
  case UiEventKind::CompositionEnd:
    return "composition_end";
  }
  return nullptr;
}

ScriptModuleErrorCode map_status(turbo_script_status_t status) noexcept {
  switch (status) {
  case TURBO_SCRIPT_STATUS_OK:
    return ScriptModuleErrorCode::None;
  case TURBO_SCRIPT_STATUS_PARSE_ERROR:
  case TURBO_SCRIPT_STATUS_VALIDATION_ERROR:
    return ScriptModuleErrorCode::CompileFailure;
  case TURBO_SCRIPT_STATUS_NOT_FOUND:
  case TURBO_SCRIPT_STATUS_INVALID_EXPORT_HANDLE:
    return ScriptModuleErrorCode::InvalidExport;
  case TURBO_SCRIPT_STATUS_LIMIT_EXCEEDED:
  case TURBO_SCRIPT_STATUS_OUT_OF_MEMORY:
    return ScriptModuleErrorCode::ResourceLimitExceeded;
  case TURBO_SCRIPT_STATUS_INTERRUPTED:
    return ScriptModuleErrorCode::Interrupted;
  default:
    return ScriptModuleErrorCode::RuntimeFailure;
  }
}

std::string copy_error_message(turbo_script_result_t *result, turbo_script_status_t status,
                               std::string_view operation) {
  turbo_script_error_info_t info{};
  info.struct_size = sizeof(info);
  if (result != nullptr && turbo_script_result_get_error(result, &info) == TURBO_SCRIPT_STATUS_OK &&
      info.message.data != nullptr) {
    return std::string(info.message.data, info.message.size);
  }
  return std::string(operation) + " failed with TurboScript status " + std::to_string(status);
}

ScriptModuleError make_error(turbo_script_result_t *result, turbo_script_status_t status,
                             std::string_view operation) {
  return {map_status(status), copy_error_message(result, status, operation)};
}

ScriptModuleError invalid_effect(std::string message) {
  return {ScriptModuleErrorCode::RuntimeFailure, std::move(message)};
}

ScriptModuleError validate_fields(const turbo_script_value_view_t &record,
                                  std::initializer_list<std::string_view> allowed,
                                  std::string_view subject) {
  for (std::size_t index = 0; index < record.as.record.count; ++index) {
    const auto &key = record.as.record.entries[index].key;
    if (key.data == nullptr && key.size != 0) {
      return invalid_effect("TurboScript " + std::string(subject) + " has an invalid field name");
    }
    const std::string_view name =
        key.size == 0 ? std::string_view{} : std::string_view(key.data, key.size);
    bool known = false;
    for (const auto candidate : allowed) {
      if (name == candidate) {
        known = true;
        break;
      }
    }
    if (!known) {
      return invalid_effect("unknown TurboScript " + std::string(subject) + " field '" +
                            std::string(name) + "'");
    }
  }
  return {};
}

const turbo_script_value_view_t *find_field(const turbo_script_value_view_t &record,
                                            std::string_view name) noexcept {
  if (record.kind != TURBO_SCRIPT_VALUE_RECORD) {
    return nullptr;
  }
  for (std::size_t index = 0; index < record.as.record.count; ++index) {
    const auto &entry = record.as.record.entries[index];
    if (entry.key.size == name.size() && entry.key.data != nullptr &&
        std::string_view(entry.key.data, entry.key.size) == name) {
      return &entry.value;
    }
  }
  return nullptr;
}

bool required_string(const turbo_script_value_view_t &record, std::string_view field,
                     std::string &output, ScriptModuleError &error) {
  const auto *value = find_field(record, field);
  if (value == nullptr || value->kind != TURBO_SCRIPT_VALUE_STRING) {
    error =
        invalid_effect("TurboScript effect field '" + std::string(field) + "' must be a string");
    return false;
  }
  if (value->as.string.size == 0) {
    output.clear();
    return true;
  }
  if (value->as.string.data == nullptr) {
    error = invalid_effect("TurboScript effect field '" + std::string(field) +
                           "' contains an invalid string view");
    return false;
  }
  output.assign(value->as.string.data, value->as.string.size);
  return true;
}

bool required_bool(const turbo_script_value_view_t &record, std::string_view field, bool &output,
                   ScriptModuleError &error) {
  const auto *value = find_field(record, field);
  if (value == nullptr || value->kind != TURBO_SCRIPT_VALUE_BOOL) {
    error =
        invalid_effect("TurboScript effect field '" + std::string(field) + "' must be a boolean");
    return false;
  }
  output = value->as.boolean != 0;
  return true;
}

bool required_number(const turbo_script_value_view_t &record, std::string_view field,
                     double &output, ScriptModuleError &error) {
  const auto *value = find_field(record, field);
  if (value == nullptr ||
      (value->kind != TURBO_SCRIPT_VALUE_NUMBER && value->kind != TURBO_SCRIPT_VALUE_INT64)) {
    error =
        invalid_effect("TurboScript effect field '" + std::string(field) + "' must be a number");
    return false;
  }
  output = value->kind == TURBO_SCRIPT_VALUE_NUMBER ? value->as.number
                                                    : static_cast<double>(value->as.integer);
  if (!std::isfinite(output)) {
    error = invalid_effect("TurboScript effect field '" + std::string(field) + "' must be finite");
    return false;
  }
  return true;
}

bool required_positive_integer(const turbo_script_value_view_t &record, std::string_view field,
                               std::uint64_t &output, ScriptModuleError &error) {
  const auto *value = find_field(record, field);
  if (value == nullptr || value->kind != TURBO_SCRIPT_VALUE_INT64 || value->as.integer <= 0) {
    error = invalid_effect("TurboScript effect field '" + std::string(field) +
                           "' must be a positive int64");
    return false;
  }
  output = static_cast<std::uint64_t>(value->as.integer);
  return true;
}

bool required_target(const turbo_script_value_view_t &record, UiHandle &target,
                     ScriptModuleError &error) {
  const auto *value = find_field(record, "target");
  if (value == nullptr || value->kind != TURBO_SCRIPT_VALUE_RECORD) {
    error = invalid_effect("TurboScript mutation field 'target' must be a record");
    return false;
  }
  if (!required_string(*value, "id", target.id, error) ||
      !required_positive_integer(*value, "generation", target.generation, error)) {
    return false;
  }
  if (!target) {
    error = invalid_effect("TurboScript mutation target must be valid");
    return false;
  }
  return true;
}

ScriptModuleError append_mutation(const turbo_script_value_view_t &encoded,
                                  UiMutationBatch &batch) {
  if (encoded.kind != TURBO_SCRIPT_VALUE_RECORD) {
    return invalid_effect("TurboScript mutation entry must be a record");
  }
  std::string type;
  ScriptModuleError error;
  if (!required_string(encoded, "type", type, error)) {
    return error;
  }

  UiMutation mutation;
  if (type == "set_text") {
    if (auto field_error =
            validate_fields(encoded, {"type", "target", "text"}, "set_text mutation");
        field_error) {
      return field_error;
    }
    SetTextMutation value;
    if (!required_target(encoded, value.target, error) ||
        !required_string(encoded, "text", value.text, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "set_attribute") {
    if (auto field_error =
            validate_fields(encoded, {"type", "target", "name", "value"}, "set_attribute mutation");
        field_error) {
      return field_error;
    }
    SetAttributeMutation value;
    if (!required_target(encoded, value.target, error) ||
        !required_string(encoded, "name", value.name, error) ||
        !required_string(encoded, "value", value.value, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "remove_attribute") {
    if (auto field_error =
            validate_fields(encoded, {"type", "target", "name"}, "remove_attribute mutation");
        field_error) {
      return field_error;
    }
    RemoveAttributeMutation value;
    if (!required_target(encoded, value.target, error) ||
        !required_string(encoded, "name", value.name, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "set_classes") {
    if (auto field_error =
            validate_fields(encoded, {"type", "target", "classes"}, "set_classes mutation");
        field_error) {
      return field_error;
    }
    SetClassesMutation value;
    if (!required_target(encoded, value.target, error) ||
        !required_string(encoded, "classes", value.classes, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "set_utilities") {
    if (auto field_error =
            validate_fields(encoded, {"type", "target", "utilities"}, "set_utilities mutation");
        field_error) {
      return field_error;
    }
    SetUtilitiesMutation value;
    if (!required_target(encoded, value.target, error) ||
        !required_string(encoded, "utilities", value.utilities, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "set_binding_number") {
    if (auto field_error =
            validate_fields(encoded, {"type", "name", "value"}, "set_binding_number mutation");
        field_error) {
      return field_error;
    }
    SetBindingInputNumberMutation value;
    if (!required_string(encoded, "name", value.name, error) ||
        !required_number(encoded, "value", value.value, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "set_binding_bool") {
    if (auto field_error =
            validate_fields(encoded, {"type", "name", "value"}, "set_binding_bool mutation");
        field_error) {
      return field_error;
    }
    SetBindingInputBoolMutation value;
    if (!required_string(encoded, "name", value.name, error) ||
        !required_bool(encoded, "value", value.value, error)) {
      return error;
    }
    mutation = std::move(value);
  } else if (type == "set_binding_string") {
    if (auto field_error =
            validate_fields(encoded, {"type", "name", "value"}, "set_binding_string mutation");
        field_error) {
      return field_error;
    }
    SetBindingInputStringMutation value;
    if (!required_string(encoded, "name", value.name, error) ||
        !required_string(encoded, "value", value.value, error)) {
      return error;
    }
    mutation = std::move(value);
  } else {
    return invalid_effect("unknown TurboScript mutation type '" + type + "'");
  }

  const auto appended = batch.append(std::move(mutation));
  if (!appended) {
    return {ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message};
  }
  return {};
}

ScriptModuleError append_command(const turbo_script_value_view_t &encoded,
                                 ApplicationCommandBatch &batch) {
  if (encoded.kind != TURBO_SCRIPT_VALUE_RECORD) {
    return invalid_effect("TurboScript command entry must be a record");
  }
  if (auto error =
          validate_fields(encoded, {"request_id", "capability", "operation", "payload"}, "command");
      error) {
    return error;
  }
  ApplicationCommand command;
  ScriptModuleError error;
  if (!required_positive_integer(encoded, "request_id", command.request_id, error) ||
      !required_string(encoded, "capability", command.capability, error) ||
      !required_string(encoded, "operation", command.operation, error) ||
      !required_string(encoded, "payload", command.payload, error)) {
    return error;
  }
  const auto appended = batch.append(std::move(command));
  if (!appended) {
    return {ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message};
  }
  return {};
}

ScriptModuleError decode_effects(const turbo_script_value_view_t &value, ScriptCallResult &output) {
  if (value.kind == TURBO_SCRIPT_VALUE_NULL) {
    return {};
  }
  if (value.kind != TURBO_SCRIPT_VALUE_RECORD) {
    return invalid_effect("TurboScript callback result must be null or an effect record");
  }
  if (auto error = validate_fields(value, {"mutations", "commands"}, "effect"); error) {
    return error;
  }
  if (const auto *mutations = find_field(value, "mutations"); mutations != nullptr) {
    if (mutations->kind != TURBO_SCRIPT_VALUE_ARRAY) {
      return invalid_effect("TurboScript effect field 'mutations' must be an array");
    }
    for (std::size_t index = 0; index < mutations->as.array.count; ++index) {
      if (auto error = append_mutation(mutations->as.array.items[index], output.mutations); error) {
        error.message = "mutation[" + std::to_string(index) + "]: " + std::move(error.message);
        return error;
      }
    }
  }
  if (const auto *commands = find_field(value, "commands"); commands != nullptr) {
    if (commands->kind != TURBO_SCRIPT_VALUE_ARRAY) {
      return invalid_effect("TurboScript effect field 'commands' must be an array");
    }
    for (std::size_t index = 0; index < commands->as.array.count; ++index) {
      if (auto error = append_command(commands->as.array.items[index], output.commands); error) {
        error.message = "command[" + std::to_string(index) + "]: " + std::move(error.message);
        return error;
      }
    }
  }
  return {};
}

bool deny_native_plugin(const char *, void *) { return false; }

int interrupt_adapter(void *user_data) noexcept {
  auto *options = static_cast<TurboScriptControllerOptions *>(user_data);
  if (options == nullptr || options->interrupt == nullptr) {
    return 0;
  }
  try {
    return options->interrupt(options->interrupt_user_data) ? 1 : 0;
  } catch (...) {
    return 1;
  }
}

struct ExportMetadata {
  std::uint32_t min_arity = 0;
  std::uint32_t max_arity = 0;
};

class TurboScriptModule final : public IScriptModule {
public:
  TurboScriptModule(turbo_script_ctx_t *context, turbo_script_result_t *result,
                    turbo_script_module_t *module, turbo_script_instance_t *instance,
                    TurboScriptControllerOptions options)
      : context_(context), result_(result), module_(module), instance_(instance), options_(options),
        owner_thread_(std::this_thread::get_id()) {
    turbo_script_call_options_init(&call_options_);
    override_if_nonzero(call_options_.max_recursion, options_.call_limits.max_recursion);
    override_if_nonzero(call_options_.max_steps, options_.call_limits.max_steps);
    override_if_nonzero(call_options_.max_loop_iterations,
                        options_.call_limits.max_loop_iterations);
    override_if_nonzero(call_options_.max_host_callbacks, options_.call_limits.max_host_callbacks);
    override_if_nonzero(call_options_.max_result_bytes, options_.call_limits.max_result_bytes);
    if (options_.interrupt != nullptr) {
      call_options_.interrupt = interrupt_adapter;
      call_options_.interrupt_user_data = &options_;
    }
  }

  ~TurboScriptModule() override {
    if (std::this_thread::get_id() != owner_thread_) {
      std::terminate();
    }
    close_or_terminate();
  }

  ScriptResolveResult resolve_export(std::string_view name, ScriptCallbackKind callback) override {
    if (const auto found = export_cache_.find(std::string(name)); found != export_cache_.end()) {
      if (!arity_matches(found->second, callback)) {
        return {{},
                {ScriptModuleErrorCode::InvalidExport,
                 "TurboScript export arity does not match its FlexUI callback"}};
      }
      return {found->second, {}};
    }
    if (std::this_thread::get_id() != owner_thread_) {
      return {{},
              {ScriptModuleErrorCode::RuntimeFailure,
               "TurboScript module accessed from a non-owner thread"}};
    }
    turbo_script_result_reset(result_);
    turbo_script_export_handle_t native_handle = 0;
    const auto status =
        turbo_script_instance_resolve_export(instance_, string_view(name), result_, &native_handle);
    if (status == TURBO_SCRIPT_STATUS_NOT_FOUND) {
      return {};
    }
    if (status != TURBO_SCRIPT_STATUS_OK) {
      return {{}, make_error(result_, status, "resolve export")};
    }
    if (native_handle == 0) {
      return {{},
              {ScriptModuleErrorCode::InvalidExport, "TurboScript returned a zero export handle"}};
    }

    turbo_script_export_info_t info{};
    info.struct_size = sizeof(info);
    const auto info_status =
        turbo_script_instance_get_export_info(instance_, native_handle, result_, &info);
    if (info_status != TURBO_SCRIPT_STATUS_OK) {
      return {{}, make_error(result_, info_status, "read export metadata")};
    }

    const ScriptExportHandle handle{native_handle};
    export_cache_.emplace(name, handle);
    export_metadata_.emplace(handle.value, ExportMetadata{info.min_arity, info.max_arity});
    if (!arity_matches(handle, callback)) {
      return {{},
              {ScriptModuleErrorCode::InvalidExport,
               "TurboScript export arity does not match its FlexUI callback"}};
    }
    return {handle, {}};
  }

  ScriptCallResult call(ScriptExportHandle handle, const ScriptCallContext &context) override {
    if (std::this_thread::get_id() != owner_thread_) {
      return {{ScriptModuleErrorCode::RuntimeFailure,
               "TurboScript module accessed from a non-owner thread"}};
    }
    const auto metadata = export_metadata_.find(handle.value);
    if (!handle || metadata == export_metadata_.end()) {
      return {{ScriptModuleErrorCode::InvalidExport, "unknown TurboScript export handle"}};
    }

    std::array<turbo_script_value_view_t, 1> arguments{};
    std::array<turbo_script_record_entry_view_t, 2> target_entries{};
    std::array<turbo_script_record_entry_view_t, 2> current_target_entries{};
    std::array<turbo_script_record_entry_view_t, 17> event_entries{};
    std::size_t argument_count = 0;
    ScriptModuleError argument_error;
    if (!make_arguments(context, arguments, argument_count, target_entries, current_target_entries,
                        event_entries, argument_error)) {
      return {std::move(argument_error)};
    }
    if (argument_count < metadata->second.min_arity ||
        argument_count > metadata->second.max_arity) {
      return {{ScriptModuleErrorCode::InvalidExport,
               "TurboScript export arity does not match its FlexUI callback"}};
    }

    turbo_script_result_reset(result_);
    const auto status = turbo_script_instance_call(instance_, handle.value, arguments.data(),
                                                   argument_count, &call_options_, result_);
    if (status != TURBO_SCRIPT_STATUS_OK) {
      return {make_error(result_, status, "call export")};
    }
    turbo_script_value_view_t returned{};
    const auto value_status = turbo_script_result_get_value(result_, &returned);
    if (value_status != TURBO_SCRIPT_STATUS_OK) {
      return {make_error(result_, value_status, "read callback result")};
    }
    ScriptCallResult output;
    if (auto error = decode_effects(returned, output); error) {
      return {std::move(error)};
    }
    return output;
  }

private:
  bool arity_matches(ScriptExportHandle handle, ScriptCallbackKind callback) const noexcept {
    const auto found = export_metadata_.find(handle.value);
    if (found == export_metadata_.end()) {
      return false;
    }
    const std::uint32_t expected =
        callback == ScriptCallbackKind::Mount || callback == ScriptCallbackKind::Unmount ? 0 : 1;
    return expected >= found->second.min_arity && expected <= found->second.max_arity;
  }

  static bool make_arguments(
      const ScriptCallContext &context, std::array<turbo_script_value_view_t, 1> &arguments,
      std::size_t &argument_count, std::array<turbo_script_record_entry_view_t, 2> &target_entries,
      std::array<turbo_script_record_entry_view_t, 2> &current_target_entries,
      std::array<turbo_script_record_entry_view_t, 17> &event_entries, ScriptModuleError &error) {
    switch (context.callback) {
    case ScriptCallbackKind::Mount:
    case ScriptCallbackKind::Unmount:
      argument_count = 0;
      return true;
    case ScriptCallbackKind::Frame:
      arguments[0] = number_value(context.delta_seconds);
      argument_count = 1;
      return true;
    case ScriptCallbackKind::Event:
      break;
    }
    if (context.event == nullptr) {
      error = {ScriptModuleErrorCode::RuntimeFailure,
               "FlexUI event callback is missing its event snapshot"};
      return false;
    }
    const char *kind_name = event_kind_name(context.event->event);
    if (kind_name == nullptr) {
      error = {ScriptModuleErrorCode::RuntimeFailure,
               "FlexUI event callback has an invalid event kind"};
      return false;
    }
    const UiHandle &current_target =
        context.event->current_target ? context.event->current_target : context.event->target;
    if (context.event->target.generation >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        current_target.generation >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      error = {ScriptModuleErrorCode::ResourceLimitExceeded,
               "FlexUI event target generation exceeds TurboScript int64 range"};
      return false;
    }

    target_entries = {record_entry("id", text_value(context.event->target.id)),
                      record_entry("generation", integer_value(static_cast<std::int64_t>(
                                                     context.event->target.generation)))};
    turbo_script_value_view_t target{};
    target.kind = TURBO_SCRIPT_VALUE_RECORD;
    target.as.record = {target_entries.data(), target_entries.size()};

    current_target_entries = {record_entry("id", text_value(current_target.id)),
                              record_entry("generation", integer_value(static_cast<std::int64_t>(
                                                             current_target.generation)))};
    turbo_script_value_view_t current_target_value{};
    current_target_value.kind = TURBO_SCRIPT_VALUE_RECORD;
    current_target_value.as.record = {current_target_entries.data(), current_target_entries.size()};

    event_entries = {
        record_entry("kind", text_value(kind_name)),
        record_entry("target", target),
        record_entry("current_target", current_target_value),
        record_entry("x", number_value(context.event->x)),
        record_entry("y", number_value(context.event->y)),
        record_entry("delta_x", number_value(context.event->delta_x)),
        record_entry("delta_y", number_value(context.event->delta_y)),
        record_entry("button", integer_value(context.event->button)),
        record_entry("key", integer_value(context.event->key)),
        record_entry("modifiers", integer_value(context.event->modifiers)),
        record_entry("text", text_value(context.event->text)),
        record_entry("composition_text", text_value(context.event->composition_text)),
        record_entry("timestamp_ms", number_value(context.event->timestamp_ms)),
        record_entry("handled", bool_value(context.event->handled)),
        record_entry("propagate", bool_value(context.event->propagate)),
        record_entry("target_valid", bool_value(static_cast<bool>(context.event->target))),
        record_entry("current_target_valid", bool_value(static_cast<bool>(current_target)))};
    arguments[0].kind = TURBO_SCRIPT_VALUE_RECORD;
    arguments[0].as.record = {event_entries.data(), event_entries.size()};
    argument_count = 1;
    return true;
  }

  void close_or_terminate() noexcept {
    if (instance_ != nullptr) {
      turbo_script_result_reset(result_);
      if (turbo_script_instance_destroy(instance_, result_) != TURBO_SCRIPT_STATUS_OK) {
        std::terminate();
      }
      instance_ = nullptr;
    }
    if (module_ != nullptr) {
      turbo_script_result_reset(result_);
      if (turbo_script_module_destroy(module_, result_) != TURBO_SCRIPT_STATUS_OK) {
        std::terminate();
      }
      module_ = nullptr;
    }
    if (result_ != nullptr) {
      turbo_script_result_destroy(result_);
      result_ = nullptr;
    }
    if (context_ != nullptr) {
      turbo_script_free(context_);
      context_ = nullptr;
    }
  }

  turbo_script_ctx_t *context_ = nullptr;
  turbo_script_result_t *result_ = nullptr;
  turbo_script_module_t *module_ = nullptr;
  turbo_script_instance_t *instance_ = nullptr;
  TurboScriptControllerOptions options_;
  turbo_script_call_options_t call_options_{};
  std::thread::id owner_thread_;
  std::unordered_map<std::string, ScriptExportHandle> export_cache_;
  std::unordered_map<std::uint64_t, ExportMetadata> export_metadata_;
};

void destroy_partial_or_terminate(turbo_script_ctx_t *context, turbo_script_result_t *result,
                                  turbo_script_module_t *module,
                                  turbo_script_instance_t *instance) noexcept {
  if (instance != nullptr) {
    turbo_script_result_reset(result);
    if (turbo_script_instance_destroy(instance, result) != TURBO_SCRIPT_STATUS_OK) {
      std::terminate();
    }
  }
  if (module != nullptr) {
    turbo_script_result_reset(result);
    if (turbo_script_module_destroy(module, result) != TURBO_SCRIPT_STATUS_OK) {
      std::terminate();
    }
  }
  if (result != nullptr) {
    turbo_script_result_destroy(result);
  }
  if (context != nullptr) {
    turbo_script_free(context);
  }
}

} // namespace

TurboScriptModuleCreateResult
create_turboscript_controller_module(std::string_view source, std::string_view module_name,
                                     TurboScriptControllerOptions options) {
  turbo_script_ctx_t *context = nullptr;
  turbo_script_result_t *result = nullptr;
  turbo_script_module_t *module = nullptr;
  turbo_script_instance_t *instance = nullptr;
  try {
    context = turbo_script_init_with_plugin_authorizer(TURBO_SCRIPT_INIT_DEFAULT,
                                                       deny_native_plugin, nullptr);
    if (context == nullptr) {
      return {
          {},
          {ScriptModuleErrorCode::ResourceLimitExceeded, "unable to create TurboScript context"}};
    }
    auto status = turbo_script_result_create(context, &result);
    if (status != TURBO_SCRIPT_STATUS_OK) {
      destroy_partial_or_terminate(context, result, module, instance);
      return {{}, {map_status(status), "unable to create TurboScript result storage"}};
    }

    turbo_script_module_options_t module_options{};
    turbo_script_module_options_init(&module_options);
    module_options.module_name = string_view(module_name);
    override_if_nonzero(module_options.max_source_bytes, options.compile_limits.max_source_bytes);
    override_if_nonzero(module_options.max_ast_nodes, options.compile_limits.max_ast_nodes);
    override_if_nonzero(module_options.max_imports, options.compile_limits.max_imports);
    override_if_nonzero(module_options.max_exports, options.compile_limits.max_exports);
    override_if_nonzero(module_options.max_string_bytes, options.compile_limits.max_string_bytes);
    status =
        turbo_script_module_compile(context, string_view(source), &module_options, result, &module);
    if (status != TURBO_SCRIPT_STATUS_OK) {
      auto error = make_error(result, status, "compile module");
      destroy_partial_or_terminate(context, result, module, instance);
      return {{}, std::move(error)};
    }

    turbo_script_instance_options_t instance_options{};
    turbo_script_instance_options_init(&instance_options);
    instance_options.mode = options.execution_mode == TurboScriptExecutionMode::Jit
                                ? TURBO_SCRIPT_EXEC_JIT
                                : TURBO_SCRIPT_EXEC_INTERPRETER;
    override_if_nonzero(instance_options.max_retained_bytes,
                        options.instance_limits.max_retained_bytes);
    override_if_nonzero(instance_options.max_stack_bytes, options.instance_limits.max_stack_bytes);
    override_if_nonzero(instance_options.max_recursion, options.instance_limits.max_recursion);
    override_if_nonzero(instance_options.max_globals, options.instance_limits.max_globals);
    override_if_nonzero(instance_options.max_value_depth, options.instance_limits.max_value_depth);
    override_if_nonzero(instance_options.max_value_nodes, options.instance_limits.max_value_nodes);
    override_if_nonzero(instance_options.max_result_bytes,
                        options.instance_limits.max_result_bytes);
    status = turbo_script_instance_create(module, &instance_options, result, &instance);
    if (status != TURBO_SCRIPT_STATUS_OK) {
      auto error = make_error(result, status, "create module instance");
      destroy_partial_or_terminate(context, result, module, instance);
      return {{}, std::move(error)};
    }

    return {std::make_unique<TurboScriptModule>(context, result, module, instance, options), {}};
  } catch (const std::exception &exception) {
    destroy_partial_or_terminate(context, result, module, instance);
    return {{},
            {ScriptModuleErrorCode::ResourceLimitExceeded,
             std::string("TurboScript adapter allocation failed: ") + exception.what()}};
  } catch (...) {
    destroy_partial_or_terminate(context, result, module, instance);
    return {{},
            {ScriptModuleErrorCode::RuntimeFailure,
             "TurboScript adapter failed with an unknown exception"}};
  }
}

} // namespace flexUI
