#include <stun/markdown_html.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum { STUN_HTML_MAX_DEPTH = 64 };

typedef struct stun_html_writer {
    const stun_ui_rich_document_view *doc;
    char *out;
    size_t length;
    size_t capacity;
    stun_ui_status error;
} stun_html_writer;

static void html_bytes(stun_html_writer *w, const char *bytes, size_t len) {
    if (w->error != STUN_UI_OK) return;
    if ((!bytes && len) || len > SIZE_MAX - w->length) {
        w->error = STUN_UI_CAPACITY_EXCEEDED;
        return;
    }
    if (w->out) {
        if (len > w->capacity - w->length) {
            w->error = STUN_UI_CAPACITY_EXCEEDED;
            return;
        }
        if (len) memcpy(w->out + w->length, bytes, len);
    }
    w->length += len;
}
static void html_str(stun_html_writer *w, const char *str) {
    html_bytes(w,str,strlen(str));
}

static void html_escaped(stun_html_writer *w,
                         const char *s, size_t len, bool attribute) {
    if (!s && len) { w->error = STUN_UI_INVALID_ARGUMENT; return; }
    for (size_t i=0;i<len && w->error==STUN_UI_OK;++i) {
        switch(s[i]) {
        case '&': html_str(w,"&amp;"); break;
        case '<': html_str(w,"&lt;"); break;
        case '>': html_str(w,"&gt;"); break;
        case '"':
            /* cmark's HTML text contract escapes literal double quotes in
             * text and code as well as in attributes (CommonMark #352).
             * The same bounded writer/measurement path owns all output. */
            html_str(w,"&quot;");
            break;
        default: html_bytes(w,s+i,1u); break;
        }
    }
}

static bool html_ascii_equal(const char *bytes, size_t len, const char *key) {
    const size_t n = strlen(key);
    if (n != len) return false;
    for (size_t i=0;i<n;++i) {
        const unsigned char c=(unsigned char)bytes[i];
        const unsigned char k=(unsigned char)key[i];
        const unsigned char low=(c>='A' && c<='Z') ? c-'A'+'a' : c;
        if(low!=k) return false;
    }
    return true;
}

/* Treat an explicit URL scheme as a capability requiring admission. Reject
 * whitespace, raw control characters and unknown schemes; never emit a
 * javascript:, data: or vbscript: URL into an HTML attribute. */
static bool html_safe_href(const char *s, size_t n) {
    if (!s && n) return false;
    for (size_t i=0;i<n;++i) {
        const unsigned char c=(unsigned char)s[i];
        if (c<=0x20 || c==0x7f) return false;
        if (c=='/' || c=='?' || c=='#') return true;
        if (c==':') return
            html_ascii_equal(s,i,"https") ||
            html_ascii_equal(s,i,"http") ||
            html_ascii_equal(s,i,"mailto");
    }
    return true; /* Relative link, or fragment-only handled above. */
}

static bool html_metadata(stun_html_writer *w, size_t id,
                          const char **s, size_t *n) {
    if (stun_ui_rich_document_metadata(w->doc,id,s,n)!=STUN_UI_OK) {
        w->error = STUN_UI_INVALID_ARGUMENT;
        return false;
    }
    return true;
}

static bool html_list_meta(stun_html_writer *w,size_t id,
                           stun_ui_list_semantics *info) {
    const stun_ui_block *node=&w->doc->document.blocks[id];
    const char *data=NULL;size_t size=0;
    *info=(stun_ui_list_semantics){0};
    info->start=node->kind==STUN_UI_BLOCK_ORDERED_LIST?1u:0u;
    if(!html_metadata(w,id,&data,&size)) return false;
    if(!size) return true; /* existing projected tight lists */
    if(size!=sizeof(*info)) {w->error=STUN_UI_INVALID_ARGUMENT;return false;}
    memcpy(info,data,sizeof(*info));
    const bool ordered=node->kind==STUN_UI_BLOCK_ORDERED_LIST;
    if(info->loose>1u||info->reserved[0]||info->reserved[1]||
       (ordered ? (info->start>999999999u||
                   (info->marker!='.'&&info->marker!=')'))
                : (info->start!=0u||(info->marker!='-'&&
                       info->marker!='+'&&info->marker!='*')))) {
        w->error=STUN_UI_INVALID_ARGUMENT;return false;
    }
    return true;
}
static bool html_loose_item(stun_html_writer *w,size_t id) {
    const uint32_t parent=w->doc->document.blocks[id].parent_index;
    stun_ui_list_semantics info;
    if(parent>=w->doc->document.block_count ||
       !html_list_meta(w,parent,&info)) {
        w->error=STUN_UI_INVALID_ARGUMENT;return false;
    }
    return info.loose!=0u;
}
static void html_number(stun_html_writer *w,uint32_t v) {
    char digits[10];size_t n=0;
    do {digits[n++]=(char)('0'+v%10u);v/=10u;} while(v);
    while(n) html_bytes(w,&digits[--n],1u);
}

