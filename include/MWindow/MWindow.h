#ifndef M_WINDOW_H
#define M_WINDOW_H

// MWindow should
//  * be associated with 1 window
//  * shouldn't contain anything platform dependent
//  * should use PIMPL
//  * expose some of MGlobal's functionality

#include "MWindow/MEvents.h"
#include "MWindow/MMonitor.h"
#include "MWindow/MWindowInit.h"
#include "MWindow/MDef.h"
#include "MWindow/MRendering.h"

#include <cstdint>
#include <vector>
#include <memory>

namespace MW {

    void init(const MInitConfig& config = {});
    void shutdown();

    // Drains the event queue, walks each event through registered handler chains
    void poll();

    //bool isRunning();

    // Device query
    const std::vector<MDeviceInfo>& getConnectedDevices();

    // Monitor query
    const std::vector<MMonitor>& getMonitors();
    const MMonitor&              getPrimaryMonitor();

    float getCursorX();
    float getCursorY();

    // Key state query for between-event polling
    bool isKeyHeld(MKey key);
    
    class MWindow {
    public:
        virtual ~MWindow() = default;

        // ── Lifecycle ─────────────────────────────────────────────────────────
        virtual void show()  = 0;
        virtual void hide()  = 0;
        virtual void close() = 0;

        [[nodiscard]] virtual bool isOpen()    const = 0;
        [[nodiscard]] virtual bool isVisible() const = 0;  // false when minimized

        // ── Properties (all logical) ──────────────────────────────────────────
        virtual void               setTitle(const std::string& title) = 0;
        virtual const std::string& getTitle() const = 0;

        virtual void     resize(MSize sz) = 0; // logical
        virtual float getWidth()  const = 0;              // logical
        virtual float getHeight() const = 0;              // logical

        virtual void  setPosition(MPoint p) = 0;    // logical, virtual desktop
        virtual float getX() const = 0;
        virtual float getY() const = 0;

        virtual void        setWindowMode(MWindowMode mode) = 0;
        virtual MWindowMode getWindowMode() const = 0;

        // Which monitor this window currently lives on (dominant monitor)
        virtual const MMonitor& getMonitor() const = 0;

        // DPI scale of the current monitor
        virtual float getDpiScale() const = 0;

        // ── Rendering ─────────────────────────────────────────────────────────
        // Physical pixels — pass directly to Vulkan/Metal/DX/GL
        virtual MSize getPhysicalSize() const = 0;

        virtual MRenderSurface getRenderSurface() const = 0;

        // ── Event handlers ────────────────────────────────────────────────────
        // Handlers are called in registration order.
        // Return EventResult::Consumed to stop propagation.
        virtual void pushEventHandler(MEventHandler handler) = 0;
        virtual void popEventHandler() = 0;

        // ── Factory ───────────────────────────────────────────────────────────
        [[nodiscard]] static std::unique_ptr<MWindow> create(const MWindowDesc& desc);
    };

} // namespace MWindow

#endif