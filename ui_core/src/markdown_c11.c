#include <stun/markdown_c11.h>

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The input's exact bytes are copied once and never rewritten. The parser
 * emits semantic text spans and metadata into two distinct owned buffers.
 * All RichDoc views borrow this document until stun_md_document_destroy().
 */
struct stun_md_document {
    char *raw;
    size_t raw_size;
    char *text;
    size_t text_size;
    char *metadata;
    size_t metadata_size;
    stun_ui_block *nodes;
    stun_ui_block_metadata *node_metadata;
    size_t node_count;
    size_t capacity;
    size_t node_limit;
    stun_ui_status error;
};

typedef struct md_line {
    size_t begin;
    size_t end;
    size_t next;
    bool newline;
} md_line;

typedef struct md_fence {
    char marker;
    size_t count;
    uint32_t node;
} md_fence;

static bool md_space(char c) { return c == ' ' || c == '\t'; }
static bool md_ascii_punct(char c) {
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') ||
           (c >= '[' && c <= (char)0x60) || (c >= '{' && c <= '~');
}

static md_line md_read_line(const stun_md_document *d, size_t pos) {
    md_line line = {pos, pos, pos, false};
    while (line.end < d->raw_size && d->raw[line.end] != '\n')
        ++line.end;
    line.newline = line.end < d->raw_size;
    line.next = line.end + (line.newline ? 1u : 0u);
    if (line.end > pos && d->raw[line.end - 1] == '\r') --line.end;
    return line;
}

static size_t md_left(const char *s, size_t b, size_t e) {
    while (b < e && md_space(s[b])) ++b;
    return b;
}
static size_t md_right(const char *s, size_t b, size_t e) {
    while (e > b && md_space(s[e-1])) --e;
    return e;
}
static bool md_blank(const char *s, size_t b, size_t e) {
    return md_left(s,b,e) == e;
}

static void md_error(stun_md_document *d, stun_ui_status error) {
    if (d->error == STUN_UI_OK) d->error = error;
}

static bool md_grow(stun_md_document *d) {
    if (d->node_count < d->capacity) return true;
    if (d->node_count >= d->node_limit) {
        md_error(d, STUN_UI_CAPACITY_EXCEEDED);
        return false;
    }
    size_t next = d->capacity ? d->capacity * 2 : 32;
    if (next < d->capacity || next > d->node_limit) next = d->node_limit;
    if (next > SIZE_MAX / sizeof(stun_ui_block) ||
        next > SIZE_MAX / sizeof(stun_ui_block_metadata)) {
        md_error(d, STUN_UI_CAPACITY_EXCEEDED);
        return false;
    }
    stun_ui_block *nodes = (stun_ui_block *)realloc(
        d->nodes, next * sizeof(*nodes));
    if (!nodes) {
        md_error(d, STUN_UI_OUT_OF_MEMORY);
        return false;
    }
    d->nodes = nodes;
    stun_ui_block_metadata *meta = (stun_ui_block_metadata *)realloc(
        d->node_metadata, next * sizeof(*meta));
    if (!meta) {
        md_error(d, STUN_UI_OUT_OF_MEMORY);
        return false;
    }
    d->node_metadata = meta;
    d->capacity = next;
    return true;
}

static uint32_t md_add(stun_md_document *d, uint32_t parent,
                       stun_ui_block_kind kind, uint32_t level) {
    if (d->error != STUN_UI_OK || !md_grow(d)) return UINT32_MAX;
    const uint32_t i = (uint32_t)d->node_count++;
    d->nodes[i] = (stun_ui_block){
        .stable_id = (uint64_t)i + 1u, .parent_index = parent,
        .kind = kind, .heading_level = level,
        .byte_begin = 0, .byte_end = 0
    };
    d->node_metadata[i] = (stun_ui_block_metadata){0,0};
    return i;
}

