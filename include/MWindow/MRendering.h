#ifndef M_RENDERING_H
#define M_RENDERING_H

#include <cstdint>
#include <variant>

namespace MW
{
    enum class MRendererBackend { None, Vulkan, Metal, DirectX, OpenGL };


    struct MVulkanSurface  { void* instance; void* surface;      };
    struct MMetalSurface   { void* caMetalLayer;                 };
    struct MDirectXSurface { void* hwnd;                         };
    struct MOpenGLSurface  { void* nativeWindow;                 };
    struct MRawSurface     { void* nativeDisplay; void* nativeWindow;
                            uint32_t widthPx; uint32_t heightPx; };

    using MRenderSurface = std::variant <
        MVulkanSurface, MMetalSurface,
        MDirectXSurface, MOpenGLSurface,
        MRawSurface
    >;
} // namespace MW


#endif