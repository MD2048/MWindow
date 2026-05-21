#include "windows/MWindowImpl.h"

namespace MW
{
    MWindowImpl::MWindowImpl(const MWindowDesc& desc) {  }

    MWindowImpl::~MWindowImpl() {  }

    void MWindowImpl::show()  {  }
    void MWindowImpl::hide()  {  }
    void MWindowImpl::close() {  }

    [[nodiscard]] bool MWindowImpl::isOpen()    const {  }
    [[nodiscard]] bool MWindowImpl::isVisible() const {  }  // false when minimized

    // ── Properties (all logical) ──────────────────────────────────────────
    void               MWindowImpl::setTitle(const std::string& title) {  }
    const std::string& MWindowImpl::getTitle() const {  }

    void     MWindowImpl::resize(MSize sz) {  } // logical
    MSize MWindowImpl::getSize()  const {  }            // logical

    void  MWindowImpl::setTopLeftCorner(MPoint p) {  }    // logical, virtual desktop
    MPoint MWindowImpl::getTopLeftCorner() const {  }

    MRect MWindowImpl::getRect() const {  }

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

    // ── Factory ───────────────────────────────────────────────────────────
    [[nodiscard]] std::unique_ptr<MWindow> MWindowImpl::create(const MWindowDesc& desc) {
        
    }
} // namespace MW
