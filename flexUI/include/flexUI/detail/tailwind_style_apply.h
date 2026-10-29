#pragma once

#include <flexUI/computed_style.h>
#include <flexUI/tailwind_native.h>

#include <cstdint>
#include <optional>

namespace flexUI::tailwind {

struct NativeStyleResult {
    // A CPU style candidate only: no backend admission or publication occurs.
    std::optional<ComputedStyle> style;
    // Union of registry impacts for this evaluation's matched writes. This is
    // not a diff against a previously published style. A future publisher must
    // compare snapshots, including when classes are removed or states exit.
    std::uint32_t impact = 0;
    NativeDiagnostic diagnostic;

    explicit operator bool() const noexcept { return style.has_value(); }
};

// Evaluate a per-element program in its compiler-defined precedence order.
// Always start with the supplied base/inherited style, never the last evaluated
// candidate. Neither base nor any previously returned result is modified.
NativeStyleResult apply_native_style(
    const NativeProgram& program,
    const ComputedStyle& base,
    const NativeState& state = {});

} // namespace flexUI::tailwind

namespace flexUI::detail {

struct NativeWriteResult {
    tailwind::NativeError error = tailwind::NativeError::None;
    std::uint32_t impact = 0;

    explicit operator bool() const noexcept {
        return error == tailwind::NativeError::None;
    }
};

// Internal bridge for the current native paint-profile IDs. Validate the actual
// stored type and finite value domain before using the canonical registry
// writer. On failure candidate is unchanged, with no reported write impact.
NativeWriteResult write_native_declaration(
    const tailwind::NativeDeclaration& declaration,
    ComputedStyle& candidate) noexcept;

} // namespace flexUI::detail
