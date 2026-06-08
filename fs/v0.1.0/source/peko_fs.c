/*
 * peko_fs.c
 * Filesystem implementation for the Peko fs library.
 * Pure C99, runs on Windows, macOS, Linux, iOS, and Android.
 */

#include "peko_fs.h"

extern void *pgc_alloc_atomic(size_t size);

/* =========================================================================
 * Metadata
 * ====================================================================== */

bool fs_exists(const char *fpath)
{
    struct stat buf;
    return stat(fpath, &buf) == 0;
}

int fs_get_mode(const char *fpath)
{
    struct stat buf;
    if (stat(fpath, &buf) != 0)
        return -1;
    return (int)buf.st_mode;
}

bool fs_is_directory(const char *fpath)
{
    struct stat buf;
    if (stat(fpath, &buf) != 0)
        return false;
    return S_ISDIR(buf.st_mode);
}

bool fs_is_regular(const char *fpath)
{
    struct stat buf;
    if (stat(fpath, &buf) != 0)
        return false;
    return S_ISREG(buf.st_mode);
}

bool fs_is_link(const char *fpath)
{
    struct stat buf;
    /* lstat does not follow the final symlink in the path. */
#ifdef _WIN32
    if (stat(fpath, &buf) != 0)
        return false;
#else
    if (lstat(fpath, &buf) != 0)
        return false;
#endif
    return S_ISLNK(buf.st_mode);
}

bool fs_is_block(const char *fpath)
{
    struct stat buf;
    if (stat(fpath, &buf) != 0)
        return false;
#ifdef S_ISBLK
    return S_ISBLK(buf.st_mode);
#else
    return false; /* Windows has no block devices. */
#endif
}

bool fs_chmod(const char *fpath, int mode)
{
#ifndef _WIN32
    return chmod(fpath, (mode_t)mode) == 0;
#else
    /* Windows _chmod understands only _S_IREAD and _S_IWRITE.
     * Map the Unix mode bits down to those two flags. */
    int win_mode = 0;
    if (mode & 0444) win_mode |= _S_IREAD;
    if (mode & 0222) win_mode |= _S_IWRITE;
    return _chmod(fpath, win_mode) == 0;
#endif
}

/* =========================================================================
 * File handle open / close
 * ====================================================================== */

void *fs_open_handle(const char *fpath, int mode)
{
    const char *m = fs_mode_string(mode);
    return (void *)fs_fopen(fpath, m);
}

void fs_close_handle(void *handle)
{
    if (handle)
        fclose((FILE *)handle);
}

/* =========================================================================
 * Read operations
 * ====================================================================== */

void *fs_read_bytes(void *handle, int n, int *out_len)
{
    FILE  *fp  = (FILE *)handle;
    void  *buf = pgc_alloc_atomic((size_t)(n + 1));

    if (!buf)
        return NULL;

    int bytes_read = (int)fread(buf, 1, (size_t)n, fp);
    if (bytes_read <= 0) {
        /* The GC reclaims atomic allocations, so nothing to free here. */
        return NULL;
    }

    /* Null-terminate so the buffer is also safe to use as a string. */
    ((char *)buf)[bytes_read] = '\0';
    *out_len = bytes_read;
    return buf;
}

char *fs_read_string(void *handle, int n)
{
    int   len = 0;
    void *buf = fs_read_bytes(handle, n, &len);
    return (char *)buf;
}

