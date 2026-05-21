#include "headers/MGlobalBase.h"
#include "MWindow/MWindow.h"

namespace MW
{
    MGlobalBase::MGlobalBase(MInitConfig config, int deviceCount, int monitorCount, int handlerCount)
    : settings{config}
    , mask{settings.eventQueueCapacity-1}
    , head{0}
    , tail{0}
    , buffer{std::make_unique<MEventSlot[]>(config.eventQueueCapacity)}
    , nextMonID{0}
    , nextWinID{0}
    , nextDevID{0}
    , maxHandlers{static_cast<size_t>(handlerCount)}
    , handlerCount{0}
    , nextID{0}
    , global_handlers{std::vector<MEventHandler>(handlerCount, nullptr)}
    , id_stack{std::vector<MEventHandlerID>(handlerCount, 0)}
    {
        devices.reserve(deviceCount);
        statebuf1.reserve(deviceCount);
        statebuf2.reserve(deviceCount);
        monitors.reserve(monitorCount);

        front_buf.store(&statebuf1, std::memory_order_release);
        back_buf.store(&statebuf2, std::memory_order_release);

        setup_finished.store(false, std::memory_order_release);

        global_handlers.reserve(handlerCount);
    }
    
    bool MGlobalBase::push(MEventSlot&& ev)
    {
        const std::size_t h = head.load(std::memory_order_acquire);
        const std::size_t t = tail.load(std::memory_order_acquire);

        if((h - t) >= settings.eventQueueCapacity)
            return false; // buffer is full, drop

        MEventSlot& slot { buffer[h & mask] };

        slot = std::move(ev);
        
        head.fetch_add(1,std::memory_order_release);
        return true;
    }

    void MGlobalBase::consumeAll()
    {
        const std::size_t h = head.load(std::memory_order_acquire);
        std::size_t t = tail.load(std::memory_order_acquire);
        
        while(h != t)
        {
            MEventSlot& slot { buffer[t & mask] };
            if(slot.global)
            {
                executeGlobalHandlerChain(slot.event);
            }
            else {
                auto it = window_map.find(slot.id);
                if(it != window_map.end())
                    (*it).second->executeHandlerChain(slot.event);
            }
            t++;
            tail.fetch_add(1, std::memory_order_release);
        }

        switchBuffers();
    }

    void MGlobalBase::switchBuffers() {
        auto* f = front_buf.load(std::memory_order_acquire);
        auto* b = back_buf.exchange(f, std::memory_order_acquire);
        front_buf.store(b, std::memory_order_release);
    }


    void MGlobalBase::executeGlobalHandlerChain(const MEvent& ev) {
        for(size_t i{handlerCount-1};i >= 0;--i)
        {
            if(global_handlers[i](ev) == MEventResult::Consumed)
                break;
        }
    }
}