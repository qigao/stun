# Native Tailwind paint profile: first M1 implementation slice

This implements a bounded, per-element compiler and a canonical property-registry
bridge for `stun-native-paint-v1`. It is the first executable slice of
[the native Tailwind/backend design](TAILWIND_NATIVE_BACKENDS_DESIGN.md), tracked
by [#17](https://github.com/qigao/stun/issues/17).

The new path neither generates nor parses CSS. The default Box/XML application
path has not yet migrated. This slice does not implement layout utilities,
complete Tailwind compatibility, backend effect admission, shader manifests or
GPU/terminal publication. Unsupported input never falls back to the existing
CSS-emitting compiler.

## Dependency and ownership boundary

`FlexUI::TailwindNative` is a C++17 static target with no linked JSON, CSS, Core,
CMeta, Flex runtime or renderer dependency. Its public headers use the existing
`flex::Color` value type and the same `StylePropertyId` enum as the property
registry. Moving that enum to `style_property_id.h` preserves its namespace,
underlying type and values.

The compiler accepts one element's complete class string and an explicitly
typed Theme. It returns a shared, immutable `NativeProgram` or one structured
diagnostic. The program owns its tokens and resolved colors, and never borrows
the input string, Theme, Element or property descriptors. It has no mutable
cache or execution VM. To share a program, share its `shared_ptr<const ...>`;
program objects cannot be copied, moved or assigned independently.

```cpp
#include <flexUI/tailwind_native.h>

flexUI::tailwind::NativeTheme theme;
theme.colors.push_back({"brand", flex::Color(0.2f, 0.4f, 0.6f, 0.8f)});
auto compiled = flexUI::tailwind::compile_native_class_list(
    "bg-brand/50 hover:opacity-37.5 disabled:opacity-25!", theme);
if (!compiled) {
    // Report diagnostic.code, source.offset/length and message.
}
```

The installed `TailwindNative` component is independently consumable with
`find_package(FlexUI REQUIRED COMPONENTS TailwindNative)` and
`FlexUI::TailwindNative`. It contains the compiler and its header closure. The
style bridge belongs to Core and is not bundled into this independent component.

## Exact reference and declared coverage

The reference is Tailwind **4.1.16**, immutable upstream commit
[`cbbbe844759a35f5bd2a976bb6e66e1f7104c574`](https://github.com/tailwindlabs/tailwindcss/tree/cbbbe844759a35f5bd2a976bb6e66e1f7104c574).
The source-level reference covers [candidate/rule ordering](https://github.com/tailwindlabs/tailwindcss/blob/cbbbe844759a35f5bd2a976bb6e66e1f7104c574/packages/tailwindcss/src/compile.ts),
[natural comparison](https://github.com/tailwindlabs/tailwindcss/blob/cbbbe844759a35f5bd2a976bb6e66e1f7104c574/packages/tailwindcss/src/utils/compare.ts),
[numeric validity](https://github.com/tailwindlabs/tailwindcss/blob/cbbbe844759a35f5bd2a976bb6e66e1f7104c574/packages/tailwindcss/src/utils/infer-data-type.ts),
and [variant definitions](https://github.com/tailwindlabs/tailwindcss/blob/cbbbe844759a35f5bd2a976bb6e66e1f7104c574/packages/tailwindcss/src/variants.ts).
Reserved utility spellings and longer roots follow the pinned
[utilities](https://github.com/tailwindlabs/tailwindcss/blob/cbbbe844759a35f5bd2a976bb6e66e1f7104c574/packages/tailwindcss/src/utilities.ts)
and [upstream compatibility registrations](https://github.com/tailwindlabs/tailwindcss/blob/cbbbe844759a35f5bd2a976bb6e66e1f7104c574/packages/tailwindcss/src/compat/legacy-utilities.ts);
reserving a spelling rejects its use in this slice and does not import a legacy
execution path.
This source pin and the explicit native tests are not a claim that the complete
upstream compiler/browser differential gate has run or that all M1 work is done.

| Input | Native output and supported domain |
| --- | --- |
| `opacity-N` | `Opacity` float; canonical quarter-percent steps between 0 and 100 |
| `opacity-[number]`, `opacity-[percent%]` | Finite decimal value in 0–1 or 0–100%; signs/exponents are parsed completely, then range-checked |
| `bg-name`, `text-name`, `border-name` | Typed Theme color to BackgroundColor, TextColor or BorderColor |
| `bg-[#hex]`, `text-[#hex]`, `border-[#hex]` | Strict 3/4/6/8-digit hexadecimal color; no permissive prefix conversion |
| Color `/N`, `/[number]`, `/[percent%]` | Multiply the color's existing alpha by the validated scalar; RGB remains straight-alpha sRGB |
| `bg-transparent`, `text-transparent`, `border-transparent` | Transparent black |
| `focus-within:`, `hover:`, `focus:`, `focus-visible:`, `active:`, `disabled:`, `dark:` | Conjunction of explicitly supplied state conditions |
| Trailing `!` | Important declaration priority |

Bare values such as `opacity-050`, `opacity-25.0` and `bg-brand/05` are rejected;
arbitrary values such as `opacity-[00.50]` use the arbitrary-value grammar.
Nonzero values that underflow the native float representation are rejected.
Decimal conversion uses the complete mantissa/exponent and a fixed classic
locale, independent of platform `long double` width or process decimal locale.
Unknown names do not become application CSS classes. Layout, font-size,
relationships, attributes, responsive/container queries, pseudo-elements,
expressions, inherited/current colors and leading `!` are outside this slice.
Names that collide with unsupported static utilities or longer utility roots
must not be reinterpreted as color-only utilities.
Their spellings stay reserved even with an alpha modifier in this conservative
profile. This restriction is per utility family: `text-center` is rejected,
while a Theme color named `center` can still be used as `border-center`.

Theme colors are finite straight-alpha sRGB channels in 0–1. Names start with an
ASCII letter, continue with letters/digits/underscore/hyphen, and cannot end in
underscore/hyphen. Duplicates and reserved `inherit`, `current`, `transparent`
names are invalid. The full upstream default palette is not embedded; application
colors, including named black/white if used, are supplied as typed Theme tokens.
Changing Theme data requires compiling a new candidate program; the previous
program retains its frozen values.

## State, precedence and application

`hover:` requires both `state.hover` and `state.hover_capable`. `dark:` consumes
the host's resolved effective theme, so explicit light/dark selection is not
overridden by an unexamined system preference. Duplicate variants are rejected
within this profile. The scanner respects bracket/parenthesis/quote structure
before splitting variants; an inner colon cannot become a new variant.

Rules are returned in increasing application priority: importance, supported
pseudo specificity, variant order, property partition and natural candidate
order. Class input order does not decide which declaration wins. A compound
`hover:focus:` rule has higher pseudo specificity than a single `disabled:` rule;
`dark:` adds no pseudo specificity. Numeric runs in candidate names compare as
numbers, with their spelling breaking numeric ties, matching the pinned source.

The Core bridge is `apply_native_style(program, base, state)` in
`detail/tailwind_style_apply.h`. It copies the explicit base/inherited style,
evaluates matching rules and writes through the existing property registry with
the CMeta descriptor of the value's actual variant storage. No second property
writer is introduced. The canonical uniform BorderColor writer updates all four
side colors together, matching uniform border-color behavior; stale per-side
colors cannot override the newly computed value.

Each evaluation starts from base, not from the previous evaluated result. This
is required when hover exits, classes disappear or an empty program restores
base. Failed writes return no candidate and do not modify base or previously
returned results. The reported `impact` is the union of matched property impacts,
not a full difference from the old published snapshot. A future publisher must
compare old/new snapshots, including property removal.

The result is only a **CPU style candidate**. In particular, computing an opacity
does not prove that a backend implements the required subtree compositing.
Effect admission, layout/hit geometry consistency, resource reservations and
atomic publication remain the M2–M3 work described in the main decision.

## Limits and diagnostics

All limits must be nonzero. Defaults are:

| Limit | Default |
| --- | --- |
| Raw source bytes | 64 KiB |
| Bytes per token | 512 |
| Tokens before deduplication | 4096 |
| Emitted rules | 4096 |
| Variant depth | 8 |
| Theme color tokens | 256 |
| Bytes per Theme name | 128 |
| Accounted retained program bytes | 1 MiB |

Retained accounting includes object/vector/string capacities, not allocator
bookkeeping. Temporary compilation work is additionally bounded by input,
token and Theme limits; there is no historical token cache. These are compiler
limits, not a substitute for the future backend's aggregate resource ledger.

Errors distinguish malformed syntax, unsupported utility/variant/value, invalid
numeric domains, invalid/missing Theme data, resource limits, type mismatch and
allocation failure. Candidate errors carry the class-string byte range; an XML
caller must map that range through its decoded-attribute source map. A failure
never returns a partially usable program.

## Verification and remaining migration

`test_tailwind_native` tests values, ordering, state conditions, frozen ownership,
negative syntax and every limit. `test_tailwind_native_style` compiles the real
bridge and property registry and checks actual ComputedStyle values, dirty
impact, base restoration, canonical types and failure behavior.
`test_tailwindcss_module` continues to check the existing unmigrated module.

All three are registered in the existing XML runtime qualification and Win/mac
headless test selection. Existing cache, runner and sanitizer policies are reused.
Independent component install/consumer checks verify that native compilation
does not acquire JSON, CSS or Core linkage. Local checks and exact-HEAD CI are
separate evidence; backend or application qualification is not implied.

Remaining M1 work includes complete typed Theme/Preset support, layout and deferred
values, broader variants, animation and the executable differential profile.
Box/XML/Widget/ChartFrame migration and CSS deletion remain M4. No production
entry point is redirected until its required behavior and backend contracts are
implemented and verified.
