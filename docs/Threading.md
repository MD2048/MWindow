# Threading

## The Core Rule

`MW::poll()` must be called from the main thread (the thread that called `MW::init()`).
This is not a MWindow restriction — it reflects reality on every supported platform. Win32 windows have thread affinity. AppKit (macOS) enforces main-thread access with assertions. Wayland's `wl_display` is not thread-safe. UIKit is main-thread only. MWindow's design makes this constraint explicit rather than hiding it.
While MW::poll() is running do not query any state! When the event handlers start executing you can start querying again.

## Reading State — Any Thread

State reads return copies. You get a value that is valid for as long as you hold it, independent of anything `poll()` might do concurrently:

```cpp
// Safe from any thread, any time
MPoint    cursor  = MW::getCursorPos();
bool      held    = MW::isKeyHeld(MW::MKey::W);
MMods     mods    = MW::getMods();
MSize     size    = window->getSize();
MPoint    pos     = window->getTopLeftCorner();
float     dpi     = window->getDpiScale();
MSize     phys    = window->getPhysicalSize();
```

**Exception:** `MW::getInputState()` returns a reference to internal state. Do not cache this reference across frames — re-query it each frame.
More on this [here](Input.md#state-vector).

## Writing State — Command Queue

Any thread can request window state changes. Requests are enqueued and applied by `poll()` on the main thread at the end of the frame:

```cpp
// Safe from any thread
window->resize({ 1920, 1080 });
window->setTopLeftCorner({ 100, 100 });
window->setWindowMode(MW::MWindowMode::Fullscreen);
window->setTitle("New Title");
window->show();
window->hide();
window->close();
```

None of these take effect immediately. The confirmed new state is written by `poll()` after the OS acknowledges the change. 
This also means that if you set something to a different value (eg. resize) and you query the current value this frame you will get the old one.

## Event Handlers

You are free to register any handler from any thread, it might block though.
Handlers are called during `MW::poll()` on the main thread.

## Summary

| Operation | Thread |
|---|---|
| `MW::init()` / `MW::shutdown()` | Main thread only |
| `MW::poll()` | Main thread only |
| Register / unregister handlers | Any thread |
| Read any state | Any thread |
| Request state changes | Any thread |
| `MW::getInputState()` | Any thread — do not cache reference |
