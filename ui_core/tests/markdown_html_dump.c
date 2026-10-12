/* Test-only native RichDoc -> HTML CLI.
 * Input: stdin exact UTF-8 bytes (at most 1 MiB).
 * Output: stdout HTML bytes only; failure emits diagnostics to stderr.
 * Uses the same production-candidate C11 parse + renderer implementations.
 */
#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    MD_TEST_INPUT_MAX = 1 << 20,
    MD_TEST_OUTPUT_MAX = 8 << 20,
    MD_TEST_NODES_MAX = 16384
};

int main(void) {
    char *source = (char *)malloc(MD_TEST_INPUT_MAX + 1u);
    if (!source) {
        fputs("markdown HTML test CLI: input OOM\n", stderr);
        return 2;
    }
    size_t length=0u;
    while(length<MD_TEST_INPUT_MAX) {
        const size_t n = fread(source+length,1u,MD_TEST_INPUT_MAX-length,stdin);
        length+=n;
        if(!n) break;
    }
    unsigned char extra=0u;
    if(ferror(stdin) || (length==MD_TEST_INPUT_MAX &&
                        fread(&extra,1u,1u,stdin)!=0u)) {
        fputs("markdown HTML test CLI: input too large or I/O failure\n",stderr);
        free(source);
        return 2;
    }

    stun_md_document *doc=NULL;
    stun_ui_status status=stun_md_parse(
        source,length,MD_TEST_NODES_MAX,MD_TEST_INPUT_MAX,&doc);
    free(source);
    if(status!=STUN_UI_OK || !doc) {
        fprintf(stderr,"markdown HTML test CLI: parse error %d\n",(int)status);
        return 2;
    }
    stun_ui_rich_document_view view=stun_md_document_view(doc);
    size_t size=0u;
    status=stun_md_render_html(&view,MD_TEST_NODES_MAX,NULL,0u,&size);
    if(status!=STUN_UI_OK || size>MD_TEST_OUTPUT_MAX) {
        fprintf(stderr,"markdown HTML test CLI: render/count error %d (%zu)\n",
                (int)status,size);
        stun_md_document_destroy(doc);
        return 2;
    }

    char *html=(char*)malloc(size+1u);
    if(!html) {
        fputs("markdown HTML test CLI: output OOM\n",stderr);
        stun_md_document_destroy(doc);
        return 2;
    }
    status=stun_md_render_html(&view,MD_TEST_NODES_MAX,html,size+1u,&size);
    if(status!=STUN_UI_OK) {
        fprintf(stderr,"markdown HTML test CLI: render error %d\n",(int)status);
        free(html);
        stun_md_document_destroy(doc);
        return 2;
    }

    const bool wrote=(size==0u || fwrite(html,1u,size,stdout)==size) &&
                     fflush(stdout)==0 && !ferror(stdout);
    free(html);
    stun_md_document_destroy(doc);
    if(!wrote) {
        fputs("markdown HTML test CLI: stdout write failed\n",stderr);
        return 2;
    }
    return 0;
}
