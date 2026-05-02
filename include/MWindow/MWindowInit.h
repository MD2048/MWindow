#ifndef M_WINDOWINIT_H
#define M_WINDOWINIT_H

#include "MWindow/MMonitor.h"
#include "MWindow/MRendering.h"

#include <cstdint>
#include <string>

namespace MW
{
    enum class MWindowMode      { Windowed, Fullscreen, BorderlessFullscreen };

    struct MWindowDesc {
        std::string     title   = "MWindow";
        uint32_t        width   = 1280;   // logical pixels
        uint32_t        height  = 720;    // logical pixels
        MWindowMode      mode    = MWindowMode::Windowed;
        MRendererBackend backend = MRendererBackend::None;

        bool resizable  = true;
        bool decorated  = true;
        bool visible    = true;

        // Logical position in virtual desktop space. -1 = centre on primary monitor
        float x = -1;
        float y = -1;

        // nullptr = use primary monitor
        const MMonitor* monitor = nullptr;
    };

    enum class MCoalescePolicy {
        None,       // keep all events
        Latest,     // drop all but the last (mouse move, touch move)
        Accumulate, // sum deltas (scroll wheel)
    };

    struct MInitConfig {
        uint32_t       eventQueueCapacity = 256;  // must be power of 2
        MCoalescePolicy mouseMovePolicy    = MCoalescePolicy::Latest;
        MCoalescePolicy touchMovePolicy    = MCoalescePolicy::Latest;
        MCoalescePolicy scrollPolicy       = MCoalescePolicy::Accumulate;
    };

} // namespace MW


#endif