static void html_text(stun_html_writer *w, const stun_ui_block *node) {
    if (node->byte_end < node->byte_begin ||
        node->byte_end > w->doc->document.source_bytes) {
        w->error = STUN_UI_INVALID_ARGUMENT;
        return;
    }
    const size_t length=node->byte_end-node->byte_begin;
    if (!w->doc->document.source) {
        if (length) w->error=STUN_UI_INVALID_ARGUMENT;
        return;
    }
    html_escaped(w,w->doc->document.source+node->byte_begin,
                 length,false);
}

static void html_node(stun_html_writer *w, size_t id,
                      size_t *next, size_t depth, bool table_header);

static void html_children(stun_html_writer *w, size_t id,
                          size_t *next, size_t depth, bool table_header) {
    size_t at=id+1u;
    const stun_ui_document_view *doc=&w->doc->document;
    while(w->error==STUN_UI_OK && at<doc->block_count &&
          doc->blocks[at].parent_index==id) {
        size_t after=at;
        html_node(w,at,&after,depth+1u,table_header);
        if(w->error!=STUN_UI_OK || after<=at) {
            if(w->error==STUN_UI_OK) w->error=STUN_UI_INVALID_ARGUMENT;
            break;
        }
        at=after;
    }
    *next=at;
}

static void html_table(stun_html_writer *w, size_t id,
                       size_t *next, size_t depth) {
    const stun_ui_document_view *doc=&w->doc->document;
    size_t at=id+1u;
    if (at>=doc->block_count || doc->blocks[at].parent_index!=id ||
        doc->blocks[at].kind!=STUN_UI_BLOCK_TABLE_ROW) {
        w->error=STUN_UI_INVALID_ARGUMENT;
        return;
    }
    html_str(w,"<table>\n<thead>\n");
    size_t after=at;
    html_node(w,at,&after,depth+1u,true);
    at=after;
    html_str(w,"</thead>\n");
    bool has_body=false;
    while(w->error==STUN_UI_OK && at<doc->block_count &&
          doc->blocks[at].parent_index==id) {
        if(doc->blocks[at].kind!=STUN_UI_BLOCK_TABLE_ROW) {
            w->error=STUN_UI_INVALID_ARGUMENT;
            break;
        }
        if(!has_body) { html_str(w,"<tbody>\n"); has_body=true; }
        html_node(w,at,&after,depth+1u,false);
        if(after<=at) { w->error=STUN_UI_INVALID_ARGUMENT; break; }
        at=after;
    }
    if(has_body) html_str(w,"</tbody>\n");
    html_str(w,"</table>\n");
    *next=at;
}

