/*
 * flexUI MarkdownWidget -- one Stun C11 RichDoc grammar authority.
 *
 * Text/ownership/semantic metadata are projected from a single
 * stun_md_parse() result. No md::parse(), HTML reparsing, compatibility
 * grammar, locale Unicode table or persistent borrowed RichDoc pointer.
 *
 * Changes are prepared against immutable C11 IR and only published as a
 * complete widget tree. Source-size/node limits fail closed.
 */
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/syntax_highlighter.h>
#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flexUI {
namespace {

constexpr size_t kMaxMarkdownBytes = 1024u * 1024u;
constexpr size_t kMaxRichNodes = 4096u;

using OwnedRichDoc = std::unique_ptr<stun_md_document,
                                     decltype(&stun_md_document_destroy)>;

struct PreparedElement {
    uint32_t owner = STUN_UI_PARENT_NONE;
    std::string tag;
    std::vector<std::string> classes;
    std::string text;
    std::string language;
    std::string code;
    std::string alignment;
    uint32_t ordered_start = 0u;
};

bool decoded_text(const stun_ui_rich_document_view& v,
                  const stun_ui_block& node, std::string* output) {
    if (!output || node.byte_begin > node.byte_end ||
        node.byte_end > v.document.source_bytes ||
        (!v.document.source && node.byte_end != node.byte_begin))
        return false;
    const size_t n = node.byte_end - node.byte_begin;
    output->assign(n ? v.document.source + node.byte_begin : "", n);
    return true;
}

bool metadata(const stun_ui_rich_document_view& v, size_t i,
              std::string* output) {
    const char* data = nullptr;
    size_t length = 0u;
    if (!output || stun_ui_rich_document_metadata(&v, i, &data, &length) !=
        STUN_UI_OK || (!data && length))
        return false;
    output->assign(length ? data : "", length);
    return true;
}

bool harmless_class_token(const std::string& token) {
    if (token.empty() || token.size() > 48u) return false;
    for (unsigned char c : token) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '+'))
            return false;
    }
    return true;
}

/* A detached, presentation-only C++ plan; it never stores borrowed RichDoc
 * pointers, never reparses Markdown or HTML, and is bounded by max_nodes. */
