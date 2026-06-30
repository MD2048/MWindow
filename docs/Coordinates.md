# Coordinate System

## The Rule

Everything in MWindow uses **logical pixels** with one exception: `getPhysicalSize()` returns physical pixels because GPU APIs require them.

| Thing | Unit |
|---|---|
| Window size (`getSize()`) | Logical |
| Window position (`getTopLeftCorner()`) | Logical |
| Event positions | Logical |
| Monitor layout | Logical |
| Cursor position (`MW::getCursorPos()`) | Logical |
| `getPhysicalSize()` | Physical — pass to swapchain |

## Definition

```
1 logical pixel = 1/96th of an inch

dpiScale = dpi / 96.0f
physical = logical * dpiScale
logical  = physical / dpiScale
```

On a 96 DPI screen: 1 logical = 1 physical.
On a 192 DPI Retina screen: 1 logical = 2 physical.

## Virtual Desktop Space

All monitors are mapped into one unified 2D logical coordinate space. X increases right, Y increases down. The primary monitor's top-left corner is `(0, 0)`.

Example:
- Monitor A: 1920×1080 physical @ 1x DPI → occupies `(0,0)–(1920,1080)` logical
- Monitor B: 3840×2160 physical @ 2x DPI → occupies `(1920,0)–(3840,1080)` logical

Both monitors present the same logical height. The cursor moves the same visual distance per logical unit on both.

## DPI Changes

When a window moves to a monitor, `MMonitorChangeEvent` fires:

```cpp
struct MMonitorChangeEvent { MMonitorID old_mon; MMonitorID new_mon; };
```

If the the monitors `.dpiScale` isn't the same resize your swapchain to the new physical size:

```cpp
uint32_t newWidthPx  = window->getPhysicalSize().width;
uint32_t newHeightPx = window->getPhysicalSize().height;
// recreate or resize swapchain
```

The window's logical size does not change on a DPI change — only the scale factor changes.

## Multi-Monitor DPI Policy

When a window spans two monitors with different DPI scales, the **dominant monitor** wins — whichever monitor contains the majority of the window's area. The window has exactly one DPI scale at any given time.

## Platform Notes

MWindow handles all coordinate translation internally. You always receive logical coordinates regardless of platform.
