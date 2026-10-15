#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TICK "\x60"
#define NBSP "\xc2\xa0"
#define EM_SPACE "\xe2\x80\x83"
#define NNBSP "\xe2\x80\xaf"
#define ZWSP "\xe2\x80\x8b"
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
        "| 中😀 | " TICK "a\\|b" TICK " |\n"
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
        "| " TICK TICK "a\\|b" TICK TICK " | Value |\n"
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

/* #186: the delimiter row publishes the column alignment vector on
 * the TABLE node (never inferred by the HTML renderer from inline text).
 * Subsequent short rows still inherit all column alignments. */
static int test_gfm_table_alignment_ownership(void) {
    const char sample[]=
        "| A | B | C | D |\n"
        "| :--- | :---: | ---: | --- |\n"
        "| first | second |\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(sample,sizeof(sample)-1u,128u,512u,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE)==1u);
    size_t table=SIZE_MAX;
    for(size_t i=0u;i<view.document.block_count;++i)
        if(view.document.blocks[i].kind==STUN_UI_BLOCK_TABLE)table=i;
    CHECK(table!=SIZE_MAX);
    const char *metadata=NULL;size_t bytes=0u;
    CHECK(stun_ui_rich_document_metadata(&view,table,&metadata,&bytes)==STUN_UI_OK);
    CHECK(metadata!=NULL && bytes==4u);
    const uint8_t expected[4]={STUN_UI_TABLE_ALIGN_LEFT,
        STUN_UI_TABLE_ALIGN_CENTER,STUN_UI_TABLE_ALIGN_RIGHT,
        STUN_UI_TABLE_ALIGN_NONE};
    CHECK(memcmp(metadata,expected,sizeof(expected))==0);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE_CELL)==8u);
    stun_md_document_destroy(doc);

    /* A TABLE with more columns than the admitted node budget must fail
     * before any unbounded alignment scratch allocation or RichDoc publish. */
    doc=(stun_md_document*)(uintptr_t)0x1;
    CHECK(stun_md_parse(sample,sizeof(sample)-1u,3u,512u,&doc)==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(doc==NULL);
    return 0;
}

/* #186: GFM table escape shields the pipe even when preceded by
 * another backslash; normalize into the same owned semantic cell. */
static int test_gfm_even_backslash_cell_ownership(void) {
    const char markdown[]=
        "| Name | Value |\n"
        "| --- | --- |\n"
        "| a\\\\|b | tail |\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(markdown,sizeof(markdown)-1u,128u,256u,
                        &doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(kinds(v,STUN_UI_BLOCK_TABLE)==1u);
    CHECK(kinds(v,STUN_UI_BLOCK_TABLE_ROW)==2u);
    CHECK(kinds(v,STUN_UI_BLOCK_TABLE_CELL)==4u);
    /* Inline escaping may publish several owned TEXT leaves (a, |, b),
     * not a single coalesced leaf. Validate the semantic cell content
     * and immediate TABLE_CELL ancestry rather than TEXT fragmentation. */
    size_t row=SIZE_MAX,cell=SIZE_MAX,rows=0u;
    for(size_t i=0u;i<v.document.block_count;++i) {
        if(v.document.blocks[i].kind==STUN_UI_BLOCK_TABLE_ROW) {
            ++rows;
            if(rows==2u) row=i;
        } else if(row!=SIZE_MAX && cell==SIZE_MAX &&
                  v.document.blocks[i].kind==STUN_UI_BLOCK_TABLE_CELL &&
                  v.document.blocks[i].parent_index==row) {
            cell=i;
        }
    }
    CHECK(rows==2u && cell!=SIZE_MAX);
    static const char expected[]="a|b";
    size_t used=0u;
    for(size_t i=cell+1u;i<v.document.block_count &&
        v.document.blocks[i].parent_index==cell;++i) {
        const stun_ui_block *leaf=&v.document.blocks[i];
        CHECK(leaf->kind==STUN_UI_BLOCK_TEXT);
        const size_t n=leaf->byte_end-leaf->byte_begin;
        CHECK(used<=sizeof(expected)-1u && 
              n<=sizeof(expected)-1u-used);
        CHECK(memcmp(v.document.source+leaf->byte_begin,
                     expected+used,n)==0);
        used+=n;
    }
    CHECK(used==sizeof(expected)-1u);
    CHECK(text_is(v,"tail"));
    CHECK(v.raw_markdown_bytes==sizeof(markdown)-1u &&
          memcmp(v.raw_markdown,markdown,sizeof(markdown)-1u)==0);
    stun_md_document_destroy(doc);
    return 0;
}


/* #186: quoted GFM tables use the same canonical TABLE_ROW/CELL parser
 * but are owned by the innermost explicitly marked QUOTE. A partial or
 * unquoted delimiter cannot donate a TABLE or a detached metadata vector. */
static int test_gfm_explicit_quote_table_ownership(void) {
    const char sample[]=
        "> > | Name | Count |\n"
        "> > | :--- | ---: |\n"
        "> > | a\\|b | 7 |\n"
        "> >\n"
        "> > After\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(sample,sizeof(sample)-1u,128u,512u,
                        &doc)==STUN_UI_OK);
    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
    CHECK(kinds(view,STUN_UI_BLOCK_QUOTE)==2u);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE)==1u);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE_ROW)==2u);
    CHECK(kinds(view,STUN_UI_BLOCK_TABLE_CELL)==4u);
    size_t table=SIZE_MAX,inner=SIZE_MAX,outer=SIZE_MAX;
    for(size_t i=1u;i<view.document.block_count;++i) {
        const stun_ui_block *node=&view.document.blocks[i];
        if(node->kind==STUN_UI_BLOCK_QUOTE) {
            if(outer==SIZE_MAX)outer=i;
            else if(inner==SIZE_MAX)inner=i;
        }
        if(node->kind==STUN_UI_BLOCK_TABLE)table=i;
    }
    CHECK(outer!=SIZE_MAX && inner!=SIZE_MAX && table!=SIZE_MAX);
    CHECK(view.document.blocks[outer].parent_index==0u);
    CHECK(view.document.blocks[inner].parent_index==outer);
    CHECK(view.document.blocks[table].parent_index==inner);
    const char *metadata=NULL;size_t size=SIZE_MAX;
    CHECK(stun_ui_rich_document_metadata(&view,table,&metadata,&size)==
          STUN_UI_OK);
    CHECK(metadata!=NULL && size==2u);
    CHECK((uint8_t)metadata[0]==STUN_UI_TABLE_ALIGN_LEFT);
    CHECK((uint8_t)metadata[1]==STUN_UI_TABLE_ALIGN_RIGHT);
    CHECK(view.raw_markdown_bytes==sizeof(sample)-1u);
    CHECK(memcmp(view.raw_markdown,sample,sizeof(sample)-1u)==0);
    unsigned char out[1024];memset(out,0xA5,sizeof(out));
    size_t required=SIZE_MAX;
    CHECK(stun_md_render_html(&view,128u,(char*)out,sizeof(out),
                              &required)==STUN_UI_OK);
    CHECK(strstr((char*)out,"<blockquote>\n<blockquote>\n<table>\n")!=NULL);
    CHECK(strstr((char*)out,"<td align=\"left\">a|b</td>")!=NULL);
    memset(out,0xA5,sizeof(out));
    CHECK(stun_md_render_html(&view,128u,(char*)out,1u,&required)==
          STUN_UI_CAPACITY_EXCEEDED);
    for(size_t i=0u;i<sizeof(out);++i)CHECK(out[i]==0xA5);
    stun_md_document_destroy(doc);

    /* A quoted header followed by a root delimiter must not accidentally
     * establish a QUOTE TABLE; all source bytes remain parser-owned. */
    const char rejected[]="> | A | B |\n| --- | --- |\n";
    CHECK(stun_md_parse(rejected,sizeof(rejected)-1u,128u,512u,
                        &doc)==STUN_UI_OK);
    CHECK(kinds(stun_md_document_view(doc),STUN_UI_BLOCK_TABLE)==0u);
    stun_md_document_destroy(doc);

    doc=(stun_md_document*)(uintptr_t)0x1;
    CHECK(stun_md_parse(sample,sizeof(sample)-1u,6u,512u,
                        &doc)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(doc==NULL);
    return 0;
}

