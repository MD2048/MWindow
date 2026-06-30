# Architecture

## The Hidden Singleton

MWindow is backed by a hidden singleton called `MGlobal`. It is never exposed publicly. All interaction goes through free functions in the `MW` namespace, which forward to it internally.

`MGlobal` owns:
- The event queue (ring buffer)
- Monitor list and HDR info
- Global input state — keyboard bits, cursor position, mouse buttons, touch, stylus
- Gamepad device list
- Window entry list

```cpp
MW::init();     // constructs MGlobal
MW::poll();     // forwards to MGlobal
MW::shutdown(); // destroys MGlobal
```

## Window Identity

Every window has a unique id stored in MGlobal:

```cpp
using MWindowID = uint64_t;
```

Windows, like most other things, are stored in a flat `std::vector`. Linear search is used for lookup — window counts are always small enough that this is irrelevant.

## Lifetime

`MW::MWindow::create()` returns a `std::unique_ptr<MW::MWindow>`. The user owns the lifetime. MWindow never destroys a window behind your back.

`close()` does not immediately destroy the window. It pushes an `MCloseRequestEvent` and marks the window for cleanup at the end of the current frame. This lets functions succeed until the end of the frame.

## Poll Loop

`MW::poll()` does the following each frame:

1. Clean up windows that were closed this frame
2. Execute queued window state commands (resize, mode change, etc.)
3. Drain the OS message queue (platform-specific)
4. Poll gamepad state
5. Drain the MWindow event ring buffer — walk each event through its handler chain

## Game Loop

This is how a game loop should work using MWindow:

```cpp
bool runnning = true;
while(isRunning) {
  MW::poll(); // poll() will start calling your registered event handlers
              // if you want you can launch threads/jobs from there, if they don't need the full input
  startRendering(); // launch threads, jobs
}
```

## Command Queue for State Changes

Any thread can request window state changes. These are not applied immediately — they are enqueued and executed by `MW::poll()` on the main thread. This is required because most platform window APIs are main-thread only.

```cpp
// Safe to call from any thread
window->resize({ 1920, 1080 });
window->setWindowMode(MW::MWindowMode::Fullscreen);
// Applied at the end of the next MW::poll() call
```
**WindowWndProc** — attached to every real window. Handles per-window messages: resize, move, focus, touch, stylus, DPI change, close.

This keeps global input state and per-window state completely separate in the message handling path.
