#include <stun/markdown_c11.h>
#include <stun/markdown_html.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test_) do { \
    if (!(test_)) { \
        fprintf(stderr,"%s:%d: failed: %s\n",__FILE__,__LINE__,#test_); \
        return 1; \
    } \
} while (0)

#define TICK "\x60"
/* Explicit UTF-8 test bytes; production Unicode classification comes only
 * from Salts::Unicode 17, never from this fixture vocabulary. */
#define NBSP "\xc2\xa0"
#define EM_SPACE "\xe2\x80\x83"
#define NNBSP "\xe2\x80\xaf"
#define ZWSP "\xe2\x80\x8b"
typedef struct {
    const char *markdown;
    const char *html;
} html_fixture;

static const html_fixture cases[] = {
    { "## Hello\n", "<h2>Hello</h2>\n" },
    { "Normal **Bold** text\n",
      "<p>Normal <strong>Bold</strong> text</p>\n" },
    { "A *bright* day.\n",
      "<p>A <em>bright</em> day.</p>\n" },
    { "> quote\n", "<blockquote>\n<p>quote</p>\n</blockquote>\n" },
    { "1. alpha\n2. beta\n",
      "<ol>\n<li>alpha</li>\n<li>beta</li>\n</ol>\n" },
    { "- one\n- two\n",
      "<ul>\n<li>one</li>\n<li>two</li>\n</ul>\n" },
    { TICK TICK TICK "c\nint v;\n" TICK TICK TICK "\n",
      "<pre><code class=\"language-c\">int v;\n</code></pre>\n" },
    { "A [B](dest)\n", "<p>A <a href=\"dest\">B</a></p>\n" },
    { "a " TICK "code" TICK " b\n",
      "<p>a <code>code</code> b</p>\n" },
    { "---\n", "<hr />\n" },
    { "first\n===\n", "<h1>first</h1>\n" },
    { "first\nsecond\n", "<p>first\nsecond</p>\n" },
    { "a \\*b\n", "<p>a *b</p>\n" },
    { "中😀 👩‍💻\n", "<p>中😀 👩‍💻</p>\n" },
    { "a ~~red~~ day\n", "<p>a <del>red</del> day</p>\n" },
    { "| A | B |\n| --- | --- |\n| one | two |\n",
      "<table>\n<thead>\n<tr>\n<th>A</th>\n<th>B</th>\n</tr>\n"
      "</thead>\n<tbody>\n<tr>\n<td>one</td>\n<td>two</td>\n</tr>\n"
      "</tbody>\n</table>\n" },
    /* CommonMark 0.31.2 #329–#331, #338–#340, #347; code-span
     * contents are literal UTF-8, with exact-run and edge-space rules. */
    { TICK TICK " foo " TICK " bar " TICK TICK "\n",
      "<p><code>foo " TICK " bar</code></p>\n" },          /* #329 */
    { TICK " " TICK TICK " " TICK "\n",
      "<p><code>" TICK TICK "</code></p>\n" },             /* #330 */
    { TICK "  " TICK TICK "  " TICK "\n",
      "<p><code> " TICK TICK " </code></p>\n" },           /* #331 */
    { TICK "\xc2\xa0" "b" "\xc2\xa0" TICK "\n",
      "<p><code>\xc2\xa0" "b" "\xc2\xa0</code></p>\n" },    /* #333 */
    { TICK "  " TICK "\n",
      "<p><code>  </code></p>\n" },                        /* #334 */
    { TICK "foo\\" TICK "bar" TICK "\n",
      "<p><code>foo\\</code>bar" TICK "</p>\n" },          /* #338 */
    { TICK TICK "foo" TICK "bar" TICK TICK "\n",
      "<p><code>foo" TICK "bar</code></p>\n" },             /* #339 */
    { TICK " foo " TICK TICK " bar " TICK "\n",
      "<p><code>foo " TICK TICK " bar</code></p>\n" },     /* #340 */
    { TICK TICK TICK "foo" TICK TICK "\n",
      "<p>" TICK TICK TICK "foo" TICK TICK "</p>\n" },    /* #347 */
    /* #182: a code span may cross root-paragraph physical lines; its
     * internal line endings become spaces, preserving all other bytes. */
    { TICK TICK "\nfoo\nbar  \nbaz\n" TICK TICK "\n",
      "<p><code>foo bar   baz</code></p>\n" },           /* #335 */
    { TICK TICK "\nfoo \n" TICK TICK "\n",
      "<p><code>foo </code></p>\n" },                    /* #336 */
    { TICK "foo   bar \nbaz" TICK "\n",
      "<p><code>foo   bar  baz</code></p>\n" },          /* #337 */
    { TICK TICK "\r\nfoo\r\nbar  \r\nbaz\r\n" TICK TICK "\r\n",
      "<p><code>foo bar   baz</code></p>\n" },
    { TICK "line\nnext" TICK " tail\n",
      "<p><code>line next</code> tail</p>\n" },
    { TICK "foo\nbar" TICK "\nafter\n",
      "<p><code>foo bar</code>\nafter</p>\n" },
    { TICK "foo\n\nbar" TICK "\n",
      "<p>" TICK "foo</p>\n<p>bar" TICK "</p>\n" },
    { TICK "foo\n# Heading\nbar" TICK "\n",
      "<p>" TICK "foo</p>\n<h1>Heading</h1>\n<p>bar" TICK "</p>\n" },
    { TICK TICK "a\n" TICK "b" TICK TICK "\n",
      "<p><code>a " TICK "b</code></p>\n" },
    { TICK " \n " TICK "\n",
      "<p><code>   </code></p>\n" },
    /* #182: emphasis/link closers inside a valid code span are opaque.
     * An unclosed backtick run remains literal and cannot hide a closer. */
    { "*foo" TICK "*" TICK "\n",
      "<p>*foo<code>*</code></p>\n" },                     /* #341 */
    { "[not a " TICK "link](/foo" TICK ")\n",
      "<p>[not a <code>link](/foo</code>)</p>\n" },        /* #342 */
    { "*hello " TICK "a*b" TICK " world*\n",
      "<p><em>hello <code>a*b</code> world</em></p>\n" },
    { "[hello " TICK "x]y" TICK "](url)\n",
      "<p><a href=\"url\">hello <code>x]y</code></a></p>\n" },
    { "*foo" TICK TICK "bar*\n",
      "<p><em>foo" TICK TICK "bar</em></p>\n" },
    { "**hi " TICK "x*y" TICK " done**\n",
      "<p><strong>hi <code>x*y</code> done</strong></p>\n" },
    /* ASCII CommonMark flanking: adjacent punctuation cannot open from a
     * word; underscores inside words cannot become emphasis delimiters. */
    { "a*\"foo\"*\n", "<p>a*&quot;foo&quot;*</p>\n" },         /* #352 */
    { "a_\"foo\"_\n", "<p>a_&quot;foo&quot;_</p>\n" },         /* #359 */
    { "foo_bar_\n", "<p>foo_bar_</p>\n" },                /* #360 */
    { "5_6_78\n", "<p>5_6_78</p>\n" },                   /* #361 */
    { "пристаням_стремятся_\n",
      "<p>пристаням_стремятся_</p>\n" },                 /* #362 */
    { "aa_\"bb\"_cc\n", "<p>aa_&quot;bb&quot;_cc</p>\n" },  /* #363 */
    { "*(*foo)\n", "<p>*(*foo)</p>\n" },                  /* #368 */
    { "_(_foo)\n", "<p>_(_foo)</p>\n" },                  /* #372 */
    { "_foo_bar\n", "<p>_foo_bar</p>\n" },                /* #374 */
    { "_пристаням_стремятся\n",
      "<p>_пристаням_стремятся</p>\n" },                 /* #375 */
    { "_foo_bar_baz_\n",
      "<p><em>foo_bar_baz</em></p>\n" },                 /* #376 */
    { "a**\"foo\"**\n",
      "<p>a**&quot;foo&quot;**</p>\n" },                         /* #380 */
    /* A later delimiter immediately following whitespace may be a nearer
     * opener; it must retain ownership of the final matching closer. */
    { "**foo **bar baz**\n",
      "<p>**foo <strong>bar baz</strong></p>\n" },       /* #471 */
    { "*foo *bar baz*\n",
      "<p>*foo <em>bar baz</em></p>\n" },                /* #472 */
    /* Same-length nested flanking runs claim their own nearer closers.
     * The outer delimiter remains valid only when another closer exists. */
    { "*(*foo*)*\n",
      "<p><em>(<em>foo</em>)</em></p>\n" },           /* #369 */
    { "_(_foo_)_\n",
      "<p><em>(<em>foo</em>)</em></p>\n" },           /* #373 */
    { "__foo, __bar__, baz__\n",
      "<p><strong>foo, <strong>bar</strong>, baz</strong></p>\n" }, /* #389 */
    { "*emph *with emph* in it*\n",
      "<p><em>emph <em>with emph</em> in it</em></p>\n" },
    { "**strong **with strong** in it**\n",
      "<p><strong>strong <strong>with strong</strong> in it</strong></p>\n" },
    { "*x (*y*) z*\n",
      "<p><em>x (<em>y</em>) z</em></p>\n" },
    { "**x (**y**) z**\n",
      "<p><strong>x (<strong>y</strong>) z</strong></p>\n" },
    /* Unicode 17 White_Space is resolved through the published provider.
     * #353 uses U+00A0; U+2003 and U+202F are extra positive controls,
     * whereas U+200B is NOT Unicode White_Space. */
    { "*" NBSP "a" NBSP "*\n",
      "<p>*" NBSP "a" NBSP "*</p>\n" },              /* #353 */
    { "a*" NBSP "b*\n",
      "<p>a*" NBSP "b*</p>\n" },
    { "*x" NBSP "*\n",
      "<p>*x" NBSP "*</p>\n" },
    { "*foo" NBSP "bar*\n",
      "<p><em>foo" NBSP "bar</em></p>\n" },
    { "**" NBSP "foo**\n",
      "<p>**" NBSP "foo**</p>\n" },
    { "_" NBSP "foo_\n",
      "<p>_" NBSP "foo_</p>\n" },
    { "*" EM_SPACE "a" EM_SPACE "*\n",
      "<p>*" EM_SPACE "a" EM_SPACE "*</p>\n" },
    { "*" NNBSP "a" NNBSP "*\n",
      "<p>*" NNBSP "a" NNBSP "*</p>\n" },
    { "*" ZWSP "a*\n",
      "<p><em>" ZWSP "a</em></p>\n" },
    { "*中*\n", "<p><em>中</em></p>\n" },
    /* Longer terminal delimiter runs can close a shorter outer span with
     * unused literal suffix, or exactly close inner+outer same-length spans.
     * Nonterminal and triple/mixed-length rules are NOT generalized here. */
    { "*foo**\n", "<p><em>foo</em>*</p>\n" },              /* #443 */
    { "**foo***\n", "<p><strong>foo</strong>*</p>\n" },    /* #446 */
    { "*foo****\n", "<p><em>foo</em>***</p>\n" },           /* #447 */
    { "_foo__\n", "<p><em>foo</em>_</p>\n" },               /* #455 */
    { "__foo___\n", "<p><strong>foo</strong>_</p>\n" },    /* #458 */
    { "_foo____\n", "<p><em>foo</em>___</p>\n" },           /* #459 */
    { "*foo *bar**\n", "<p><em>foo <em>bar</em></em></p>\n" }, /* #409 */
    { "**foo **bar****\n",
      "<p><strong>foo <strong>bar</strong></strong></p>\n" }, /* #427 */
    /* #182: maximal 1/2/3+ runs, nested strong/emphasis, and rule-of-three.
     * Reference bytes cross-checked against CommonMark 0.31.2 semantics.
     * These are independent strict HTML fixtures, not the full 652 gate. */
    { "***foo***\n", "<p><em><strong>foo</strong></em></p>\n" },
    { "___foo___\n", "<p><em><strong>foo</strong></em></p>\n" },
    { "***foo**\n", "<p>*<strong>foo</strong></p>\n" },
    { "***foo* bar**\n",
      "<p><strong><em>foo</em> bar</strong></p>\n" },
    { "*foo**bar*\n", "<p><em>foo**bar</em></p>\n" },
    { "**foo*, bar**\n",
      "<p><em><em>foo</em>, bar</em>*</p>\n" },
    { "*foo **bar***\n",
      "<p><em>foo <strong>bar</strong></em></p>\n" },
    { "***foo**bar*\n",
      "<p><em><strong>foo</strong>bar</em></p>\n" },
    { "*foo**bar**baz*\n",
      "<p><em>foo<strong>bar</strong>baz</em></p>\n" },
    { "a***b***c\n",
      "<p>a<em><strong>b</strong></em>c</p>\n" },
    { "***foo " TICK "a*b" TICK " bar***\n",
      "<p><em><strong>foo <code>a*b</code> bar</strong></em></p>\n" },
    /* Official CommonMark 0.31.2 exact expected HTML (previously failing):
     * #408/#413/#415–417/#431/#468/#470. Mixed units must neither
     * steal a closer from an inner run nor violate the odd-match rule. */
    { "__foo_ bar_\n",
      "<p><em><em>foo</em> bar</em></p>\n" },             /* #408 */
    { "***foo** bar*\n",
      "<p><em><strong>foo</strong> bar</em></p>\n" },      /* #413 */
    { "*foo**bar***\n",
      "<p><em>foo<strong>bar</strong></em></p>\n" },       /* #415 */
    { "foo***bar***baz\n",
      "<p>foo<em><strong>bar</strong></em>baz</p>\n" },    /* #416 */
    { "foo******bar*********baz\n",
      "<p>foo<strong><strong><strong>bar</strong></strong></strong>***baz</p>\n" }, /* #417 */
    { "**foo *bar***\n",
      "<p><strong>foo <em>bar</em></strong></p>\n" },      /* #431 */
    { "_____foo_____\n",
      "<p><em><strong><strong>foo</strong></strong></em></p>\n" }, /* #468 */
    { "*foo __bar *baz bim__ bam*\n",
      "<p><em>foo <strong>bar *baz bim</strong> bam</em></p>\n" }, /* #470 */
    /* Official #473/#474/#521: link labels cannot donate markers to
     * outside emphasis runs. */
    { "*[bar*](/url)\n",
      "<p>*<a href=\"/url\">bar*</a></p>\n" }, /* #473 */
    { "_foo [bar_](/url)\n",
      "<p>_foo <a href=\"/url\">bar_</a></p>\n" }, /* #474 */
    { "*[foo*](/uri)\n",
      "<p>*<a href=\"/uri\">foo*</a></p>\n" }, /* #521 */
    /* #182 root paragraph-scoped inline ownership: CommonMark 0.31.2
     * #394/#405/#423/#432 must preserve typed SOFT breaks *inside* their
     * nested emphasis/strong owners, never as paragraph-level siblings. */
    { "**Gomphocarpus (*Gomphocarpus physocarpus*, syn.\n"
      "*Asclepias physocarpa*)**\n",
      "<p><strong>Gomphocarpus (<em>Gomphocarpus physocarpus</em>, syn.\n"
      "<em>Asclepias physocarpa</em>)</strong></p>\n" }, /* #394 */
    { "*foo\nbar*\n",
      "<p><em>foo\nbar</em></p>\n" },                     /* #405 */
    { "**foo\nbar**\n",
      "<p><strong>foo\nbar</strong></p>\n" },             /* #423 */
    { "**foo *bar **baz**\nbim* bop**\n",
      "<p><strong>foo <em>bar <strong>baz</strong>\n"
      "bim</em> bop</strong></p>\n" },                   /* #432 */
    /* CRLF normalizes in the semantic inline source but stays unchanged in
     * the separately owned original Markdown byte buffer. */
    { "*foo\r\nbar*\r\n",
      "<p><em>foo\nbar</em></p>\n" },
    /* A blank line, heading or quote interrupts the paragraph: no marker
     * may match across these real block boundaries. */
    { "*foo\n\nbar*\n",
      "<p>*foo</p>\n<p>bar*</p>\n" },
    { "*foo\n# head\nbar*\n",
      "<p>*foo</p>\n<h1>head</h1>\n<p>bar*</p>\n" },
    { "*foo\n> quote*\n",
      "<p>*foo</p>\n<blockquote>\n<p>quote*</p>\n</blockquote>\n" },
    /* #81: paragraph spanning two lines can subsequently become an
     * inherited Setext heading without losing inner emphasis ownership. */
    { "Foo *bar\nbaz*\n====\n",
      "<h1>Foo <em>bar\nbaz</em></h1>\n" }, /* #81 */
    /* #185: real CommonMark hard/soft physical break origin. Only root
     * paragraph boundaries are admitted here; nested/list/quote still use
     * their previously qualified semantics until scoped later. */
    { "foo  \nbaz\n", "<p>foo<br />\nbaz</p>\n" },          /* #633 */
    { "foo\\\nbaz\n", "<p>foo<br />\nbaz</p>\n" },         /* #634 */
    { "foo       \nbaz\n", "<p>foo<br />\nbaz</p>\n" },     /* #635 */
    { "foo  \n     bar\n", "<p>foo<br />\nbar</p>\n" },    /* #636 */
    { "foo\\\n     bar\n", "<p>foo<br />\nbar</p>\n" },   /* #637 */
    { "foo  \n", "<p>foo</p>\n" },                          /* #645 */
    { "foo \n baz\n", "<p>foo\nbaz</p>\n" },                /* #649 */
    { "foo\\\\\nbaz\n", "<p>foo\\\nbaz</p>\n" },
    { "foo\\\n", "<p>foo\\</p>\n" },
    { "foo \nbaz\n", "<p>foo\nbaz</p>\n" },
    { "", "" },
};

