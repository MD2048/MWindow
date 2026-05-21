#ifndef M_MONITOR_H
#define M_MONITOR_H

#include <cstdint>
#include <string>
#include <cstddef>
#include <vector>

#include "MWindow/MDef.h"

namespace MW
{
    using MMonitorID = uint64_t;

    struct MVideoMode {
        uint32_t widthPx;
        uint32_t heightPx;
        uint32_t refreshRate;
        uint32_t bitsPerChannel;  // typically 8, 10, or 16

        bool operator==(const MVideoMode& m)
        {
            return (widthPx == m.widthPx) && (heightPx == m.heightPx) &&
                (refreshRate == m.refreshRate) && (bitsPerChannel == m.bitsPerChannel);
        }
    };

    enum class MColorGamut {
        Unknown,
        SRGB,       // standard, ~99% of monitors
        DCI_P3,     // wide gamut, most Apple displays
        Rec2020,    // ultra wide, high-end HDR monitors
    };

    struct MHDRInfo {
        bool         supported;
        bool         active;         // HDR currently enabled in OS settings
        float        maxLuminance;   // peak brightness in nits
        float        minLuminance;   // black level in nits
        MColorGamut  colorGamut;
    };

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

} // namespace MW



#endif