# sockets

Socket and HTTP networking for Pekoscript.

## Overview

The `sockets` package covers outbound requests and inbound servers. It supports raw TCP, HTTP, HTTPS over TLS, and WebSockets. Outbound traffic is built with the `Request` class and its `HttpRequest` subclass. Inbound and received traffic is read with the `Response` class and its `HttpResponse` subclass.

The package also provides incremental streaming in both directions, Server-Sent Events parsing, and file downloads.

## Import

```
import sockets@"v0.1.0";
```

This package is not auto-imported. Its names are reached with the `sockets::` prefix. It depends on `fs` and `threads`.

## Building requests

`Request` builds an outbound request with chained builder methods. Each builder returns the request, so calls chain.

```
constructor()
constructor(content: String)
fn method(method: String) => Request
fn path(path: String) => Request
fn header(name: String, value: String) => Request
fn body(body: String) => Request
fn content(mime: String, body: String) => Request
fn json(body: String) => Request
fn text(body: String) => Request
fn tls() => Request
fn secure(secure: bool) => Request
fn version(version: String) => Request
fn is_http() => bool
fn is_secure() => bool
fn serialize(host: String) => String
```

`method` and `path` set the request line. `header` adds a header. `body` sets a raw body, while `content` sets a body with a MIME type, and `json` and `text` are shorthands for the common content types. `tls` enables TLS, and `secure(bool)` sets it explicitly. `serialize` produces the wire bytes for a host.

## Sending requests

```
fn send(host: String, port: int) => Response?
fn send(host: String) => Response?
fn send_raw(host: String, port: int) => String
fn send_http(host: String, port: int) => HttpResponse?
fn send_http(host: String) => HttpResponse?
fn send_no_response(host: String, port: int)
fn send_no_response(host: String)
```

`send` sends the request and returns the parsed `Response`, or `None` on failure. `send_http` returns the response as an `HttpResponse`. `send_raw` returns the raw response string. `send_no_response` sends on a new thread and does not wait for a reply. The single-host forms use port 443 when TLS is enabled and port 80 otherwise.

```
import sockets@"v0.1.0";

fn main() {
    resp := sockets::Request()
        .method("GET")
        .path("/")
        .tls()
        .send_http("example.com")?;

    console::println(resp.get_status_code());
    console::println(resp.body());
}
```

## Responses

`Response` is the base for received data.

```
constructor(content: String)
fn get_response() => String
fn body() => String
```

`HttpResponse` extends it with status, headers, and a builder for server replies.

```
constructor(content: String, status_code: int)
constructor(content: String, status_code: int, content_type: String)
constructor(raw_response: String)
fn status(status_code: int) => HttpResponse
fn content_type(content_type: String) => HttpResponse
fn header(name: String, value: String) => HttpResponse
fn body_content(body: String) => HttpResponse
fn status_code() => int
fn get_status_code() => int
fn get_content_type() => String
fn body() => String
fn header_value(name: String) => String?
fn get_response() => String
```

The builder methods return the response, so a server reply can be built in a chain. The content, status, and raw-response constructors cover building a reply and parsing a received one.

## Streaming responses

`stream` reads a response incrementally rather than buffering the whole body.

```
fn stream(host: String, port: int,
          on_headers: closure(HttpResponse) => bool,
          on_chunk:   closure(String) => bool) => bool
fn stream(host: String,
          on_headers: closure(HttpResponse) => bool,
          on_chunk:   closure(String) => bool) => bool
```

`on_headers` is called once with the parsed status line and headers, before any body bytes. Returning false from it stops the read. `on_chunk` is called for each chunk of body bytes, with chunked transfer encoding already de-framed. Returning false from it stops the read. The function returns true on a clean finish and false on a transport error.

```
ok := sockets::Request().method("GET").path("/stream/20").tls()
    .stream("httpbin.org", 443,
        closure(resp: sockets::HttpResponse) => bool {
            console::println(`status ${resp.get_status_code()}`);
            return true;
        },
        closure(chunk: String) => bool {
            console::print(chunk);
            return true;
        });
```

`stream_sse` parses the body as Server-Sent Events and calls `on_event` once per complete event with the event's data payload.

```
fn stream_sse(host: String, port: int,
              on_headers: closure(HttpResponse) => bool,
              on_event:   closure(String) => bool) => bool
fn stream_sse(host: String,
              on_headers: closure(HttpResponse) => bool,
              on_event:   closure(String) => bool) => bool
```

