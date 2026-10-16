/*
 * FlexUI MarkdownWidget: exactly one canonical Markdown parser, Stun C11.
 *
 * C11 RichDoc is the sole syntax/semantic authority, including Unicode 17
 * reference folding, task checkboxes and GFM table/list ownership. The
 * public view is borrowed only until projection finishes; FlexUI owns its
 * copied text/elements. No md::parse, alternate grammar or fallback.
 */
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <stun/markdown_c11.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace flexUI {
namespace {

constexpr size_t kMaxMarkdownBytes = 1024u * 1024u;
constexpr size_t kMaxMarkdownNodes = 16384u;

std::string semantic(const stun_ui_rich_document_view& view,
                     const stun_ui_block& block) {
    const size_t n = block.byte_end - block.byte_begin;
    return n ? std::string(view.document.source + block.byte_begin, n)
             : std::string{};
}

bool known_kind(stun_ui_block_kind kind) {
    switch (kind) {
    case STUN_UI_BLOCK_DOCUMENT:
    case STUN_UI_BLOCK_PARAGRAPH:
    case STUN_UI_BLOCK_HEADING:
    case STUN_UI_BLOCK_TEXT:
    case STUN_UI_BLOCK_CODE_FENCE:
    case STUN_UI_BLOCK_CODE_INDENTED:
    case STUN_UI_BLOCK_CODE_SPAN:
    case STUN_UI_BLOCK_LIST:
    case STUN_UI_BLOCK_LIST_ITEM:
    case STUN_UI_BLOCK_ORDERED_LIST:
    case STUN_UI_BLOCK_TASK_ITEM:
    case STUN_UI_BLOCK_CHECKBOX:
    case STUN_UI_BLOCK_QUOTE:
    case STUN_UI_BLOCK_TABLE:
    case STUN_UI_BLOCK_TABLE_ROW:
    case STUN_UI_BLOCK_TABLE_CELL:
    case STUN_UI_BLOCK_EMPHASIS:
    case STUN_UI_BLOCK_STRONG:
    case STUN_UI_BLOCK_LINK:
    case STUN_UI_BLOCK_IMAGE:
    case STUN_UI_BLOCK_STRIKETHROUGH:
    case STUN_UI_BLOCK_UNDERLINE:
    case STUN_UI_BLOCK_RULE:
    case STUN_UI_BLOCK_ENTITY:
    case STUN_UI_BLOCK_LINE_BREAK:
    case STUN_UI_BLOCK_HARD_LINE_BREAK:
    case STUN_UI_BLOCK_MATH:
    case STUN_UI_BLOCK_MATH_BLOCK:
    case STUN_UI_BLOCK_HIGHLIGHT:
    case STUN_UI_BLOCK_SUPERSCRIPT:
    case STUN_UI_BLOCK_SUBSCRIPT:
    case STUN_UI_BLOCK_FOOTNOTE:
    case STUN_UI_BLOCK_FOOTNOTE_DEF:
    case STUN_UI_BLOCK_DEFINITION_LIST:
    case STUN_UI_BLOCK_DEFINITION_TERM:
    case STUN_UI_BLOCK_DEFINITION_DESC:
    case STUN_UI_BLOCK_DIAGRAM:
    case STUN_UI_BLOCK_LINK_TITLE:
    case STUN_UI_BLOCK_IMAGE_TITLE:
    case STUN_UI_BLOCK_RAW_HTML_INLINE:
    case STUN_UI_BLOCK_RAW_HTML_BLOCK:
    case STUN_UI_BLOCK_URI_AUTOLINK:
        return true;
    default:
        return false;
    }
}

bool preflight(const stun_ui_rich_document_view& view) {
    if (stun_ui_rich_document_validate(&view, kMaxMarkdownNodes) != STUN_UI_OK ||
        view.document.block_count == 0 ||
        view.document.blocks[0].kind != STUN_UI_BLOCK_DOCUMENT)
        return false;
    for (size_t i = 0; i < view.document.block_count; ++i) {
        const stun_ui_block& node = view.document.blocks[i];
        if (!known_kind(node.kind)) return false;
        if (node.kind == STUN_UI_BLOCK_HEADING &&
            (node.heading_level < 1u || node.heading_level > 6u))
            return false;
        if (node.kind == STUN_UI_BLOCK_CHECKBOX) {
            const char* data = nullptr;
            size_t length = 0;
            if (stun_ui_rich_document_metadata(&view, i, &data, &length) != STUN_UI_OK ||
                length != 1u || !data || (data[0] != 'x' && data[0] != ' '))
                return false;
            const uint32_t parent = node.parent_index;
            if (parent == STUN_UI_PARENT_NONE ||
                view.document.blocks[parent].kind != STUN_UI_BLOCK_TASK_ITEM)
                return false;
        }
    }
    return true;
}

bool is_metadata_leaf(stun_ui_block_kind k) {
    return k == STUN_UI_BLOCK_LINK_TITLE || k == STUN_UI_BLOCK_IMAGE_TITLE;
}

void add_content(Element* target, const std::string& value) {
    if (!value.empty()) target->set_text(value);
}

/* The owned staged forest is detached from the visible Box tree until
 * the entire C11 projection succeeds. Failed projections destroy these
 * unique_ptrs instead of accumulating orphaned Box::create() nodes. */
struct Projection {
    std::vector<std::unique_ptr<Element>> elements;
    std::vector<Element*> roots;

    ~Projection() {
        // During rejected staging, children refer to other nodes in this
        // batch. Unlink in reverse construction order while all nodes live,
        // then let unique_ptr release the entire unpublished forest.
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            Element* element = it->get();
            if (element && element->parent_elem())
                element->parent_elem()->remove_child(element);
        }
    }
};

Element* staged_create(Projection& result, const std::string& tag,
                       size_t limit) {
    if (result.elements.size() >= limit)
        throw std::length_error("MarkdownWidget projection capacity exceeded");
    auto element = std::make_unique<Element>();
    element->set_tag(tag);
    Element* ptr = element.get();
    result.elements.push_back(std::move(element));
    return ptr;
}

/* Rendering is an owned projection, NOT a second Markdown parser. The
 * semantic source indexes the immutable decoded C11 text arena, never the
 * raw UTF-8 Markdown. Store only copied std::strings in each Element. */
void project(const stun_ui_rich_document_view& view,
             Projection& result, size_t limit) {
    const size_t count = view.document.block_count;
    std::vector<Element*> owners(count, nullptr);
    std::vector<size_t> child_count(count, 0u);
    for (size_t i = 1; i < count; ++i) {
        const uint32_t parent = view.document.blocks[i].parent_index;
        if (!is_metadata_leaf(view.document.blocks[i].kind)) ++child_count[parent];
    }
    for (size_t i = 1; i < count; ++i) {
        const stun_ui_block& block = view.document.blocks[i];
        Element* parent = owners[block.parent_index];
        if (block.parent_index != 0u && !parent)
            throw std::logic_error("invalid RichDoc projection ancestry");
        const stun_ui_block_kind kind = block.kind;
        if (is_metadata_leaf(kind)) continue;
        Element* elem = nullptr;
        const bool single_text = kind == STUN_UI_BLOCK_TEXT &&
            child_count[block.parent_index] == 1u &&
            (view.document.blocks[block.parent_index].kind == STUN_UI_BLOCK_HEADING ||
             view.document.blocks[block.parent_index].kind == STUN_UI_BLOCK_PARAGRAPH ||
             view.document.blocks[block.parent_index].kind == STUN_UI_BLOCK_EMPHASIS ||
             view.document.blocks[block.parent_index].kind == STUN_UI_BLOCK_STRONG);
        if (single_text) {
            add_content(parent, semantic(view, block));
            owners[i] = parent;
            continue;
        }
        switch (kind) {
        case STUN_UI_BLOCK_HEADING: {
            char heading_tag[4] = {'h', char('0' + block.heading_level), '\0', '\0'};
            elem = staged_create(result, heading_tag, limit);
            elem->add_class("md-header");
            elem->add_class(heading_tag);
            break;
        }
        case STUN_UI_BLOCK_TEXT:
        case STUN_UI_BLOCK_ENTITY:
        case STUN_UI_BLOCK_RAW_HTML_INLINE:
        case STUN_UI_BLOCK_URI_AUTOLINK:
            elem = staged_create(result, "span", limit);
            add_content(elem, semantic(view, block));
            break;
        case STUN_UI_BLOCK_PARAGRAPH:
            elem = staged_create(result, "div", limit); elem->add_class("md-p"); break;
        case STUN_UI_BLOCK_LIST:
            elem = staged_create(result, "div", limit); elem->add_class("md-ul"); break;
        case STUN_UI_BLOCK_ORDERED_LIST:
            elem = staged_create(result, "div", limit); elem->add_class("md-ol"); break;
        case STUN_UI_BLOCK_LIST_ITEM:
        case STUN_UI_BLOCK_TASK_ITEM:
            elem = staged_create(result, "div", limit); elem->add_class("md-li");
            if (kind == STUN_UI_BLOCK_TASK_ITEM) elem->add_class("md-task-item");
            break;
        case STUN_UI_BLOCK_CHECKBOX: {
            const char* data = nullptr; size_t size = 0;
            (void)stun_ui_rich_document_metadata(&view, i, &data, &size);
            elem = staged_create(result, "span", limit);
            elem->add_class("md-checkbox");
            elem->set_text(data[0] == 'x' ? "[x] " : "[ ] ");
            break;
        }
        case STUN_UI_BLOCK_QUOTE:
            elem = staged_create(result, "div", limit); elem->add_class("md-quote"); break;
        case STUN_UI_BLOCK_TABLE:
            elem = staged_create(result, "div", limit); elem->add_class("md-table"); break;
        case STUN_UI_BLOCK_TABLE_ROW:
            elem = staged_create(result, "div", limit); elem->add_class("md-tr"); break;
        case STUN_UI_BLOCK_TABLE_CELL:
            elem = staged_create(result, "div", limit); elem->add_class("md-td"); break;
        case STUN_UI_BLOCK_EMPHASIS:
            elem = staged_create(result, "span", limit); elem->add_class("md-em"); break;
        case STUN_UI_BLOCK_STRONG:
            elem = staged_create(result, "span", limit); elem->add_class("md-strong"); break;
        case STUN_UI_BLOCK_STRIKETHROUGH:
            elem = staged_create(result, "span", limit); elem->add_class("md-del"); break;
        case STUN_UI_BLOCK_UNDERLINE:
            elem = staged_create(result, "span", limit); elem->add_class("md-u"); break;
        case STUN_UI_BLOCK_LINK:
            elem = staged_create(result, "span", limit); elem->add_class("md-a"); break;
        case STUN_UI_BLOCK_IMAGE:
            elem = staged_create(result, "span", limit); elem->add_class("md-img"); break;
        case STUN_UI_BLOCK_CODE_FENCE:
        case STUN_UI_BLOCK_CODE_INDENTED:
            elem = staged_create(result, "div", limit); elem->add_class("md-code-block"); break;
        case STUN_UI_BLOCK_CODE_SPAN:
            elem = staged_create(result, "span", limit); elem->add_class("md-code"); break;
        case STUN_UI_BLOCK_RULE:
            elem = staged_create(result, "div", limit); elem->add_class("md-hr"); break;
        case STUN_UI_BLOCK_LINE_BREAK:
        case STUN_UI_BLOCK_HARD_LINE_BREAK:
            elem = staged_create(result, "div", limit); elem->add_class("md-br"); break;
        case STUN_UI_BLOCK_MATH:
            elem = staged_create(result, "span", limit); elem->add_class("md-math"); break;
        case STUN_UI_BLOCK_MATH_BLOCK:
            elem = staged_create(result, "div", limit); elem->add_class("md-math-block"); break;
        case STUN_UI_BLOCK_HIGHLIGHT:
            elem = staged_create(result, "span", limit); elem->add_class("md-highlight"); break;
        case STUN_UI_BLOCK_SUPERSCRIPT:
            elem = staged_create(result, "span", limit); elem->add_class("md-sup"); break;
        case STUN_UI_BLOCK_SUBSCRIPT:
            elem = staged_create(result, "span", limit); elem->add_class("md-sub"); break;
        case STUN_UI_BLOCK_FOOTNOTE:
            elem = staged_create(result, "span", limit); elem->add_class("md-footnote"); break;
        case STUN_UI_BLOCK_FOOTNOTE_DEF:
            elem = staged_create(result, "div", limit); elem->add_class("md-footnote-def"); break;
        case STUN_UI_BLOCK_DEFINITION_LIST:
            elem = staged_create(result, "div", limit); elem->add_class("md-def-list"); break;
        case STUN_UI_BLOCK_DEFINITION_TERM:
            elem = staged_create(result, "div", limit); elem->add_class("md-def-term"); break;
        case STUN_UI_BLOCK_DEFINITION_DESC:
            elem = staged_create(result, "div", limit); elem->add_class("md-def-desc"); break;
        case STUN_UI_BLOCK_DIAGRAM:
            elem = staged_create(result, "div", limit); elem->add_class("md-diagram"); break;
        case STUN_UI_BLOCK_RAW_HTML_BLOCK:
            elem = staged_create(result, "div", limit); elem->add_class("md-raw-html");
            add_content(elem, semantic(view, block));
            break;
        case STUN_UI_BLOCK_DOCUMENT:
        case STUN_UI_BLOCK_LINK_TITLE:
        case STUN_UI_BLOCK_IMAGE_TITLE:
            break; // preflight and leaf handling already covered
        }
        if (elem) {
            if (block.parent_index == 0u)
                result.roots.push_back(elem);
            else if (parent->append(elem) != elem)
                throw std::logic_error("detached projection append rejected");
            owners[i] = elem;
        }
    }
}

} // namespace