static bool md_store(stun_md_document *d, bool is_meta,
                     const char *s, size_t n, size_t *begin, size_t *end) {
    size_t *used = is_meta ? &d->metadata_size : &d->text_size;
    char *target = is_meta ? d->metadata : d->text;
    if (d->error != STUN_UI_OK) return false;
    if (n > d->raw_size - *used) {
        md_error(d, STUN_UI_CAPACITY_EXCEEDED);
        return false;
    }
    *begin = *used;
    if (n) memcpy(target + *used, s, n);
    *used += n;
    *end = *used;
    target[*used] = '\0';
    return true;
}

static void md_text(stun_md_document *d, uint32_t parent,
                    const char *s, size_t n) {
    if (!n || d->error != STUN_UI_OK) return;
    const uint32_t node = md_add(d, parent, STUN_UI_BLOCK_TEXT, 0);
    if (node == UINT32_MAX) return;
    size_t from = 0, to = 0;
    if (md_store(d, false, s, n, &from, &to)) {
        d->nodes[node].byte_begin = from;
        d->nodes[node].byte_end = to;
    }
}

static void md_meta(stun_md_document *d, uint32_t node,
                    const char *s, size_t n) {
    if (node == UINT32_MAX || !n) return;
    size_t from = 0, to = 0;
    if (md_store(d, true, s, n, &from, &to))
        d->node_metadata[node] = (stun_ui_block_metadata){from,to};
}


/* CommonMark tab stops are four *columns*, not four bytes. For a root-level
 * indented code line, consume precisely four indentation columns. A tab
 * advances to the next multiple of four and is never mistaken for one space.
 * Bytes after those four columns belong to literal code (including tabs). */
static size_t md_code_unindent(const char *s, size_t begin, size_t end) {
    size_t column=0u, at=begin;
    while(at<end && column<4u && md_space(s[at])) {
        column += s[at]=='\t' ? 4u-(column%4u) : 1u;
        ++at;
    }
    return column>=4u ? at : begin;
}

/* One text child per logical code line, with LF normalized at EOF/CRLF.
 * Code is not recursively parsed as inline Markdown; decoded text and the
 * original raw input remain separately owned by the same document. */
static bool md_code_line(stun_md_document *d, uint32_t parent,
                         md_line line, bool blank) {
    const char *s=d->raw;
    size_t content=md_code_unindent(s,line.begin,line.end);
    if (content==line.begin) {
        if (!blank) {
            md_error(d,STUN_UI_INVALID_ARGUMENT);
            return false;
        }
        content=line.end; /* blank lines indented less than four columns */
    }
    const uint32_t node=md_add(d,parent,STUN_UI_BLOCK_TEXT,0u);
    if(node==UINT32_MAX) return false;
    size_t from=0u,to=0u,lf_begin=0u,lf_end=0u;
    if(!md_store(d,false,s+content,line.end-content,&from,&to) ||
       !md_store(d,false,"\n",1u,&lf_begin,&lf_end))
        return false;
    d->nodes[node].byte_begin=from;
    d->nodes[node].byte_end=lf_end;
    return true;
}

/* This is a constrained, bounded first pass, not a claim of full CommonMark
 * delimiter-run or nested-link conformance. Unmatched delimiters are text. */
static size_t md_match(const char *s, size_t n, size_t start,
                       char symbol, size_t count) {
    for (size_t p = start; p < n; ++p) {
        if (s[p] == '\\' && p + 1 < n) { ++p; continue; }
        if (s[p] != symbol) continue;
        size_t run = 0;
        while (p + run < n && s[p + run] == symbol) ++run;
        if (run == count) return p;
        p += run - 1;
    }
    return SIZE_MAX;
}

