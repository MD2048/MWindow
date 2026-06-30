# MWindow — Windows Implementation Design Document

This document captures every design decision made during the Windows (Win32) implementation of MWindow.

---

## 1. Overview

MWindow is a thin, cross-platform C++17 window abstraction. It creates windows, receives input events, and hands native surface handles to a renderer. It does not render anything itself.

**Namespace:** `MW`  
**Main class:** `MW::MWindow` (pure virtual)  
**Concrete Win32 class:** `MWindowImpl : public MW::MWindow`  
**Standard:** C++17  
**Build:** CMake 3.24+

---

## 2. Public API (Final — source of truth)

```cpp
namespace MW {

    void init(const MInitConfig& config = {});
    void shutdown();

    // Drains the event queue, walks each event through registered handler chains
    void poll();

    MEventHandlerID registerGlobalEventHandler(MEventHandler ha);
    void            unregisterGlobalEventHandler(MEventHandlerID id);

    std::vector<MMonitor>   getConnectedMonitors();
    std::optional<MMonitor> getMonitor(MMonitorID id);
    std::optional<MMonitor> getPrimaryMonitor();
    bool                    isMonitorConnected(MMonitorID id);

    bool isGamepadSlotActive(MGamepadSlot id);
    std::vector<MGamepadSlot> getActiveGamepadSlots();

    MPoint getCursorPos();
    MMods getMods();
    bool isKeyHeld(MKey key);

    const std::vector<MDeviceState>& getInputState(); // MUST re-query per frame !!!
    
    class MWindow {
    public:
        void executeHandlerChain(const MEvent& ev); // Don't call this!

        MEventHandlerID registerEventHandler(MEventHandler ha);
        void            unregisterEventHandler(MEventHandlerID id);

        virtual ~MWindow() = default;

        virtual MWindowID getId() const = 0;

        virtual void show()  = 0;
        virtual void hide()  = 0;
        virtual void close() = 0;

        [[nodiscard]] virtual bool isAlive() const = 0;
        [[nodiscard]] virtual bool isVisible() const = 0;  // false when minimized too

        virtual void               setTitle(const std::string& title) = 0;
        virtual const std::string& getTitle() const = 0;
        
        // Every coordinate in logical desktop space

        virtual void     resize(MSize sz) = 0;
        virtual MSize getSize()  const = 0;

        virtual void  setTopLeftCorner(MPoint p) = 0;
        virtual MPoint getTopLeftCorner() const = 0;

        virtual MRect getRect() const = 0;

        virtual void        setWindowMode(MWindowMode mode) = 0;
        virtual MWindowMode getWindowMode() const = 0;

        virtual MMonitorID getCurrentMonitorID() const = 0;

        virtual float getDpiScale() const = 0;

        // Physical pixels - cast to an integer type
        virtual MSize getPhysicalSize() const = 0;

        virtual MNativeWindow getNativeWindow() const = 0;

        [[nodiscard]] static std::unique_ptr<MWindow> create(const MWindowDesc& desc);
    };
}
```

---

## 3. Coordinate System

### Rule
Everything in MWindow uses **logical pixels** except `getPhysicalSize()` which returns physical pixels for GPU APIs.

### Logical pixel definition
A logical pixel = 1/96th of an inch. On a 96 DPI screen: 1 logical = 1 physical. On a 192 DPI screen: 1 logical = 2 physical.

```
dpiScale = dpi / 96.f
physical = logical * dpiScale
logical  = physical / dpiScale
```

### Win32 PerMonitorV2 coordinate rules
**Everything Win32 gives you and everything you pass to Win32 is physical pixels. There are no logical coordinates anywhere in the Win32 API.** MWindow translates at every boundary.

| API | Coordinates |
|---|---|
| `CreateWindowExW` x,y,w,h | All physical |
| `SetWindowPos` x,y,w,h | All physical |
| `GetWindowRect` | Physical |
| `GetClientRect` | Physical |
| `WM_SIZE` lParam w,h | Physical |
| `WM_MOVE` lParam x,y | Physical |
| `WM_MOUSEMOVE` lParam | Physical |
| `GetCursorPos` | Physical always |
| `WM_DPICHANGED` lParam RECT | Physical |
| `AdjustWindowRectExForDpi` | Physical in, physical out |

**Translation pattern:**
```
App (logical) → multiply by dpiScale → Win32 (physical)
Win32 (physical) → divide by dpiScale → App (logical)
```

### Virtual desktop space
All monitors mapped into one unified 2D logical coordinate space. X right, Y down. Primary monitor top-left = (0,0).

---

## 4. Architecture

### MGlobal — the hidden singleton
Never exposed publicly. Owns:
- Event queue (ring buffer)
- Monitor list
- Global input state (keyboard bits, cursor pos, mouse buttons)
- Touch state
- Stylus state
- Gamepad device list
- Notification window HWND
- Window entry list

### MWindowEntry — triple identity
```cpp
struct MWindowEntry {
    MWindowID    id;      // for the user / event tagging
    HWND         hwnd;    // for WndProc lookup
    MWindowImpl* window;  // for interacting with the instance
};

std::vector<MWindowEntry> windows; // flat vector, linear search — N is tiny
```