/* #186: admitted GFM LIST_ITEM tables use the SAME typed TABLE owner and
 * alignment metadata as ROOT/QUOTE tables. Check nested ownership, retained
 * source bytes, actual HTML emission and bounded failure rollback. */
static int test_gfm_list_table_ownership(void) {
    const char *sources[] = {
        "- | A | B |\n  | :--- | ---: |\n  | a\\|b | 7 |\n",
        "> - | A | B |\n>   | :--- | ---: |\n>   | a\\|b | 7 |\n",
        "- intro\n\n  | A | B |\n  | :--- | ---: |\n"
        "  | a\\|b | 7 |\n"
    };
    for(size_t k=0u;k<sizeof(sources)/sizeof(sources[0]);++k) {
        const size_t bytes=strlen(sources[k]);
        stun_md_document *owned=NULL;
        CHECK(stun_md_parse(sources[k],bytes,256u,1024u,&owned)==STUN_UI_OK);
        const stun_ui_rich_document_view v=stun_md_document_view(owned);
        CHECK(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE)==1u);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE_ROW)==2u);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE_CELL)==4u);
        CHECK(kinds(v,STUN_UI_BLOCK_LIST)==1u);
        CHECK(kinds(v,STUN_UI_BLOCK_QUOTE)==(k==1u?1u:0u));
        size_t list=SIZE_MAX,item=SIZE_MAX,table=SIZE_MAX;
        for(size_t i=1u;i<v.document.block_count;++i) {
            switch(v.document.blocks[i].kind) {
            case STUN_UI_BLOCK_LIST: list=i;break;
            case STUN_UI_BLOCK_LIST_ITEM: item=i;break;
            case STUN_UI_BLOCK_TABLE: table=i;break;
            default: break;
            }
        }
        CHECK(list!=SIZE_MAX && item!=SIZE_MAX && table!=SIZE_MAX);
        CHECK(v.document.blocks[item].parent_index==list);
        CHECK(v.document.blocks[table].parent_index==item);
        if(k==1u) {
            const uint32_t quote=v.document.blocks[list].parent_index;
            CHECK(quote>0u && quote<list &&
                  v.document.blocks[quote].kind==STUN_UI_BLOCK_QUOTE);
        } else CHECK(v.document.blocks[list].parent_index==0u);
        const char *align=NULL;size_t align_bytes=0u;
        CHECK(stun_ui_rich_document_metadata(&v,table,&align,
                                             &align_bytes)==STUN_UI_OK);
        CHECK(align && align_bytes==2u);
        CHECK((uint8_t)align[0]==STUN_UI_TABLE_ALIGN_LEFT);
        CHECK((uint8_t)align[1]==STUN_UI_TABLE_ALIGN_RIGHT);
        CHECK(v.raw_markdown_bytes==bytes &&
              memcmp(v.raw_markdown,sources[k],bytes)==0);
        unsigned char out[2048];memset(out,0xb4,sizeof(out));
        size_t required=SIZE_MAX;
        CHECK(stun_md_render_html(&v,256u,(char*)out,1u,&required)==
              STUN_UI_CAPACITY_EXCEEDED);
        for(size_t i=0u;i<sizeof(out);++i)CHECK(out[i]==0xb4);
        CHECK(stun_md_render_html(&v,256u,(char*)out,
                                  sizeof(out),&required)==STUN_UI_OK);
        CHECK(strstr((char*)out,"<table>\n<thead>")!=NULL);
        CHECK(strstr((char*)out,"<td align=\"left\">a|b</td>")!=NULL);
        stun_md_document_destroy(owned);
    }
    /* Dedented delimiter may not attach to an open ITEM table header. */
    const char invalid[]="- | A | B |\n| --- | --- |\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(invalid,sizeof(invalid)-1u,256u,256u,
                        &doc)==STUN_UI_OK);
    CHECK(kinds(stun_md_document_view(doc),STUN_UI_BLOCK_TABLE)==0u);
    stun_md_document_destroy(doc);
    doc=(stun_md_document*)(uintptr_t)0x1;
    CHECK(stun_md_parse(sources[0],strlen(sources[0]),5u,1024u,
                        &doc)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(doc==NULL);
    return 0;
}

/* #186: true TASK_ITEM -> TABLE and nested LIST_ITEM -> TABLE ownership
 * on the one C11 block parser. Source and checkbox metadata remain owned
 * by the same immutable document; count/write HTML stays transactional. */
/* #186: an existing list-owned paragraph may be followed without an
 * intervening blank by a canonical two-line GFM TABLE header/delimiter.
 * The already-owned paragraph keeps its earlier inline children; the
 * TABLE becomes a new sibling under the SAME list item. A root line
 * beginning with a valid "- " marker is a LIST opener, not root TABLE. */