static bool md_inline(stun_md_document *d, uint32_t parent,
                      const char *s, size_t n, unsigned depth) {
    if (depth > 32) {
        md_error(d, STUN_UI_CAPACITY_EXCEEDED);
        return false;
    }
    size_t p = 0, text_start = 0;
    while (p < n && d->error == STUN_UI_OK) {
        size_t opener = p, close = SIZE_MAX, after = 0;
        stun_ui_block_kind kind = STUN_UI_BLOCK_TEXT;
        size_t marker = 1;

        if (s[p] == '\\' && p + 1 < n && md_ascii_punct(s[p+1])) {
            md_text(d, parent, s+text_start, p-text_start);
            md_text(d, parent, s+p+1, 1);
            p += 2; text_start = p;
            continue;
        }
        if (s[p] == (char)0x60) {
            while (p + marker < n && s[p + marker] == (char)0x60) ++marker;
            close = md_match(s, n, p+marker, (char)0x60, marker);
            if (close != SIZE_MAX) kind = STUN_UI_BLOCK_CODE_SPAN;
        } else if (s[p] == '*' || s[p] == '_' || s[p] == '~') {
            if (s[p] == '~' && p + 1 < n && s[p+1] == '~') {
                marker = 2;
                kind = STUN_UI_BLOCK_STRIKETHROUGH;
            } else if (p + 1 < n && s[p+1] == s[p]) {
                marker = 2;
                kind = STUN_UI_BLOCK_STRONG;
            } else if (s[p] != '~') {
                kind = STUN_UI_BLOCK_EMPHASIS;
            }
            if (kind != STUN_UI_BLOCK_TEXT && p+marker < n &&
                !md_space(s[p+marker])) {
                close = md_match(s, n, p+marker, s[p], marker);
                if (close != SIZE_MAX && (close == p+marker ||
                    md_space(s[close-1]))) close = SIZE_MAX;
            }
        } else if (s[p] == '[' || (s[p] == '!' && p+1 < n && s[p+1] == '[')) {
            const bool image = s[p] == '!';
            const size_t b = p + (image ? 2 : 1);
            const size_t square = md_match(s,n,b,']',1);
            if (square != SIZE_MAX && square+1 < n && s[square+1] == '(') {
                const size_t round = md_match(s,n,square+2,')',1);
                if (round != SIZE_MAX) {
                    md_text(d,parent,s+text_start,p-text_start);
                    const uint32_t node = md_add(
                        d,parent,image ? STUN_UI_BLOCK_IMAGE : STUN_UI_BLOCK_LINK,0);
                    if (node == UINT32_MAX) return false;
                    md_meta(d,node,s+square+2,round-(square+2));
                    if (!md_inline(d,node,s+b,square-b,depth+1)) return false;
                    p = round + 1; text_start = p;
                    continue;
                }
            }
        }

        if (close == SIZE_MAX || kind == STUN_UI_BLOCK_TEXT) {
            ++p;
            continue;
        }
        md_text(d,parent,s+text_start,opener-text_start);
        const uint32_t node = md_add(d,parent,kind,0);
        if (node == UINT32_MAX) return false;
        if (kind == STUN_UI_BLOCK_CODE_SPAN)
            md_text(d,node,s+opener+marker,close-opener-marker);
        else if (!md_inline(d,node,s+opener+marker,close-opener-marker,depth+1))
            return false;
        after = close + marker;
        p = after;
        text_start = p;
    }
    if (d->error != STUN_UI_OK) return false;
    md_text(d,parent,s+text_start,n-text_start);
    return d->error == STUN_UI_OK;
}

static bool md_rule(const char *s, size_t b, size_t e) {
    char mark = 0;
    size_t count = 0;
    for (size_t p=b; p<e; ++p) {
        if (md_space(s[p])) continue;
        if (s[p] != '-' && s[p] != '*' && s[p] != '_') return false;
        if (mark && mark != s[p]) return false;
        mark = s[p]; ++count;
    }
    return count >= 3;
}

static unsigned md_setext(const char *s, size_t b, size_t e) {
    b = md_left(s,b,e); e = md_right(s,b,e);
    if (b == e || (s[b] != '=' && s[b] != '-')) return 0;
    const char marker = s[b];
    for (size_t p=b; p<e; ++p) if (s[p] != marker) return 0;
    return marker == '=' ? 1u : 2u;
}