Lookup helpers on MGlobal:
```cpp
std::optional<MWindowID> idFromHWND(void* hwnd);
std::optional<MWindowImpl*> ptrFromHWND(void* hwnd);
std::optional<MWindowImpl*> ptrFromID(MWindowID id);
std::optional<MWindowID> getFocusedID();
```

`GWLP_USERDATA` stores `MGlobal*` on every HWND (both notification window and real windows).

---

## 5. Init Sequence

```cpp
// Not the real code
void MGlobal::init(const MInitConfig& config) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); // Tells Windows
                                                                               // not to lie about DPI
    EnableMouseInPointer(FALSE);        // touchscreen → WM_POINTER only, no mouse synthesis
                                        // touchpad   → Windows mouse emulation (correct)
    initClock();
    createNotificationWindow();         // Never shown, just to receive messages
    registerRawInputDevices();          // arm BEFORE enumerating
    enumerateMonitors(ptr->monitors);
    if(!ignoreGamepads)
        decideGamepadBackend();         // GameInput or XInput fallback

    setup_finished = true;
}
```

### registerRawInput
```cpp
void MGlobal::registerRawInput() {
    RAWINPUTDEVICE rids[2]{};

    // Keyboard
    rids[0].usUsagePage = 0x01; // HID Generic Desktop
    rids[0].usUsage     = 0x06; // Keyboard
    rids[0].dwFlags     = RIDEV_INPUTSINK;
    rids[0].hwndTarget  = reinterpret_cast<HWND>(notificationHWND);

    // Mouse
    rids[1].usUsagePage = 0x01;
    rids[1].usUsage     = 0x02; // Mouse
    rids[1].dwFlags     = RIDEV_INPUTSINK;
    rids[1].hwndTarget  = reinterpret_cast<HWND>(notificationHWND);

    RegisterRawInputDevices(rids, 2, sizeof(RAWINPUTDEVICE));
}
```
This registers **interest in device classes**, not individual devices. Covers all current and future keyboards/mice automatically. `RIDEV_DEVNOTIFY` sends `WM_INPUT_DEVICE_CHANGE` — but we ignore it for keyboards/mice since we don't track them individually anymore.

---

## 6. Two WndProcs

### NotificationWndProc
Handles global state. Receives Raw Input via `RIDEV_INPUTSINK`.

```
WM_INPUT                →  keyboard + mouse events
WM_INPUT_DEVICE_CHANGE  →  ignored for keyboard/mouse; not used
WM_DISPLAYCHANGE        →  re-enumerate monitors
WM_DEVICECHANGE         →  re-enumerate monitors (hotplug)
```

### WindowWndProc
Handles per-window state. Registered for all real windows.

```
WM_CLOSE              →  push MCloseRequestEvent only, do NOT DestroyWindow
WM_DESTROY            →  cleanup state, remove from global list
WM_SIZE               →  update physicalSize, logicalSize, visible flag
WM_MOVE               →  update topLeft
WM_WINDOWPOSCHANGED   →  update monitorId; always call DefWindowProc
WM_DPICHANGED         →  update dpiScale, SetWindowPos with suggested rect
WM_SETFOCUS           →  push MFocusChangeEvent{true}
WM_KILLFOCUS          →  push MFocusChangeEvent{false}
WM_SHOWWINDOW         →  update visible state
WM_ERASEBKGND         →  return 1 (prevent GDI flicker)
WM_MOUSEMOVE          →  mouse enter/leave tracking + touchpad input
WM_MOUSELEAVE         →  push MMouseLeaveEvent
WM_CHAR               →  push MCharEvent (UTF-8)
WM_SYSCHAR            →  DefWindowProc only
WM_POINTERDOWN        →  touch begin / stylus down
WM_POINTERUPDATE      →  touch move / stylus move / stylus hover
WM_POINTERUP          →  touch end / stylus up
WM_POINTERENTER       →  stylus enter hover range
WM_POINTERLEAVE       →  stylus leave hover range
WM_POINTERCAPTURECHANGED → touch cancel / stylus cancel
```

---

## 7. State Caching

**All getters read from cached state. WndProc updates the cache. No syscalls at read time.**

```cpp
struct MWindowState {
    bool focused = false;
    DWORD windowStyle = 0;      // saved before entering fullscreen
    RECT currentRect{};         // avoids comparing floats
    RECT preFullscreenRect{};   // not adjusted for physical DPI
                                // exstyle is always WS_EX_APPWINDOW
    MWindowDesc desc{};
};
struct MWindowDesc {
    std::string      title   = "MWindow";
    MRect            rect    = {100,100,800,600};
    MWindowMode      mode    = MWindowMode::Windowed;

    bool resizable  = true;     // best effort
    bool decorated  = true;     // best effort
    bool visible    = true;

    bool centered   = true;

    MMonitorID monitor;
};
```

