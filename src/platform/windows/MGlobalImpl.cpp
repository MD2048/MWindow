#include "windows/MGlobalImpl.h"
#include <windows.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

}

namespace MW {
    std::vector<MDeviceInfo> MGlobal::devices(6);
    std::vector<MMonitor> MGlobal::monitors(4);

    void MGlobal::init(const MInitConfig& config) {
        
    }
    void MGlobal::shutdown() {}

    // Drains the event queue, walks each event through registered handler chains
    void MGlobal::poll() {}

    //bool isRunning();

    // Device query
    const std::vector<MDeviceInfo>& MGlobal::getConnectedDevices() {}

    // Monitor query
    const std::vector<MMonitor>& MGlobal::getMonitors() {}
    const MMonitor&              MGlobal::getPrimaryMonitor() {}

    float MGlobal::getCursorX() {}
    float MGlobal::getCursorY() {}

    // Key state query for between-event polling
    bool MGlobal::isKeyHeld(MKey key) {}
}