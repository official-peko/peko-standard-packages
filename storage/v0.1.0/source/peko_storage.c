/*
 * peko_storage.c
 * Local key-value storage backend for Pekoscript.
 * Persists JSON text in a per-app SQLite database. A single serialization
 * mutex guards all database access. Every blocking database call runs inside
 * a pgc_begin_blocking and pgc_end_blocking bracket so the collector can
 * proceed while this thread waits.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "sqlite3.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#endif

/* Diagnostic log macro. Writes to the Android log on Android and to stderr on
 * other platforms. Set PEKO_ST_DEBUG to 0 to compile the tracing out. */
#define PEKO_ST_DEBUG 1
#if PEKO_ST_DEBUG
#  ifdef __ANDROID__
#    include <android/log.h>
#    define PEKO_ST_LOG(...) __android_log_print(ANDROID_LOG_INFO, "peko_storage", __VA_ARGS__)
#  else
#    define PEKO_ST_LOG(...) (fprintf(stderr, "[peko_storage] "), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#  endif
#else
#  define PEKO_ST_LOG(...) ((void)0)
#endif

/* Peko GC interface. These symbols resolve from the pgc runtime object that
 * every Peko program links. pgc_alloc_atomic returns a collector-managed byte
 * buffer with no traced children. pgc_begin_blocking and pgc_end_blocking
 * bracket blocking calls so a collection can run on other threads. */
extern void *pgc_alloc_atomic(size_t size);
extern void  pgc_begin_blocking(void);
extern void  pgc_end_blocking(void);

/* Resolves the per-app data directory, creating it when needed. Implemented
 * per platform in the peko_storage_path object. Returns a malloc'd path or
 * NULL on failure. */
extern char *peko_storage_data_dir(const char *app_id);

/* -------------------------------------------------------------------------
 * Small helpers
 * ---------------------------------------------------------------------- */

/* Copies a C string into a fresh malloc buffer. */
static char *dup_cstr(const char *s)
{
    size_t n = strlen(s) + 1;
    char *c = (char *)malloc(n);
    if (c) {
        memcpy(c, s, n);
    }
    return c;
}

/* -------------------------------------------------------------------------
 * Storage context
 * ---------------------------------------------------------------------- */

typedef struct {
    sqlite3 *db;
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
} peko_storage_t;

static void ctx_lock_init(peko_storage_t *ctx)
{
#ifdef _WIN32
    InitializeCriticalSection(&ctx->lock);
#else
    pthread_mutex_init(&ctx->lock, NULL);
#endif
}

static void ctx_lock_destroy(peko_storage_t *ctx)
{
#ifdef _WIN32
    DeleteCriticalSection(&ctx->lock);
#else
    pthread_mutex_destroy(&ctx->lock);
#endif
}

/* Acquires the serialization lock. The caller is parked through
 * pgc_begin_blocking before calling this. */
static void ctx_lock(peko_storage_t *ctx)
{
#ifdef _WIN32
    EnterCriticalSection(&ctx->lock);
#else
    pthread_mutex_lock(&ctx->lock);
#endif
}

static void ctx_unlock(peko_storage_t *ctx)
{
#ifdef _WIN32
    LeaveCriticalSection(&ctx->lock);
#else
    pthread_mutex_unlock(&ctx->lock);
#endif
}

/* -------------------------------------------------------------------------
 * Schema setup
 * ---------------------------------------------------------------------- */

/* Applies connection pragmas and creates the tables when they are absent.
 * Returns SQLITE_OK on success. */
