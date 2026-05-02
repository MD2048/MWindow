#ifndef M_WINDOWIMPL_H
#define M_WINDOWIMPL_H

#ifndef MPLATFORM_WINDOWS
    #error "src/platform/windows/MWindowImpl.h should not be included on the current platform!"
#endif

#include "MWindow/MWindow.h"
 
namespace MW {
    class MWindowImpl : public MWindow
    {
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
        float getWidth()  const override;              // logical
        float getHeight() const override;              // logical

        void  setPosition(MPoint p) override;    // logical, desktop
        float getX() const override;
        float getY() const override;

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

        // ── Event handlers ────────────────────────────────────────────────────
        // Handlers are called in registration order.
        // Return EventResult::Consumed to stop propagation.
        void pushEventHandler(MEventHandler handler) override;
        void popEventHandler() override;

        // ── Factory ───────────────────────────────────────────────────────────
        [[nodiscard]] static std::unique_ptr<MWindow> create(const MWindowDesc& desc);
    };
}

#endif