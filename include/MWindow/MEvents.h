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

    using MTouchID = uint64_t;

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
        uint64_t timestamp;
        MKey key;
        MMods mods;
    };

    struct MKeyReleaseEvent {
        uint64_t timestamp;
        MKey key;
        MMods mods;
    };

    struct MCharEvent {
        std::string input;
    };

    struct MMouseMoveEvent {
        uint64_t timestamp;
        float dx, dy;
    };

    struct MMouseButtonPressEvent {
        uint64_t timestamp;
        MMouseButton button;
        MPoint pos;
        MMods mods;
    };

    struct MMouseButtonReleaseEvent {
        uint64_t timestamp;
        MMouseButton button;
        MPoint pos;
        MMods mods;
    };

    struct MMouseScrollEvent {
        uint64_t timestamp;
        float dx;
        float dy;
        MMods mods;
    };

    struct MMouseEnterEvent { uint64_t timestamp; };

    struct MMouseLeaveEvent { uint64_t timestamp; };

    struct MTouchBeginEvent {
        uint64_t timestamp;
        MTouchID id;
        MPoint pos;
    };

    struct MTouchMoveEvent {
        uint64_t timestamp;
        MTouchID id;

        MPoint new_pos;

        float dx;
        float dy;
    };
    
    struct MTouchEndEvent {
        uint64_t timestamp;
        MTouchID id;

        MPoint pos;
    };

    struct MTouchCancelEvent {
        uint64_t timestamp;
        MTouchID id;
    };

    struct MStylusEnterEvent {
        uint64_t timestamp;
        MPoint pos;
    };

    struct MStylusHoverEvent {
        uint64_t timestamp;

        MPoint new_pos;
        float dx,dy;
    };

    struct MStylusLeaveEvent {
        uint64_t timestamp;
        MPoint pos;
    };

    struct MStylusDownEvent {
        uint64_t timestamp;
        MPoint pos;
    };

    struct MStylusMoveEvent {
        uint64_t timestamp;

        MPoint new_pos;
        float dx,dy;
    };

    struct MStylusUpEvent {
        uint64_t timestamp;
        MPoint pos;
    };

    struct MStylusCancelEvent {
        uint64_t timestamp;
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

    inline bool isAffectedByDeliverToFocused(const MEvent& ev)
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
            [&os](const MFocusChangeEvent& e)   { os << "MFocusChangeEvent: " << e.focused ? "focused" : "not focused"; },
            [&os](const MMonitorChangeEvent& e) { os << "MMonitorChangeEvent: " << e.new_id; },

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
            [&os](const MTouchBeginEvent&)  { os << "MTouchBeginEvent"; },
            [&os](const MTouchMoveEvent&)   { os << "MTouchMoveEvent"; },
            [&os](const MTouchEndEvent&)    { os << "MTouchEndEvent"; },
            [&os](const MTouchCancelEvent&) { os << "MTouchCancelEvent"; },

            // Monitors
            [&os](const MDisplaySettingChangeEvent&) { os << "MDisplaySettingChangeEvent"; },
            [&os](const MDisplayConnectedEvent&)     { os << "MDisplayConnectedEvent"; },
            [&os](const MDisplayDisconnectedEvent&)  { os << "MDisplayDisconnectedEvent"; },

            [&os](const auto&) {os << "Unknown";}
            
        }, ev);
        
        return os;
    }
}

#endif