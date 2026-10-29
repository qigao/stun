# Backend effect contract validation and resource reservations

Status: the first CPU foundation for M2 of [#17](https://github.com/qigao/stun/issues/17).
The canonical target remains [native Tailwind and backend contracts](../../flexUI/docs/TAILWIND_NATIVE_BACKENDS_DESIGN.md).
This implementation belongs to `Flex::Render`, independently of Tailwind parsing,
FlexUI, CMeta, CSS, the scene runtime and any graphics API SDK.

## What the implementation proves

`flex/render/effect_admission.h` provides two separate operations:

```cpp
auto checked = flex::effects::validate_effect_contract(
    requirement, selected_manifest, capabilities);
if (!checked) {
    // Report checked.diagnostic; the previously committed UI is untouched.
    return;
}

auto prepared = flex::effects::prepare_effect_admission(
    *checked.contract, shared_budget, current_capabilities);
if (!prepared) {
    // Report prepared.diagnostic; no budget was partially reserved.
    return;
}

// Only CPU validation and a budget lease exist here. The backend owner still
// has to create native resources from checked.contract->manifest(), retain the
// lease through cleanup/retirement, and perform final revision checks before
// it can publish a RenderPlan with the corresponding UI/geometry snapshot.
```

The first operation checks one explicitly selected implementation against the
effect requirement, qualified **adapter** records, separate **device** records
and trusted artifact-loader observations. It returns an immutable object owning
the complete checked requirement and manifest. It does not borrow input strings,
vectors, capabilities, Element pointers, device handles or mutable style state.

The second operation rechecks the resource owner/domain, device generation,
capability revision, backend and target identity, then atomically reserves all
budget dimensions on a shared ledger. It creates no native resource and does not
compile a shader, execute a pass, produce a RenderPlan or publish UI state.

There is no CSS input or fallback. Neither operation selects another variant,
tries another provider, changes backend, drops an effect or reduces its quality.
The existing `RendererCapabilities` booleans cannot be promoted into qualified
records. **No production renderer is registered or qualified by this slice.**

## Semantic identity and target contracts

`EffectIdentity` consists of an ID, a semantic version and a contract digest. All
three match exactly; version `0.0.0`, empty identifiers and zero digests are
uninitialized values in this profile. Different input and composition semantics
are also checked explicitly: a backdrop request cannot bind to a content blur,
and subtree isolation cannot silently become per-primitive alpha.

`TargetProfile` fixes its kind, x/y units and a host-defined metrics/unit-policy
digest. NativePixel uses logical pixels on both axes or device pixels on both
axes. TerminalCell uses columns and rows. No implicit DPI conversion, pixel-to-cell
rounding, palette downgrade or target substitution is performed.

The target for Tango must be TerminalCell; the other API families require
NativePixel. Parameter ranges carry their own units and binding classification
(uniform, specialization or structural). Cell units cannot enter a pixel target,
and pixel units cannot enter a cell target. Other unit or binding differences
are rejected rather than converted.

Every scalar parameter range is finite, has a unique nonzero ID and satisfies
`lower <= upper`. Requirement IDs must exactly cover the manifest schema, and
the manifest must cover the qualified adapter schema. Each reachable requirement
range must fit the manifest, and each manifest range must fit adapter support.
There are no omitted-parameter defaults or string expressions in this contract.
In one selected manifest, structural and specialization parameters must each be
a single value. Their qualified adapter domain may be wider, but changing such
a value requires a new checked manifest and owner preparation. A dynamic range
cannot be passed off as an ordinary uniform update.

The caller supplies the full reachable parameter domain, including relevant
states and animation overshoot. It also supplies nonzero `expanded_width` and
`expanded_height`: the maximum output extent after transform, DPI and effect
expansion, in physical pixels or cells. The one output resource must contain
that extent. This slice checks supplied bounds; it does not implement the
Tailwind/state/animation range extractor or prove the caller's range analysis.

## Per-API programs

| API | Artifact contract | Pass contract accepted by this profile |
| --- | --- | --- |
| Direct2D | Registered native binding or D2D-compatible bytecode | NativeBuiltin, or D2DPixelEffect with exactly one fragment artifact and native resource accesses |
| gCanvas desktop OpenGL | Desktop GLSL source | Graphics with one vertex and one fragment artifact; compute only with an explicit adapter and device stage/profile tuple and execution limits |
| gCanvas OpenGL ES | ESSL source | Same separation of graphics/compute, independently qualified ESSL profile |
| gCanvas Vulkan | SPIR-V | Independently qualified graphics or compute modules and execution limits |
| Tango | Registered native binding | NativeBuiltin only; no vertex, fragment or compute shader |

A native binding may also be used by a pixel backend if that exact native
operation is independently declared by both adapter and device records. Its
artifact has `ShaderStage::None`, a nonempty binding ID, zero payload bytes and
no shader entry point. Its content digest identifies the registered implementation
binding; it is not a fabricated shader blob.

GPU artifacts require a nonempty entry point, target profile, producer identity,
content digest, interface ABI digest and a nonzero byte count. GLSL, ESSL,
SPIR-V and D2D bytecode are never interchangeable, even when other identifiers
match. D2D vertex/compute transforms and opaque internal custom effect graphs
are outside this initial profile; a D2D pixel effect does not invent a generic
vertex stage to fit the GL/Vulkan graphics model.

Stage support is a tuple of artifact kind, stage and target profile. The adapter
and device must both contain the exact tuple. An API name or hardware compute
feature alone grants no compute execution path. Profile strings are opaque
qualified identities: this validator does not infer feature levels or extensions
from a version-looking string. Dispatch dimensions, workgroup dimensions, total
workgroup invocations and shared memory are checked independently against both
sets of limits, with overflow checks on products.

`InstalledArtifact` is a trusted backend-loader observation, tied to the selected
variant and artifact ID. The validator compares its observed content digest,
size, interface ABI, kind, stage, profile, entry point and builtin binding with
the manifest. It **does not read artifact bytes, calculate their hashes, perform
reflection, authenticate a package or call a device compiler**. Those are loader
and backend-owner responsibilities. A C++ caller fabricating both sides is not
an independent integrity proof. XML and TurboScript are not artifact authorities.

## Surface and pass graph profile

Surface support is an exact tuple of format, color space and alpha mode, not
independent masks whose combinations might be unsupported. NativePixel currently
describes RGBA8 or RGBA16F, sRGB or linear sRGB, and opaque, straight or
premultiplied alpha. TerminalCell describes cell storage, a specific 16/256/truecolor
mode and opaque alpha. Both adapter and device must support every used tuple.

The first graph profile deliberately specifies whole resources:

- Passes and resources have unique nonzero IDs. Passes are supplied in topological
  order; missing, forward, self, duplicate or cyclic dependencies fail.
- Each pass references existing artifacts and uses the stage combination required
  by its execution model. Unused artifact declarations fail.
- Every resource is used. Every non-external resource has one writer; external
  inputs are read-only. Exactly one resource is the output.
- Reads of intermediate/output resources require an earlier writer reachable
  through declared dependencies. Merely placing two passes consecutively does
  not establish a synchronization dependency.
- Content, subtree, backdrop, texture and cell input kinds require an external
  input. Every pass and resource must contribute to the output through actual
  read/producer edges. A disconnected branch or an ordering dependency alone
  cannot stand in for the effect's input data.
- Same-pass feedback and duplicate resource uses fail. There is no implicit
  aliasing, region-level overlap, in-place read/write or resource reuse.
- Native passes use native read/write, graphics passes use sampled read and color
  attachment write, and compute passes use sampled/storage read and storage write.
  Both adapter and device must declare the exact surface/access tuple. Supporting
  one format for sampling and another for storage writes does not create support
  for storage writes to the first format. Execution limits are conservative
  common bounds for all tuples in each declared domain.
- Each pass writes at least one whole resource. Non-compute passes cannot carry
  unused dispatch/workgroup/shared-memory parameters.

This graph check does not generate GL barriers, Vulkan transitions or D2D graph
objects. A backend must implement those operations and validate its native
binding/property layout before qualifying the corresponding profile.

All resource dimensions are nonzero and within both extent limits. Declared
bytes must be at least `width * height * 4` for RGBA8 and `* 8` for RGBA16F. Cell
storage must declare at least one byte per cell as a basic sanity floor; this is
not a fixed terminal-cell ABI. Owners must declare their actual conservative
storage needs. Format checks and minimum byte checks are not driver allocation
size queries.

## Shared resource accounting

The CPU contract computes a conservative charge for **all** declared resources,
including external inputs, plus all artifact payload bytes. Pixels and cells
are summed separately. Allocation slots equal resource count plus artifact count;
a native binding occupies a program/binding slot despite its zero payload bytes.
All arithmetic is checked. There is no alias discount or external-resource/cache
identity deduplication in this initial profile.

`SharedResourceBudget` has one immutable `ResourceScope`:

```text
owner_id / domain_id / device_generation
```

These IDs and the capability revision must be nonzero. Equal generation numbers
on different GL contexts, devices or terminal owners do not make resources
interchangeable. An owner creates one authoritative ledger for its scope and
shares copies of the handle; copies refer to the same mutex-protected state.
Constructing a second ledger for the same scope is not a supported way to divide
the budget. There is no global registry that enforces owner discipline.

The ledger bounds bytes, pixels, cells, allocation slots and the number of live
reservations. A capacity dimension of zero means none of that dimension is
available. `max_reservations` must be nonzero. Invalid construction throws
`std::invalid_argument`; allocation can throw `std::bad_alloc`. Validation and
preparation report allocation failure through their result types.

Reservation checks and charges every dimension in one critical section. A
failure charges nothing. Reservation handles are move-only, release exactly
once and keep the accounting state alive after the budget handle is destroyed.
The backend owner must keep the reservation through candidate, live, cached,
in-flight and retiring ownership until real failure cleanup or retirement has
completed. Moving a lease between those stages does not release and re-reserve it.

`mark_device_lost()` stops new reservations and preserves outstanding charges.
It does not synthesize completion, clear a fence, free native handles or claim a
successful frame. The owner cleans up the old generation and then releases its
leases; rebuilding uses a separate ledger for the new generation.

This is real atomic CPU accounting, but **not yet native resource accounting**:
driver overhead, native pipeline objects, actual allocation sizes, shared-resource
identity, cache charging and submission retirement are not integrated. The
profile's charge is not sufficient by itself to claim a complete renderer memory
bound. No current renderer is allowed to treat the reservation as permission to
skip its actual cost, creation or retirement checks.

## Limits, ownership and errors

Default input limits are 16 parameters, 16 artifacts, 64 resources, 32 passes,
64 resource uses per pass, 32 dependencies per pass, 64 adapter records,
256 installed-artifact observations, 32 entries per capability domain,
128 bytes per identifier, 1024 bytes per source label/token and 128 KiB of
accounted metadata. Raw counts are checked before duplicate detection or graph
work. Zero is invalid for a validation limit.

Metadata accounting uses object/element sizes and string bytes. It bounds the
described input and retained copy, not allocator bookkeeping or unrelated caller
storage. The dependency-closure workspace has a separate bound equal to
`max_metadata_bytes`; other work arrays are bounded by admitted counts. Diagnostic
label/token copies are each capped at 1024 bytes even on rejected oversized input.
The output retains original source offsets/lengths. No process-wide mutable
history cache is introduced.

Errors distinguish unknown/unsupported semantic effects, backend/target/units,
formats, parameter domains, stages and accesses; invalid graphs; missing or
ambiguous variants; content/ABI mismatches; invalid values; resource/input limits;
budget exhaustion; owner/domain mismatch; stale generation/capabilities; device
loss and allocation failure. The result has no successful partial contract or
reservation. Diagnostics carry bounded source/token information and a pass or
resource ID when relevant.

Capabilities are immutable host snapshots for the duration of a call. Any
adapter-domain, device-domain or installed-artifact change must advance
`capability_revision`. Reusing an old revision for different records violates the
host contract. CPU input snapshots and final UI/viewport/program revision checks
remain part of the later transactional publisher.


## Pre-submission native owner bridge (second M2 slice, #17)

`flex/render/effect_resource_owner.h` adds
`prepare_native_effect(validated, shared_budget, native_owner)`.
This API does not create a second provider or select an alternative backend.
It only moves from *validated metadata plus budget* to a single candidate
`PreparedNativeEffect` whose backend-specific resource bundle and shared
reservation live and die together.

The backend supplies both `NativeEffectResourceOwner::capabilities()` and
`prepare_resources(const ValidatedEffectContract&)`. The latter must return
a complete **real** owner-specific bundle; null or exception is a hard failure.
A backend is responsible for cleaning its partial allocations before returning
an error, and the returned bundle's noexcept destructor must complete native
cleanup synchronously on the proper owner thread. The bundle must retain any
backend/device lifetime dependencies independently of the short-lived factory.

The generic bridge performs:
1. A fresh owner capability query and an atomic reservation from the existing
   domain ledger, before asking the owner to create any native resource.
2. Native preparation from the **frozen** validated contract only.
3. Verification of the complete bundle's owner/domain/device generation,
   backend profile, target/unit digest, variant, implementation and parameter
   ABI hashes, and entire conservatively charged resource cost.
4. A second owner capability query plus lost-ledger check, before returning a
   successful candidate. A stale generation, changed capability revision or
   lost device is rejected without publishing the candidate.
5. Cleanup in a strict order on all failure, reset, move-assignment and normal
   destruction paths: **native resources first, reserved budget second**.
   Repeated reset is idempotent; old and candidate batches consume their peak
   combined budget until each actually retires.

`NativeEffectResourceIdentity::charged_cost` must match the contract's
conservative reservation. This is *not* a GPU memory query and does not
prove actual driver allocation size. The owner must use an accurate
conservative bound and ensure all capability-domain or artifact changes
advance its revision. No new D2D, GL, GLES, Vulkan or Tango shader/manifest is
registered by this bridge, and no production adapter currently implements
the new owner interface.

**Pre-submission only:** `PreparedNativeEffect` has no `submit()` method
or implicit GPU fence, and must not be destroyed while resources could still
be accessed by an in-flight frame. Moving it into a future RenderPlan
requires an independently implemented owner-thread completion, retirement
and lost/aborted protocol. CPU admission/owner tests with synthetic
artifacts cannot qualify real shader compilation, GPU execution, or PTY UI.
This bridge does not publish Style/Layout/hit geometry and has no CSS
input or fallback.

## First production native buffer owner: Tango TerminalCell clear

The next independently scoped M2/M3 boundary is a **real, limited Tango
native cell-buffer owner** in `flex_backend_tui`, implemented using the
existing C11 `tui_buffer_create`, `tui_buffer_clear`, and
`tui_buffer_destroy` functions. This is a qualified **NativeBuiltin**
operation named `tango.cell-buffer.clear`, not a GPU shader.

`TangoCellClearOwner` advertises only its actual Tango
`TerminalCell / CellColumn / CellRow / TerminalTrueColor` binding. It
declares one native output, no parameters, one native-write pass and
one linked builtin artifact with zero shader bytes. Fixed versioned
semantic/target/binding/ABI hashes identify these contracts; they
**do not** pretend to be hashes of compiled Tango binary files.
Actual dynamically loaded shader artifacts are not involved.

A caller explicitly requests bounded columns/rows (max total 65,536
cells). The complete output reservation covers
`sizeof(tui_buffer_t) + columns * rows * sizeof(tui_cell_t)`, the
two actual C allocation blocks, and the cell count. The owner checks
the exact frozen manifest and never allocates from an uncontrolled
width/height. `tui_buffer_create` failure is a hard allocation
error; it never falls back to a different backend. A prepared bundle
owns the buffer directly, independent of its factory's lifetime.
Owner-thread-only borrowing is exposed to native C++ integration
through `PreparedNativeEffect::native_resources()` and
`TangoCellClearResources::buffer_on_owner()`; these handles
must not escape their prepared resource's lifetime.

The existing real Flex→Tango synchronous `RenderCommandList`
renderer can draw to that buffer. Its replay must finish and release
all borrows **before** prepared resources are retired; this contract
does not create a terminal front/back-buffer transaction, resize a
live terminal, or implement asynchronous in-flight GPU retirement.
Explicit `mark_device_lost(budget)` makes the old scope unavailable
for new preparation while retaining the old resource and budget until
its owner-thread destruction. Recovery creates a distinct generation
and new ledger.

The production Tango test now verifies real buffer creation, clear,
actual Flex command replay, insufficient-budget refusal without
mutating existing cells, invalid dimensions and GPU shader rejection,
wrong-thread refusal, lost-vs-new-generation resource ownership and
factory-independent retirement. It runs in the **existing**
`test_render_command_tango` native headless and sanitizer CI;
no new workflow or provider is needed.

**Not qualified:** blur, shadow, custom Tango cell effects, GPU shaders,
D2D/GL/GLES/Vulkan resource owners, XML/Tailwind default mounting,
a whole UI revision or actual terminal present. The older
`RendererCapabilities` booleans are not used to advertise this
native binding.

## Qualification and remaining integration

`flex/tests/test_effect_admission.cpp` uses explicitly synthetic capability and
loader fixtures. It tests the real validator and shared ledger, including
cross-API rejection, stage/format/access intersections, bounded graphs, overflow,
range/extent failures, frozen inputs, ownership/generations, device loss and
concurrent reservation. These are CPU contract tests, not D2D/GL/GLES/Vulkan
shader execution or real PTY evidence.

The existing Flex test target and the existing Linux XML qualification and
Windows/macOS headless selections compile the actual implementation. The source
is part of the existing `Flex::Render` target and header installation; no new
backend, workflow, cache policy or product dependency is introduced.

Still required before M2/M3 acceptance: extracting requirements from all reachable
native styles; real qualified backend snapshots and artifact loaders; native
allocation/cost reservations; aggregate whole-update preparation; immutable
in-flight parameters; transactional style/layout/hit-geometry/plan publication;
device-specific synchronization, loss and retirement; and exact-source evidence
on each actual backend. The default Box/XML path is not switched by this work.
