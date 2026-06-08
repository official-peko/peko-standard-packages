# Peko Standard Library

The built-in packages that ship with the Peko toolchain.

## Overview

This repository holds the standard packages available to every Peko program. Together they cover the core language types, console and filesystem I/O, data parsing, concurrency, networking, cryptography, storage, and the PekoUI application framework. Each package is written in Pekoscript with a native C layer where it needs one, and each has its own README with the full API.

A few packages are loaded into every file automatically. The rest are imported by name when a program needs them.

## Importing packages

A package is imported by name and version.

```
import fs@"v0.1.0";
import crypto@"v0.1.0";
```

After import, a package's names are reached through its prefix, such as `fs::read_file` or `crypto::sha256`.

Four packages are loaded automatically and need no import statement:

| Package | How it loads | How its names are written |
|---|---|---|
| `standard` | Unpacked into every file | Bare, with no prefix (`Array`, `String`, `Map`) |
| `runtime` | Imported as `Runtime` | `Runtime::IntToString`, and so on |
| `console` | Imported as `console` | `console::println`, and so on |
| `pekoui` | Imported as `ui` | `ui::App`, `ui::Page`, and so on |

## Packages

### Language core

| Package | Summary |
|---|---|
| `standard` | The core types: `Option`, `Array`, `List`, `Map`, `Pair`, `Box`, `String`, iterators, and `format`. |
| `runtime` | Base runtime helpers: allocation, program exit, length, math, string operations, and conversions. |
| `ffi` | Thin bindings to common C library functions, exposed under the `extern` module. |

### Console and filesystem

| Package | Summary |
|---|---|
| `console` | File descriptor I/O, printing, stdin reading, ANSI styling, and cursor control. |
| `fs` | Files and directories, file handles with cursors, buffered readers and writers, and binary buffers. |
| `storage` | A per-app key-value store backed by SQLite, plus secure credential storage in the platform keychain. |

### Data and parsing

| Package | Summary |
|---|---|
| `json` | JSON parsing into a value tree and serialization back to text. |
| `lexer` | Basic lexical analysis used by `json` and HTTP request parsing. |

### Concurrency

| Package | Summary |
|---|---|
| `threads` | Threads, mutexes, channels, futures, cooperative cancellation, and structured scopes. |

### Networking

| Package | Summary |
|---|---|
| `sockets` | Raw TCP, HTTP, HTTPS over TLS, and WebSockets, with request building, streaming, servers, and downloads. |

### Security and randomness

| Package | Summary |
|---|---|
| `crypto` | Hashing, authenticated encryption, public key encryption and signatures, key derivation, and secure random, backed by libsodium. |
| `random` | Fast non-cryptographic random numbers using the CMWC algorithm. |

### Application and UI

| Package | Summary |
|---|---|
| `pekoui` | The PekoUI v2 framework: components, stores, pages, routing, layouts, and styling. |
| `webview` | A native window that renders HTML, used as the PekoUI rendering surface. |
| `assets` | Bundling and serving of app assets over a local HTTP socket. |

## Dependencies between packages

Several packages build on others:

- `standard` builds on `runtime`, which builds on `ffi`.
- `json` builds on `lexer`.
- `console` and `crypto` build on `fs`.
- `sockets` builds on `fs` and `threads`.
- `storage` builds on `json` and `crypto`.
- `webview` builds on `threads`.
- `pekoui` builds on `webview`, `sockets`, `json`, `fs`, `threads`, and `assets`.

## Garbage collection

The Peko collector is a stop-the-world sliding mark-compact collector that moves live objects. Managed values returned by these packages are tracked across collections, so references stay valid after a move. Packages that perform blocking calls bracket them so a waiting thread parks for collections rather than stalling them. Resources that the OS tracks by address, such as mutexes, file handles, and sockets, are held as stable non-managed handles.

Some packages expose a `free` method on a type that holds a non-managed handle, such as `random::Rng`, `threads::Channel`, and `threads::CancelToken`. Call `free` when the object is no longer needed, since the collector manages the wrapper object but not the underlying handle.