char *fs_read_all_string(void *handle)
{
    FILE *fp = (FILE *)handle;

    /* Use fseek and ftell to get the size without reading twice. */
    long size = fs_file_size(fp);

    if (size >= 0) {
        /* Fast path: size is known, so read in one call. */
        char *buf = (char *)pgc_alloc_atomic((size_t)(size + 1));
        if (!buf)
            return NULL;

        int bytes_read = (int)fread(buf, 1, (size_t)size, fp);
        buf[bytes_read] = '\0';
        return buf;
    }

    /* Slow path: size is unknown, such as pipes, so grow the buffer. */
    {
        size_t   capacity = PEKO_FS_READ_INITIAL_SIZE;
        size_t   length   = 0;
        char    *buf      = (char *)malloc(capacity);

        if (!buf)
            return NULL;

        for (;;) {
            if (length + PEKO_FS_READ_CHUNK + 1 > capacity) {
                capacity *= 2;
                char *tmp = (char *)realloc(buf, capacity);
                if (!tmp) {
                    free(buf);
                    return NULL;
                }
                buf = tmp;
            }

            size_t n = fread(buf + length, 1, PEKO_FS_READ_CHUNK, fp);
            length += n;
            if (n < PEKO_FS_READ_CHUNK)
                break;
        }

        buf[length] = '\0';

        /* Copy into GC-managed memory. */
        char *gc_buf = (char *)pgc_alloc_atomic((size_t)(length + 1));
        if (!gc_buf) {
            free(buf);
            return NULL;
        }
        memcpy(gc_buf, buf, length + 1);
        free(buf);
        return gc_buf;
    }
}

void *fs_read_all_bytes(void *handle, int *out_len)
{
    FILE *fp   = (FILE *)handle;
    long  size = fs_file_size(fp);

    if (size >= 0) {
        /* Fast path: size is known. */
        char *buf = (char *)pgc_alloc_atomic((size_t)(size + 1));
        if (!buf)
            return NULL;

        int bytes_read = (int)fread(buf, 1, (size_t)size, fp);
        buf[bytes_read] = '\0';
        *out_len  = bytes_read;
        return buf;
    }

    /* Slow path: size is unknown, so grow the buffer. */
    {
        size_t   capacity = PEKO_FS_READ_INITIAL_SIZE;
        size_t   length   = 0;
        char    *buf      = (char *)malloc(capacity);

        if (!buf)
            return NULL;

        for (;;) {
            if (length + PEKO_FS_READ_CHUNK + 1 > capacity) {
                capacity *= 2;
                char *tmp = (char *)realloc(buf, capacity);
                if (!tmp) {
                    free(buf);
                    return NULL;
                }
                buf = tmp;
            }

            size_t n = fread(buf + length, 1, PEKO_FS_READ_CHUNK, fp);
            length += n;
            if (n < PEKO_FS_READ_CHUNK)
                break;
        }

        char *gc_buf = (char *)pgc_alloc_atomic((size_t)(length + 1));
        if (!gc_buf) {
            free(buf);
            return NULL;
        }
        memcpy(gc_buf, buf, length + 1);
        free(buf);
        *out_len = (int)length;
        return gc_buf;
    }
}

/* =========================================================================
 * Write operations
 * ====================================================================== */

int fs_write_bytes(void *handle, const void *buf, int n)
{
    size_t written = fwrite(buf, 1, (size_t)n, (FILE *)handle);
    return (written == (size_t)n) ? (int)written : -1;
}

int fs_write_string(void *handle, const char *text)
{
    int len    = (int)strlen(text);
    int result = fputs(text, (FILE *)handle);
    return (result >= 0) ? len : -1;
}

/* =========================================================================
 * Seek / tell / flush
 * ====================================================================== */

int fs_seek(void *handle, long offset, int origin)
{
    return fseek((FILE *)handle, offset, origin) == 0 ? 0 : -1;
}

long fs_tell(void *handle)
{
    return ftell((FILE *)handle);
}

int fs_flush(void *handle)
{
    return fflush((FILE *)handle) == 0 ? 0 : -1;
}

/* =========================================================================
 * Filesystem operations
 * ====================================================================== */

bool fs_mkdir(const char *dirpath)
{
#ifndef _WIN32
    return mkdir(dirpath, 0777) == 0;
#else
    return CreateDirectoryA(dirpath, NULL) != 0;
#endif
}

