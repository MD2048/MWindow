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
#include <shared_mutex>
#include <memory>
#include <optional>

namespace MW {

    void init(const MInitConfig& config = {});
    void shutdown();

    // Drains the event queue, walks each event through registered handler chains
    void poll();

    //bool isRunning();

    MEventHandlerID registerGlobalEventHandler(MEventHandler ha);
    void            unregisterGlobalEventHandler(MEventHandlerID id);


    // Device query
    std::vector<MDeviceInfo>    getConnectedDevices();
    std::optional<MDeviceState const*> getDeviceState(MDeviceID id);
    bool                        isDeviceConnected(MDeviceID id);

    // Monitor query
    std::vector<MMonitor>   getConnectedMonitors();
    std::optional<MMonitor> getMonitor(MMonitorID id);
    std::optional<MMonitor> getPrimaryMonitor();
    bool                    isMonitorConnected(MMonitorID id);

    float getCursorX();
    float getCursorY();

    // Key state query for between-event polling
    bool isKeyHeld(MKey key);
    
    class MWindow {
    private:
        std::shared_mutex handler_lock;
        std::vector<MEventHandlerEntry> handlers;
        MEventHandlerID nextID;
    protected:
        MWindow();
    public:

        void executeHandlerChain(const MEvent& ev);

        MEventHandlerID registerEventHandler(MEventHandler ha);
        void            unregisterEventHandler(MEventHandlerID id);

        virtual ~MWindow() = default;

        // ── Lifecycle ─────────────────────────────────────────────────────────
        virtual void show()  = 0;
        virtual void hide()  = 0;
        virtual void close() = 0;

        [[nodiscard]] virtual bool isVisible() const = 0;  // false when minimized

        // ── Properties (all logical) ──────────────────────────────────────────
        virtual void               setTitle(const std::string& title) = 0;
        virtual const std::string& getTitle() const = 0;

        virtual void     resize(MSize sz) = 0; // logical
        virtual MSize getSize()  const = 0;              // logical

        virtual void  setTopLeftCorner(MPoint p) = 0;    // logical, virtual desktop
        virtual MPoint getTopLeftCorner() const = 0;

        virtual MRect getRect() const = 0;

        virtual void        setWindowMode(MWindowMode mode) = 0;
        virtual MWindowMode getWindowMode() const = 0;

        // Which monitor this window currently lives on (dominant monitor)
        virtual MMonitorID getCurrentMonitorID() const = 0;

        // DPI scale of the current monitor
        virtual float getDpiScale() const = 0;

        // ── Rendering ─────────────────────────────────────────────────────────
        // Physical pixels — pass directly to Vulkan/Metal/DX/GL
        virtual MSize getPhysicalSize() const = 0;

        virtual MRenderSurface getRenderSurface() const = 0;

        // ── Factory ───────────────────────────────────────────────────────────
        [[nodiscard]] static std::unique_ptr<MWindow> create(const MWindowDesc& desc);
    };

} // namespace MWindow

#endif