static int apply_schema(sqlite3 *db)
{
    static const char *setup =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA synchronous=NORMAL;"
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS kv("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL,"
        "updated_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS meta("
        "k TEXT PRIMARY KEY,"
        "v TEXT NOT NULL"
        ");"
        "INSERT OR IGNORE INTO meta(k,v) VALUES('schema_version','1');";

    return sqlite3_exec(db, setup, NULL, NULL, NULL);
}

/* -------------------------------------------------------------------------
 * Open and close
 * ---------------------------------------------------------------------- */

void *peko_storage_open(const char *app_id)
{
    PEKO_ST_LOG("open: enter app_id=%s", app_id ? app_id : "(null)");
    peko_storage_t *ctx = (peko_storage_t *)malloc(sizeof(peko_storage_t));
    if (!ctx) {
        return NULL;
    }
    ctx->db = NULL;
    ctx_lock_init(ctx);

    int ok = 0;

    /* Directory creation and database open are blocking file operations. */
    PEKO_ST_LOG("open: begin_blocking");
    pgc_begin_blocking();

    char *dir = peko_storage_data_dir(app_id);
    if (dir) {
        PEKO_ST_LOG("open: data_dir=%s", dir);
        size_t need = strlen(dir) + strlen("/storage.db") + 1;
        char *path = (char *)malloc(need);
        if (path) {
            snprintf(path, need, "%s/storage.db", dir);

            int rc = sqlite3_open_v2(path, &ctx->db,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                     NULL);
            PEKO_ST_LOG("open: sqlite3_open_v2 rc=%d path=%s", rc, path);
            if (rc == SQLITE_OK) {
                int src = apply_schema(ctx->db);
                PEKO_ST_LOG("open: apply_schema rc=%d", src);
                if (src == SQLITE_OK) {
                    ok = 1;
                }
            }
            free(path);
        }
        free(dir);
    } else {
        PEKO_ST_LOG("open: data_dir null");
    }

    pgc_end_blocking();
    PEKO_ST_LOG("open: end_blocking ok=%d", ok);

    if (!ok) {
        if (ctx->db) {
            sqlite3_close_v2(ctx->db);
        }
        ctx_lock_destroy(ctx);
        free(ctx);
        PEKO_ST_LOG("open: returning null");
        return NULL;
    }

    PEKO_ST_LOG("open: returning handle");
    return ctx;
}

void peko_storage_close(void *handle)
{
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx) {
        return;
    }

    pgc_begin_blocking();
    ctx_lock(ctx);
    if (ctx->db) {
        sqlite3_close_v2(ctx->db);
        ctx->db = NULL;
    }
    ctx_unlock(ctx);
    pgc_end_blocking();

    ctx_lock_destroy(ctx);
    free(ctx);
}

/* -------------------------------------------------------------------------
 * Set
 * ---------------------------------------------------------------------- */

int peko_storage_set(void *handle, const char *key, const char *json_text)
{
    PEKO_ST_LOG("set: enter key=%s", key ? key : "(null)");
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx || !ctx->db || !key || !json_text) {
        return 0;
    }

    /* Copy the inputs out of the collector heap before parking. The arguments
     * reference managed strings. dup_cstr runs without a safepoint, so the
     * source bytes stay in place for the copy. The copies are stable malloc
     * memory, so the statement binds them with SQLITE_STATIC. */
    char *k = dup_cstr(key);
    char *v = dup_cstr(json_text);
    if (!k || !v) {
        free(k);
        free(v);
        return 0;
    }

    int ok = 0;

    pgc_begin_blocking();
    ctx_lock(ctx);

    sqlite3_stmt *stmt = NULL;
    static const char *sql =
        "INSERT INTO kv(key,value,updated_at) VALUES(?,?,?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value, "
        "updated_at=excluded.updated_at;";

    if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, k, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, v, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)time(NULL));
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);

    ctx_unlock(ctx);
    pgc_end_blocking();

    free(k);
    free(v);
    PEKO_ST_LOG("set: return ok=%d", ok);
    return ok;
}

/* -------------------------------------------------------------------------
 * Get
 * ---------------------------------------------------------------------- */

const char *peko_storage_get(void *handle, const char *key)
{
    PEKO_ST_LOG("get: enter key=%s", key ? key : "(null)");
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx || !ctx->db || !key) {
        return NULL;
    }

    char *k = dup_cstr(key);
    if (!k) {
        return NULL;
    }

    char  *found = NULL;
    size_t found_len = 0;

    pgc_begin_blocking();
    ctx_lock(ctx);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(ctx->db, "SELECT value FROM kv WHERE key=?;",
                           -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, k, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void *txt = sqlite3_column_blob(stmt, 0);
            int n = sqlite3_column_bytes(stmt, 0);
            if (n < 0) {
                n = 0;
            }
            found = (char *)malloc((size_t)n + 1);
            if (found) {
                if (n > 0 && txt) {
                    memcpy(found, txt, (size_t)n);
                }
                found[n] = '\0';
                found_len = (size_t)n;
            }
        }
    }
    sqlite3_finalize(stmt);

    ctx_unlock(ctx);
    pgc_end_blocking();

    free(k);

    if (!found) {
        PEKO_ST_LOG("get: return null key=%s", key);
        return NULL;
    }

    /* The blocking bracket has ended, so the collector may run. Allocating the
     * managed return buffer is safe here. */
    char *gc = (char *)pgc_alloc_atomic(found_len + 1);
    if (!gc) {
        free(found);
        return NULL;
    }
    memcpy(gc, found, found_len + 1);
    free(found);
    PEKO_ST_LOG("get: return value len=%zu", found_len);
    return gc;
}

/* -------------------------------------------------------------------------
 * Has
 * ---------------------------------------------------------------------- */

