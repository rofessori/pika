/* SPDX-License-Identifier: GPL-3.0-or-later
 * Unit tests for libpika. Built and run with `make check`.
 */
#include "pika/pika.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond) do { \
    if (cond) { tests_passed++; } \
    else { failures++; \
        printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
static int tests_passed = 0;

static void test_orp(void) {
    CHECK(pika_orp_for_length(1) == 0);
    CHECK(pika_orp_for_length(4) == 1);
    CHECK(pika_orp_for_length(7) == 2);
    CHECK(pika_orp_for_length(12) == 3);
    CHECK(pika_orp_for_length(20) == 4);
}

static void test_tokenize(void) {
    pika_tokens t;
    pika_tokenize("The quick brown fox.", NULL, &t);
    CHECK(t.count == 4);
    CHECK(strcmp(t.items[0].text, "The") == 0);
    CHECK(strcmp(t.items[3].text, "fox.") == 0);
    /* sentence-final word should dwell longer than a plain word */
    CHECK(t.items[3].weight > t.items[0].weight);
    pika_tokens_free(&t);
}

static void test_tokenize_empty(void) {
    pika_tokens t;
    pika_tokenize("   \n\t  ", NULL, &t);
    CHECK(t.count == 0);
    pika_tokens_free(&t);
}

static void test_html(void) {
    strbuf out; sb_init(&out);
    const char *html =
        "<html><head><title>x</title><style>p{}</style></head>"
        "<body><nav>menu home</nav><p>Hello <b>world</b>.</p>"
        "<script>alert(1)</script></body></html>";
    pika_html_to_text(html, strlen(html), 1, &out);
    CHECK(strstr(out.data, "Hello world.") != NULL);
    CHECK(strstr(out.data, "menu") == NULL);   /* nav dropped in reader mode */
    CHECK(strstr(out.data, "alert") == NULL);  /* script always dropped */
    sb_free(&out);
}

static void test_html_entities(void) {
    strbuf out; sb_init(&out);
    const char *html = "<p>Ben &amp; Jerry &mdash; 100&deg; &#65;</p>";
    pika_html_to_text(html, strlen(html), 0, &out);
    CHECK(strstr(out.data, "Ben & Jerry") != NULL);
    CHECK(strstr(out.data, "A") != NULL);
    sb_free(&out);
}

static void test_markdown(void) {
    strbuf out; sb_init(&out);
    const char *md = "# Title\n\nSome **bold** and a [link](http://x).\n";
    pika_markdown_to_text(md, strlen(md), &out);
    CHECK(strstr(out.data, "Title") != NULL);
    CHECK(strstr(out.data, "**") == NULL);
    CHECK(strstr(out.data, "bold") != NULL);
    CHECK(strstr(out.data, "link") != NULL);
    CHECK(strstr(out.data, "http://x") == NULL);
    sb_free(&out);
}

static void test_json_escape(void) {
    strbuf out; sb_init(&out);
    pika_json_escape(&out, "a\"b\\c\n");
    CHECK(strcmp(out.data, "a\\\"b\\\\c\\n") == 0);
    sb_free(&out);
}

static void test_detect(void) {
    const char *h = "<!DOCTYPE html><html>";
    CHECK(pika_detect_format(NULL, h, strlen(h)) == PIKA_FMT_HTML);
    CHECK(pika_detect_format("a.pdf", "", 0) == PIKA_FMT_PDF);
    CHECK(pika_detect_format("a.md", "", 0) == PIKA_FMT_MARKDOWN);
    CHECK(pika_detect_format(NULL, "%PDF-1.7", 8) == PIKA_FMT_PDF);

    /* pasted snippets / source code are detected as HTML ... */
    const char *snippet = "<span class=\"x\">hi</span>";
    CHECK(pika_detect_format(NULL, snippet, strlen(snippet)) == PIKA_FMT_HTML);
    const char *closing = "some text then </div> more";
    CHECK(pika_detect_format(NULL, closing, strlen(closing)) == PIKA_FMT_HTML);
    /* ... but ordinary prose and non-markup code are not */
    const char *prose = "if a < b and c > d then it holds";
    CHECK(pika_detect_format(NULL, prose, strlen(prose)) == PIKA_FMT_TEXT);
    const char *cpp = "std::vector<int> v; return a<b;";
    CHECK(pika_detect_format(NULL, cpp, strlen(cpp)) == PIKA_FMT_TEXT);
}

int main(void) {
    test_orp();
    test_tokenize();
    test_tokenize_empty();
    test_html();
    test_html_entities();
    test_markdown();
    test_json_escape();
    test_detect();

    printf("pika tests: %d passed, %d failed\n", tests_passed, failures);
    return failures ? 1 : 0;
}