WndProc update examples:
```cpp
case WM_SIZE: {
    RECT& r {state->currentRect};
    LONG w = LOWORD(lParam);
    LONG h = HIWORD(lParam);

    if(!(r.right-r.left == w && r.bottom-r.top == h))
    {
        r.right += w - (r.right - r.left);
        r.bottom += h - (r.bottom - r.top);

        MMonitor new_mon = global->monitorFromHandle(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)).value();
        if(new_mon.id != state->desc.monitor)
        {
            global->push(MMonitorChangeEvent{state->desc.monitor,new_mon.id}, hwnd);
            state->desc.monitor = new_mon.id;
            mw->setStateChange();
        }
        state->desc.rect.width = (float)w / new_mon.dpiScale;
        state->desc.rect.height = (float)h / new_mon.dpiScale;
        mw->setStateChange();
        global->push(MResizeEvent{state->desc.rect.size()}, hwnd);
    }
}
case WM_DPICHANGED:
{
    float dpiScale = (float)LOWORD(wParam) / 96.0f;
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);

    SetWindowPos(hwnd, nullptr, 
        suggested->left, suggested->top,
        suggested->right - suggested->left,
        suggested->bottom - suggested->top,
        SWP_NOZORDER | SWP_NOACTIVATE
    );
    return 1;
}
```

**Important:** Always call `DefWindowProc` for `WM_WINDOWPOSCHANGED` — it synthesizes `WM_SIZE` and `WM_MOVE` internally.

---

## 8. Window Lifecycle & Destruction

### close() — queues the kill request, which is consumed at the next poll()
```cpp
void MWindowImpl::close() {
    alive.store(false,std::memory_order_release); // poll() checks alive
}

MWindowImpl::~MWindowImpl() {                     // safety net if unique_ptr goes out of scope
    alive.store(false,std::memory_order_release);
    if(hwnd)
    {
        global->unregisterWindow(id);
        DestroyWindow(hwnd);
    }
    hwnd = nullptr;
}
```

### WM_CLOSE — notification only, never destroy
```cpp
case WM_CLOSE:
    global->pushEvent(entry->id, MCloseRequestEvent{});
    return 0; // do NOT call DefWindowProc — that calls DestroyWindow
```
The app decides whether to call `close()` after receiving `MCloseRequestEvent`. This is how "confirm quit" dialogs work.

### Message lifecycle
```
WM_CLOSE    → interception point, HWND still valid, interruptible
WM_DESTROY  → point of no return, HWND valid but dying
WM_NCDESTROY → HWND fully invalid, never use after this
```

### Window mode transitions
Both `Fullscreen` and `BorderlessFullscreen` are borderless at the MWindow level — exclusive fullscreen (mode switch) is the renderer's concern. Save/restore `GWL_STYLE` and window rect when transitioning.

---

## 9. Window Creation

```cpp
// scale is an assumption based on the top left of the window
// after there is a valid HWND the current monitor is queryable from the OS

HWND hw = CreateWindowExW(
    WS_EX_APPWINDOW,
    L"MWindow",
    toWide(desc.title).c_str(),
    curStyle,
    static_cast<int>(state2.desc.rect.x*scale),         // convert to physical
    static_cast<int>(state2.desc.rect.y*scale),
    static_cast<int>(state2.desc.rect.width*scale),
    static_cast<int>(state2.desc.rect.height*scale),
    nullptr,
    nullptr,
    GetModuleHandle(nullptr),
    nullptr
);

// Store MGlobal* for WndProc lookup
SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(global));

// Initialize state from desc BEFORE any getters are called
// Recalculate DPI, center window
```

---

## 10. Monitor System

### MMonitor struct
```cpp
using MMonitorID = uint64_t;

struct MVideoMode {
    uint32_t widthPx;
    uint32_t heightPx;
    uint32_t refreshRate;
    uint32_t bitsPerChannel;  // typically 8, 10, or 16
};

enum class MColorGamut {
    Unknown,
    SRGB,       // standard, ~99% of monitors
    DCI_P3,     // wide gamut, most Apple displays
    Rec2020,    // ultra wide, high-end HDR monitors
};

struct MHDRInfo {
    bool         supported;
    bool         active;         // HDR currently enabled in OS settings
    float        maxLuminance;   // peak brightness in nits
    float        minLuminance;   // black level in nits
    MColorGamut  colorGamut;
};

struct MMonitor {
    MMonitorID  id;      // assigned by MWindow
    std::string name;

    MRect    rect;       // logical virtual desktop space
    float    dpiScale;
    bool     isPrimary;

    std::size_t             currentModeIndex;
    std::vector<MVideoMode> availableModes;

    MHDRInfo hdr;
};
```

### Enumeration
- `EnumDisplayMonitors` at init and on every `WM_DISPLAYCHANGE` / `WM_DEVICECHANGE`
- `EnumDisplaySettingsW(ENUM_CURRENT_SETTINGS)` for current mode
- `EnumDisplaySettingsW(i)` loop for available modes — deduplicate on all 4 fields
- `GetDpiForMonitor(MDT_EFFECTIVE_DPI)` for DPI
- `dmBitsPerPel`: 32→8bpc, 30→10bpc, 48→16bpc
- HDR capability from `IDXGIOutput6::GetDesc1`
- HDR active state from `DisplayConfigGetDeviceInfo(DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO)`
- DCI-P3 detected via red primary x coordinate > 0.670 (no DXGI constant exists)

### Change detection
After re-enumerating, diff against previous list. **Cannot rely on ordering** — always N×N match by `name`. Multiple monitors can change in one event — always scan the full list, never break early.

```cpp
struct MDisplayChangeFlags {
    bool rect        : 1;  // monitor moved in virtual desktop
    bool scale       : 1;  // DPId change
    bool resolution  : 1;  // widthPx / heightPx changed
    bool refreshRate : 1;  // refresh rate changed
    bool bitDepth    : 1;  // bitsPerChannel changed
    bool hdrState    : 1;  // HDR enabled/disabled
};

struct MDisplaySettingChangeEvent {
    MMonitor old;
    MDisplayChangeFlags what;
    MMonitorID new_id;
};
```