MarkdownWidget::MarkdownWidget(const std::string& markdown,
                               std::size_t max_render_elements)
    : markdown_(markdown),
      max_render_elements_(std::min(max_render_elements, kMaxMarkdownNodes)) {}

void MarkdownWidget::set_markdown(const std::string& markdown) {
    if (markdown_ == markdown) return;
    markdown_ = markdown;
    needs_rebuild_ = true;
}

void MarkdownWidget::rebuild_elements(Element& elem) {
    if (!needs_rebuild_ || !elem.owner_box_) return;
    /* No-fallback C11 admission. Keep the previous element tree if the next
     * source is over budget or invalid; never publish a partially built AST.
     * The widget remains marked for update rather than adopting old grammar. */
    if (markdown_.size() >= kMaxMarkdownBytes) return;
    stun_md_document* document = nullptr;
    const auto status = stun_md_parse(
        markdown_.data(), markdown_.size(),
        kMaxMarkdownNodes, markdown_.size() + 1u, &document);
    if (status != STUN_UI_OK || !document) return;
    // C11 document remains borrowed only during preflight/projection.
    std::unique_ptr<stun_md_document, decltype(&stun_md_document_destroy)>
        owned_document(document, &stun_md_document_destroy);
    const auto view = stun_md_document_view(document);
    if (!preflight(view)) return;

    Projection staged;
    std::vector<Element*> next_projection;
    try {
        project(view, staged, max_render_elements_);
        next_projection.reserve(staged.elements.size());
        for (const auto& element : staged.elements)
            next_projection.push_back(element.get());

        // Box reserves and validates the whole detached forest before taking
        // sole ownership; a failed stage leaves the old view untouched.
        elem.owner_box_->adopt_detached_elements(std::move(staged.elements));
    } catch (const std::exception&) {
        return; // no legacy parser and no partial publication
    }

    try {
        // Group now prepares all child storage before committing one topology
        // replacement, so even bad_alloc preserves the prior visible tree.
        elem.replace_children(staged.roots);
    } catch (const std::exception&) {
        // Adoption succeeded but publication did not: explicitly reclaim the
        // detached batch to avoid persistent orphan nodes in the Box.
        (void)elem.owner_box_->retire_detached_elements(next_projection);
        return;
    }

    elem.set_text("");
    if (elem.computed_style)
        elem.computed_style->flex_direction = FlexDirection::Column;
    needs_rebuild_ = false;
    elem.mark_style_dirty();

    // Once the new view is published, retire precisely the old projection.
    // All released Nodes are id-less, application-owned descendants; Box
    // first clears focus/capture, animation, style, and container caches.
    std::vector<Element*> previous = std::move(published_projection_);
    published_projection_ = std::move(next_projection);
    (void)elem.owner_box_->retire_detached_elements(previous);
}

void MarkdownWidget::emit_render_commands(const Element&, RenderCommandList&) {}

bool MarkdownWidget::handle_event(const Event&, Element&) { return false; }

void MarkdownWidget::update(float, Element& elem) { rebuild_elements(elem); }

} // namespace flexUI
