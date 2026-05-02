#ifndef M_DEF_H
#define M_DEF_H

namespace MW
{
    struct MPoint {
        float x{ 0 };
        float y{ 0 };

        constexpr MPoint() = default;
        constexpr MPoint(float x, float y) noexcept : x(x), y(y) {}

        constexpr MPoint operator+(const MPoint& other) const noexcept { return { x + other.x, y + other.y }; }
        constexpr MPoint operator-(const MPoint& other) const noexcept { return { x - other.x, y - other.y }; }
        constexpr MPoint& operator+=(const MPoint& other) noexcept { x += other.x; y += other.y; return *this; }
        constexpr MPoint& operator-=(const MPoint& other) noexcept { x -= other.x; y -= other.y; return *this; }

        constexpr bool operator==(const MPoint& other) const noexcept { return x == other.x && y == other.y; }
        constexpr bool operator!=(const MPoint& other) const noexcept { return !(*this == other); }

    };

    struct MSize {
        float width{ 0 };
        float height{ 0 };

        constexpr MSize() = default;
        constexpr MSize(float w, float h) noexcept : width(w), height(h) {}

        constexpr bool operator==(const MSize& other) const noexcept { return width == other.width && height == other.height; }
        constexpr bool operator!=(const MSize& other) const noexcept { return !(*this == other); }
        
    };
    
    struct MRect {
        float x{ 0 };       // stores top left corner
        float y{ 0 };
        float width{ 0 };
        float height{ 0 };

        constexpr MRect() = default;
        constexpr MRect(float x, float y, float w, float h) noexcept
            : x(x), y(y), width(w), height(h) {
        }

        constexpr MPoint topLeft() const noexcept { return { x, y }; }
        constexpr MPoint topRight() const noexcept { return { x + width, y }; }
        constexpr MPoint bottomLeft() const noexcept { return {x, y - height}; }
        constexpr MPoint bottomRight() const noexcept { return { x + width, y - height }; }
        constexpr MSize  size() const noexcept { return { width, height }; }
    };   
}


#endif