static void html_node(stun_html_writer *w, size_t id,
                      size_t *next, size_t depth, bool table_header) {
    const stun_ui_document_view *doc=&w->doc->document;
    if (w->error!=STUN_UI_OK) return;
    if (id>=doc->block_count || depth>STUN_HTML_MAX_DEPTH) {
        w->error=STUN_UI_CAPACITY_EXCEEDED;
        return;
    }
    const stun_ui_block *node=&doc->blocks[id];
    const stun_ui_block_kind kind=node->kind;
    if(kind==STUN_UI_BLOCK_TABLE) {
        html_table(w,id,next,depth);
        return;
    }

    const char *meta=NULL;
    size_t meta_length=0;
    switch(kind) {
    case STUN_UI_BLOCK_DOCUMENT: break;
    case STUN_UI_BLOCK_PARAGRAPH: {
        const uint32_t parent=node->parent_index;
        if(parent==0u || (doc->blocks[parent].kind!=STUN_UI_BLOCK_LIST_ITEM &&
                          doc->blocks[parent].kind!=STUN_UI_BLOCK_TASK_ITEM) ||
           html_loose_item(w,parent)) html_str(w,"<p>");
        break;
    }
    case STUN_UI_BLOCK_HEADING:
        if(node->heading_level<1 || node->heading_level>6) {
            w->error=STUN_UI_INVALID_ARGUMENT; return;
        }
        html_str(w,"<h");
        { char digit=(char)('0'+node->heading_level); html_bytes(w,&digit,1u); }
        html_str(w,">");
        break;
    case STUN_UI_BLOCK_TEXT:
        html_text(w,node);
        if(id+1u<doc->block_count && doc->blocks[id+1u].parent_index==id)
            w->error=STUN_UI_INVALID_ARGUMENT;
        *next=id+1u;
        return;
    case STUN_UI_BLOCK_EMPHASIS: html_str(w,"<em>"); break;
    case STUN_UI_BLOCK_STRONG: html_str(w,"<strong>"); break;
    case STUN_UI_BLOCK_STRIKETHROUGH: html_str(w,"<del>"); break;
    case STUN_UI_BLOCK_CODE_SPAN: html_str(w,"<code>"); break;
    case STUN_UI_BLOCK_LINK:
        if (!html_metadata(w,id,&meta,&meta_length)) return;
        if(!html_safe_href(meta,meta_length)) {
            w->error=STUN_UI_INVALID_ARGUMENT; return;
        }
        html_str(w,"<a href=\"");
        html_escaped(w,meta,meta_length,true);
        html_str(w,"\">");
        break;
    case STUN_UI_BLOCK_CODE_INDENTED:
        html_str(w,"<pre><code>");
        break;
    case STUN_UI_BLOCK_CODE_FENCE:
        if(!html_metadata(w,id,&meta,&meta_length)) return;
        html_str(w,"<pre><code");
        if(meta_length) {
            size_t lang=0;
            while(lang<meta_length && meta[lang]!=' ' && meta[lang]!='\t')
                ++lang;
            if(lang) {
                html_str(w," class=\"language-");
                html_escaped(w,meta,lang,true);
                html_str(w,"\"");
            }
        }
        html_str(w,">");
        break;
    case STUN_UI_BLOCK_LIST: {
        stun_ui_list_semantics info;
        if(!html_list_meta(w,id,&info)) return;
        html_str(w,"<ul>\n");break;
    }
    case STUN_UI_BLOCK_ORDERED_LIST: {
        stun_ui_list_semantics info;
        if(!html_list_meta(w,id,&info)) return;
        html_str(w,"<ol");
        if(info.start!=1u) {
            html_str(w," start=\"");
            html_number(w,info.start);
            html_str(w,"\"");
        }
        html_str(w,">\n");break;
    }
    case STUN_UI_BLOCK_LIST_ITEM: {
        html_str(w,"<li>");
        /* A loose list does not give an empty item a phantom newline.
         * A direct list or code child, however, starts on a new HTML line
         * even when this outer list is tight (CommonMark #298/#299). */
        const bool has_child=id+1u<doc->block_count &&
                             doc->blocks[id+1u].parent_index==id;
        if(has_child &&
           (html_loose_item(w,id) ||
            doc->blocks[id+1u].kind==STUN_UI_BLOCK_CODE_FENCE ||
            doc->blocks[id+1u].kind==STUN_UI_BLOCK_CODE_INDENTED ||
            doc->blocks[id+1u].kind==STUN_UI_BLOCK_HEADING ||
            doc->blocks[id+1u].kind==STUN_UI_BLOCK_QUOTE ||
            doc->blocks[id+1u].kind==STUN_UI_BLOCK_LIST ||
            doc->blocks[id+1u].kind==STUN_UI_BLOCK_ORDERED_LIST))
            html_str(w,"\n");
        break;
    }
    case STUN_UI_BLOCK_QUOTE: html_str(w,"<blockquote>\n"); break;
    case STUN_UI_BLOCK_TABLE_ROW: html_str(w,"<tr>\n"); break;
    case STUN_UI_BLOCK_TABLE_CELL:
        html_str(w,table_header?"<th>":"<td>");
        break;
    case STUN_UI_BLOCK_RULE:
        html_str(w,"<hr />\n");
        *next=id+1u; return;
    case STUN_UI_BLOCK_LINE_BREAK:
        html_str(w,"\n"); /* soft line break; hard break IR is not yet distinct */
        *next=id+1u; return;
    default:
        /* No silent HTML generation for unsupported Markdown semantics.
         * In particular: tasks, images, math, diagrams, entities, etc. */
        w->error=STUN_UI_INVALID_ARGUMENT;
        return;
    }

    size_t child_end=id+1u;
    html_children(w,id,&child_end,depth,table_header);
    if (w->error!=STUN_UI_OK) return;

    switch(kind) {
    case STUN_UI_BLOCK_DOCUMENT: break;
    case STUN_UI_BLOCK_PARAGRAPH: {
        const uint32_t parent=node->parent_index;
        if(parent==0u || (doc->blocks[parent].kind!=STUN_UI_BLOCK_LIST_ITEM &&
                          doc->blocks[parent].kind!=STUN_UI_BLOCK_TASK_ITEM) ||
           html_loose_item(w,parent)) html_str(w,"</p>\n");
        else if(child_end<doc->block_count &&
                doc->blocks[child_end].parent_index==parent)
            html_str(w,"\n");
        break;
    }
    case STUN_UI_BLOCK_HEADING:
        html_str(w,"</h");
        { char digit=(char)('0'+node->heading_level); html_bytes(w,&digit,1u); }
        html_str(w,">\n");
        break;
    case STUN_UI_BLOCK_EMPHASIS: html_str(w,"</em>"); break;
    case STUN_UI_BLOCK_STRONG: html_str(w,"</strong>"); break;
    case STUN_UI_BLOCK_STRIKETHROUGH: html_str(w,"</del>"); break;
    case STUN_UI_BLOCK_CODE_SPAN: html_str(w,"</code>"); break;
    case STUN_UI_BLOCK_LINK: html_str(w,"</a>"); break;
    case STUN_UI_BLOCK_CODE_FENCE:
    case STUN_UI_BLOCK_CODE_INDENTED: html_str(w,"</code></pre>\n"); break;
    case STUN_UI_BLOCK_LIST: html_str(w,"</ul>\n"); break;
    case STUN_UI_BLOCK_ORDERED_LIST: html_str(w,"</ol>\n"); break;
    case STUN_UI_BLOCK_LIST_ITEM: html_str(w,"</li>\n"); break;
    case STUN_UI_BLOCK_QUOTE: html_str(w,"</blockquote>\n"); break;
    case STUN_UI_BLOCK_TABLE_ROW: html_str(w,"</tr>\n"); break;
    case STUN_UI_BLOCK_TABLE_CELL:
        html_str(w,table_header?"</th>\n":"</td>\n");
        break;
    default: w->error=STUN_UI_INVALID_ARGUMENT; return;
    }
    *next=child_end;
}

