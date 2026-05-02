#ifndef M_GLOBALIMPL_H
#define M_GLOBALIMPL_H

#ifndef MPLATFORM_WINDOWS
    #error "src/platform/windows/MGlobalImpl.h should not be included on the current platform!"
#endif

#include "MWindow/MEvents.h"
#include "MWindow/MWindowInit.h"

#include <vector>


namespace MW {
    class MGlobal {
    private:
        static std::vector<MDeviceInfo> devices;
        static std::vector<MMonitor> monitors;
    public:
        static void init(const MInitConfig& config = {});
        static void shutdown();

        // Drains the event queue, walks each event through registered handler chains
        static void poll();

        //bool isRunning();

        // Device query
        static const std::vector<MDeviceInfo>& getConnectedDevices();

        // Monitor query
        static const std::vector<MMonitor>& getMonitors();
        static const MMonitor&              getPrimaryMonitor();

        static float getCursorX();
        static float getCursorY();

        // Key state query for between-event polling
        static bool isKeyHeld(MKey key);
    };
}

#endif