#ifndef M_WINDOWINIT_H
#define M_WINDOWINIT_H

#include "MWindow/MMonitor.h"
#include "MWindow/MRendering.h"

#include <cstdint>
#include <string>

namespace MW
{
    using MWindowID = uint64_t;

    enum class MWindowMode : uint32_t  { Windowed, Fullscreen, BorderlessFullscreen };

    struct MWindowDesc {
        std::string      title   = "MWindow";
        MRect            rect    = {100,100,800,600};
        MWindowMode      mode    = MWindowMode::Windowed;
        MRendererBackend backend = MRendererBackend::None;

        bool resizable  = true;     // best effort
        bool decorated  = true;     // best effort
        bool visible    = true;

        bool centered   = true;

        MMonitorID monitor;
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

} // namespace MW


#endif