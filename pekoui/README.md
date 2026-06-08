# pekoui

The PekoUI v2 framework.

## Overview

PekoUI is a component framework for building native desktop and mobile apps with a web rendering layer. It provides a component model with reactive state and shared stores, page-based routing, layouts, and styling. An app renders its component tree to HTML, serves it over a local HTTP socket, drives live updates over a WebSocket, and displays the result in a native webview.

The model has a few core pieces:

- `Component` renders an element tree and re-renders when its state changes.
- `Store` holds shared state and notifies subscribed components when it changes.
- `Page` is a component bound to a route. It owns path parameters, a title, and styling.
- `App` registers routes and layouts and runs the servers and the webview.
- `Element` is a node in the rendered tree. Element literals produce these.
- `Event` carries the payload of a user interaction to a handler.
- `Styling` holds the CSS for a page or layout.

## Import

This package is auto-imported as `ui` in every Peko file. No import statement is needed. Its names are reached with the `ui::` prefix, for example `ui::App` and `ui::Page`.

## State and reactivity

A field marked `[state]` on a `Component`, `Page`, or `Store` is tracked by the framework. After the field is mutated, or after a `[mutates]` method that changes it returns, the framework refreshes the affected pages. Components do not call a refresh function directly in normal use.

## Quick start

```
class CounterStore from ui::Store {
    [state] n: int;

    constructor() => super() {
        this.n = 0;
    }

    [mutates] fn increment() {
        this.n += 1;
    }
}

counter := CounterStore();

class HomePage from ui::Page {
    constructor() => super() {
        this.set_title("Counter");
    }

    fn on_mount() {
        counter.subscribe(this);
    }

    fn render() => ui::Element {
        return <div>
            <h1>Count: {counter.n}</h1>
            <button onclick={closure(e: ui::Event) { counter.increment(); }}>Add one</button>
        </div>;
    }
}

fn main() {
    app := ui::App("Counter", 400, 300);
    app.add_page("/", HomePage());
    app.run();
}
```

## Component

The base class for a unit of UI. Subclass it and override `render`.

```
constructor() => super()
fn render() => Element
fn on_mount()
fn onStateChanged(attr_name: string)
fn on_store_changed()
```

`render` returns the element tree for the component. Override it. `on_mount` runs once the first time the component is mounted under a page. Override it to subscribe to stores or do other one-time setup. `onStateChanged` and `on_store_changed` are framework hooks that trigger a refresh. The default `render` returns a placeholder, so a subclass must provide its own.

A component is registered with the framework when it is constructed, which lets the page tree find and mount it.

## Store

The base class for shared state. Subclass it, declare `[state]` fields, and mutate them through `[mutates]` methods. Subscribers are notified on every change.

```
constructor() => super()
fn subscribe(s: Subscriber)
fn unsubscribe(s: Subscriber)
fn subscriber_count() => int
```

`subscribe` adds a subscriber, and adding the same one twice is a no-op. `unsubscribe` removes it. A component usually subscribes to a store in its `on_mount`. A store can also subscribe to another store, which forwards changes onward.

## Page

A component bound to a route. It extends `Component`, so it also has `render` and `[state]` fields.

```
constructor() => super()
fn set_title(title: string)
fn set_styling(styling: Styling)
fn get_styling() => Styling
fn get_param(name: String) => String?
fn navigate(url: String)
fn mount(c: Component)
fn unmount(c: Component)
fn on_request(request: sockets::HttpRequest) => Element
fn handle_request(request: sockets::HttpRequest)
fn refresh()
```

`set_title` sets the window title. `set_styling` installs the page's CSS, called from the subclass constructor after `super()`. `get_param` reads a path parameter captured by the route, such as `id` from `/users/:id`. `navigate` requests a client-side navigation, sent after the current handler returns.

`mount` attaches a class component so its state changes refresh this page, and it runs the component's `on_mount` once. `unmount` detaches a component and releases it.

`handle_request` runs before the page renders on an HTTP request. Override it to read the request method through `request.request_type()` and the body through `request.get_body()`, then update page state. `refresh` forces a re-render, which is rarely needed because state mutations and store notifications already refresh the page.

## App

The application root. It registers routes and layouts and starts the runtime.

