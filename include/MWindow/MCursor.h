#ifndef M_CURSOR_H
#define M_CURSOR_H

#include <cstdint>
#include <vector>

#ifdef MWINDOW_BUILD_PRINTS
    #include <iostream>
#endif

namespace MW
{
    // Raw RGBA8 pixel buffer, row-major, top-to-bottom, straight (non-premultiplied) alpha.
    // pixels.size() must equal width * height * 4.
    // hotspot is the pixel that tracks the actual pointer position (e.g. the tip of an arrow).
    struct MCursorData {
        uint32_t              width  = 0;
        uint32_t              height = 0;
        std::vector<uint8_t>  pixels;
        uint32_t              hotspotX = 0;
        uint32_t              hotspotY = 0;
    };

    #ifdef MWINDOW_BUILD_PRINTS
    inline std::ostream& operator<<(std::ostream& os, const MCursorData& cursor) {
        return os << "MCursorData[" << cursor.width << "x" << cursor.height << ", "
                  << cursor.pixels.size() << " bytes, hotspot=(" << cursor.hotspotX
                  << ", " << cursor.hotspotY << ")]";
    }
    #endif
} // namespace MW

#endif
