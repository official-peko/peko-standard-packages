# runtime

Base runtime functionality for Pekoscript.

## Overview

The `runtime` package holds the core helpers the language depends on: GC-managed allocation, program exit, numeric and string length, math helpers, string operations, character and string predicates, and conversions between strings and primitive values.

Many of these functions are invoked by the compiler for built-in operations. The `^` operator calls `Exponential`, the `%` operator calls `Modulus`, string concatenation calls `StringAdd`, and a cast between a string and a primitive calls the matching conversion. Because of this, the names in this package follow the compiler's naming rather than the usual Peko style. Most code uses the operators and casts and never calls these functions directly, but they are available.

## Import

This package is auto-imported as `Runtime` in every Peko file. No import statement is needed. Its names are reached with the `Runtime::` prefix.

## Allocation

```
fn Allocate<T>(amount: int) => Pointer<T>
fn CreateManaged(src: cstr) => string
```

`Allocate` allocates a GC-managed array of `amount` elements of type `T`. It routes through the compiler allocation built-in, which produces an atomic buffer for a primitive `T` and a traced buffer for a managed `T`. `CreateManaged` copies a raw C string into a fresh GC-managed string and null-terminates it.

## Program exit

```
fn Exit(exit_code: int)
fn Exit(exit_message: string)
```

`Exit(exit_code)` detaches the thread from the GC, shuts the GC down, and ends the process with the given code. `Exit(exit_message)` prints the message with the current file and line, then exits with code 134.

## Platform

```
fn HideWindowsConsole()
```

Hides the Windows console window. It is present in Windows GUI builds.

## Length

```
fn length(str: string) => int
fn length(num: int) => int
fn length(num: float) => int
```

`length(str)` returns the number of characters in a string. `length(num: int)` returns the number of digits in an integer, counting the minus sign for a negative value. `length(num: float)` returns the number of characters the float would produce as a string.

## Math

```
fn Exponential(base: float, power: float) => float
fn Modulus(lhs: float, rhs: float) => float
```

`Exponential` raises `base` to `power` and backs the `^` operator. `Modulus` returns the floating-point remainder and backs the `%` operator.

## String operations

```
fn StringAdd(str1: string, str2: string) => string
fn StringRemove(str: string, index: int) => string
fn StringInsert(str1: string, str2: string, index: int) => string
```

`StringAdd` concatenates two strings into a new GC-managed string and backs the string `+` operator. `StringRemove` returns a copy with the character at `index` removed. `StringInsert` returns a copy of `str1` with `str2` inserted at `index`.

## Character predicates

```
fn IsDigit(ch: char) => bool
fn IsAlpha(ch: char) => bool
fn IsAlnum(ch: char) => bool
```

`IsDigit` reports whether a character is a decimal digit. `IsAlpha` reports whether it is an ASCII letter. `IsAlnum` reports whether it is a letter or a digit.

## String predicates

```
fn StringIsBool(str: string) => bool
fn StringIsFloat(str: string) => bool
fn StringIsInt(str: string) => int
fn StringIsChar(str: string) => bool
```

Each predicate reports whether a string can be read as the named type. `StringIsBool` is true for `"true"` and `"false"`. `StringIsFloat` allows an optional leading minus and a single decimal point. `StringIsInt` is true for an optional leading minus followed by digits. `StringIsChar` is true for a single character.

## Conversion to a value

```
fn StringToBool(str: string) => bool
fn StringToInt(str: string) => int
fn StringToFloat(str: string) => float
fn StringToChar(str: string) => char
```

Each function parses a string into the named primitive and backs the matching cast. If the string is not valid for the target type, the function prints an error and halts the program with code 134. Check with the predicates first when the input may be invalid.

```
n := Runtime::StringToInt("123");   // 123
```

## Conversion to a string

```
fn BoolToString(boolean: bool) => string
fn IntToString(number: int) => string
fn FloatToString(number: float) => string
fn CharToString(ch: char) => string
```

Each function renders a primitive value as a GC-managed string. `FloatToString` produces up to six significant digits.

## Notes

The GC and thread ABI symbols, including the allocation object entry points and the thread attach and detach calls, are declared inside this package. They belong to the runtime module and must not be redeclared in any other Peko file. Redeclaring them produces duplicate symbol errors and undefined behavior. User code allocates through `Allocate` and the language built-ins rather than the raw ABI.
