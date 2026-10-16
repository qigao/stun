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
#include <flexUI/syntax_highlighter.h>
#include <stun/markdown_c11.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace flexUI {
namespace {

constexpr size_t kMaxMarkdownBytes = 1024u * 1024u;
constexpr size_t kMaxMarkdownNodes = 16384u;
// Renderer-only providers do not receive an unbounded source or unlimited
// byte-range hints. Larger code blocks still display all C11-owned text.
constexpr size_t kMaxHighlightBytes = 64u * 1024u;
constexpr size_t kMaxHighlightTokens = 1024u;

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

// Syntax highlighting is presentation over already-accepted C11 code
// TEXT. Neither the highlighter nor its hint may add Markdown grammar.
std::string fence_language(const stun_ui_rich_document_view& view, size_t id) {
    const char* value = nullptr;
    size_t length = 0u;
    if (stun_ui_rich_document_metadata(&view, id, &value, &length) != STUN_UI_OK)
        throw std::logic_error("invalid C11 code fence metadata");
    if (!value || !length) return {};
    size_t n = 0u;
    while (n < length && value[n] != ' ' && value[n] != '\t') ++n;
    if (!n || n > 48u) return {};
    for (size_t i = 0; i < n; ++i) {
        const char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '+'))
            return {};
    }
    return std::string(value, n);
}

bool utf8_boundary(std::string_view code, size_t offset) {
    return offset == 0u || offset == code.size() ||
        (static_cast<unsigned char>(code[offset]) & 0xc0u) != 0x80u;
}

void project_code_text(Element* code_block, Projection& result, size_t limit,
                       const std::string& source, const std::string& language) {
    std::vector<HighlightToken> tokens;
    // An optional Tree-sitter highlighter may tokenize C11-owned code text,
    // but cannot become a Markdown parser. Invalid/unavailable highlights
    // retain the exact same visible code as plain presentation.
    if (!language.empty() && source.size() <= kMaxHighlightBytes) {
        try {
            if (auto* highlighter = HighlighterFactory::instance().get(language))
                tokens = highlighter->highlight(source);
        } catch (const std::exception&) {
            tokens.clear();
        }
    }
    size_t prev = 0u;
    // Optional highlighter complexity must never consume the Widget's entire
    // bounded Element quota and reject otherwise printable C11-owned code.
    // Plain text is always the semantic fallback; not a Markdown fallback.
    const size_t remaining = limit - result.elements.size();
    if (tokens.size() > kMaxHighlightTokens ||
        tokens.size() > remaining / 3u)
        tokens.clear();
    for (const auto& token : tokens) {
        if (token.length == 0u || token.start < prev ||
            token.start > source.size() ||
            token.length > source.size() - token.start ||
            !utf8_boundary(source, token.start) ||
            !utf8_boundary(source, token.start + token.length)) {
            tokens.clear();
            break;
        }
        prev = token.start + token.length;
    }

    Element* line = nullptr;
    const auto ensure_line = [&]() -> Element* {
        if (!line) {
            line = staged_create(result, "div", limit);
            line->add_class("md-code-line");
            if (code_block->append(line) != line)
                throw std::logic_error("detached code line rejected");
        }
        return line;
    };
    const auto emit = [&](size_t from, size_t to, HighlightType type) {
        while (from < to) {
            Element* current = ensure_line();
            const size_t newline = source.find('\n', from);
            const size_t end = newline == std::string::npos
                ? to : std::min(newline, to);
            if (end > from) {
                Element* part = staged_create(result, "span", limit);
                if (type != HighlightType::Plain)
                    part->add_class(highlight_class(type));
                part->set_text(source.substr(from, end - from));
                if (current->append(part) != part)
                    throw std::logic_error("detached code span rejected");
            }
            if (end < to && source[end] == '\n') {
                line = nullptr;
                from = end + 1u;
            } else {
                from = end;
            }
        }
    };

    if (source.empty()) {
        (void)ensure_line();
        return;
    }
    size_t cursor = 0u;
    for (const auto& token : tokens) {
        if (token.start > cursor)
            emit(cursor, token.start, HighlightType::Plain);
        emit(token.start, token.start + token.length, token.type);
        cursor = token.start + token.length;
    }
    if (cursor < source.size()) emit(cursor, source.size(), HighlightType::Plain);
}

