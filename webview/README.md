# webview

A cross-platform webview for Peko.

## Overview

The `webview` package opens a native window that renders HTML, the rendering surface the PekoUI framework displays its pages in. It wraps each platform's native webview: WebView2 on Windows, WebKitGTK on Linux, WKWebView on macOS and iOS, and the Android System WebView.

A `WebView` is created with a title and a window size, pointed at a URL or given HTML directly, and then run. The `run` call blocks on the native event loop until the window closes.

## Import

```
import webview@"v0.1.0";
```

This package is not auto-imported. Its names are reached with the `webview::` prefix. It depends on `threads`.

## WebView

```
constructor(title: String, width: int, height: int)
constructor(title: String, width: int, height: int, url: String)
fn set_size(width: int, height: int)
fn set_title(new_title: string)
fn navigate(url: String)
fn set_html(html: String)
fn run()
```

The constructors create the window with a title and a size, and the four-argument form also loads an initial URL. `set_size` and `set_title` change the window. `navigate` loads a URL. `set_html` renders an HTML string directly rather than loading a URL. `run` starts the native event loop and blocks until the window closes, then releases the native window.

```
import webview@"v0.1.0";

fn main() {
    wv := webview::WebView("My App", 800, 600);
    wv.navigate("https://example.com");
    wv.run();
}
```

## Size hints

```
module WebViewHint {
    NoHint: int = 0;
    Min:    int = 1;
    Max:    int = 2;
    Fixed:  int = 3;
}
```

The size hint controls how the window may be resized. `NoHint` allows free resizing, `Min` and `Max` set a lower or upper bound, and `Fixed` locks the size. The window is created with `NoHint`.

## Platform differences

The desktop platforms (Windows, Linux, macOS) provide the full surface above. The mobile platforms differ:

- On Android, the constructor takes a title and a size, but the size is not used because the window fills the device. `set_title`, `navigate`, and `run` are available, and `set_title` must be called before `run`.
- On iOS, the constructor takes a URL as its first argument, and the size is managed by the iOS runtime. `set_title` is a no-op because the title is managed by the runtime. `navigate` must be called before `run`.

## Notes

`run` brackets the native event loop for the garbage collector, so the thread that runs the window parks for the whole loop and collections proceed without it. The native window handle is held as an `opaque` value that the collector does not move, which keeps the handle stable across collections. On Android, `navigate` performs its native call on a separate thread that is bracketed the same way.
