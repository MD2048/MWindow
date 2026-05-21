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

    //bool isRunning() {  }

    MEventHandlerID registerGlobalEventHandler(MEventHandler ha) {
        assert(ptr && "MWindow::registerGlobalEventHandler(): Initialize MWindow before calling this function!"); 
        return ptr->registerGlobalEventHandler(ha);
    }
    
    void unregisterGlobalEventHandler(MEventHandlerID id) {
        assert(ptr && "MWindow::unregisterGlobalEventHandler(): Initialize MWindow before calling this function!"); 
        ptr->unregisterGlobalEventHandler(id);
    }

    std::vector<MDeviceInfo> getConnectedDevices() {
        assert(ptr && "MWindow::getConnectedDevices(): Initialize MWindow before calling this function!"); 
        return ptr->getConnectedDevices();
    }

    std::optional<MDeviceState> getDeviceState(MDeviceID id) { 
        assert(ptr && "MWindow::getDeviceState(): Initialize MWindow before calling this function!"); 
        return ptr->getDeviceState(id); 
    }

    bool isDeviceConnected(MDeviceID id) {
        assert(ptr && "MWindow::isDeviceConnected(): Initialize MWindow before calling this function!"); 
        return ptr->isDeviceConnected(id); }

    // Monitor query

    std::vector<MMonitor> getConnectedMonitors() {
        assert(ptr && "MWindow::getConnectedMonitors(): Initialize MWindow before calling this function!"); 
        return ptr->getConnectedMonitors();
    }

    std::optional<MMonitor> getMonitor(MMonitorID id) { 
        assert(ptr && "MWindow::getMonitor(): Initialize MWindow before calling this function!"); 
        return ptr->getMonitor(id); }

    const MMonitor& getPrimaryMonitor() { 
        assert(ptr && "MWindow::getPrimaryMonitor(): Initialize MWindow before calling this function!"); 
        return ptr->getPrimaryMonitor(); }

    bool isMonitorConnected(MMonitorID id) { 
        assert(ptr && "MWindow::isMonitorConnected(): Initialize MWindow before calling this function!"); 
        return ptr->isMonitorConnected(id); }

    float getCursorX() { 
        assert(ptr && "MWindow::getCursorX(): Initialize MWindow before calling this function!"); 
        return ptr->getCursorX(); 
    }
    float getCursorY() { 
        assert(ptr && "MWindow::getCursorY(): Initialize MWindow before calling this function!"); 
        return ptr->getCursorY(); 
    }

    bool isKeyHeld(MKey key) {
        assert(ptr && "MWindow::isKeyHeld(): Initialize MWindow before calling this function!"); 
        return ptr->isKeyHeld(key); 
    }

    std::unique_ptr<MWindow> MWindow::create(const MWindowDesc& desc) {
        assert(ptr && "MWindow::create(): Initialize MWindow before calling this function!"); 
        return std::make_unique<MWindowImpl>(desc);
    }

    MWindow::MWindow()
    : maxHandlers{8}
    , handlerCount{0}
    , nextID{0}
    , handlers{std::vector<MEventHandler>(maxHandlers, nullptr)}
    , id_table{std::vector<MEventHandlerID>(maxHandlers, 0)}
    {
    }

    void MWindow::executeHandlerChain(const MEvent& ev)
    {
        for(size_t i{handlerCount-1};i >= 0;--i)
        {
            if(handlers[i](ev) == MEventResult::Consumed)
                break;
        }
    }

    MEventHandlerID MWindow::registerEventHandler(MEventHandler ha)
    {
        std::lock_guard<std::mutex> lock(consumerLock);

        if(handlerCount >= maxHandlers)
        {
            handlers.push_back(ha);
            id_table.push_back(nextID++);
            handlerCount++;
            maxHandlers++;
        }
        else
        {
            handlers[handlerCount] = ha;
            id_table[handlerCount++] = nextID++;
        }
        return nextID-1;
    }

    void MWindow::unregisterEventHandler(MEventHandlerID id)
    {
        std::lock_guard<std::mutex> lock(consumerLock);
        for(size_t i{0};i < handlerCount;++i)
        {
            if(id_table[i] == id)
            {
                for(size_t j{i};j < handlerCount-1;++j)
                    handlers[j] = handlers[j+1];
                
                for(size_t j{i};j < handlerCount-1;++j)
                {
                    id_table[j] = id_table[j+1];
                }
                handlerCount--;
                handlers[handlerCount] = nullptr;
                return;
            }
        }
        return;
    }

}