```
constructor(title: string, width: int, height: int)
constructor(title: string)                 // default window 1024 x 768
constructor()                              // empty title, default window
fn add_page(pattern: String, page: Page)
fn add_page(pattern: String, factory: closure(Map<String, String>) => Page)
fn set_root_layout(layout: closure(Element) => Element)
fn set_root_layout(layout: closure(Element) => Element, styling: Styling)
fn add_layout(pattern: String, layout: closure(Element) => Element)
fn add_layout(pattern: String, layout: closure(Element) => Element, styling: Styling)
fn run()
```

`add_page` maps a route pattern to a page. The instance form serves one shared page. The factory form builds a fresh page per request and receives the matched path parameters, which suits dynamic routes such as `/users/:id`. When several patterns match, the most specific one wins.

`set_root_layout` wraps every page in a layout function that receives the page content and returns a wrapping element. `add_layout` adds a layout for a path prefix. Both layout forms accept optional styling.

`run` starts the WebSocket server, the HTTP server, the optional debug server, and the webview, then blocks on the webview event loop until the window closes.

## Element

A node in the rendered tree. Element literals in the `<tag>...</tag>` form produce `Element` values, and an expression in braces such as `{value}` inserts dynamic content.

```
constructor(tag_name, attributes, children, inner_text, events)   // all default
fn get_tag_name() => String
fn get_attribute(attribute: String) => String?
fn get_attributes() => Map<String, String>
fn get_inner_text() => String
fn get_children() => Array<Element>
fn get_nth_child(idx: int) => Element?
fn set_attribute(attribute: String, value: String)
fn remove_attribute(attribute: String) => String?
fn set_inner_text(inner_text: String)
fn append_child(child: Element)
fn remove_nth_child(idx: int) => Element?
fn add_event_listener(event_kind: String, handler: closure(Event) => void)
fn get_elements_by_class_name(class_name: String) => Array<Element>
fn get_elements_by_tag_name(tag_name: String) => Array<Element>
fn get_element_by_id(id: String) => Element?
fn render() => String
```

The getters and mutators read and change tags, attributes, text, and children. `add_event_listener` attaches a handler for an event kind such as `onclick`. The tree query methods search this element and its descendants. `render` produces the HTML string for the element and its subtree.

## Event

The payload delivered to an event handler. A handler is a `closure(Event) => void`.

```
fn kind() => String                  // event kind, e.g. "onclick"
fn element_uid() => String
fn value() => String                 // current value of the target element
fn form_data() => Map<String, String>  // field name to value, for onsubmit
fn key() => String                   // keyboard key name
fn key_code() => int
fn shift_held() => bool
fn ctrl_held() => bool
fn alt_held() => bool
fn meta_held() => bool
fn mouse_x() => int                  // -1 when not applicable
fn mouse_y() => int                  // -1 when not applicable
fn mouse_button() => int             // 0 left, 1 middle, 2 right; -1 otherwise
fn wheel_dy() => int
fn raw() => json::JsonObject
```

`value` returns the current value of the element that fired, which is useful for inputs. `form_data` is populated for submit events and maps each field name to its value. The keyboard and mouse accessors return their fallbacks when the event was not of that type. `raw` exposes the full JSON payload for cases the typed accessors do not cover.

## Styling

Holds the CSS for a page or layout.

```
constructor()                            // empty
constructor(css: string)                 // static CSS
constructor(css: String)                 // static CSS from a String
constructor(creator: closure() => string)  // CSS resolved on every render
fn resolve() => string
```

Construct a `Styling` from a CSS string and pass it to `Page::set_styling`, `App::set_root_layout`, or `App::add_layout`. The closure form is resolved fresh on every render, which the build tooling uses for hot reload.

## Notes

The framework serves each connection on its own thread. Component construction, the registry, the current page, and the render tree are guarded by mutexes so the HTTP and WebSocket handlers do not corrupt shared state. User code does not manage these locks.

Stores and components are GC-managed objects. The registry holds them in an array of managed pointers, which the collector relocates correctly across a collection, so references stay valid. A component is released for collection only after it is unmounted and nothing else holds a reference to it.

In hot reload runs, the build tooling sets the framework into debug mode, starts a style server, and serves CSS from disk so style edits appear without a rebuild.
