#ifndef M_MONITOR_H
#define M_MONITOR_H

#include <cstdint>

namespace MW
{
    struct MVideoMode {
        uint32_t widthPx;
        uint32_t heightPx;
        uint32_t refreshRate;
    };

    struct MMonitor {
        uint32_t    id;
        std::string name;

        // Position and size in logical virtual desktop space
        float x, y;
        float widthLogical, heightLogical;

        float    dpiScale;      // e.g. 2.0 on Retina
        uint32_t dpi;           // raw DPI value

        bool isPrimary;

        MVideoMode currentMode;
        std::vector<MVideoMode> availableModes;
    };

} // namespace MW



#endif