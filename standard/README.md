# standard

The core standard library for Peko.

## Overview

The `standard` package provides the foundational types the language builds on: the optional type behind `?`, the collection types `Array`, `List`, and `Map`, the `String` class, the `Pair` and `Box` containers, the iterator protocol, and the `format` helper.

## Import

This package is auto-unpacked into every Peko file with `import { * } from standard`. No import statement is needed, and its names are used bare, without a prefix. For example, `Array<int>()` and `String("hi")` are written directly.

## Option and the optional type

`Option<T>` is the type behind the `?` optional. A value of type `T?` is an `Option<T>`. It holds a value, is `None`, or is an `Error` carrying a message.

```
constructor(value: T)                    // a value
constructor(error: string, iserror: bool)  // an error
constructor()                            // None
fn unwrap() => T
fn get_error() => string
fn is_value() => bool
fn is_none() => bool
fn is_error() => bool
[operator ?]() => T
```

The language provides sugar over these. `None` constructs a None option, `Error("message")` constructs an error option, and the `?` postfix operator unwraps. `unwrap` and `?` halt the program if the option is None or an error, so guard with `is_value` first when a result may be absent.

```
fn first_char(s: String) => char? {
    if s.length() == 0 {
        return None;
    }
    return s[0];
}

fn main() {
    c := first_char(String("hi"));
    if c.is_value() {
        console::println(c?);
    }
}
```

## Array

A growable list of `T`.

```
constructor()
fn length() => int
fn back() => T?
fn remove(index: int) => T?
fn pop() => T?
fn clear()
fn insert(value: T, index: int)
fn push(value: T)
fn extend(other: Array<T>)
[operator []](index: int) => T
[operator iterator]() => Iterator<T>
```

`push` appends, `pop` removes and returns the last element, and `back` returns the last element without removing it. `insert` places a value at an index, and `remove` deletes the value at an index. `extend` appends every element of another array. The index operator reads an element, and the iterator operator drives a `for` loop.

```
nums := Array<int>();
nums.push(1);
nums.push(2);

for n in nums {
    console::println(n);
}
```

## List

Extends `Array` with search methods.

```
fn find(value: T) => int
fn contains(value: T) => bool
```

`find` returns the index of a value, and `contains` reports whether the value is present.

## Map

An associative container from keys of type `KT` to values of type `VT`.

```
constructor()
fn length() => int
fn remove(key: KT) => VT?
fn insert(key: KT, value: VT)
fn contains(key: KT) => bool
fn get(key: KT) => VT?
fn get_pair(index: int) => Pair<KT, VT>?
fn extend(other: Map<KT, VT>)
[operator []](index: KT) => VT
[operator iterator]() => Iterator<Pair<KT, VT>>
```

`insert` adds or replaces a value at a key. `get` returns the value at a key, or `None` when the key is absent. `contains` checks for a key. Iterating a map yields a `Pair` of key and value for each entry.

```
ages := Map<String, int>();
ages.insert(String("Joe"), 42);

for entry in ages {
    console::println(entry.get_first());
    console::println(entry.get_second());
}
```

## Pair

Holds two values.

```
constructor(first: FT, second: ST)
fn get_first() => FT
fn get_second() => ST
fn first_reference() => &FT
fn second_reference() => &ST
fn set_first(first: FT)
fn set_second(second: ST)
```

The reference methods return a mutable reference to each member.

## String

A mutable string class with a wide method set.

```
constructor(value: string)
constructor()
fn length() => int
fn push(str: string)
fn push(ch: char)
fn pop() => char?
fn back() => char?
fn insert(c: char, index: int)
fn remove(index: int) => char?
fn cancel(ch: char) => String
fn substring(start: int, end: int) => String?
fn substring(start: int) => String?
fn find(ch: char) => int
fn find(needle: string) => int
fn split(delimiter: char) => Array<String>
fn split(delimiter: string) => Array<String>
fn lsplit(delimiter: char) => Array<String>
fn lsplit(delimiter: string) => Array<String>
fn contains(ch: char) => bool
fn contains(needle: string) => bool
fn trim() => String
fn starts_with(prefix: string) => bool
fn ends_with(suffix: string) => bool
[operator []](index: int) => char
[operator []](r: RangeIterator) => String?
[operator ==](other: string) => bool
[operator !=](other: String) => bool
```

`push` appends a string or a character. `substring` returns a slice, with the single-argument form running to the end. `find` returns the index of a character or substring. `split` and `lsplit` break the string on a delimiter. `cancel` returns a copy with each occurrence of a character escaped with a backslash, which is used for quoting. The conversion operators (`to_string`, `to_int`, `to_float`, `to_char`, `to_double`, `to_bool`) let a `String` cast to a primitive. The range index operator returns a slice for a `range`.

## Iterators and range

`Iterator<IT>` is the base iterator. It exposes `next`, `back`, `current`, `inrange`, and `iter`. Collections return an iterator from their iterator operator, which drives `for` loops. Subclass it to make a custom type iterable.

```
fn range(start: int, end: int) => RangeIterator
```

`range` produces an iterator over integers from `start` up to `end`. The `start..end` form is sugar for it.

```
for i in 0..5 {
    console::println(i);   // 0 1 2 3 4
}
```

## Box

A reference cell that holds one value of type `T`. Holders of the same `Box` share one cell.

```
constructor(value: T)
fn get() => T
fn set(value: T)
fn update(f: closure(T) => T)
```

`get` returns the value, `set` replaces it, and `update` applies a function to the value and stores the result.

```
counter := Box<int>(0);
counter.update(closure(n: int) => int { return n + 1; });
console::println(counter.get());   // 1
```

## format

```
fn format(format_string: string, formats: Array<string>) => String?
```

Interpolates each item of `formats` at each `%` in the format string. Use `%~` to write a literal `%`. Returns an error when there are fewer items than placeholders.

```
greeting := format("Hello %.", #["Joe"])?;        // "Hello Joe."
progress := format("We are %%~ done!", #["80"])?; // "We are 80% done!"
```

## Notes

`Option`, the collections, and `String` are managed types tracked by the collector. The reference accessors on `Pair`, and `Box` as a shared cell, return live references into managed memory, which the collector keeps valid across a move.