stun_ui_status stun_md_render_html(const stun_ui_rich_document_view *view,
                                   size_t max_nodes,
                                   char *buffer, size_t capacity,
                                   size_t *required) {
    if(!required || !view || (!buffer && capacity!=0u))
        return STUN_UI_INVALID_ARGUMENT;
    const stun_ui_status valid=stun_ui_rich_document_validate(view,max_nodes);
    if(valid!=STUN_UI_OK) return valid;

    /* Dry-run first: validation and exact byte count before any write. */
    stun_html_writer measuring={view,NULL,0u,0u,STUN_UI_OK};
    size_t next=0u;
    html_node(&measuring,0u,&next,0u,false);
    if(measuring.error!=STUN_UI_OK) return measuring.error;
    if(next!=view->document.block_count)
        return STUN_UI_INVALID_ARGUMENT;

    *required=measuring.length;
    if(!buffer) return STUN_UI_OK;
    if(measuring.length==SIZE_MAX || capacity<=measuring.length)
        return STUN_UI_CAPACITY_EXCEEDED;

    stun_html_writer writing={view,buffer,0u,capacity-1u,STUN_UI_OK};
    html_node(&writing,0u,&next,0u,false);
    if(writing.error!=STUN_UI_OK || writing.length!=measuring.length ||
       next!=view->document.block_count)
        return STUN_UI_INVALID_ARGUMENT;
    buffer[writing.length]='\0';
    return STUN_UI_OK;
}
