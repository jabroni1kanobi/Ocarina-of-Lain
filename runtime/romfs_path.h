// Contains code Copyright (c) 2026 MacNativeGames contributors (MIT; see THIRD_PARTY_NOTICES.md).
// RomFS path validation shared by the runtime (romfs.c) and its probe (fs_host.c probe_fs).
#pragma once
#include <stddef.h>
#include <string.h>

// A RomFS-relative path: non-empty, no leading '/', no '\', no empty, "." or ".." component.
static inline int romfs_path_valid_relative(const char *path) {
    if (!path || !*path || *path == '/' || strchr(path, '\\')) return 0;
    for (const char *part = path; *part;) {
        const char *end = strchr(part, '/');
        size_t size = end ? (size_t)(end - part) : strlen(part);
        if (!size || (size == 1 && part[0] == '.') || (size == 2 && part[0] == '.' && part[1] == '.')) return 0;
        if (!end) break;
        part = end + 1;
    }
    return 1;
}

// Copies path into out (cap bytes) with each run of '/' after the first ':' folded into one, as a file system
// reads "a//b". Switch 2 voice requests join "Voice/Resource/" and "/Stream/..." (Ocarina's
// "content://Voice/Resource//Stream/USen_...bwav"). 0 when it does not fit.
static inline int romfs_path_collapse(const char *path, char *out, size_t cap) {
    const char *colon = path ? strchr(path, ':') : NULL;
    size_t n = 0;
    for (const char *p = path; p && *p; p++) {
        if (colon && p > colon + 1 && *p == '/' && p[-1] == '/') continue;   // keep "<mount>:/" and one more
        if (n + 1 >= cap) return 0;
        out[n++] = *p;
    }
    if (!cap) return 0;
    out[n] = 0;
    return 1;
}

// "<mount>:/<path>" -> "<path>", or NULL. Audio stream requests join a mount root and a leading-slash resource,
// producing "content://Sound/...": repeated root separators are skipped; traversal, backslashes and empty interior
// components are still rejected.
static inline const char *romfs_path_relative(const char *mount, const char *path) {
    if (!mount || !*mount || !path) return NULL;
    size_t size = strlen(mount);
    if (strncmp(path, mount, size) || path[size] != ':' || path[size + 1] != '/') return NULL;
    path += size + 2;
    while (*path == '/') ++path;
    return romfs_path_valid_relative(path) ? path : NULL;
}
