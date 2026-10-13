#include <stun/markdown_c11.h>
#include <salts_unicode.h>
#include "markdown_entities.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The input's exact bytes are copied once and never rewritten. The parser
 * emits semantic text spans and metadata into two distinct owned buffers.
 * All RichDoc views borrow this document until stun_md_document_destroy().
 */
/* Parse-local root reference-definition bindings, never exposed through
 * RichDoc or persisted as runtime state. First normalized label wins.
 * Restrict this slice to bounded ASCII alphanumeric/whitespace labels:
 * full Unicode Default Case Folding and escaped-label equivalence are
 * tracked separately, not implemented via a locale or Stun fallback. */
enum { MD_REF_MAX=128u, MD_REF_KEY_MAX=128u };
typedef struct md_reference_binding {
    char key[MD_REF_KEY_MAX+1u];
    char *href;
    size_t href_bytes;
    char *title;
    size_t title_bytes;
    bool has_title;
    size_t source_line;
} md_reference_binding;

struct stun_md_document {
    char *raw;
    size_t raw_size;
    char *text;
    size_t text_size;
    size_t text_capacity;
    char *metadata;
    size_t metadata_size;
    size_t metadata_capacity;
    stun_ui_block *nodes;
    stun_ui_block_metadata *node_metadata;
    size_t node_count;
    size_t capacity;
    size_t node_limit;
    md_reference_binding *references;
    size_t reference_count;
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
    size_t limit=is_meta ? d->metadata_capacity : d->text_capacity;
    if(*used>limit) {md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;}
    if(!is_meta && n>limit-*used) {
        /* Two-codepoint HTML5 aliases such as &nGt; expand five raw ASCII
         * bytes into six UTF-8 bytes. The existing semantic arena therefore
         * cannot be capped at exactly raw_size. Grow only on demand, with
         * an explicit 2 * raw_size absolute bound and no borrowed views
         * until the successful document is published. */
        if(d->raw_size>(SIZE_MAX-1u)/2u) {
            md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
        }
        const size_t maximum=d->raw_size*2u;
        if(*used>maximum || n>maximum-*used) {
            md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
        }
        char *data=(char*)realloc(d->text,maximum+1u);
        if(!data) {md_error(d,STUN_UI_OUT_OF_MEMORY);return false;}
        d->text=data;d->text_capacity=maximum;limit=maximum;
    }
    if(is_meta && n>limit-*used) {
        /* Bound metadata by the node budget; short Markdown sources may still
         * need eight metadata bytes for one list. */
        if(d->raw_size>(SIZE_MAX-1u)/2u ||
           d->node_limit>SIZE_MAX/sizeof(stun_ui_list_semantics)) {
            md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
        }
        const size_t semantic_ceiling=d->raw_size*2u;
        if(semantic_ceiling>
           SIZE_MAX-d->node_limit*sizeof(stun_ui_list_semantics)) {
            md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
        }
        const size_t maximum=semantic_ceiling+
                             d->node_limit*sizeof(stun_ui_list_semantics);
        if(*used>maximum || n>maximum-*used) {
            md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
        }
        size_t cap=limit?limit:1u;
        while(cap<*used+n) {
            if(cap>(maximum-1u)/2u) {cap=maximum;break;}
            cap=cap*2u+1u;
        }
        char *data=(char*)realloc(d->metadata,cap+1u);
        if(!data) {md_error(d,STUN_UI_OUT_OF_MEMORY);return false;}
        d->metadata=data;d->metadata_capacity=cap;limit=cap;
    }
    if(n>limit-*used) {md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;}
    target=is_meta ? d->metadata : d->text;
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


/* One bounded component decoder for fence-info strings and link destinations
 * / titles. Only raw bytes undergo Markdown ASCII punctuation unescaping and
 * HTML5 character reference decoding. Results are never reparsed as grammar.
 * Two-scalar aliases can expand UTF-8 output beyond raw length; twice source
 * bytes bounds the scratch and is checked before every write. */
static bool md_decode_component(stun_md_document *d,const char *s,size_t n,
                                 char **output,size_t *output_bytes) {
    if(d->error!=STUN_UI_OK || !output || !output_bytes)return false;
    if(n>(SIZE_MAX-1u)/2u) {
        md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
    }
    const size_t cap=n*2u;
    char *scratch=(char*)malloc(cap+1u);
    if(!scratch) {md_error(d,STUN_UI_OUT_OF_MEMORY);return false;}
    size_t used=0u;
    for(size_t p=0u;p<n && d->error==STUN_UI_OK;) {
        if(s[p]=='\\' && p+1u<n && md_ascii_punct(s[p+1u])) {
            scratch[used++]=s[p+1u];
            p+=2u;continue;
        }
        char decoded[8];
        size_t consumed=0u,written=0u;
        if(s[p]=='&' && stun_md_decode_character_reference(
                    s+p,n-p,&consumed,decoded,&written)) {
            if(written>cap-used) {
                md_error(d,STUN_UI_CAPACITY_EXCEEDED);break;
            }
            memcpy(scratch+used,decoded,written);
            used+=written;p+=consumed;continue;
        }
        scratch[used++]=s[p++];
    }
    if(d->error!=STUN_UI_OK) {free(scratch);return false;}
    scratch[used]='\0'; /* the length remains authoritative */
    *output=scratch;*output_bytes=used;
    return true;
}

/* Fence info strings share this exact decoder with inline link metadata.
 * The code fence BODY is strictly literal. All decoded metadata is copied
 * into the document-owned arena before the scratch is released. */
static void md_fence_info_meta(stun_md_document *d,uint32_t node,
                               const char *s,size_t n) {
    if(node==UINT32_MAX || !n || d->error!=STUN_UI_OK)return;
    char *text=NULL;size_t bytes=0u;
    if(!md_decode_component(d,s,n,&text,&bytes))return;
    md_meta(d,node,text,bytes);
    free(text);
}

/* List metadata is owned by the parser; update before publishing the view. */
static bool md_list_meta(stun_md_document *d, uint32_t id,
                         uint32_t start, char marker) {
    const stun_ui_list_semantics info={start,(uint8_t)marker,0u,{0u,0u}};
    md_meta(d,id,(const char*)&info,sizeof(info));
    return d->error==STUN_UI_OK;
}
static void md_mark_list_loose(stun_md_document *d, uint32_t id) {
    if(id>=d->node_count) {md_error(d,STUN_UI_INVALID_ARGUMENT);return;}
    const stun_ui_block_metadata span=d->node_metadata[id];
    if(span.byte_end-span.byte_begin!=sizeof(stun_ui_list_semantics)) {
        md_error(d,STUN_UI_INVALID_ARGUMENT);return;
    }
    stun_ui_list_semantics info;
    memcpy(&info,d->metadata+span.byte_begin,sizeof(info));
    info.loose=1u;
    memcpy(d->metadata+span.byte_begin,&info,sizeof(info));
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

/* Code spans are opaque to outer emphasis and link-label delimiter scans,
 * and close only on an EXACTLY equal-sized backtick run. */
static size_t md_code_span_close(const char *s,size_t n,size_t begin,
                                 size_t marker_size);

/* ASCII CommonMark left/right flanking: punctuation adjacent to an opener
 * or closer matters, and underscores inside a word cannot open or close.
 * Unicode whitespace/punctuation classification remains a separate #182
 * provider-backed task; do not invent a second Unicode character table. */
/* Inline flanking has to classify the *adjacent scalar*, not its last
 * UTF-8 byte. The only non-ASCII property requested here is Unicode 17
 * White_Space from the released Salts::Unicode provider: there is no copied
 * Unicode table or locale-sensitive fallback. Salts validates the borrowed
 * scalar; an invalid UTF-8 fragment has no provable White_Space property.
 *
 * The previous scalar begins at most three continuation bytes to the left.
 * The public Unicode scanner decodes/validates one scalar without allocation.
 * All source/text lifetimes remain owned by the containing RichDoc. */
static bool md_inline_whitespace_after(const char *s,size_t n,size_t at) {
    if(at>=n) return true; /* end of span */
    if((unsigned char)s[at]<0x80u)
        return md_space(s[at]) || s[at]=='\n' || s[at]=='\r';
    size_t cursor=0u;
    salts_unicode_scalar scalar={0};
    const vstr borrowed=vstr_from_buf(s+at,n-at);
    return salts_unicode_utf8_next(borrowed,&cursor,&scalar)==
           SALTS_UNICODE_OK &&
           (scalar.properties&SALTS_UNICODE_PROPERTY_WHITE_SPACE)!=0u;
}

static bool md_inline_whitespace_before(const char *s,size_t at) {
    if(!at) return true; /* beginning of span */
    const size_t last=at-1u;
    if((unsigned char)s[last]<0x80u)
        return md_space(s[last]) || s[last]=='\n' || s[last]=='\r';
    size_t first=last;
    while(first>0u && at-first<4u &&
          ((unsigned char)s[first]&0xc0u)==0x80u) --first;
    size_t cursor=0u;
    salts_unicode_scalar scalar={0};
    const vstr borrowed=vstr_from_buf(s+first,at-first);
    return salts_unicode_utf8_next(borrowed,&cursor,&scalar)==
           SALTS_UNICODE_OK && cursor==at-first &&
           (scalar.properties&SALTS_UNICODE_PROPERTY_WHITE_SPACE)!=0u;
}

static void md_emph_flanking(const char *s,size_t n,size_t at,size_t run,
                             char marker,bool *can_open,bool *can_close) {
    const bool left_space=md_inline_whitespace_before(s,at);
    const bool right_space=md_inline_whitespace_after(s,n,at+run);
    const bool left_punct=at>0u && md_ascii_punct(s[at-1u]);
    const bool right_punct=at+run<n && md_ascii_punct(s[at+run]);
    const bool left_flanking=!right_space &&
        (!right_punct || left_space || left_punct);
    const bool right_flanking=!left_space &&
        (!left_punct || right_space || right_punct);
    *can_open=left_flanking &&
              (marker!='_' || !right_flanking || left_punct);
    *can_close=right_flanking &&
               (marker!='_' || !left_flanking || right_punct);
}

/* Generic exact-size matcher for link/strike/fence syntax. Emphasis and
 * strong runs are handled EXCLUSIVELY by md_emph_pairs below: do not
 * resurrect a competing greedy * / _ matcher here. */
static size_t md_match(const char *s, size_t n, size_t start,
                       char symbol, size_t count) {
    const bool opaque_code=symbol=='~' || symbol==']';
    for(size_t p=start;p<n;) {
        if(s[p]=='\\' && p+1u<n) {p+=2u;continue;}
        if(opaque_code && s[p]==(char)0x60) {
            size_t end=p+1u;
            while(end<n && s[end]==(char)0x60) ++end;
            const size_t close=md_code_span_close(s,n,end,end-p);
            p=close==SIZE_MAX?end:close+(end-p);
            continue;
        }
        if(s[p]!=symbol) {++p;continue;}
        size_t end=p+1u;
        while(end<n && s[end]==symbol) ++end;
        if(end-p==count) return p;
        p=end;
    }
    return SIZE_MAX;
}

/* One raw-source link destination/title parser shared by delimiter
 * discovery and RichDoc emission. Do not use the first ')' as a closer:
 * CommonMark admits balanced destination parentheses and <...> targets
 * containing spaces or ')' bytes. Angle delimiters are stripped BEFORE
 * decoding the semantic destination, and none of the decoded characters
 * are reused as Markdown grammar.
 *
 * All scans remain within n owned source bytes. Unbracketed parentheses
 * nesting is bounded to 32 levels; malformed/over-depth syntax remains
 * literal input rather than inventing partial LINK/URI ownership.
 * Reference links and complex bracket labels belong to a separate slice. */
typedef struct md_link_parts {
    size_t destination_begin,destination_end;
    size_t title_begin,title_end;
    bool has_title;
} md_link_parts;

static bool md_link_target_parse(const char *s,size_t n,size_t round_open,
                                 md_link_parts *out,size_t *round_close) {
    if(!s || !out || !round_close || round_open>=n ||
       s[round_open]!='(')return false;
    size_t p=round_open+1u;
    while(p<n && md_space(s[p]))++p;
    const bool angle=p<n && s[p]=='<';
    size_t begin=p,end=p;

    if(angle) {
        /* In an angle destination the first UNESCAPED '>' owns the end;
         * literal ')' and ASCII spaces are ordinary destination bytes.
         * A newline or a nested raw '<' cannot enter angle syntax. */
        begin=++p;
        while(p<n && s[p]!='>') {
            if(s[p]=='\n' || s[p]=='\r' || s[p]=='<')return false;
            if(s[p]=='\\' && p+1u<n &&
               md_ascii_punct(s[p+1u]))p+=2u;
            else ++p;
        }
        if(p>=n || s[p]!='>')return false;
        end=p++;
    } else {
        begin=p;
        unsigned depth=0u;
        while(p<n) {
            const char ch=s[p];
            if(ch=='\\' && p+1u<n && md_ascii_punct(s[p+1u])) {
                p+=2u;continue;
            }
            if(ch=='\n' || ch=='\r' || ch=='<' || ch=='>' ||
               md_space(ch))break;
            if(ch=='(') {
                if(depth>=32u)return false;
                ++depth;
            } else if(ch==')') {
                if(!depth)break;
                --depth;
            }
            ++p;
        }
        if(depth || p>=n)return false;
        end=p;
    }

    const size_t separator_start=p;
    while(p<n && md_space(s[p]))++p;
    const bool separated=p>separator_start;
    md_link_parts parts={
        .destination_begin=begin,.destination_end=end,
        .title_begin=0u,.title_end=0u,.has_title=false
    };
    if(p<n && s[p]==')') {
        *out=parts;*round_close=p;return true;
    }
    if(!separated || p>=n)return false;
    const char quote=s[p];
    if(quote!='"' && quote!='\'' && quote!='(')return false;
    ++p;
    const size_t title_begin=p;
    const char closer=quote=='('?')':quote;
    while(p<n && s[p]!=closer) {
        if(s[p]=='\n' || s[p]=='\r')return false;
        if(s[p]=='\\' && p+1u<n &&
           md_ascii_punct(s[p+1u]))p+=2u;
        else ++p;
    }
    if(p>=n)return false;
    parts.title_begin=title_begin;
    parts.title_end=p++;
    parts.has_title=true;
    while(p<n && md_space(s[p]))++p;
    if(p>=n || s[p]!=')')return false;
    *out=parts;*round_close=p;
    return true;
}

/* Label normalization operates on the bounded, literal ASCII subset:
 * trim/collapse horizontal whitespace and ASCII case-fold. Never claim
 * Unicode case folding by silently borrowing locale-dependent C routines.
 * Only recognized reference keys are allowed to create LINK nodes. */
static bool md_reference_key(const char *s,size_t b,size_t e,
                             char key[MD_REF_KEY_MAX+1u]) {
    if(b>=e || e-b>MD_REF_KEY_MAX)return false;
    size_t used=0u;bool gap=false;
    for(size_t i=b;i<e;++i) {
        const unsigned char c=(unsigned char)s[i];
        if(c==' ' || c=='\t') {if(used)gap=true;continue;}
        const bool alpha=(c>='A'&&c<='Z')||(c>='a'&&c<='z');
        const bool digit=c>='0'&&c<='9';
        if(!alpha && !digit)return false;
        if(gap) {
            if(used>=MD_REF_KEY_MAX)return false;
            key[used++]=' ';
            gap=false;
        }
        if(used>=MD_REF_KEY_MAX)return false;
        key[used++]=(char)((c>='A'&&c<='Z')?c+('a'-'A'):c);
    }
    if(!used)return false;
    key[used]='\0';
    return true;
}

static const md_reference_binding *md_reference_lookup(
    const stun_md_document *d,const char *s,size_t begin,size_t end) {
    if(!d->reference_count)return NULL;
    char key[MD_REF_KEY_MAX+1u];
    if(!md_reference_key(s,begin,end,key))return NULL;
    for(size_t i=0u;i<d->reference_count;++i)
        if(!strcmp(d->references[i].key,key))
            return &d->references[i]; /* first matching definition wins */
    return NULL;
}

/* One semantic admission path for full, collapsed and shortcut references.
 * Resolve explicit second labels before looking for shortcut bindings.
 * The surrounding delimiter matcher and RichDoc emitter call this SAME
 * function so emphasis cannot borrow characters from a real link label. */
static bool md_reference_inline_match(
    const stun_md_document *d,const char *s,size_t n,size_t at,
    const md_reference_binding **binding,size_t *label_end,
    size_t *token_end) {
    if(!d->reference_count || !binding || !label_end || !token_end ||
       at>=n || s[at]!='[')return false;
    const size_t first=md_match(s,n,at+1u,']',1u);
    if(first==SIZE_MAX)return false;
    if(first+1u<n && s[first+1u]=='[') {
        const size_t second=md_match(s,n,first+2u,']',1u);
        if(second!=SIZE_MAX) {
            const bool collapsed=second==first+2u;
            const md_reference_binding *found=md_reference_lookup(
                d,s,collapsed?at+1u:first+2u,collapsed?first:second);
            if(found) {
                *binding=found;*label_end=first;*token_end=second+1u;
                return true;
            }
            return false;
        }
    }
    const md_reference_binding *found=
        md_reference_lookup(d,s,at+1u,first);
    if(!found)return false;
    *binding=found;*label_end=first;*token_end=first+1u;
    return true;
}

/* CommonMark backtick delimiters close only on an EXACTLY equal-sized run.
 * Unlike emphasis/link matching, backslashes are ordinary literal payload
 * inside a code span, including when directly preceding a backtick. Skip
 * entire mismatching runs so their suffix cannot impersonate a delimiter. */
static size_t md_code_span_close(const char *s,size_t n,size_t begin,
                                 size_t marker_size) {
    for(size_t p=begin;p<n;) {
        if(s[p]!=(char)0x60) { ++p;continue; }
        size_t end=p+1u;
        while(end<n && s[end]==(char)0x60) ++end;
        if(end-p==marker_size) return p;
        p=end;
    }
    return SIZE_MAX;
}

/* Normalize code-span payload into owned semantic UTF-8. Each LF/CRLF
 * line ending becomes ONE ASCII space. Trim exactly one edge space when
 * both edges are spaces and the payload is not all spaces. Backslashes,
 * other interior spaces and Unicode bytes are literal. The scratch buffer
 * cannot exceed the already admitted raw-source budget; publication still
 * goes through the single RichDoc text arena and failure-atomic md_text. */
static void md_code_span_text(stun_md_document *d,uint32_t parent,
                              const char *s,size_t start,size_t end) {
    const size_t n=end-start;
    const bool multiline=memchr(s+start,'\n',n)!=NULL ||
                         memchr(s+start,'\r',n)!=NULL;
    char *normalized=NULL;
    const char *text=s+start;
    size_t begin=0u,bytes=n;
    if(multiline) {
        normalized=(char*)malloc(n+1u);
        if(!normalized) {md_error(d,STUN_UI_OUT_OF_MEMORY);return;}
        bytes=0u;
        for(size_t i=start;i<end;++i) {
            const char c=s[i];
            if(c=='\r') {
                if(i+1u<end && s[i+1u]=='\n') ++i;
                normalized[bytes++]=' ';
            } else if(c=='\n') normalized[bytes++]=' ';
            else normalized[bytes++]=c;
        }
        text=normalized;
    }
    if(bytes>1u && text[0]==' ' && text[bytes-1u]==' ') {
        bool nonspace=false;
        for(size_t i=0u;i<bytes;++i)
            if(text[i]!=' ') {nonspace=true;break;}
        if(nonspace) {++begin;--bytes;}
    }
    md_text(d,parent,text+begin,bytes-begin);
    free(normalized);
}

/* Runs retain their original source offsets, with closers consuming from the
 * LEFT and openers from the RIGHT. Each consumed marker receives exactly one
 * event; the source buffer itself is never changed. This is one C11 grammar,
 * not a fallback renderer or a second Markdown parser. */
typedef struct md_emph_run {
    size_t start, remaining, consumed_left;
    char marker;
    bool can_open, can_close, inactive;
} md_emph_run;

typedef struct md_emph_event {
    uint8_t open, close; /* 1: emphasis, 2: strong, 0: literal */
} md_emph_event;

/* Build all * / _ pairs before emitting RichDoc nodes. This is essential
 * when one delimiter run is both opener and closer, when runs have lengths
 * 1/2/3+, or when an inner opener owns the nearer closer. Code/link/strike
 * payloads are scanned by their existing canonical inline semantics.
 *
 * A pair is forbidden by CommonMark's rule of three if either endpoint can
 * both open and close, the combined remaining lengths are divisible by 3,
 * and at least one remaining length is NOT divisible by 3.
 *
 * Comparison work is capped relative to this inline source's byte count:
 * malicious alternating runs fail atomically instead of searching forever. */
static bool md_emph_pairs(stun_md_document *d,const char *s,size_t n,
                          md_emph_event *events) {
    if(n>SIZE_MAX/sizeof(md_emph_run)) {
        md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
    }
    md_emph_run *runs=(md_emph_run*)malloc((n?n:1u)*sizeof(*runs));
    if(!runs) {md_error(d,STUN_UI_OUT_OF_MEMORY);return false;}
    size_t count=0u;
    for(size_t p=0u;p<n;) {
        if(s[p]=='\\' && p+1u<n && md_ascii_punct(s[p+1u])) {
            p+=2u;continue;
        }
        if(s[p]==(char)0x60) {
            size_t end=p+1u;
            while(end<n && s[end]==(char)0x60) ++end;
            const size_t closer=md_code_span_close(s,n,end,end-p);
            p=closer==SIZE_MAX?end:closer+end-p;
            continue;
        }
        if(s[p]=='[' || (s[p]=='!' && p+1u<n && s[p+1u]=='[')) {
            const size_t b=p+(s[p]=='!'?2u:1u);
            const size_t square=md_match(s,n,b,']',1u);
            if(square!=SIZE_MAX && square+1u<n && s[square+1u]=='(') {
                size_t round=SIZE_MAX;
                md_link_parts target={0};
                if(md_link_target_parse(
                       s,n,square+1u,&target,&round)) {
                    p=round+1u;continue;
                }
            }
            if(s[p]=='[') {
                const md_reference_binding *ref=NULL;
                size_t label_end=0u,token_end=0u;
                if(md_reference_inline_match(d,s,n,p,&ref,
                                             &label_end,&token_end)) {
                    p=token_end;continue;
                }
            }
        }
        if(s[p]=='~' && p+1u<n && s[p+1u]=='~') {
            const size_t closer=md_match(s,n,p+2u,'~',2u);
            if(closer!=SIZE_MAX) {p=closer+2u;continue;}
        }
        if(s[p]=='*' || s[p]=='_') {
            size_t end=p+1u;
            while(end<n && s[end]==s[p]) ++end;
            bool can_open=false,can_close=false;
            md_emph_flanking(s,n,p,end-p,s[p],&can_open,&can_close);
            if(can_open || can_close)
                runs[count++]=(md_emph_run){
                    .start=p,.remaining=end-p,.consumed_left=0u,
                    .marker=s[p],.can_open=can_open,.can_close=can_close,
                    .inactive=false
                };
            p=end;continue;
        }
        ++p;
    }
    const size_t limit=n>(SIZE_MAX-64u)/64u?
                        SIZE_MAX:n*64u+64u;
    size_t examined=0u;
    for(size_t c=0u;c<count && d->error==STUN_UI_OK;++c) {
        md_emph_run *closer=&runs[c];
        if(!closer->can_close || closer->inactive) continue;
        while(closer->remaining) {
            size_t candidate=SIZE_MAX;
            for(size_t k=c;k>0u;) {
                --k;
                if(++examined>limit) {
                    md_error(d,STUN_UI_CAPACITY_EXCEEDED);break;
                }
                const md_emph_run *opener=&runs[k];
                if(opener->inactive || !opener->remaining ||
                   !opener->can_open || opener->marker!=closer->marker)
                    continue;
                const size_t sum_mod3=(opener->remaining%3u+
                                       closer->remaining%3u)%3u;
                const bool odd_match=
                    (opener->can_close || closer->can_open) &&
                    sum_mod3==0u &&
                    (opener->remaining%3u!=0u ||
                     closer->remaining%3u!=0u);
                if(!odd_match) {candidate=k;break;}
            }
            if(d->error!=STUN_UI_OK || candidate==SIZE_MAX) break;
            md_emph_run *opener=&runs[candidate];
            const uint8_t width=opener->remaining>=2u &&
                                closer->remaining>=2u?2u:1u;
            const size_t opening=opener->start+opener->consumed_left+
                                 opener->remaining-width;
            const size_t closing=closer->start+closer->consumed_left;
            if(opening>=n || closing>=n ||
               events[opening].open || events[closing].close ||
               opening+width>closing) {
                md_error(d,STUN_UI_INVALID_ARGUMENT);break;
            }
            events[opening].open=width;
            events[closing].close=width;
            opener->remaining-=width;
            closer->remaining-=width;
            closer->consumed_left+=width;
            /* Delimiters enclosed by a completed match cannot subsequently
             * participate in a crossing pair. They remain literal text if
             * not already consumed by a nested pair. */
            for(size_t j=candidate+1u;j<c;++j)
                runs[j].inactive=true;
        }
    }
    free(runs);
    return d->error==STUN_UI_OK;
}

/* Emit the paired spans as typed nodes in source order. Node ownership is
 * explicit, with a bounded ancestor stack; escapes and code/link payloads
 * still follow their original single C11 producer. */
typedef enum md_root_break {
    MD_ROOT_BREAK_SOFT = 0,
    MD_ROOT_BREAK_SPACES,
    MD_ROOT_BREAK_BACKSLASH
} md_root_break;

static md_root_break md_root_line_content(const char *s,size_t begin,
                                           size_t end,bool continues,
                                           size_t *content_end) {
    size_t trimmed=md_right(s,begin,end);
    *content_end=trimmed; /* one trailing space/tab is not semantic TEXT */
    if(!continues || trimmed==begin)return MD_ROOT_BREAK_SOFT;
    /* CommonMark #633–#635: two or more literal ASCII spaces before LF.
     * Tab-column hard-break interpretation remains a separate #185 slice. */
    if(end-trimmed>=2u) {
        bool only_spaces=true;
        for(size_t i=trimmed;i<end;++i)
            if(s[i]!=' ') {only_spaces=false;break;}
        if(only_spaces)return MD_ROOT_BREAK_SPACES;
    }
    /* CommonMark #634/#637: ONLY an unescaped final backslash marks a hard
     * break. Even-length trailing backslash runs retain literal backslashes. */
    if(trimmed==end && s[end-1u]=='\\') {
        size_t at=end;
        while(at>begin && s[at-1u]=='\\')--at;
        if((end-at)%2u!=0u) {
            *content_end=end-1u;
            return MD_ROOT_BREAK_BACKSLASH;
        }
    }
    return MD_ROOT_BREAK_SOFT;
}

static bool md_root_emit_break(stun_md_document *d,uint32_t paragraph,
                               md_root_break kind) {
    const bool hard=kind!=MD_ROOT_BREAK_SOFT;
    const uint32_t node=md_add(d,paragraph,hard?
        STUN_UI_BLOCK_HARD_LINE_BREAK:STUN_UI_BLOCK_LINE_BREAK,0u);
    if(node==UINT32_MAX)return false;
    if(hard) {
        const stun_ui_line_break_semantics info={
            (uint8_t)(kind==MD_ROOT_BREAK_SPACES ?
             STUN_UI_LINE_BREAK_TRAILING_SPACES :
             STUN_UI_LINE_BREAK_BACKSLASH),{0u,0u,0u}
        };
        md_meta(d,node,(const char*)&info,sizeof(info));
    }
    return d->error==STUN_UI_OK;
}

static bool md_inline(stun_md_document *d, uint32_t parent,
                      const char *s, size_t n, unsigned depth) {
    if(depth>32u || n>SIZE_MAX/sizeof(md_emph_event)) {
        md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
    }
    md_emph_event *events=(md_emph_event*)calloc(
        n?n:1u,sizeof(*events));
    if(!events) {md_error(d,STUN_UI_OUT_OF_MEMORY);return false;}
    if(!md_emph_pairs(d,s,n,events)) {
        free(events);return false;
    }
    uint32_t owners[33]={0u};
    owners[0]=parent;
    unsigned nested=0u;
    size_t p=0u,text_start=0u,physical_line_start=0u;
    while(p<n && d->error==STUN_UI_OK) {
        if(events[p].close) {
            if(!nested) {md_error(d,STUN_UI_INVALID_ARGUMENT);break;}
            md_text(d,owners[nested],s+text_start,p-text_start);
            --nested;
            p+=events[p].close;
            text_start=p;continue;
        }
        if(events[p].open) {
            if(depth+nested>=32u) {
                md_error(d,STUN_UI_CAPACITY_EXCEEDED);break;
            }
            md_text(d,owners[nested],s+text_start,p-text_start);
            const stun_ui_block_kind kind=events[p].open==2u?
                STUN_UI_BLOCK_STRONG:STUN_UI_BLOCK_EMPHASIS;
            const uint32_t node=md_add(d,owners[nested],kind,0u);
            if(node==UINT32_MAX) break;
            owners[++nested]=node;
            p+=events[p].open;
            text_start=p;continue;
        }
        /* A single paragraph-scoped inline source can span physical LF
         * boundaries. The same #185 source-origin classifier trims semantic
         * whitespace/backslash BEFORE publishing TEXT, and publishes a
         * hard or soft break under the actual nested Emphasis/Strong owner.
         * Code spans are opaque and remain literal under this path. */
        if(s[p]=='\n') {
            /* Classify the physical line, NOT only the remaining TEXT
             * suffix: inner strong/link owners may have closed immediately
             * before the hard-break whitespace. Keep output TEXT trimmed
             * from its own admitted span without reviving source markers. */
            size_t content_end=p;
            const md_root_break kind=md_root_line_content(
                s,physical_line_start,p,true,&content_end);
            if(content_end<text_start)content_end=text_start;
            md_text(d,owners[nested],s+text_start,content_end-text_start);
            if(!md_root_emit_break(d,owners[nested],kind))break;
            ++p;
            text_start=physical_line_start=p;
            continue;
        }
        if(s[p]=='\\' && p+1u<n && md_ascii_punct(s[p+1u])) {
            md_text(d,owners[nested],s+text_start,p-text_start);
            md_text(d,owners[nested],s+p+1u,1u);
            p+=2u;text_start=p;continue;
        }
        /* #185: decode a recognized HTML5 entity as SEMANTIC UTF-8 only
         * after raw-source punctuation/escape/code admission. The decoded
         * value is never rescanned as a Markdown delimiter, so &#42; cannot
         * manufacture emphasis or a list marker. Code spans remain opaque;
         * malformed or semicolon-less references remain literal source. */
        if(s[p]=='&') {
            char decoded[8];
            size_t consumed=0u,written=0u;
            if(stun_md_decode_character_reference(
                    s+p,n-p,&consumed,decoded,&written)) {
                md_text(d,owners[nested],s+text_start,p-text_start);
                md_text(d,owners[nested],decoded,written);
                p+=consumed;
                text_start=p;
                continue;
            }
        }
        if(s[p]==(char)0x60) {
            size_t end=p+1u;
            while(end<n && s[end]==(char)0x60) ++end;
            const size_t close=md_code_span_close(s,n,end,end-p);
            if(close!=SIZE_MAX) {
                md_text(d,owners[nested],s+text_start,p-text_start);
                const uint32_t node=md_add(d,owners[nested],
                    STUN_UI_BLOCK_CODE_SPAN,0u);
                if(node==UINT32_MAX) break;
                md_code_span_text(d,node,s,end,close);
                p=close+end-p;text_start=p;
            } else p=end;
            continue;
        }
        if(s[p]=='~' && p+1u<n && s[p+1u]=='~') {
            const size_t close=md_match(s,n,p+2u,'~',2u);
            if(close!=SIZE_MAX) {
                md_text(d,owners[nested],s+text_start,p-text_start);
                const uint32_t node=md_add(d,owners[nested],
                    STUN_UI_BLOCK_STRIKETHROUGH,0u);
                if(node==UINT32_MAX) break;
                if(!md_inline(d,node,s+p+2u,close-(p+2u),
                              depth+nested+1u)) break;
                p=close+2u;text_start=p;continue;
            }
        }
        if(s[p]=='[' || (s[p]=='!' && p+1u<n && s[p+1u]=='[')) {
            const bool image=s[p]=='!';
            const size_t b=p+(image?2u:1u);
            const size_t square=md_match(s,n,b,']',1u);
            if(square!=SIZE_MAX && square+1u<n && s[square+1u]=='(') {
                size_t round=SIZE_MAX;
                md_link_parts target={0};
                if(md_link_target_parse(
                       s,n,square+1u,&target,&round)) {
                    /* Decode metadata before publishing nodes. An invalid
                     * or unsafe decoded href still fails closed in the HTML
                     * renderer; no URL filtering is delegated to the text
                     * grammar or weakened by title syntax. */
                    char *dest=NULL,*title=NULL;
                    size_t dest_len=0u,title_len=0u;
                    if(!md_decode_component(d,
                            s+target.destination_begin,
                            target.destination_end-
                            target.destination_begin,&dest,&dest_len))
                        break;
                    if(target.has_title &&
                       !md_decode_component(d,
                            s+target.title_begin,
                            target.title_end-target.title_begin,
                            &title,&title_len)) {
                        free(dest);break;
                    }
                    md_text(d,owners[nested],s+text_start,p-text_start);
                    const uint32_t node=md_add(d,owners[nested],
                        image?STUN_UI_BLOCK_IMAGE:STUN_UI_BLOCK_LINK,0u);
                    if(node!=UINT32_MAX) {
                        md_meta(d,node,dest,dest_len);
                        if(target.has_title && !image) {
                            const uint32_t title_node=md_add(
                                d,node,STUN_UI_BLOCK_LINK_TITLE,0u);
                            if(title_node!=UINT32_MAX)
                                md_meta(d,title_node,title,title_len);
                        }
                    }
                    free(title);free(dest);
                    if(node==UINT32_MAX || d->error!=STUN_UI_OK)break;
                    if(!md_inline(d,node,s+b,square-b,
                                  depth+nested+1u))break;
                    p=round+1u;text_start=p;continue;
                }
            }
            if(!image) {
                const md_reference_binding *ref=NULL;
                size_t first_end=0u,token_end=0u;
                if(md_reference_inline_match(
                       d,s,n,p,&ref,&first_end,&token_end)) {
                    md_text(d,owners[nested],s+text_start,p-text_start);
                    const uint32_t node=md_add(
                        d,owners[nested],STUN_UI_BLOCK_LINK,0u);
                    if(node==UINT32_MAX)break;
                    md_meta(d,node,ref->href,ref->href_bytes);
                    if(ref->has_title) {
                        const uint32_t title=md_add(
                            d,node,STUN_UI_BLOCK_LINK_TITLE,0u);
                        if(title==UINT32_MAX)break;
                        md_meta(d,title,ref->title,ref->title_bytes);
                    }
                    if(d->error!=STUN_UI_OK)break;
                    if(!md_inline(d,node,s+p+1u,first_end-p-1u,
                                  depth+nested+1u))break;
                    p=token_end;
                    text_start=p;
                    continue;
                }
            }
        }
        ++p;
    }
    if(d->error==STUN_UI_OK && nested)
        md_error(d,STUN_UI_INVALID_ARGUMENT);
    if(d->error==STUN_UI_OK)
        md_text(d,owners[0],s+text_start,n-text_start);
    free(events);
    return d->error==STUN_UI_OK;
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


enum { MD_LIST_MAX_DEPTH = 32 };
typedef struct md_list_marker_info {
    size_t column;
    size_t content_column;
    size_t content_offset;
    uint32_t start;
    char marker;
    bool ordered;
} md_list_marker_info;
typedef struct md_list_frame {
    uint32_t list, item, paragraph, code, quote, quote_paragraph;
    md_fence fence;
    bool fence_open;
    size_t code_blank_lines;
    size_t marker_column, content_column, list_base_column;
    char marker;
    bool ordered, pending_blank, empty_item;
} md_list_frame;
typedef struct md_list_stack {
    md_list_frame frames[MD_LIST_MAX_DEPTH];
    size_t depth;
} md_list_stack;

/* CommonMark indentation is measured in source columns. Relative container
 * indentation is derived after matching the outer container; a tab occupies
 * the remainder of its four-column stop. */
static size_t md_columns(const char *s,size_t begin,size_t end) {
    size_t col=0u;
    for(size_t p=begin;p<end;++p) {
        if(s[p]==' ') ++col;
        else if(s[p]=='\t') col+=4u-(col%4u);
        else break;
    }
    return col;
}
static size_t md_skip_columns(const char *s,size_t begin,size_t end,
                              size_t wanted) {
    size_t col=0u, p=begin;
    while(p<end && col<wanted && md_space(s[p])) {
        col+=s[p]=='\t'?4u-(col%4u):1u;
        ++p;
    }
    return col>=wanted?p:begin;
}

/* Admit a list marker, recording the marker's visual column, exact ordered
 * start, and the item content column. Marker type is part of list identity. */
static bool md_list_token(const char *s,md_line line,
                          md_list_marker_info *out) {
    const size_t b=md_left(s,line.begin,line.end);
    bool ordered=false;
    size_t content=0u;
    if(!md_list_marker(s,b,line.end,&ordered,&content)) return false;
    size_t marker_end=b;
    uint32_t start=0u;
    char marker=s[b];
    if(ordered) {
        while(marker_end<line.end && s[marker_end]>='0' &&
              s[marker_end]<='9') {
            start=start*10u+(uint32_t)(s[marker_end]-'0');
            ++marker_end;
        }
        marker=s[marker_end];
        ++marker_end;
    } else ++marker_end;
    const size_t marker_col=md_columns(s,line.begin,b);
    size_t total=marker_col+(marker_end-b);
    for(size_t p=marker_end;p<content;++p)
        total+=s[p]=='\t' ? 4u-(total%4u) : 1u;
    /* A marker-only item has no content on this line. Its continuation
     * indentation is one column past the marker, independent of trailing
     * whitespace. "-   \n  foo" keeps foo within the item (CommonMark #279).
     * When FIVE OR MORE ASCII spaces separate a marker and actual content,
     * only one belongs to the list marker. The remaining four-plus columns
     * form an indented code block inside the item (#273/#274). Preserve the
     * residual source bytes instead of discarding them as marker padding.
     * The more general tab-expanded marker case remains separately scoped. */
    if(content==line.end) {
        total=marker_col+(marker_end-b)+1u;
    } else if(content-marker_end>4u) {
        bool ascii_spaces=true;
        for(size_t p=marker_end;p<content;++p)
            if(s[p]!=' ') {ascii_spaces=false;break;}
        if(ascii_spaces) {
            total=marker_col+(marker_end-b)+1u;
            content=marker_end+1u;
        }
    }
    *out=(md_list_marker_info){
        marker_col,total,content,start,marker,ordered
    };
    return true;
}

/* A marker-only item cannot interrupt an existing paragraph, whether it is
 * root-owned or within a Blockquote. Ordered interrupters also start at 1.
 * This is an admission rule, not a restriction on empty sibling list items. */
static bool md_list_interrupts_paragraph(const md_list_marker_info *m,
                                         md_line line) {
    return m->content_offset<line.end &&
           (!m->ordered || m->start==1u);
}

/* Both marker-line and continuation-line fences use the same owned item
 * frame.  A recognized opener is consumed even when allocation fails; the
 * caller will propagate d->error and discard the incomplete document. */
static bool md_list_begin_fence(stun_md_document *d,md_list_frame *f,
                                const char *s,size_t begin,size_t end) {
    char marker=0;
    size_t run=0,info_b=0,info_e=0;
    if(!md_fence_start(s,begin,end,&marker,&run,&info_b,&info_e))
        return false;
    const uint32_t node=md_add(d,f->item,STUN_UI_BLOCK_CODE_FENCE,0u);
    if(node==UINT32_MAX) return true;
    md_fence_info_meta(d,node,s+info_b,info_e-info_b);
    if(d->error!=STUN_UI_OK) return true;
    f->fence=(md_fence){marker,run,node};
    f->fence_open=true;
    f->code_blank_lines=0u;
    f->paragraph=f->code=f->quote=f->quote_paragraph=UINT32_MAX;
    return true;
}

/* Recursive admission only follows list markers on the same physical
 * line and is bounded by MD_LIST_MAX_DEPTH. The frame stack, not the C call
 * stack, continues to own all subsequent source-line admission. */
static bool md_list_open(stun_md_document *d,md_list_stack *stack,
                         uint32_t parent,const md_list_marker_info *m,
                         const char *s,size_t end);

/* Use the same ATX grammar and owned IR for headings admitted directly
 * within a list item; no competing inline parser is introduced. */
static bool md_list_begin_atx(stun_md_document *d,md_list_frame *f,
                              const char *s,size_t begin,size_t end) {
    size_t text_begin=begin,text_end=end;
    const unsigned level=md_atx(s,&text_begin,&text_end);
    if(!level) return false;
    const uint32_t heading=md_add(d,f->item,STUN_UI_BLOCK_HEADING,level);
    if(heading!=UINT32_MAX)
        (void)md_inline(d,heading,s+text_begin,text_end-text_begin,0u);
    f->paragraph=f->code=f->quote=f->quote_paragraph=UINT32_MAX;
    return true;
}

/* An inline '>' on a list marker line and subsequent explicitly marked
 * quote lines share the same list-owned quote paragraph. The owned quote
 * node stays a child of the list item; no root quote grammar is duplicated. */
static bool md_list_append_quote(stun_md_document *d,md_list_frame *f,
                                 const char *s,size_t begin,size_t end) {
    if(begin>=end || s[begin]!='>') return false;
    if(f->quote==UINT32_MAX) {
        f->quote=md_add(d,f->item,STUN_UI_BLOCK_QUOTE,0u);
        if(f->quote==UINT32_MAX) return true;
        f->quote_paragraph=UINT32_MAX;
    }
    const size_t body=md_left(s,begin+1u,end);
    if(body==end) {
        f->quote_paragraph=UINT32_MAX;
    } else {
        if(f->quote_paragraph==UINT32_MAX) {
            f->quote_paragraph=md_add(d,f->quote,STUN_UI_BLOCK_PARAGRAPH,0u);
            if(f->quote_paragraph==UINT32_MAX) return true;
        } else if(md_add(d,f->quote_paragraph,STUN_UI_BLOCK_LINE_BREAK,0u)
                  ==UINT32_MAX) return true;
        (void)md_inline(d,f->quote_paragraph,s+body,end-body,0u);
    }
    f->paragraph=UINT32_MAX;
    return true;
}

static bool md_list_append_item(stun_md_document *d,md_list_stack *stack,
                                md_list_frame *f,
                                const md_list_marker_info *m,
                                const char *s,size_t end) {
    char state=0;
    size_t content=m->content_offset;
    const bool task=md_task_marker(s,content,end,&state,&content);
    f->item=md_add(d,f->list,task ? STUN_UI_BLOCK_TASK_ITEM :
                                    STUN_UI_BLOCK_LIST_ITEM,0u);
    if(f->item==UINT32_MAX) return false;
    if(task) {
        const uint32_t checkbox=md_add(d,f->item,STUN_UI_BLOCK_CHECKBOX,0u);
        if(checkbox==UINT32_MAX) return false;
        md_meta(d,checkbox,&state,1u);
        if(d->error!=STUN_UI_OK) return false;
    }
    /* A marker-only item is a genuinely empty item. Do not manufacture
     * an empty paragraph or permit an intervening blank to attach a
     * following top-level paragraph to it. */
    f->empty_item=content==end && !task;
    f->paragraph=f->code=f->quote=f->quote_paragraph=UINT32_MAX;
    f->content_column=m->content_column;
    f->code_blank_lines=0u;
    f->fence_open=f->pending_blank=false;
    if(!f->empty_item) {
        /* The excess marker separation belongs to an indented code block,
         * not a manufactured paragraph. Admit the first code line through
         * the same owned frame as later indented-code continuation lines. */
        if(!task && md_columns(s,content,end)>=4u) {
            f->code=md_add(d,f->item,STUN_UI_BLOCK_CODE_INDENTED,0u);
            if(f->code==UINT32_MAX) return false;
            const md_line initial={content,end,end,false};
            return md_code_line(d,f->code,initial,false);
        }
        /* CommonMark permits list containers to nest on one source line:
         * "- - foo" and "1. - 2. foo". Reuse the same bounded list-frame
         * admission, with nested marker columns relative to this item. */
        md_list_marker_info nested={0};
        const md_line tail={content,end,end,false};
        if(!task && md_list_token(s,tail,&nested) &&
           !md_rule(s,content,end)) {
            if(nested.column>SIZE_MAX-m->content_column ||
               nested.content_column>SIZE_MAX-m->content_column) {
                md_error(d,STUN_UI_CAPACITY_EXCEEDED);
                return false;
            }
            nested.column+=m->content_column;
            nested.content_column+=m->content_column;
            return md_list_open(d,stack,f->item,&nested,s,end);
        }
        /* A list marker may introduce a heading or fenced block directly.
         * Both retain their list-item parent instead of creating a paragraph. */
        if(!task && md_list_begin_atx(d,f,s,content,end))
            return d->error==STUN_UI_OK;
        if(!task && md_list_begin_fence(d,f,s,content,end))
            return d->error==STUN_UI_OK;
        if(!task && md_list_append_quote(d,f,s,content,end))
            return d->error==STUN_UI_OK;
        f->paragraph=md_add(d,f->item,STUN_UI_BLOCK_PARAGRAPH,0u);
        if(f->paragraph==UINT32_MAX) return false;
        if(!md_inline(d,f->paragraph,s+content,end-content,0u)) return false;
    }
    return d->error==STUN_UI_OK;
}

static bool md_list_open(stun_md_document *d,md_list_stack *stack,
                         uint32_t parent,const md_list_marker_info *m,
                         const char *s,size_t end) {
    if(stack->depth>=MD_LIST_MAX_DEPTH) {
        md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
    }
    const uint32_t list=md_add(d,parent,m->ordered ?
        STUN_UI_BLOCK_ORDERED_LIST : STUN_UI_BLOCK_LIST,0u);
    if(list==UINT32_MAX || !md_list_meta(d,list,m->start,m->marker))
        return false;
    md_list_frame *f=&stack->frames[stack->depth++];
    *f=(md_list_frame){0};
    f->list=list;
    f->marker=m->marker;
    f->ordered=m->ordered;
    f->marker_column=m->column;
    f->list_base_column=m->column;
    return md_list_append_item(d,stack,f,m,s,end);
}

/* A blank line first belongs to every still-open candidate container. If a
 * deeper item consumes the next nonblank line, that line proves the blank
 * was internal to the child, not a separator of outer list items. Only the
 * consuming list is loosened; outer list tightness remains unchanged. */
static void md_list_claim_nested_blank(md_list_stack *stack) {
    for(size_t depth=0u;depth+1u<stack->depth;++depth)
        stack->frames[depth].pending_blank=false;
}

/* Defined with the quote-container admission policy below. */
static bool md_quote_lazy_interrupts(const char *s,md_line line,
                                     size_t begin);

static bool md_list_consume(stun_md_document *d,md_list_stack *stack,
                            md_line line) {
    if(!stack->depth) return false;
    const char *s=d->raw;
    if(stack->frames[stack->depth-1u].fence_open &&
       md_blank(s,line.begin,line.end)) {
        md_list_frame *f=&stack->frames[stack->depth-1u];
        md_text(d,f->fence.node,"\n",1u);
        return true;
    }
    if(md_blank(s,line.begin,line.end)) {
        /* A separator can loosen an enclosing list item even when the
         * deepest nested list closes before the next nonblank line. */
        for(size_t depth=0u;depth<stack->depth;++depth)
            stack->frames[depth].pending_blank=true;
        /* Defer interior code blanks until a following indented line proves
         * continuation. EOF or another block discards trailing blanks. */
        md_list_frame *top=&stack->frames[stack->depth-1u];
        if(top->code!=UINT32_MAX) ++top->code_blank_lines;
        return true;
    }
    const size_t indent=md_columns(s,line.begin,md_left(s,line.begin,line.end));
    md_list_marker_info marker={0};
    const bool marker_found=md_list_token(s,line,&marker);
    const size_t line_begin=md_left(s,line.begin,line.end);
    /* An underline indented within an existing item converts the active
     * paragraph to a setext heading BEFORE the thematic-break precedence.
     * An unindented "---" cannot be owned by the item and remains root HR. */
    md_list_frame *active=&stack->frames[stack->depth-1u];
    if(!active->fence_open && !active->pending_blank &&
       active->paragraph!=UINT32_MAX &&
       indent>=active->content_column &&
       indent-active->content_column<=3u) {
        const unsigned level=md_setext(s,line_begin,line.end);
        if(level) {
            d->nodes[active->paragraph].kind=STUN_UI_BLOCK_HEADING;
            d->nodes[active->paragraph].heading_level=level;
            active->paragraph=UINT32_MAX;
            return true;
        }
    }
    /* Thematic breaks interrupt active list items (including '* * *').
     * They do not become a new list item merely because '*' is a marker. */
    if(indent<=3u && md_rule(s,line_begin,line.end) &&
       !stack->frames[stack->depth-1u].fence_open) {
        stack->depth=0u;
        return false;
    }
    while(stack->depth) {
        md_list_frame *f=&stack->frames[stack->depth-1u];
        /* A sibling's marker can shift between 0..3 root columns, while
         * its content indent still determines the nesting boundary. For
         * instance '- a /  - b /   - c' denotes three siblings when the
         * preceding marker is at the previous column. */
        const bool sibling=!f->fence_open && marker_found &&
            marker.column>=f->list_base_column &&
            (marker.column==f->marker_column ||
             (marker.column<f->content_column &&
              marker.column-f->list_base_column<=3u));
        if(sibling) {
            if(f->ordered==marker.ordered && f->marker==marker.marker) {
                if(f->pending_blank) {
                    md_mark_list_loose(d,f->list);
                    md_list_claim_nested_blank(stack);
                }
                f->marker_column=marker.column;
                (void)md_list_append_item(d,stack,f,&marker,s,line.end);
                return true;
            }
            --stack->depth; /* differing marker families never coalesce */
            continue;
        }
        if(f->fence_open) {
            if(indent<f->content_column) {
                --stack->depth;
                continue;
            }
            const size_t body=md_skip_columns(s,line.begin,line.end,
                                               f->content_column);
            const size_t content=md_left(s,body,line.end);
            if(md_fence_close(s,content,line.end,f->fence)) {
                f->fence_open=false;
                return true;
            }
            md_text(d,f->fence.node,s+body,line.end-body);
            if(line.newline) md_text(d,f->fence.node,"\n",1u);
            return true;
        }
        if(f->empty_item && f->pending_blank) {
            --stack->depth;
            continue;
        }
        if(indent>=f->content_column) {
            const size_t body=md_skip_columns(s,line.begin,line.end,
                                              f->content_column);
            if(body==line.begin && f->content_column) {
                md_error(d,STUN_UI_INVALID_ARGUMENT);return true;
            }
            /* A nonblank continuation makes the previously marker-only item
             * nonempty. Only truly empty items are barred from attaching
             * content after a following blank line. */
            f->empty_item=false;
            /* A nested list is admitted after matching this item, never as
             * a new root list. Indentation four columns deeper instead
             * begins a literal code block. */
            if(marker_found && marker.column>=f->content_column &&
               marker.column<=f->content_column+3u) {
                if(f->pending_blank) {
                    md_mark_list_loose(d,f->list);
                    md_list_claim_nested_blank(stack);
                }
                (void)md_list_open(d,stack,f->item,&marker,s,line.end);
                return true;
            }
            const size_t inner_indent=indent-f->content_column;
            if(f->pending_blank && f->code!=UINT32_MAX &&
               inner_indent>=4u) {
                /* Interior blank lines belong to the *same* indented code
                 * node; they are not paragraph/list separators and must not
                 * split one <pre><code> into two blocks. This also works for
                 * quote-owned list frames because no raw prefix is replayed. */
                while(f->code_blank_lines) {
                    md_text(d,f->code,"\n",1u);
                    if(d->error!=STUN_UI_OK) return true;
                    --f->code_blank_lines;
                }
                f->pending_blank=false;
                md_list_claim_nested_blank(stack);
                md_line inner=line;
                inner.begin=body;
                (void)md_code_line(d,f->code,inner,false);
                return true;
            }
            if(f->pending_blank) {
                md_mark_list_loose(d,f->list);
                md_list_claim_nested_blank(stack);
                f->paragraph=f->code=f->quote=f->quote_paragraph=UINT32_MAX;
                f->pending_blank=false;
                f->code_blank_lines=0u;
            }
            const size_t inner_begin=md_left(s,body,line.end);
            if(inner_indent<=3u &&
               md_list_begin_atx(d,f,s,inner_begin,line.end))
                return true;
            if(inner_indent<=3u &&
               md_list_begin_fence(d,f,s,inner_begin,line.end))
                return true;
            if(inner_indent>=4u && f->paragraph==UINT32_MAX &&
               f->quote==UINT32_MAX) {
                if(f->code==UINT32_MAX) {
                    f->code=md_add(d,f->item,STUN_UI_BLOCK_CODE_INDENTED,0u);
                    if(f->code==UINT32_MAX) return true;
                }
                md_line inner=line;
                inner.begin=body;
                (void)md_code_line(d,f->code,inner,false);
                return true;
            }
            f->code=UINT32_MAX;
            if(md_list_append_quote(d,f,s,body,line.end)) return true;
            f->quote=f->quote_paragraph=UINT32_MAX;
            if(f->paragraph==UINT32_MAX) {
                f->paragraph=md_add(d,f->item,STUN_UI_BLOCK_PARAGRAPH,0u);
                if(f->paragraph==UINT32_MAX) return true;
            } else (void)md_add(d,f->paragraph,STUN_UI_BLOCK_LINE_BREAK,0u);
            const size_t text_begin=md_left(s,body,line.end);
            (void)md_inline(d,f->paragraph,s+text_begin,
                            line.end-text_begin,0u);
            return true;
        }
        /* Lazy paragraph continuation may also belong to the list-owned
         * inner quote (CommonMark #292/#293). Only its already-open text
         * may continue: new block openers, blanks and competing quote markers
         * must not be silently admitted into the borrowed paragraph. */
        const uint32_t continuation=f->quote_paragraph!=UINT32_MAX ?
                                    f->quote_paragraph:f->paragraph;
        if(!f->pending_blank &&
           (!marker_found || marker.column>3u) &&
           continuation!=UINT32_MAX && !md_rule(s,line.begin,line.end) &&
           s[line_begin]!='>' &&
           (f->quote_paragraph==UINT32_MAX ||
            !md_quote_lazy_interrupts(s,line,line_begin))) {
            if(md_add(d,continuation,STUN_UI_BLOCK_LINE_BREAK,0u)
               ==UINT32_MAX) return true;
            (void)md_inline(d,continuation,s+line_begin,
                            line.end-line_begin,0u);
            return true;
        }
        --stack->depth;
    }
    return false;
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

/* Lazy quote continuation is admitted only into an already open paragraph.
 * A new block opener ends that admission; indented candidate markers are
 * ordinary continuation text, not a new root list. */
static bool md_quote_lazy_interrupts(const char *s,md_line line,
                                     size_t begin) {
    /* Four-space indentation after an open quote paragraph is literal
     * lazy text, not a root heading/rule/fence/list admission. */
    if(md_columns(s,line.begin,begin)>3u) return false;
    if(md_rule(s,begin,line.end)) return true;
    size_t hbegin=begin,hend=line.end;
    if(md_atx(s,&hbegin,&hend)) return true;
    char mark=0;size_t count=0,info_begin=0,info_end=0;
    if(md_fence_start(s,begin,line.end,
                      &mark,&count,&info_begin,&info_end)) return true;
    md_list_marker_info marker={0};
    if(md_list_token(s,line,&marker) && marker.column<=3u &&
       md_list_interrupts_paragraph(&marker,line)) return true;
    return false;
}


enum { MD_QUOTE_MAX_DEPTH=32 };
typedef struct md_quote_stack {
    uint32_t nodes[MD_QUOTE_MAX_DEPTH];
    size_t depth;
    uint32_t paragraph, code;
    md_fence fence;
    md_list_stack lists; /* bounded list frames owned by the active quote */
    bool lazy, fence_open;
} md_quote_stack;

/* Match all explicit quote markers using CommonMark's <=3-column leading
 * indent per layer and consume at most one optional whitespace character
 * after each '>'. No unbounded recursion or separate Markdown frontend. */
static size_t md_quote_prefix(const char *s,md_line line,size_t *body) {
    size_t pos=line.begin,depth=0u;
    while(pos<line.end) {
        size_t next=pos,column=0u;
        while(next<line.end && md_space(s[next]) && column<=3u) {
            const size_t step=s[next]=='\t' ? 4u-column%4u : 1u;
            column+=step;
            if(column>3u) break;
            ++next;
        }
        if(column>3u || next>=line.end || s[next]!='>') break;
        ++depth;
        pos=next+1u;
        if(pos<line.end && md_space(s[pos])) ++pos;
        if(depth>MD_QUOTE_MAX_DEPTH) break;
    }
    *body=pos;
    return depth;
}

/* One explicitly owned stack of quote containers. All admitted blocks keep
 * their true parent in the pre-order RichDoc, with one document lifetime. */
static bool md_quote_line(stun_md_document *d,md_quote_stack *q,
                          md_line line,size_t level,size_t body) {
    if(level>MD_QUOTE_MAX_DEPTH) {
        md_error(d,STUN_UI_CAPACITY_EXCEEDED);return false;
    }
    if(level!=q->depth) {
        if(level<q->depth) q->depth=level;
        else {
            while(q->depth<level) {
                const uint32_t parent=q->depth ? q->nodes[q->depth-1u] : 0u;
                const uint32_t node=md_add(d,parent,STUN_UI_BLOCK_QUOTE,0u);
                if(node==UINT32_MAX) return false;
                q->nodes[q->depth++]=node;
            }
        }
        q->paragraph=q->code=UINT32_MAX;
        q->lists.depth=0u; /* no list state across quote generation/depth */
        q->fence_open=q->lazy=false;
    }
    const uint32_t parent=q->nodes[level-1u];
    const char *s=d->raw;
    const size_t begin=md_left(s,body,line.end);
    const size_t indent=md_columns(s,body,begin);
    if(q->fence_open) {
        if(indent<=3u && md_fence_close(s,begin,line.end,q->fence)) {
            q->fence_open=false;
            return true;
        }
        /* Quoted fence payload excludes quote markers, never the raw UTF-8
         * bytes of the owned source. */
        md_text(d,q->fence.node,s+body,line.end-body);
        if(line.newline) md_text(d,q->fence.node,"\n",1u);
        return d->error==STUN_UI_OK;
    }
    md_line inner=line;
    inner.begin=body; /* all list columns relative to the quote content */
    if(md_blank(s,body,line.end)) {
        if(q->lists.depth) (void)md_list_consume(d,&q->lists,inner);
        q->paragraph=q->code=UINT32_MAX;
        q->lazy=false;
        return d->error==STUN_UI_OK;
    }
    if(q->lists.depth && md_list_consume(d,&q->lists,inner))
        return d->error==STUN_UI_OK;
    if(d->error!=STUN_UI_OK)return false;
    if(q->code!=UINT32_MAX) {
        if(indent>=4u) {
            md_line code_line=line;
            code_line.begin=body;
            return md_code_line(d,q->code,code_line,false);
        }
        q->code=UINT32_MAX;
    }
    if(q->paragraph!=UINT32_MAX && indent<=3u) {
        const unsigned setext=md_setext(s,begin,line.end);
        if(setext) {
            d->nodes[q->paragraph].kind=STUN_UI_BLOCK_HEADING;
            d->nodes[q->paragraph].heading_level=setext;
            q->paragraph=UINT32_MAX;q->lazy=false;
            return true;
        }
    }
    if(indent<=3u && md_rule(s,begin,line.end)) {
        if(md_add(d,parent,STUN_UI_BLOCK_RULE,0u)==UINT32_MAX)return false;
        q->paragraph=UINT32_MAX;q->lazy=false;return true;
    }
    char marker=0u;size_t run=0u,info_begin=0u,info_end=0u;
    if(indent<=3u && md_fence_start(s,begin,line.end,
                                    &marker,&run,&info_begin,&info_end)) {
        const uint32_t node=md_add(d,parent,STUN_UI_BLOCK_CODE_FENCE,0u);
        if(node==UINT32_MAX)return false;
        md_fence_info_meta(d,node,s+info_begin,info_end-info_begin);
        if(d->error!=STUN_UI_OK)return false;
        q->fence=(md_fence){marker,run,node};
        q->fence_open=true;
        q->paragraph=UINT32_MAX;q->lazy=false;
        return true;
    }
    if(indent<=3u) {
        size_t hbegin=begin,hend=line.end;
        const unsigned heading=md_atx(s,&hbegin,&hend);
        if(heading) {
            const uint32_t node=md_add(d,parent,STUN_UI_BLOCK_HEADING,heading);
            if(node==UINT32_MAX)return false;
            q->paragraph=UINT32_MAX;q->lazy=false;
            return md_inline(d,node,s+hbegin,hend-hbegin,0u);
        }
    }
    /* An admitted quote list belongs to this quote node, not the document.
     * A numbered marker >1 never interrupts an existing paragraph. */
    md_list_marker_info item={0};
    if(indent<=3u && md_list_token(s,inner,&item) &&
       (q->paragraph==UINT32_MAX ||
        md_list_interrupts_paragraph(&item,inner)) &&
       !md_rule(s,begin,line.end)) {
        if(!md_list_open(d,&q->lists,parent,&item,s,line.end))
            return false;
        q->paragraph=q->code=UINT32_MAX;
        q->lazy=false;
        return true;
    }
    if(indent>=4u && q->paragraph==UINT32_MAX) {
        q->code=md_add(d,parent,STUN_UI_BLOCK_CODE_INDENTED,0u);
        if(q->code==UINT32_MAX)return false;
        md_line code_line=line;
        code_line.begin=body;
        q->lazy=false;
        return md_code_line(d,q->code,code_line,false);
    }
    if(q->paragraph==UINT32_MAX) {
        q->paragraph=md_add(d,parent,STUN_UI_BLOCK_PARAGRAPH,0u);
        if(q->paragraph==UINT32_MAX)return false;
    } else if(md_add(d,q->paragraph,STUN_UI_BLOCK_LINE_BREAK,0u)
              ==UINT32_MAX) return false;
    q->lazy=true;
    return md_inline(d,q->paragraph,s+begin,line.end-begin,0u);
}

/* Bounded multiline code-span admission within ROOT paragraphs only.
 * The legacy C11 per-line inline scan remains canonical for all other
 * constructs. Read ahead for the SAME exact-length closing backtick run,
 * never crossing a blank line or a block that interrupts this paragraph.
 * At most 64 continuation lines are examined per opener to keep repeated
 * unmatched candidates from causing unbounded quadratic rescanning.
 * Nested quote/list paragraph admission is a separate #182 slice. */
static bool md_root_code_span_interrupts(stun_md_document *d,md_line line) {
    const char *s=d->raw;
    const size_t begin=md_left(s,line.begin,line.end);
    if(begin==line.end) return true;
    const size_t indent=md_columns(s,line.begin,begin);
    if(indent>3u) return false; /* still part of an open paragraph */
    if(md_rule(s,begin,line.end) || md_setext(s,begin,line.end)) return true;
    size_t hb=begin,he=line.end;
    if(md_atx(s,&hb,&he)) return true;
    char fence=0;size_t count=0,info_begin=0,info_end=0;
    if(md_fence_start(s,begin,line.end,&fence,&count,
                      &info_begin,&info_end)) return true;
    size_t quote_body=line.begin;
    if(md_quote_prefix(s,line,&quote_body)) return true;
    md_list_marker_info item={0};
    return md_list_token(s,line,&item) &&
           md_list_interrupts_paragraph(&item,line);
}

static bool md_root_multiline_code_span(stun_md_document *d,
                                        uint32_t *paragraph,md_line first,
                                        size_t begin,size_t *next_pos) {
    const char *s=d->raw;
    if(begin>=first.end || s[begin]!=(char)0x60) return false;
    size_t run=1u;
    while(begin+run<first.end && s[begin+run]==(char)0x60) ++run;
    /* A same-line match already belongs to the normal inline scanner. */
    if(md_code_span_close(s,first.end,begin+run,run)!=SIZE_MAX)
        return false;
    size_t pos=first.next;
    for(size_t lines=0u;pos<d->raw_size && lines<64u;++lines) {
        const md_line line=md_read_line(d,pos);
        if(md_root_code_span_interrupts(d,line)) break;
        const size_t close=md_code_span_close(s,line.end,line.begin,run);
        if(close!=SIZE_MAX) {
            if(*paragraph==UINT32_MAX) {
                *paragraph=md_add(d,0u,STUN_UI_BLOCK_PARAGRAPH,0u);
                if(*paragraph==UINT32_MAX) return true;
            } else if(md_add(d,*paragraph,STUN_UI_BLOCK_LINE_BREAK,0u)
                      ==UINT32_MAX) return true;
            const uint32_t code=md_add(d,*paragraph,STUN_UI_BLOCK_CODE_SPAN,0u);
            if(code==UINT32_MAX) return true;
            md_code_span_text(d,code,s,begin+run,close);
            if(d->error!=STUN_UI_OK) return true;
            if(close+run<line.end)
                (void)md_inline(d,*paragraph,s+close+run,
                                line.end-close-run,0u);
            *next_pos=line.next;
            return true;
        }
        pos=line.next;
    }
    return false;
}

/* Root-paragraph inline ownership spans physical lines when * / _ runs
 * pair across a SOFT line break. Collect only an unindented, uninterrupted,
 * bounded root paragraph prefix; no quote/list/container, Setext, code,
 * hard-break admission, or competing block may be crossed here.
 *
 * The original raw bytes remain owned unchanged by d->raw. This scratch
 * UTF-8 buffer normalizes LF/CRLF solely for semantic inline processing.
 * The same md_inline/md_emph_pairs code produces the typed RichDoc; there
 * is no second Markdown grammar and no delayed pointer into scratch.
 *
 * Delimiters may be parsed twice in this test-and-admit path. Both passes
 * remain subject to the identical work cap and the 32-deep owner ceiling;
 * this is bounded by the full original raw-byte input budget. */
static bool md_root_paragraph_emphasis(stun_md_document *d,
                                        uint32_t *paragraph,
                                        md_line first,size_t begin,
                                        size_t *next_pos) {
    enum { MD_PARAGRAPH_LOOKAHEAD_LINES=32 };
    const char *s=d->raw;
    if(*paragraph!=UINT32_MAX || begin>=first.end ||
       memchr(s+begin,(char)0x60,first.end-begin)!=NULL)
        return false;
    /* #185 supplies explicit hard/soft origin even when the marker pair
     * crosses spaces/backslash; keep the complete physical line in scratch. */
    md_line lines[MD_PARAGRAPH_LOOKAHEAD_LINES]={0};
    size_t begins[MD_PARAGRAPH_LOOKAHEAD_LINES]={0};
    lines[0]=first;begins[0]=begin;
    size_t count=1u,bytes=first.end-begin;
    for(size_t cursor=first.next;
        cursor<d->raw_size && count<MD_PARAGRAPH_LOOKAHEAD_LINES;) {
        const md_line next=md_read_line(d,cursor);
        const size_t at=md_left(s,next.begin,next.end);
        if(at!=next.begin || at==next.end ||
           md_root_code_span_interrupts(d,next) ||
           memchr(s+at,(char)0x60,next.end-at)!=NULL)
            break;
        if(bytes>=d->raw_size || next.end-at>d->raw_size-bytes-1u)
            break;
        bytes+=1u+next.end-at;
        lines[count]=next;begins[count]=at;
        ++count;cursor=next.next;
    }
    if(count<2u)return false;
    /* No trailing source whitespace is semantic text on the final physical
     * line, including before a Setext underline (#82). Intermediate lines
     * retain their suffix so LF admission can classify HARD vs SOFT. */
    const size_t last_end=md_right(s,begins[count-1u],lines[count-1u].end);
    const size_t removed=lines[count-1u].end-last_end;
    if(removed>bytes)return false;
    bytes-=removed;
    if(!bytes || bytes>=SIZE_MAX/sizeof(md_emph_event))return false;
    char *joined=(char*)malloc(bytes+1u);
    md_emph_event *events=(md_emph_event*)calloc(bytes,sizeof(*events));
    if(!joined || !events) {
        free(joined);free(events);
        md_error(d,STUN_UI_OUT_OF_MEMORY);
        return false;
    }
    size_t used=0u;
    for(size_t i=0u;i<count;++i) {
        if(i) joined[used++]='\n';
        const size_t end=(i+1u==count)?last_end:lines[i].end;
        const size_t len=end-begins[i];
        if(len) memcpy(joined+used,s+begins[i],len);
        used+=len;
    }
    joined[used]='\0';
    if(used!=bytes || !md_emph_pairs(d,joined,bytes,events)) {
        if(d->error==STUN_UI_OK)md_error(d,STUN_UI_INVALID_ARGUMENT);
        free(events);free(joined);return false;
    }
    /* Check actual matched pair ancestry across at least one LF. Merely
     * seeing markers on both lines is not sufficient admission. */
    size_t openings[33]={0u},last_lf=SIZE_MAX;
    unsigned nested=0u;
    bool across=false;
    for(size_t p=0u;p<bytes;++p) {
        if(joined[p]=='\n')last_lf=p;
        if(events[p].close) {
            if(nested && last_lf!=SIZE_MAX &&
               openings[nested-1u]<last_lf)
                across=true;
            if(nested)--nested;
        }
        if(events[p].open) {
            if(nested>=32u)break; /* normal per-line bounded parser */
            openings[nested++]=p;
        }
    }
    free(events);
    if(!across) {free(joined);return false;}
    const uint32_t node=md_add(d,0u,STUN_UI_BLOCK_PARAGRAPH,0u);
    if(node==UINT32_MAX) {free(joined);return false;}
    *paragraph=node;
    const bool ok=md_inline(d,node,joined,bytes,0u);
    free(joined);
    if(!ok)return true; /* parse error is already recorded; no publication */
    *next_pos=lines[count-1u].next;
    return true;
}

/* #185: root paragraph line boundaries must decide whitespace and break
 * origin BEFORE publishing decoded RichDoc text. A backward trim of the
 * shared text arena would invalidate borrowed byte spans and node owners.
 * Do not cross competing blocks, list/quote generations or blank lines. */
static bool md_root_paragraph_continues(stun_md_document *d,md_line current) {
    if(!current.newline || current.next>=d->raw_size)return false;
    const md_line next=md_read_line(d,current.next);
    const char *s=d->raw;
    if(md_blank(s,next.begin,next.end))return false;
    /* An indented continuation is still a paragraph continuation when its
     * owner already exists. This preserves the existing paragraph precedence. */
    return !md_root_code_span_interrupts(d,next);
}

/* Collect only single-line ASCII root reference definitions. The scan is
 * BEFORE inline admission, so a later [ref]: ... can bind an earlier
 * [text][ref]. Definitions inside code fences, indented code, lists or
 * quotes are NOT recognized by this restricted root-only slice. This
 * prepass has no AST side effects and cannot reinterpret decoded entities
 * as syntax. Tables/complex reference label folding remain separate.
 *
 * Definition entries also record exact source-line offsets to suppress
 * their block output; duplicates are consumed but lookup selects the FIRST.
 * Fixed 128-entry budget prevents unbounded hash/state growth. */
static bool md_reference_definition_line(
    const stun_md_document *d,size_t begin) {
    size_t low=0u,high=d->reference_count;
    while(low<high) {
        const size_t mid=low+(high-low)/2u;
        if(d->references[mid].source_line<begin)low=mid+1u;
        else high=mid;
    }
    return low<d->reference_count &&
           d->references[low].source_line==begin;
}

static bool md_reference_collect_line(stun_md_document *d,md_line line,
                                      size_t begin) {
    const char *s=d->raw;
    if(begin>=line.end || s[begin]!='[')return false;
    const size_t close=md_match(s,line.end,begin+1u,']',1u);
    if(close==SIZE_MAX || close+1u>=line.end ||
       s[close+1u]!=':')return false;
    char key[MD_REF_KEY_MAX+1u];
    if(!md_reference_key(s,begin+1u,close,key))return false;
    const size_t target_begin=md_left(s,close+2u,line.end);
    const size_t length=line.end-target_begin;
    if(!length || length>SIZE_MAX-3u)return false;
    char *scratch=(char*)malloc(length+3u);
    if(!scratch) {
        md_error(d,STUN_UI_OUT_OF_MEMORY);return false;
    }
    scratch[0]='(';
    memcpy(scratch+1u,s+target_begin,length);
    scratch[length+1u]=')';
    scratch[length+2u]='\0';
    md_link_parts parts={0};
    size_t close_paren=SIZE_MAX;
    const bool valid=md_link_target_parse(
        scratch,length+2u,0u,&parts,&close_paren) &&
        close_paren==length+1u;
    if(!valid) {free(scratch);return false;}
    if(d->reference_count>=MD_REF_MAX) {
        md_error(d,STUN_UI_CAPACITY_EXCEEDED);free(scratch);return true;
    }
    char *href=NULL,*title=NULL;
    size_t href_len=0u,title_len=0u;
    if(!md_decode_component(
           d,scratch+parts.destination_begin,
           parts.destination_end-parts.destination_begin,
           &href,&href_len)) {
        free(scratch);return true;
    }
    if(parts.has_title &&
       !md_decode_component(d,scratch+parts.title_begin,
                            parts.title_end-parts.title_begin,
                            &title,&title_len)) {
        free(href);free(scratch);return true;
    }
    if(!d->references) {
        d->references=(md_reference_binding*)calloc(
            MD_REF_MAX,sizeof(*d->references));
        if(!d->references) {
            md_error(d,STUN_UI_OUT_OF_MEMORY);
            free(title);free(href);free(scratch);return true;
        }
    }
    md_reference_binding *item=&d->references[d->reference_count++];
    memcpy(item->key,key,strlen(key)+1u);
    item->href=href;item->href_bytes=href_len;
    item->title=title;item->title_bytes=title_len;
    item->has_title=parts.has_title;
    item->source_line=line.begin;
    free(scratch);
    return true;
}

static void md_reference_collect_root(stun_md_document *d) {
    const char *s=d->raw;
    bool paragraph_started=false;
    bool fenced=false;
    bool container_pending=false;
    /* Root-only admission guard, not a second nested-list grammar. Keep
     * the outer list content column across blank lines: a blank does not
     * terminate a list item when the next line remains content-indented. */
    size_t list_content_column=0u;
    bool after_list_blank=false;
    md_fence fence={0u,0u,UINT32_MAX};
    for(size_t pos=0u;pos<d->raw_size && d->error==STUN_UI_OK;) {
        const md_line line=md_read_line(d,pos);
        pos=line.next;
        const size_t begin=md_left(s,line.begin,line.end);
        const size_t columns=md_columns(s,line.begin,begin);
        if(fenced) {
            if(columns<=3u && md_fence_close(s,begin,line.end,fence))
                fenced=false;
            continue;
        }
        if(md_blank(s,line.begin,line.end)) {
            paragraph_started=false;
            container_pending=false;
            after_list_blank=list_content_column!=0u;
            continue;
        }
        if(after_list_blank) {
            /* The real list parser matches the existing item's content
             * column even after a blank. Never donate a continuation's
             * [label]: syntax to the ROOT reference table. Dedenting below
             * that column proves that this restricted root list has ended. */
            if(columns>=list_content_column)container_pending=true;
            else list_content_column=0u;
            after_list_blank=false;
        }
        /* Root thematic breaks can interrupt a preceding list/quote/code
         * owner without a blank line. Release its conservative reference
         * guard only when the production block parser can prove ROOT
         * ownership: spaced/star rules are unambiguous; a bare Setext
         * underline inside a still-indented list item is NOT.
         *
         * A root paragraph's "---" is a Setext heading, not a rule.
         * No nested definition grammar or list-owned binding is added. */
        const unsigned rule_setext=columns<=3u?
            md_setext(s,begin,line.end):0u;
        if(columns<=3u && md_rule(s,begin,line.end) &&
           !(paragraph_started && rule_setext) &&
           (!container_pending || !list_content_column ||
            columns<list_content_column || !rule_setext)) {
            paragraph_started=false;
            container_pending=false;
            list_content_column=0u;
            continue;
        }
        char marker=0;size_t count=0,ib=0,ie=0;
        if(columns<=3u &&
           md_fence_start(s,begin,line.end,&marker,&count,&ib,&ie)) {
            fenced=true;
            fence=(md_fence){marker,count,UINT32_MAX};
            paragraph_started=false;continue;
        }
        md_list_marker_info list_marker={0};
        const bool list_start=columns<=3u &&
                              md_list_token(s,line,&list_marker);
        if(columns>3u || s[begin]=='>' || list_start) {
            /* The outermost content column is sufficient for bounded
             * root-definition admission; nested list definitions remain
             * deferred. A new dedented list marker starts a new guard. */
            if(list_start &&
               (!list_content_column ||
                list_marker.column<list_content_column))
                list_content_column=list_marker.content_column;
            paragraph_started=false;
            container_pending=true;
            continue;
        }
        if(container_pending)continue;
        /* A Setext underline ends its preceding root paragraph in the
         * production block parser. The prepass must recognize that same
         * boundary before admitting a following root definition. Invalid
         * or standalone underlines never reset paragraph ownership. */
        if(paragraph_started && columns<=3u &&
           md_setext(s,begin,line.end)) {
            paragraph_started=false;
            continue;
        }
        if(md_rule(s,begin,line.end)) {
            paragraph_started=false;continue;
        }
        size_t heading_begin=begin,heading_end=line.end;
        if(md_atx(s,&heading_begin,&heading_end)) {
            paragraph_started=false;continue;
        }
        if(!paragraph_started &&
           md_reference_collect_line(d,line,begin)) {
            paragraph_started=false;continue;
        }
        paragraph_started=true;
    }
}

static void md_reference_clear(stun_md_document *d) {
    if(!d)return;
    for(size_t i=0u;i<d->reference_count;++i) {
        free(d->references[i].href);
        free(d->references[i].title);
    }
    free(d->references);
    d->references=NULL;
    d->reference_count=0u;
}

static void md_parse_blocks(stun_md_document *d) {
    uint32_t paragraph = UINT32_MAX;
    md_root_break pending_break=MD_ROOT_BREAK_SOFT;
    uint32_t indented = UINT32_MAX;
    size_t pending_code_blank = SIZE_MAX;
    md_list_stack lists={0};
    bool code_open=false;
    md_quote_stack quotes={0};
    quotes.paragraph=quotes.code=UINT32_MAX;
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
        if(md_list_consume(d,&lists,line)) continue;
        if(d->error!=STUN_UI_OK) return;
        /* A root quote container is matched before admitting root blocks.
         * Lazy lines may continue only the last quoted paragraph and never
         * create a new quote or survive a blank/competing block opener. */
        size_t quote_body=line.begin;
        const size_t quote_depth=md_quote_prefix(s,line,&quote_body);
        if(quote_depth) {
            /* A continuing nested quote paragraph may omit any number of
             * innermost markers. Match already-open quote containers
             * before closing them; block starts and blanks are not lazy. */
            if(quote_depth<quotes.depth && quotes.lazy &&
               quotes.paragraph!=UINT32_MAX &&
               quotes.lists.depth==0u && !quotes.fence_open &&
               !md_blank(s,quote_body,line.end)) {
                md_line inner=line;
                inner.begin=quote_body;
                const size_t content=md_left(s,quote_body,line.end);
                if(!md_quote_lazy_interrupts(s,inner,content)) {
                    if(md_add(d,quotes.paragraph,STUN_UI_BLOCK_LINE_BREAK,0u)
                       ==UINT32_MAX ||
                       !md_inline(d,quotes.paragraph,s+content,
                                  line.end-content,0u))
                        return;
                    continue;
                }
            }
            if(indented!=UINT32_MAX) {
                indented=UINT32_MAX;
                pending_code_blank=SIZE_MAX;
            }
            paragraph=UINT32_MAX;
            if(!md_quote_line(d,&quotes,line,quote_depth,quote_body)) return;
            continue;
        }
        if(quotes.depth) {
            /* A missing OUTER '>' can lazily continue the deepest paragraph
             * of a list-owned INNER quote, but not a generic list paragraph.
             * This is bounded by the existing active quote/list generations. */
            if(quotes.lists.depth) {
                md_list_frame *top=
                    &quotes.lists.frames[quotes.lists.depth-1u];
                if(top->quote_paragraph!=UINT32_MAX &&
                   !top->pending_blank &&
                   !md_blank(s,line.begin,line.end) &&
                   !md_quote_lazy_interrupts(s,line,begin)) {
                    if(md_list_consume(d,&quotes.lists,line)) continue;
                    if(d->error!=STUN_UI_OK) return;
                }
            }
            if(quotes.lazy && quotes.paragraph!=UINT32_MAX &&
               !md_blank(s,line.begin,line.end) &&
               !md_quote_lazy_interrupts(s,line,begin)) {
                if(md_add(d,quotes.paragraph,STUN_UI_BLOCK_LINE_BREAK,0u)
                   ==UINT32_MAX ||
                   !md_inline(d,quotes.paragraph,s+begin,end-begin,0u))
                    return;
                continue;
            }
            quotes.depth=0u;
            quotes.paragraph=quotes.code=UINT32_MAX;
            quotes.lists.depth=0u;
            quotes.fence_open=quotes.lazy=false;
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
            continue;
        }
        /* Definitions are recorded by the bounded root prepass. Do not
         * emit their RAW syntax as paragraph TEXT; all link metadata was
         * already copied and is consulted only during this parse. */
        if(paragraph==UINT32_MAX && lists.depth==0u && indent<=3u &&
           md_reference_definition_line(d,line.begin)) {
            pending_break=MD_ROOT_BREAK_SOFT;
            continue;
        }
        /* Four indentation columns select literal code before checking ATX,
         * thematic rules, fences, or other block markers. Indented code
         * cannot interrupt a paragraph or the active list-item continuation. */
        if(paragraph==UINT32_MAX && lists.depth==0u &&
           md_code_unindent(s,line.begin,line.end)>line.begin) {
            indented=md_add(d,0u,STUN_UI_BLOCK_CODE_INDENTED,0u);
            if(indented==UINT32_MAX || !md_code_line(d,indented,line,false))
                return;
            continue;
        }
        unsigned setext = md_setext(s,begin,end);
        if (paragraph!=UINT32_MAX && indent<=3 && setext) {
            d->nodes[paragraph].kind=STUN_UI_BLOCK_HEADING;
            d->nodes[paragraph].heading_level=setext;
            paragraph=UINT32_MAX;
            continue;
        }
        if (indent <= 3 && md_rule(s,begin,end)) {
            (void)md_add(d,0u,STUN_UI_BLOCK_RULE,0);
            paragraph=UINT32_MAX;
            continue;
        }
        char marker=0; size_t run=0, info_b=0,info_e=0;
        if (indent<=3 && md_fence_start(s,begin,end,
                &marker,&run,&info_b,&info_e)) {
            const uint32_t node=md_add(d,0u,STUN_UI_BLOCK_CODE_FENCE,0);
            if (node==UINT32_MAX) return;
            md_fence_info_meta(d,node,s+info_b,info_e-info_b);
            fence=(md_fence){marker,run,node};
            code_open=true;
            paragraph=UINT32_MAX;
            continue;
        }
        if (indent<=3) {
            size_t hstart=begin, hend=end;
            const unsigned level=md_atx(s,&hstart,&hend);
            if (level) {
                const uint32_t node=md_add(d,0u,STUN_UI_BLOCK_HEADING,level);
                if (node==UINT32_MAX) return;
                (void)md_inline(d,node,s+hstart,hend-hstart,0);
                paragraph=UINT32_MAX;
                continue;
            }
        }
        /* Start a table only at a new block boundary. */
        if (indent<=3 && paragraph==UINT32_MAX &&
            lists.depth==0u && pos<d->raw_size) {
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
                paragraph=UINT32_MAX;
                continue;
            }
        }
        md_list_marker_info top_marker={0};
        if(indent<=3 && md_list_token(s,line,&top_marker) &&
           (paragraph==UINT32_MAX ||
            md_list_interrupts_paragraph(&top_marker,line)) &&
           !md_rule(s,begin,end)) {
            if(!md_list_open(d,&lists,0u,&top_marker,s,end)) return;
            paragraph=UINT32_MAX;
            continue;
        }
        /* A root-paragraph code span can own multiple physical lines.
         * Consume them as ONE typed code node before emitting soft breaks. */
        if(md_root_multiline_code_span(d,&paragraph,line,begin,&pos)) {
            if(d->error!=STUN_UI_OK) return;
            continue;
        }
        /* #182: allow matched runs across SOFT physical paragraph lines
         * without permitting a match across a block/container boundary. */
        if(paragraph==UINT32_MAX && lists.depth==0u &&
           md_root_paragraph_emphasis(d,&paragraph,line,begin,&pos)) {
            if(d->error!=STUN_UI_OK)return;
            pending_break=MD_ROOT_BREAK_SOFT;
            continue;
        }
        if(d->error!=STUN_UI_OK)return;
        /* This slice owns root paragraph semantics only. Nested list/quote
         * breaks still use their existing parser until scoped admission. */
        const bool root=lists.depth==0u && quotes.depth==0u;
        const bool continues=root && md_root_paragraph_continues(d,line);
        size_t text_end=end;
        const md_root_break next_break=root?
            md_root_line_content(s,begin,end,continues,&text_end):
            MD_ROOT_BREAK_SOFT;
        if (paragraph==UINT32_MAX) {
            paragraph=md_add(d,0u,STUN_UI_BLOCK_PARAGRAPH,0);
            if (paragraph==UINT32_MAX) return;
        } else if(root) {
            if(!md_root_emit_break(d,paragraph,pending_break))return;
        } else if(md_add(d,paragraph,STUN_UI_BLOCK_LINE_BREAK,0)==
                  UINT32_MAX)return;
        if(!md_inline(d,paragraph,s+begin,text_end-begin,0u))return;
        pending_break=next_break;
    }
}

void stun_md_document_destroy(stun_md_document *d) {
    if (!d) return;
    free(d->raw);
    free(d->text);
    free(d->metadata);
    free(d->nodes);
    free(d->node_metadata);
    md_reference_clear(d);
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
    d->metadata_capacity=length;
    d->text_capacity=length;
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
    md_reference_collect_root(d);
    if(d->error==STUN_UI_OK)md_parse_blocks(d);
    /* Parse-local binding state is discarded before the public RichDoc
     * snapshot is validated or returned. No raw runtime binding persists. */
    md_reference_clear(d);
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
