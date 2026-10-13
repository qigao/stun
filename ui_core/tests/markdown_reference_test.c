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
        "- item\n\n  [hidden]: /uri\n\n[hidden]\n",
        "1. item\n\n   [hidden]: /uri\n\n[hidden]\n",
        "- item\n\n\t[hidden]: /uri\n\n[hidden]\n",
        "- outer\n\n  - inner\n\n    [hidden]: /uri\n\n[hidden]\n",
        "- item\n\n  [hidden]: /uri\n\n  continuation\n\n[hidden]\n"
    };
    _Static_assert(sizeof(blocked)/sizeof(blocked[0])==11u,
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

int main(void) {
    static const reference_fixture cases[]={
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
         "<hr />\n<p><a href=\"/root\">key</a></p>\n"}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==17u,
                   "reference AST/HTML fixture denominator");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i)
        if(test_reference(&cases[i]))return 1;
    if(test_unsafe() || test_fence() ||
       test_container_definition_fences() ||
       test_list_blank_scope() || test_list_capacity_isolation())return 1;
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
    puts("C11 root references: 17/17 AST/HTML, forward/duplicate/case "
         "binding, safe URL, blank-list/quote/fence opacity and atomic bounded ownership");
    return 0;
}
