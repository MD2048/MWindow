#ifndef M_GLOBALIMPL_H
#define M_GLOBALIMPL_H

#ifndef MPLATFORM_WINDOWS
    //#error "src/platform/windows/MGlobalImpl.h should not be included on the current platform!"
#endif

#include "MWindow/MMonitor.h"
#include "MWindow/MEvents.h"
#include "MWindow/MWindowInit.h"
#include "MWindow/MDevices.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include <vector>
#include <optional>


namespace MW {

    inline std::atomic<bool> setup_finished; // prevents a rare race between NotificationWndProc and enumerateInputDevices

    constexpr int WINDOWS_DEVICE_COUNT { 6 };
    constexpr int WINDOWS_MONITOR_COUNT { 4 };
    constexpr int WINDOWS_GLOBAL_HANDLER_COUNT { 4 };
    constexpr int WINDOWS_WINDOW_COUNT { 4 };

    class MWindowImpl;

    class MGlobal {
    private:
        static MGlobal* ptr;
        
        struct MWindowEntry {
            MWindowID    id;
            void*        hwnd;
            MWindowImpl* window;
        };

        struct MEventSlot{
            MEvent event;
            bool global;
            MWindowID id;
        };

        MInitConfig settings;

        std::size_t mask;
        alignas(64) std::atomic<std::size_t> head;
        alignas(64) std::atomic<std::size_t> tail;
        std::unique_ptr<MEventSlot[]> buffer;

        std::atomic<std::vector<std::pair<MDeviceID, MDeviceState>>*> front_buf;
        std::atomic<std::vector<std::pair<MDeviceID, MDeviceState>>*> back_buf;

        std::shared_mutex        dev_info_lock;
        std::vector<MDeviceInfo> devices;

        std::vector<std::pair<MDeviceID, MDeviceState>> statebuf1;
        std::vector<std::pair<MDeviceID, MDeviceState>> statebuf2;
        MDeviceID nextDevID;

        MMonitorID nextMonID;

        std::shared_mutex         window_lock;
        std::vector<MWindowEntry> windows;
        MWindowID                 nextWinID;

        std::mutex handler_lock;
        std::vector<MEventHandlerEntry> global_handlers;
        MEventHandlerID nextHandlerID;

        std::vector<std::pair<void*, MMonitorID>> monitorHandles;
        std::vector<std::pair<void*, MDeviceID>> deviceHandles;

        void* notificationHWND;

        MGlobal(const MInitConfig& config);

        void createNotificationWindow();
        void registerRawInputDevices();

        void enumerateInputDevices();

        void consumeAll();
        void switchBuffers();
        void executeGlobalHandlerChain(const MEvent& ev);
    public:
        static MGlobal* init(const MInitConfig& config = {});
        static MGlobal* Get();
        static void shutdown();

        ~MGlobal();

        std::shared_mutex     monitor_lock;
        std::vector<MMonitor> monitors;
        void enumerateMonitors(std::vector<MMonitor>& vec);
        std::vector<std::pair<MDeviceID, MDeviceState>>* getFrontStatePtr() const;
        std::vector<std::pair<MDeviceID, MDeviceState>>* getBackStatePtr()  const;
        // Drains the event queue, walks each event through registered handler chains
        void poll();

        // Event queue push
        bool push(MEventSlot&& ev); 

        //bool isRunning();

        // Device query
        std::vector<MDeviceInfo>    getConnectedDevices();
        std::optional<MDeviceState const*> getDeviceState(MDeviceID id);
        bool                        isDeviceConnected(MDeviceID id);

        // Monitor query
        std::vector<MMonitor>   getConnectedMonitors();
        std::optional<MMonitor> getMonitor(MMonitorID id);
        const MMonitor&         getPrimaryMonitor();
        bool                    isMonitorConnected(MMonitorID id);


        MEventHandlerID registerGlobalEventHandler(MEventHandler ha);
        void unregisterGlobalEventHandler(MEventHandlerID id);

        MWindowID registerWindow(void* hwnd, MWindowImpl* ptr);
        void unregisterWindow(MWindowID id);

        std::optional<MWindowID> idFromHWND(void* hwnd);
        std::optional<MWindowImpl*> ptrFromHWND(void* hwnd);
        std::optional<MWindowImpl*> ptrFromID(MWindowID id);

        std::optional<MMonitorID> monIDFromHandle(void* hMon);
        std::optional<MMonitor>  monitorFromHandle(void* hMon);
        std::optional<MMonitor> monitorFromID(MMonitorID id);

        std::optional<MDeviceID> devIDFromHandle(void* hDevice);

        MMonitor monitorFromPoint(MPoint pt);

        float getCursorX();
        float getCursorY();

        // Key state query for between-event polling
        bool isKeyHeld(MKey key);

        void onDeviceConnected(void* hDevice);
        void onDeviceDisconnected(void* hDevice);
    };
}

#endif