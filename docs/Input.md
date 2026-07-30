# Input

## Keyboard

Keyboard state is tracked globally by `MGlobal`. Query it at any time:

```cpp
bool held = MW::isKeyHeld(MW::MKey::Space);
MMods mods = MW::getMods();
```

By default, keyboard and mouse state keep tracking even while none of your windows are focused (e.g. alt-tabbed away). Opt into focus-gating instead:

```cpp
MInitConfig config;
config.ignoreUnfocusedInput = true;
MW::init(config);
```

### MKey

`MKey` represents a physical key position (scancode-based). It is layout-independent — `MKey::A` always refers to the key in the A position regardless of the user's keyboard layout.

### MMods

```cpp
struct MMods {
    bool shift : 1;
    bool ctrl : 1;
    bool alt : 1;
    bool gui : 1;  // Win / Cmd
    bool caps : 1; // CapsLock state, not the key itself
    bool num : 1;  // NumLock state
};
```

Left and right variants of modifier keys collapse into one flag. Note: AltGr on European keyboards is reported as `RightAlt` — the synthetic LeftCtrl that Win32 generates alongside it is currently not filtered out.

### Text Input

Use `MCharEvent` for text input rather than `MKeyEvent`. It carries a Unicode codepoint and fires for every character including Backspace and Enter — the app decides their meaning in context.

```cpp
struct MCharEvent {
    std::string input;
};
```

## Mouse

```cpp
MPoint pos = MW::getCursorPos(); // logical, sampled once per poll()
```

Cursor position is sampled once at the start of each `poll()` call. All events in a frame share this position.

```cpp
enum class MMouseButton { Left, Middle, Right, X1, X2 };

struct MMouseMoveEvent { MMicroSec timestamp; float dx, dy; };
struct MMouseScrollEvent { MMicroSec timestamp; float dx; float dy; MMods mods; };
```

## Touch

Touch input is delivered per-finger via `MTouchPoint`. Each finger has a stable `id` for the duration of a gesture.
Touchpads are treated as mice.

## Stylus

Stylus events are tracked separately from touch. There is only one stylus at a time by design.

## Gamepad

MWindow exposes Gamepad slots, not gamepad devices.

```cpp
using MGamepadSlot = uint64_t;
std::vector<MGamepadSlot> slots = MW::getActiveGamepadSlots();
```

```cpp
enum class MGamepadButton : uint32_t {
    DpadUp, DpadDown, DpadLeft, DpadRight,
    ActionBottom, ActionRight, ActionLeft, ActionTop,
    BumperLeft, BumperRight,
    ThumbLeft, ThumbRight,
    Start, Select, Guide,
    Count
};

A radial deadzone of ~0.1 is applied before storing. Button names use neutral labels — `ActionBottom` maps to A on Xbox, Cross on PlayStation.

```cpp
// Disable gamepad support entirely:
MInitConfig config;
config.ignoreGamepads = true;
MW::init(config);
```

## State Vector

You can query the current cached state from MWindow:
```cpp
struct MKeyboardState { std::bitset<static_cast<size_t>(MKey::Count)> held; MMods mods; };
struct MMouseState { MPoint p; std::bitset<static_cast<size_t>(MMouseButton::Count)> buttons; };
struct MTouchState { std::unordered_map<MTouchID, MPoint> activePoints; }; // touch id → point
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

// Query
const std::vector<MW::MDeviceState>& state = MW::getInputState();
```
**NOTE: You must requery this vector every frame!**

Index the vector with the `MDeviceType` enum class:
```cpp
enum class MDeviceType { Unknown = -1, Keyboard, Mouse, Touchscreen, Stylus, Gamepad };

auto& kbState = std::get<MKeyboardState>(state[static_cast<size_t>(MW::MDeviceType::Keyboard)]);
auto& msState = std::get<MMouseState>(state[static_cast<size_t>(MW::MDeviceType::Mouse)]);
auto& tcState = std::get<MTouchState>(state[static_cast<size_t>(MDeviceType::Touchscreen)]);
auto& stState = std::get<MStylusState>(state[static_cast<size_t>(MDeviceType::Stylus)]);
// All gamepad slots have different MGamepadStates
// Get them with this formula
MGamepadSlot slot = 2; // 0-4 usually but more is allowed
auto gpState = std::get<MGamepadState>(state[static_cast<size_t>(MDeviceType::Gamepad) + slot]);
```

## Monitors

```cpp
std::vector<MW::MMonitor> monitors = MW::getConnectedMonitors();
std::optional<MW::MMonitor> primary = MW::getPrimaryMonitor();
std::optional<MW::MMonitor> mon = MW::getMonitor(id);
```

```cpp
struct MMonitor {
    MMonitorID  id;
    std::string name;
    MRect    rect;       // logical virtual desktop space
    float    dpiScale;
    bool     isPrimary;
    std::size_t             currentModeIndex;
    std::vector<MVideoMode> availableModes;
    MHDRInfo hdr;
};

struct MHDRInfo {
    bool        supported;
    bool        active;
    float       maxLuminance;  // nits
    float       minLuminance;  // nits
    MColorGamut colorGamut;    // sRGB, DCI_P3, Rec2020
};
```
