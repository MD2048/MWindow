#ifndef M_DEF_H
#define M_DEF_H

#ifdef MWINDOW_BUILD_PRINTS
    #include <iostream>
#endif
namespace MW
{
    struct MPoint {
        float x{ 0.f };
        float y{ 0.f };

        constexpr MPoint() = default;
        constexpr MPoint(float x, float y) noexcept : x(x), y(y) {}

        constexpr MPoint operator+(const MPoint& other) const noexcept { return { x + other.x, y + other.y }; }
        constexpr MPoint operator-(const MPoint& other) const noexcept { return { x - other.x, y - other.y }; }
        constexpr MPoint& operator+=(const MPoint& other) noexcept { x += other.x; y += other.y; return *this; }
        constexpr MPoint& operator-=(const MPoint& other) noexcept { x -= other.x; y -= other.y; return *this; }

        constexpr bool operator==(const MPoint& other) const noexcept { return x == other.x && y == other.y; }
        constexpr bool operator!=(const MPoint& other) const noexcept { return !(*this == other); }

        #ifdef MWINDOW_BUILD_PRINTS
            friend std::ostream& operator<<(std::ostream& os, const MPoint& p) {
                return os << "(" << p.x << ", " << p.y << ")";
            }
        #endif
    };

    struct MSize {
        float width{ 0.f };
        float height{ 0.f };

        constexpr MSize() = default;
        constexpr MSize(float w, float h) noexcept : width(w), height(h) {}

        constexpr bool operator==(const MSize& other) const noexcept { return width == other.width && height == other.height; }
        constexpr bool operator!=(const MSize& other) const noexcept { return !(*this == other); }
        
        #ifdef MWINDOW_BUILD_PRINTS
        friend std::ostream& operator<<(std::ostream& os, const MSize& s) {
            return os << "[" << s.width << "x" << s.height << "] ";
        }
        #endif
    };
    
    struct MRect {
        float x{ 0.f };       // stores top left corner
        float y{ 0.f };       // y increases downwards
        float width{ 0.f };
        float height{ 0.f };

        constexpr MRect() = default;
        constexpr MRect(float x, float y, float w, float h) noexcept
            : x(x), y(y), width(w), height(h) {
        }

        constexpr bool operator==(const MRect& other) const noexcept { return size() == other.size() && topLeft() == other.topLeft(); }
        constexpr bool operator!=(const MRect& other) const noexcept { return !(*this == other); }

        constexpr MPoint topLeft() const noexcept { return { x, y }; }
        constexpr MPoint topRight() const noexcept { return { x + width, y }; }
        constexpr MPoint bottomLeft() const noexcept { return {x, y + height}; }
        constexpr MPoint bottomRight() const noexcept { return { x + width, y + height }; }
        constexpr MPoint middle() const noexcept { return { x + (width/2), y + (height/2) }; }
        constexpr MSize  size() const noexcept { return { width, height }; }
        constexpr bool   contains(MPoint p) const noexcept { return (x <= p.x) && (p.x < x+width) && (y <= p.y) && (p.y < y+height); }

        #ifdef MWINDOW_BUILD_PRINTS
        friend std::ostream& operator<<(std::ostream& os, const MRect& r) {
            return os << "[" << r.x << ", " << r.y << ", " << r.width << "x" << r.height << "]";
        }
        #endif
    };
    
    struct MStick {
        float x{ 0.f }; // always 1 >=, >= -1
        float y{ 0.f };

        constexpr MStick() = default;
        constexpr MStick(float x, float y) noexcept : x(x), y(y) {}

        constexpr bool operator==(const MStick& other) const noexcept { return x == other.x && y == other.y; }
        constexpr bool operator!=(const MStick& other) const noexcept { return !(*this == other); }
    };
}


#endif