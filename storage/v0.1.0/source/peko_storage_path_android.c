/*
 * peko_storage_path_android.c
 * Per-app data directory resolver for Android.
 * Android does not expose a path through environment variables. The host
 * provides the application files directory once during startup by calling
 * peko_storage_set_files_dir with the value of Context.getFilesDir.
 */

#ifdef __ANDROID__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Diagnostic log macro. Writes to the Android log. Set PEKO_SP_DEBUG to 0 to
 * compile the tracing out. */
#define PEKO_SP_DEBUG 1
#if PEKO_SP_DEBUG
#  include <android/log.h>
#  define PEKO_SP_LOG(...) __android_log_print(ANDROID_LOG_INFO, "peko_storage_path", __VA_ARGS__)
#else
#  define PEKO_SP_LOG(...) ((void)0)
#endif

static char *g_files_dir = NULL;

/* Records the application files directory supplied by the Android host.
 * The host calls this once during startup with the value of
 * Context.getFilesDir. A NULL or empty path clears the stored value. */
void peko_storage_set_files_dir(const char *path)
{
    PEKO_SP_LOG("set_files_dir: path=%s", path ? path : "(null)");
    free(g_files_dir);
    g_files_dir = NULL;
    if (path && path[0] != '\0') {
        size_t n = strlen(path) + 1;
        g_files_dir = (char *)malloc(n);
        if (g_files_dir) {
            memcpy(g_files_dir, path, n);
        }
    }
}

/* Resolves the per-app data directory and creates it when needed.
 * The directory is a storage subdirectory of the host files directory.
 * app_id is ignored because the Android host owns the application identity.
 * Returns a malloc'd path or NULL when the host directory is not set. */
char *peko_storage_data_dir(const char *app_id)
{
    (void)app_id;
    PEKO_SP_LOG("data_dir: enter files_dir=%s", g_files_dir ? g_files_dir : "(null)");
    if (!g_files_dir) {
        PEKO_SP_LOG("data_dir: files_dir not set, returning null");
        return NULL;
    }

    size_t need = strlen(g_files_dir) + strlen("/storage") + 1;
    char *dir = (char *)malloc(need);
    if (!dir) {
        return NULL;
    }
    snprintf(dir, need, "%s/storage", g_files_dir);

    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        PEKO_SP_LOG("data_dir: mkdir failed dir=%s errno=%d", dir, errno);
        free(dir);
        return NULL;
    }
    PEKO_SP_LOG("data_dir: result=%s", dir);
    return dir;
}

#endif /* __ANDROID__ */
