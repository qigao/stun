#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TICK "\x60"
#define VERIFY(expr) do { \
    if(!(expr)) { \
        fprintf(stderr,"%s:%d: failed: %s\n",__FILE__,__LINE__,#expr); \
        failure=1;goto done; \
    } \
} while(0)

typedef struct reference_fixture {
    const char *markdown,*href,*title,*visible,*html;
} reference_fixture;

static size_t kind_count(stun_ui_rich_document_view v,
                         stun_ui_block_kind kind) {
    size_t sum=0u;
    for(size_t i=0u;i<v.document.block_count;++i)
        sum+=(v.document.blocks[i].kind==kind);
    return sum;
}

static int test_reference(const reference_fixture *f) {
    int failure=0;
    stun_md_document *doc=NULL;
    const size_t n=strlen(f->markdown);
    VERIFY(stun_md_parse(f->markdown,n,128u,n+1u,&doc)==STUN_UI_OK);
    VERIFY(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    VERIFY(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
    VERIFY(v.raw_markdown_bytes==n);
    VERIFY(!memcmp(v.raw_markdown,f->markdown,n));
    VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==1u);
    VERIFY(kind_count(v,STUN_UI_BLOCK_LINK_TITLE)==(f->title?1u:0u));
    VERIFY(kind_count(v,STUN_UI_BLOCK_PARAGRAPH)<=1u);
    bool href_found=false,title_found=!f->title,visible_found=false;
    for(size_t i=1u;i<v.document.block_count;++i) {
        const stun_ui_block *node=&v.document.blocks[i];
        VERIFY(node->parent_index<i);
        if(node->kind==STUN_UI_BLOCK_LINK ||
           node->kind==STUN_UI_BLOCK_LINK_TITLE) {
            const char *meta=NULL;size_t size=SIZE_MAX;
            VERIFY(stun_ui_rich_document_metadata(
                &v,i,&meta,&size)==STUN_UI_OK);
            const char *expect=node->kind==STUN_UI_BLOCK_LINK?
                               f->href:f->title;
            VERIFY(expect!=NULL);
            VERIFY(size==strlen(expect));
            VERIFY(!size || (meta&&!memcmp(meta,expect,size)));
            if(node->kind==STUN_UI_BLOCK_LINK)href_found=true;
            else {
                VERIFY(node->parent_index+1u==i);
                VERIFY(v.document.blocks[node->parent_index].kind==
                       STUN_UI_BLOCK_LINK);
                title_found=true;
            }
        }
        if(node->kind==STUN_UI_BLOCK_TEXT &&
           v.document.blocks[node->parent_index].kind==STUN_UI_BLOCK_LINK) {
            const size_t bytes=node->byte_end-node->byte_begin;
            VERIFY(node->byte_end<=v.document.source_bytes);
            if(bytes==strlen(f->visible) &&
               !memcmp(v.document.source+node->byte_begin,
                       f->visible,bytes))visible_found=true;
        }
    }
    VERIFY(href_found && title_found && visible_found);
    const size_t expected=strlen(f->html);
    size_t need=SIZE_MAX;
    VERIFY(stun_md_render_html(&v,128u,NULL,0u,&need)==STUN_UI_OK);
    VERIFY(need==expected);
    char output[256];
    VERIFY(expected+2u<sizeof(output));
    memset(output,0x31,sizeof(output));
    size_t actual=SIZE_MAX;
    VERIFY(stun_md_render_html(&v,128u,output,expected,&actual)==
           STUN_UI_CAPACITY_EXCEEDED);
    VERIFY(actual==expected);
    for(size_t i=0u;i<sizeof(output);++i)
        VERIFY((unsigned char)output[i]==0x31u);
    VERIFY(stun_md_render_html(&v,128u,output,expected+1u,&actual)==
           STUN_UI_OK);
    VERIFY(actual==expected);
    VERIFY(!memcmp(output,f->html,expected+1u));
    VERIFY((unsigned char)output[expected+1u]==0x31u);
done:
    stun_md_document_destroy(doc);
    return failure;
}

static int test_unsafe(void) {
    int failure=0;stun_md_document *doc=NULL;
    const char source[]="[x][evil]\n\n[evil]: java&#x73;cript:alert\n";
    const size_t n=sizeof(source)-1u;
    VERIFY(stun_md_parse(source,n,128u,n+1u,&doc)==STUN_UI_OK);
    VERIFY(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==1u);
    char output[128];memset(output,0x39,sizeof(output));
    size_t required=SIZE_MAX;
    VERIFY(stun_md_render_html(&v,128u,output,sizeof(output),&required)==
           STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0u;i<sizeof(output);++i)
        VERIFY((unsigned char)output[i]==0x39u);
done:
    stun_md_document_destroy(doc);
    return failure;
}

static int test_fence(void) {
    int failure=0;stun_md_document *doc=NULL;
    const char source[]=
        TICK TICK TICK "text\n[foo]: /url\n" TICK TICK TICK "\n\n[foo]\n";
    const size_t n=sizeof(source)-1u;
    VERIFY(stun_md_parse(source,n,128u,n+1u,&doc)==STUN_UI_OK);
    VERIFY(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    VERIFY(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
    VERIFY(kind_count(v,STUN_UI_BLOCK_CODE_FENCE)==1u);
    VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==0u);
    VERIFY(v.raw_markdown_bytes==n);
    VERIFY(!memcmp(v.raw_markdown,source,n));
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* Reference definitions belong only to root paragraph boundaries in this
 * restricted slice. A lazy quote/list continuation and indented code line
 * must NEVER be promoted into the document reference lookup table. */
static int test_container_definition_fences(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    static const char *blocked[]={
        "- item\n  [hidden]: /uri\n\n[hidden]\n",
        "> quote\n[hidden]: /uri\n\n[hidden]\n",
        "    [hidden]: /uri\n\n[hidden]\n",
        "paragraph\n[hidden]: /uri\n\n[hidden]\n",
        "paragraph\n== invalid\n[hidden]: /uri\n\n[hidden]\n",
        "- item\n    * * *\n    [hidden]: /bad\n\n[hidden]\n",
        /* A Setext item heading followed by a real list paragraph:
         * the continued [hidden]: line remains paragraph TEXT, not a
         * root definition. Without the preceding continuation, a list
         * can terminate before the label and that would be a false XFAIL. */
        "- item\n  ---\n  continuation\n  [hidden]: /bad\n\n[hidden]\n",
        "> quote\n> # inner\n> [hidden]: /uri\n\n[hidden]\n",
        "    # literal\n    [hidden]: /uri\n\n[hidden]\n",
        "- item\n\n  [hidden]: /uri\n\n[hidden]\n",
        "Root\n1. real\n===\n[hidden]: /uri\n\n[hidden]\n",
        "Root\n+ real\n===\n[hidden]: /uri\n\n[hidden]\n",
        "1. item\n\n   [hidden]: /uri\n\n[hidden]\n",
        "- item\n\n\t[hidden]: /uri\n\n[hidden]\n",
        "- outer\n\n  - inner\n\n    [hidden]: /uri\n\n[hidden]\n",
        "- item\n\n  [hidden]: /uri\n\n  continuation\n\n[hidden]\n"
    };
    _Static_assert(sizeof(blocked)/sizeof(blocked[0])==16u,
                   "reference-owner negative denominator");
    for(size_t i=0u;i<sizeof(blocked)/sizeof(blocked[0]);++i) {
        const size_t n=strlen(blocked[i]);
        VERIFY(stun_md_parse(blocked[i],n,128u,n+1u,&doc)==STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==0u);
        VERIFY(v.raw_markdown_bytes==n);
        VERIFY(!memcmp(v.raw_markdown,blocked[i],n));
        stun_md_document_destroy(doc);
        doc=NULL;
    }
    /* Actual dedentation after a blank restores root definition admission,
     * including the three-space root boundary of a '10.' list item. */
    static const char *roots[]={
        "- item\n\n[visible]: /uri\n\n[visible]\n",
        "10. item\n\n   [visible]: /uri\n\n[visible]\n"
    };
    for(size_t i=0u;i<sizeof(roots)/sizeof(roots[0]);++i) {
        const size_t size=strlen(roots[i]);
        VERIFY(stun_md_parse(roots[i],size,128u,size+1u,&doc)==STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view view=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
        VERIFY(kind_count(view,STUN_UI_BLOCK_LINK)==1u);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* Admission denial applies to metadata too: an indented list item
 * cannot override a later unindented root binding after a second blank. */
static int test_list_blank_scope(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    const char source[]=
        "- item\n\n  [hidden]: /bad\n\n[visible]: /good\n\n[hidden] [visible]\n";
    const size_t n=sizeof(source)-1u;
    VERIFY(stun_md_parse(source,n,256u,n+1u,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    VERIFY(stun_ui_rich_document_validate(&view,256u)==STUN_UI_OK);
    VERIFY(view.raw_markdown_bytes==n);
    VERIFY(!memcmp(view.raw_markdown,source,n));
    VERIFY(kind_count(view,STUN_UI_BLOCK_LINK)==1u);
    bool saw_good=false;
    for(size_t i=1u;i<view.document.block_count;++i) {
        if(view.document.blocks[i].kind!=STUN_UI_BLOCK_LINK)continue;
        const char *meta=NULL;size_t size=SIZE_MAX;
        VERIFY(stun_ui_rich_document_metadata(
            &view,i,&meta,&size)==STUN_UI_OK);
        VERIFY(size==5u && !memcmp(meta,"/good",5u));
        saw_good=true;
    }
    VERIFY(saw_good);
    char html[512];size_t required=SIZE_MAX;
    VERIFY(stun_md_render_html(&view,256u,html,sizeof(html),&required)==
           STUN_UI_OK);
    VERIFY(required<sizeof(html));
    VERIFY(strstr(html,"<a href=\"/good\">visible</a>")!=NULL);
    VERIFY(strstr(html,"href=\"/bad\"")==NULL);
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* 129 apparent labels INSIDE a list must not exhaust the 128-entry
 * ROOT-definition quota. Real root definitions still hit the quota in
 * the independent opposite-direction capacity test below. */
static int test_list_capacity_isolation(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    char source[8192];
    const char prefix[]="- item\n\n";
    size_t used=sizeof(prefix)-1u;
    memcpy(source,prefix,used);
    for(size_t i=0u;i<129u;++i) {
        char line[32];
        const int n=snprintf(line,sizeof(line),"  [k%zu]: /uri\n",i);
        VERIFY(n>0 && (size_t)n<sizeof(line));
        VERIFY(used+(size_t)n<sizeof(source));
        memcpy(source+used,line,(size_t)n);
        used+=(size_t)n;
    }
    const char suffix[]="\n[k0]\n";
    VERIFY(used+sizeof(suffix)-1u<sizeof(source));
    memcpy(source+used,suffix,sizeof(suffix)-1u);
    used+=sizeof(suffix)-1u;
    VERIFY(stun_md_parse(source,used,1024u,used+1u,&doc)==STUN_UI_OK);
    VERIFY(doc!=NULL);
    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    VERIFY(stun_ui_rich_document_validate(&view,1024u)==STUN_UI_OK);
    VERIFY(view.raw_markdown_bytes==used);
    VERIFY(!memcmp(view.raw_markdown,source,used));
    VERIFY(kind_count(view,STUN_UI_BLOCK_LINK)==0u);
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* A verified root thematic break releases an earlier container guard.
 * These cases intentionally leave the list/quote/code without a blank line;
 * document ownership and the emitted LINK's copied metadata must agree. */
static int test_root_rule_owner_release(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    static const char *const sources[]={
        "- item\n* * *\n[key]: /root\n\n[key]\n",
        "- item\n---\n[key]: /root\n\n[key]\n",
        "> quote\n* * *\n[key]: /root\n\n[key]\n",
        "    code\n* * *\n[key]: /root\n\n[key]\n"
    };
    _Static_assert(sizeof(sources)/sizeof(sources[0])==4u,
                   "root-rule release test count");
    for(size_t k=0u;k<sizeof(sources)/sizeof(sources[0]);++k) {
        const size_t n=strlen(sources[k]);
        VERIFY(stun_md_parse(sources[k],n,256u,n+1u,&doc)==STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view view=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&view,256u)==STUN_UI_OK);
        VERIFY(view.raw_markdown_bytes==n);
        VERIFY(!memcmp(view.raw_markdown,sources[k],n));
        VERIFY(kind_count(view,STUN_UI_BLOCK_RULE)==1u);
        VERIFY(kind_count(view,STUN_UI_BLOCK_LINK)==1u);
        bool copied=false;
        for(size_t i=1u;i<view.document.block_count;++i) {
            if(view.document.blocks[i].kind!=STUN_UI_BLOCK_LINK)continue;
            const char *meta=NULL;size_t bytes=SIZE_MAX;
            VERIFY(stun_ui_rich_document_metadata(
                &view,i,&meta,&bytes)==STUN_UI_OK);
            VERIFY(meta!=NULL && bytes==5u && !memcmp(meta,"/root",5u));
            copied=true;
        }
        VERIFY(copied);
        char output[512];size_t required=SIZE_MAX;
        VERIFY(stun_md_render_html(
            &view,256u,output,sizeof(output),&required)==STUN_UI_OK);
        VERIFY(required<sizeof(output));
        VERIFY(strstr(output,"<a href=\"/root\">key</a>")!=NULL);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* A root-level ATX heading terminates an already open quote or
 * indented code block. The following root ASCII definition is source-owned
 * and its metadata is copied into the RichDoc before prepass teardown. */
static int test_root_atx_owner_release(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    static const char *const cases[]={
        "> quote\n# heading\n[ref]: /uri\n\n[ref]\n",
        "    code\n# heading\n[ref]: /uri\n\n[ref]\n",
        "> quote\r\n  ## heading\r\n[ref]: /uri\r\n\r\n[ref]\r\n"
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==3u,
                   "ATX root owner evidence denominator");
    for(size_t k=0u;k<sizeof(cases)/sizeof(cases[0]);++k) {
        const size_t bytes=strlen(cases[k]);
        VERIFY(stun_md_parse(cases[k],bytes,256u,bytes+1u,&doc)==STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        VERIFY(v.raw_markdown_bytes==bytes);
        VERIFY(!memcmp(v.raw_markdown,cases[k],bytes));
        VERIFY(kind_count(v,STUN_UI_BLOCK_HEADING)==1u);
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==1u);
        bool copied=false;
        for(size_t i=1u;i<v.document.block_count;++i) {
            const stun_ui_block *link=&v.document.blocks[i];
            if(link->kind!=STUN_UI_BLOCK_LINK)continue;
            VERIFY(link->parent_index<i);
            const stun_ui_block *parent=&v.document.blocks[link->parent_index];
            VERIFY(parent->kind==STUN_UI_BLOCK_PARAGRAPH);
            VERIFY(parent->parent_index==0u);
            const char *meta=NULL;size_t len=SIZE_MAX;
            VERIFY(stun_ui_rich_document_metadata(
                &v,i,&meta,&len)==STUN_UI_OK);
            VERIFY(meta && len==4u && !memcmp(meta,"/uri",4u));
            copied=true;
        }
        VERIFY(copied);
        char html[512];memset(html,0x6b,sizeof(html));
        size_t needed=SIZE_MAX;
        VERIFY(stun_md_render_html(&v,256u,NULL,0u,&needed)==STUN_UI_OK);
        VERIFY(needed+1u<sizeof(html));
        size_t reported=SIZE_MAX;
        VERIFY(stun_md_render_html(&v,256u,html,needed,&reported)==
               STUN_UI_CAPACITY_EXCEEDED);
        VERIFY(reported==needed);
        for(size_t i=0u;i<sizeof(html);++i)
            VERIFY((unsigned char)html[i]==0x6bu);
        VERIFY(stun_md_render_html(&v,256u,html,needed+1u,&reported)==
               STUN_UI_OK);
        VERIFY(reported==needed);
        VERIFY(strstr(html,"<a href=\"/uri\">ref</a>")!=NULL);
        VERIFY((unsigned char)html[needed+1u]==0x6bu);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* Root fenced-code admission must end a prior quote/indented-code
 * scope without granting reference definitions to the literal fence body.
 * The same root-only prepass and owned RichDoc emitter are under test. */
static int test_root_fence_owner_release(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    typedef struct fenced_case { const char *source,*html; } fenced_case;
    static const fenced_case cases[]={
        {"> quote\n" TICK TICK TICK "c\n[shadow]: /blocked\n"
         TICK TICK TICK "\n[ref]: /uri\n\n[ref]\n",
         "<blockquote>\n<p>quote</p>\n</blockquote>\n"
         "<pre><code class=\"language-c\">[shadow]: /blocked\n</code></pre>\n"
         "<p><a href=\"/uri\">ref</a></p>\n"},
        {"    code\n~~~\n[shadow]: /blocked\n~~~\n[ref]: /uri\n\n[ref]\n",
         "<pre><code>code\n</code></pre>\n"
         "<pre><code>[shadow]: /blocked\n</code></pre>\n"
         "<p><a href=\"/uri\">ref</a></p>\n"},
        {"> quote\n~~~~\n[shadow]: /blocked\n~~~\n[shadow]: /blocked\n"
         "~~~~\n[ref]: /uri\n\n[ref]\n",
         "<blockquote>\n<p>quote</p>\n</blockquote>\n"
         "<pre><code>[shadow]: /blocked\n~~~\n[shadow]: /blocked\n</code></pre>\n"
         "<p><a href=\"/uri\">ref</a></p>\n"}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==3u,
                   "root-fence owner positives");
    for(size_t k=0u;k<sizeof(cases)/sizeof(cases[0]);++k) {
        const size_t n=strlen(cases[k].source);
        VERIFY(stun_md_parse(cases[k].source,n,256u,n+1u,&doc)==STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        VERIFY(v.raw_markdown_bytes==n);
        VERIFY(!memcmp(v.raw_markdown,cases[k].source,n));
        VERIFY(kind_count(v,STUN_UI_BLOCK_CODE_FENCE)==1u);
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==1u);
        bool copied=false;
        for(size_t i=1u;i<v.document.block_count;++i) {
            const stun_ui_block *node=&v.document.blocks[i];
            if(node->kind!=STUN_UI_BLOCK_LINK)continue;
            VERIFY(node->parent_index<i);
            const stun_ui_block *parent=&v.document.blocks[node->parent_index];
            VERIFY(parent->kind==STUN_UI_BLOCK_PARAGRAPH);
            VERIFY(parent->parent_index==0u);
            const char *meta=NULL;size_t bytes=SIZE_MAX;
            VERIFY(stun_ui_rich_document_metadata(
                &v,i,&meta,&bytes)==STUN_UI_OK);
            VERIFY(meta && bytes==4u && !memcmp(meta,"/uri",4u));
            copied=true;
        }
        VERIFY(copied);
        const size_t expected=strlen(cases[k].html);
        char output[512];size_t required=SIZE_MAX;
        VERIFY(expected+2u<sizeof(output));
        VERIFY(stun_md_render_html(&v,256u,NULL,0u,&required)==STUN_UI_OK);
        VERIFY(required==expected);
        memset(output,0x73,sizeof(output));
        size_t actual=SIZE_MAX;
        VERIFY(stun_md_render_html(&v,256u,output,expected,&actual)==
               STUN_UI_CAPACITY_EXCEEDED);
        VERIFY(actual==expected);
        for(size_t i=0u;i<sizeof(output);++i)
            VERIFY((unsigned char)output[i]==0x73u);
        VERIFY(stun_md_render_html(&v,256u,output,expected+1u,&actual)==
               STUN_UI_OK);
        VERIFY(actual==expected);
        VERIFY(!memcmp(output,cases[k].html,expected+1u));
        VERIFY((unsigned char)output[expected+1u]==0x73u);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* Closed and unclosed fences cannot donate root definitions. Even a
 * quoted or list-owned fake definition remains inert after the fence. */
static int test_fence_definition_opacity(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    static const char *const sources[]={
        "> quote\n~~~\n[shadow]: /blocked\n~~~\n\n[shadow]\n",
        "> quote\n~~~\n[shadow]: /blocked\n\n[shadow]\n",
        "- item\n  ~~~\n  [shadow]: /blocked\n  ~~~\n\n[shadow]\n"
    };
    _Static_assert(sizeof(sources)/sizeof(sources[0])==3u,
                   "fence-negative owner cases");
    for(size_t k=0u;k<sizeof(sources)/sizeof(sources[0]);++k) {
        const size_t n=strlen(sources[k]);
        VERIFY(stun_md_parse(sources[k],n,256u,n+1u,&doc)==STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        VERIFY(v.raw_markdown_bytes==n);
        VERIFY(!memcmp(v.raw_markdown,sources[k],n));
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==0u);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* GFM tables are parsed by the existing block parser. Their one-column
 * pipe-optional DATA rows cannot donate ASCII reference definitions, while
 * a definition after a real root boundary can still bind a table-cell link.
 * Require copied href ownership in the final RichDoc, not raw source state. */
static int test_table_reference_owner(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    typedef struct table_fixture {
        const char *source,*html;
        size_t links,table_links;
    } table_fixture;
    static const table_fixture cases[]={
        {"| Column |\n| --- |\n[shadow]: /blocked\n\n"
         "[ref]: /uri \"T\"\n\n[shadow] [ref]\n",
         "<table>\n<thead>\n<tr>\n<th>Column</th>\n</tr>\n</thead>\n"
         "<tbody>\n<tr>\n<td>[shadow]: /blocked</td>\n</tr>\n</tbody>\n"
         "</table>\n<p>[shadow] <a href=\"/uri\" title=\"T\">ref</a></p>\n",
         1u,0u},
        {"| Item | Link |\n| --- | --- |\n| A | [go][ref] |\n\n"
         "[ref]: /uri\n\n[go][ref]\n",
         "<table>\n<thead>\n<tr>\n<th>Item</th>\n<th>Link</th>\n"
         "</tr>\n</thead>\n<tbody>\n<tr>\n<td>A</td>\n"
         "<td><a href=\"/uri\">go</a></td>\n</tr>\n</tbody>\n"
         "</table>\n<p><a href=\"/uri\">go</a></p>\n",
         2u,1u},
        {"| Column |\n| --- |\n[shadow]: /blocked\n"
         "# Root\n[ref]: /uri\n\n[shadow] [ref]\n",
         "<table>\n<thead>\n<tr>\n<th>Column</th>\n</tr>\n</thead>\n"
         "<tbody>\n<tr>\n<td>[shadow]: /blocked</td>\n</tr>\n</tbody>\n"
         "</table>\n<h1>Root</h1>\n"
         "<p>[shadow] <a href=\"/uri\">ref</a></p>\n",
         1u,0u}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==3u,
                   "table/root reference owner denominator");
    for(size_t k=0u;k<sizeof(cases)/sizeof(cases[0]);++k) {
        const size_t bytes=strlen(cases[k].source);
        VERIFY(stun_md_parse(cases[k].source,bytes,256u,bytes+1u,&doc)==
               STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        VERIFY(v.raw_markdown_bytes==bytes);
        VERIFY(!memcmp(v.raw_markdown,cases[k].source,bytes));
        VERIFY(kind_count(v,STUN_UI_BLOCK_TABLE)==1u);
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==cases[k].links);
        size_t link_count=0u,cell_links=0u;
        for(size_t i=1u;i<v.document.block_count;++i) {
            const stun_ui_block *node=&v.document.blocks[i];
            if(node->kind!=STUN_UI_BLOCK_LINK)continue;
            VERIFY(node->parent_index<i);
            const stun_ui_block *owner=&v.document.blocks[node->parent_index];
            VERIFY(owner->kind==STUN_UI_BLOCK_PARAGRAPH ||
                   owner->kind==STUN_UI_BLOCK_TABLE_CELL);
            if(owner->kind==STUN_UI_BLOCK_TABLE_CELL) {
                VERIFY(owner->parent_index<node->parent_index);
                const stun_ui_block *row=&v.document.blocks[owner->parent_index];
                VERIFY(row->kind==STUN_UI_BLOCK_TABLE_ROW);
                ++cell_links;
            } else VERIFY(owner->parent_index==0u);
            const char *meta=NULL;size_t size=SIZE_MAX;
            VERIFY(stun_ui_rich_document_metadata(
                &v,i,&meta,&size)==STUN_UI_OK);
            VERIFY(meta && size==4u && !memcmp(meta,"/uri",4u));
            ++link_count;
        }
        VERIFY(link_count==cases[k].links);
        VERIFY(cell_links==cases[k].table_links);
        const size_t expected=strlen(cases[k].html);
        char html[1024];memset(html,0x6a,sizeof(html));
        size_t required=SIZE_MAX;
        VERIFY(expected+2u<sizeof(html));
        VERIFY(stun_md_render_html(&v,256u,NULL,0u,&required)==STUN_UI_OK);
        VERIFY(required==expected);
        size_t reported=SIZE_MAX;
        VERIFY(stun_md_render_html(&v,256u,html,expected,&reported)==
               STUN_UI_CAPACITY_EXCEEDED);
        VERIFY(reported==expected);
        for(size_t i=0u;i<sizeof(html);++i)
            VERIFY((unsigned char)html[i]==0x6au);
        VERIFY(stun_md_render_html(&v,256u,html,expected+1u,&reported)==
               STUN_UI_OK);
        VERIFY(reported==expected);
        VERIFY(!memcmp(html,cases[k].html,expected+1u));
        VERIFY((unsigned char)html[expected+1u]==0x6au);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* A 129-row one-column table with fake [key]: /uri rows must not consume
 * any of the 128 parse-local ROOT reference slots. Prove both the native
 * table row ancestry and the independent output capacity failure atomicity. */
static int test_table_reference_capacity_isolation(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    char source[8192];
    const char prefix[]="| Key |\n| --- |\n";
    size_t used=sizeof(prefix)-1u;
    memcpy(source,prefix,used);
    for(size_t i=0u;i<129u;++i) {
        char line[40];
        const int n=snprintf(line,sizeof(line),"[k%zu]: /blocked\n",i);
        VERIFY(n>0 && (size_t)n<sizeof(line));
        VERIFY(used+(size_t)n<sizeof(source));
        memcpy(source+used,line,(size_t)n);
        used+=(size_t)n;
    }
    const char suffix[]="\n[real]: /uri\n\n[k0] [real]\n";
    VERIFY(used+sizeof(suffix)-1u<sizeof(source));
    memcpy(source+used,suffix,sizeof(suffix)-1u);
    used+=sizeof(suffix)-1u;
    VERIFY(stun_md_parse(source,used,1024u,used+1u,&doc)==STUN_UI_OK);
    VERIFY(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    VERIFY(stun_ui_rich_document_validate(&v,1024u)==STUN_UI_OK);
    VERIFY(v.raw_markdown_bytes==used);
    VERIFY(!memcmp(v.raw_markdown,source,used));
    VERIFY(kind_count(v,STUN_UI_BLOCK_TABLE)==1u);
    VERIFY(kind_count(v,STUN_UI_BLOCK_TABLE_ROW)==130u);
    VERIFY(kind_count(v,STUN_UI_BLOCK_TABLE_CELL)==130u);
    VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==1u);
    bool found=false;
    for(size_t i=1u;i<v.document.block_count;++i) {
        if(v.document.blocks[i].kind!=STUN_UI_BLOCK_LINK)continue;
        const stun_ui_block *node=&v.document.blocks[i];
        VERIFY(v.document.blocks[node->parent_index].kind==
               STUN_UI_BLOCK_PARAGRAPH);
        const char *meta=NULL;size_t length=SIZE_MAX;
        VERIFY(stun_ui_rich_document_metadata(
            &v,i,&meta,&length)==STUN_UI_OK);
        VERIFY(meta && length==4u && !memcmp(meta,"/uri",4u));
        found=true;
    }
    VERIFY(found);
    char html[16384];memset(html,0x59,sizeof(html));
    size_t required=SIZE_MAX,reported=SIZE_MAX;
    VERIFY(stun_md_render_html(&v,1024u,NULL,0u,&required)==STUN_UI_OK);
    VERIFY(required+2u<sizeof(html));
    VERIFY(stun_md_render_html(&v,1024u,html,required,&reported)==
           STUN_UI_CAPACITY_EXCEEDED);
    VERIFY(reported==required);
    for(size_t i=0u;i<sizeof(html);++i)
        VERIFY((unsigned char)html[i]==0x59u);
    VERIFY(stun_md_render_html(&v,1024u,html,required+1u,&reported)==
           STUN_UI_OK);
    VERIFY(reported==required);
    VERIFY(strstr(html,"<td>[k0]: /blocked</td>")!=NULL);
    VERIFY(strstr(html,"<td>[k128]: /blocked</td>")!=NULL);
    VERIFY(strstr(html,"<p>[k0] <a href=\"/uri\">real</a></p>")!=NULL);
    VERIFY(strstr(html,"href=\"/blocked\"")==NULL);
    VERIFY((unsigned char)html[required+1u]==0x59u);
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* A list-looking root GFM table HEADER is not an actual list; its body
 * may carry real forward references and fake [shadow]: data rows. A genuine
 * list-marker ROW, however, terminates the table into a root ordered list.
 * This is an independent AST/HTML/metadata gate against prepass drift. */
static int test_table_list_competition(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    typedef struct table_list_fixture {
        const char *source,*html;
        size_t rows,links,table_links,ordered;
    } table_list_fixture;
    static const table_list_fixture cases[]={
        {"- Name | Link\n| --- | --- |\n[shadow]: /blocked\n"
         "| A | [go][ref] |\n# Root\n[ref]: /uri\n\n[shadow] [go][ref]\n",
         "<table>\n<thead>\n<tr>\n<th>- Name</th>\n<th>Link</th>\n"
         "</tr>\n</thead>\n<tbody>\n<tr>\n<td>[shadow]: /blocked</td>\n"
         "<td></td>\n</tr>\n<tr>\n<td>A</td>\n"
         "<td><a href=\"/uri\">go</a></td>\n</tr>\n</tbody>\n</table>\n"
         "<h1>Root</h1>\n<p>[shadow] <a href=\"/uri\">go</a></p>\n",
         3u,2u,1u,0u},
        {"1. Item | Link\n| --- | --- |\n| A | [go][ref] |\n"
         "# Root\n[ref]: /uri\n\n[go][ref]\n",
         "<table>\n<thead>\n<tr>\n<th>1. Item</th>\n<th>Link</th>\n"
         "</tr>\n</thead>\n<tbody>\n<tr>\n<td>A</td>\n"
         "<td><a href=\"/uri\">go</a></td>\n</tr>\n</tbody>\n</table>\n"
         "<h1>Root</h1>\n<p><a href=\"/uri\">go</a></p>\n",
         2u,2u,1u,0u},
        {"| Key | Value |\n| --- | --- |\n| A | B |\n"
         "1. first\n2. second\n\n[ref]: /uri\n\n[ref]\n",
         "<table>\n<thead>\n<tr>\n<th>Key</th>\n<th>Value</th>\n"
         "</tr>\n</thead>\n<tbody>\n<tr>\n<td>A</td>\n<td>B</td>\n"
         "</tr>\n</tbody>\n</table>\n<ol>\n<li>first</li>\n"
         "<li>second</li>\n</ol>\n<p><a href=\"/uri\">ref</a></p>\n",
         2u,1u,0u,1u}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==3u,
                   "table-versus-list owner denominator");
    for(size_t k=0u;k<sizeof(cases)/sizeof(cases[0]);++k) {
        const size_t bytes=strlen(cases[k].source);
        VERIFY(stun_md_parse(cases[k].source,bytes,256u,bytes+1u,&doc)==
               STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        VERIFY(v.raw_markdown_bytes==bytes);
        VERIFY(!memcmp(v.raw_markdown,cases[k].source,bytes));
        VERIFY(kind_count(v,STUN_UI_BLOCK_TABLE)==1u);
        VERIFY(kind_count(v,STUN_UI_BLOCK_TABLE_ROW)==cases[k].rows);
        VERIFY(kind_count(v,STUN_UI_BLOCK_ORDERED_LIST)==cases[k].ordered);
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==cases[k].links);
        size_t cell_links=0u;
        for(size_t i=1u;i<v.document.block_count;++i) {
            const stun_ui_block *node=&v.document.blocks[i];
            if(node->kind!=STUN_UI_BLOCK_LINK)continue;
            VERIFY(node->parent_index<i);
            const stun_ui_block *owner=&v.document.blocks[node->parent_index];
            VERIFY(owner->kind==STUN_UI_BLOCK_TABLE_CELL ||
                   owner->kind==STUN_UI_BLOCK_PARAGRAPH);
            if(owner->kind==STUN_UI_BLOCK_TABLE_CELL) {
                const stun_ui_block *row=&v.document.blocks[owner->parent_index];
                VERIFY(row->kind==STUN_UI_BLOCK_TABLE_ROW);
                ++cell_links;
            } else VERIFY(owner->parent_index==0u);
            const char *meta=NULL;size_t size=SIZE_MAX;
            VERIFY(stun_ui_rich_document_metadata(
                &v,i,&meta,&size)==STUN_UI_OK);
            VERIFY(meta && size==4u && !memcmp(meta,"/uri",4u));
        }
        VERIFY(cell_links==cases[k].table_links);
        const size_t expected=strlen(cases[k].html);
        char html[1024];memset(html,0x51,sizeof(html));
        size_t required=SIZE_MAX;
        VERIFY(expected+2u<sizeof(html));
        VERIFY(stun_md_render_html(&v,256u,NULL,0u,&required)==STUN_UI_OK);
        VERIFY(required==expected);
        size_t actual=SIZE_MAX;
        VERIFY(stun_md_render_html(&v,256u,html,expected,&actual)==
               STUN_UI_CAPACITY_EXCEEDED);
        VERIFY(actual==expected);
        for(size_t i=0u;i<sizeof(html);++i)
            VERIFY((unsigned char)html[i]==0x51u);
        VERIFY(stun_md_render_html(&v,256u,html,expected+1u,&actual)==
               STUN_UI_OK);
        VERIFY(actual==expected);
        VERIFY(!memcmp(html,cases[k].html,expected+1u));
        VERIFY((unsigned char)html[expected+1u]==0x51u);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* Root [label]: must not consume the very next actual block marker as its
 * URL target. Assert exact source bytes, no borrowed LINK and byte-exact
 * safe HTML for fenced code, Setext and quoted blocks. */
static int test_split_reference_block_barriers(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    static const struct {
        const char *source,*html;
    } samples[]={
        {"[foo]:\n" TICK TICK TICK "\ncode\n" TICK TICK TICK
         "\n\n[foo]\n",
         "<p>[foo]:</p>\n<pre><code>code\n</code></pre>\n"
         "<p>[foo]</p>\n"},
        {"[foo]:\n===\n\n[foo]\n",
         "<h1>[foo]:</h1>\n<p>[foo]</p>\n"},
        {"[foo]:\n> /uri\n\n[foo]\n",
         "<p>[foo]:</p>\n<blockquote>\n<p>/uri</p>\n"
         "</blockquote>\n<p>[foo]</p>\n"}
    };
    _Static_assert(sizeof(samples)/sizeof(samples[0])==3u,
                   "split-definition block barrier cases");
    for(size_t k=0u;k<3u;++k) {
        const size_t n=strlen(samples[k].source);
        VERIFY(stun_md_parse(samples[k].source,n,128u,n+1u,&doc)==
               STUN_UI_OK);
        VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        VERIFY(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        VERIFY(v.raw_markdown_bytes==n);
        VERIFY(!memcmp(v.raw_markdown,samples[k].source,n));
        VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==0u);
        const size_t expected=strlen(samples[k].html);
        size_t need=SIZE_MAX,reported=SIZE_MAX;
        VERIFY(stun_md_render_html(&v,128u,NULL,0u,&need)==STUN_UI_OK);
        VERIFY(need==expected);
        char out[256];memset(out,0x56,sizeof(out));
        VERIFY(need+2u<sizeof(out));
        VERIFY(stun_md_render_html(&v,128u,out,need,&reported)==
               STUN_UI_CAPACITY_EXCEEDED);
        VERIFY(reported==need);
        for(size_t i=0u;i<sizeof(out);++i)
            VERIFY((unsigned char)out[i]==0x56u);
        VERIFY(stun_md_render_html(&v,128u,out,need+1u,&reported)==
               STUN_UI_OK);
        VERIFY(reported==need);
        VERIFY(!memcmp(out,samples[k].html,need+1u));
        VERIFY((unsigned char)out[need+1u]==0x56u);
        stun_md_document_destroy(doc);doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
}

/* The final LINK TEXT allocation of a split definition must roll back the
 * entire parse on node-budget exhaustion. No partially published RichDoc. */
static int test_split_reference_capacity_rollback(void) {
    int failure=0;
    stun_md_document *doc=NULL;
    const char sample[]="[foo]:\n/url\n\n[foo]\n";
    const size_t n=sizeof(sample)-1u;
    VERIFY(stun_md_parse(sample,n,128u,n+1u,&doc)==STUN_UI_OK);
    VERIFY(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    VERIFY(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
    VERIFY(kind_count(v,STUN_UI_BLOCK_LINK)==1u);
    const size_t nodes=v.document.block_count;
    VERIFY(nodes>=4u);
    VERIFY(v.document.blocks[nodes-1u].kind==STUN_UI_BLOCK_TEXT);
    stun_md_document_destroy(doc);doc=NULL;
    stun_md_document *rejected=(stun_md_document*)(uintptr_t)1u;
    VERIFY(stun_md_parse(sample,n,nodes-1u,n+1u,&rejected)==
           STUN_UI_CAPACITY_EXCEEDED);
    VERIFY(rejected==NULL);
done:
    stun_md_document_destroy(doc);
    return failure;
}

int main(void) {
    static const reference_fixture cases[]={
        {"[foo]:\n/url\n\n[foo]\n",
         "/url",NULL,"foo",
         "<p><a href=\"/url\">foo</a></p>\n"}, /* CommonMark #198 */
        {"[foo]:  \r\n  /uri \"A &amp; B\"\r\n\r\n[foo]\r\n",
         "/uri","A & B","foo",
         "<p><a href=\"/uri\" title=\"A &amp; B\">foo</a></p>\n"},
        {"[a][ref]\n\n[ref]:\n/uri\n",
         "/uri",NULL,"a",
         "<p><a href=\"/uri\">a</a></p>\n"},

        {"[foo][bar]\n\n[bar]: /url \"title\"\n",
         "/url","title","foo",
         "<p><a href=\"/url\" title=\"title\">foo</a></p>\n"}, /* #527 */
        {"[foo][BaR]\n\n[bar]: /url \"title\"\n",
         "/url","title","foo",
         "<p><a href=\"/url\" title=\"title\">foo</a></p>\n"}, /* #539 */
        {"[foo]: /url1\n\n[foo]: /url2\n\n[bar][foo]\n",
         "/url1",NULL,"bar",
         "<p><a href=\"/url1\">bar</a></p>\n"}, /* #544 */
        {"[foo][]\n\n[foo]: /url \"title\"\n",
         "/url","title","foo",
         "<p><a href=\"/url\" title=\"title\">foo</a></p>\n"}, /* #553 */
        {"[Foo][]\n\n[foo]: /url \"title\"\n",
         "/url","title","Foo",
         "<p><a href=\"/url\" title=\"title\">Foo</a></p>\n"}, /* #555 */
        {"[foo]\n\n[foo]: /url \"title\"\n",
         "/url","title","foo",
         "<p><a href=\"/url\" title=\"title\">foo</a></p>\n"}, /* #557 */
        {"[Foo]\n\n[foo]: /url \"title\"\n",
         "/url","title","Foo",
         "<p><a href=\"/url\" title=\"title\">Foo</a></p>\n"}, /* #561 */
        {"[foo] bar\n\n[foo]: /url\n",
         "/url",NULL,"foo",
         "<p><a href=\"/url\">foo</a> bar</p>\n"}, /* #562 */
        {"[foo][bar]\n\n[foo]: /url1\n[bar]: /url2\n",
         "/url2",NULL,"foo",
         "<p><a href=\"/url2\">foo</a></p>\n"}, /* #565 */
        {"[foo][]\n\n[foo]: /url1\n",
         "/url1",NULL,"foo",
         "<p><a href=\"/url1\">foo</a></p>\n"}, /* #566 */
        {"[x][id]\n\n[id]: /f&ouml; \"A &copy;\"\n",
         "/fö","A ©","x",
         "<p><a href=\"/f%C3%B6\" title=\"A ©\">x</a></p>\n"},
        {"Setext\n===\n[key]: /root\n\n[key]\n",
         "/root",NULL,"key",
         "<h1>Setext</h1>\n<p><a href=\"/root\">key</a></p>\n"},
        {"Setext\n---\n[key]: /root\n\n[key]\n",
         "/root",NULL,"key",
         "<h2>Setext</h2>\n<p><a href=\"/root\">key</a></p>\n"},
        {"Setext\r\n===\r\n[key]: /root\r\n\r\n[key]\r\n",
         "/root",NULL,"key",
         "<h1>Setext</h1>\n<p><a href=\"/root\">key</a></p>\n"},
        {"* * *\n[key]: /root\n\n[key]\n",
         "/root",NULL,"key",
         "<hr />\n<p><a href=\"/root\">key</a></p>\n"},
        {"- - -\n[key]: /root\n\n[key]\n",
         "/root",NULL,"key",
         "<hr />\n<p><a href=\"/root\">key</a></p>\n"},
        {"  * * *\r\n[key]: /root\r\n\r\n[key]\r\n",
         "/root",NULL,"key",
         "<hr />\n<p><a href=\"/root\">key</a></p>\n"},
        {"Root\n2. text\n===\n[id]: /uri\n\n[id]\n",
         "/uri",NULL,"id",
         "<h1>Root\n2. text</h1>\n<p><a href=\"/uri\">id</a></p>\n"},
        {"Root\n+\n===\n[id]: /uri\n\n[id]\n",
         "/uri",NULL,"id",
         "<h1>Root\n+</h1>\n<p><a href=\"/uri\">id</a></p>\n"},
        {"Root\n-\n[id]: /uri\n\n[id]\n",
         "/uri",NULL,"id",
         "<h2>Root</h2>\n<p><a href=\"/uri\">id</a></p>\n"},
        {"Root\r\n2. text\r\n===\r\n[id]: /uri\r\n\r\n[id]\r\n",
         "/uri",NULL,"id",
         "<h1>Root\n2. text</h1>\n<p><a href=\"/uri\">id</a></p>\n"}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==24u,
                   "reference AST/HTML fixture denominator");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(test_reference(&cases[i]))return 1;
    if(test_unsafe() || test_fence() ||
       test_container_definition_fences() ||
       test_list_blank_scope() || test_list_capacity_isolation() ||
       test_root_rule_owner_release() ||
       test_root_atx_owner_release() ||
       test_root_fence_owner_release() ||
       test_fence_definition_opacity() ||
       test_table_reference_owner() ||
       test_table_reference_capacity_isolation() ||
       test_table_list_competition() ||
       test_split_reference_block_barriers() ||
       test_split_reference_capacity_rollback())return 1;
    const char limited[]="[foo][bar]\n\n[bar]: /url \"title\"\n";
    stun_md_document *rejected=(stun_md_document*)(uintptr_t)1u;
    if(stun_md_parse(limited,sizeof(limited)-1u,3u,
                     sizeof(limited),&rejected)!=STUN_UI_CAPACITY_EXCEEDED ||
       rejected!=NULL) {
        fprintf(stderr,"node budget must reject partial reference\n");
        return 1;
    }
    char many[4096];size_t used=0u;
    for(size_t i=0u;i<129u;++i) {
        char line[32];
        const int len=snprintf(line,sizeof(line),"[k%zu]: /url\n",i);
        if(len<=0 || (size_t)len>=sizeof(line) ||
           used+(size_t)len>=sizeof(many))return 1;
        memcpy(many+used,line,(size_t)len);
        used+=(size_t)len;
    }
    rejected=(stun_md_document*)(uintptr_t)1u;
    if(stun_md_parse(many,used,256u,used+1u,&rejected)!=
       STUN_UI_CAPACITY_EXCEEDED || rejected!=NULL) {
        fprintf(stderr,"reference admission must be bounded\n");
        return 1;
    }
    puts("C11 root references: 24/24 AST/HTML, forward/duplicate/case "
         "binding, safe URL, root-rule, ATX, fence and table ownership");
    return 0;
}
