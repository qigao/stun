/* #186: isolated C11 RichDoc -> real Tango buffer acceptance.
 * This is a test-only projection, never a second grammar or production
 * MarkdownWidget/Tango parser switch. The candidate C11 parser owns
 * immutable source and metadata until the consumer returns.
 */
#include <stun/markdown_c11.h>
#include <tui.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Tango Markdown candidate: %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        failed=1; goto cleanup; \
    } \
} while (0)

static bool ancestor_kind(const stun_ui_rich_document_view *v, size_t index,
                          stun_ui_block_kind kind) {
    while (index != 0u) {
        if (index >= v->document.block_count) return false;
        const stun_ui_block *b=&v->document.blocks[index];
        if (b->kind==kind) return true;
        if (b->parent_index>=index) return false;
        index=b->parent_index;
    }
    return false;
}

static bool copy_text(const stun_ui_rich_document_view *v, size_t index,
                      char *buffer, size_t capacity) {
    const stun_ui_block *node=&v->document.blocks[index];
    if (node->kind!=STUN_UI_BLOCK_TEXT ||
        node->byte_end<node->byte_begin ||
        node->byte_end>v->document.source_bytes) return false;
    const size_t length=node->byte_end-node->byte_begin;
    if (length==0u || length>=capacity) return false;
    memcpy(buffer,v->document.source+node->byte_begin,length);
    buffer[length]='\0';
    return true;
}

static int run_case(const char *source, bool expected_checked,
                    const char *expected_label) {
    int failed=0;
    stun_md_document *document=NULL;
    tui_buffer_t *screen=NULL;
    const size_t length=strlen(source);
    REQUIRE(stun_md_parse(source,length,512u,length+1u,&document)==STUN_UI_OK);
    REQUIRE(document!=NULL);
    const stun_ui_rich_document_view v=stun_md_document_view(document);
    REQUIRE(stun_ui_rich_document_validate(&v,512u)==STUN_UI_OK);
    REQUIRE(v.raw_markdown_bytes==length);
    REQUIRE(memcmp(v.raw_markdown,source,length)==0);

    size_t task_count=0u,checkbox_count=0u,table_count=0u;
    size_t data_row=SIZE_MAX;
    unsigned row_count=0u;
    bool checked=false;
    char label[40]={0};
    for (size_t i=1u;i<v.document.block_count;++i) {
        const stun_ui_block *b=&v.document.blocks[i];
        if (b->kind==STUN_UI_BLOCK_TASK_ITEM) ++task_count;
        if (b->kind==STUN_UI_BLOCK_CHECKBOX) {
            ++checkbox_count;
            REQUIRE(b->parent_index<i);
            REQUIRE(v.document.blocks[b->parent_index].kind==STUN_UI_BLOCK_TASK_ITEM);
            const char *metadata=NULL;size_t size=0u;
            REQUIRE(stun_ui_rich_document_metadata(&v,i,&metadata,&size)==STUN_UI_OK);
            REQUIRE(metadata!=NULL && size==1u);
            REQUIRE(metadata[0]=='x' || metadata[0]==' ');
            checked=metadata[0]=='x';
        }
        if (b->kind==STUN_UI_BLOCK_TABLE) {
            ++table_count;
            REQUIRE(ancestor_kind(&v,i,STUN_UI_BLOCK_TASK_ITEM));
            const char *metadata=NULL;size_t size=0u;
            REQUIRE(stun_ui_rich_document_metadata(&v,i,&metadata,&size)==STUN_UI_OK);
            REQUIRE(metadata!=NULL && size==2u);
            REQUIRE((unsigned char)metadata[0]==STUN_UI_TABLE_ALIGN_LEFT);
            REQUIRE((unsigned char)metadata[1]==STUN_UI_TABLE_ALIGN_RIGHT);
        }
        if (b->kind==STUN_UI_BLOCK_TABLE_ROW) {
            REQUIRE(ancestor_kind(&v,i,STUN_UI_BLOCK_TABLE));
            if (++row_count==2u) data_row=i;
        }
        if (b->kind==STUN_UI_BLOCK_TEXT &&
            ancestor_kind(&v,i,STUN_UI_BLOCK_TASK_ITEM) &&
            !ancestor_kind(&v,i,STUN_UI_BLOCK_TABLE)) {
            REQUIRE(label[0]=='\0');
            REQUIRE(copy_text(&v,i,label,sizeof(label)));
        }
    }
    REQUIRE(task_count==1u && checkbox_count==1u && table_count==1u);
    REQUIRE(row_count==2u && data_row!=SIZE_MAX);
    REQUIRE(checked==expected_checked);
    REQUIRE(strcmp(label,expected_label)==0);

    /* Inspect typed data cells instead of reparsing the Markdown. */
    char cells[2][40]={{0}};
    unsigned column=0u;
    for (size_t i=data_row+1u;i<v.document.block_count;++i) {
        const stun_ui_block *b=&v.document.blocks[i];
        if (b->kind==STUN_UI_BLOCK_TABLE_CELL &&
            b->parent_index==data_row) {
            REQUIRE(column<2u);
            unsigned cell_text_count=0u;
            for (size_t j=i+1u;j<v.document.block_count;++j) {
                const stun_ui_block *child=&v.document.blocks[j];
                if (child->parent_index==i &&
                    child->kind==STUN_UI_BLOCK_TEXT) {
                    REQUIRE(cell_text_count++==0u);
                    REQUIRE(copy_text(&v,j,cells[column],sizeof(cells[column])));
                }
            }
            REQUIRE(cell_text_count==1u);
            ++column;
        }
    }
    REQUIRE(column==2u);
    REQUIRE(strcmp(cells[0],"left")==0 && strcmp(cells[1],"right")==0);

    /* Real Tango consumes the immutable C11 typed projection synchronously. */
    screen=tui_buffer_create(32,3);
    REQUIRE(screen!=NULL);
    const tui_theme_t theme=tui_theme_default();
    tui_widget_checkbox(screen,0,0,checked,label,&theme);
    char rendered_row[90]={0};
    const int written=snprintf(rendered_row,sizeof(rendered_row),
                               "%s | %s",cells[0],cells[1]);
    REQUIRE(written>0 && (size_t)written<sizeof(rendered_row));
    tui_buffer_text(screen,0,1,rendered_row,theme.fg,theme.bg);
    const tui_cell_t *glyph=tui_buffer_at_const(screen,0,0);
    REQUIRE(glyph!=NULL && glyph->ch==(checked?0x2611u:0x2610u));
    for (size_t i=0u;i<(size_t)written;++i) {
        const tui_cell_t *cell=tui_buffer_at_const(screen,(int)i,1);
        REQUIRE(cell!=NULL && cell->ch==(unsigned char)rendered_row[i]);
    }
    REQUIRE(v.raw_markdown_bytes==length);
    REQUIRE(memcmp(v.raw_markdown,source,length)==0);

cleanup:
    if (screen) tui_buffer_destroy(screen);
    stun_md_document_destroy(document);
    return failed;
}

int main(void) {
    static const char checked[] =
        "- [x] shipped\n\n"
        "  | A | B |\n  | :--- | ---: |\n  | left | right |\n";
    static const char unchecked[] =
        "- [ ] pending\n\n"
        "  | A | B |\n  | :--- | ---: |\n  | left | right |\n";
    if (run_case(checked,true,"shipped") ||
        run_case(unchecked,false,"pending")) return 1;
    puts("Tango C11 candidate consumer: 2/2 isolated cases passed");
    return 0;
}
