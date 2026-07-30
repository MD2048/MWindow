#ifndef M_WINDOWINIT_H
#define M_WINDOWINIT_H

#include "MWindow/MMonitor.h"
#include "MWindow/MNativeWindow.h"
#include "MWindow/MIcon.h"
#include "MWindow/MCursor.h"

#include <cstdint>
#include <string>
#include <optional>

namespace MW
{
    using MWindowID = uint64_t;

    enum class MWindowMode : uint32_t  { Windowed, Fullscreen, BorderlessFullscreen };

    struct MWindowDesc {
        std::string      title   = "MWindow";
        MRect            rect    = {100,100,800,600};
        MWindowMode      mode    = MWindowMode::Windowed;

        bool resizable  = true;     // best effort
        bool decorated  = true;     // best effort
        bool visible    = true;

        bool centered   = true;

        MMonitorID monitor;

        std::optional<MIconData> icon;   // nullopt = OS default icon
        std::optional<MCursorData> cursor;   // nullopt = OS default cursor
    };

    struct MInitConfig {
        std::size_t     eventQueueCapacity = 256;  // must be power of 2
        bool ignoreGamepads         = false;
        bool mouseMoveCoalescing    = true;
        bool scrollCoalescing       = true;
        bool touchMoveCoalescing    = true;
        bool stylusMoveCoalescing   = true;
        bool gamepadCoalescing      = true;
    };

    struct MEventHandlerEntry {
        MEventHandlerID id;
        MEventHandler   handler;
    };

    #ifdef MWINDOW_BUILD_PRINTS

    inline std::ostream& operator<<(std::ostream& os, const MWindowMode& mode) {
        switch(mode) {
            case MWindowMode::Windowed: return os << "Windowed";
            case MWindowMode::Fullscreen: return os << "Fullscreen";
            case MWindowMode::BorderlessFullscreen: return os << "BorderlessFullscreen";
            default: return os << "???";
        }
    }

    #endif

} // namespace MW

#endif