#include <stun/markdown_c11.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TICK "\x60"
#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static size_t kinds(stun_ui_rich_document_view v, stun_ui_block_kind kind) {
    size_t n=0;
    for(size_t i=0;i<v.document.block_count;++i)
        n+= v.document.blocks[i].kind==kind;
    return n;
}

static bool text_is(stun_ui_rich_document_view v, const char *needle) {
    const size_t len=strlen(needle);
    for(size_t i=0;i<v.document.block_count;++i) {
        const stun_ui_block *b=&v.document.blocks[i];
        if(b->kind==STUN_UI_BLOCK_TEXT &&
           b->byte_end-b->byte_begin==len &&
           !memcmp(v.document.source+b->byte_begin,needle,len))
            return true;
    }
    return false;
}

static bool metadata_is(stun_ui_rich_document_view v,
                        stun_ui_block_kind kind, const char *needle) {
    const size_t n=strlen(needle);
    for(size_t i=0;i<v.document.block_count;++i) {
        if(v.document.blocks[i].kind!=kind) continue;
        const char *value=NULL;size_t size=SIZE_MAX;
        if(stun_ui_rich_document_metadata(&v,i,&value,&size)==STUN_UI_OK &&
           size==n && !memcmp(value,needle,n))
           return true;
    }
    return false;
}

static int test_block_and_inline_baseline(void) {
    const char md[]=
        "# Heading ###\n"
        "A **bold** and *emphasis* with " TICK "code" TICK
        " and [link](https://example.org).\n"
        "\n"
        "- first\n"
        "- second\n"
        "> Quote line\n"
        "> continues\n"
        "\n"
        TICK TICK TICK "c\n"
        "int value = 10;\n"
        TICK TICK TICK "\n"
        "\n"
        "Text with escaped \\* star.\n";
    stun_md_document *owned=NULL;
    CHECK(stun_md_parse(md,sizeof(md)-1,512,16384,&owned)==STUN_UI_OK);
    CHECK(owned!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(owned);
    CHECK(stun_ui_rich_document_validate(&v,512)==STUN_UI_OK);
    CHECK(v.raw_markdown_bytes==sizeof(md)-1);
    CHECK(!memcmp(v.raw_markdown,md,sizeof(md)-1));
    CHECK(v.document.blocks[0].kind==STUN_UI_BLOCK_DOCUMENT);
    CHECK(kinds(v,STUN_UI_BLOCK_HEADING)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_PARAGRAPH)>=3);
    CHECK(kinds(v,STUN_UI_BLOCK_STRONG)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_EMPHASIS)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_CODE_SPAN)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_LINK)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_LIST)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_LIST_ITEM)==2);
    CHECK(kinds(v,STUN_UI_BLOCK_QUOTE)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_CODE_FENCE)==1);
    CHECK(metadata_is(v,STUN_UI_BLOCK_LINK,"https://example.org"));
    CHECK(metadata_is(v,STUN_UI_BLOCK_CODE_FENCE,"c"));
    CHECK(text_is(v,"Heading"));
    CHECK(text_is(v,"bold"));
    CHECK(text_is(v,"emphasis"));
    CHECK(text_is(v,"code"));
    CHECK(text_is(v,"int value = 10;\n"));
    CHECK(text_is(v,"*"));
    stun_md_document_destroy(owned);
    return 0;
}