static unsigned md_atx(const char *s, size_t *begin, size_t *end) {
    size_t p = *begin, n = 0;
    while (p < *end && s[p] == '#' && n < 7) { ++p; ++n; }
    if (n < 1 || n > 6 || (p < *end && !md_space(s[p]))) return 0;
    p = md_left(s,p,*end);
    size_t e = md_right(s,p,*end);
    /* Closing ATX markers require whitespace before the trailing # run. */
    size_t x = e;
    while (x > p && s[x-1] == '#') --x;
    if (x < e && (x == p || md_space(s[x-1])))
        e = md_right(s,p,x);
    *begin = p; *end = e;
    return (unsigned)n;
}

static bool md_fence_start(const char *s, size_t b, size_t e,
                           char *marker, size_t *count,
                           size_t *info_begin, size_t *info_end) {
    if (b >= e || (s[b] != (char)0x60 && s[b] != '~')) return false;
    const char ch = s[b];
    size_t p=b;
    while (p<e && s[p]==ch) ++p;
    if (p-b<3) return false;
    if (ch == (char)0x60)
        for (size_t q=p; q<e; ++q)
            if (s[q] == (char)0x60) return false;
    *marker=ch; *count=p-b;
    *info_begin=md_left(s,p,e);
    *info_end=md_right(s,*info_begin,e);
    return true;
}
static bool md_fence_close(const char *s, size_t b, size_t e,
                           md_fence fence) {
    size_t p=b;
    while (p<e && s[p]==fence.marker) ++p;
    if (p-b < fence.count) return false;
    return md_blank(s,p,e);
}

static bool md_list_marker(const char *s, size_t b, size_t e,
                           bool *ordered, size_t *content) {
    if (b>=e) return false;
    if (s[b]=='-' || s[b]=='+' || s[b]=='*') {
        if (b+1<e && !md_space(s[b+1])) return false;
        *ordered=false;
        *content=md_left(s,b+1,e);
        return true;
    }
    size_t p=b;
    while (p<e && p-b<10 && s[p]>='0' && s[p]<='9') ++p;
    if (p==b || p-b>9 || p>=e || (s[p]!='.' && s[p]!=')') ||
        (p+1<e && !md_space(s[p+1]))) return false;
    *ordered=true;
    *content=md_left(s,p+1,e);
    return true;
}

/* GFM task marker: optional leading spaces after the list marker were already
 * consumed by md_list_marker. A marker belongs only to the first paragraph
 * of a list item, not to arbitrary text or link syntax. */
static bool md_task_marker(const char *s, size_t start, size_t end,
                           char *state, size_t *content) {
    if(end-start<3 || s[start]!='[' || s[start+2]!=']') return false;
    const char mark=s[start+1];
    if(mark!=' ' && mark!='\t' && mark!='x' && mark!='X') return false;
    const size_t after=start+3u;
    if(after<end && !md_space(s[after])) return false;
    *state=(mark=='x'||mark=='X') ? 'x' : ' ';
    *content=md_left(s,after,end);
    return true;
}

/* GFM pipe-table subset. Escaped pipes and backtick code spans do not
 * separate cells. Header/delimiter column counts must agree.
 * A table is admitted only after two-line lookahead succeeds. */
static bool md_table_unescaped(const char *s, size_t pos, size_t begin) {
    size_t slashes = 0;
    for (size_t p=pos; p>begin && s[p-1]=='\\'; --p) ++slashes;
    return (slashes % 2u)==0u;
}

/* Skip a valid fenced inline-code run (including multi-backtick runs).
 * Unclosed runs are ordinary text. The caller processes the next candidate
 * separator after the returned byte. */
static size_t md_table_skip_code(const char *s, size_t begin,
                                 size_t end, size_t pos) {
    if (pos >= end || s[pos] != (char)0x60 ||
        !md_table_unescaped(s,pos,begin)) return pos;
    size_t run = 1;
    while (pos+run<end && s[pos+run]==(char)0x60) ++run;
    const size_t close=md_match(s,end,pos+run,(char)0x60,run);
    return close==SIZE_MAX ? pos+run-1 : close+run-1;
}

static bool md_table_delimiter_cell(const char *s, size_t begin, size_t end) {
    begin=md_left(s,begin,end);
    end=md_right(s,begin,end);
    if (begin<end && s[begin]==':') ++begin;
    if (end>begin && s[end-1]==':') --end;
    if (begin==end) return false;
    for (size_t i=begin;i<end;++i)
        if (s[i]!='-') return false;
    return true;
}

