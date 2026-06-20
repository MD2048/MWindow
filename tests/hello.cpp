#include "MWindow/MWindow.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace MW;

MEventResult ha(const MEvent& ev)
{
    std::cout << ev << "\n";
    return MEventResult::Consumed;
}

int main()
{
    MInitConfig config {256, MCoalescePolicy::Latest, MCoalescePolicy::Latest, MCoalescePolicy::Accumulate};

    init(config);

    MEventHandlerID haid = registerGlobalEventHandler(&ha);
    //unregisterGlobalEventHandler(haid);


    // Device query
    std::vector<MDeviceInfo> devs = getConnectedDevices();
    for(auto& devinfo : devs)
    {
        std::cout << devinfo << "\n";
        std::cout << *getDeviceState(devinfo.id).value() << "\n";
        std::cout << std::boolalpha << isDeviceConnected(devinfo.id) << '\n';
    }
    //getDeviceState(MDeviceID id);

    // Monitor query
    std::vector<MMonitor> mons =   getConnectedMonitors();

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

    MWindowDesc d{};
    d.title = "EVO+";
    std::unique_ptr<MWindow> mw = MWindow::create(d);
    //std::this_thread::sleep_for(std::chrono::minutes(1));
    mw->registerEventHandler(
        [ptr = mw.get()](const MEvent& ev){
            if(std::holds_alternative<MCloseRequestEvent>(ev))
            {
                ptr->close();
            }
            return MEventResult::Consumed;
        });
    while(true)
    {
        poll();
    }

    shutdown();

    return 0;
}