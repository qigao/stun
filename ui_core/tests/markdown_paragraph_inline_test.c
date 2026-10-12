#include <stun/markdown_c11.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#condition); \
        return 1; \
    } \
} while (0)

static size_t kind_count(stun_ui_rich_document_view v,stun_ui_block_kind kind) {
    size_t result=0u;
    for(size_t i=0u;i<v.document.block_count;++i)
        result+=v.document.blocks[i].kind==kind;
    return result;
}

static bool has_text(stun_ui_rich_document_view v,const char *expected) {
    const size_t n=strlen(expected);
    for(size_t i=0u;i<v.document.block_count;++i) {
        const stun_ui_block *node=&v.document.blocks[i];
        if(node->kind==STUN_UI_BLOCK_TEXT &&
           node->byte_end>=node->byte_begin &&
           node->byte_end<=v.document.source_bytes &&
           node->byte_end-node->byte_begin==n &&
           memcmp(v.document.source+node->byte_begin,expected,n)==0)
            return true;
    }
    return false;
}

/* The normalized soft break belongs to the actual nested emphasis owner
 * while raw Markdown (including CRLF) remains exactly the original bytes. */
static int check_multiline(const char *source,const char *before,
                           const char *after,size_t emphasis,size_t strong,
                           stun_ui_block_kind break_owner) {
    const size_t n=strlen(source);
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(source,n,128u,n+1u,&doc)==STUN_UI_OK);
    CHECK(doc!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
    CHECK(v.raw_markdown_bytes==n);
    CHECK(memcmp(v.raw_markdown,source,n)==0);
    CHECK(kind_count(v,STUN_UI_BLOCK_EMPHASIS)==emphasis);
    CHECK(kind_count(v,STUN_UI_BLOCK_STRONG)==strong);
    CHECK(kind_count(v,STUN_UI_BLOCK_LINE_BREAK)==1u);
    CHECK(has_text(v,before));
    CHECK(has_text(v,after));
    bool owned_break=false;
    for(size_t i=1u;i<v.document.block_count;++i) {
        const stun_ui_block *node=&v.document.blocks[i];
        CHECK(node->parent_index<i);
        if(node->kind==STUN_UI_BLOCK_LINE_BREAK &&
           v.document.blocks[node->parent_index].kind==break_owner)
            owned_break=true;
    }
    CHECK(owned_break);
    stun_md_document_destroy(doc);
    return 0;
}

int main(void) {
    CHECK(check_multiline(
        "**Gomphocarpus (*Gomphocarpus physocarpus*, syn.\n"
        "*Asclepias physocarpa*)**\n",
        "Gomphocarpus (","Asclepias physocarpa",2u,1u,
        STUN_UI_BLOCK_STRONG)==0); /* #394 */
    CHECK(check_multiline("*foo\nbar*\n","foo","bar",1u,0u,
                          STUN_UI_BLOCK_EMPHASIS)==0); /* #405 */
    CHECK(check_multiline("**foo\nbar**\n","foo","bar",0u,1u,
                          STUN_UI_BLOCK_STRONG)==0); /* #423 */
    CHECK(check_multiline("**foo *bar **baz**\nbim* bop**\n",
                          "baz","bim",1u,2u,
                          STUN_UI_BLOCK_EMPHASIS)==0); /* #432 */
    CHECK(check_multiline("*foo\r\nbar*\r\n","foo","bar",1u,0u,
                          STUN_UI_BLOCK_EMPHASIS)==0);

    static const char *barriers[]={
        "*foo\n\nbar*\n",
        "*foo\n# Heading\nbar*\n",
        "*foo\n> quote*\n",
        "*foo  \nbar*\n",
        "*foo\\\nbar*\n"
    };
    _Static_assert(sizeof(barriers)/sizeof(barriers[0])==5u,
                   "cross-block barrier fixture count");
    for(size_t i=0u;i<sizeof(barriers)/sizeof(barriers[0]);++i) {
        const size_t n=strlen(barriers[i]);
        stun_md_document *doc=NULL;
        CHECK(stun_md_parse(barriers[i],n,128u,n+1u,&doc)==STUN_UI_OK);
        CHECK(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        CHECK(kind_count(v,STUN_UI_BLOCK_EMPHASIS)==0u);
        CHECK(v.raw_markdown_bytes==n);
        CHECK(memcmp(v.raw_markdown,barriers[i],n)==0);
        stun_md_document_destroy(doc);
    }

    const char limited[]="**foo\nbar**\n";
    stun_md_document *rejected=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,5u,
                        sizeof(limited),&rejected)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(rejected==NULL);
    puts("C11 Markdown root paragraph inline ownership: 5/5 positive, "
         "5/5 block barriers and atomic budget rejection");
    return 0;
}
