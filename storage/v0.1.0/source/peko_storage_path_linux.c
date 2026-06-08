/*
 * peko_storage_path_linux.c
 * Per-app data directory resolver for desktop Linux.
 * Returns a directory under the XDG data home, named by the application
 * identity. The directory tree is created when it is absent.
 */

#if defined(__linux__) && !defined(__ANDROID__)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Copies a C string into a fresh malloc buffer. Returns NULL on failure. */
static char *dup_cstr(const char *s)
{
    size_t n = strlen(s) + 1;
    char *c = (char *)malloc(n);
    if (c) {
        memcpy(c, s, n);
    }
    return c;
}

/* Creates each directory along path with the given mode.
 * A directory that already exists is accepted. Returns 0 on success. */
static int make_path(const char *path, mode_t mode)
{
    char *copy = dup_cstr(path);
    if (!copy) {
        return -1;
    }
    for (char *p = copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(copy, mode) != 0 && errno != EEXIST) {
                free(copy);
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(copy, mode) != 0 && errno != EEXIST) {
        free(copy);
        return -1;
    }
    free(copy);
    return 0;
}

/* Resolves the per-app data directory and creates it when needed.
 * The base is XDG_DATA_HOME, or HOME with /.local/share appended when
 * XDG_DATA_HOME is unset. app_id selects the directory name. An empty app_id
 * falls back to a fixed default. Returns a malloc'd path or NULL on failure. */
char *peko_storage_data_dir(const char *app_id)
{
    const char *base = getenv("XDG_DATA_HOME");
    char *owned_base = NULL;

    if (!base || base[0] == '\0') {
        const char *home = getenv("HOME");
        if (!home || home[0] == '\0') {
            return NULL;
        }
        size_t n = strlen(home) + strlen("/.local/share") + 1;
        owned_base = (char *)malloc(n);
        if (!owned_base) {
            return NULL;
        }
        snprintf(owned_base, n, "%s/.local/share", home);
        base = owned_base;
    }

    const char *ident = (app_id && app_id[0] != '\0') ? app_id : "peko-app";

    size_t need = strlen(base) + 1 + strlen(ident) + 1;
    char *dir = (char *)malloc(need);
    if (!dir) {
        free(owned_base);
        return NULL;
    }
    snprintf(dir, need, "%s/%s", base, ident);
    free(owned_base);

    if (make_path(dir, 0700) != 0) {
        free(dir);
        return NULL;
    }
    return dir;
}

#endif /* __linux__ && !__ANDROID__ */