bool prepare(const stun_ui_rich_document_view& view,
             std::vector<PreparedElement>* plan) {
    if (!plan || view.document.block_count == 0u ||
        stun_ui_rich_document_validate(&view, kMaxRichNodes) != STUN_UI_OK)
        return false;

    /* In addition to the structural validation, use the canonical SAFE
     * C11 writer's semantic-owner and URI policy admission. Measuring
     * never executes HTML or exposes an unsafe string to the DOM. */
    size_t required = 0u;
    if (stun_md_render_html(&view, kMaxRichNodes, nullptr, 0u, &required) !=
        STUN_UI_OK)
        return false;

    const auto& d = view.document;
    if (d.blocks[0].kind != STUN_UI_BLOCK_DOCUMENT) return false;
    plan->assign(d.block_count, PreparedElement{});
    std::vector<size_t> direct_children(d.block_count, 0u);
    std::vector<size_t> row_column(d.block_count, 0u);
    std::vector<std::string> table_columns(d.block_count);
    for (size_t i=1u; i<d.block_count; ++i) {
        const uint32_t parent = d.blocks[i].parent_index;
        if (parent >= i) return false;
        if (d.blocks[i].kind != STUN_UI_BLOCK_IMAGE_TITLE &&
            d.blocks[i].kind != STUN_UI_BLOCK_LINK_TITLE)
            ++direct_children[parent];
    }

    for (size_t i=1u; i<d.block_count; ++i) {
        const stun_ui_block& node = d.blocks[i];
        const uint32_t parent = node.parent_index;
        const auto parent_kind = d.blocks[parent].kind;
        PreparedElement& e = (*plan)[i];
        e.owner = parent;
        switch (node.kind) {
        case STUN_UI_BLOCK_PARAGRAPH:
            e.tag="div"; e.classes={"md-p"}; break;
        case STUN_UI_BLOCK_HEADING:
            if (node.heading_level < 1u || node.heading_level > 6u)
                return false;
            e.tag="h"+std::to_string(node.heading_level);
            e.classes={"md-header", e.tag};
            break;
        case STUN_UI_BLOCK_LIST:
            e.tag="div"; e.classes={"md-ul"}; break;
        case STUN_UI_BLOCK_ORDERED_LIST: {
            e.tag="div"; e.classes={"md-ol"};
            std::string payload;
            if (!metadata(view,i,&payload)) return false;
            if (!payload.empty()) {
                if (payload.size()!=sizeof(stun_ui_list_semantics))
                    return false;
                stun_ui_list_semantics info{};
                std::memcpy(&info, payload.data(), sizeof(info));
                if (info.loose>1u || info.reserved[0] || info.reserved[1] ||
                    info.start>999999999u || (info.marker!='.' && info.marker!=')'))
                    return false;
                e.ordered_start=info.start;
            } else e.ordered_start=1u;
            break;
        }
        case STUN_UI_BLOCK_LIST_ITEM:
            e.tag="div"; e.classes={"md-li"}; break;
        case STUN_UI_BLOCK_TASK_ITEM:
            e.tag="div"; e.classes={"md-li","md-task-item"}; break;
        case STUN_UI_BLOCK_QUOTE:
            e.tag="div"; e.classes={"md-quote"}; break;
        case STUN_UI_BLOCK_TABLE: {
            e.tag="div"; e.classes={"md-table"};
            if (!metadata(view,i,&table_columns[i]) ||
                table_columns[i].empty()) return false;
            for (unsigned char align : table_columns[i]) {
                if (align > STUN_UI_TABLE_ALIGN_RIGHT) return false;
            }
            break;
        }
        case STUN_UI_BLOCK_TABLE_ROW:
            e.tag="div"; e.classes={"md-tr"}; break;
        case STUN_UI_BLOCK_TABLE_CELL: {
            e.tag="div"; e.classes={"md-td"};
            if (parent_kind!=STUN_UI_BLOCK_TABLE_ROW) return false;
            const uint32_t table_id=d.blocks[parent].parent_index;
            if (table_id>=parent || table_columns[table_id].empty())
                return false;
            const size_t column=row_column[parent]++;
            if (column>=table_columns[table_id].size())return false;
            const unsigned char align=(unsigned char)table_columns[table_id][column];
            static constexpr const char* labels[]={"", "left", "center", "right"};
            if (align) {
                e.alignment=labels[align];
                e.classes.push_back(std::string("md-align-")+e.alignment);
            }
            break;
        }
        case STUN_UI_BLOCK_TEXT: {
            if (!decoded_text(view,node,&e.text)) return false;
            if (parent_kind == STUN_UI_BLOCK_CODE_FENCE ||
                parent_kind == STUN_UI_BLOCK_CODE_INDENTED) {
                (*plan)[parent].code.append(e.text);
                e.text.clear();
                break;
            }
            /* A single semantic TEXT child is the parent Element's text,
             * matching the existing heading/paragraph styling contract.
             * With mixed inline nodes, keep a real span in source order. */
            switch (parent_kind) {
            case STUN_UI_BLOCK_PARAGRAPH:
            case STUN_UI_BLOCK_HEADING:
            case STUN_UI_BLOCK_TABLE_CELL:
            case STUN_UI_BLOCK_EMPHASIS:
            case STUN_UI_BLOCK_STRONG:
            case STUN_UI_BLOCK_CODE_SPAN:
            case STUN_UI_BLOCK_LINK:
            case STUN_UI_BLOCK_IMAGE:
            case STUN_UI_BLOCK_STRIKETHROUGH:
            case STUN_UI_BLOCK_UNDERLINE:
                if (direct_children[parent]==1u) {
                    (*plan)[parent].text=std::move(e.text);
                    break;
                }
                [[fallthrough]];
            default:
                e.tag="span";
                break;
            }
            break;
        }
        case STUN_UI_BLOCK_CHECKBOX: {
            e.tag="span"; e.classes={"md-checkbox"};
            if (parent_kind!=STUN_UI_BLOCK_TASK_ITEM) return false;
            std::string info;
            if (!metadata(view,i,&info) || info.size()!=1u ||
                (info[0]!='x' && info[0]!=' '))return false;
            e.text=info[0]=='x'?"[x] ":"[ ] ";
            break;
        }
        case STUN_UI_BLOCK_EMPHASIS:
            e.tag="span"; e.classes={"md-em"}; break;
        case STUN_UI_BLOCK_STRONG:
            e.tag="span"; e.classes={"md-strong"}; break;
        case STUN_UI_BLOCK_STRIKETHROUGH:
            e.tag="span"; e.classes={"md-del"}; break;
        case STUN_UI_BLOCK_UNDERLINE:
            e.tag="span"; e.classes={"md-u"}; break;
        case STUN_UI_BLOCK_LINK:
            e.tag="span"; e.classes={"md-a"}; break;
        case STUN_UI_BLOCK_IMAGE:
            e.tag="span"; e.classes={"md-img"}; break;
        case STUN_UI_BLOCK_LINK_TITLE:
        case STUN_UI_BLOCK_IMAGE_TITLE:
            /* Metadata-only child, never a second visible node. */
            break;
        case STUN_UI_BLOCK_CODE_FENCE: {
            e.tag="div"; e.classes={"md-code-block"};
            std::string info;
            if (!metadata(view,i,&info))return false;
            size_t n=info.find_first_of(" \t");
            if (n==std::string::npos)n=info.size();
            if (n && harmless_class_token(info.substr(0,n))) {
                e.language=info.substr(0,n);
                e.classes.push_back("lang-"+e.language);
            }
            break;
        }
        case STUN_UI_BLOCK_CODE_INDENTED:
            e.tag="div"; e.classes={"md-code-block"}; break;
        case STUN_UI_BLOCK_CODE_SPAN:
            e.tag="span"; e.classes={"md-code"}; break;
        case STUN_UI_BLOCK_RULE:
            e.tag="div"; e.classes={"md-hr"}; break;
        case STUN_UI_BLOCK_LINE_BREAK:
        case STUN_UI_BLOCK_HARD_LINE_BREAK:
            e.tag="div"; e.classes={"md-br"}; break;
        case STUN_UI_BLOCK_RAW_HTML_INLINE:
        case STUN_UI_BLOCK_RAW_HTML_BLOCK:
            e.tag="span"; e.classes={"md-raw-html"};
            if (!decoded_text(view,node,&e.text))return false;
            break;
        case STUN_UI_BLOCK_URI_AUTOLINK:
            e.tag="span"; e.classes={"md-uri-autolink"};
            if (!decoded_text(view,node,&e.text))return false;
            e.text="<"+e.text+">";
            break;
        case STUN_UI_BLOCK_ENTITY:
            e.tag="span"; e.classes={"md-entity"};
            if (!decoded_text(view,node,&e.text))return false;
            break;
        default:
            /* No syntax degradation, fallback C++ grammar, implicit HTML,
             * unreviewed renderer kind or invisible ignored node. */
            return false;
        }
    }
    return true;
}

