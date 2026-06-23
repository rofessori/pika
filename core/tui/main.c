/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * pika-tui — a terminal RSVP reader built on libpika.
 *
 * Words are shown one at a time, each aligned so its Optimal Recognition
 * Point sits on a fixed focal column. The eye never moves, so reading speed
 * is bounded by recognition rather than saccades. Controls let the reader
 * pick a comfortable words-per-minute.
 *
 *   Space  play / pause        +/-   words-per-minute -/+ 10
 *   ←  →   step word (paused)   ↑/↓   words-per-minute +/- 25
 *   r      restart             q     quit
 */
#include "pika/pika.h"

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>

static int g_tty = -1;            /* terminal fd for IO          */
static struct termios g_saved;    /* original terminal settings  */
static int g_raw = 0;

/* ---- terminal control ---------------------------------------------------- */

static void out(const char *s) { (void)!write(g_tty, s, strlen(s)); }

static void restore_term(void) {
    if (g_raw) {
        tcsetattr(g_tty, TCSANOW, &g_saved);
        g_raw = 0;
    }
    out("\x1b[?25h\x1b[0m");     /* show cursor, reset attributes */
}

static void on_exit_cleanup(void) { restore_term(); }

static int raw_mode(void) {
    if (tcgetattr(g_tty, &g_saved) != 0) return -1;
    struct termios t = g_saved;
    t.c_lflag &= ~(unsigned)(ICANON | ECHO | ISIG);
    t.c_iflag &= ~(unsigned)(IXON | ICRNL);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(g_tty, TCSANOW, &t) != 0) return -1;
    g_raw = 1;
    out("\x1b[?25l");           /* hide cursor */
    return 0;
}

static void term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(g_tty, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *rows = ws.ws_row; *cols = ws.ws_col;
    } else {
        *rows = 24; *cols = 80;
    }
}

/* ---- utf-8 ---------------------------------------------------------------- */

static int seqlen(unsigned char b) {
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;
}
/* byte offset of codepoint index `cp` in s */
static int cp_off(const char *s, int cp) {
    const char *p = s;
    while (cp-- > 0 && *p) p += seqlen((unsigned char)*p);
    return (int)(p - s);
}

/* ---- rendering ----------------------------------------------------------- */

#define C_RESET "\x1b[0m"
#define C_DIM   "\x1b[2m"
#define C_RED   "\x1b[1;38;5;160m"
#define C_GUIDE "\x1b[38;5;238m"

static void draw(const pika_token *tok, int idx, int total, int wpm,
                 int paused) {
    int rows, cols;
    term_size(&rows, &cols);
    int focal = cols / 2 - 2;
    if (focal < 1) focal = 1;
    int mid = rows / 2;

    strbuf b; sb_init(&b);
    sb_puts(&b, "\x1b[2J\x1b[H");        /* clear + home */

    /* focal guide ticks above and below, like a sighting reticle */
    sb_printf(&b, "\x1b[%d;%dH%s\xe2\x94\x82%s", mid - 1, focal + 1,
              C_GUIDE, C_RESET);
    sb_printf(&b, "\x1b[%d;%dH%s\xe2\x94\x82%s", mid + 1, focal + 1,
              C_GUIDE, C_RESET);

    /* the word, ORP aligned to the focal column */
    const char *w = tok ? tok->text : "";
    int orp = tok ? tok->orp : 0;
    int a = cp_off(w, orp);                 /* bytes before pivot   */
    int b2 = cp_off(w, orp + 1);            /* bytes incl. pivot    */
    int col = focal - orp;
    if (col < 1) col = 1;
    sb_printf(&b, "\x1b[%d;%dH", mid, col);
    sb_append(&b, w, (size_t)a);                       /* head        */
    sb_puts(&b, C_RED);
    sb_append(&b, w + a, (size_t)(b2 - a));            /* pivot (red) */
    sb_puts(&b, C_RESET);
    sb_puts(&b, w + b2);                               /* tail        */

    /* status bar */
    int pct = total > 1 ? (int)((long)idx * 100 / (total - 1)) : 100;
    sb_printf(&b, "\x1b[%d;1H%s%s  %d wpm  \xc2\xb7  %d/%d  \xc2\xb7  %d%%"
                  "  \xc2\xb7  space play  q quit%s",
              rows, C_DIM, paused ? "PAUSED " : "\xe2\x96\xb6 PLAY ",
              wpm, idx + 1, total, pct, C_RESET);

    (void)!write(g_tty, b.data, b.len);
    sb_free(&b);
}

/* ---- input --------------------------------------------------------------- */

enum { K_NONE = 0, K_QUIT, K_SPACE, K_LEFT, K_RIGHT, K_UP, K_DOWN,
       K_FASTER, K_SLOWER, K_RESTART };

