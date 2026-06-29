#include "MWindow/MWindow.h"
#include <iostream>
#include <thread>
#include <chrono>

#include <windows.h>

using namespace MW;

MEventResult ha(const MEvent& ev)
{
    std::cout << ev << std::endl;
    return MEventResult::Consumed;
}

void tester(MWindow* mw,int ctr)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    switch(ctr)
    {
        case 0: std::cout << getCursorPos() << "\n"; break;
        case 1: std::cout << mw->getTopLeftCorner(); break;
        case 2: mw->setWindowMode(MWindowMode::Windowed); break;
        case 3: std::cout << mw->getRect(); break;
        case 4: std::this_thread::sleep_for(std::chrono::seconds(2)); mw->setWindowMode(MWindowMode::Fullscreen); break;
        case 5: std::cout << mw->getRect(); break;
        case 6: mw->setWindowMode(MWindowMode::Windowed); break;
        case 7: std::cout << mw->getRect(); break;
        case 8: std::cout << mw->getCurrentMonitorID(); break;
        case 9: std::cout << mw->getDpiScale(); break;
        case 10: std::cout << mw->getPhysicalSize(); break;
    }
}

int main()
{
    MInitConfig config {};
    config.ignoreGamepads = false;
    config.eventQueueCapacity = 256;
    config.mouseMoveCoalescing = true;
    config.touchMoveCoalescing = true;
    config.scrollCoalescing = true;

    init(config);

    MEventHandlerID haid = registerGlobalEventHandler(&ha);
    //unregisterGlobalEventHandler(haid);

    // Monitor query
    std::vector<MMonitor> mons = getConnectedMonitors();

    for(auto& mon : mons)
    {
        std::cout << mon << "\n";
        std::cout << isMonitorConnected(mon.id);
    }

    std::cout << getPrimaryMonitor().value();

    //float getCursorX();
    //float getCursorY();

    // Key state query for between-event polling
    //bool isKeyHeld(MKey key);
    auto mon = getPrimaryMonitor().value();

    MWindowDesc d{};
    d.title = "EVO+";
    d.rect    = {0,0,200,300};
    d.mode    = MWindowMode::Fullscreen;
    d.backend = MRendererBackend::None;
    d.resizable  = true;
    d.decorated  = true;
    d.visible    = true;
    d.centered   = true;

    d.monitor = getPrimaryMonitor().value().id;
    std::unique_ptr<MWindow> mw = MWindow::create(d);
    //std::this_thread::sleep_for(std::chrono::minutes(1));
    mw->registerEventHandler(
        [&mw](const MEvent& ev){
            if(std::holds_alternative<MCloseRequestEvent>(ev))
            {
                mw->close();
            }
            /*
            else if(std::holds_alternative<MResizeEvent>(ev))
            {
                HWND ha = reinterpret_cast<HWND>(mw->passHandle());
                RECT r;
                GetClientRect(reinterpret_cast<HWND>(ha),&r);
                std::cout << r.left << " " << r.top << " " << r.right-r.left << " " << r.bottom-r.top << "\n";
            }*/
            else if(std::holds_alternative<MKeyPressEvent>(ev))
            {
                const MKeyPressEvent& e = std::get<MKeyPressEvent>(ev);
                if(e.key == MKey::Q)
                    mw->setWindowMode(MWindowMode::Fullscreen);
                else if(e.key == MKey::W)
                    mw->setWindowMode(MWindowMode::Windowed);
                if(e.key == MKey::M)
                    std::cout << getMods();
            }
            std::cout << ev << std::endl;
            return MEventResult::Consumed;
        });
    int i{0};
    while(true)
    {
        poll();
        if(!mw->isAlive())
            break;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        /*
        std::thread t(tester,mw.get(),i);
        t.join();
        */
    }

    shutdown();

    return 0;
}