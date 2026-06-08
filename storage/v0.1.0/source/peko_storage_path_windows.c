/*
 * peko_storage_path_win.c
 * Per-app data directory resolver for Windows.
 * Returns a directory under Local AppData, named by the application identity.
 * The directory tree is created when it is absent.
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#pragma comment(lib, "shell32.lib")

/* Converts a wide string to a malloc'd UTF-8 string. Returns NULL on failure. */
static char *wide_to_utf8(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) {
        return NULL;
    }
    char *out = (char *)malloc((size_t)n);
    if (!out) {
        return NULL;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL) <= 0) {
        free(out);
        return NULL;
    }
    return out;
}

/* Resolves the per-app data directory and creates it when needed.
 * app_id selects the directory name. An empty app_id falls back to a fixed
 * default. Returns a malloc'd UTF-8 path or NULL on failure. */
char *peko_storage_data_dir(const char *app_id)
{
    wchar_t base[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                         NULL, 0, base) != S_OK) {
        return NULL;
    }

    wchar_t ident[256];
    int have_ident = 0;
    if (app_id && app_id[0] != '\0') {
        int n = MultiByteToWideChar(CP_UTF8, 0, app_id, -1, ident, 256);
        if (n > 0) {
            have_ident = 1;
        }
    }
    if (!have_ident) {
        wcscpy(ident, L"peko.app");
    }

    wchar_t full[MAX_PATH];
    _snwprintf(full, MAX_PATH, L"%s\\%s", base, ident);
    full[MAX_PATH - 1] = L'\0';

    int rc = SHCreateDirectoryExW(NULL, full, NULL);
    if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_EXISTS) {
        return NULL;
    }

    return wide_to_utf8(full);
}

#endif /* _WIN32 */