---

## 11. Event System

### Ring buffer
```cpp
struct MEventSlot {
    MEvent event = std::monostate{};
    bool global  = false;
    MWindowID id = std::numeric_limits<uint64_t>::max();
};
// Members of MGlobal
std::size_t mask;
std::size_t head;
std::size_t tail;
std::unique_ptr<MEventSlot[]> buffer;

```

`std::monostate` is the first type in `MEvent` variant — makes it default-constructible and serves as the "empty slot" sentinel.

**Overflow policy:** drop newest.

### Coalescing
```cpp             
bool shouldCoalesce(const MEvent& a);                                  // std::visit - determines if the event type is coalescable
bool canCoalesce(const MEventSlot& a, const MEventSlot& b);            // std::visit - determines if 2 events are coalescable
size_t findCoalescableEventIndex(const MEventSlot& ev, void* hwnd);    // finds the right index for coalescing
void coalesceEvent(size_t index, const MEvent& ev);                    // std::visit - coalescs
```

| Event | Policy | Notes |
|---|---|---|
| `MResizeEvent` | Replace | latest size wins |
| `MMoveEvent` | Replace | latest position wins |
| `MVisibilityChangeEvent` | Replace | latest state wins |
| `MFocusChangeEvent` | Replace | latest state wins |
| `MCharEvent` | Accumulate | all in one string |
| `MMouseMoveEvent` | Accumulate dx/dy | configurable |
| `MMouseScrollEvent` | Accumulate dx/dy | configurable |
| `MTouchMoveEvent` | Accumulate dx/dy, keyed on touchId | configurable |
| `MStylusHoverEvent` | Accumulate dx/dy | configurable |
| `MStylusMoveEvent` | Accumulate dx/dy | configurable | 
| `MGamepadTriggerEvent` | Accumulate dx/dy | configurable |
| `MGamepadStickEvent` | Accumulate dx/dy | configurable |
| All others | Never | every event preserved |

### Configurable coalescing
```cpp
struct MInitConfig {
    std::size_t     eventQueueCapacity = 256;  // must be power of 2
    bool ignoreGamepads         = false;
    bool mouseMoveCoalescing    = true;
    bool scrollCoalescing       = true;
    bool touchMoveCoalescing    = true;
    bool stylusMoveCoalescing   = true;
    bool gamepadCoalescing      = true;
};
```

### poll()
```cpp
void MW::poll() {
    // 1. Consume state request from previous frame
    for(auto& entry : windows)
    {
        entry.window->handleStateRequests();
    }
    // 2. Snapshot cursor position — one syscall, used all frame
    POINT p{};
    GetCursorPos(&p);
    HANDLE hMon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
    float scale = dpiFromHandle(hMon);
    auto* state = getBackStatePtr();
    auto& msst = std::get<MMouseState>((*state)[(size_t)MW::MDeviceType::Mouse]);
    msst.p.x = (float)p.x / scale;
    msst.p.y = (float)p.y / scale;

    // 3. Drain OS messages
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // 4. Poll gamepads
    if(!settings.ignoreGamepads)
    {
        if(usingXInput)
            getGamepadInputX();
        else
            getGamepadInputG();
    }
    // 5. Switch buffers, and flush events through the handler chains
    switchBuffers();
    consumeAll();
}
```

---

## 12. Event Types

```cpp
using MEvent = std::variant <
    std::monostate,

    // Visibibility
    MVisibilityChangeEvent, // coalescable
    // Window
    MCloseRequestEvent,
    MResizeEvent,           // coalescable
    MMoveEvent,             // coalescable
    MFocusChangeEvent,      // coalescable
    MMonitorChangeEvent,
    // Keyboard
    MKeyPressEvent,
    MKeyReleaseEvent,
    MCharEvent,             // coaleascable
    // Mouse
    MMouseMoveEvent,        // coalescable
    MMouseButtonPressEvent,
    MMouseButtonReleaseEvent,
    MMouseScrollEvent,      // coalescable
    MMouseEnterEvent,
    MMouseLeaveEvent,
    // Touch
    MTouchBeginEvent,
    MTouchMoveEvent,        // coalescable
    MTouchEndEvent,
    MTouchCancelEvent,
    // Gamepad
    MGamepadConnectedEvent,
    MGamepadDisconnectedEvent,
    MGamepadButtonPressEvent,
    MGamepadButtonReleaseEvent,
    MGamepadTriggerEvent,
    MGamepadStickEvent,
    // Stylus
    MStylusEnterEvent,
    MStylusHoverEvent,      // coalescable
    MStylusLeaveEvent,
    MStylusDownEvent,
    MStylusMoveEvent,       // coalescable
    MStylusUpEvent,
    MStylusCancelEvent,
    // Monitors
    MDisplaySettingChangeEvent,
    MDisplayConnectedEvent,
    MDisplayDisconnectedEvent
>;
```

