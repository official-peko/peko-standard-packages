# ffi

Common C FFI bindings for Pekoscript.

## Overview

The `ffi` package declares a small set of standard C library functions so they can be called from Pekoscript. Every function in this package lives under the `extern` module, so each is called with the `extern::` prefix.

The bindings are thin. They map directly onto the underlying C function with no extra logic. Some functions are platform specific and are selected at build time.

## Import

```
import ffi@"v0.1.0";
```

After import, the functions are reached through `extern::`, for example `extern::malloc(64)`.

## Functions

```
[external variadic] fn sprintf(src: cstr, fmt: cstr) => int
[external]          fn strcmp(str1: cstr, str2: cstr) => int
[external]          fn fprintf(file: opaque, str: cstr) => int
[external variadic] fn fscanf(file: opaque, fmt: cstr) => int64
[external]          fn getchar() => char
[external]          fn fopen(path: cstr, func: cstr) => opaque
[external]          fn fclose(file: opaque)
[external]          fn fgetc(file: opaque) => char
[external]          fn mkdir(dir: cstr) => int64
[external]          fn exit(code: int)
[external]          fn sleep(ms: int) => int
[external]          fn malloc(size: int64) => opaque
```

`sprintf` and `fscanf` are variadic and accept extra arguments after the format string. `fopen` returns a file handle as `opaque`, which is passed to `fprintf`, `fscanf`, `fgetc`, and `fclose`. `malloc` returns raw memory as `opaque`. `exit` ends the process with the given status code.

`sleep` pauses the current thread for the given number of milliseconds. It maps to `usleep` on macOS, Linux, Android, and iOS, and to `Sleep` on Windows. The platform-native `usleep` and `Sleep` bindings are also available under `extern::` on their respective platforms.

## Notes

These bindings return `opaque` for memory and handles that come from C, such as `malloc` results and `fopen` file handles. The garbage collector does not track `opaque` values, which is correct for these resources because the OS and the C allocator track them by a stable address.

The bindings do not bracket blocking calls for the garbage collector. A blocking function such as `sleep`, `fgetc`, `fscanf`, or `getchar` called directly on a GC-attached thread does not reach a safepoint while it waits, which can stall a stop-the-world collection. For blocking I/O and timing on attached threads, prefer the higher-level packages such as `fs`, `console`, and `threads`, which park the thread correctly while they wait.
