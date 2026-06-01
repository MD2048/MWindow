#ifndef M_WINDOWIMPL_H
#define M_WINDOWIMPL_H

#ifndef MPLATFORM_WINDOWS
    #error "src/platform/windows/MWindowImpl.h should not be included on the current platform!"
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "MWindow/MWindow.h"
 
namespace MW {
    class MGlobal;
    class MWindowImpl : public MWindow
    {
    public:
        struct MWindowState {
            MWindowDesc desc         {};
            DWORD currentStyle       = 0;
            DWORD preFullScreenStyle = 0;      // saved before entering fullscreen
            RECT  currentRect        {};       // this avoids comparing floats
            RECT  preFullscreenRect  {};       // not adjusted physical
                                               // exstyle is 0 except in fullscreen: WS_EX_APPWINDOW
        };
    private:
        MGlobal* global;
        
        MWindowID id;
        std::atomic<bool> alive;
        std::atomic<HWND> hwnd;

        mutable std::mutex back_state_lock;
        std::atomic<MWindowState*> back_state;
        std::atomic<MWindowState*> front_state;
        MWindowState state1;
        MWindowState state2;
    public:
        MWindowImpl(const MWindowDesc& desc);

        ~MWindowImpl() override;

        MWindowState const* getFrontStatePtr() const;
        MWindowState* getBackStatePtr() const;
        void switchBuffers();

        // ── Lifecycle ─────────────────────────────────────────────────────────
        void show()  override;
        void hide()  override;
        void close() override;

        [[nodiscard]] bool isVisible() const override;  // false when minimized

        // ── Properties (all logical) ──────────────────────────────────────────
        void               setTitle(const std::string& title) override;
        const std::string& getTitle() const override;

        void     resize(MSize sz) override; // logical
        MSize getSize()  const override;              // logical

        void  setTopLeftCorner(MPoint p) override;    // logical, virtual desktop
        MPoint getTopLeftCorner() const override;

        MRect getRect() const override;

        void        setWindowMode(MWindowMode mode) override;
        MWindowMode getWindowMode() const override;

        // Which monitor this window currently lives on (dominant monitor)
        MMonitorID getCurrentMonitorID() const override;

        // DPI scale of the current monitor
        float getDpiScale() const override;

        // ── Rendering ─────────────────────────────────────────────────────────
        // Physical pixels — pass directly to Vulkan/Metal/DX/GL
        MSize getPhysicalSize() const override;

        MRenderSurface getRenderSurface() const override;
    };
}

#endif