int peko_storage_has(void *handle, const char *key)
{
    PEKO_ST_LOG("has: enter key=%s", key ? key : "(null)");
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx || !ctx->db || !key) {
        return 0;
    }

    char *k = dup_cstr(key);
    if (!k) {
        return 0;
    }

    int found = 0;

    pgc_begin_blocking();
    ctx_lock(ctx);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(ctx->db, "SELECT 1 FROM kv WHERE key=? LIMIT 1;",
                           -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, k, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            found = 1;
        }
    }
    sqlite3_finalize(stmt);

    ctx_unlock(ctx);
    pgc_end_blocking();

    free(k);
    PEKO_ST_LOG("has: return found=%d key=%s", found, key);
    return found;
}

/* -------------------------------------------------------------------------
 * Remove
 * ---------------------------------------------------------------------- */

int peko_storage_remove(void *handle, const char *key)
{
    PEKO_ST_LOG("remove: enter key=%s", key ? key : "(null)");
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx || !ctx->db || !key) {
        return 0;
    }

    char *k = dup_cstr(key);
    if (!k) {
        return 0;
    }

    int ok = 0;

    pgc_begin_blocking();
    ctx_lock(ctx);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(ctx->db, "DELETE FROM kv WHERE key=?;",
                           -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, k, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);

    ctx_unlock(ctx);
    pgc_end_blocking();

    free(k);
    PEKO_ST_LOG("remove: return ok=%d", ok);
    return ok;
}

/* -------------------------------------------------------------------------
 * Clear
 * ---------------------------------------------------------------------- */

int peko_storage_clear(void *handle)
{
    PEKO_ST_LOG("clear: enter");
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx || !ctx->db) {
        return 0;
    }

    int ok = 0;

    pgc_begin_blocking();
    ctx_lock(ctx);
    if (sqlite3_exec(ctx->db, "DELETE FROM kv;", NULL, NULL, NULL) == SQLITE_OK) {
        ok = 1;
    }
    ctx_unlock(ctx);
    pgc_end_blocking();

    PEKO_ST_LOG("clear: return ok=%d", ok);
    return ok;
}

/* -------------------------------------------------------------------------
 * Key listing snapshot
 * ---------------------------------------------------------------------- */

typedef struct {
    char **keys;
    int    count;
} peko_keys_t;

void *peko_storage_keys_open(void *handle)
{
    peko_storage_t *ctx = (peko_storage_t *)handle;
    if (!ctx || !ctx->db) {
        return NULL;
    }

    peko_keys_t *snap = (peko_keys_t *)malloc(sizeof(peko_keys_t));
    if (!snap) {
        return NULL;
    }
    snap->keys = NULL;
    snap->count = 0;

    int cap = 0;
    int failed = 0;

    pgc_begin_blocking();
    ctx_lock(ctx);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(ctx->db, "SELECT key FROM kv ORDER BY key;",
                           -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const void *txt = sqlite3_column_blob(stmt, 0);
            int n = sqlite3_column_bytes(stmt, 0);
            if (n < 0) {
                n = 0;
            }

            char *copy = (char *)malloc((size_t)n + 1);
            if (!copy) {
                failed = 1;
                break;
            }
            if (n > 0 && txt) {
                memcpy(copy, txt, (size_t)n);
            }
            copy[n] = '\0';

            if (snap->count == cap) {
                int next = (cap == 0) ? 16 : cap * 2;
                char **grown = (char **)realloc(snap->keys,
                                                (size_t)next * sizeof(char *));
                if (!grown) {
                    free(copy);
                    failed = 1;
                    break;
                }
                snap->keys = grown;
                cap = next;
            }
            snap->keys[snap->count] = copy;
            snap->count += 1;
        }
    } else {
        failed = 1;
    }
    sqlite3_finalize(stmt);

    ctx_unlock(ctx);
    pgc_end_blocking();

    if (failed) {
        for (int i = 0; i < snap->count; i++) {
            free(snap->keys[i]);
        }
        free(snap->keys);
        free(snap);
        return NULL;
    }
    return snap;
}

int peko_storage_keys_count(void *snapshot)
{
    peko_keys_t *snap = (peko_keys_t *)snapshot;
    return snap ? snap->count : 0;
}

const char *peko_storage_keys_at(void *snapshot, int index)
{
    peko_keys_t *snap = (peko_keys_t *)snapshot;
    if (!snap || index < 0 || index >= snap->count) {
        return NULL;
    }
    return snap->keys[index];
}

void peko_storage_keys_free(void *snapshot)
{
    peko_keys_t *snap = (peko_keys_t *)snapshot;
    if (!snap) {
        return;
    }
    for (int i = 0; i < snap->count; i++) {
        free(snap->keys[i]);
    }
    free(snap->keys);
    free(snap);
}
