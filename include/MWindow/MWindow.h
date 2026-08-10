#ifndef M_WINDOW_H
#define M_WINDOW_H

#include "MWindow/MEvents.h"
#include "MWindow/MMonitor.h"
#include "MWindow/MWindowInit.h"
#include "MWindow/MDef.h"
#include "MWindow/MNativeWindow.h"
#include "MWindow/MIcon.h"
#include "MWindow/MCursor.h"

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

    MEventHandlerID registerGlobalEventHandler(MEventHandler ha);
    void            unregisterGlobalEventHandler(MEventHandlerID id);

    std::vector<MMonitor>   getConnectedMonitors();
    std::optional<MMonitor> getMonitor(MMonitorID id);
    std::optional<MMonitor> getPrimaryMonitor();
    bool                    isMonitorConnected(MMonitorID id);

    bool isGamepadSlotActive(MGamepadSlot id);
    std::vector<MGamepadSlot> getActiveGamepadSlots();

    MPoint getCursorPos();
    MMods getMods();
    bool isKeyHeld(MKey key);

    const std::vector<MDeviceState>& getInputState(); // MUST re-query per frame !!!
    
    class MWindow {
    private:
        std::mutex handler_lock;
        std::vector<MEventHandlerEntry> handlers;
        MEventHandlerID nextID;
    protected:
        MWindow();
    public:

        void executeHandlerChain(const MEvent& ev);

        MEventHandlerID registerEventHandler(MEventHandler ha);
        void            unregisterEventHandler(MEventHandlerID id);

        virtual ~MWindow() = default;

        virtual MWindowID getId() const = 0;

        virtual void show()  = 0;
        virtual void hide()  = 0;
        virtual void close() = 0;

        [[nodiscard]] virtual bool isAlive() const = 0;
        [[nodiscard]] virtual bool isVisible() const = 0;  // false when minimized too

        virtual void               setTitle(const std::string& title) = 0;
        virtual const std::string& getTitle() const = 0;

        virtual void setIcon(const MIconData& icon) = 0;
        virtual void setCursor(const MCursorData& cursor) = 0;

        virtual void setTextInputEnabled(bool enabled) = 0;
        [[nodiscard]] virtual bool isTextInputEnabled() const = 0;

        // Every coordinate in logical desktop space

        virtual void     resize(MSize sz) = 0;
        virtual MSize getSize()  const = 0;

        virtual void  setTopLeftCorner(MPoint p) = 0;
        virtual MPoint getTopLeftCorner() const = 0;

        virtual MRect getRect() const = 0;

        virtual void        setWindowMode(MWindowMode mode) = 0;
        virtual MWindowMode getWindowMode() const = 0;

        virtual MMonitorID getCurrentMonitorID() const = 0;

        virtual float getDpiScale() const = 0;

        // Physical pixels - cast to an integer type
        virtual MSize getPhysicalSize() const = 0;

        virtual MNativeWindow getNativeWindow() const = 0;

        // MAIN-THREAD ONLY
        virtual bool startMouseCapture(bool hideCursor = true) = 0;
        virtual void endMouseCapture() = 0;
        [[nodiscard]] virtual bool isMouseCaptured() const = 0;

        [[nodiscard]] static std::unique_ptr<MWindow> create(const MWindowDesc& desc);
    };
}

#endif