#include "MWindow/MWindow.h"

#include <cassert>

#ifdef MPLATFORM_WINDOWS
    #include "windows/MWindowImpl.h"
    #include "windows/MGlobalImpl.h"
#elif defined(MPLATFORM_LINUX)
#elif defined(MPLATFORM_MAC)
#elif defined(MPLATFORM_IOS)
#elif defined(MPLATFORM_ANDROID)
#endif

namespace MW {

    static MGlobal* ptr{ nullptr };

    void init(const MInitConfig& config) { ptr = MGlobal::init(config); }

    void shutdown() { 
        assert(ptr && "MWindow::shutdown(): Initialize MWindow before calling this function!"); 
        MGlobal::shutdown();
    }

    void poll() { 
        assert(ptr && "MWindow::poll(): Initialize MWindow before calling this function!"); 
        ptr->poll(); 
    }

    MEventHandlerID registerGlobalEventHandler(MEventHandler ha) {
        assert(ptr && "MWindow::registerGlobalEventHandler(): Initialize MWindow before calling this function!"); 
        return ptr->registerGlobalEventHandler(ha);
    }
    
    void unregisterGlobalEventHandler(MEventHandlerID id) {
        assert(ptr && "MWindow::unregisterGlobalEventHandler(): Initialize MWindow before calling this function!"); 
        ptr->unregisterGlobalEventHandler(id);
    }

    std::vector<MMonitor> getConnectedMonitors() {
        assert(ptr && "MWindow::getConnectedMonitors(): Initialize MWindow before calling this function!"); 
        return ptr->getConnectedMonitors();
    }

    std::optional<MMonitor> getMonitor(MMonitorID id) { 
        assert(ptr && "MWindow::getMonitor(): Initialize MWindow before calling this function!"); 
        return ptr->getMonitor(id); }

    std::optional<MMonitor> getPrimaryMonitor() { 
        assert(ptr && "MWindow::getPrimaryMonitor(): Initialize MWindow before calling this function!"); 
        return ptr->getPrimaryMonitor(); }

    bool isMonitorConnected(MMonitorID id) { 
        assert(ptr && "MWindow::isMonitorConnected(): Initialize MWindow before calling this function!"); 
        return ptr->isMonitorConnected(id); }

    
    bool isGamepadSlotActive(MGamepadSlot id) {
        assert(ptr && "MWindow::isGamepadSlotActive(): Initialize MWindow before calling this function!"); 
        return ptr->isGamepadSlotActive(id);
    }

    std::vector<MGamepadSlot> getActiveGamepadSlots() {
        assert(ptr && "MWindow::getActiveGamepadSlots(): Initialize MWindow before calling this function!"); 
        return ptr->getActiveGamepadSlots();
    }


    MPoint getCursorPos() { 
        assert(ptr && "MWindow::getCursorPos(): Initialize MWindow before calling this function!"); 
        return ptr->getCursorPos(); 
    }

    MMods getMods() {
        assert(ptr && "MWindow::getMods(): Initialize MWindow before calling this function!"); 
        return ptr->getMods(); 
    }

    bool isKeyHeld(MKey key) {
        assert(ptr && "MWindow::isKeyHeld(): Initialize MWindow before calling this function!"); 
        return ptr->isKeyHeld(key); 
    }

    const std::vector<MDeviceState>& getInputState() {
        assert(ptr && "MWindow::getInputState(): Initialize MWindow before calling this function!"); 
        return *(ptr->getFrontStatePtr()); 
    }

    [[nodiscard]] std::unique_ptr<MWindow> MWindow::create(const MWindowDesc& desc) {
        assert(ptr && "MWindow::create(): Initialize MWindow before calling this function!"); 
        return std::make_unique<MWindowImpl>(desc);
    }

    MWindow::MWindow()
    : nextID{0}
    {
        handlers.reserve(8);
    }

    void MWindow::executeHandlerChain(const MEvent& ev) {
        std::lock_guard<std::mutex> lock(handler_lock);
        if(!handlers.size()) return;
        for(int i{static_cast<int>(handlers.size()-1)};i >= 0;--i)
        {
            if(handlers[i].handler(ev) == MEventResult::Consumed)
                break;
        }
    }

    MEventHandlerID MWindow::registerEventHandler(MEventHandler ha) {
        std::lock_guard<std::mutex> lock(handler_lock);
        handlers.push_back(MEventHandlerEntry{nextID, ha});

        return nextID++;
    }
    
    void MWindow::unregisterEventHandler(MEventHandlerID id) {
        std::lock_guard<std::mutex> lock(handler_lock);

        for(size_t i{0};i < handlers.size();++i)
        {
            if(handlers[i].id == id)
            {
                handlers.erase(handlers.begin() + i);
            }
        }
    }

}