/* Read one logical key (non-blocking). Returns K_NONE if nothing pending. */
static int read_key(void) {
    unsigned char c;
    ssize_t n = read(g_tty, &c, 1);
    if (n <= 0) return K_NONE;
    switch (c) {
    case 'q': case 'Q': case 3 /*^C*/: return K_QUIT;
    case ' ': return K_SPACE;
    case 'r': case 'R': return K_RESTART;
    case '+': case '=': return K_FASTER;
    case '-': case '_': return K_SLOWER;
    case 'l': return K_RIGHT;
    case 'h': return K_LEFT;
    case 'k': return K_UP;
    case 'j': return K_DOWN;
    case 0x1b: {
        unsigned char seq[2];
        if (read(g_tty, &seq[0], 1) != 1) return K_NONE;
        if (seq[0] != '[' && seq[0] != 'O') return K_NONE;
        if (read(g_tty, &seq[1], 1) != 1) return K_NONE;
        switch (seq[1]) {
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        }
        return K_NONE;
    }
    }
    return K_NONE;
}

static int clamp_wpm(int w) {
    if (w < 50) return 50;
    if (w > 1500) return 1500;
    return w;
}

/* ---- main ---------------------------------------------------------------- */

static void usage(FILE *f) {
    fputs("Usage: pika-tui [--wpm N] [--rm] [FILE]\n"
          "Read FILE (or stdin) one word at a time on a focal point.\n",
          f);
}

int main(int argc, char **argv) {
    int wpm = 350, rm = 0;
    static struct option lo[] = {
        {"wpm", required_argument, 0, 'w'},
        {"rm",  no_argument,       0, 1000},
        {"help",no_argument,       0, 'h'},
        {0,0,0,0}
    };
    int c;
    while ((c = getopt_long(argc, argv, "w:h", lo, NULL)) != -1) {
        switch (c) {
        case 'w': wpm = clamp_wpm(atoi(optarg)); break;
        case 1000: rm = 1; break;
        case 'h': usage(stdout); return 0;
        default: usage(stderr); return 2;
        }
    }
    const char *file = optind < argc ? argv[optind] : NULL;

    /* load text */
    strbuf text; sb_init(&text);
    if (file) {
        if (sb_read_file(&text, file, 16u * 1024 * 1024) != 0) {
            fprintf(stderr, "pika-tui: cannot read '%s'\n", file);
            return 1;
        }
        if (rm) unlink(file);
    } else {
        if (isatty(STDIN_FILENO)) {
            fprintf(stderr, "pika-tui: no input (give a FILE or pipe text in)\n");
            return 1;
        }
        sb_read_stream(&text, stdin, 16u * 1024 * 1024);
    }

    pika_tok_opts to; pika_tok_opts_default(&to); to.wpm = wpm;
    pika_tokens toks;
    pika_tokenize(text.data, &to, &toks);
    sb_free(&text);
    if (toks.count == 0) {
        fprintf(stderr, "pika-tui: nothing to read\n");
        return 1;
    }

    g_tty = open("/dev/tty", O_RDWR);
    if (g_tty < 0) g_tty = STDOUT_FILENO;
    if (raw_mode() != 0) {
        fprintf(stderr, "pika-tui: not a terminal\n");
        pika_tokens_free(&toks);
        return 1;
    }
    atexit(on_exit_cleanup);

    int idx = 0, paused = 0, quit = 0;
    while (!quit) {
        pika_token *tok = &toks.items[idx];
        draw(tok, idx, (int)toks.count, wpm, paused);

        int wait_ms = paused ? 1000
                             : (int)pika_token_delay_ms(tok, wpm);
        struct pollfd pfd = { g_tty, POLLIN, 0 };
        int advance = !paused;

        int pr = poll(&pfd, 1, wait_ms);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            int k;
            while ((k = read_key()) != K_NONE) {
                switch (k) {
                case K_QUIT:   quit = 1; break;
                case K_SPACE:  paused = !paused; advance = 0; break;
                case K_RESTART:idx = 0; advance = 0; paused = 1; break;
                case K_FASTER: wpm = clamp_wpm(wpm + 10); to.wpm = wpm;
                               advance = 0; break;
                case K_SLOWER: wpm = clamp_wpm(wpm - 10); to.wpm = wpm;
                               advance = 0; break;
                case K_UP:     wpm = clamp_wpm(wpm + 25); to.wpm = wpm;
                               advance = 0; break;
                case K_DOWN:   wpm = clamp_wpm(wpm - 25); to.wpm = wpm;
                               advance = 0; break;
                case K_LEFT:   if (idx > 0) idx--; paused = 1; advance = 0;
                               break;
                case K_RIGHT:  if (idx + 1 < (int)toks.count) idx++;
                               paused = 1; advance = 0; break;
                }
            }
        }
        if (quit) break;
        if (advance) {
            if (idx + 1 < (int)toks.count) idx++;
            else paused = 1;        /* hold on the final word */
        }
    }

    restore_term();
    out("\x1b[2J\x1b[H");
    pika_tokens_free(&toks);
    return 0;
}