### Event structs
```cpp
using MTouchID  = uint64_t;
using MMicroSec = uint64_t;
// Visibility
struct MVisibilityChangeEvent { bool isVisible; };

// Window
struct MCloseRequestEvent {};
struct MResizeEvent { MSize new_size; };
struct MMoveEvent { MPoint new_pos; };
struct MFocusChangeEvent { bool focused; };
struct MMonitorChangeEvent { MMonitorID old_mon; MMonitorID new_id; };

// Keyboard
struct MKeyPressEvent { MMicroSec timestamp; MKey key; MMods mods; };
struct MKeyReleaseEvent { MMicroSec timestamp; MKey key; MMods mods; };
struct MCharEvent { std::string input; };

// Mouse
struct MMouseMoveEvent { MMicroSec timestamp; float dx, dy; };
struct MMouseButtonPressEvent { MMicroSec timestamp; MMouseButton button; MPoint pos; MMods mods; };
struct MMouseButtonReleaseEvent { MMicroSec timestamp; MMouseButton button; MPoint pos; MMods mods; };
struct MMouseScrollEvent { MMicroSec timestamp; float dx; float dy; MMods mods; };
struct MMouseEnterEvent { MMicroSec timestamp; };
struct MMouseLeaveEvent { MMicroSec timestamp; };

// Touch
struct MTouchBeginEvent { MMicroSec timestamp; MTouchID id; MPoint pos; };
struct MTouchMoveEvent { MMicroSec timestamp; MTouchID id; MPoint new_pos; float dx; float dy; };
struct MTouchEndEvent { MMicroSec timestamp; MTouchID id; MPoint pos; };
struct MTouchCancelEvent { MMicroSec timestamp; MTouchID id; };

// Stylus
struct MStylusEnterEvent { MMicroSec timestamp; MPoint pos; };
struct MStylusHoverEvent { MMicroSec timestamp; MPoint new_pos; float dx, dy; };
struct MStylusLeaveEvent { MMicroSec timestamp; MPoint pos; };
struct MStylusDownEvent { MMicroSec timestamp; MPoint pos; };
struct MStylusMoveEvent { MMicroSec timestamp; MPoint new_pos; float dx, dy; };
struct MStylusUpEvent { MMicroSec timestamp; MPoint pos; };
struct MStylusCancelEvent { MMicroSec timestamp; };

// Gamepad
struct MGamepadConnectedEvent { MMicroSec timestamp; MGamepadSlot id; };
struct MGamepadDisconnectedEvent { MMicroSec timestamp; MGamepadSlot id; };
struct MGamepadButtonPressEvent { MMicroSec timestamp; MGamepadSlot id; MGamepadButton button; };
struct MGamepadButtonReleaseEvent { MMicroSec timestamp; MGamepadSlot id; MGamepadButton button; };
struct MGamepadTriggerEvent { MMicroSec timestamp; MGamepadSlot id; bool left; float new_val; float d; };
struct MGamepadStickEvent { MMicroSec timestamp; MGamepadSlot id; bool left; MStick new_val; float dx, dy; };

// Monitors
struct MDisplaySettingChangeEvent { MMonitor old; MDisplayChangeFlags what; MMonitorID new_id; };
struct MDisplayConnectedEvent { MMonitorID id; };
struct MDisplayDisconnectedEvent { MMonitorID id; };
```

### Cursor position
`getCursorPos()` returns the cached value from `poll()` — no syscall. All button events use this same per-frame snapshot. This is acceptable because high-precision mice pair with high FPS.

---

## 13. Input Device Philosophy

**Keyboards and mice are merged — no per-device tracking.**

This matches GLFW, SDL, and every other windowing lib. Per-device keyboard/mouse distinction is impractical due to Win32 Raw Input noise (audio HID, webcams, RDP virtual devices, etc.).

**Only track connected gamepads.** MWindow exposes slots for gamepads, not IDs.

### Global input state
MGlobal uses double buffering. The state is stored in two ```std::vector<MDeviceState>```
```cpp
using MGamepadSlot = uint64_t;

enum class MDeviceType { Unknown = -1, Keyboard, Mouse, Touchscreen, Stylus, Gamepad };

struct MKeyboardState { std::bitset<static_cast<size_t>(MKey::Count)> held; MMods mods; };
struct MMouseState { MPoint p; std::bitset<static_cast<size_t>(MMouseButton::Count)> buttons; };
struct MTouchState { std::unordered_map<uint32_t, MPoint> activePoints; }; // touch id → point
struct MGamepadState {
    bool connected = false;
    std::bitset<static_cast<size_t>(MGamepadButton::Count)> held;
    MStick left;        // -1.f to 1.f
    MStick right;
    float leftTrigger = 0.f;  // 0.f to 1.f
    float rightTrigger = 0.f;
};
struct MStylusState { bool inRange = false; bool inContact = false; MPoint pos = {0, 0}; };

using MDeviceState = std::variant<MKeyboardState, MMouseState, MTouchState, MGamepadState, MStylusState>;
```
These vectors are indexed by the MDeviceType enum: so the first element(index 0) is ```MKeyboardState```, the second is ```MMouseState```.
The fifth, sixth, etc. are ```MGamepadState```. To get the gamepad's state in ```MGamepadSlot``` 1, write: 
```cpp
MGamepadSlot index = 1;
const std::vector<MDeviceState>& inputVec = getInputState(); // you must requery this vector, in every frame!
MGamepadState& gpst = std::get<MGamepadState>(inputVec[static_cast<size_t(MDeviceType::Gamepad)> + index]);
```