static int test_gfm_paragraph_to_list_table(void) {
    const char *inputs[] = {
        "- intro\n  | A | B |\n  | --- | --- |\n  | x | y |\n",
        "> - intro\n>   | A | B |\n>   | --- | --- |\n"
        ">   | x | y |\n",
        "- intro\n  continued\n  | A | B |\n"
        "  | --- | --- |\n  | x | y |\n",
        "- left | right\n  --- | ---\n  x | y\n"
    };
    for(size_t k=0u;k<sizeof(inputs)/sizeof(inputs[0]);++k) {
        const size_t bytes=strlen(inputs[k]);
        stun_md_document *owner=NULL;
        CHECK(stun_md_parse(inputs[k],bytes,256u,1024u,&owner)==STUN_UI_OK);
        const stun_ui_rich_document_view v=stun_md_document_view(owner);
        CHECK(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE)==1u);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE_ROW)==2u);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE_CELL)==4u);
        CHECK(kinds(v,STUN_UI_BLOCK_LIST)==1u);
        CHECK(kinds(v,STUN_UI_BLOCK_PARAGRAPH)==(k==3u?0u:1u));
        CHECK(kinds(v,STUN_UI_BLOCK_QUOTE)==(k==1u?1u:0u));
        size_t table=SIZE_MAX,item=SIZE_MAX,list=SIZE_MAX;
        for(size_t i=1u;i<v.document.block_count;++i) {
            switch(v.document.blocks[i].kind) {
            case STUN_UI_BLOCK_TABLE: table=i;break;
            case STUN_UI_BLOCK_LIST_ITEM: item=i;break;
            case STUN_UI_BLOCK_LIST: list=i;break;
            default: break;
            }
        }
        CHECK(table!=SIZE_MAX && item!=SIZE_MAX && list!=SIZE_MAX);
        CHECK(v.document.blocks[table].parent_index==item);
        CHECK(v.document.blocks[item].parent_index==list);
        CHECK(v.document.blocks[list].parent_index==
              (k==1u?1u:0u));
        if(k<3u)CHECK(text_is(v,"intro"));
        if(k==2u)CHECK(text_is(v,"continued"));
        CHECK(text_is(v,k==3u?"left":"A"));
        CHECK(text_is(v,"x"));
        CHECK(text_is(v,"y"));
        CHECK(v.raw_markdown_bytes==bytes &&
              memcmp(v.raw_markdown,inputs[k],bytes)==0);
        size_t need=SIZE_MAX;
        CHECK(stun_md_render_html(&v,256u,NULL,0u,&need)==STUN_UI_OK);
        CHECK(need>0u && need<1024u);
        unsigned char bytes_out[1024];
        memset(bytes_out,0x7b,sizeof(bytes_out));
        size_t required=SIZE_MAX;
        CHECK(stun_md_render_html(&v,256u,(char*)bytes_out,need,
                                  &required)==STUN_UI_CAPACITY_EXCEEDED);
        for(size_t i=0u;i<sizeof(bytes_out);++i)
            CHECK(bytes_out[i]==0x7b);
        CHECK(stun_md_render_html(&v,256u,(char*)bytes_out,
                                  sizeof(bytes_out),&required)==STUN_UI_OK);
        CHECK(strstr((char*)bytes_out,"<table>\n<thead>")!=NULL);
        stun_md_document_destroy(owner);
    }
    return 0;
}

static int test_gfm_composed_list_table_ownership(void) {
    const char *inputs[] = {
        "- [x] parent\n\n  | A | B |\n  | --- | --- |\n"
        "  | a | b |\n",
        "> - [x] parent\n>\n>   | A | B |\n"
        ">   | --- | --- |\n>   | a | b |\n",
        "- parent\n  - | A | B |\n    | --- | --- |\n"
        "    | a | b |\n",
        "> - outer\n>   - | A | B |\n"
        ">     | --- | --- |\n>     | a | b |\n"
    };
    for(size_t k=0u;k<sizeof(inputs)/sizeof(inputs[0]);++k) {
        const size_t bytes=strlen(inputs[k]);
        stun_md_document *owner=NULL;
        CHECK(stun_md_parse(inputs[k],bytes,256u,1024u,&owner)==STUN_UI_OK);
        const stun_ui_rich_document_view v=stun_md_document_view(owner);
        CHECK(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE)==1u);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE_ROW)==2u);
        CHECK(kinds(v,STUN_UI_BLOCK_TABLE_CELL)==4u);
        CHECK(kinds(v,STUN_UI_BLOCK_LIST)==(k<2u?1u:2u));
        CHECK(kinds(v,STUN_UI_BLOCK_QUOTE)==((k==1u||k==3u)?1u:0u));
        size_t table=SIZE_MAX;
        for(size_t i=1u;i<v.document.block_count;++i)
            if(v.document.blocks[i].kind==STUN_UI_BLOCK_TABLE)table=i;
        CHECK(table!=SIZE_MAX);
        const uint32_t item=v.document.blocks[table].parent_index;
        CHECK(item>0u && item<table);
        CHECK(v.document.blocks[item].kind==
              (k<2u?STUN_UI_BLOCK_TASK_ITEM:STUN_UI_BLOCK_LIST_ITEM));
        const uint32_t list=v.document.blocks[item].parent_index;
        CHECK(list>0u && list<item &&
              v.document.blocks[list].kind==STUN_UI_BLOCK_LIST);
        if(k<2u) {
            CHECK(item+1u<table &&
                  v.document.blocks[item+1u].kind==STUN_UI_BLOCK_CHECKBOX &&
                  v.document.blocks[item+1u].parent_index==item);
        }
        CHECK(v.raw_markdown_bytes==bytes &&
              memcmp(v.raw_markdown,inputs[k],bytes)==0);
        size_t required=SIZE_MAX;
        CHECK(stun_md_render_html(&v,256u,NULL,0u,&required)==STUN_UI_OK);
        CHECK(required>0u && required<2048u);
        unsigned char html[2048];
        memset(html,0x8c,sizeof(html));
        CHECK(stun_md_render_html(&v,256u,(char*)html,required,
                                  &required)==STUN_UI_CAPACITY_EXCEEDED);
        for(size_t i=0u;i<sizeof(html);++i)CHECK(html[i]==0x8c);
        CHECK(stun_md_render_html(&v,256u,(char*)html,sizeof(html),
                                  &required)==STUN_UI_OK);
        CHECK(strstr((char*)html,"<table>\n<thead>")!=NULL);
        if(k<2u)
            CHECK(strstr((char*)html,"<input type=\"checkbox\" checked=\"\" disabled=\"\" />")!=NULL);
        stun_md_document_destroy(owner);
    }
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
    CHECK(kinds(v,STUN_UI_BLOCK_TASK_ITEM)==4);
    CHECK(kinds(v,STUN_UI_BLOCK_CHECKBOX)==4);
    CHECK(kinds(v,STUN_UI_BLOCK_LIST_ITEM)==3);
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
    CHECK(text_is(v,"[ ]"));

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
    CHECK(unchecked==1);
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

/* #186/GFM Example 280: nested task groups remain children of the real
 * list-item owner. The Checkbox is always the first direct item child and
 * carries a one-byte checked fact, not application-owned mutable state. */
