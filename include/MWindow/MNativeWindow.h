#ifndef M_RENDERING_H
#define M_RENDERING_H

#include <cstdint>
#include <variant>

namespace MW
{
    using MOpaqueHandle = std::uintptr_t;

    struct MWindowsNativeWindow
    {
        MOpaqueHandle hwnd;
        MOpaqueHandle hinstance;
    };

    struct MX11NativeWindow
    {
        MOpaqueHandle display;
        MOpaqueHandle window;
    };

    struct MWaylandNativeWindow
    {
        MOpaqueHandle display;
        MOpaqueHandle surface;
    };

    struct MMacOSNativeWindow
    {
        MOpaqueHandle nsWindow;
    };

    struct MIOSNativeWindow
    {
        MOpaqueHandle uiWindow;
        MOpaqueHandle uiView;
    };

    struct MAndroidNativeWindow
    {
        MOpaqueHandle nativeWindow;
    };

    using MNativeWindow = std::variant<
        MWindowsNativeWindow,
        MX11NativeWindow,
        MWaylandNativeWindow,
        MMacOSNativeWindow,
        MIOSNativeWindow,
        MAndroidNativeWindow
    >;
} // namespace MW


#endif