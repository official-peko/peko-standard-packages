# console

Console and file descriptor I/O for Pekoscript.

## Overview

The `console` package is the low-level file descriptor interface for console and file output. It covers writing and printing, reading from standard input, opening and closing file descriptors, ANSI text styling, and cursor control.

The package initializes its platform layer the first time it is imported. On Windows this enables ANSI escape codes and sets UTF-8 output.

## Import

This package is auto-imported as `console` in every Peko file. No import statement is needed. All functions are called with the `console::` prefix.

## Constants

### Fd

Standard file descriptor constants.

| Name | Value |
|---|---|
| `Fd::Stdin` | 0 |
| `Fd::Stdout` | 1 |
| `Fd::Stderr` | 2 |

### FdFlags

Flags for opening file descriptors. Combine them by passing an array to `open_fd`.

| Name | Value | Meaning |
|---|---|---|
| `FdFlags::Read` | 1 | Open the file for reading |
| `FdFlags::Write` | 2 | Open the file for writing |
| `FdFlags::Append` | 4 | Append writes to the end of the file |
| `FdFlags::Create` | 8 | Create the file if it does not exist |
| `FdFlags::Truncate` | 16 | Truncate the file to zero length when opening |

## Printing

`println` prints a value to stdout followed by a newline. `print` prints a value to stdout with no newline. Both are overloaded for `string`, `int`, `float`, `double`, `bool`, `char`, and `Array<string>`. For an array, `println` prints one element per line and `print` separates elements with spaces.

```
console::println("hello world");
console::println(42);
console::print("no newline here");
```

## Writing to a descriptor

### write

```
fn write(fd: int, str: string) => int?
fn write(fd: int, buf: fs::Buffer) => int?
```

Writes a string or a buffer of bytes to a file descriptor. Returns the number of bytes written, or `None` on error.

```
bytes := console::write(Fd::Stdout, "hello world")?;
```

## Standard error

```
fn eprintln(str: string)
fn eprint(str: string)
fn error(err: string)
```

`eprintln` prints a string to stderr with a newline. `eprint` prints with no newline. `error` prints a string to stderr prefixed with `Error: ` and followed by a newline.

```
console::error("file not found");
// Output: Error: file not found
```

## Flushing

```
fn flush()
fn flush(fd: int)
```

`flush()` flushes the stdout write buffer. `flush(fd)` flushes the buffer for a specific descriptor.

## Reading

### read_char

```
fn read_char() => char?
fn read_char(fd: int) => char?
```

Reads a single character from stdin or from a given descriptor. Returns `None` on end of input or error.

### read_line

```
fn read_line() => string?
fn read_line(fd: int) => string?
```

Reads a line from stdin or from a given descriptor until a newline or end of input. The newline is not included in the returned string. Returns `None` on error.

### input

```
fn input(query: string) => string?
fn input() => string?
```

`input(query)` prints a prompt to stdout, flushes, then reads a line from stdin. `input()` reads a line with no prompt. Returns `None` on error.

```
name := console::input("What is your name? ")?;
console::println(name);
```

## File descriptors

### open_fd

```
fn open_fd(path: string, flag: int) => int?
fn open_fd(path: string, flags: Array<int>) => int?
```

Opens a file with a single flag or a combination of flags from `FdFlags`. Returns the descriptor on success, or `None` on error.

```
fd := console::open_fd("log.txt", [FdFlags::Write, FdFlags::Create])?;
console::write(fd, "started\n");
console::close_fd(fd);
```

### close_fd

```
fn close_fd(fd: int) => bool
```

Closes a descriptor. Returns `false` on error.

### open_null

```
fn open_null() => int?
```

Opens a null device descriptor for discarding output. Returns `None` on error.

```
null_fd := console::open_null()?;
console::write(null_fd, "this is discarded");
console::close_fd(null_fd);
```

## Command line arguments

### get_argv

```
fn get_argv() => Array<String>
```

Returns the command line arguments passed to the program.

## Styling

Each styling function wraps a string in ANSI escape codes and returns the styled string. The codes reset at the end of the string, so styled strings can be nested and concatenated.

```
console::println(console::bold("important"));
console::println(console::red(console::underline("alert")));
```

### Text attributes

`bold`, `dim`, `italic`, `underline`, `blinking`, `reverse`, `hidden`, `strikethrough`.

### Foreground colors

`black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`.

### Background colors

`bg_black`, `bg_red`, `bg_green`, `bg_yellow`, `bg_blue`, `bg_magenta`, `bg_cyan`, `bg_white`.

### True color

```
fn rgb(input: string, r: int, g: int, b: int) => string
fn bg_rgb(input: string, r: int, g: int, b: int) => string
```

`rgb` sets the foreground color and `bg_rgb` sets the background color to the given red, green, and blue components, each in the range 0 to 255.

```
console::println(console::rgb("orange text", 255, 165, 0));
```

## Cursor control

### Movement

```
fn left()            fn left(n: int)
fn right()           fn right(n: int)
fn up()              fn up(n: int)
fn down()            fn down(n: int)
fn move_to(row: int, col: int)
fn home()
fn move_to_start()
```

The directional functions move the cursor by one cell or line, or by `n` cells or lines. `move_to` moves to a row and column using 1-based coordinates. `home` moves to the top-left corner. `move_to_start` moves to the start of the current line.

### State

```
fn save()
fn restore()
fn hide_cursor()
fn show_cursor()
```

`save` records the current cursor position and `restore` returns to it. `hide_cursor` and `show_cursor` control cursor visibility.

### Erasing

```
fn erase_to_end()
fn erase_from_start()
fn erase_line()
fn clear()
fn clear_scrollback()
```

`erase_to_end` clears from the cursor to the end of the line. `erase_from_start` clears from the start of the line to the cursor. `erase_line` clears the whole current line. `clear` clears the visible screen. `clear_scrollback` clears the screen and the scrollback buffer.