`isKeyHeld(MKey)` = single bitset lookup on this global state.

---

## 14. Raw Input (Keyboard + Mouse)

Raw Input goes to the **notification window** via `RIDEV_INPUTSINK`.

### Discard rule
```cpp
// RIM_TYPEHID — discard entirely
// Touch/stylus come from WM_POINTER, gamepads from GameInput/XInput
if (raw->header.dwType == RIM_TYPEHID) return 0;
```

### Keyboard — MakeCode + E0 flag → MKey
```cpp
// PS/2 scan code translation
// index = makeCode | (E0 ? 0x100 : 0)
// Flat array of 512 entries, built once at init
static MKey sScanToMKey[512];

MKey translateMakeCode(USHORT makeCode, bool e0) {
    uint32_t index = makeCode | (e0 ? 0x100u : 0u);
    return (index < 512) ? sScanToMKey[index] : MKey::Unknown;
}

// Special case — Pause uses RI_KEY_E1, handle before table lookup
if (kb.Flags & RI_KEY_E1) 
    MKey key = MKey::Pause;

// L/R modifier resolution
USHORT vk = kb.VKey;
if (vk == VK_SHIFT)   vk = (kb.MakeCode == 0x2A) ? VK_LSHIFT   : VK_RSHIFT;
if (vk == VK_CONTROL) vk = (kb.Flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
if (vk == VK_MENU)    vk = (kb.Flags & RI_KEY_E0) ? VK_RMENU    : VK_LMENU;

// Key pressed = !(kb.Flags & RI_KEY_BREAK)
```

### Mouse — absolute position from GetCursorPos, not Raw Input
Raw Input's strength is relative deltas. Absolute position always comes from the `GetCursorPos` snapshot taken at `poll()` start.

```cpp
// Relative delta — what Raw Input does well
if (!(ms.usFlags & MOUSE_MOVE_ABSOLUTE)) {
    float scale = global->monitorFromPoint(msst.p).dpiScale;

    dx = ms.lLastX / scale;
    dy = ms.lLastY / scale;
}
// MOUSE_MOVE_ABSOLUTE — leave dx/dy as 0, these devices go through WM_POINTER

// Buttons
// usButtonFlags bitmask — multiple buttons can change in one message
// RI_MOUSE_LEFT_BUTTON_DOWN / UP, RIGHT, MIDDLE, BUTTON_4, BUTTON_5

// Scroll
// RI_MOUSE_WHEEL  → dy = (short)usButtonData / WHEEL_DELTA
// RI_MOUSE_HWHEEL → dx = (short)usButtonData / WHEEL_DELTA
```

---

## 15. WM_CHAR → MCharEvent

`TranslateMessage` in `poll()` generates `WM_CHAR` from `WM_KEYDOWN`. Goes to focused window WndProc.

```
- Filter only null character (0x00)
- Keep Backspace, Tab, Enter, Escape — they are text editing intent
- Handle surrogate pairs: buffer high surrogate, combine with low surrogate
- Convert UTF-16 → UTF-8 via WideCharToMultiByte(CP_UTF8)
- WM_SYSCHAR → DefWindowProc only, no MCharEvent
```

`MWindowImpl` member: `wchar_t pendingSurrogate = 0`

---

## 16. WM_POINTER (Touch + Stylus)

Goes to focused **window WndProc**. `EnableMouseInPointer(FALSE)` ensures touchscreen does not also generate synthetic mouse messages.

### Device type filtering
```cpp
// Discard PT_MOUSE — handled via Raw Input
if (pi.pointerType == PT_MOUSE) return DefWindowProcW(hwnd, msg, wParam, lParam);
// PT_TOUCH  → touch path
// PT_PEN    → stylus path (PT_STYLUS constant does not exist in public SDK)
```

### Always check GetPointerInfo return value
Pointer ID can be stale if message was queued after pointer was cancelled.

### Touch state
```cpp
using MTouchID  = uint64_t;
struct MTouchState { std::unordered_map<MTouchID, MPoint> activePoints; };
```

### Stylus state
```cpp
struct MStylusState { bool inRange = false; bool inContact = false; MPoint pos = {0, 0}; };
```

One active stylus assumed — no map needed.

`POINTER_FLAG_INCONTACT` in `WM_POINTERUPDATE` distinguishes `MStylusMoveEvent` (in contact) from `MStylusHoverEvent` (hovering).

### Touchpad as mouse
Precision touchpads report as mouse with `hDevice = nullptr` in Raw Input. Windows emulates the touchpad as a regular mouse, which is the correct behavior. No special touchpad handling needed. This matches GLFW and SDL.
Touchpad clicks also handled in WM_INPUT.

---

## 17. Mouse Enter/Leave

Handled in window WndProc from `WM_MOUSEMOVE` / `WM_MOUSELEAVE`.

```cpp
// MGlobal member
bool mouseTracking = false;

case WM_MOUSEMOVE: {
    if (!global->mouseTracking) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);

        global->mouseTracking = true;
        global->push(MMouseEnterEvent{}, hwnd);
    }
    return 0;
}
case WM_MOUSELEAVE: {
    global->mouseTracking = false;
    global->push(MMouseLeaveEvent{}, hwnd);
    return 0;
}
```

---

