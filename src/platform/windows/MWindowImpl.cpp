#include "windows/MWindowImpl.h"

namespace MW
{
    void MWindowImpl::show()  {  }
    void MWindowImpl::hide()  {  }
    void MWindowImpl::close() {  }

    [[nodiscard]] bool MWindowImpl::isOpen()    const {  }
    [[nodiscard]] bool MWindowImpl::isVisible() const {  }  // false when minimized

    // ── Properties (all logical) ──────────────────────────────────────────
    void               MWindowImpl::setTitle(const std::string& title) {  }
    const std::string& MWindowImpl::getTitle() const {  }

    void     MWindowImpl::resize(MSize sz) {  } // logical
    float MWindowImpl::getWidth()  const {  }              // logical
    float MWindowImpl::getHeight() const {  }              // logical

    void  MWindowImpl::setPosition(MPoint p) {  }    // logical, desktop
    float MWindowImpl::getX() const {  }
    float MWindowImpl::getY() const {  }

    void        MWindowImpl::setWindowMode(MWindowMode mode) {  }
    MWindowMode MWindowImpl::getWindowMode() const {  }

    // Which monitor this window currently lives on (dominant monitor)
    const MMonitor& MWindowImpl::getMonitor() const {  }

    // DPI scale of the current monitor
    float MWindowImpl::getDpiScale() const {  }

    // ── Rendering ─────────────────────────────────────────────────────────
    // Physical pixels — pass directly to Vulkan/Metal/DX/GL
    MSize MWindowImpl::getPhysicalSize() const {  }

    MRenderSurface MWindowImpl::getRenderSurface() const {  }

    // ── Event handlers ────────────────────────────────────────────────────
    // Handlers are called in registration order.
    // Return EventResult::Consumed to stop propagation.
    void MWindowImpl::pushEventHandler(MEventHandler handler) {  }
    void MWindowImpl::popEventHandler() {  }

    // ── Factory ───────────────────────────────────────────────────────────
    [[nodiscard]] std::unique_ptr<MWindow> MWindowImpl::create(const MWindowDesc& desc) {
        
    }
} // namespace MW