static int test_gfm_nested_task_ownership(void) {
    const char sample[] =
        "- [x] foo\n"
        "  - [ ] bar\n"
        "  - [x] baz\n"
        "- [ ] bim\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(sample,sizeof(sample)-1u,256u,512u,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    CHECK(stun_ui_rich_document_validate(&v,256u)==STUN_UI_OK);
    CHECK(kinds(v,STUN_UI_BLOCK_LIST)==2u);
    CHECK(kinds(v,STUN_UI_BLOCK_TASK_ITEM)==4u);
    CHECK(kinds(v,STUN_UI_BLOCK_CHECKBOX)==4u);
    size_t nested_lists=0u,checked=0u,unchecked=0u;
    for(size_t i=1u;i<v.document.block_count;++i) {
        const stun_ui_block *node=&v.document.blocks[i];
        if(node->kind==STUN_UI_BLOCK_LIST) {
            const uint32_t parent=node->parent_index;
            if(parent!=0u) {
                CHECK(parent<i);
                CHECK(v.document.blocks[parent].kind==STUN_UI_BLOCK_TASK_ITEM);
                ++nested_lists;
            }
        }
        if(node->kind!=STUN_UI_BLOCK_CHECKBOX) continue;
        const uint32_t parent=node->parent_index;
        CHECK(parent<i && i==parent+1u);
        CHECK(v.document.blocks[parent].kind==STUN_UI_BLOCK_TASK_ITEM);
        const char *metadata=NULL;size_t bytes=0u;
        CHECK(stun_ui_rich_document_metadata(&v,i,&metadata,&bytes)==STUN_UI_OK);
        CHECK(metadata!=NULL && bytes==1u);
        if(metadata[0]=='x')++checked;
        else { CHECK(metadata[0]==' ');++unchecked; }
    }
    CHECK(nested_lists==1u && checked==2u && unchecked==2u);
    size_t need=SIZE_MAX;
    CHECK(stun_md_render_html(&v,256u,NULL,0u,&need)==STUN_UI_OK);
    CHECK(need>0u && need<1024u);
    char html[1024];memset(html,0x5a,sizeof(html));
    size_t required=SIZE_MAX;
    CHECK(stun_md_render_html(&v,256u,html,need,&required)==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(required==need);
    for(size_t i=0u;i<sizeof(html);++i)CHECK((unsigned char)html[i]==0x5a);
    CHECK(stun_md_render_html(&v,256u,html,need+1u,&required)==STUN_UI_OK);
    CHECK(required==need);
    CHECK(strstr(html,"<ul>")!=NULL && strstr(html,"</ul>")!=NULL);
    CHECK(strstr(html,"<input type=\"checkbox\" checked=\"\" disabled=\"\" />")!=NULL);
    stun_md_document_destroy(doc);
    return 0;
}

/* #182: the decoded code-span payload belongs to one typed node, never
 * recursively parses inner punctuation and never changes the raw source. */
static int test_code_span_exact_runs_and_text(void) {
    typedef struct {
        const char *markdown;
        const char *expected;
        unsigned code_spans;
    } code_fixture;
    static const code_fixture samples[] = {
        {TICK TICK " foo " TICK " bar " TICK TICK "\n",
         "foo " TICK " bar",1u},                           /* #329 */
        {TICK " " TICK TICK " " TICK "\n",
         TICK TICK,1u},                                   /* #330 */
        {TICK "  " TICK TICK "  " TICK "\n",
         " " TICK TICK " ",1u},                            /* #331 */
        {TICK "\xc2\xa0" "b" "\xc2\xa0" TICK "\n",
         "\xc2\xa0" "b" "\xc2\xa0",1u},                     /* #333 */
        {TICK "  " TICK "\n","  ",1u},                     /* #334 */
        {TICK "foo\\" TICK "bar" TICK "\n","foo\\",1u},    /* #338 */
        {TICK TICK "foo" TICK "bar" TICK TICK "\n",
         "foo" TICK "bar",1u},                             /* #339 */
        {TICK " foo " TICK TICK " bar " TICK "\n",
         "foo " TICK TICK " bar",1u},                      /* #340 */
        {TICK TICK TICK "foo" TICK TICK "\n",NULL,0u},  /* #347 */
        {TICK TICK "\nfoo\nbar  \nbaz\n" TICK TICK "\n",
         "foo bar   baz",1u},                               /* #335 */
        {TICK TICK "\nfoo \n" TICK TICK "\n",
         "foo ",1u},                                       /* #336 */
        {TICK "foo   bar \nbaz" TICK "\n",
         "foo   bar  baz",1u},                              /* #337 */
        {TICK TICK "\r\nfoo\r\nbar  \r\nbaz\r\n" TICK TICK "\r\n",
         "foo bar   baz",1u},
        {TICK "line\nnext" TICK " tail\n","line next",1u},
        {TICK "foo\nbar" TICK "\nafter\n","foo bar",1u},
        {TICK "foo\n\nbar" TICK "\n",NULL,0u},
        {TICK "foo\n# Heading\nbar" TICK "\n",NULL,0u},
        {TICK TICK "a\n" TICK "b" TICK TICK "\n",
         "a " TICK "b",1u},
        {TICK " \n " TICK "\n","   ",1u}
    };
    _Static_assert(sizeof(samples)/sizeof(samples[0])==19u,
                   "code-span fixture count");
    for(size_t i=0u;i<sizeof(samples)/sizeof(samples[0]);++i) {
        const size_t n=strlen(samples[i].markdown);
        stun_md_document *doc=NULL;
        CHECK(stun_md_parse(samples[i].markdown,n,128u,n+1u,&doc)==STUN_UI_OK);
        CHECK(doc!=NULL);
        const stun_ui_rich_document_view view=stun_md_document_view(doc);
        CHECK(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
        CHECK(view.raw_markdown_bytes==n);
        CHECK(memcmp(view.raw_markdown,samples[i].markdown,n)==0);
        CHECK(kinds(view,STUN_UI_BLOCK_CODE_SPAN)==samples[i].code_spans);
        if(samples[i].expected) {
            const size_t want=strlen(samples[i].expected);
            bool found=false;
            for(size_t at=0u;at<view.document.block_count;++at) {
                if(view.document.blocks[at].kind!=STUN_UI_BLOCK_CODE_SPAN)
                    continue;
                for(size_t j=at+1u;j<view.document.block_count;++j) {
                    const stun_ui_block *node=&view.document.blocks[j];
                    if(node->parent_index!=at ||
                       node->kind!=STUN_UI_BLOCK_TEXT)continue;
                    CHECK(node->byte_end>=node->byte_begin);
                    CHECK(node->byte_end<=view.document.source_bytes);
                    if(node->byte_end-node->byte_begin==want &&
                       memcmp(view.document.source+node->byte_begin,
                              samples[i].expected,want)==0)
                        found=true;
                }
            }
            CHECK(found);
        }
        stun_md_document_destroy(doc);
    }
    const char limited[]=TICK TICK " foo " TICK " bar " TICK TICK "\n";
    stun_md_document *out=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,3u,
                        sizeof(limited),&out)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(out==NULL);
    /* A multiline opener can allocate its paragraph and typed code node,
     * but exhausting its normalized TEXT must destroy the entire document. */
    const char multi_limited[]=TICK TICK "\nfoo\nbar  \nbaz\n" TICK TICK "\n";
    out=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(multi_limited,sizeof(multi_limited)-1u,3u,
                        sizeof(multi_limited),&out)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(out==NULL);
    out=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(multi_limited,sizeof(multi_limited)-1u,128u,
                        sizeof(multi_limited)-2u,&out)==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(out==NULL);
    return 0;
}

/* A valid code span is opaque to its surrounding emphasis/link scan.
 * The typed RichDoc child must belong to the REAL paragraph/emphasis/link
 * owner, rather than being swallowed by an earlier delimiter candidate. */
static int test_code_span_delimiter_precedence(void) {
    typedef struct {
        const char *markdown,*code;
        stun_ui_block_kind parent;
        size_t emphasis,link,strong;
    } precedence_fixture;
    static const precedence_fixture cases[]={
        {"*foo" TICK "*" TICK "\n","*",STUN_UI_BLOCK_PARAGRAPH,0u,0u,0u},
        {"[not a " TICK "link](/foo" TICK ")\n",
         "link](/foo",STUN_UI_BLOCK_PARAGRAPH,0u,0u,0u},
        {"*hello " TICK "a*b" TICK " world*\n",
         "a*b",STUN_UI_BLOCK_EMPHASIS,1u,0u,0u},
        {"[hello " TICK "x]y" TICK "](url)\n",
         "x]y",STUN_UI_BLOCK_LINK,0u,1u,0u},
        {"*foo" TICK TICK "bar*\n",
         NULL,STUN_UI_BLOCK_PARAGRAPH,1u,0u,0u},
        {"**hi " TICK "x*y" TICK " done**\n",
         "x*y",STUN_UI_BLOCK_STRONG,0u,0u,1u}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==6u,
                   "code-span precedence denominator");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const size_t n=strlen(cases[i].markdown);
        stun_md_document *d=NULL;
        CHECK(stun_md_parse(cases[i].markdown,n,128u,n+1u,&d)==STUN_UI_OK);
        CHECK(d!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(d);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        CHECK(v.raw_markdown_bytes==n);
        CHECK(memcmp(v.raw_markdown,cases[i].markdown,n)==0);
        CHECK(kinds(v,STUN_UI_BLOCK_EMPHASIS)==cases[i].emphasis);
        CHECK(kinds(v,STUN_UI_BLOCK_LINK)==cases[i].link);
        CHECK(kinds(v,STUN_UI_BLOCK_STRONG)==cases[i].strong);
        CHECK(kinds(v,STUN_UI_BLOCK_CODE_SPAN)==(cases[i].code?1u:0u));
        if(cases[i].code) {
            bool found=false;
            for(size_t j=1u;j<v.document.block_count;++j) {
                const stun_ui_block *node=&v.document.blocks[j];
                if(node->kind!=STUN_UI_BLOCK_CODE_SPAN)continue;
                CHECK(node->parent_index<j);
                CHECK(v.document.blocks[node->parent_index].kind==
                      cases[i].parent);
                for(size_t k=j+1u;k<v.document.block_count;++k) {
                    const stun_ui_block *text=&v.document.blocks[k];
                    if(text->parent_index!=j||
                       text->kind!=STUN_UI_BLOCK_TEXT)continue;
                    const size_t len=strlen(cases[i].code);
                    CHECK(text->byte_end<=v.document.source_bytes);
                    if(text->byte_end-text->byte_begin==len &&
                       memcmp(v.document.source+text->byte_begin,
                              cases[i].code,len)==0) found=true;
                }
            }
            CHECK(found);
        }
        stun_md_document_destroy(d);
    }
    /* Capacity exhaustion while publishing nested code-span text is
     * failure atomic even when a surrounding emphasis was admitted. */
    const char limited[]="*hello " TICK "a*b" TICK " world*\n";
    stun_md_document *out=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,5u,
                        sizeof(limited),&out)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(out==NULL);
    return 0;
}