## 18. MKey Enum

Indexed sequentially from 0. Used as bitset index in `MGlobalInputState::keysHeld`.

```cpp
enum class MKey : uint32_t {
    Unknown = 0,

    // Alphanumeric
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4,
    Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1,  F2,  F3,  F4,  F5,  F6,
    F7,  F8,  F9,  F10, F11, F12,
    F13, F14, F15, F16, F17, F18,  // F13–F24 appear on extended keyboards
    F19, F20, F21, F22, F23, F24, 

    // Modifier
    LeftShift,  RightShift,
    LeftCtrl,   RightCtrl,
    LeftAlt,    RightAlt,   // RightAlt = AltGr on European keyboards 
    LeftGui,    RightGui,   // Windows key / Cmd key

    // Control & editing
    Escape,
    Tab,
    CapsLock,
    Enter,
    Backspace,
    Space,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,

    // Arrow key
    Up,
    Down,
    Left,
    Right,

    // Lock / system
    NumLock,
    ScrollLock,
    PrintScreen,
    Pause,          // Arrives with RI_KEY_E1 flag — unique among all keys
    App,            // Application/Menu key — right of RightGui, opens context menu

    // Media key
    VolumeUp,       // Dedicated key on the top-right cluster of some keyboards
    VolumeDown,
    Mute,

    // Punctuation (position-based, US layout names)
    Grave,          // ` ~
    Minus,          // - _
    Equal,          // = +
    LeftBracket,    // [ {
    RightBracket,   // ] }
    Backslash,      // \ |   present on all ANSI and ISO keyboards
    Semicolon,      // ; :
    Apostrophe,     // ' "
    Comma,          // , 
    Period,         // . >
    Slash,          // / ?

    // ISO-only keys (absent on ANSI/US keyboards)─
    NonUSBackslash, // Extra key between LeftShift and Z on ISO keyboards.
                    // Typically < > on most European layouts, \ | on some others.
                    // Same MakeCode as Backslash but carries RI_KEY_E0.

    NonUSHash,      // Extra key between Apostrophe and Enter on ISO keyboards.
                    // Typically # ~ on UK layout.
                    // The larger ISO Enter key fills the space this occupies on ANSI.

    // Japanese layout keys (JIS keyboards only)
    Yen, Ro, Muhenkan,
    Henkan, KatakanaHiragana, 

    // Numpad
    KP0, KP1, KP2, KP3, KP4,
    KP5, KP6, KP7, KP8, KP9,
    KPDecimal,      // .  (comma on Brazilian and some European layouts — see KPComma)
    KPDivide,       // /  shares MakeCode with main Slash, distinguished by RI_KEY_E0
    KPMultiply,     // *
    KPSubtract,     // -
    KPAdd,          // +
    KPEnter,        // shares MakeCode with main Enter, distinguished by RI_KEY_E0
    KPEqual,        // =  found on Mac keyboards and some extended layouts
    KPComma,        // ,  used as decimal separator on Brazilian/some European numpad layouts

    Count
};
```

### PS/2 MakeCode → MKey translation table
```
index = makeCode | (E0 ? 0x100 : 0)
Array: static MKey sScanToMKey[512]  — built once at MGlobal init
Pause: RI_KEY_E1 flag → MKey::Pause (before table lookup, only key with E1)
```

Key E0 disambiguations:
```
0x1C no E0 → Enter       0x1C E0 → KPEnter
0x1D no E0 → LeftCtrl    0x1D E0 → RightCtrl
0x35 no E0 → Slash       0x35 E0 → KPDivide
0x38 no E0 → LeftAlt     0x38 E0 → RightAlt
0x47 no E0 → KP7         0x47 E0 → Home
0x48 no E0 → KP8         0x48 E0 → Up
0x49 no E0 → KP9         0x49 E0 → PageUp
0x4B no E0 → KP4         0x4B E0 → Left
0x4D no E0 → KP6         0x4D E0 → Right
0x4F no E0 → KP1         0x4F E0 → End
0x50 no E0 → KP2         0x50 E0 → Down
0x51 no E0 → KP3         0x51 E0 → PageDown
0x52 no E0 → KP0         0x52 E0 → Insert
0x53 no E0 → KPDecimal   0x53 E0 → Delete
0x5B E0    → LeftGui
0x5C E0    → RightGui
0x5D E0    → App
```

---

## 19. MMods Struct

```cpp
struct MMods {
    bool shift : 1;
    bool ctrl  : 1;
    bool alt   : 1;
    bool super : 1;
    bool caps  : 1; // CapsLock ON
    bool num   : 1; // NumLock ON
    bool none() const { return !shift && !ctrl && !alt && !super; }
};
```

Left/right variants collapse into single flags.

**AltGr:** Win32 reports AltGr as `RightAlt + synthetic LeftCtrl`. The synthetic LeftCtrl arrives as a Raw Input message with the same timestamp. This is a known pain point currently it doesn't get decoupled, treat RightAlt as AltGr

---

## 20. Gamepad Input

### Backend selection
```cpp
void MGlobal::decideGamepadBackend() {
    HRESULT hr = GameInputCreate(&gameInput);
    if (SUCCEEDED(hr)) {
        gamepadsGI = std::vector<MGameInputEntry>(DEFAULT_GAMEPAD_COUNT);
        usingXInput = false;
        registerDeviceCallback();
    }
    else {
        gamepadsXI = std::vector<MXInputEntry>(DEFAULT_GAMEPAD_COUNT);
        usingXInput = true;
        checkConnectedGamepads();
    }
}
```

### MGamepadState
```cpp
enum class MGamepadButton : uint32_t {
    DpadUp, DpadDown, DpadLeft, DpadRight,
    ActionBottom, ActionRight, ActionLeft, ActionTop, // Xbox: A, B, X, Y  PS: Cross, Circle, Square, Triangle
    BumperLeft, BumperRight,
    ThumbLeft, ThumbRight, // Stick clicks
    Start, Select, Guide,
    Count
};

