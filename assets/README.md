# assets

Asset bundling and serving for Pekoscript.

## Overview

The `assets` package makes files that ship with an app available at runtime. Bundled assets are served over a local HTTP socket bound to a loopback port. Two access paths are provided:

- A URL for an asset, suitable for use directly in HTML tags rendered by a webview.
- The raw bytes of an asset, returned as an array of byte values.

The asset server starts automatically the first time a URL or asset bytes are requested. The byte source depends on the build:

- Debug builds read assets from the project assets directory on disk, so hot-reload runs do not require rebundling.
- Release builds read assets from the native bundle for the target platform (AppImage squashfs on Linux, NSBundle resources on macOS and iOS, the NDK AAssetManager on Android, embedded resources on Windows).

Asset names are hierarchical. A name like `icons/home.png` preserves its subdirectory.

## Import

```
import assets@"v0.1.0";
```

This package is not auto-imported.

## API

### get_asset_url

```
fn get_asset_url(name: string) => string
```

Returns the URL for a bundled asset. The URL is served over the local HTTP socket at a reserved prefix, so it works the same in debug and release builds. A missing asset is not reported as an error here. The URL returns a 404 status when it is fetched.

```
import assets@"v0.1.0";

fn main() {
    url := assets::get_asset_url("icons/home.png");
    img := <img src={url} />;
}
```

### get_asset_bytes

```
fn get_asset_bytes(name: string) => Array<int>
```

Returns the raw bytes of a bundled asset as an array of byte values in the range 0 to 255. Each call returns a fresh copy. The package does not cache on the Pekoscript side. Returns an empty array if the asset does not exist.

```
import assets@"v0.1.0";

fn main() {
    bytes := assets::get_asset_bytes("data/config.json");
    console::println(bytes.length());
}
```

### ensure_started

```
fn ensure_started()
```

Starts the asset server if it is not already running. It binds a dynamic loopback port and records it for URL construction. The public access functions call this automatically, so most code does not call it directly.

### stop

```
fn stop()
```

Stops the asset server and releases its resources. Asset URLs become invalid after this call. This function is intended for clean shutdown.

## Notes

The asset server runs in the background and serves byte ranges, so large assets stream to a webview rather than loading all at once. The reading path returns GC-managed memory and copies each asset into a fresh array on every call.
