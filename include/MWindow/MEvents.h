#ifndef M_EVENTS_H
#define M_EVENTS_H

#include "MWindow/MDef.h"
#include "MWindow/MDevices.h"

#include <cstdint>
#include <string>
#include <variant>
#include <functional>

using TouchID = uint64_t;

namespace MW {

    enum class MMouseButton : uint8_t {
        Unknown = 0,

        Left,
        Right,
        Middle,

        X1,
        X2,
    };

    enum class MKey : uint32_t {
        Unknown = 0,

        // Printable
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        Num0, Num1, Num2, Num3, Num4,
        Num5, Num6, Num7, Num8, Num9,

        Space, Apostrophe, Comma, Minus, Period, Slash,
        Semicolon, Equal, LeftBracket, Backslash, RightBracket, Grave,

        // Function
        F1,  F2,  F3,  F4,  F5,  F6,
        F7,  F8,  F9,  F10, F11, F12,

        // Navigation
        Up, Down, Left, Right,
        Home, End, PageUp, PageDown,
        Insert, Delete,

        // Control
        Enter, Escape, Backspace, Tab,
        CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

        // Modifiers (physical keys — for remapping/scancode use)
        LeftShift,  RightShift,
        LeftCtrl,   RightCtrl,
        LeftAlt,    RightAlt,
        LeftSuper,  RightSuper,   // Win key / Cmd key
        Menu,

        // Numpad
        KP0, KP1, KP2, KP3, KP4,
        KP5, KP6, KP7, KP8, KP9,
        KPDecimal, KPDivide, KPMultiply,
        KPSubtract, KPAdd, KPEnter,
    };


    struct MVisibilityChangeEvent {
        bool isVisible;
        constexpr MVisibilityChangeEvent(bool visible) noexcept
            : isVisible(visible)
        {}
    };

    struct MCloseEvent {};

    struct MResizeEvent {
        MSize new_size;
    };

    struct MMoveEvent {
        MPoint new_pos;
    };

    struct MFocusGainEvent {};

    struct MFocusLoseEvent {};

    struct MDPIChangeEvent {
        float oldDPI;
        float newDPI;
    };

    struct MKeyPressEvent {
        uint64_t timestamp;
        MDeviceID deviceId;
        MKey key;
        uint32_t scancode;
        MMods mods;
    };

    struct MKeyReleaseEvent {
        uint64_t timestamp;
        MDeviceID deviceId;
        MKey key;
        uint32_t scancode;
        MMods mods;
    };

    struct MCharEvent {
        MDeviceID deviceId;
        std::string input;
    };

    struct MMouseMoveEvent {
        uint64_t timestamp;
        MDeviceID deviceId;
        MPoint new_pos;
        float dx, dy;
    };

    struct MMouseButtonPressEvent {
        uint64_t timestamp;
        MDeviceID deviceId;
        MMouseButton button;
        MMods mods;
    };

    struct MMouseButtonReleaseEvent {
        uint64_t timestamp;
        MDeviceID deviceId;
        MMouseButton button;
        MMods mods;
    };

    struct MMouseScrollEvent {
        uint64_t timestamp;
        MDeviceID deviceId;
        float dx;
        float dy;
        MMods mods;
    };

    struct MMouseEnterEvent { MDeviceID deviceId; };

    struct MMouseLeaveEvent { MDeviceID deviceId; };

    struct MTouchBeginEvent {
        uint64_t timestamp;
        TouchID id;

        float x;
        float y;
    };

    struct MTouchMoveEvent {
        uint64_t timestamp;
        TouchID id;

        float x;
        float y;

        float dx;
        float dy;
    };
    
    struct MTouchEndEvent {
        uint64_t timestamp;
        TouchID id;

        float x;
        float y;
    };

    struct MTouchCancelEvent {
        uint64_t timestamp;
        TouchID id;
    };

    struct MDeviceConnectedEvent {
        MDeviceInfo dev_info;
    };

    struct MDeviceDisconnectedEvent {
        MDeviceInfo dev_info;
    };

    using MEvent = std::variant <
            // Visibibility
            MVisibilityChangeEvent,
            // Window
            MCloseEvent,
            MResizeEvent,
            MMoveEvent,
            MFocusGainEvent,
            MFocusLoseEvent,
            MDPIChangeEvent,
            // Keyboard
            MKeyPressEvent,
            MKeyReleaseEvent,
            MCharEvent,
            // Mouse
            MMouseMoveEvent,
            MMouseButtonPressEvent,
            MMouseButtonReleaseEvent,
            MMouseScrollEvent,
            MMouseEnterEvent,
            MMouseLeaveEvent,
            // Touch
            MTouchBeginEvent,
            MTouchMoveEvent,
            MTouchEndEvent,
            MTouchCancelEvent,

            //Devices
            MDeviceConnectedEvent,
            MDeviceDisconnectedEvent
            // Drop - cut from 1. version
            // MDropEnterEvent,
            // MDropMoveEvent,
            // MDropLeaveEvent,
            // MDropEvent,
    >;


    enum class MEventResult { Continue, Consumed };

    using MEventHandler = std::function<MEventResult(const MEvent&)>;
    using MEventHandlerID = uint64_t;
}

#endif