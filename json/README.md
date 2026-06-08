# json

JSON parsing and creation for Pekoscript.

## Overview

The `json` package parses JSON text into a tree of value objects and builds JSON text from those objects. Every value is a `JsonValue`. Specific kinds derive from it: `JsonObject`, `JsonArray`, `JsonString`, `JsonNumber`, `JsonBoolean`, and `JsonNull`. Each kind reports its type through `get_type` and serializes through `to_string`.

Parsing is done by the `json` function, which returns an optional value. The result is `None` when the input is malformed, carrying an error message that describes the problem.

## Import

```
import json@"v0.1.0";
```

This package is not auto-imported. It depends on `lexer` for tokenization. The examples below use the `json::` prefix for the package's names.

## Parsing

```
fn json(str: String) => JsonValue?
```

Parses JSON text into a `JsonValue` tree. Returns `None` on malformed input. Use the `?` operator to propagate the result, then cast to the expected kind.

```
import json@"v0.1.0";

fn main() {
    person := json::json(`
    {
        "name": "Joe",
        "age": 42,
        "active": true,
        "manager": null
    }
    `)? as json::JsonObject;

    console::println(person["name"]?.to_string());   // "Joe"
    console::println(person["age"]?.to_string());     // 42
    console::println(person["active"]?.get_type());   // boolean
    console::println(person["manager"]?.get_type());  // null
    console::println(person.contains("SSN"));         // false
}
```

## Value types

### JsonValue

The base type for every JSON value. It is intended to be derived, not used directly.

```
fn to_string() => String
fn get_type() => String
```

`to_string` serializes the value to JSON text. `get_type` returns the kind of the value as a string.

### JsonObject

A JSON object backed by a map of string keys to values.

```
constructor()
constructor(key_values: Map<String, JsonValue>)
fn length() => int
fn contains(key: String) => bool
fn insert(key: String, value: JsonValue)
fn to_string() => String                 // returns serialized object text
fn get_type() => String                  // returns "object"
[operator []](index: String) => JsonValue?
```

`insert` adds a new key or replaces the value at an existing key. The index operator returns the value at a key, or `None` if the key is absent. `to_string` serializes the object without pretty printing.

### JsonArray

An ordered list of values.

```
constructor()
constructor(values: Array<JsonValue>)
fn push(value: JsonValue)
fn length() => int
fn to_string() => String
fn get_type() => String                  // returns "array"
[operator []](index: int) => JsonValue?
```

### JsonString

Holds a string value.

```
constructor()
constructor(string_value: String)
fn get_string() => String
fn length() => int
fn to_string() => String                 // adds surrounding quotes
fn get_type() => String                  // returns "string"
```

`get_string` returns the raw string. `to_string` returns the JSON form, with surrounding quotes and the inner quotes escaped.

### JsonNumber

Holds a numeric value as a float.

```
constructor(number_value: float)
fn get_number() => float
fn to_string() => String
fn get_type() => String                  // returns "number"
```

### JsonBoolean

Holds a boolean value.

```
constructor(bool_value: bool)
fn get_bool() => bool
fn to_string() => String                 // returns "true" or "false"
fn get_type() => String                  // returns "boolean"
```

### JsonNull

Represents a null value.

```
constructor()
fn to_string() => String                 // returns "null"
fn get_type() => String                  // returns "null"
```

## Building JSON

Construct value objects and serialize them with `to_string`.

```
obj := json::JsonObject();
obj.insert("name", json::JsonString("Joe"));
obj.insert("age", json::JsonNumber(42.0));
obj.insert("active", json::JsonBoolean(true));

console::println(obj.to_string());
// {"name":"Joe","age":42,"active":true}
```

## Notes

Reading a value out of a `JsonObject` or `JsonArray` returns an optional, so use the `?` operator at each access. Use `get_type` to branch on the kind of a value before casting it to the matching class.
