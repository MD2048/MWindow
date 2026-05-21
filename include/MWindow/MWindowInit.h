#ifndef M_WINDOWINIT_H
#define M_WINDOWINIT_H

#include "MWindow/MMonitor.h"
#include "MWindow/MRendering.h"

#include <cstdint>
#include <string>

namespace MW
{
    using MWindowID = uint64_t;

    enum class MWindowMode      { Windowed, Fullscreen, BorderlessFullscreen };

    struct MWindowDesc {
        std::string      title   = "MWindow";
        MRect            rect    = {0,0,0,0};
        MWindowMode      mode    = MWindowMode::Windowed;
        MRendererBackend backend = MRendererBackend::None;

        bool resizable  = true;
        bool decorated  = true;
        bool visible    = true;

        bool centered   = true;

        // -1 = use primary monitor
        MMonitorID monitor = -1;
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

} // namespace MW


#endif