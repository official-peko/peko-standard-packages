# storage

Native local key-value and secure credential storage for Pekoscript.

## Overview

The `storage` package offers two stores:

- Local storage persists JSON values in a per-app SQLite database. It suits app settings, cached data, and other non-secret state.
- Secure storage holds small secrets such as auth tokens in the platform keychain, with an encrypted file fallback on systems that have no keychain.

The database opens automatically the first time the package is imported, using the application identifier the compiler sets at build time, and it closes when the program exits. Most programs do not manage the connection themselves.

## Import

```
import storage@"v0.1.0";
```

This package is not auto-imported. Its names are reached with the `storage::` prefix. It depends on `json` for value handling and `crypto` for the encrypted fallback.

## Lifecycle

```
fn init(app_id: string) => bool
fn available() => bool
```

`init` opens the database for a specific application identifier, replacing any open connection, and sets the keychain namespace. Most programs do not need it, since the database opens automatically. `available` reports whether the local database is open and ready.

## Local key-value storage

Values are `json::JsonValue` objects, so any JSON shape can be stored.

```
fn set(key: string, value: json::JsonValue) => bool
fn get(key: string) => json::JsonValue?
fn has(key: string) => bool
fn remove(key: string) => bool
fn clear() => bool
fn keys() => Array<String>
```

`set` serializes a value and stores it under a key, overwriting any existing value. `get` reads a value back, returning `None` when the key is absent and an error when storage is unavailable or the stored text does not parse. `has` tests for a key. `remove` deletes a key, and removing an absent key still succeeds. `clear` empties the store. `keys` returns all keys sorted in ascending order.

```
import storage@"v0.1.0";

fn main() {
    obj := json::JsonObject();
    obj.insert(String("name"), json::JsonString(String("Joe")));
    storage::set("user", obj);

    user := storage::get("user");
    if user.is_value() {
        console::println(user?.to_string());
    }
}
```

## Secure credential storage

```
fn secure_set(key: string, value: string) => bool
fn secure_get(key: string) => string?
fn secure_remove(key: string) => bool
```

`secure_set` stores a secret string in the platform keychain. `secure_get` reads it back, returning `None` when the key is absent. `secure_remove` deletes it, and removing an absent key still succeeds. These functions are meant for small secrets such as auth tokens.

```
storage::secure_set("auth_token", token);

saved := storage::secure_get("auth_token");
if saved.is_value() {
    use_token(saved?);
}
```

## Notes

Secrets are isolated per application by a namespace derived from the application identifier. The namespace maps to the keychain service on Apple platforms, the credential target prefix on Windows, and the app attribute on Linux. The local database handle is a stable malloc-backed context that holds the connection and a serialization mutex, so concurrent access from multiple threads is serialized inside the backend.
