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

    struct MDisplayChangeFlags {
        bool rect        : 1;
        bool scale       : 1;
        bool resolution  : 1;  // widthPx / heightPx changed
        bool refreshRate : 1;  // refresh rate changed
        bool bitDepth    : 1;  // bitsPerChannel changed
        bool hdrState    : 1;  // HDR enabled/disabled
        bool position    : 1;  // monitor moved in virtual desktop
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

        const MVideoMode& currentMode() const { return availableModes[currentModeIndex]; }

    };

    #ifdef MWINDOW_BUILD_PRINTS
    inline const char* toString(MColorGamut gamut) {
        switch (gamut) {
            case MColorGamut::SRGB:    return "sRGB";
            case MColorGamut::DCI_P3:  return "DCI-P3";
            case MColorGamut::Rec2020: return "Rec.2020";
            default:                   return "Unknown";
        }
    }

    inline std::ostream& operator<<(std::ostream& os, const MVideoMode& mode) {
        os << mode.widthPx << "x" << mode.heightPx << " @ " 
        << mode.refreshRate << "Hz (" << mode.bitsPerChannel << "-bit)";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MHDRInfo& hdr) {
        os << "HDR Info:\n"
        << "    Supported:     " << (hdr.supported ? "Yes" : "No") << "\n"
        << "    Active:        " << (hdr.active ? "Yes" : "No") << "\n"
        << "    Max Luminance: " << hdr.maxLuminance << " nits\n"
        << "    Min Luminance: " << hdr.minLuminance << " nits\n"
        << "    Color Gamut:   " << toString(hdr.colorGamut);
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MDisplayChangeFlags& flags) {
        os << "Display Changes: ["
        << (flags.resolution  ? " Resolution "  : "")
        << (flags.refreshRate ? " RefreshRate " : "")
        << (flags.bitDepth    ? " BitDepth "    : "")
        << (flags.hdrState    ? " HDRState "    : "")
        << (flags.position    ? " Position "    : "") << "]";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MMonitor& monitor) {
        os << "==================================================\n"
        << " MONITOR: " << monitor.name << " (ID: " << monitor.id << ")\n"
        << "==================================================\n"
        << " Primary Display: " << (monitor.isPrimary ? "Yes" : "No") << "\n"
        << " Bounds (Rect):   " << monitor.rect << "\n"
        << " DPI Scale:       " << (monitor.dpiScale * 100.0f) << "%\n"
        << " Current Mode:    " << monitor.currentMode() << "\n"
        << "--------------------------------------------------\n";
        
        os << monitor.hdr << "\n"
        << "--------------------------------------------------\n"
        << " Available Video Modes (" << monitor.availableModes.size() << "):\n";
        
        for (size_t i = 0; i < monitor.availableModes.size(); ++i) {
            os << (i == monitor.currentModeIndex ? "  > " : "    ") 
            << "[" << i << "] " << monitor.availableModes[i] << "\n";
        }
        os << "==================================================";
        return os;
    }
    #endif

} // namespace MW



#endif