/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pika/json.h"
#include "pika/pika.h"

void pika_json_escape(strbuf *sb, const char *s) {
    if (!s) return;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  sb_puts(sb, "\\\""); break;
        case '\\': sb_puts(sb, "\\\\"); break;
        case '\b': sb_puts(sb, "\\b");  break;
        case '\f': sb_puts(sb, "\\f");  break;
        case '\n': sb_puts(sb, "\\n");  break;
        case '\r': sb_puts(sb, "\\r");  break;
        case '\t': sb_puts(sb, "\\t");  break;
        default:
            if (*p < 0x20) sb_printf(sb, "\\u%04x", *p);
            else           sb_putc(sb, (char)*p);
        }
    }
}

void pika_json_tokens(strbuf *sb, const pika_tokens *t, int wpm) {
    sb_printf(sb, "{\"version\":\"%s\",\"wpm\":%d,\"count\":%zu,\"tokens\":[",
              PIKA_VERSION, wpm, t->count);
    for (size_t i = 0; i < t->count; i++) {
        const pika_token *tok = &t->items[i];
        if (i) sb_putc(sb, ',');
        sb_puts(sb, "{\"t\":\"");
        pika_json_escape(sb, tok->text);
        sb_printf(sb, "\",\"orp\":%d,\"ncp\":%d,\"w\":%.3f}",
                  tok->orp, tok->ncp, tok->weight);
    }
    sb_puts(sb, "]}");
}