static int test_setext_unicode_and_crlf(void) {
    static const char md[]="中😀\r\n===\r\n"
        "🇯🇵 👩‍💻 and 👍🏽\r\n"
        "-----\r\n"
        "outside\r\n";
    stun_md_document *owned=NULL;
    CHECK(stun_md_parse(md,sizeof(md)-1,64,2048,&owned)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(owned);
    CHECK(kinds(v,STUN_UI_BLOCK_HEADING)==2);
    CHECK(kinds(v,STUN_UI_BLOCK_PARAGRAPH)==1);
    CHECK(text_is(v,"中😀"));
    CHECK(text_is(v,"🇯🇵 👩‍💻 and 👍🏽"));
    CHECK(v.document.blocks[1].kind==STUN_UI_BLOCK_HEADING);
    CHECK(v.document.blocks[1].heading_level==1);
    bool saw_second=false;
    for(size_t i=0;i<v.document.block_count;++i)
      if(v.document.blocks[i].kind==STUN_UI_BLOCK_HEADING &&
         v.document.blocks[i].heading_level==2) saw_second=true;
    CHECK(saw_second);
    stun_md_document_destroy(owned);
    return 0;
}

static int test_unclosed_fence_and_empty(void) {
    const char sample[]=TICK TICK TICK "txt\nline";
    stun_md_document *d=NULL;
    CHECK(stun_md_parse(sample,sizeof(sample)-1,8,128,&d)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(d);
    CHECK(kinds(v,STUN_UI_BLOCK_CODE_FENCE)==1);
    CHECK(text_is(v,"line"));
    stun_md_document_destroy(d);

    CHECK(stun_md_parse("",0,1,1,&d)==STUN_UI_OK);
    const stun_ui_rich_document_view empty=stun_md_document_view(d);
    CHECK(empty.document.block_count==1);
    CHECK(empty.document.source_bytes==0);
    CHECK(stun_ui_rich_document_validate(&empty,1)==STUN_UI_OK);
    stun_md_document_destroy(d);
    return 0;
}

static int test_limit_and_failure_transaction(void) {
    const char md[]="## One\n\nA **strong** paragraph.\n";
    stun_md_document *d=(stun_md_document*)(uintptr_t)0x1;
    CHECK(stun_md_parse(md,sizeof(md)-1,2,128,&d)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(d==NULL);
    d=(stun_md_document*)(uintptr_t)0x1;
    CHECK(stun_md_parse(md,sizeof(md)-1,128,5,&d)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(d==NULL);
    CHECK(stun_md_parse(NULL,3,128,128,&d)==STUN_UI_INVALID_ARGUMENT);
    CHECK(d==NULL);
    CHECK(stun_md_document_view(NULL).document.blocks==NULL);

    stun_md_document *good=NULL;
    CHECK(stun_md_parse(md,sizeof(md)-1,128,128,&good)==STUN_UI_OK);
    const stun_ui_rich_document_view view=stun_md_document_view(good);
    CHECK(stun_ui_rich_document_validate(&view,128)==STUN_UI_OK);
    CHECK(stun_ui_rich_document_validate(&view,1)==STUN_UI_CAPACITY_EXCEEDED);
    stun_md_document_destroy(good);
    return 0;
}

static int test_deterministic_ids(void) {
    const char md[]="A [link](dest) and **strong**.\n";
    stun_md_document *first=NULL, *again=NULL;
    CHECK(stun_md_parse(md,sizeof(md)-1,128,128,&first)==STUN_UI_OK);
    CHECK(stun_md_parse(md,sizeof(md)-1,128,128,&again)==STUN_UI_OK);
    const stun_ui_rich_document_view a=stun_md_document_view(first);
    const stun_ui_rich_document_view b=stun_md_document_view(again);
    CHECK(a.document.block_count==b.document.block_count);
    for(size_t i=0;i<a.document.block_count;++i) {
        CHECK(a.document.blocks[i].stable_id==b.document.blocks[i].stable_id);
        CHECK(a.document.blocks[i].kind==b.document.blocks[i].kind);
        CHECK(a.document.blocks[i].parent_index==b.document.blocks[i].parent_index);
    }
    stun_md_document_destroy(again);
    stun_md_document_destroy(first);
    return 0;
}

static int test_gfm_tables(void) {
    const char markdown[] =
        "| Name | Value |\n"
        "| :--- | ---: |\n"
        "| 中😀 | " TICK "a|b" TICK " |\n"
        "| a\\|b | **bold** |\n"
        "\n"
        "After table.\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(markdown,sizeof(markdown)-1,128,2048,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&v,128)==STUN_UI_OK);
    CHECK(kinds(v,STUN_UI_BLOCK_TABLE)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_TABLE_ROW)==3);
    CHECK(kinds(v,STUN_UI_BLOCK_TABLE_CELL)==6);
    CHECK(kinds(v,STUN_UI_BLOCK_CODE_SPAN)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_STRONG)==1);
    CHECK(text_is(v,"Name"));
    CHECK(text_is(v,"Value"));
    CHECK(text_is(v,"中😀"));
    CHECK(text_is(v,"a|b"));
    CHECK(text_is(v,"bold"));
    CHECK(text_is(v,"After table."));
    size_t table_index=SIZE_MAX, rows=0;
    for(size_t i=0;i<v.document.block_count;++i) {
        const stun_ui_block *b=&v.document.blocks[i];
        if(b->kind==STUN_UI_BLOCK_TABLE) table_index=i;
        if(b->kind==STUN_UI_BLOCK_TABLE_ROW) {
            CHECK(table_index!=SIZE_MAX);
            CHECK(b->parent_index==table_index);
            ++rows;
        }
        if(b->kind==STUN_UI_BLOCK_TABLE_CELL) {
            CHECK(b->parent_index<i);
            CHECK(v.document.blocks[b->parent_index].kind==STUN_UI_BLOCK_TABLE_ROW);
        }
    }
    CHECK(rows==3);
    stun_md_document_destroy(doc);

    const char invalid[]="| A | B |\n| not | delimiter |\n";
    CHECK(stun_md_parse(invalid,sizeof(invalid)-1,128,128,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view no_table=stun_md_document_view(doc);
    CHECK(kinds(no_table,STUN_UI_BLOCK_TABLE)==0);
    stun_md_document_destroy(doc);

    doc=(stun_md_document *)(uintptr_t)0x1;
    CHECK(stun_md_parse(markdown,sizeof(markdown)-1,7,2048,&doc)==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(doc==NULL);
    return 0;
}

static int test_gfm_table_termination_and_uneven_rows(void) {
    const char sample[] =
        "| First | Second |\n"
        "| --- | :---: |\n"
        "| " TICK TICK "a|b" TICK TICK " | Value |\n"
        "alone\n"
        "| final | extra | ignored |\n"
        "||\n"
        "> quote after table\n"
        "## Section\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(sample,sizeof(sample)-1,256,4096,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&view,256)==STUN_UI_OK);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE)==1);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE_ROW)==5);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE_CELL)==10);
    CHECK(kinds(view,STUN_UI_BLOCK_CODE_SPAN)==1);
    CHECK(text_is(view,"a|b"));
    CHECK(text_is(view,"alone"));
    CHECK(!text_is(view,"ignored"));
    CHECK(kinds(view,STUN_UI_BLOCK_QUOTE)==1);
    CHECK(kinds(view,STUN_UI_BLOCK_HEADING)==1);

    /* Each accepted row keeps exactly two cells, regardless of data width. */
    for(size_t i=0;i<view.document.block_count;++i) {
        if(view.document.blocks[i].kind!=STUN_UI_BLOCK_TABLE_ROW) continue;
        size_t cells=0;
        for(size_t j=i+1;j<view.document.block_count;++j) {
            if(view.document.blocks[j].parent_index==i &&
               view.document.blocks[j].kind==STUN_UI_BLOCK_TABLE_CELL) ++cells;
        }
        CHECK(cells==2);
    }
    stun_md_document_destroy(doc);
    return 0;
}

static int test_gfm_task_items(void) {
    const char markdown[] =
        "- [x] shipped\n"
        "- [ ] pending 👩‍💻\n"
        "- [X] uppercase\n"
        "- [x]invalid marker\n"
        "- [y] invalid glyph\n"
        "1. [x] ordered\n"
        "2. [ ]\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(markdown,sizeof(markdown)-1,128,2048,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&v,128)==STUN_UI_OK);
    CHECK(kinds(v,STUN_UI_BLOCK_TASK_ITEM)==5);
    CHECK(kinds(v,STUN_UI_BLOCK_CHECKBOX)==5);
    CHECK(kinds(v,STUN_UI_BLOCK_LIST_ITEM)==2);
    CHECK(kinds(v,STUN_UI_BLOCK_ORDERED_LIST)==1);
    CHECK(kinds(v,STUN_UI_BLOCK_LIST)==1);
    CHECK(metadata_is(v,STUN_UI_BLOCK_CHECKBOX,"x"));
    CHECK(metadata_is(v,STUN_UI_BLOCK_CHECKBOX," "));
    CHECK(text_is(v,"shipped"));
    CHECK(text_is(v,"pending 👩‍💻"));
    CHECK(text_is(v,"uppercase"));
    CHECK(text_is(v,"[x]invalid marker"));
    CHECK(text_is(v,"[y] invalid glyph"));
    CHECK(text_is(v,"ordered"));

    size_t checked=0,unchecked=0;
    for(size_t i=0;i<v.document.block_count;++i) {
        const stun_ui_block *b=&v.document.blocks[i];
        if(b->kind!=STUN_UI_BLOCK_CHECKBOX) continue;
        CHECK(b->parent_index<i);
        CHECK(v.document.blocks[b->parent_index].kind==STUN_UI_BLOCK_TASK_ITEM);
        const char *bytes=NULL;
        size_t len=0;
        CHECK(stun_ui_rich_document_metadata(&v,i,&bytes,&len)==STUN_UI_OK);
        CHECK(bytes!=NULL && len==1);
        if(bytes[0]=='x') ++checked;
        else { CHECK(bytes[0]==' '); ++unchecked; }
    }
    CHECK(checked==3);
    CHECK(unchecked==2);
    stun_md_document_destroy(doc);

    /* The state is derived from source; no mutable checkbox registry exists. */
    const char plain[]="* [x]done\n* \\[x] escaped\n";
    CHECK(stun_md_parse(plain,sizeof(plain)-1,64,256,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view unmarked=stun_md_document_view(doc);
    CHECK(kinds(unmarked,STUN_UI_BLOCK_TASK_ITEM)==0);
    CHECK(kinds(unmarked,STUN_UI_BLOCK_CHECKBOX)==0);
    stun_md_document_destroy(doc);
    return 0;
}

int main(void) {
    if(test_block_and_inline_baseline()) return 1;
    if(test_setext_unicode_and_crlf()) return 1;
    if(test_unclosed_fence_and_empty()) return 1;
    if(test_limit_and_failure_transaction()) return 1;
    if(test_deterministic_ids()) return 1;
    if(test_gfm_tables()) return 1;
    if(test_gfm_table_termination_and_uneven_rows()) return 1;
    if(test_gfm_task_items()) return 1;
    puts("C11 Markdown parser: all native tests passed");
    return 0;
}
