# Events

## Event Type

All events are held in a `std::variant`:

```cpp
using MEvent = std::variant<
    std::monostate,          // empty/default slot; never delivered to handlers

    MVisibilityChangeEvent,
    MCloseRequestEvent,
    MResizeEvent,
    MMoveEvent,
    MFocusChangeEvent,
    MMonitorChangeEvent,

    MKeyPressEvent,
    MKeyReleaseEvent,
    MCharEvent,

    MMouseMoveEvent,
    MMouseButtonPressEvent,
    MMouseButtonReleaseEvent,
    MMouseScrollEvent,
    MMouseEnterEvent,
    MMouseLeaveEvent,

    MTouchBeginEvent,
    MTouchMoveEvent,
    MTouchEndEvent,
    MTouchCancelEvent,

    MGamepadConnectedEvent,
    MGamepadDisconnectedEvent,
    MGamepadButtonPressEvent,
    MGamepadButtonReleaseEvent,
    MGamepadTriggerEvent,
    MGamepadStickEvent,

    MStylusEnterEvent,
    MStylusHoverEvent,
    MStylusLeaveEvent,
    MStylusDownEvent,
    MStylusMoveEvent,
    MStylusUpEvent,
    MStylusCancelEvent,

    MDisplaySettingChangeEvent,
    MDisplayConnectedEvent,
    MDisplayDisconnectedEvent
>;
```

Use `std::get_if` or `std::visit` to handle events:

```cpp
window->registerEventHandler([](const MW::MEvent& e) {
    if (auto* key = std::get_if<MW::MKeyPressEvent>(&e)) {
        if (key->key == MW::MKey::Escape)
            return MW::MEventResult::Consumed;
    }
    return MW::MEventResult::Continue;
});
```

## Handler Chain

Each window holds an ordered list of event handlers. `MW::poll()` walks each event through the chain. A handler returns `MEventResult::Consumed` to stop propagation, or `MEventResult::Continue` to pass the event to the next handler.

```cpp
using MEventHandler   = std::function<MEventResult(const MEvent&)>;
using MEventHandlerID = uint64_t;

MEventHandlerID id = window->registerEventHandler(handler);
window->unregisterEventHandler(id);
```

Handlers are checked in reverse registration order. The last registered handler has the highest priority. Register your UI layer after your game layer so the UI can consume input events the game should not see.

Global events (not tied to any window) use the global handler chain:

```cpp
MW::registerGlobalEventHandler(handler);
MW::unregisterGlobalEventHandler(id);
```

## Event Reference

### Window Events

| Event | Fields | Notes |
|---|---|---|
| `MCloseRequestEvent` | empty | OS or user requested close. App controls destruction. |
| `MResizeEvent` | `MSize new_size` | Logical pixels |
| `MMoveEvent` | `MPoint new_pos` | Logical, virtual desktop space |
| `MFocusChangeEvent` | `bool focused` | Pushed on creation too, if the window gets initial focus |
| `MVisibilityChangeEvent` | `bool isVisible` | false when minimized |
| `MMonitorChangeEvent` | MMonitorID old_mon, MMonitorID new_mon | Resize your swapchain |

### Keyboard Events

| Event | Fields | Notes |
|---|---|---|
| `MKeyPressEvent` | `MMicroSec timestamp`, `MKey key`, `MMods mods` | |
| `MKeyReleaseEvent` | `MMicroSec timestamp`, `MKey key`, `MMods mods` | |
| `MCharEvent` | std::string input | UTF-8 text input. Includes Backspace and Enter — app decides meaning. |

### Mouse Events

| Event | Fields | Notes |
|---|---|---|
| `MMouseMoveEvent` | `MMicroSec timestamp`, `float dx`, `float dy` | |
| `MMouseButtonPressEvent` | `MMicroSec timestamp`, `MMouseButton button`, `MPoint pos`, `MMods mods` | |
| `MMouseButtonReleaseEvent` | `MMicroSec timestamp`, `MMouseButton button`, `MPoint pos`, `MMods mods` | |
| `MMouseScrollEvent` | `MMicroSec timestamp`, `float dx`, `float dy`, `MMods mods` | Normalised, 1.0 per notch |
| `MMouseEnterEvent` | `MMicroSec timestamp` | Cursor entered window area |
| `MMouseLeaveEvent` | `MMicroSec timestamp` | Cursor left window area |

### Touch Events

| Event               | Fields                                                                         | Notes                                        |
| ------------------- | ------------------------------------------------------------------------------ | -------------------------------------------- |
| `MTouchBeginEvent`  | `MMicroSec timestamp`, `MTouchID id`, `MPoint pos`                             | A new touch contact began.                   |
| `MTouchMoveEvent`   | `MMicroSec timestamp`, `MTouchID id`, `MPoint new_pos`, `float dx`, `float dy` | Position and delta since the previous event. |
| `MTouchEndEvent`    | `MMicroSec timestamp`, `MTouchID id`, `MPoint pos`                             | Touch contact ended normally.                |
| `MTouchCancelEvent` | `MMicroSec timestamp`, `MTouchID id`                                           | Touch was cancelled by the system.           |


