#ifndef M_GLOBALIMPL_H
#define M_GLOBALIMPL_H

#ifndef MPLATFORM_WINDOWS
    #error "src/platform/windows/MGlobalImpl.h should not be included on the current platform!"
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


#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <GameInput.h>

namespace MW {

    inline std::atomic<bool> setup_finished; // prevents a rare race between NotificationWndProc and enumerateInputDevices

    constexpr int WINDOWS_DEVICE_COUNT { 1+1+1+1+4 }; // keyboard + mouse + touchscreen + stylus + 4 * gamepad
                                                      // index with MDeviceType enum
    constexpr int WINDOWS_MONITOR_COUNT { 4 };
    constexpr int WINDOWS_GLOBAL_HANDLER_COUNT { 4 };
    constexpr int WINDOWS_WINDOW_COUNT { 4 };

    class MWindowImpl;
    class MGlobal {
    private:
        static MGlobal* ptr;

    public:
        struct MWindowEntry {
            MWindowID    id;
            void*        hwnd;
            MWindowImpl* window;
        };

        struct MEventSlot {
            MEvent event = std::monostate{};
            bool global  = false;
            MWindowID id = std::numeric_limits<uint64_t>::max();
        };

        struct MMonitorEntry {
            MMonitorID id;
            void* handle;
            std::string name;
        };

        struct MXInputEntry {
            unsigned long prevPacket;
            bool connected;
        };

        struct MGameInputEntry {
            APP_LOCAL_DEVICE_ID appId;
            IGameInputDevice* device;
            bool connected;
        };

        MInitConfig settings;

        // Event queue
        std::size_t mask;
        std::size_t head;
        std::size_t tail;
        std::unique_ptr<MEventSlot[]> buffer;

        // Device-state double-buffer
        std::atomic<std::vector<MDeviceState>*> front_buf;
        std::atomic<std::vector<MDeviceState>*> back_buf;
        std::vector<MDeviceState> statebuf1;
        std::vector<MDeviceState> statebuf2;

        // Window / monitor / handler registries
        std::shared_mutex window_lock;
        std::vector<MWindowEntry> windows;
        MWindowID nextWinID;

        std::mutex handler_lock;
        std::vector<MEventHandlerEntry> global_handlers;
        MEventHandlerID nextHandlerID;

        std::vector<MMonitorEntry> monitorEntrys;
        MMonitorID nextMonID;
        std::vector<MMonitor> monitors;

        void* notificationHWND;
        bool mouseTracking;
        wchar_t pendingSurrogate; // for WM_CHAR surrogate pair handling

        // Lifecycle and callbacks
        MGlobal(const MInitConfig& config);

        void initClock();
        void createNotificationWindow();
        void registerRawInputDevices();
        void decideGamepadBackend();
        void registerDeviceCallback();
        void checkConnectedGamepads(); // XInput

        void getGamepadInputG();
        void getGamepadInputX();    // XInput
        void onMonitorChange();
        void consumeAll();
        void switchBuffers();
        void executeGlobalHandlerChain(const MEvent& ev);
        bool canCoalesce(const MEventSlot& a, const MEventSlot& b);
        bool shouldCoalesce(const MEvent& a);
        size_t findCoalescableEventIndex(const MEventSlot& ev, void* hwnd);
        void coalesceEvent(size_t index, const MEvent& ev);

        static MGlobal* init(const MInitConfig& config = {});
        static MGlobal* Get();
        static void shutdown();

        ~MGlobal();

        // State access and query helpers
        void enumerateMonitors(std::vector<MMonitor>& vec);
        std::vector<MDeviceState>* getFrontStatePtr() const;
        std::vector<MDeviceState>* getBackStatePtr()  const;

        // Event queue
        void poll();
        bool push(MEvent&& ev, void* hwnd);

        // Clock
        static constexpr uint64_t microSecConstant { 1000000 };
        uint64_t qpcFreq;
        uint64_t qpcStart;
        MMicroSec getTimeNow();

        // Monitor query
        std::vector<MMonitor> getConnectedMonitors();
        std::optional<MMonitor> getMonitor(MMonitorID id);
        const MMonitor& getPrimaryMonitor();
        bool isMonitorConnected(MMonitorID id);

        // Global handlers and window registry
        MEventHandlerID registerGlobalEventHandler(MEventHandler ha);
        void unregisterGlobalEventHandler(MEventHandlerID id);

        MWindowID registerWindow(void* hwnd, MWindowImpl* ptr);
        void unregisterWindow(MWindowID id);

        std::optional<MWindowID> idFromHWND(void* hwnd);
        std::optional<MWindowImpl*> ptrFromHWND(void* hwnd);
        std::optional<MWindowImpl*> ptrFromID(MWindowID id);
        std::optional<MWindowID> getFocusedID();

        std::optional<MMonitorID> monIDFromHandle(void* hMon);
        std::optional<MMonitor> monitorFromHandle(void* hMon);
        std::optional<void*> handleFromID(MWindowID id);

        MMonitor monitorFromPoint(MPoint pt);
        float dpiFromHandle(void* hMon);

        MPoint getCursorPos();
        MMods getMods();
        bool isKeyHeld(MKey key);

        // Gamepads
        bool usingXInput;
        std::vector<MXInputEntry> gamepadsXI;
        bool isGamepadSlotActive(MGamepadSlot id) const;
        std::vector<MGamepadSlot> getActiveGamepadSlots() const;

        // XInput state
        bool gamepadMightHaveConnected;

        // GameInput state and device-change queue
        std::vector<MGameInputEntry> gamepadsGI;
        IGameInput* gameInput = nullptr;
        GameInputCallbackToken deviceToken;
        std::optional<MGamepadSlot> toGamepadID(const APP_LOCAL_DEVICE_ID& appId);
        static constexpr size_t DEVICE_CHANGE_BUFFER_CAPACITY{16};
        std::unique_ptr<MGameInputEntry[]> devChangeBuffer;
        alignas(64) std::atomic<size_t> d_head;
        alignas(64) std::atomic<size_t> d_tail;
        std::size_t d_mask; // capacity-1

        void pushDevChange(MGameInputEntry&& dc);
        bool popDevChange(MGameInputEntry& out);
    };
}

#endif