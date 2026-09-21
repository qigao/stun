# XML content preflight (#35)

This is a bounded implementation slice under #16, based on the canonical dependency and CI work in #34. It carries forward the descriptor/preflight idea from Draft #12 without importing the obsolete dependency surface of the old #11–#14 stack.

## Ownership and ordering

`WidgetRegistry` owns each tag's immutable `UiNodeDescriptor` and factory. `UiNodeDescriptor::validate_content` checks only one node's declared text and child count. `WidgetRegistry::validate` additionally resolves all tags in depth-first source order, with O(nodes) time and O(depth) auxiliary storage bounded by `UiDocumentLimits`.

Both registry-aware `UiDocumentInstantiator::instantiate` overloads validate the entire tree before allocating its detached tree or invoking any factory. `DesktopApplication` performs the same validation before allocating the candidate Box or loading CSS. An invalid later sibling therefore cannot cause an earlier factory to run. Failed content preflight returns a structured UI compile/instantiation error and leaves the target Box untouched.

`WidgetRegistry::create` creates one widget, not a whole UI tree. It validates that node's content before calling its factory. Whole-tree callers must use the registry preflight; direct creation is not a recursive child-instantiation API.

## Content contract

| Model | Scalar `text`, `content`, `bind.text` | Element children |
| --- | --- | --- |
| Empty | Rejected, including explicit empty strings | None |
| Text | Optional; every declared value must be a string | None |
| Children | Rejected | Zero or more |
| SingleChild | Rejected | Exactly one |
| TextAndChildren | Optional strings | Zero or more |

Other widget properties are not silently coerced by this check. Their complete property/event/binding schemas remain #16. The XML adapter still rejects natural/mixed text it cannot lower; TextAndChildren describes the existing explicit text-property plus ordered-element model, not a claim of ordered XML mixed-content support.

All existing built-in structural tags explicitly retain TextAndChildren because generic Elements support text alongside children. Built-in widget classifications are explicit at registration:

- Empty: input, slider, progress, select, image, spinner, divider, dropdown, calendar, colorpicker, avatar, pagination, stepper, gradient-editor.
- Text: label, checkbox, radio, switch, textarea, badge, toast, markdown.
- Children: tabs, accordion, table, tree, group-button, toggle-group, breadcrumb.
- TextAndChildren: button, tooltip, modal, card.

No new tag names are admitted. Existing custom-widget registration defaults to its original TextAndChildren contract; a caller can select a stricter content model explicitly. Structural registration defaults to Children. Invalid descriptor enum values and duplicate tags are rejected without replacing an existing entry.

## Source and API compatibility

The tag SourceSpan is appended to UiNodeDefinition, preserving the order of prior aggregate fields. XML records the parser's tag-name position. Content diagnostics prefer an existing property span, otherwise the owning tag. Tests assert exact line/column values for simple multiline input. Precise attribute/entity/CDATA byte ranges, Unicode source admission and normalization are not implemented here and remain #15/#16.

The definition layout and registry registration signatures change; participating C++ consumers must rebuild together. Invalid leaf text/children formerly admitted by the registry will now be rejected. The no-registry instantiator remains an explicitly structural, generic-Element API and does not establish schema acceptance. Its retirement/separation remains #16/#33; it is not used to bypass typed application validation.

## Validation

`test_ui_content_models` has ten pure, executable content-policy cases and needs no renderer, parser or JIT. The dedicated XML content workflow compiles it with the actual TinyTest sources from Salts master under ASan/UBSan/leak detection and requires complete unfiltered summaries. This is unit evidence, not whole-application acceptance.

The existing production workflow now additionally builds and executes test_ui_content_models, test_ui_xml, test_ui_document and test_widget_registry, retaining its previous eleven tests and installed-component consumers. Added registry tests cover a later unknown sibling with zero earlier factory calls, both definition/program entry points, empty/text leaf rejection, direct-node checks, bounded traversal and immutable duplicate registration. Application tests require schema errors to precede CSS errors.

Do not close #35 until those actual integrated tests pass on the PR head. TurboScript #11 and remaining dependency-source policy work are separate blockers and are not bypassed by this feature.
