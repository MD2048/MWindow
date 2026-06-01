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

    enum class MCoalescePolicy {
        None,       // keep all events
        Latest,     // drop all but the last (mouse move, touch move)
        Accumulate, // sum deltas (scroll wheel)
    };

    struct MInitConfig {
        std::size_t     eventQueueCapacity = 256;  // must be power of 2
        MCoalescePolicy mouseMovePolicy    = MCoalescePolicy::Latest;
        MCoalescePolicy touchMovePolicy    = MCoalescePolicy::Latest;
        MCoalescePolicy scrollPolicy       = MCoalescePolicy::Accumulate;
    };

    struct MEventHandlerEntry {
        MEventHandlerID id;
        MEventHandler   handler;
    };

} // namespace MW


#endif