_Static_assert(sizeof(cases)/sizeof(cases[0])==122,
               "Explicit HTML fixture denominator changed");

static int test_golden(const html_fixture *f) {
    stun_md_document *doc=NULL;
    size_t source_bytes=strlen(f->markdown);
    CHECK(stun_md_parse(f->markdown,source_bytes,256u,
                        source_bytes+1u,&doc)==STUN_UI_OK);
    const stun_ui_rich_document_view v=stun_md_document_view(doc);
    size_t need=SIZE_MAX;
    CHECK(stun_md_render_html(&v,256u,NULL,0u,&need)==STUN_UI_OK);
    const size_t expect=strlen(f->html);
    CHECK(need==expect);
    char *out=(char *)malloc(need+2u);
    CHECK(out!=NULL);
    memset(out,0xa5,need+2u);

    /* Prove atomic rejection of insufficient output capacity. */
    size_t required=SIZE_MAX;
    if(need) {
        CHECK(stun_md_render_html(&v,256u,out,need,&required)==
              STUN_UI_CAPACITY_EXCEEDED);
        CHECK(required==need);
        for(size_t k=0;k<need+2u;++k)
            CHECK((unsigned char)out[k]==0xa5);
    }
    CHECK(stun_md_render_html(&v,256u,out,need+1u,&required)==STUN_UI_OK);
    CHECK(required==need);
    CHECK(!memcmp(out,f->html,need+1u));
    CHECK((unsigned char)out[need+1u]==0xa5);
    free(out);
    stun_md_document_destroy(doc);
    return 0;
}

