/*
 * peko_fs.h
 * Types, constants, and function declarations for the Peko filesystem
 * library. Include this header in peko_fs.c only.
 */

#ifndef PEKO_FS_H
#define PEKO_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <fileapi.h>
#  include <sys/stat.h>
#  include <io.h>
/* MSVC sys/stat.h has no POSIX S_IS* macros, so define them here. */
#  ifndef S_ISDIR
#    define S_ISDIR(m)  (((m) & _S_IFMT) == _S_IFDIR)
#  endif
#  ifndef S_ISREG
#    define S_ISREG(m)  (((m) & _S_IFMT) == _S_IFREG)
#  endif
#  ifndef S_ISLNK
#    define S_ISLNK(m)  (0)
#  endif
#  ifndef S_ISBLK
#    define S_ISBLK(m)  (0)
#  endif
#else
#  include <dirent.h>
#  include <unistd.h>
#endif

/* -------------------------------------------------------------------------
 * Buffer constants
 * ---------------------------------------------------------------------- */

/* Default buffer size for the buffered reader and writer (8 KB). */
#define PEKO_FS_DEFAULT_BUF_SIZE  8192

/* Initial capacity for dynamic read buffers. */
#define PEKO_FS_READ_INITIAL_SIZE 4096

/* Read chunk size when reading in a loop. */
#define PEKO_FS_READ_CHUNK        4096

/* -------------------------------------------------------------------------
 * Open mode flags. These match the OpenMode module values in main.peko.
 * ---------------------------------------------------------------------- */

#define PEKO_MODE_READ       1
#define PEKO_MODE_WRITE      2
#define PEKO_MODE_APPEND     4
#define PEKO_MODE_BINARY     8
#define PEKO_MODE_READ_WRITE 16

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/*
 * Converts a Peko mode bitmask to a C fopen mode string.
 * Returns a pointer to a static string. Do not free it.
 */
static const char *fs_mode_string(int mode)
{
    int rw  = (mode & PEKO_MODE_READ_WRITE) != 0;
    int rd  = (mode & PEKO_MODE_READ)       != 0;
    int wr  = (mode & PEKO_MODE_WRITE)      != 0;
    int ap  = (mode & PEKO_MODE_APPEND)     != 0;
    int bin = (mode & PEKO_MODE_BINARY)     != 0;

    if (rw)  return bin ? "r+b" : "r+";
    if (ap)  return bin ? "ab"  : "a";
    if (wr)  return bin ? "wb"  : "w";
    if (rd)  return bin ? "rb"  : "r";
    return "r";
}

/*
 * Returns the size in bytes of an open FILE, or -1 on error.
 * Restores the original file position before returning.
 */
static long fs_file_size(FILE *fp)
{
    long original = ftell(fp);
    if (original < 0)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0)
        return -1;
    long size = ftell(fp);
    fseek(fp, original, SEEK_SET);
    return size;
}

/*
 * Cross-platform fopen wrapper.
 * Returns NULL on failure.
 */
static FILE *fs_fopen(const char *path, const char *mode)
{
#ifdef _WIN32
    FILE *fp = NULL;
    fopen_s(&fp, path, mode);
    return fp;
#else
    return fopen(path, mode);
#endif
}

/* -------------------------------------------------------------------------
 * Function declarations
 * ---------------------------------------------------------------------- */

/* --- Metadata --- */
bool fs_exists(const char *fpath);
int  fs_get_mode(const char *fpath);
bool fs_is_directory(const char *fpath);
bool fs_is_regular(const char *fpath);
bool fs_is_link(const char *fpath);
bool fs_is_block(const char *fpath);
bool fs_chmod(const char *fpath, int mode);

/* --- Basic I/O --- */

/*
 * Opens the file at fpath with the given mode bitmask.
 * Returns the FILE* cast to a void*, or NULL on failure.
 * The caller is responsible for closing via fs_close_handle.
 */
void *fs_open_handle(const char *fpath, int mode);

/*
 * Closes a file handle previously opened with fs_open_handle.
 */
void fs_close_handle(void *handle);

/*
 * Reads up to n bytes from handle into a GC-managed buffer.
 * Writes the number of bytes actually read to *out_len.
 * Returns NULL on failure or EOF.
 */
void *fs_read_bytes(void *handle, int n, int *out_len);

/*
 * Reads up to n bytes from handle as a null-terminated GC string.
 * Returns NULL on failure or EOF.
 */
char *fs_read_string(void *handle, int n);

/*
 * Reads the entire remaining contents of handle as a null-terminated
 * GC string. Returns NULL on failure.
 */
char *fs_read_all_string(void *handle);

/*
 * Reads the entire remaining contents of handle into a GC-managed
 * byte buffer. Writes the byte count to *out_len.
 * Returns NULL on failure.
 */
void *fs_read_all_bytes(void *handle, int *out_len);

/*
 * Writes n bytes from buf to handle.
 * Returns the number of bytes written, or -1 on error.
 */
int fs_write_bytes(void *handle, const void *buf, int n);

/*
 * Writes a null-terminated string to handle.
 * Returns the number of bytes written, or -1 on error.
 */
int fs_write_string(void *handle, const char *text);

/*
 * Seeks to offset bytes from origin (SEEK_SET=0, SEEK_CUR=1, SEEK_END=2).
 * Returns 0 on success, -1 on error.
 */
int fs_seek(void *handle, long offset, int origin);

/*
 * Returns the current byte offset within the file, or -1 on error.
 */
long fs_tell(void *handle);

/*
 * Flushes any buffered writes to the OS.
 * Returns 0 on success, -1 on error.
 */
int fs_flush(void *handle);

/* --- Filesystem operations --- */
bool  fs_mkdir(const char *dirpath);
bool  fs_remove(const char *fpath);
bool  fs_copy(const char *src, const char *dst);
bool  fs_move(const char *src, const char *dst);

/* --- Directory listing --- */
int    fs_child_count(const char *dirpath);
char **fs_list_children(const char *dirpath);
char **fs_walk(const char *dirpath, int max_depth, int *out_count);

/* --- Convenience helpers (open, operate, close in one call) --- */
char *fs_helper_read_file(const char *fpath);
bool  fs_helper_write_file(const char *fpath, const char *text);
bool  fs_helper_append_file(const char *fpath, const char *text);
void *fs_helper_read_bytes(const char *fpath, int *out_len);
bool  fs_helper_copy(const char *src, const char *dst);
bool  fs_helper_move(const char *src, const char *dst);

/* --- Buffered comparison --- */

/*
 * Compares the file at fpath against snapshot (length snapshot_len) as it
 * reads, without loading the whole file into memory.
 * Returns true if the file contents differ from the snapshot.
 */
bool fs_content_changed(const char *fpath, const char *snapshot,
                        int snapshot_len);

#endif /* PEKO_FS_H */
