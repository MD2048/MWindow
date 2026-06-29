#ifndef M_EVENTS_H
#define M_EVENTS_H

#include "MWindow/MDef.h"
#include "MWindow/MDevices.h"
#include "MWindow/MMonitor.h"
#include "MWindow/MKey.h"

#include <cstdint>
#include <string>
#include <variant>
#include <functional>

namespace MW {

    using MTouchID  = uint64_t;
    using MMicroSec = uint64_t;

    struct MVisibilityChangeEvent {
        bool isVisible;
        constexpr MVisibilityChangeEvent(bool visible) noexcept
            : isVisible(visible)
        {}
    };

    struct MCloseRequestEvent {};

    struct MResizeEvent {
        MSize new_size;
    };

    struct MMoveEvent {
        MPoint new_pos;
    };

    struct MFocusChangeEvent {
        bool focused;
    };

    struct MMonitorChangeEvent { // Not to confuse with displaychange!!!
        MMonitorID old_mon;
        MMonitorID new_id;
    };

    struct MKeyPressEvent {
        MMicroSec timestamp;
        MKey key;
        MMods mods;
    };

    struct MKeyReleaseEvent {
        MMicroSec timestamp;
        MKey key;
        MMods mods;
    };

    struct MCharEvent {
        std::string input;
    };

    struct MMouseMoveEvent {
        MMicroSec timestamp;
        float dx, dy;
    };

    struct MMouseButtonPressEvent {
        MMicroSec timestamp;
        MMouseButton button;
        MPoint pos;
        MMods mods;
    };

    struct MMouseButtonReleaseEvent {
        MMicroSec timestamp;
        MMouseButton button;
        MPoint pos;
        MMods mods;
    };

    struct MMouseScrollEvent {
        MMicroSec timestamp;
        float dx;
        float dy;
        MMods mods;
    };

    struct MMouseEnterEvent { MMicroSec timestamp; };

    struct MMouseLeaveEvent { MMicroSec timestamp; };

    struct MTouchBeginEvent {
        MMicroSec timestamp;
        MTouchID id;
        MPoint pos;
    };

    struct MTouchMoveEvent {
        MMicroSec timestamp;
        MTouchID id;

        MPoint new_pos;

        float dx;
        float dy;
    };
    
    struct MTouchEndEvent {
        MMicroSec timestamp;
        MTouchID id;

        MPoint pos;
    };

    struct MTouchCancelEvent {
        MMicroSec timestamp;
        MTouchID id;
    };

    struct MStylusEnterEvent {
        MMicroSec timestamp;
        MPoint pos;
    };

    struct MStylusHoverEvent {
        MMicroSec timestamp;

        MPoint new_pos;
        float dx,dy;
    };

    struct MStylusLeaveEvent {
        MMicroSec timestamp;
        MPoint pos;
    };

    struct MStylusDownEvent {
        MMicroSec timestamp;
        MPoint pos;
    };

    struct MStylusMoveEvent {
        MMicroSec timestamp;

        MPoint new_pos;
        float dx,dy;
    };

    struct MStylusUpEvent {
        MMicroSec timestamp;
        MPoint pos;
    };

    struct MStylusCancelEvent {
        MMicroSec timestamp;
    };

    struct MGamepadConnectedEvent {
        MMicroSec timestamp;
        MGamepadID id;
    };

    struct MGamepadDisconnectedEvent {
        MMicroSec timestamp;
        MGamepadID id;
    };

    struct MGamepadButtonPressEvent {
        MMicroSec timestamp;
        MGamepadID id;

        MGamepadButton button;
    };

    struct MGamepadButtonReleaseEvent {
        MMicroSec timestamp;
        MGamepadID id;

        MGamepadButton button;
    };

    struct MGamepadTriggerEvent {
        MMicroSec timestamp;
        MGamepadID id;

        bool left;

        float new_val;
        float d;
    };

    struct MGamepadStickEvent {
        MMicroSec timestamp;
        MGamepadID id;
        
        bool left;

        MStick new_val;
        float dx,dy;
    };

    struct MDisplaySettingChangeEvent {
        MMonitor old;
        MDisplayChangeFlags what;
        MMonitorID new_id;
    };

    struct MDisplayConnectedEvent {
        MMonitorID id;
    };

    struct MDisplayDisconnectedEvent {
        MMonitorID id;
    };

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
            MCharEvent,
            // Mouse
            MMouseMoveEvent,        // coalescable
            MMouseButtonPressEvent,
            MMouseButtonReleaseEvent,
            MMouseScrollEvent,      // coalescable
            MMouseEnterEvent,
            MMouseLeaveEvent,
            // Touch
            MTouchBeginEvent,
            MTouchMoveEvent,        // coalescable !
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
            MStylusHoverEvent,      // coalescable !
            MStylusLeaveEvent,
            MStylusDownEvent,
            MStylusMoveEvent,       // coalescable !
            MStylusUpEvent,
            MStylusCancelEvent,
            // Monitors
            MDisplaySettingChangeEvent,
            MDisplayConnectedEvent,
            MDisplayDisconnectedEvent
            // Drop - cut from 1. version
            // MDropEnterEvent,
            // MDropMoveEvent,
            // MDropLeaveEvent,
            // MDropEvent,
    >;

    inline bool shouldBeDeliveredToFocused(const MEvent& ev)
    {
        return std::visit(OVL {
            [](const MKeyPressEvent& ev) { return true; },
            [](const MKeyReleaseEvent& ev) { return true; },

            [](const MMouseMoveEvent& ev) { return true; },
            [](const MMouseButtonPressEvent& ev) { return true; },
            [](const MMouseButtonReleaseEvent& ev) { return true; },
            [](const MMouseScrollEvent& ev) { return true; },

            [](const MTouchBeginEvent& ev) { return true; },
            [](const MTouchMoveEvent& ev) { return true; },
            [](const MTouchEndEvent& ev) { return true; },
            [](const MTouchCancelEvent& ev) { return true; },

            [](const MGamepadTriggerEvent& ev) { return true; },
            [](const MGamepadStickEvent& ev) { return true; },

            [](const auto&) { return false; }
        }, ev);
    }