static bool md_table_scan(const char *s, size_t begin, size_t end,
                          bool delimiter, bool require_pipe,
                          size_t *out_columns) {
    if (begin>=end || !out_columns) return false;
    begin=md_left(s,begin,end);
    end=md_right(s,begin,end);
    if (begin>=end) return false;
    bool pipe=false;
    if (s[begin]=='|') { ++begin; pipe=true; }
    if (end>begin && s[end-1]=='|' &&
        md_table_unescaped(s,end-1,begin)) { --end; pipe=true; }
    if (begin>=end) {
        if (pipe && !delimiter) { *out_columns=1u; return true; }
        return false;
    }
    size_t cell_begin=begin, cells=0;
    for (size_t p=begin;p<=end;++p) {
        if(p<end && s[p]==(char)0x60) {
            p=md_table_skip_code(s,begin,end,p);
            continue;
        }
        const bool boundary = p==end ||
            (s[p]=='|' && md_table_unescaped(s,p,begin));
        if (!boundary) continue;
        pipe |= p<end;
        if (delimiter && !md_table_delimiter_cell(s,cell_begin,p))
            return false;
        ++cells;
        cell_begin=p+1;
    }
    *out_columns=cells;
    return cells>0 && (!require_pipe || pipe);
}

static bool md_table_row(stun_md_document *d, uint32_t table,
                         const char *s, size_t begin, size_t end,
                         size_t expected_columns) {
    const uint32_t row=md_add(d,table,STUN_UI_BLOCK_TABLE_ROW,0u);
    if (row==UINT32_MAX) return false;
    begin=md_left(s,begin,end);
    end=md_right(s,begin,end);
    if (begin<end && s[begin]=='|') ++begin;
    if (end>begin && s[end-1]=='|' &&
        md_table_unescaped(s,end-1,begin)) --end;
    size_t cell_begin=begin, cells=0;
    for(size_t p=begin;p<=end && d->error==STUN_UI_OK;++p) {
        if(p<end && s[p]==(char)0x60) {
            p=md_table_skip_code(s,begin,end,p);
            continue;
        }
        const bool boundary=p==end ||
            (s[p]=='|' && md_table_unescaped(s,p,begin));
        if(!boundary) continue;
        if(cells<expected_columns) {
            const uint32_t cell=md_add(d,row,STUN_UI_BLOCK_TABLE_CELL,0u);
            if(cell==UINT32_MAX) return false;
            const size_t cell_start=md_left(s,cell_begin,p);
            const size_t cell_end=md_right(s,cell_start,p);
            if(!md_inline(d,cell,s+cell_start,cell_end-cell_start,0u))
                return false;
        }
        ++cells;
        cell_begin=p+1;
    }
    while(cells<expected_columns && d->error==STUN_UI_OK) {
        if(md_add(d,row,STUN_UI_BLOCK_TABLE_CELL,0u)==UINT32_MAX)
            return false;
        ++cells;
    }
    return d->error==STUN_UI_OK;
}

/* A table ends before a new block-level structure. Ordinary text without
 * a pipe still supplies a one-column data row (padded to header width). */
static bool md_table_starts_new_block(const char *s, size_t b, size_t e) {
    if (b>=e || s[b]=='>') return true;
    if (md_rule(s,b,e)) return true;
    size_t hb=b,he=e;
    if (md_atx(s,&hb,&he)) return true;
    char marker=0; size_t count=0,info_begin=0,info_end=0;
    if (md_fence_start(s,b,e,&marker,&count,&info_begin,&info_end)) return true;
    bool ordered=false;size_t content=0;
    if (md_list_marker(s,b,e,&ordered,&content)) return true;
    return false;
}

