#ifndef M_ICON_H
#define M_ICON_H

#include <cstdint>
#include <vector>

#ifdef MWINDOW_BUILD_PRINTS
    #include <iostream>
#endif

namespace MW
{
    // Raw RGBA8 pixel buffer, row-major, top-to-bottom, straight (non-premultiplied) alpha.
    // pixels.size() must equal width * height * 4.
    struct MIconData {
        uint32_t              width  = 0;
        uint32_t              height = 0;
        std::vector<uint8_t>  pixels;
    };

    #ifdef MWINDOW_BUILD_PRINTS
    inline std::ostream& operator<<(std::ostream& os, const MIconData& icon) {
        return os << "MIconData[" << icon.width << "x" << icon.height << ", " << icon.pixels.size() << " bytes]";
    }
    #endif
} // namespace MW

#endif
