#include "MWindow/MWindow.h"

#ifdef MPLATFORM_WINDOWS
    #include "windows/MWindowImpl.h"
    #include "windows/MGlobalImpl.h"
#elif defined(MPLATFORM_LINUX)
#elif defined(MPLATFORM_MAC)
#elif defined(MPLATFORM_IOS)
#elif defined(MPLATFORM_ANDROID)
#endif

namespace MW {
    void init(const MInitConfig& config) { MGlobal::init(config); }
    void shutdown() { MGlobal::shutdown(); }

    void poll() { MGlobal::poll(); }

    //bool isRunning() {  }

    const std::vector<MDeviceInfo>& getConnectedDevices() { return MGlobal::getConnectedDevices(); }

    const std::vector<MMonitor>& getMonitors() { return MGlobal::getMonitors(); }
    const MMonitor&              getPrimaryMonitor() { return MGlobal::getPrimaryMonitor(); }

    float getCursorX() { return MGlobal::getCursorX(); }
    float getCursorY() { return MGlobal::getCursorY(); }

    bool isKeyHeld(MKey key) { return MGlobal::isKeyHeld(key); }

}