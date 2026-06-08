# fs

Filesystem input and output for Pekoscript.

## Overview

The `fs` package provides file and directory access. It offers two layers:

- Convenience helpers for whole-file reads and writes in a single call.
- A class-based API with file handles, cursor-based reads and writes, buffered readers and writers, binary buffers, and directory traversal.

The `Buffer` class from this package is the byte container used across the standard library, including by `crypto` and `console`.

## Import

```
import fs@"v0.1.0";
```

This package is not auto-imported.

## Constants

### FileTypes

File type ids returned by `File::get_file_type`.

| Name | Value |
|---|---|
| `FileTypes::Unknown` | -1 |
| `FileTypes::Directory` | 0 |
| `FileTypes::Regular` | 1 |
| `FileTypes::Link` | 2 |
| `FileTypes::Block` | 3 |

### OpenMode

Modes for opening a file. Combine them by passing an array to `File::open`.

| Name | Value | Meaning |
|---|---|---|
| `OpenMode::Read` | 1 | Open for reading |
| `OpenMode::Write` | 2 | Open for writing |
| `OpenMode::Append` | 4 | Append writes to the end |
| `OpenMode::Binary` | 8 | Open in binary mode |
| `OpenMode::ReadWrite` | 16 | Open for both reading and writing |

### SeekFrom

Origins for `FileHandle::seek`.

| Name | Value | Meaning |
|---|---|---|
| `SeekFrom::Start` | 0 | Measure from the start of the file |
| `SeekFrom::Current` | 1 | Measure from the current cursor |
| `SeekFrom::End` | 2 | Measure from the end of the file |

## Convenience helpers

```
fn mkdir(dirpath: string) => bool
fn create(fpath: string) => bool
fn exists(fpath: string) => bool
fn read_file(fpath: string) => string?
fn write_file(fpath: string, text: string) => bool
fn append_file(fpath: string, text: string) => bool
fn read_bytes(fpath: string) => Buffer?
fn copy(src: string, dst: string) => bool
fn move(src: string, dst: string) => bool
```

`mkdir` creates a directory. `create` creates a new empty file. `exists` reports whether a path is present. `read_file` returns the whole file as a string, and `read_bytes` returns it as a `Buffer`. `write_file` replaces a file's contents, and `append_file` adds to the end. `copy` and `move` transfer a file to a new path. The functions that can fail return `None` or `false`.

```
import fs@"v0.1.0";

fn main() {
    contents := fs::read_file("/tmp/notes.txt")?;
    fs::write_file("/tmp/copy.txt", contents);
}
```

## Buffer

A binary-safe byte buffer. It tracks its length explicitly, so null bytes inside the data are preserved.

```
constructor()                                              // empty buffer
constructor(data: Pointer<void>, length: int, capacity: int)
fn len() => int
fn is_empty() => bool
fn as_raw() => Pointer<void>
fn to_string() => string
```

`len` returns the byte count. `as_raw` returns the raw data pointer, to be used together with `len`. `to_string` returns the contents as a string, which stops at the first null byte. For true binary data, use `as_raw` and `len`.

## File

Represents a path and its metadata. It is lightweight and holds no OS resources. Call `open` to obtain a `FileHandle`.

```
constructor(file_path: string)
fn path() => string
fn name() => string
fn extension() => string
fn exists() => bool
fn is_dir() => bool
fn get_file_type() => int
fn mode() => int
fn set_mode(mode: int) => bool
fn open(mode: int) => FileHandle?
fn open(modes: Array<int>) => FileHandle?
fn delete()
fn content_changed(snapshot: string) => bool
```

The constructor reads the path's metadata once. `name` returns the final path component, and `extension` returns the part after the last dot. `open` takes a single `OpenMode` value or an array of them, which are combined. `delete` removes the file or directory, recursing into directory contents. `content_changed` compares the current on-disk contents against a previously captured snapshot string.

```
f := fs::File("/tmp/data.bin");
if f.exists() {
    handle := f.open([fs::OpenMode::Read, fs::OpenMode::Binary])?;
    chunk  := handle.read(1024)?;
    handle.close();
}
```

## FileHandle

A live handle with a cursor. Obtain one from `File::open`. Operations return `None` once the handle is closed.

```
fn is_open() => bool
fn read(n: int) => Buffer?
fn read_to_string(n: int) => string?
fn read_all() => Buffer?
fn read_all_to_string() => string?
fn write(buf: Buffer) => int?
fn write_string(text: string) => int?
fn seek(offset: int, origin: int) => bool
fn tell() => int?
fn flush() => bool
fn close()
```

`read` and `read_to_string` read up to `n` bytes from the cursor. `read_all` and `read_all_to_string` read the rest of the file. `write` and `write_string` write at the cursor and return the number of bytes written. `seek` moves the cursor relative to a `SeekFrom` origin, and `tell` reports the current offset. `flush` pushes buffered writes to the OS. `close` releases the handle.

## BufReader

A buffered reader that reads ahead in chunks to reduce syscall overhead. Wrap an open `FileHandle`.

```
constructor(handle: FileHandle, chunk_size: int)
constructor(handle: FileHandle)            // default chunk size 8 KB
fn read_line() => string?
fn read_all() => string?
```

`read_line` returns the next line without its newline, and returns `None` at end of file. Carriage returns are stripped, so the same code handles both line ending styles.

```
handle := fs::File("/tmp/log.txt").open(fs::OpenMode::Read)?;
reader := fs::BufReader(handle);

while true {
    line := reader.read_line();
    if !line.is_value() {
        break;
    }
    console::println(line?);
}

handle.close();
```

## BufWriter

A buffered writer that accumulates writes in memory and flushes in chunks. Wrap an open `FileHandle`.

```
constructor(handle: FileHandle, chunk_size: int)
constructor(handle: FileHandle)            // default chunk size 8 KB
fn write(text: string) => bool
fn write_line(text: string) => bool
fn flush() => bool
fn close()
```

`write` adds text to the buffer and flushes automatically when the buffer reaches the chunk size. `write_line` adds text followed by a newline. `flush` writes buffered data to the handle. `close` flushes and then closes the underlying handle.

## Directory

Extends `File` with directory operations.

```
constructor(file_path: string)
fn child_count() => int
fn get_child(idx: int) => File?
fn children() => Array<File>
fn walk(max_depth: int) => Array<string>
```

`child_count` returns the number of direct children. `get_child` returns the child at an index, or `None` if out of range. `children` returns all direct children as `File` objects. `walk` recursively returns all descendant paths, with `max_depth` limiting the depth. Pass -1 for unlimited depth.

```
dir := fs::Directory("/tmp/project");
for path in dir.walk(-1) {
    console::println(path);
}
```

## Notes

A `File` holds no OS resources, so it is cheap to create and keep. A `FileHandle` holds an OS file handle and must be closed when finished, either directly with `close` or through `BufWriter::close`. Read functions return `Buffer` objects backed by GC-managed memory, which the collector tracks across moves.
