# Independent Mermaid semantics

Run `npm ci --ignore-scripts` in this directory, then
`node verify.mjs /tmp/flowchart-mermaid-oracle.json`.
The exact Mermaid 11.12.0 and jsdom 26.1.0 dependency graph is locked with
registry integrity hashes. These JavaScript packages are test dependencies;
the production parser and ownership model remain C11.

This check parses the original and rewritten sources for the seven unresolved
legacy golden cases. It verifies their endpoint sequences against their
respective `input.json` and `output.json`, records source hashes, and verifies
semantic differences independently of Stun. It never generates golden files,
sorts arrays, or treats a failed Stun comparison as successful.

Two differences require care:

- `mermaid-024`: rewriting `A→B, B→C, C→D` to `A→B, C→D, B→C` while retaining
  `linkStyle 1` changes the styled edge from B→C to C→D.
- `v2-mermaid-104`: the original source has six graph nodes and eight named
  edges; seven of those edges receive curve properties. The rewritten source
  drops the edge IDs and curves and introduces seven extra nodes. Neither
  the legacy input JSON nor the output JSON is a complete semantic oracle.

The historical generator version remains unknown. This establishes a pinned
independent semantic baseline, not provenance for the old generator. Fixture
migration and any versioned JSON contract change require a separate review
under issue #236; the original strict golden gate remains intact.

Parsed edge IDs and curves can be read through
`flowchart_edge_get_id` / `flowchart_edge_get_curve`. Their strings are owned
by the parsed edge and freed with its diagram. Public AST structure layouts
remain unchanged. These accessors accept parser-owned edges only. The current
renderer applies the supported parsed curve choices to SVG path interpolation,
with a bounded node-clearance check; see the module README.
The legacy JSON adapter still omits edge IDs and curves; adding those JSON
fields belongs to the separately reviewed output-contract migration.

## Proposed fixture migration (#236)

`node migrate.mjs` verifies an explicit 95-case test contract. It keeps the
existing expectations for 88 cases, and checks seven new `*.source-v1.json`
expectations against pinned Mermaid edge declaration order. The C test reads
`contracts.json` on both Windows and POSIX; an absent case, unsupported schema
or missing expected file fails the test. No link array is normalized during
comparison. All original input/output Mermaid and JSON files remain unchanged.

The migration permits only two changes to the historical snapshots: a
bijective permutation of complete link objects into independently observed
declaration order, and removal of IDs that Mermaid recognizes as named edges
rather than nodes. The latter affects the seven phantom nodes in
`v2-mermaid-104`. Node properties, class/style data and every link object are
otherwise preserved. `source-v1-provenance.json` records source, historical and
proposed hashes and the before/after order. `node migrate.mjs --write` explicitly
regenerates the proposal for review; CI only checks it.

This preserves the existing sparse JSON adapter contract, including its
limitations: double-ended point arrows are projected as `arrow_point`, dotted
arrow length is 1, node-level inline styles are not represented, and edge IDs
and curves are exposed through AST accessors rather than JSON. These omissions
are not excused as full Mermaid conformance by a passing test. The source
contract preserves numeric `linkStyle` declaration indices. The focused renderer
profile applies stroke/width by those indices and supported named curves.
Broader CSS and stronger curve-clearance constraints remain outside the profile. See [the module README](../../../README.md#flowchart-svg-link-styles)
for supported values and rejection behavior.

`verify.mjs` also checks the default/indexed precedence used by the headless
renderer regression: repeated indexed declarations replace the previous style
list, and the final default is combined under the final indexed list. This
evidence comes from the pinned Mermaid parser database, independent of Stun.

## Production SVG curve evidence

After the native headless renderer test has emitted its artifacts, run:

```sh
node verify-curves.mjs /path/to/flowchart-linkstyle.svg.curves \
  /path/to/flowchart-linkstyle.svg.curves.legacy \
  /path/to/flowchart-linkstyle.svg.curves.professional
```

The oracle uses lock-pinned d3-shape 3.2.0, a Mermaid dependency, to compute
commands from actual native route knots. It checks all 313 emitted SVG paths,
owned IDs/curve names, every control/end coordinate and viewport containment.
Independent diagnostic sampling confirms the original legacy scene's refused
node intersection; it does not replace the production conservative certificate.
Native tests require full-scene professional acceptance and legacy refusal.
