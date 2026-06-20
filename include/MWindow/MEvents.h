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

using TouchID = uint64_t;

namespace MW {

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
            MDeviceID deviceId;
            MKey key;
            MMods mods;
        };

        struct MKeyReleaseEvent {
            uint64_t timestamp;
            MDeviceID deviceId;
            MKey key;
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
            MVisibilityChangeEvent,
            // Window
            MCloseRequestEvent,
            MResizeEvent,
            MMoveEvent,
            MFocusChangeEvent,
            MMonitorChangeEvent,
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
            MDeviceDisconnectedEvent,
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


    enum class MEventResult { Continue, Consumed };

    using MEventHandler = std::function<MEventResult(const MEvent&)>;
    using MEventHandlerID = uint64_t;


    inline std::ostream& operator<<(std::ostream& os, const MEvent& ev) {
        std::visit(OVL{
            [&os](std::monostate) { os << "[Empty Event]"; },

            // Visibility
            [&os](const MVisibilityChangeEvent&) { os << "MVisibilityChangeEvent"; },

            // Window
            [&os](const MCloseRequestEvent&)  { os << "MCloseRequestEvent"; },
            [&os](const MResizeEvent&)        { os << "MResizeEvent"; },
            [&os](const MMoveEvent&)          { os << "MMoveEvent"; },
            [&os](const MFocusChangeEvent&)   { os << "MFocusChangeEvent"; },
            [&os](const MMonitorChangeEvent&) { os << "MMonitorChangeEvent"; },

            // Keyboard
            [&os](const MKeyPressEvent&)   { os << "MKeyPressEvent"; },
            [&os](const MKeyReleaseEvent&) { os << "MKeyReleaseEvent"; },
            [&os](const MCharEvent&)       { os << "MCharEvent"; },

            // Mouse
            [&os](const MMouseMoveEvent&)          { os << "MMouseMoveEvent"; },
            [&os](const MMouseButtonPressEvent&)   { os << "MMouseButtonPressEvent"; },
            [&os](const MMouseButtonReleaseEvent&) { os << "MMouseButtonReleaseEvent"; },
            [&os](const MMouseScrollEvent&)        { os << "MMouseScrollEvent"; },
            [&os](const MMouseEnterEvent&)         { os << "MMouseEnterEvent"; },
            [&os](const MMouseLeaveEvent&)         { os << "MMouseLeaveEvent"; },

            // Touch
            [&os](const MTouchBeginEvent&)  { os << "MTouchBeginEvent"; },
            [&os](const MTouchMoveEvent&)   { os << "MTouchMoveEvent"; },
            [&os](const MTouchEndEvent&)    { os << "MTouchEndEvent"; },
            [&os](const MTouchCancelEvent&) { os << "MTouchCancelEvent"; },

            // Devices
            [&os](const MDeviceConnectedEvent&)    { os << "MDeviceConnectedEvent"; },
            [&os](const MDeviceDisconnectedEvent&) { os << "MDeviceDisconnectedEvent"; },

            // Monitors
            [&os](const MDisplaySettingChangeEvent&) { os << "MDisplaySettingChangeEvent"; },
            [&os](const MDisplayConnectedEvent&)     { os << "MDisplayConnectedEvent"; },
            [&os](const MDisplayDisconnectedEvent&)  { os << "MDisplayDisconnectedEvent"; }
            
        }, ev);
        
        return os;
    }
}

#endif