### Stylus Events

| Event                | Fields                                                          | Notes                                           |
| -------------------- | --------------------------------------------------------------- | ----------------------------------------------- |
| `MStylusEnterEvent`  | `MMicroSec timestamp`, `MPoint pos`                             | Stylus entered hover range of the window.       |
| `MStylusHoverEvent`  | `MMicroSec timestamp`, `MPoint new_pos`, `float dx`, `float dy` | Stylus moved while hovering (not in contact).   |
| `MStylusLeaveEvent`  | `MMicroSec timestamp`, `MPoint pos`                             | Stylus left hover range.                        |
| `MStylusDownEvent`   | `MMicroSec timestamp`, `MPoint pos`                             | Stylus made contact with the surface.           |
| `MStylusMoveEvent`   | `MMicroSec timestamp`, `MPoint new_pos`, `float dx`, `float dy` | Stylus moved while in contact.                  |
| `MStylusUpEvent`     | `MMicroSec timestamp`, `MPoint pos`                             | Stylus contact ended.                           |
| `MStylusCancelEvent` | `MMicroSec timestamp`                                           | Stylus interaction was cancelled by the system. |

### Gamepad Events

| Event                        | Fields                                                                                          | Notes                                                            |
| ---------------------------- | ----------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| `MGamepadConnectedEvent`     | `MMicroSec timestamp`, `MGamepadSlot id`                                                        | A gamepad became available.                                      |
| `MGamepadDisconnectedEvent`  | `MMicroSec timestamp`, `MGamepadSlot id`                                                        | A gamepad was disconnected.                                      |
| `MGamepadButtonPressEvent`   | `MMicroSec timestamp`, `MGamepadSlot id`, `MGamepadButton button`                               |                                                                  |
| `MGamepadButtonReleaseEvent` | `MMicroSec timestamp`, `MGamepadSlot id`, `MGamepadButton button`                               |                                                                  |
| `MGamepadTriggerEvent`       | `MMicroSec timestamp`, `MGamepadSlot id`, `bool left`, `float new_val`, `float d`               | Trigger value changed. `left` selects the left or right trigger. |
| `MGamepadStickEvent`         | `MMicroSec timestamp`, `MGamepadSlot id`, `bool left`, `MStick new_val`, `float dx`, `float dy` | Stick position changed. `left` selects the left or right stick.  |


### Display events

| Event | Fields | Notes |
| --- | --- | --- |
| `MDisplaySettingChangeEvent` | `MMonitor old`, `MDisplayChangeFlags what`, `MMonitorID new_id` | Properties of a display changed (e.g. resolution, refresh rate, DPI, orientation). |
| `MDisplayConnectedEvent`     | `MMonitorID id`                                                 | A display was connected and enumerated.                                            |
| `MDisplayDisconnectedEvent`  | `MMonitorID id`                                                 | A display was disconnected and removed.                                            |

### Drag & Drop

Not implemented yet — no event types exist in `MEvent` for this. Planned for a future version.

## Ring Buffer & Coalescing

Events are stored in a fixed-size ring buffer. On overflow, the newest event is dropped.

High-frequency events of the same type are merged before the app sees them, but only when no other event type sits between them in the queue:

| Event | Policy |
|---|---|
| Visibility change | `Latest` — keep only the final state |
| Resize | `Latest` |
| Move | `Latest` |
| Focus change | `Latest` — keep only the final state |
| Char input | `Accumulate` — merge strings |
| Mouse move | `Latest` — keep only the final delta |
| Mouse scroll | `Accumulate` — sum dx/dy |
| Touch move | `Latest` position + `Accumulate` delta — new_pos replaced, dx/dy summed |
| Stylus move/hover | `Accumulate` — sum dx/dy (position also replaced with the latest) |
| Gamepad trigger/stick | `Accumulate` — sum dx/dy |
| Everything else | `None` — every event preserved |

`Visibility change`, `Resize`, `Move`, and `Focus change` always coalesce unconditionally. Every other configurable row above is gated by a plain on/off flag in `MInitConfig` — there is no per-event-type merge *strategy* setting; the strategy itself (replace vs. accumulate) is fixed in code, only whether coalescing happens at all is configurable:

```cpp
MW::MInitConfig config;
config.eventQueueCapacity = 512;   // must be power of 2
config.mouseMoveCoalescing = true;
config.scrollCoalescing = true;
config.touchMoveCoalescing = true;
config.stylusMoveCoalescing = true;
config.gamepadCoalescing = true;
MW::init(config);
```

## Timestamps

All input events carry `MMicroSec time` — microseconds since `MW::init()` measured via a monotonic clock. Gamepad events share one timestamp per frame (sampled at poll time).

Use timestamps for double-click detection, gesture velocity, key repeat rate — cases where the interval between events matters more than when your handler ran.
