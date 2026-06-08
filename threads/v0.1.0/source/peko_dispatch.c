/*
 * peko_dispatch.c
 * Main-thread dispatch implementation.
 * Each platform uses its native mechanism to post work to the main thread.
 */

#include "peko_threads.h"
#include "include/pgc.h"

typedef struct {
    void      (*worker)(void *);
    pgc_handle  handle;   /* keeps GC context alive across async dispatch */
} peko_dispatch_data_t;

/* =========================================================================
 * macOS / iOS - Grand Central Dispatch
 * ====================================================================== */

#if defined(__APPLE__)
#include <dispatch/dispatch.h>

static void dispatch_trampoline(void *raw)
{
    peko_dispatch_data_t *d = (peko_dispatch_data_t *)raw;
    void *ctx = pgc_handle_get(d->handle);
    d->worker(ctx);
    pgc_handle_release(d->handle);
    free(d);
}

void peko_dispatch_main(void (*worker)(void *), void *data)
{
    peko_dispatch_data_t *d = (peko_dispatch_data_t *)malloc(sizeof(peko_dispatch_data_t));
    if (!d) return;
    d->worker  = worker;
    d->handle  = pgc_handle_create(data);
    dispatch_async_f(dispatch_get_main_queue(), d, dispatch_trampoline);
}

/* =========================================================================
 * Linux (non-Android) - GLib main loop
 * ====================================================================== */

#elif defined(__linux__) && !defined(__ANDROID__)
#include <glib.h>

static gboolean dispatch_trampoline(gpointer raw)
{
    peko_dispatch_data_t *d = (peko_dispatch_data_t *)raw;
    void *ctx = pgc_handle_get(d->handle);
    d->worker(ctx);
    pgc_handle_release(d->handle);
    free(d);
    return G_SOURCE_REMOVE;
}

void peko_dispatch_main(void (*worker)(void *), void *data)
{
    peko_dispatch_data_t *d = (peko_dispatch_data_t *)malloc(sizeof(peko_dispatch_data_t));
    if (!d) return;
    d->worker  = worker;
    d->handle  = pgc_handle_create(data);
    g_idle_add_full(G_PRIORITY_DEFAULT, dispatch_trampoline, d, NULL);
}

/* =========================================================================
 * Windows - hidden message pump window
 * ====================================================================== */

#elif defined(_WIN32)

#define WM_PEKO_DISPATCH (WM_APP + 0x42)

static DWORD g_main_thread_id  = 0;
static HWND  g_dispatch_pump   = NULL;
static LONG  g_pump_init       = 0;

static LRESULT CALLBACK peko_wnd_proc(HWND h, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    if (msg == WM_PEKO_DISPATCH) {
        peko_dispatch_data_t *d = (peko_dispatch_data_t *)lp;
        void *ctx = pgc_handle_get(d->handle);
        d->worker(ctx);
        pgc_handle_release(d->handle);
        free(d);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void ensure_pump(void)
{
    if (InterlockedCompareExchange(&g_pump_init, 1, 0) != 0)
        return;

    WNDCLASSW wc    = {0};
    wc.lpfnWndProc  = peko_wnd_proc;
    wc.hInstance    = GetModuleHandleW(NULL);
    wc.lpszClassName = L"PekoDispatchPump";
    RegisterClassW(&wc);

    g_dispatch_pump = CreateWindowExW(0, L"PekoDispatchPump", L"",
        0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
    g_main_thread_id = GetCurrentThreadId();
}

void peko_dispatch_main(void (*worker)(void *), void *data)
{
    ensure_pump();

    peko_dispatch_data_t *d = (peko_dispatch_data_t *)malloc(sizeof(peko_dispatch_data_t));
    if (!d) return;
    d->worker  = worker;
    d->handle  = pgc_handle_create(data);
    PostMessageW(g_dispatch_pump, WM_PEKO_DISPATCH, 0, (LPARAM)d);
}

/* =========================================================================
 * Android - run synchronously as a stub.
 * A proper implementation would post to the main Looper via JNI.
 * ====================================================================== */

#elif defined(__ANDROID__)

void peko_dispatch_main(void (*worker)(void *), void *data)
{
    /* Pin data via a handle so a GC collection between this call and
     * the worker invocation cannot leave data pointing at moved memory.
     * This matches the behaviour of the other platform implementations
     * which go through pgc_handle_create/pgc_handle_get/pgc_handle_release
     * in their trampolines. */
    pgc_handle h = pgc_handle_create(data);
    void *ctx = pgc_handle_get(h);
    pgc_handle_release(h);
    worker(ctx);
}

#else
#error "peko_dispatch_main: unsupported platform"
#endif