/* Keep C++ syntax highlighting as a rendering step over C11-owned code
 * TEXT bytes. It is not a Markdown parser and cannot change RichDoc owners. */
void append_code_lines(Box& box, Element& parent,
                       const std::string& code, const std::string& language) {
    SyntaxHighlighter* highlighter=language.empty()?nullptr:
        HighlighterFactory::instance().get(language);
    if (!highlighter) {
        size_t offset=0u;
        while (offset<code.size()) {
            size_t end=code.find('\n',offset);
            if(end==std::string::npos)end=code.size();
            auto* line=box.create("div");
            line->add_class("md-code-line");
            line->set_text(code.substr(offset,end-offset));
            parent.append(line);
            offset=end+1u;
        }
        return;
    }
    const auto tokens=highlighter->highlight(code);
    auto* line=box.create("div");
    line->add_class("md-code-line");
    for(const auto& token: tokens) {
        if(token.start>code.size() || token.length>code.size()-token.start)
            throw std::invalid_argument("highlighter produced invalid code byte span");
        const std::string_view slice(code.data()+token.start,token.length);
        size_t at=0u;
        while(at<slice.size()) {
            const size_t next=slice.find('\n',at);
            const size_t stop=next==std::string_view::npos?slice.size():next;
            if(stop>at) {
                auto* span=box.create("span");
                span->add_class(highlight_class(token.type));
                span->set_text(std::string(slice.substr(at,stop-at)));
                line->append(span);
            }
            if(next==std::string_view::npos)break;
            parent.append(line);
            line=box.create("div");
            line->add_class("md-code-line");
            at=stop+1u;
        }
    }
    if(!line->children().empty() || parent.child_count()==0u)
        parent.append(line);
}