/* #182: delimiter boundary decisions are IR-level ownership decisions.
 * Rejected openers stay literal; word-internal underscores never invent a
 * nested Emphasis node, and admitted outer emphasis retains its real text. */
static int test_ascii_emphasis_flanking(void) {
    typedef struct {
        const char *markdown,*text;
        size_t emphasis,strong;
    } flanking_fixture;
    static const flanking_fixture samples[]={
        {"a*\"foo\"*\n","a*\"foo\"*",0u,0u},
        {"a_\"foo\"_\n","a_\"foo\"_",0u,0u},
        {"foo_bar_\n","foo_bar_",0u,0u},
        {"5_6_78\n","5_6_78",0u,0u},
        {"aa_\"bb\"_cc\n","aa_\"bb\"_cc",0u,0u},
        {"_foo_bar\n","_foo_bar",0u,0u},
        {"_foo_bar_baz_\n","foo_bar_baz",1u,0u},
        {"a**\"foo\"**\n","a**\"foo\"**",0u,0u},
        {"**foo **bar baz**\n","bar baz",0u,1u}, /* #471 */
        {"*foo *bar baz*\n","bar baz",1u,0u}   /* #472 */
    };
    _Static_assert(sizeof(samples)/sizeof(samples[0])==10u,
                   "flanking ownership sample count");
    for(size_t i=0u;i<sizeof(samples)/sizeof(samples[0]);++i) {
        const size_t n=strlen(samples[i].markdown);
        stun_md_document *d=NULL;
        CHECK(stun_md_parse(samples[i].markdown,n,128u,n+1u,&d)==STUN_UI_OK);
        CHECK(d!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(d);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        CHECK(v.raw_markdown_bytes==n);
        CHECK(memcmp(v.raw_markdown,samples[i].markdown,n)==0);
        CHECK(kinds(v,STUN_UI_BLOCK_EMPHASIS)==samples[i].emphasis);
        CHECK(kinds(v,STUN_UI_BLOCK_STRONG)==samples[i].strong);
        CHECK(text_is(v,samples[i].text));
        stun_md_document_destroy(d);
    }
    return 0;
}

/* Nested same-length delimiter pairs must own their INNER decoded text,
 * not attach it to the outer sibling or manufacture literal delimiter nodes.
 * The producer retains the exact original UTF-8 document for every sample. */
static int test_nested_emphasis_pairs(void) {
    typedef struct {
        const char *source,*inner_text;
        stun_ui_block_kind kind;
    } nested_fixture;
    static const nested_fixture cases[]={
        {"*(*foo*)*\n","foo",STUN_UI_BLOCK_EMPHASIS},      /* #369 */
        {"_(_foo_)_\n","foo",STUN_UI_BLOCK_EMPHASIS},      /* #373 */
        {"__foo, __bar__, baz__\n","bar",STUN_UI_BLOCK_STRONG}, /* #389 */
        {"*emph *with emph* in it*\n","with emph",STUN_UI_BLOCK_EMPHASIS},
        {"**strong **with strong** in it**\n","with strong",STUN_UI_BLOCK_STRONG},
        {"*x (*y*) z*\n","y",STUN_UI_BLOCK_EMPHASIS},
        {"**x (**y**) z**\n","y",STUN_UI_BLOCK_STRONG}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==7u,
                   "nested delimiter fixture denominator");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const size_t bytes=strlen(cases[i].source);
        stun_md_document *d=NULL;
        CHECK(stun_md_parse(cases[i].source,bytes,128u,bytes+1u,&d)==STUN_UI_OK);
        CHECK(d!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(d);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        CHECK(v.raw_markdown_bytes==bytes);
        CHECK(memcmp(v.raw_markdown,cases[i].source,bytes)==0);
        CHECK(kinds(v,cases[i].kind)==2u);
        const size_t expected=strlen(cases[i].inner_text);
        bool child_found=false;
        for(size_t j=1u;j<v.document.block_count;++j) {
            const stun_ui_block *inner=&v.document.blocks[j];
            if(inner->kind!=cases[i].kind || inner->parent_index>=j)
                continue;
            if(v.document.blocks[inner->parent_index].kind!=cases[i].kind)
                continue;
            for(size_t k=j+1u;k<v.document.block_count;++k) {
                const stun_ui_block *text=&v.document.blocks[k];
                if(text->parent_index!=j ||
                   text->kind!=STUN_UI_BLOCK_TEXT ||
                   text->byte_end<text->byte_begin ||
                   text->byte_end>v.document.source_bytes) continue;
                if(text->byte_end-text->byte_begin==expected &&
                   memcmp(v.document.source+text->byte_begin,
                          cases[i].inner_text,expected)==0)
                    child_found=true;
            }
        }
        CHECK(child_found);
        stun_md_document_destroy(d);
    }
    /* The inner TEXT would exceed the fifth allocated node. A rejected
     * source never escapes as an incomplete RichDoc owner. */
    const char limited[]="*(*foo*)*\n";
    stun_md_document *out=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,5u,
                        sizeof(limited),&out)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(out==NULL);
    return 0;
}

