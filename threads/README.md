# threads

Threading for Pekoscript.

## Overview

The `threads` package provides system threads and the tools to coordinate them: mutexes, channels, futures, cooperative cancellation, and structured concurrency scopes. Blocking operations in this package are bracketed for the garbage collector, so a waiting thread parks for collections rather than stalling them.

## Import

```
import threads@"v0.1.0";
```

This package is not auto-imported. Its names are reached with the `threads::` prefix.

## Mutex

A mutual exclusion lock that protects a value of type `T`.

```
constructor(data: T)
fn lock() => T
fn with_lock(body: closure(T) => void)
fn release()
```

`lock` acquires the lock and returns the protected value, and `release` must be called when the work is done. For a reference type, the returned value is the shared object, so mutations through it are visible to other holders. For a plain value type the return is a copy, so use a `Mutex` of a value type as a lock token and keep the guarded state elsewhere. `with_lock` acquires the lock, passes the value to a closure, and releases the lock after the closure returns, even when the closure returns early.

```
shared := threads::Mutex<List<int>>(List<int>());

threads::new_thread(closure[shared]() {
    items := shared.lock();
    items.push(1);
    shared.release();
});
```

## Thread

A system thread.

```
constructor(thread_func: closure())
fn set_threaded_function(thread_func: closure())
fn start()
fn start_blocking()
fn is_active() => bool
fn kill()
```

`start` runs the thread in the background and returns at once. `start_blocking` runs the thread and blocks until it finishes. `kill` force-stops the thread, which is unsafe because the thread may hold a lock or be mid-allocation. Use a `CancelToken` for safe cancellation instead.

```
t := threads::Thread(closure() {
    console::println("running on a thread");
});
t.start();
```

## Convenience functions

```
fn new_thread(threaded_function: closure())
fn sleep(seconds: int)
fn sleep_ms(ms: int)
fn dispatch_main(work: closure())
```

`new_thread` creates and starts a thread in one call. `sleep` and `sleep_ms` block the current thread, with the GC bracketed so collections proceed. `dispatch_main` runs a closure on the main thread and returns immediately, which is useful for UI work that must happen on the main thread.

## CancelToken

A cooperative cancellation token shared between a thread and its caller. The running closure checks the token at natural points and returns early when it is cancelled. Cancellation is cooperative, so the thread is never forcibly stopped.

```
constructor()
fn is_cancelled() => bool
fn cancel()
fn on_cancel(callback: closure())
fn free()
```

`cancel` is safe to call from any thread and has no effect if the token is already cancelled. `on_cancel` registers a single callback that fires when the token is cancelled, or immediately if it is already cancelled. Call `free` when the token is no longer needed.

```
token := threads::CancelToken();

threads::new_thread(closure[token]() {
    while !token.is_cancelled() {
        do_work();
    }
});

token.cancel();
```

## Channel

A thread-safe message channel. It passes values between threads without shared state.

```
constructor(buffer_size: int)
fn send(item: T) => bool
fn try_send(item: T) => bool
fn recv() => T?
fn try_recv() => T?
fn close()
fn is_closed() => bool
fn free()
```

A `buffer_size` of 0 makes a rendezvous channel where `send` blocks until a receiver is ready. A positive size buffers up to that many items before `send` blocks. `send` blocks when the buffer is full and returns false when the channel is closed. `recv` blocks when the buffer is empty and returns `None` once the channel is closed and drained. The `try_` forms never block. `close` lets receivers drain remaining items and then receive `None`. Call `free` when the channel is no longer needed.

```
ch := threads::Channel<int>(10);

threads::new_thread(closure[ch]() {
    for i in 0..10 {
        ch.send(i);
    }
    ch.close();
});

while true {
    item := ch.recv();
    if item == None {
        break;
    }
    console::println(item?);
}
```

## Future

The result of an asynchronous computation. Obtain one from `make_future` or `Scope::spawn`.

```
constructor(handle: opaque)
fn await() => T?
fn await_timeout(ms: int) => T?
fn is_complete() => bool
fn cancel()
```

`await` blocks until the result is ready and returns `None` if the future was cancelled. `await_timeout` blocks up to a number of milliseconds and returns `None` on timeout or cancellation. `is_complete` checks readiness without blocking. `cancel` signals that the result is no longer needed, after which `await` returns `None` while the running work continues.

```
fn make_future<T>(work: closure() => T) => Future<T>
```

`make_future` runs a closure on a new thread and returns a future that resolves to its result.

```
f := threads::make_future(closure() => string {
    return fetch_data();
});

// other work here

data := f.await()?;
```

## Scope

Structured concurrency. Every future spawned on a scope is awaited automatically when the scope body finishes, so no spawned work outlives the scope.

```
constructor()
fn spawn(work: closure() => Pointer<void>) => Future<Pointer<void>>
fn spawn_void(work: closure())
fn get_cancel_token() => CancelToken
fn cancel()
```

`spawn` runs a closure on a new thread and tracks its future. `spawn_void` does the same for work with no result. `get_cancel_token` returns a token shared by all work in the scope, and `cancel` cancels every piece of work through it.

```
fn scope(body: closure(Scope))
```

`scope` runs a body inside a scope and waits for all spawned futures before it returns.

```
threads::scope(closure(s: threads::Scope) {
    f1 := s.spawn(closure() => Pointer<void> { return fetch_a() as Pointer<void>; });
    f2 := s.spawn(closure() => Pointer<void> { return fetch_b() as Pointer<void>; });
    a := f1.await()? as int;
    b := f2.await()? as int;
});
```

## Notes

A `Mutex` allocates its OS lock on the C heap so the collector never moves it, which keeps the lock valid across collections. Threads launched through this package attach to the collector at entry and detach before exit, so they reach safepoints correctly. A thread created outside this package must attach and detach itself before touching managed memory.

Force-killing a thread with `Thread::kill` can stop it while it holds a lock or allocates, which leads to deadlocks or corruption. Prefer a `CancelToken` so a thread stops at a safe point of its own choosing.