static void md_parse_blocks(stun_md_document *d) {
    uint32_t paragraph = UINT32_MAX;
    uint32_t list = UINT32_MAX;
    uint32_t quote = UINT32_MAX;
    uint32_t previous_item_paragraph = UINT32_MAX;
    uint32_t indented = UINT32_MAX;
    size_t pending_code_blank = SIZE_MAX;
    bool previous_ordered = false, code_open = false;
    md_fence fence = {0,0,UINT32_MAX};

    for (size_t pos=0; pos<d->raw_size && d->error==STUN_UI_OK;) {
        const md_line line = md_read_line(d,pos);
        pos=line.next;
        const char *s=d->raw;
        const size_t indent_begin=md_left(s,line.begin,line.end);
        /* Block precedence uses visual columns, not raw prefix bytes:
         * a tab at column N advances to the next 4-column stop. Saturating
         * at four is enough for every current admission/continuation test. */
        size_t indent=0u;
        for(size_t i=line.begin;i<indent_begin && indent<4u;++i)
            indent += s[i]=='\t' ? 4u-(indent%4u) : 1u;
        size_t begin=indent_begin, end=line.end;
        if (code_open) {
            if (indent<=3 && md_fence_close(s,begin,end,fence)) {
                code_open=false;
                continue;
            }
            /* Preserve exact CRLF/LF bytes in fenced code payloads. */
            const size_t size=line.next-line.begin;
            md_text(d,fence.node,s+line.begin,size);
            continue;
        }
        /* Keep interstitial blank lines pending until another indented line
         * proves that they are interior. On termination/EOF they vanish. */
        if (indented!=UINT32_MAX) {
            if(md_blank(s,line.begin,line.end)) {
                if(pending_code_blank==SIZE_MAX) pending_code_blank=line.begin;
                continue;
            }
            if(md_code_unindent(s,line.begin,line.end)>line.begin) {
                if(pending_code_blank!=SIZE_MAX) {
                    for(size_t at=pending_code_blank;at<line.begin;) {
                        const md_line gap=md_read_line(d,at);
                        if(!md_code_line(d,indented,gap,true)) return;
                        at=gap.next;
                    }
                    pending_code_blank=SIZE_MAX;
                }
                if(!md_code_line(d,indented,line,false)) return;
                continue;
            }
            indented=UINT32_MAX;
            pending_code_blank=SIZE_MAX;
        }
        if (md_blank(s,line.begin,line.end)) {
            paragraph=UINT32_MAX;
            quote=UINT32_MAX;
            list=UINT32_MAX;
            previous_item_paragraph=UINT32_MAX;
            continue;
        }
        /* A non-quote line closes the previous quote paragraph before block
         * admission. This allows an indented block immediately after a quote. */
        if(quote!=UINT32_MAX && !(indent<=3 && begin<end && s[begin]=='>'))
            quote=paragraph=UINT32_MAX;

        /* Four indentation columns select literal code before checking ATX,
         * thematic rules, fences, or other block markers. Indented code
         * cannot interrupt a paragraph or the active list-item continuation. */
        if(paragraph==UINT32_MAX && list==UINT32_MAX &&
           md_code_unindent(s,line.begin,line.end)>line.begin) {
            indented=md_add(d,0u,STUN_UI_BLOCK_CODE_INDENTED,0u);
            if(indented==UINT32_MAX || !md_code_line(d,indented,line,false))
                return;
            previous_item_paragraph=UINT32_MAX;
            continue;
        }
        unsigned setext = md_setext(s,begin,end);
        if (paragraph!=UINT32_MAX && indent<=3 && setext) {
            d->nodes[paragraph].kind=STUN_UI_BLOCK_HEADING;
            d->nodes[paragraph].heading_level=setext;
            paragraph=UINT32_MAX;
            list=UINT32_MAX;
            quote=UINT32_MAX;
            continue;
        }
        if (indent <= 3 && md_rule(s,begin,end)) {
            (void)md_add(d,0u,STUN_UI_BLOCK_RULE,0);
            paragraph=list=quote=UINT32_MAX;
            continue;
        }
        char marker=0; size_t run=0, info_b=0,info_e=0;
        if (indent<=3 && md_fence_start(s,begin,end,
                &marker,&run,&info_b,&info_e)) {
            const uint32_t node=md_add(d,0u,STUN_UI_BLOCK_CODE_FENCE,0);
            if (node==UINT32_MAX) return;
            md_meta(d,node,s+info_b,info_e-info_b);
            fence=(md_fence){marker,run,node};
            code_open=true;
            paragraph=list=quote=UINT32_MAX;
            continue;
        }
        if (indent<=3) {
            size_t hstart=begin, hend=end;
            const unsigned level=md_atx(s,&hstart,&hend);
            if (level) {
                const uint32_t node=md_add(d,0u,STUN_UI_BLOCK_HEADING,level);
                if (node==UINT32_MAX) return;
                (void)md_inline(d,node,s+hstart,hend-hstart,0);
                paragraph=list=quote=UINT32_MAX;
                continue;
            }
        }
        if (indent<=3 && begin<end && s[begin]=='>') {
            if (quote==UINT32_MAX) {
                quote=md_add(d,0u,STUN_UI_BLOCK_QUOTE,0);
                if (quote==UINT32_MAX) return;
                paragraph=UINT32_MAX;
            }
            begin=md_left(s,begin+1,end);
            if (paragraph==UINT32_MAX) {
                paragraph=md_add(d,quote,STUN_UI_BLOCK_PARAGRAPH,0);
                if (paragraph==UINT32_MAX) return;
            } else (void)md_add(d,paragraph,STUN_UI_BLOCK_LINE_BREAK,0);
            (void)md_inline(d,paragraph,s+begin,end-begin,0);
            list=UINT32_MAX;
            continue;
        }
        if (quote!=UINT32_MAX) { quote=paragraph=UINT32_MAX; }
        /* Start a table only at a new block boundary. */
        if (indent<=3 && paragraph==UINT32_MAX &&
            list==UINT32_MAX && pos<d->raw_size) {
            const md_line sep=md_read_line(d,pos);
            size_t columns=0, delimiter_columns=0;
            if (md_table_scan(s,begin,end,false,true,&columns) &&
                md_table_scan(s,md_left(s,sep.begin,sep.end),sep.end,
                              true,true,&delimiter_columns) &&
                columns==delimiter_columns) {
                const uint32_t table=md_add(d,0u,STUN_UI_BLOCK_TABLE,0u);
                if(table==UINT32_MAX) return;
                if(!md_table_row(d,table,s,begin,end,columns)) return;
                pos=sep.next;
                while(pos<d->raw_size && d->error==STUN_UI_OK) {
                    const md_line row=md_read_line(d,pos);
                    const size_t row_begin=md_left(s,row.begin,row.end);
                    size_t row_columns=0;
                    if(row_begin-row.begin>3 || md_blank(s,row.begin,row.end) ||
                       md_table_starts_new_block(s,row_begin,row.end) ||
                       !md_table_scan(s,row_begin,row.end,false,false,&row_columns))
                        break;
                    if(!md_table_row(d,table,s,row_begin,row.end,columns))
                        return;
                    pos=row.next;
                }
                paragraph=list=quote=previous_item_paragraph=UINT32_MAX;
                continue;
            }
        }
        bool ordered=false;size_t item_start=0;
        if (indent<=3 && md_list_marker(s,begin,end,&ordered,&item_start)) {
            if (list==UINT32_MAX || previous_ordered!=ordered) {
                list=md_add(d,0u,ordered?
                    STUN_UI_BLOCK_ORDERED_LIST:STUN_UI_BLOCK_LIST,0);
                if (list==UINT32_MAX) return;
            }
            char check_state=0;
            size_t task_content=item_start;
            const bool task=md_task_marker(s,item_start,end,
                                          &check_state,&task_content);
            const uint32_t item=md_add(d,list,
                task ? STUN_UI_BLOCK_TASK_ITEM : STUN_UI_BLOCK_LIST_ITEM,0);
            if (item==UINT32_MAX) return;
            if(task) {
                const uint32_t checkbox=md_add(d,item,STUN_UI_BLOCK_CHECKBOX,0);
                if(checkbox==UINT32_MAX) return;
                md_meta(d,checkbox,&check_state,1u);
                if(d->error!=STUN_UI_OK) return;
                item_start=task_content;
            }
            previous_item_paragraph=md_add(d,item,STUN_UI_BLOCK_PARAGRAPH,0);
            if (previous_item_paragraph==UINT32_MAX) return;
            (void)md_inline(d,previous_item_paragraph,
                            s+item_start,end-item_start,0);
            previous_ordered=ordered;
            paragraph=UINT32_MAX;
            continue;
        }
        if (list!=UINT32_MAX && previous_item_paragraph!=UINT32_MAX &&
            indent>=2) {
            (void)md_add(d,previous_item_paragraph,STUN_UI_BLOCK_LINE_BREAK,0);
            (void)md_inline(d,previous_item_paragraph,s+begin,end-begin,0);
            continue;
        }
        list=UINT32_MAX;
        previous_item_paragraph=UINT32_MAX;
        if (paragraph==UINT32_MAX) {
            paragraph=md_add(d,0u,STUN_UI_BLOCK_PARAGRAPH,0);
            if (paragraph==UINT32_MAX) return;
        } else (void)md_add(d,paragraph,STUN_UI_BLOCK_LINE_BREAK,0);
        (void)md_inline(d,paragraph,s+begin,end-begin,0);
    }
}