void publish(const std::vector<PreparedElement>& plan, Box& box, Element& host) {
    std::vector<Element*> elements(plan.size(),nullptr);
    std::vector<Element*> roots;
    roots.reserve(plan.size());
    for(size_t i=1u;i<plan.size();++i) {
        const PreparedElement& p=plan[i];
        if(p.tag.empty())continue;
        Element* node=box.create(p.tag);
        for(const std::string& cls:p.classes) node->add_class(cls);
        if(!p.text.empty())node->set_text(p.text);
        if(!p.alignment.empty())node->set_attribute("data-md-align",p.alignment);
        if(p.ordered_start)node->set_attribute("start",std::to_string(p.ordered_start));
        elements[i]=node;
        if(p.owner==0u)roots.push_back(node);
        else if(p.owner<elements.size() && elements[p.owner]!=nullptr) {
            if(elements[p.owner]->append(node)!=node)
                throw std::invalid_argument("failed to attach single-owner RichDoc projection");
        } else throw std::invalid_argument("RichDoc parent has no projected Element");
        if(!p.code.empty() || (!p.language.empty() && p.code.empty()))
            append_code_lines(box,*node,p.code,p.language);
    }
    /* First public mutation of the existing widget: all nodes are built,
     * owned by this Box, and checked as a complete detached forest. */
    host.replace_children(roots);
    host.set_text("");
}

} // namespace

MarkdownWidget::MarkdownWidget(const std::string& markdown)
    : markdown_(markdown) {}

void MarkdownWidget::set_markdown(const std::string& markdown) {
    if(markdown_==markdown)return;
    markdown_=markdown;
    needs_rebuild_=true;
}

void MarkdownWidget::rebuild_elements(Element& elem) {
    if(!needs_rebuild_ || !elem.owner_box_)return;
    if(elem.computed_style)
        elem.computed_style->flex_direction=FlexDirection::Column;

    /* No old grammar and no silent "best effort" tree on malformed,
     * unsupported or resource-exhausting input. Never expose a dangling
     * borrowed RichDoc: a plan copies semantic bytes before destruction. */
    bool accepted=false;
    if(markdown_.size()<=kMaxMarkdownBytes) {
        stun_md_document* raw=nullptr;
        const stun_ui_status parsed=stun_md_parse(
            markdown_.data(),markdown_.size(),kMaxRichNodes,
            kMaxMarkdownBytes,&raw);
        OwnedRichDoc owner(raw,stun_md_document_destroy);
        if(parsed==STUN_UI_OK && owner) {
            const auto view=stun_md_document_view(owner.get());
            std::vector<PreparedElement> plan;
            if(prepare(view,&plan)) {
                publish(plan,*elem.owner_box_,elem);
                accepted=true;
            }
        }
    }
    if(!accepted) {
        /* Fail-closed: a rejected source must not leave the *previous*
         * document visible as if it matched MarkdownWidget::markdown(). */
        elem.replace_children({});
        elem.set_text("");
    }
    needs_rebuild_=false;
    elem.mark_style_dirty();
}

void MarkdownWidget::emit_render_commands(const Element&,
                                          RenderCommandList&) {
    // The one owner Element tree is rendered through RenderManager.
}

bool MarkdownWidget::handle_event(const Event&, Element&) {
    return false;
}

void MarkdownWidget::update(float, Element& elem) {
    rebuild_elements(elem);
}

} // namespace flexUI