    enum class MEventResult { Continue, Consumed };

    using MEventHandler = std::function<MEventResult(const MEvent&)>;
    using MEventHandlerID = uint64_t;


    inline std::ostream& operator<<(std::ostream& os, const MEvent& ev) {
        std::visit(OVL{
            [&os](std::monostate) { os << "[Empty Event]"; },

            // Visibility
            [&os](const MVisibilityChangeEvent& e) { os << "MVisibilityChangeEvent: " << (e.isVisible ? "visible":"invisible"); },

            // Window
            [&os](const MCloseRequestEvent&)  { os << "MCloseRequestEvent"; },
            [&os](const MResizeEvent& e)        { os << "MResizeEvent: " << e.new_size; },
            [&os](const MMoveEvent& e)          { os << "MMoveEvent: " << e.new_pos; },
            [&os](const MFocusChangeEvent& e)   { os << "MFocusChangeEvent: " << (e.focused ? "focused" : "not focused"); },
            [&os](const MMonitorChangeEvent& e) { os << "MMonitorChangeEvent: old=" << e.old_mon << " -> new=" << e.new_id; },

            // Keyboard
            [&os](const MKeyPressEvent& e)   { os << "MKeyPressEvent: " << ToString(e.key); },
            [&os](const MKeyReleaseEvent& e) { os << "MKeyReleaseEvent: " << ToString(e.key); },
            [&os](const MCharEvent& e)       { os << "MCharEvent: " << e.input; },

            // Mouse
            [&os](const MMouseMoveEvent& e)          { os << "MMouseMoveEvent: " << e.dx << " " << e.dy; },
            [&os](const MMouseButtonPressEvent& e)   { os << "MMouseButtonPressEvent: " << ToString(e.button); },
            [&os](const MMouseButtonReleaseEvent& e) { os << "MMouseButtonReleaseEvent: " << ToString(e.button); },
            [&os](const MMouseScrollEvent& e)        { os << "MMouseScrollEvent: " << e.dx << " " << e.dy; },
            [&os](const MMouseEnterEvent& e)         { os << "MMouseEnterEvent"; },
            [&os](const MMouseLeaveEvent& e)         { os << "MMouseLeaveEvent"; },

            // Touch
            [&os](const MTouchBeginEvent& e)  { os << "MTouchBeginEvent: id=" << e.id << " pos=" << e.pos; },
            [&os](const MTouchMoveEvent& e)   { os << "MTouchMoveEvent: id=" << e.id << " pos=" << e.new_pos << " dx=" << e.dx << " dy=" << e.dy; },
            [&os](const MTouchEndEvent& e)    { os << "MTouchEndEvent: id=" << e.id << " pos=" << e.pos; },
            [&os](const MTouchCancelEvent& e) { os << "MTouchCancelEvent: id=" << e.id; },

            // Gamepad
            [&os](const MGamepadConnectedEvent& e)    { os << "MGamepadConnectedEvent: id=" << e.id; },
            [&os](const MGamepadDisconnectedEvent& e) { os << "MGamepadDisconnectedEvent: id=" << e.id; },
            [&os](const MGamepadButtonPressEvent& e)  { os << "MGamepadButtonPressEvent: id=" << e.id << " button=" << ToString(e.button); },
            [&os](const MGamepadButtonReleaseEvent& e) { os << "MGamepadButtonReleaseEvent: id=" << e.id << " button=" << ToString(e.button); },
            [&os](const MGamepadTriggerEvent& e)      { os << "MGamepadTriggerEvent: id=" << e.id << " " << (e.left ? "left" : "right") << " trigger=" << e.new_val << " delta=" << e.d; },
            [&os](const MGamepadStickEvent& e)        { os << "MGamepadStickEvent: id=" << e.id << " " << (e.left ? "left" : "right") << " stick=(" << e.new_val.x << ", " << e.new_val.y << ") dx=" << e.dx << " dy=" << e.dy; },

            // Stylus
            [&os](const MStylusEnterEvent& e)  { os << "MStylusEnterEvent: pos=" << e.pos; },
            [&os](const MStylusHoverEvent& e)  { os << "MStylusHoverEvent: pos=" << e.new_pos << " dx=" << e.dx << " dy=" << e.dy; },
            [&os](const MStylusLeaveEvent& e)  { os << "MStylusLeaveEvent: pos=" << e.pos; },
            [&os](const MStylusDownEvent& e)   { os << "MStylusDownEvent: pos=" << e.pos; },
            [&os](const MStylusMoveEvent& e)   { os << "MStylusMoveEvent: pos=" << e.new_pos << " dx=" << e.dx << " dy=" << e.dy; },
            [&os](const MStylusUpEvent& e)     { os << "MStylusUpEvent: pos=" << e.pos; },
            [&os](const MStylusCancelEvent&)  { os << "MStylusCancelEvent"; },

            // Monitors
            [&os](const MDisplaySettingChangeEvent& e) { os << "MDisplaySettingChangeEvent: " << e.what << " new_id=" << e.new_id; },
            [&os](const MDisplayConnectedEvent& e)     { os << "MDisplayConnectedEvent: id=" << e.id; },
            [&os](const MDisplayDisconnectedEvent& e)  { os << "MDisplayDisconnectedEvent: id=" << e.id; },

            [&os](const auto&) {os << "Unknown";}
            
        }, ev);
        
        return os;
    }
}

#endif