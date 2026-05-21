#ifndef M_GLOBALBASE_H
#define M_GLOBALBASE_H

#include "MWindow/MMonitor.h"
#include "MWindow/MEvents.h"
#include "MWindow/MWindowInit.h"
#include "MWindow/MDevices.h"

#include <map>
#include <utility>
#include <cstdint>
#include <atomic>
#include <memory>
#include <cassert>
#include <vector>
#include <mutex>
#include <shared_mutex>

namespace MW
{   
    static MInitConfig init_cfg;
    class MWindow;
    class MGlobalBase
    {
    public:
        struct MEventSlot{
            MEvent event;
            bool global;
            MWindowID id;
        };

    private:
        MInitConfig settings;
        std::size_t mask;
        alignas(64) std::atomic<std::size_t> head;
        alignas(64) std::atomic<std::size_t> tail;
        std::unique_ptr<MEventSlot[]> buffer;

    public:
        std::atomic<bool> setup_finished; // prevents a rare race between NotificationWndProc and enumerateInputDevices
        std::atomic<std::vector<std::pair<MDeviceID, MDeviceState>>*> front_buf;
        std::atomic<std::vector<std::pair<MDeviceID, MDeviceState>>*> back_buf;

        std::shared_mutex        dev_info_lock;
        std::vector<MDeviceInfo> devices;

        std::vector<std::pair<MDeviceID, MDeviceState>> statebuf1;
        std::vector<std::pair<MDeviceID, MDeviceState>> statebuf2;
        MDeviceID nextDevID;

        std::shared_mutex                            monitor_lock;
        std::vector<MMonitor> monitors;
        MMonitorID nextMonID;

        std::shared_mutex             window_lock;
        std::map<MWindowID, MWindow*> window_map;
        MWindowID nextWinID;

        std::size_t maxHandlers;
        std::mutex consumerLock;
        std::vector<MEventHandler> global_handlers;
        std::vector<MEventHandlerID> id_stack;
        std::size_t handlerCount;
        MEventHandlerID nextID;

        MGlobalBase(MInitConfig config, int deviceCount = 8, int monitorCount = 4, int handlerCount = 4);

        bool push(MEventSlot&& ev);

        void consumeAll();

        void switchBuffers();

        void executeGlobalHandlerChain(const MEvent& ev);
    };
} // namespace MW::internal


#endif