/* #182: the canonical Salts::Unicode property must decide whether a
 * Unicode scalar flanking an emphasis run is White_Space. Verify unchanged
 * raw UTF-8, typed RichDoc ownership, and boundaries that are explicitly
 * NOT White_Space (e.g. U+200B). No private classification table. */
static int test_unicode_whitespace_emphasis(void) {
    typedef struct {
        const char *input,*text;
        size_t emphasis;
    } unicode_fixture;
    static const unicode_fixture cases[]={
        {"*" NBSP "a" NBSP "*\n","*" NBSP "a" NBSP "*",0u}, /* #353 */
        {"a*" NBSP "b*\n","a*" NBSP "b*",0u},
        {"*x" NBSP "*\n","*x" NBSP "*",0u},
        {"*foo" NBSP "bar*\n","foo" NBSP "bar",1u},
        {"**" NBSP "foo**\n","**" NBSP "foo**",0u},
        {"_" NBSP "foo_\n","_" NBSP "foo_",0u},
        {"*" EM_SPACE "a" EM_SPACE "*\n",
         "*" EM_SPACE "a" EM_SPACE "*",0u},
        {"*" NNBSP "a" NNBSP "*\n",
         "*" NNBSP "a" NNBSP "*",0u},
        {"*" ZWSP "a*\n",ZWSP "a",1u},
        {"*中*\n","中",1u}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==10u,
                   "Unicode whitespace source fixture count");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const size_t length=strlen(cases[i].input);
        stun_md_document *owner=NULL;
        CHECK(stun_md_parse(cases[i].input,length,128u,
                            length+1u,&owner)==STUN_UI_OK);
        CHECK(owner!=NULL);
        const stun_ui_rich_document_view view=stun_md_document_view(owner);
        CHECK(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
        CHECK(view.raw_markdown_bytes==length);
        CHECK(memcmp(view.raw_markdown,cases[i].input,length)==0);
        CHECK(kinds(view,STUN_UI_BLOCK_EMPHASIS)==cases[i].emphasis);
        CHECK(kinds(view,STUN_UI_BLOCK_STRONG)==0u);
        CHECK(text_is(view,cases[i].text));
        if(cases[i].emphasis) {
            bool child_owned=false;
            for(size_t id=1u;id<view.document.block_count;++id) {
                const stun_ui_block *node=&view.document.blocks[id];
                if(node->kind!=STUN_UI_BLOCK_EMPHASIS ||
                   node->parent_index>=id) continue;
                if(view.document.blocks[node->parent_index].kind!=
                   STUN_UI_BLOCK_PARAGRAPH) continue;
                child_owned=true;
            }
            CHECK(child_owned);
        }
        stun_md_document_destroy(owner);
    }
    /* Failing to allocate the actual UTF-8 TEXT child must not expose a
     * partially allocated paragraph/emphasis/Unicode boundary model. */
    const char exhausted[]="*foo" NBSP "bar*\n";
    stun_md_document *result=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(exhausted,sizeof(exhausted)-1u,3u,
                        sizeof(exhausted),&result)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(result==NULL);
    return 0;
}

/* Terminal oversize delimiter runs may supply exactly one closer for the
 * outer owner (leaving literal suffix markers), or two matched same-length
 * closers in a nested owner chain. The raw source is never rewritten. */
static int test_terminal_longer_emphasis_closers(void) {
    typedef struct {
        const char *input,*text,*unused;
        stun_ui_block_kind kind;
        size_t count;
        bool nested;
    } terminal_fixture;
    static const terminal_fixture cases[]={
        {"*foo**\n","foo","*",STUN_UI_BLOCK_EMPHASIS,1u,false}, /* #443 */
        {"**foo***\n","foo","*",STUN_UI_BLOCK_STRONG,1u,false}, /* #446 */
        {"*foo****\n","foo","***",STUN_UI_BLOCK_EMPHASIS,1u,false},/* #447 */
        {"_foo__\n","foo","_",STUN_UI_BLOCK_EMPHASIS,1u,false}, /* #455 */
        {"__foo___\n","foo","_",STUN_UI_BLOCK_STRONG,1u,false}, /* #458 */
        {"_foo____\n","foo","___",STUN_UI_BLOCK_EMPHASIS,1u,false},/* #459 */
        {"*foo *bar**\n","bar",NULL,STUN_UI_BLOCK_EMPHASIS,2u,true}, /* #409 */
        {"**foo **bar****\n","bar",NULL,STUN_UI_BLOCK_STRONG,2u,true} /* #427 */
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==8u,
                   "terminal closer AST fixture denominator");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const size_t n=strlen(cases[i].input);
        stun_md_document *d=NULL;
        CHECK(stun_md_parse(cases[i].input,n,128u,n+1u,&d)==STUN_UI_OK);
        CHECK(d!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(d);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        CHECK(v.raw_markdown_bytes==n);
        CHECK(memcmp(v.raw_markdown,cases[i].input,n)==0);
        CHECK(kinds(v,cases[i].kind)==cases[i].count);
        CHECK(text_is(v,cases[i].text));
        if(cases[i].unused) CHECK(text_is(v,cases[i].unused));
        if(cases[i].nested) {
            bool owned=false;
            for(size_t k=1u;k<v.document.block_count;++k) {
                const stun_ui_block *node=&v.document.blocks[k];
                if(node->kind==cases[i].kind && node->parent_index<k &&
                   v.document.blocks[node->parent_index].kind==cases[i].kind)
                    owned=true;
            }
            CHECK(owned);
        }
        stun_md_document_destroy(d);
    }
    const char limited[]="*foo**\n";
    stun_md_document *owner=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,3u,
                        sizeof(limited),&owner)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(owner==NULL);
    return 0;
}

