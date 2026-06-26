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

    constexpr int WINDOWS_DEVICE_COUNT { 1+1+1+1+4 }; // keyboard + mouse + touchscreen + stylus + 4 * gamepad
                                                      // index with MDeviceType enum
    constexpr int WINDOWS_MONITOR_COUNT { 4 };
    constexpr int WINDOWS_GLOBAL_HANDLER_COUNT { 4 };
    constexpr int WINDOWS_WINDOW_COUNT { 4 };

    class MWindowImpl;
    class MGlobal {
    
        static MGlobal* ptr;
    public:
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

        struct MMonitorEntry {
            MMonitorID id;
            void* handle;
            std::string name;
        };
    //private:
        MInitConfig settings;

        std::size_t mask;
        std::size_t head;
        std::size_t tail;
        std::unique_ptr<MEventSlot[]> buffer;

        std::atomic<std::vector<MDeviceState>*> front_buf;
        std::atomic<std::vector<MDeviceState>*> back_buf;

        std::vector<MDeviceState> statebuf1;
        std::vector<MDeviceState> statebuf2;

        MMonitorID nextMonID;

        std::shared_mutex         window_lock;
        std::vector<MWindowEntry> windows;
        MWindowID                 nextWinID;

        std::mutex handler_lock;
        std::vector<MEventHandlerEntry> global_handlers;
        MEventHandlerID nextHandlerID;

        std::vector<MMonitorEntry> monitorEntrys;

        void* notificationHWND;

        MGlobal(const MInitConfig& config);

        void createNotificationWindow();
        void registerRawInputDevices();

        void onMonitorChange();
        void consumeAll();
        void switchBuffers();
        void executeGlobalHandlerChain(const MEvent& ev);
        bool shouldCoalesce(const MEvent& a);
        size_t findCoalescableEventIndex(const MEventSlot& ev, void* hwnd);
        void coalesceEvent(size_t index, const MEvent& ev);
    //public:
        static MGlobal* init(const MInitConfig& config = {});
        static MGlobal* Get();
        static void shutdown();

        ~MGlobal();

        std::shared_mutex     monitor_lock;
        std::vector<MMonitor> monitors;
        void enumerateMonitors(std::vector<MMonitor>& vec);
        std::vector<MDeviceState>* getFrontStatePtr() const;
        std::vector<MDeviceState>* getBackStatePtr()  const;
        bool mouseTracking;
        wchar_t pendingSurrogate; // for WM_CHAR surrogate pair handling
        bool shouldDeliverToFocused() const { return settings.deliverToFocused; }
        // Drains the event queue, walks each event through registered handler chains
        void poll();

        // Event queue push
        bool push(MEvent&& ev, void* hwnd);

        //bool isRunning();

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
        std::optional<MWindowID> getFocusedID();

        std::optional<MMonitorID> monIDFromHandle(void* hMon);
        std::optional<MMonitor>  monitorFromHandle(void* hMon);
        std::optional<void*> handleFromID(MWindowID id);

        MMonitor monitorFromPoint(MPoint pt);
        float dpiFromHandle(void* hMon);

        MPoint getCursorPos();

        // Key state query for between-event polling
        bool isKeyHeld(MKey key);
    };
}

#endif