struct MGamepadState {
    bool connected = false;
    std::bitset<static_cast<size_t>(MGamepadButton::Count)> held;
    MStick left;        // -1.f to 1.f
    MStick right;
    float leftTrigger = 0.f;  // 0.f to 1.f
    float rightTrigger = 0.f;
};
```

Deadzone, normalization applied before storing (~0.1 radial deadzone). Polled in `MW::poll()` — not event-driven. XInput and GameInput are both polled on the main/creator thread, sequential with `PeekMessage` drain — fast enough, no async needed.

---

## 21. Timestamps

All event timestamps are `using MMicroSec = uint64_t;` since `MW::init()`.

```cpp

void MGlobal::initClock() { // called at startup
    LARGE_INTEGER temp;
    QueryPerformanceFrequency(&temp);
    qpcFreq = temp.QuadPart;

    QueryPerformanceCounter(&temp);
    qpcStart = temp.QuadPart;
}

MMicroSec MGlobal::getTimeNow() {
    LARGE_INTEGER l;
    QueryPerformanceCounter(&l);
    return (l.QuadPart - qpcStart) * microSecConstant / qpcFreq;
}
```

Normal events get the timestamp when they are consumed, all gamepad events share the same timestamp per frame.

---

## 22. Threading Rules

**All Win32 window manipulation must be called from the creator thread** (the thread that called `CreateWindowExW`). Win32 windows have thread affinity — the message queue belongs to the creator thread.
That is why all the state changing function(```resize()```, ```setWindowMode()```) don't get executed immediately, just at the end of the frame.

## 23. DPI Awareness Setup

Called at process start (before any windows are created):
```cpp
SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
```

In PerMonitorV2 mode Windows does not scale or virtualize anything — MWindow handles all DPI translation.

---

## 24. AdjustWindowRectExForDpi

Used in `resize()` to expand logical client size to physical window size including chrome:

```cpp
RECT r{ 0, 0, (LONG)(sz.width * scale), (LONG)(sz.height * scale) };
AdjustWindowRectExForDpi(&r, style, FALSE, exStyle, GetDpiForWindow(hwnd));
// r now = physical outer window size
SetWindowPos(hwnd, nullptr, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOMOVE|...);
```

**Must use `GetDpiForWindow(hwnd)` directly** — the ForDpi variant accounts for chrome size at the current monitor's DPI. The older `AdjustWindowRectEx` uses system DPI, wrong on multi-monitor setups.

---

## 25. Win32 Includes Required

```cpp
#include <Windows.h>        // Core Win32
#include <windowsx.h>       // WIN32_LEAN_AND_MEAN excludes this, but its needed
#include <Dbt.h>            // needed for WM_DEVICECHANGE
#include <dwmapi.h>         // DwmGetWindowAttribute (accurate rect on Win11)
                            // Link: dwmapi.lib
#include <shellscalingapi.h>// GetDpiForMonitor
                            // Link: Shcore.lib
#include <dxgi1_6.h>        // IDXGIOutput6 for HDR info
                            // Link: dxgi.lib
// GameInput (GDK):
#include <GameInput.h>      // Link: GameInput.lib
// XInput fallback:
#include <Xinput.h>         // Link: Xinput.lib
```

---

## 26. Key Design Decisions Summary

| Decision | Rationale |
|---|---|
| Keyboards/mice merged, no device IDs | Raw Input HID noise is unmanageable; matches SDL/GLFW |
| Track connected gamepads | It's useful to know which player uses which controller |
| WM_POINTER for touch/stylus | Raw Input HID touch is driver-specific and unreliable |
| EnableMouseInPointer(FALSE) | Touchscreen → WM_POINTER only; touchpad → mouse emulation |
| Touchpad treated as mouse | No public API exists for precision touchpad as touch |
| GetCursorPos once per poll | One syscall per frame; position consistent across all events in frame |
| No absolute pos in MMouseMoveEvent | Events express change, getCursorPos() expresses state |
| MCharEvent includes Backspace/Enter | Text editing intent; app decides what they mean |
| WM_CLOSE only pushes event | App controls destruction; supports confirm-quit dialogs |
| close() doesn't call DestroyWindow directly | Window stays alive for the end of the frame |
| State cached, getters read cache | No syscalls at read time; WndProc is the single writer |
| GameInput preferred, XInput fallback | GameInput: no 4-controller limit, proper identity, wheels/sticks |
| Timestamps in us via QueryPerformanceCounter() | No GetMessageTime() overflow, timestamps matter for ordering only |
| Drop newest on queue overflow | Fastest, easiest |
| NotificationWndProc + WindowWndProc | Clean separation: global state vs per-window state |
