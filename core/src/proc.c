/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pika/proc.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

int pika_have_tool(const char *name) {
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    char buf[4096];
    const char *p = path;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t dlen = colon ? (size_t)(colon - p) : strlen(p);
        if (dlen + 1 + strlen(name) + 1 < sizeof buf) {
            memcpy(buf, p, dlen);
            buf[dlen] = '/';
            strcpy(buf + dlen + 1, name);
            if (access(buf, X_OK) == 0) return 1;
        }
        if (!colon) break;
        p = colon + 1;
    }
    return 0;
}

int pika_write_temp(const char *data, size_t len, char *path, size_t pathsz) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    if ((size_t)snprintf(path, pathsz, "%s/pika-XXXXXX", tmp) >= pathsz)
        return -1;
    int fd = mkstemp(path);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) { close(fd); unlink(path); return -1; }
        off += (size_t)n;
    }
    close(fd);
    return 0;
}

int pika_run_capture(const char *const argv[], strbuf *out) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }

    if (pid == 0) {
        /* child: stdout -> pipe, silence stderr */
        dup2(pipefd[1], STDOUT_FILENO);
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, STDERR_FILENO); close(dn); }
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    close(pipefd[1]);
    char buf[65536];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof buf)) > 0)
        sb_append(out, buf, (size_t)n);
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
