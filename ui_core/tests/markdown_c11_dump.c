/* Test-only C11 RichDoc dump for independent Python Markdown AST comparison.
 * The parser remains in Stun::UICore; this tool is not a rendering backend.
 * Each line: index TAB kind TAB parent TAB heading-level TAB text-hex
 *            TAB metadata-hex. No JSON/text escaping or third-party runtime.
 */
#include <stun/markdown_c11.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    STUN_MD_ORACLE_MAX_SOURCE = 1 << 20,
    STUN_MD_ORACLE_MAX_NODES = 16384
};

static const char *node_kind(stun_ui_block_kind kind) {
    switch (kind) {
    case STUN_UI_BLOCK_DOCUMENT: return "document";
    case STUN_UI_BLOCK_PARAGRAPH: return "paragraph";
    case STUN_UI_BLOCK_HEADING: return "heading";
    case STUN_UI_BLOCK_TEXT: return "text";
    case STUN_UI_BLOCK_CODE_FENCE: return "code_fence";
    case STUN_UI_BLOCK_CODE_INDENTED: return "code_indented";
    case STUN_UI_BLOCK_CODE_SPAN: return "code_span";
    case STUN_UI_BLOCK_LIST: return "list";
    case STUN_UI_BLOCK_LIST_ITEM: return "list_item";
    case STUN_UI_BLOCK_QUOTE: return "quote";
    case STUN_UI_BLOCK_TABLE: return "table";
    case STUN_UI_BLOCK_TABLE_ROW: return "table_row";
    case STUN_UI_BLOCK_TABLE_CELL: return "table_cell";
    case STUN_UI_BLOCK_ORDERED_LIST: return "ordered_list";
    case STUN_UI_BLOCK_EMPHASIS: return "emphasis";
    case STUN_UI_BLOCK_STRONG: return "strong";
    case STUN_UI_BLOCK_LINK: return "link";
    case STUN_UI_BLOCK_IMAGE: return "image";
    case STUN_UI_BLOCK_STRIKETHROUGH: return "strikethrough";
    case STUN_UI_BLOCK_UNDERLINE: return "underline";
    case STUN_UI_BLOCK_RULE: return "rule";
    case STUN_UI_BLOCK_ENTITY: return "entity";
    case STUN_UI_BLOCK_LINE_BREAK: return "line_break";
    case STUN_UI_BLOCK_TASK_ITEM: return "task_item";
    case STUN_UI_BLOCK_CHECKBOX: return "checkbox";
    case STUN_UI_BLOCK_MATH: return "math";
    case STUN_UI_BLOCK_MATH_BLOCK: return "math_block";
    case STUN_UI_BLOCK_HIGHLIGHT: return "highlight";
    case STUN_UI_BLOCK_SUPERSCRIPT: return "superscript";
    case STUN_UI_BLOCK_SUBSCRIPT: return "subscript";
    case STUN_UI_BLOCK_FOOTNOTE: return "footnote";
    case STUN_UI_BLOCK_FOOTNOTE_DEF: return "footnote_def";
    case STUN_UI_BLOCK_DEFINITION_LIST: return "definition_list";
    case STUN_UI_BLOCK_DEFINITION_TERM: return "definition_term";
    case STUN_UI_BLOCK_DEFINITION_DESC: return "definition_desc";
    case STUN_UI_BLOCK_DIAGRAM: return "diagram";
    default: return NULL;
    }
}

static void write_hex(const char *bytes, size_t length) {
    static const char digits[] = "0123456789abcdef";
    if (!bytes) return;
    for (size_t i=0; i<length; ++i) {
        const unsigned char value=(unsigned char)bytes[i];
        putchar(digits[value >> 4]);
        putchar(digits[value & 15u]);
    }
}

int main(void) {
    char *bytes=(char *)malloc(STUN_MD_ORACLE_MAX_SOURCE+1u);
    if (!bytes) {
        fputs("AST dump: source allocation failed\n",stderr);
        return 2;
    }
    size_t length=0;
    while (length<STUN_MD_ORACLE_MAX_SOURCE) {
        const size_t n=fread(bytes+length,1u,
            STUN_MD_ORACLE_MAX_SOURCE-length,stdin);
        length+=n;
        if (!n) break;
    }
    unsigned char extra=0;
    if (ferror(stdin) ||
        (length==STUN_MD_ORACLE_MAX_SOURCE &&
         fread(&extra,1u,1u,stdin)!=0u)) {
        fputs("AST dump: source exceeds 1 MiB or stdin failed\n",stderr);
        free(bytes);
        return 2;
    }
    stun_md_document *doc=NULL;
    const stun_ui_status status=stun_md_parse(
        bytes,length,STUN_MD_ORACLE_MAX_NODES,
        STUN_MD_ORACLE_MAX_SOURCE,&doc);
    free(bytes);
    if (status!=STUN_UI_OK || !doc) {
        fprintf(stderr,"AST dump: parse failed (status=%d)\n",(int)status);
        return 2;
    }

    const stun_ui_rich_document_view view=stun_md_document_view(doc);
    if (stun_ui_rich_document_validate(
            &view,STUN_MD_ORACLE_MAX_NODES)!=STUN_UI_OK) {
        fputs("AST dump: invalid RichDoc\n",stderr);
        stun_md_document_destroy(doc);
        return 2;
    }
    for (size_t i=0;i<view.document.block_count;++i) {
        const stun_ui_block *node=&view.document.blocks[i];
        const char *kind=node_kind(node->kind);
        if (!kind) {
            fputs("AST dump: unrecognized block kind\n",stderr);
            stun_md_document_destroy(doc);
            return 2;
        }
        printf("%zu\t%s\t%u\t%u\t",i,kind,
               node->parent_index,node->heading_level);
        if (node->kind==STUN_UI_BLOCK_TEXT) {
            write_hex(view.document.source+node->byte_begin,
                      node->byte_end-node->byte_begin);
        }
        putchar('\t');
        const char *metadata=NULL;
        size_t metadata_size=0;
        if (stun_ui_rich_document_metadata(
                &view,i,&metadata,&metadata_size)!=STUN_UI_OK) {
            fputs("AST dump: invalid metadata range\n",stderr);
            stun_md_document_destroy(doc);
            return 2;
        }
        write_hex(metadata,metadata_size);
        putchar('\n');
    }
    const int result=ferror(stdout) ? 2 : 0;
    stun_md_document_destroy(doc);
    return result;
}
