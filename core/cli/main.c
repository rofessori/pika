/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * `pika` — parse any document into clean text or a timed RSVP token stream.
 * Designed to compose in unix pipelines:
 *
 *     pika --url https://example.com --reader --text | fold -s
 *     pika paper.pdf --json | jq '.count'
 *     pika notes.md            # opens the TUI reader when on a terminal
 */
#include "pika/pika.h"
#include "pika/proc.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(FILE *f) {
    fputs(
"Usage: pika [OPTIONS] [FILE]\n"
"\n"
"Turn any text/HTML/Markdown/PDF/DOCX into clean text or an RSVP stream.\n"
"Reads FILE, or stdin when no FILE is given.\n"
"\n"
"Input:\n"
"  -u, --url URL       fetch URL with curl/wget before parsing\n"
"  -f, --format FMT    auto|text|md|html|pdf|docx  (default: auto)\n"
"  -R, --reader        HTML reader mode: keep the article, drop the chrome\n"
"  -n, --name NAME     filename hint for format auto-detection on stdin\n"
"      --safe-fetch    refuse to fetch private/loopback addresses\n"
"      --max-bytes N   cap input size in bytes (default: 8388608)\n"
"\n"
"Output:\n"
"  -t, --text          emit cleaned plain text (great for pipelines)\n"
"  -j, --json          emit the RSVP token-stream as JSON\n"
"  -r, --read          open the TUI reader (requires pika-tui)\n"
"  -w, --wpm N         base words-per-minute (default: 350)\n"
"\n"
"  -h, --help          show this help\n"
"  -V, --version       show version\n"
"\n"
"With no output flag: opens the reader on a terminal, else prints text.\n",
        f);
}

enum { OPT_MAXBYTES = 1000, OPT_SAFEFETCH };

int main(int argc, char **argv) {
    const char *url = NULL, *fmt_name = "auto", *name = NULL;
    int reader = 0, want_json = 0, want_text = 0, want_read = 0, wpm = 350;
    int safe_fetch = 0;
    size_t max_bytes = 8u * 1024 * 1024;

    static struct option lo[] = {
        {"url",        required_argument, 0, 'u'},
        {"format",     required_argument, 0, 'f'},
        {"reader",     no_argument,       0, 'R'},
        {"name",       required_argument, 0, 'n'},
        {"text",       no_argument,       0, 't'},
        {"json",       no_argument,       0, 'j'},
        {"read",       no_argument,       0, 'r'},
        {"wpm",        required_argument, 0, 'w'},
        {"safe-fetch", no_argument,       0, OPT_SAFEFETCH},
        {"max-bytes",  required_argument, 0, OPT_MAXBYTES},
        {"help",       no_argument,       0, 'h'},
        {"version",    no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    int c;
    while ((c = getopt_long(argc, argv, "u:f:Rn:tjrw:hV", lo, NULL)) != -1) {
        switch (c) {
        case 'u': url = optarg; break;
        case 'f': fmt_name = optarg; break;
        case 'R': reader = 1; break;
        case 'n': name = optarg; break;
        case 't': want_text = 1; break;
        case 'j': want_json = 1; break;
        case 'r': want_read = 1; break;
        case 'w': wpm = atoi(optarg); break;
        case OPT_SAFEFETCH: safe_fetch = 1; break;
        case OPT_MAXBYTES: max_bytes = strtoull(optarg, NULL, 10); break;
        case 'h': usage(stdout); return 0;
        case 'V': printf("pika %s\n", PIKA_VERSION); return 0;
        default: usage(stderr); return 2;
        }
    }
    if (wpm < 50)  wpm = 50;
    if (wpm > 2000) wpm = 2000;

    const char *file = optind < argc ? argv[optind] : NULL;

    /* 1. acquire raw bytes */
    strbuf raw; sb_init(&raw);
    char err[256] = {0};
    const char *src_name = file;
    if (url) {
        pika_fetch_opts fo; pika_fetch_opts_default(&fo);
        fo.max_bytes = max_bytes;
        fo.deny_private = safe_fetch;
        if (pika_fetch(url, &fo, &raw, err, sizeof err) != 0) {
            fprintf(stderr, "pika: %s\n", err);
            sb_free(&raw);
            return 1;
        }
        src_name = url;
    } else if (file) {
        if (sb_read_file(&raw, file, max_bytes) != 0) {
            fprintf(stderr, "pika: cannot read '%s'\n", file);
            sb_free(&raw);
            return 1;
        }
    } else {
        if (sb_read_stream(&raw, stdin, max_bytes) < 0) {
            fprintf(stderr, "pika: error reading stdin\n");
            sb_free(&raw);
            return 1;
        }
    }

    /* 2. parse to clean text */
    if (name) src_name = name;   /* explicit hint wins for auto-detection */
    pika_parse_opts po = {0};
    po.fmt = pika_format_from_name(fmt_name);
    po.reader = reader;
    po.src_name = src_name;
    strbuf text; sb_init(&text);
    if (pika_parse(raw.data, raw.len, &po, &text, err, sizeof err) != 0) {
        fprintf(stderr, "pika: %s\n", err);
        sb_free(&raw);
        sb_free(&text);
        return 1;
    }
    sb_free(&raw);

    /* 3. decide what to emit */
    if (!want_json && !want_text && !want_read)
        want_read = isatty(STDOUT_FILENO) && pika_have_tool("pika-tui");
    if (!want_json && !want_text && !want_read)
        want_text = 1;

    int rc = 0;
    if (want_json) {
        pika_tok_opts to; pika_tok_opts_default(&to); to.wpm = wpm;
        pika_tokens toks;
        pika_tokenize(text.data, &to, &toks);
        strbuf js; sb_init(&js);
        pika_json_tokens(&js, &toks, wpm);
        fwrite(js.data, 1, js.len, stdout);
        fputc('\n', stdout);
        sb_free(&js);
        pika_tokens_free(&toks);
    } else if (want_read) {
        char path[1024], wbuf[16];
        if (pika_write_temp(text.data, text.len, path, sizeof path) != 0) {
            fprintf(stderr, "pika: could not stage reader input\n");
            rc = 1;
        } else {
            snprintf(wbuf, sizeof wbuf, "%d", wpm);
            execlp("pika-tui", "pika-tui", "--wpm", wbuf, "--rm", path,
                   (char *)NULL);
            fprintf(stderr, "pika: pika-tui not found; install the TUI "
                            "or use --text/--json\n");
            unlink(path);
            rc = 127;
        }
    } else { /* want_text */
        fwrite(text.data, 1, text.len, stdout);
        if (text.len && text.data[text.len - 1] != '\n') fputc('\n', stdout);
    }

    sb_free(&text);
    return rc;
}