/* Rendering is an owned projection, NOT a second Markdown parser. The
 * semantic source indexes the immutable decoded C11 text arena, never the
 * raw UTF-8 Markdown. Store only copied std::strings in each Element. */
void project(const stun_ui_rich_document_view& view,
             Projection& result, size_t limit) {
    const size_t count = view.document.block_count;
    std::vector<Element*> owners(count, nullptr);
    std::vector<size_t> child_count(count, 0u);
    // Column order is a property of each typed TABLE_ROW, not Markdown
    // punctuation. Consume the parent TABLE's immutable C11 alignment bytes.
    std::vector<size_t> row_columns(count, 0u);
    std::vector<std::string> code_languages(count);
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
            if (view.document.blocks[block.parent_index].kind ==
                    STUN_UI_BLOCK_CODE_FENCE ||
                view.document.blocks[block.parent_index].kind ==
                    STUN_UI_BLOCK_CODE_INDENTED) {
                project_code_text(parent, result, limit, semantic(view, block),
                                  code_languages[block.parent_index]);
                owners[i] = parent;
                continue;
            }
            [[fallthrough]];
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
        case STUN_UI_BLOCK_TABLE_CELL: {
            const uint32_t row_id = block.parent_index;
            if (row_id >= i || view.document.blocks[row_id].kind != STUN_UI_BLOCK_TABLE_ROW)
                throw std::logic_error("table cell without typed row owner");
            const uint32_t table_id = view.document.blocks[row_id].parent_index;
            if (table_id >= row_id ||
                view.document.blocks[table_id].kind != STUN_UI_BLOCK_TABLE)
                throw std::logic_error("table row without typed table owner");
            const char* alignment = nullptr;
            size_t column_count = 0u;
            if (stun_ui_rich_document_metadata(
                    &view, table_id, &alignment, &column_count) != STUN_UI_OK ||
                !alignment || row_columns[row_id] >= column_count)
                throw std::logic_error("invalid typed GFM table alignment");
            const unsigned char align =
                static_cast<unsigned char>(alignment[row_columns[row_id]++]);
            elem = staged_create(result, "div", limit);
            elem->add_class("md-td");
            const char* value = nullptr;
            switch (align) {
            case STUN_UI_TABLE_ALIGN_NONE: break;
            case STUN_UI_TABLE_ALIGN_LEFT: value = "left"; break;
            case STUN_UI_TABLE_ALIGN_CENTER: value = "center"; break;
            case STUN_UI_TABLE_ALIGN_RIGHT: value = "right"; break;
            default: throw std::logic_error("unknown typed GFM table alignment");
            }
            if (value) {
                elem->add_class(std::string("md-align-") + value);
                elem->set_attribute("data-gfm-align", value);
            }
            break;
        }
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
            code_languages[i] = fence_language(view, i);
            elem = staged_create(result, "div", limit);
            elem->add_class("md-code-block");
            if (!code_languages[i].empty())
                elem->add_class("lang-" + code_languages[i]);
            break;
        case STUN_UI_BLOCK_CODE_INDENTED:
            elem = staged_create(result, "div", limit);
            elem->add_class("md-code-block");
            break;
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
    if (!needs_rebuild_ || !elem.owner_box_ || rebuilding_) return;
    // Focus-out during retirement can synchronously invoke application
    // callbacks. Defer any reentrant update to the next outer UI update.
    rebuilding_ = true;
    struct RebuildExit {
        bool& flag;
        ~RebuildExit() { flag = false; }
    } reset{rebuilding_};
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
