#ifndef M_GLOBALIMPL_H
#define M_GLOBALIMPL_H

#ifndef MPLATFORM_WINDOWS
    //#error "src/platform/windows/MGlobalImpl.h should not be included on the current platform!"
#endif

#include "MWindow/MMonitor.h"
#include "MWindow/MEvents.h"
#include "MWindow/MWindowInit.h"
#include "headers/MGlobalBase.h"

#include <vector>
#include <optional>


namespace MW {

    constexpr int WINDOWS_DEVICE_COUNT { 6 };
    constexpr int WINDOWS_MONITOR_COUNT { 4 };
    constexpr int WINDOWS_GLOBAL_HANDLER_COUNT { 4 };

    class MGlobal {
    private:
        static MGlobal* ptr;

        MGlobalBase base;
        std::vector<std::pair<void*, MMonitorID>> monitorHandles;
        std::vector<std::pair<void*, MDeviceID>> deviceHandles;
        void* notificationHWND;

        MGlobal(const MInitConfig& config);

        void createNotificationWindow();
        void registerRawInputDevices();

        void enumerateInputDevices();
        void enumerateMonitors();

        std::vector<std::pair<MDeviceID, MDeviceState>>* getStatePtr() const;
    public:
        static MGlobal* init(const MInitConfig& config = {});
        static MGlobal& Get();
        static void shutdown();

        ~MGlobal();

        // Drains the event queue, walks each event through registered handler chains
        void poll();

        //bool isRunning();

        // Device query
        std::vector<MDeviceInfo>    getConnectedDevices();
        std::optional<MDeviceState> getDeviceState(MDeviceID id);
        bool                        isDeviceConnected(MDeviceID id);

        // Monitor query
        std::vector<MMonitor>   getConnectedMonitors();
        std::optional<MMonitor> getMonitor(MMonitorID id);
        const MMonitor&         getPrimaryMonitor();
        bool                    isMonitorConnected(MMonitorID id);

        MEventHandlerID registerGlobalEventHandler(MEventHandler ha);
        void unregisterGlobalEventHandler(MEventHandlerID id);

        float getCursorX();
        float getCursorY();

        // Key state query for between-event polling
        bool isKeyHeld(MKey key);
    };
}

#endif