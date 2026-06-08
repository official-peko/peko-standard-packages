# random

Fast non-cryptographic random number generation for Pekoscript.

## Overview

The `random` package generates random numbers with the CMWC (Complementary Multiply With Carry) algorithm. It offers a global generator for quick use and an `Rng` class for independent, reproducible, or isolated sequences.

This generator is not cryptographically secure. For keys, tokens, salts, or any security-sensitive value, use the `crypto` package instead, through `crypto::random_bytes` and `crypto::random_u32`.

## Import

```
import random@"v0.1.0";
```

This package is not auto-imported. Its names are reached with the `random::` prefix.

## Global generator

A single shared generator seeded from the current time on first use. It is safe to call from multiple threads.

```
fn seed(s: int)
fn int(min: int, max: int) => int
fn float() => float
fn bool() => bool
fn choice<T>(array: Array<T>) => T?
```

`seed` sets the global generator to a known value for reproducible sequences in tests and simulations. If `seed` is not called, the generator seeds itself on first use. `int` returns a value in the range from `min` inclusive to `max` exclusive, and returns `min` when `max` is less than or equal to `min`. `float` returns a value in the range 0.0 inclusive to 1.0 exclusive. `bool` returns a random boolean. `choice` returns a random element of an array, or `None` when the array is empty.

```
import random@"v0.1.0";

fn main() {
    dice := random::int(1, 7);          // 1 through 6

    if random::bool() {
        console::println("heads");
    }

    names  := #["Alice", "Bob", "Carol"];
    picked := random::choice(names)?;
    console::println(picked);
}
```

## Rng

An independent generator with its own state. Seeding one `Rng` does not affect the global generator or any other `Rng`. Use it when you need reproducible or isolated sequences.

```
constructor()                            // seeded from the current time
constructor(seed: int)                   // seeded with a fixed value
fn seed(s: int)
fn int(min: int, max: int) => int
fn float() => float
fn bool() => bool
fn choice<T>(array: Array<T>) => T?
fn free()
```

The methods match the global functions. The same seed always produces the same sequence, which makes an `Rng` useful for repeatable simulations.

```
rng := random::Rng(42);
console::println(rng.int(1, 100));
console::println(rng.float());
rng.free();
```

## Notes

An `Rng` holds its generator state in C heap memory as an `opaque` handle. The garbage collector manages the `Rng` object itself but does not free that underlying C state. Call `free` when you are finished with an `Rng` to release it, and do not use the `Rng` after that call. The global generator needs no cleanup.
