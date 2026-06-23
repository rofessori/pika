/*
 * proc — run a child process and capture its stdout (no shell, no injection).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PIKA_PROC_H
#define PIKA_PROC_H

#include "pika/strbuf.h"
#include <stddef.h>

/* Returns 1 if `name` is found on PATH and executable. */
int pika_have_tool(const char *name);

/* Write `data` to a fresh temp file; path (incl. NUL) goes in `path`.
 * Returns 0 on success, -1 on error. Caller unlinks the file. */
int pika_write_temp(const char *data, size_t len, char *path, size_t pathsz);

/* Execute argv[] (NULL-terminated), capturing stdout into `out`.
 * Returns the child exit status (0 = success), or -1 if spawn failed. */
int pika_run_capture(const char *const argv[], strbuf *out);

#endif /* PIKA_PROC_H */