static int test_unsafe_unsupported(void) {
    /* Balanced-parenthesis destinations are not implemented by the C11 parser
     * yet. Use a syntactically admitted link to test the actual HTML URL gate. */
    const char bad_url[]="[bad](javascript:alert)\n";
    stun_md_document *doc=NULL;
    CHECK(stun_md_parse(bad_url,sizeof(bad_url)-1u,64,256,&doc)==STUN_UI_OK);
    stun_ui_rich_document_view v=stun_md_document_view(doc);
    bool found_link=false;
    for(size_t i=0;i<v.document.block_count;++i)
        if(v.document.blocks[i].kind==STUN_UI_BLOCK_LINK) found_link=true;
    CHECK(found_link);  /* Must exercise URL admission, not plain-text fallback. */
    char out[128];
    memset(out,0x4d,sizeof(out));
    size_t needed=0;
    CHECK(stun_md_render_html(&v,64u,out,sizeof(out),&needed)==
          STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0;i<sizeof(out);++i)
        CHECK((unsigned char)out[i]==0x4d);
    stun_md_document_destroy(doc);

    /* A second supported link scheme rejection keeps data: fail-closed. */
    const char data_url[]="[bad](data:text/html)\n";
    CHECK(stun_md_parse(data_url,sizeof(data_url)-1u,64,256,&doc)==STUN_UI_OK);
    v=stun_md_document_view(doc);
    found_link=false;
    for(size_t i=0;i<v.document.block_count;++i)
        if(v.document.blocks[i].kind==STUN_UI_BLOCK_LINK) found_link=true;
    CHECK(found_link);
    memset(out,0x4d,sizeof(out));
    CHECK(stun_md_render_html(&v,64u,out,sizeof(out),&needed)==
          STUN_UI_INVALID_ARGUMENT);
    for(size_t i=0;i<sizeof(out);++i)
        CHECK((unsigned char)out[i]==0x4d);
    stun_md_document_destroy(doc);

    const char image[]="![alt](https://example.org/x.png)\n";
    CHECK(stun_md_parse(image,sizeof(image)-1u,64,256,&doc)==STUN_UI_OK);
    v=stun_md_document_view(doc);
    CHECK(stun_md_render_html(&v,64u,NULL,0u,&needed)==
          STUN_UI_INVALID_ARGUMENT);
    stun_md_document_destroy(doc);

    CHECK(stun_md_render_html(NULL,64u,NULL,0u,&needed)==
          STUN_UI_INVALID_ARGUMENT);
    return 0;
}

int main(void) {
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        if(test_golden(&cases[i])) {
            fprintf(stderr,"HTML fixture %zu failed: %s\n",i,cases[i].markdown);
            return 1;
        }
    }
    if(test_unsafe_unsupported()) return 1;
    printf("Stun C11 RichDoc HTML: %zu/%zu bounded subset fixtures passed\n",
           sizeof(cases)/sizeof(cases[0]),sizeof(cases)/sizeof(cases[0]));
    return 0;
}