bool fs_remove(const char *fpath)
{
    return remove(fpath) == 0;
}

bool fs_copy(const char *src, const char *dst)
{
    FILE  *in  = fs_fopen(src, "rb");
    FILE  *out = fs_fopen(dst, "wb");
    char   chunk[PEKO_FS_READ_CHUNK];
    size_t n;
    bool   ok  = true;

    if (!in || !out) {
        if (in)  fclose(in);
        if (out) fclose(out);
        return false;
    }

    while ((n = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        if (fwrite(chunk, 1, n, out) != n) {
            ok = false;
            break;
        }
    }

    fclose(in);
    fclose(out);
    return ok;
}

bool fs_move(const char *src, const char *dst)
{
    /* rename works across directories on the same volume. */
    if (rename(src, dst) == 0)
        return true;

    /* Cross-volume fallback: copy, then remove the source. */
    if (!fs_copy(src, dst))
        return false;

    return fs_remove(src);
}

/* =========================================================================
 * Directory listing
 * ====================================================================== */

int fs_child_count(const char *dirpath)
{
#ifndef _WIN32
    DIR           *dr = opendir(dirpath);
    struct dirent *de;
    int            count = 0;

    if (!dr)
        return 0;

    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
            count++;
    }

    closedir(dr);
    return count;
#else
    WIN32_FIND_DATAA fd;
    HANDLE           h;
    char             pattern[MAX_PATH];
    int              count = 0;

    snprintf(pattern, sizeof(pattern), "%s\\*", dirpath);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    do {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
            count++;
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return count;
#endif
}

char **fs_list_children(const char *dirpath)
{
    int    count = fs_child_count(dirpath);

    /* Use malloc here. This array goes back to Peko as a raw string[] that
     * the GC does not trace, and the GC cannot update pointers inside an
     * atomic allocation, so the array and each name must stay put. Peko's
     * cstr to string cast copies each name into GC memory right away, so the
     * malloc strings are only needed for a moment. */
    char **list  = (char **)malloc(sizeof(char *) * (size_t)(count + 1));

    if (!list)
        return NULL;

    list[count] = NULL; /* Null-terminate the array. */

#ifndef _WIN32
    DIR           *dr = opendir(dirpath);
    struct dirent *de;
    int            i  = 0;

    if (!dr)
        return list;

    while ((de = readdir(dr)) != NULL && i < count) {
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
            size_t len  = strlen(de->d_name);
            char  *name = (char *)malloc(len + 1);
            if (name) {
                memcpy(name, de->d_name, len + 1);
                list[i++] = name;
            }
        }
    }

    closedir(dr);
#else
    WIN32_FIND_DATAA fd;
    HANDLE           h;
    char             pattern[MAX_PATH];
    int              i = 0;

    snprintf(pattern, sizeof(pattern), "%s\\*", dirpath);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return list;

    do {
        if (strcmp(fd.cFileName, ".") != 0 &&
            strcmp(fd.cFileName, "..") != 0 && i < count) {
            size_t len  = strlen(fd.cFileName);
            char  *name = (char *)malloc(len + 1);
            if (name) {
                memcpy(name, fd.cFileName, len + 1);
                list[i++] = name;
            }
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
#endif

    return list;
}

/* -------------------------------------------------------------------------
 * walk: internal recursive helper
 * ---------------------------------------------------------------------- */

typedef struct {
    char  **paths;
    int     count;
    int     capacity;
} walk_result_t;

static void walk_result_push(walk_result_t *r, const char *path)
{
    if (r->count >= r->capacity) {
        int    next = r->capacity * 2;
        char **tmp  = (char **)realloc(r->paths,
                                       sizeof(char *) * (size_t)next);
        if (!tmp)
            return;
        r->paths    = tmp;
        r->capacity = next;
    }
    size_t len  = strlen(path);
    /* Use malloc. Peko's cstr to string cast consumes this right away. */
    char  *copy = (char *)malloc(len + 1);
    if (!copy)
        return;
    memcpy(copy, path, len + 1);
    r->paths[r->count++] = copy;
}

static void walk_recursive(const char *dirpath, int max_depth,
                            int current_depth, walk_result_t *result)
{
    if (max_depth >= 0 && current_depth > max_depth)
        return;

    char **children = fs_list_children(dirpath);
    if (!children)
        return;

    for (int i = 0; children[i] != NULL; i++) {
        char child_path[4096];
        snprintf(child_path, sizeof(child_path), "%s/%s",
                 dirpath, children[i]);

        walk_result_push(result, child_path);

        if (fs_is_directory(child_path))
            walk_recursive(child_path, max_depth,
                           current_depth + 1, result);
    }
}

char **fs_walk(const char *dirpath, int max_depth, int *out_count)
{
    walk_result_t result;
    result.capacity = 64;
    result.count    = 0;
    result.paths    = (char **)malloc(sizeof(char *) * (size_t)result.capacity);

    if (!result.paths) {
        *out_count = 0;
        return NULL;
    }

    walk_recursive(dirpath, max_depth, 0, &result);

    /* Use malloc, for the same reason as fs_list_children. The array holds
     * raw cstr pointers, and Peko copies each into GC memory right away
     * through the cstr to string cast, so atomic allocation is not needed. */
    char **final_list = (char **)malloc(sizeof(char *) * (size_t)(result.count + 1));
    if (final_list) {
        memcpy(final_list, result.paths,
               sizeof(char *) * (size_t)result.count);
        final_list[result.count] = NULL;
    }

    free(result.paths);
    *out_count = result.count;
    return final_list;
}

/* =========================================================================
 * Convenience helpers
 * ====================================================================== */

char *fs_helper_read_file(const char *fpath)
{
    FILE *fp = fs_fopen(fpath, "r");
    if (!fp)
        return NULL;

    char *result = fs_read_all_string((void *)fp);
    fclose(fp);
    return result;
}

bool fs_helper_write_file(const char *fpath, const char *text)
{
    FILE *fp = fs_fopen(fpath, "w");
    if (!fp)
        return false;

    int rc = fs_write_string((void *)fp, text);
    fclose(fp);
    return rc >= 0;
}

bool fs_helper_append_file(const char *fpath, const char *text)
{
    FILE *fp = fs_fopen(fpath, "a");
    if (!fp)
        return false;

    int rc = fs_write_string((void *)fp, text);
    fclose(fp);
    return rc >= 0;
}

void *fs_helper_read_bytes(const char *fpath, int *out_len)
{
    FILE *fp = fs_fopen(fpath, "rb");
    if (!fp) {
        *out_len = 0;
        return NULL;
    }

    void *result = fs_read_all_bytes((void *)fp, out_len);
    fclose(fp);
    return result;
}

bool fs_helper_copy(const char *src, const char *dst)
{
    return fs_copy(src, dst);
}

bool fs_helper_move(const char *src, const char *dst)
{
    return fs_move(src, dst);
}

/* =========================================================================
 * Buffered comparison
 * ====================================================================== */

bool fs_content_changed(const char *fpath, const char *snapshot,
                        int snapshot_len)
{
    FILE *fp = fs_fopen(fpath, "r");
    if (!fp)
        return true; /* File is gone, so treat it as changed. */

    int  i    = 0;
    int  ch;
    bool same = true;

    while ((ch = fgetc(fp)) != EOF) {
        if (i >= snapshot_len || ch != (unsigned char)snapshot[i]) {
            same = false;
            break;
        }
        i++;
    }

    /* The file is shorter or longer than the snapshot. */
    if (same && i != snapshot_len)
        same = false;

    fclose(fp);

    /* Return true if the content changed, false if it matches. */
    return !same;
}
