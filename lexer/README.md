# lexer

Basic lexical analysis for Pekoscript.

## Overview

The `lexer` package breaks a string into a list of tokens. It was built for parsing JSON and HTTP requests, so it covers a small, general token set: identifiers, quoted strings, numbers, whitespace, and single other characters.

Each token carries its literal text and a type id. The `json` package uses this lexer as its tokenization stage.

## Import

```
import lexer@"v0.1.0";
```

This package is not auto-imported. Its names are reached with the `lexer::` prefix.

## Constants

### TokenType

The kind of a token.

| Name | Value | Meaning |
|---|---|---|
| `TokenType::Identifier` | 0 | A name made of letters, digits, and underscores |
| `TokenType::String` | 1 | A quoted string |
| `TokenType::Number` | 2 | A numeric value with an optional decimal point |
| `TokenType::Whitespace` | 3 | A single whitespace character |
| `TokenType::Other` | 4 | Any other single character |

## Token

A single unit of text produced by the lexer.

```
constructor(value: String, type: int)
fn get_value() => String
fn get_type() => int
fn value_equals(value: String) => bool
```

`get_value` returns the literal text. `get_type` returns the `TokenType` id. `value_equals` returns true when the token text matches and the token is not a string token. Use it instead of comparing the value directly, so a quoted string whose content equals a delimiter does not produce a false match.

## tokenize

```
fn tokenize(str: String) => Array<Token>
```

Breaks a string into tokens. The rules are:

- An identifier starts with a letter and continues with letters, digits, and underscores.
- A number starts with a digit or a minus sign and continues with digits, a single decimal point, underscores, and minus signs.
- A string is delimited by single or double quotes. Inside a string, `\n`, `\t`, and `\r` are converted to their control characters, and any other escaped character is taken literally.
- Each whitespace character becomes its own token.
- Any other character becomes a single `Other` token.

```
import lexer@"v0.1.0";

fn main() {
    tokens := lexer::tokenize("GET / HTTP/1.1");

    if tokens[0].value_equals("GET") {
        console::println("method is GET");
    }

    // Whitespace is also a token, so the path is the third token.
    if tokens[2].value_equals("/") {
        console::println("path is root");
    }
}
```

## Notes

Whitespace is preserved as tokens rather than skipped, so a parser that consumes this token stream advances past whitespace tokens itself. The `json` package does this in its index-advancing step.