void stun_md_document_destroy(stun_md_document *d) {
    if (!d) return;
    free(d->raw);
    free(d->text);
    free(d->metadata);
    free(d->nodes);
    free(d->node_metadata);
    free(d);
}

stun_ui_rich_document_view stun_md_document_view(const stun_md_document *d) {
    if (!d) return (stun_ui_rich_document_view){0};
    return (stun_ui_rich_document_view){
        .document = {
            .source = d->text,
            .source_bytes = d->text_size,
            .blocks = d->nodes,
            .block_count = d->node_count
        },
        .metadata = d->metadata,
        .metadata_bytes = d->metadata_size,
        .block_metadata = d->node_metadata,
        .block_metadata_count = d->node_count,
        .raw_markdown = d->raw,
        .raw_markdown_bytes = d->raw_size
    };
}

stun_ui_status stun_md_parse(const char *input,size_t length,
                             size_t max_nodes,size_t max_source_bytes,
                             stun_md_document **out) {
    if (!out) return STUN_UI_INVALID_ARGUMENT;
    *out=NULL;
    if ((!input && length) || !max_nodes || !max_source_bytes)
        return STUN_UI_INVALID_ARGUMENT;
    if (length>max_source_bytes || max_nodes>UINT32_MAX ||
        length==SIZE_MAX) return STUN_UI_CAPACITY_EXCEEDED;
    stun_md_document *d=(stun_md_document *)calloc(1,sizeof(*d));
    if (!d) return STUN_UI_OUT_OF_MEMORY;
    d->node_limit=max_nodes;
    d->raw_size=length;
    d->raw=(char*)malloc(length+1u);
    d->text=(char*)malloc(length+1u);
    d->metadata=(char*)malloc(length+1u);
    if (!d->raw || !d->text || !d->metadata) {
        stun_md_document_destroy(d);
        return STUN_UI_OUT_OF_MEMORY;
    }
    if(length) memcpy(d->raw,input,length);
    d->raw[length]='\0';
    d->text[0]=d->metadata[0]='\0';
    if (md_add(d,STUN_UI_PARENT_NONE,STUN_UI_BLOCK_DOCUMENT,0)==UINT32_MAX) {
        const stun_ui_status error=d->error;
        stun_md_document_destroy(d);
        return error;
    }
    md_parse_blocks(d);
    if (d->error==STUN_UI_OK) {
        d->nodes[0].byte_end=d->text_size;
        const stun_ui_rich_document_view view=stun_md_document_view(d);
        d->error=stun_ui_rich_document_validate(&view,max_nodes);
    }
    if (d->error!=STUN_UI_OK) {
        const stun_ui_status error=d->error;
        stun_md_document_destroy(d);
        return error;
    }
    *out=d;
    return STUN_UI_OK;
}
