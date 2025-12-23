/*
 * Flex Engine - AST to Runtime Converter
 * Converts parsed AST objects to runtime objects
 */

#include "flex/flex.h"
#include "flex/flex_ast.h"
#include "flex/flex_parser.h"
#include <iostream>

namespace flex {

// ============================================================================
// AST to Runtime Converter
// ============================================================================

class AstToRuntimeConverter {
public:
    AstToRuntimeConverter(Definition::Impl* definition_impl);
    ~AstToRuntimeConverter() = default;

    // Convert AST program to runtime objects
    void convert(const parser::AstProgram& program);

private:
    Definition::Impl* impl_;

    // Helper methods
    void convert_machines(const std::vector<std::shared_ptr<parser::AstMachine>>& machines);
    void convert_animations(const std::vector<std::shared_ptr<parser::AstAnim>>& animations);

    // Machine conversion
    RuntimeStateMachine::Ptr convert_machine(const parser::AstMachine& machine);
    void convert_layer(RuntimeStateMachine* machine, const parser::AstLayer& layer);

    // Animation conversion
    RuntimeAnimation::Ptr convert_animation(const parser::AstAnim& anim);

    // Utility
    float parse_duration(const std::string& time_str);
};

inline AstToRuntimeConverter::AstToRuntimeConverter(Definition::Impl* definition_impl)
    : impl_(definition_impl) {}

inline void AstToRuntimeConverter::convert(const parser::AstProgram& program) {
    std::cout << "[Converter] Converting AST to runtime objects...\n";

    // Convert machines
    if (!program.machines.empty()) {
        convert_machines(program.machines);
    }

    // Convert animations
    if (!program.animations.empty()) {
        convert_animations(program.animations);
    }

    std::cout << "[Converter] Conversion complete!\n";
}

inline void AstToRuntimeConverter::convert_machines(
    const std::vector<std::shared_ptr<parser::AstMachine>>& machines) {
    std::cout << "[Converter] Converting " << machines.size() << " state machine(s)...\n";

    for (const auto& ast_machine : machines) {
        auto runtime_machine = convert_machine(*ast_machine);
        impl_->machines.push_back(runtime_machine);

        std::cout << "[Converter] Converted machine: " << runtime_machine->name() << "\n";

        // Convert layers
        for (const auto& ast_layer : ast_machine->layers) {
            convert_layer(runtime_machine.get(), ast_layer);
        }
    }
}

inline void AstToRuntimeConverter::convert_animations(
    const std::vector<std::shared_ptr<parser::AstAnim>>& animations) {
    std::cout << "[Converter] Converting " << animations.size() << " animation(s)...\n";

    impl_->animations.clear();

    for (const auto& ast_anim : animations) {
        auto runtime_anim = convert_animation(*ast_anim);
        impl_->animations[runtime_anim->name()] = runtime_anim;

        std::cout << "[Converter] Converted animation: " << runtime_anim->name() << "\n";
    }
}

inline RuntimeStateMachine::Ptr AstToRuntimeConverter::convert_machine(
    const parser::AstMachine& machine) {
    auto runtime_machine = std::make_shared<RuntimeStateMachine>(machine.name);

    // Layers will be added in convert_layer

    return runtime_machine;
}

inline void AstToRuntimeConverter::convert_layer(
    RuntimeStateMachine* machine, const parser::AstLayer& layer) {
    std::cout << "[Converter]   Converting layer: " << layer.name << "\n";

    machine->add_layer(layer.name);
    auto runtime_layer = machine->get_layer(layer.name);

    // Convert states
    for (const auto& ast_state : layer.states) {
        runtime_layer->add_state(ast_state.name, ast_state.initial, ast_state.animation);
        std::cout << "[Converter]     Added state: " << ast_state.name;
        if (ast_state.initial) std::cout << " (initial)";
        if (!ast_state.animation.empty()) std::cout << " animation=\"" << ast_state.animation << "\"";
        std::cout << "\n";
    }

    // Convert transitions
    for (const auto& ast_trans : layer.transitions) {
        runtime_layer->add_transition(
            ast_trans.from_state,
            ast_trans.to_state,
            ast_trans.condition_var,
            ast_trans.condition_op,
            ast_trans.condition_val
        );

        std::cout << "[Converter]     Added transition: " << ast_trans.from_state
                  << " -> " << ast_trans.to_state;

        if (!ast_trans.condition_var.empty()) {
            std::cout << " when " << ast_trans.condition_var
                      << " " << ast_trans.condition_op
                      << " " << ast_trans.condition_val;
        }
        std::cout << "\n";
    }
}

inline RuntimeAnimation::Ptr AstToRuntimeConverter::convert_animation(
    const parser::AstAnim& anim) {
    auto runtime_anim = std::make_shared<RuntimeAnimation>(anim.name);

    // Set properties
    runtime_anim->set_duration(anim.duration);
    runtime_anim->set_loop_mode(anim.loop_mode);

    // Convert tracks
    for (const auto& ast_track : anim.tracks) {
        auto runtime_track = std::make_unique<RuntimeTrack>(ast_track.property);

        // Add keyframes
        for (const auto& ast_kf : ast_track.keyframes) {
            runtime_track->add_keyframe(ast_kf.time, ast_kf.value);
        }

        runtime_anim->add_track(ast_track.property);

        // Copy keyframes
        auto* target_track = runtime_anim->get_track(ast_track.property);
        if (target_track) {
            target_track->keyframes() = std::move(runtime_track->keyframes());
        }
    }

    return runtime_anim;
}

inline float AstToRuntimeConverter::parse_duration(const std::string& time_str) {
    // Parse duration string like "0.5s", "100ms"
    float duration = 0.0f;
    size_t i = 0;

    // Parse number
    while (i < time_str.length() && (time_str[i] == '.' || (time_str[i] >= '0' && time_str[i] <= '9'))) {
        i++;
    }

    duration = std::stof(time_str.substr(0, i));

    // Parse unit
    if (i < time_str.length()) {
        std::string unit = time_str.substr(i);
        if (unit == "ms") {
            duration /= 1000.0f; // Convert to seconds
        }
    }

    return duration;
}

} // namespace flex