The SSE helper exposes only the data payload, which suits token streams and status feeds. For full SSE semantics, including event names and ids, use `stream` and parse the chunks directly.

## Streaming request bodies

`open_stream` opens a connection and returns a `RequestWriter` for sending a body in chunks.

```
fn open_stream(host: String, port: int) => RequestWriter?
fn open_stream(host: String) => RequestWriter?
```

`RequestWriter` methods:

```
fn write(chunk: String) => bool
fn finish() => HttpResponse?
fn abort()
fn closed() => bool
```

`write` sends a chunk of body bytes. `finish` sends the terminating chunk, reads the full response, closes the connection, and returns the parsed `HttpResponse`. `abort` closes the connection without finishing, for cleanup paths. `closed` reports whether the writer has finished, aborted, or hit a transport error.

## Parsing incoming requests

`HttpRequest` parses a raw request string into a method, URI, query arguments, headers, and body.

```
constructor(content: String)
fn request_type() => String
fn uri() => String
fn get(url_argument: String) => String?
fn get_header(header: String) => String?
fn get_body() => String
```

`request_type` returns the method such as GET or POST. `uri` returns the path without the query string. `get` reads a query-string argument. `get_header` reads a header. `get_body` returns the body, with chunked encoding reassembled when present.

## Client

A small wrapper that holds a server address and sends requests to it.

```
constructor(server_address: String, port: int)
fn get_server_address() => String
fn get_port() => int
fn request(request: Request) => Response?
```

## Servers

### Socket

A raw TCP server. It serves multiple clients at once, each on its own thread. A port of 0 selects a random open port at listen time.

```
constructor(port: int)
constructor(port: int, callback: closure(Request) => Response)
constructor()
constructor(callback: closure(Request) => Response)
fn set_callback(callback: closure(Request) => Response)
fn set_port(port: int)
fn get_port() => int
fn listen_attached() => bool
fn listen()
fn stop()
```

`listen_attached` runs the accept loop on the current thread and blocks. `listen` runs it on a new thread and returns at once. The callback receives each request and returns the response to send. The convenience function `open_socket(port, callback)` builds a `Socket` and calls `listen`.

### HttpSocket

An HTTP server built on `Socket`, with path-based routing.

```
constructor(port: int)
constructor()
fn route(url: String, handler: closure(HttpRequest) => HttpResponse)
fn get_current_route() => String
fn listen_attached()
fn listen()
fn get_port() => int
```

`route` maps a path to a handler that receives an `HttpRequest` and returns an `HttpResponse`. An unrouted path returns a 404 response.

```
server := sockets::HttpSocket(8080);
server.route("/", closure(req: sockets::HttpRequest) => sockets::HttpResponse {
    return sockets::HttpResponse("<h1>Hello World</h1>", 200);
});
server.listen();
```

### WebSocket

A WebSocket server connection. It serves multiple clients at once. Reading and writing are decoupled across threads so a slow client cannot stall the receive loop. A port of 0 selects a random open port.

```
constructor()
constructor(callback: closure(WebSocket, Request))
constructor(port: int)
constructor(port: int, callback: closure(WebSocket, Request))
fn send(text: String) => bool
fn send_no_response(text: String)
fn try_send(text: String) => bool
fn set_callback(callback: closure(WebSocket, Request))
fn set_port(port: int)
fn get_port() => int
fn listen_attached() => bool
fn listen()
fn stop()
```

`send` queues a text message and reports success. `try_send` attempts a send without blocking. `send_no_response` queues without waiting. The convenience function `open_web_socket(port, callback)` builds a `WebSocket` and calls `listen`.

## Downloads

```
fn download(host: String, path: String, fpath: String) => bool
fn download(host: String, path: String, port: int, fpath: String) => bool
fn download(host: String, path: String, fpath: String, chunk_size: int) => bool
fn download(host: String, path: String, port: int, fpath: String, chunk_size: int) => bool
fn download_to_file(host: String, path: String, dest: fs::File) => bool
fn download_to_file(host: String, path: String, port: int, dest: fs::File) => bool
```

`download` fetches a path and writes it to a file path, streaming in chunks so a large file does not load fully into memory. `chunk_size` sets the streaming buffer size. `download_to_file` writes to an `fs::File` target instead of a path string.

## Notes

Blocking calls in this package are bracketed for the garbage collector, so a thread waiting on a socket parks for collections rather than stalling them. The servers dispatch each connection to its own thread through the `threads` package, so handlers run concurrently and must treat shared state as such. Streaming callbacks run on the connection's thread and should return promptly.