/* #182: the rule-of-three must preserve semantic node ancestry and raw
 * Markdown bytes, not merely happen to render a matching HTML string. */
static int test_triple_and_mixed_delimiter_runs(void) {
    typedef struct {
        const char *source,*inner_text;
        size_t emphasis,strong;
        stun_ui_block_kind outer,inner;
    } pair_fixture;
    static const pair_fixture samples[]={
        {"***foo***\n","foo",1u,1u,
         STUN_UI_BLOCK_EMPHASIS,STUN_UI_BLOCK_STRONG},
        {"___foo___\n","foo",1u,1u,
         STUN_UI_BLOCK_EMPHASIS,STUN_UI_BLOCK_STRONG},
        {"***foo* bar**\n","foo",1u,1u,
         STUN_UI_BLOCK_STRONG,STUN_UI_BLOCK_EMPHASIS},
        {"*foo **bar***\n","bar",1u,1u,
         STUN_UI_BLOCK_EMPHASIS,STUN_UI_BLOCK_STRONG},
        {"***foo**bar*\n","foo",1u,1u,
         STUN_UI_BLOCK_EMPHASIS,STUN_UI_BLOCK_STRONG},
        {"*foo**bar*\n","foo**bar",1u,0u,
         STUN_UI_BLOCK_EMPHASIS,STUN_UI_BLOCK_TEXT}
    };
    _Static_assert(sizeof(samples)/sizeof(samples[0])==6u,
                   "triple / mixed run native AST denominator");
    for(size_t i=0u;i<sizeof(samples)/sizeof(samples[0]);++i) {
        const size_t n=strlen(samples[i].source);
        stun_md_document *owner=NULL;
        CHECK(stun_md_parse(samples[i].source,n,128u,n+1u,&owner)==
              STUN_UI_OK);
        CHECK(owner!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(owner);
        CHECK(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        CHECK(v.raw_markdown_bytes==n);
        CHECK(memcmp(v.raw_markdown,samples[i].source,n)==0);
        CHECK(kinds(v,STUN_UI_BLOCK_EMPHASIS)==samples[i].emphasis);
        CHECK(kinds(v,STUN_UI_BLOCK_STRONG)==samples[i].strong);
        CHECK(text_is(v,samples[i].inner_text));
        bool correct_owner=false;
        for(size_t j=1u;j<v.document.block_count;++j) {
            const stun_ui_block *inner=&v.document.blocks[j];
            if(inner->kind!=samples[i].inner || inner->parent_index>=j)
                continue;
            const stun_ui_block *outer=
                &v.document.blocks[inner->parent_index];
            if(outer->kind!=samples[i].outer) continue;
            if(samples[i].inner==STUN_UI_BLOCK_TEXT) {
                const size_t bytes=strlen(samples[i].inner_text);
                correct_owner=inner->byte_end-inner->byte_begin==bytes &&
                    !memcmp(v.document.source+inner->byte_begin,
                            samples[i].inner_text,bytes);
            } else {
                for(size_t k=j+1u;k<v.document.block_count;++k) {
                    const stun_ui_block *text=&v.document.blocks[k];
                    if(text->kind==STUN_UI_BLOCK_TEXT &&
                       text->parent_index==j &&
                       text->byte_end-text->byte_begin==
                           strlen(samples[i].inner_text) &&
                       !memcmp(v.document.source+text->byte_begin,
                               samples[i].inner_text,
                               strlen(samples[i].inner_text)))
                        correct_owner=true;
                }
            }
        }
        CHECK(correct_owner);
        stun_md_document_destroy(owner);
    }
    const char bounded[]="***foo***\n";
    stun_md_document *rejected=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(bounded,sizeof(bounded)-1u,4u,
                        sizeof(bounded),&rejected)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(rejected==NULL);
    /* Explicitly bound worst-case delimiter searches without publishing an
     * incomplete document; repeated unmatched opener candidates are hostile. */
    char hostile[6001];
    size_t written=0u;
    for(size_t i=0u;i<1000u;++i) {
        memcpy(hostile+written,"*a ",3u);written+=3u;
    }
    for(size_t i=0u;i<1000u;++i) {
        memcpy(hostile+written,"b_ ",3u);written+=3u;
    }
    hostile[written++]='\n';
    rejected=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(hostile,written,16384u,
                        written+1u,&rejected)==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(rejected==NULL);
    return 0;
}

/* Official #473/#474/#521: delimiter runs inside a valid link label
 * cannot close emphasis outside the link. Verify the typed owner, decoded
 * label payload, borrowed metadata and failure-atomic node exhaustion. */
static int test_link_label_delimiter_isolation(void) {
    typedef struct {
        const char *source,*label,*href,*literal;
    } link_fixture;
    static const link_fixture cases[]={
        {"*[bar*](/url)\n","bar*","/url","*"},
        {"_foo [bar_](/url)\n","bar_","/url","_foo "},
        {"*[foo*](/uri)\n","foo*","/uri","*"}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==3u,
                   "link delimiter boundary fixture denominator");
    for(size_t i=0u;i<sizeof(cases)/sizeof(cases[0]);++i) {
        const size_t bytes=strlen(cases[i].source);
        stun_md_document *owner=NULL;
        CHECK(stun_md_parse(cases[i].source,bytes,128u,
                            bytes+1u,&owner)==STUN_UI_OK);
        CHECK(owner!=NULL);
        const stun_ui_rich_document_view view=stun_md_document_view(owner);
        CHECK(stun_ui_rich_document_validate(&view,128u)==STUN_UI_OK);
        CHECK(view.raw_markdown_bytes==bytes);
        CHECK(memcmp(view.raw_markdown,cases[i].source,bytes)==0);
        CHECK(kinds(view,STUN_UI_BLOCK_LINK)==1u);
        CHECK(kinds(view,STUN_UI_BLOCK_EMPHASIS)==0u);
        CHECK(kinds(view,STUN_UI_BLOCK_STRONG)==0u);
        CHECK(metadata_is(view,STUN_UI_BLOCK_LINK,cases[i].href));
        CHECK(text_is(view,cases[i].literal));
        bool link_owns_label=false;
        for(size_t k=1u;k<view.document.block_count;++k) {
            const stun_ui_block *node=&view.document.blocks[k];
            if(node->kind!=STUN_UI_BLOCK_TEXT ||
               node->parent_index>=k ||
               view.document.blocks[node->parent_index].kind!=
                   STUN_UI_BLOCK_LINK ||
               node->byte_end<node->byte_begin ||
               node->byte_end>view.document.source_bytes) continue;
            const size_t size=strlen(cases[i].label);
            if(node->byte_end-node->byte_begin==size &&
               memcmp(view.document.source+node->byte_begin,
                      cases[i].label,size)==0)
                link_owns_label=true;
        }
        CHECK(link_owns_label);
        stun_md_document_destroy(owner);
    }
    const char limited[]="*[bar*](/url)\n";
    stun_md_document *rejected=(stun_md_document*)(uintptr_t)1u;
    CHECK(stun_md_parse(limited,sizeof(limited)-1u,4u,
                        sizeof(limited),&rejected)==STUN_UI_CAPACITY_EXCEEDED);
    CHECK(rejected==NULL);
    return 0;
}

/* This is a *source-column* dedent, not a generic trim: original raw
 * Markdown stays intact and code TEXT keeps every residual space. Verify
 * independently owned code child bytes and the final escaped HTML output. */
static int test_root_fence_indent_semantics(void) {
#define FENCE_VERIFY(expr) do { \
    if(!(expr)) { \
        fprintf(stderr,"%s:%d: failed: %s\n",__FILE__,__LINE__,#expr); \
        failure=1;goto done; \
    } \
} while(0)
    int failure=0;
    stun_md_document *doc=NULL;
    typedef struct fence_indent_fixture {
        const char *source,*code,*html;
    } fence_indent_fixture;
    static const fence_indent_fixture cases[]={
        {" " TICK TICK TICK "\n aaa\naaa\n" TICK TICK TICK "\n",
         "aaa\naaa\n","<pre><code>aaa\naaa\n</code></pre>\n"},
        {"  " TICK TICK TICK "\naaa\n  aaa\naaa\n  " TICK TICK TICK "\n",
         "aaa\naaa\naaa\n","<pre><code>aaa\naaa\naaa\n</code></pre>\n"},
        {"   " TICK TICK TICK "\n   aaa\n    aaa\n  aaa\n   "
         TICK TICK TICK "\n",
         "aaa\n aaa\naaa\n","<pre><code>aaa\n aaa\naaa\n</code></pre>\n"},
        {"  ~~~\n abc\n    def\n  ~~~\n",
         "abc\n  def\n","<pre><code>abc\n  def\n</code></pre>\n"},
        {"  ~~~\n\n x\n  ~~~\n",
         "\nx\n","<pre><code>\nx\n</code></pre>\n"},
        {"  ~~~\n&copy;\n  &amp;\n  ~~~\n",
         "&copy;\n&amp;\n",
         "<pre><code>&amp;copy;\n&amp;amp;\n</code></pre>\n"}
    };
    _Static_assert(sizeof(cases)/sizeof(cases[0])==6u,
                   "root fenced-code indent coverage");
    for(size_t k=0u;k<sizeof(cases)/sizeof(cases[0]);++k) {
        const size_t n=strlen(cases[k].source);
        FENCE_VERIFY(stun_md_parse(cases[k].source,n,128u,n+1u,&doc)==
                     STUN_UI_OK);
        FENCE_VERIFY(doc!=NULL);
        const stun_ui_rich_document_view v=stun_md_document_view(doc);
        FENCE_VERIFY(stun_ui_rich_document_validate(&v,128u)==STUN_UI_OK);
        FENCE_VERIFY(v.raw_markdown_bytes==n);
        FENCE_VERIFY(!memcmp(v.raw_markdown,cases[k].source,n));
        FENCE_VERIFY(kinds(v,STUN_UI_BLOCK_CODE_FENCE)==1u);
        size_t fence_id=SIZE_MAX;
        for(size_t i=1u;i<v.document.block_count;++i) {
            if(v.document.blocks[i].kind==STUN_UI_BLOCK_CODE_FENCE)
                fence_id=i;
        }
        FENCE_VERIFY(fence_id!=SIZE_MAX);
        FENCE_VERIFY(v.document.blocks[fence_id].parent_index==0u);
        char code[256]={0};
        size_t used=0u;
        for(size_t i=fence_id+1u;i<v.document.block_count;++i) {
            const stun_ui_block *node=&v.document.blocks[i];
            if(node->parent_index!=fence_id)continue;
            FENCE_VERIFY(node->kind==STUN_UI_BLOCK_TEXT);
            FENCE_VERIFY(node->byte_end>=node->byte_begin);
            FENCE_VERIFY(node->byte_end<=v.document.source_bytes);
            const size_t len=node->byte_end-node->byte_begin;
            FENCE_VERIFY(used+len<sizeof(code));
            memcpy(code+used,v.document.source+node->byte_begin,len);
            used+=len;
        }
        FENCE_VERIFY(used==strlen(cases[k].code));
        FENCE_VERIFY(!memcmp(code,cases[k].code,used));
        size_t needed=SIZE_MAX,reported=SIZE_MAX;
        FENCE_VERIFY(stun_md_render_html(
            &v,128u,NULL,0u,&needed)==STUN_UI_OK);
        const size_t expected=strlen(cases[k].html);
        FENCE_VERIFY(needed==expected);
        char html[512];
        FENCE_VERIFY(needed+2u<sizeof(html));
        memset(html,0x4a,sizeof(html));
        FENCE_VERIFY(stun_md_render_html(&v,128u,html,needed,&reported)==
                     STUN_UI_CAPACITY_EXCEEDED);
        FENCE_VERIFY(reported==needed);
        for(size_t i=0u;i<sizeof(html);++i)
            FENCE_VERIFY((unsigned char)html[i]==0x4au);
        FENCE_VERIFY(stun_md_render_html(
            &v,128u,html,needed+1u,&reported)==STUN_UI_OK);
        FENCE_VERIFY(reported==needed);
        FENCE_VERIFY(!memcmp(html,cases[k].html,expected+1u));
        FENCE_VERIFY((unsigned char)html[expected+1u]==0x4au);
        stun_md_document_destroy(doc);
        doc=NULL;
    }
done:
    stun_md_document_destroy(doc);
    return failure;
#undef FENCE_VERIFY
}

int main(void) {
    if(test_block_and_inline_baseline()) return 1;
    if(test_setext_unicode_and_crlf()) return 1;
    if(test_unclosed_fence_and_empty()) return 1;
    if(test_root_fence_indent_semantics()) return 1;
    if(test_limit_and_failure_transaction()) return 1;
    if(test_deterministic_ids()) return 1;
    if(test_code_span_exact_runs_and_text()) return 1;
    if(test_code_span_delimiter_precedence()) return 1;
    if(test_ascii_emphasis_flanking()) return 1;
    if(test_nested_emphasis_pairs()) return 1;
    if(test_unicode_whitespace_emphasis()) return 1;
    if(test_terminal_longer_emphasis_closers()) return 1;
    if(test_triple_and_mixed_delimiter_runs()) return 1;
    if(test_link_label_delimiter_isolation()) return 1;
    if(test_gfm_tables()) return 1;
    if(test_gfm_table_termination_and_uneven_rows()) return 1;
    if(test_gfm_table_alignment_ownership()) return 1;
    if(test_gfm_even_backslash_cell_ownership()) return 1;
    if(test_gfm_explicit_quote_table_ownership()) return 1;
    if(test_gfm_list_table_ownership()) return 1;
    if(test_gfm_composed_list_table_ownership()) return 1;
    if(test_gfm_paragraph_to_list_table()) return 1;
    if(test_gfm_task_items()) return 1;
    if(test_gfm_nested_task_ownership()) return 1;
    puts("C11 Markdown parser: all native tests passed");
    return 0;
}
