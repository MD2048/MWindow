#ifndef M_WINDOWIMPL_H
#define M_WINDOWIMPL_H

#ifndef MPLATFORM_WINDOWS
    #error "src/platform/windows/MWindowImpl.h should not be included on the current platform!"
#endif

#include "MWindow/MWindow.h"
 
namespace MW {
    class MGlobal;
    class MWindowImpl : public MWindow
    {
    private:
        MGlobal* global;
        
    public:
        MWindowImpl(const MWindowDesc& desc);

        ~MWindowImpl() override;

        // ── Lifecycle ─────────────────────────────────────────────────────────
        void show()  override;
        void hide()  override;
        void close() override;

        [[nodiscard]] bool isOpen()    const override;
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
        const MMonitor& getMonitor() const override;

        // DPI scale of the current monitor
        float getDpiScale() const override;

        // ── Rendering ─────────────────────────────────────────────────────────
        // Physical pixels — pass directly to Vulkan/Metal/DX/GL
        MSize getPhysicalSize() const override;

        MRenderSurface getRenderSurface() const override;

        // ── Factory ───────────────────────────────────────────────────────────
        [[nodiscard]] static std::unique_ptr<MWindow> create(const MWindowDesc& desc);
    };
}

#endif