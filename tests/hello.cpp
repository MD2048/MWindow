#include "MWindow/MWindow.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace MW;

namespace {
    MEventResult printEvent(const MEvent& ev)
    {
        std::cout << "Global event: " << ev << '\n';
        return MEventResult::Consumed;
    }

    void printWindowApi(MWindow& window)
    {
        std::cout << "window id: " << window.getId() << '\n';
        std::cout << "alive: " << window.isAlive() << '\n';
        std::cout << "visible: " << window.isVisible() << '\n';

        window.setTitle("MWindow smoke test");
        std::cout << "title: " << window.getTitle() << '\n';

        window.resize({320.0f, 240.0f});
        std::cout << "size: " << window.getSize() << '\n';

        window.setTopLeftCorner({40.0f, 40.0f});
        std::cout << "top-left: " << window.getTopLeftCorner() << '\n';

        std::cout << "rect: " << window.getRect() << '\n';

        window.setWindowMode(MWindowMode::Windowed);
        std::cout << "mode: " << window.getWindowMode() << '\n';

        std::cout << "monitor: " << window.getCurrentMonitorID() << '\n';
        std::cout << "dpi: " << window.getDpiScale() << '\n';
        std::cout << "physical-size: " << window.getPhysicalSize() << '\n';

        auto native = window.getNativeWindow();
        std::cout << "native-window: " << native.index() << '\n';
    }
}

int main()
{
    MInitConfig config{};
    config.ignoreGamepads = false;
    config.eventQueueCapacity = 256;
    config.mouseMoveCoalescing = true;
    config.touchMoveCoalescing = true;
    config.scrollCoalescing = true;

    init(config);

    const auto globalHandler = registerGlobalEventHandler(&printEvent);

    const auto monitors = getConnectedMonitors();
    std::cout << "monitor-count: " << monitors.size() << '\n';
    for (const auto& monitor : monitors) {
        std::cout << "monitor: " << monitor << '\n';
        std::cout << "is-connected: " << isMonitorConnected(monitor.id) << '\n';
    }

    if (auto primary = getPrimaryMonitor()) {
        std::cout << "primary-monitor: " << *primary << '\n';
    }

    std::cout << "cursor: " << getCursorPos() << '\n';
    std::cout << "mods: " << getMods() << '\n';
    std::cout << "gamepad-slot-0-active: " << isGamepadSlotActive(MGamepadSlot{0}) << '\n';
    std::cout << "active-gamepads: " << getActiveGamepadSlots().size() << '\n';

    const auto inputState = getInputState();
    std::cout << "input-state-count: " << inputState.size() << '\n';

    MWindowDesc desc{};
    desc.title = "MWindow smoke test";
    desc.rect = {0.0f, 0.0f, 320.0f, 240.0f};
    desc.mode = MWindowMode::Windowed;
    desc.resizable = true;
    desc.decorated = true;
    desc.visible = true;
    desc.centered = true;

    if (auto primary = getPrimaryMonitor()) {
        desc.monitor = primary->id;
    }

    auto window = MWindow::create(desc);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        shutdown();
        return 1;
    }

    window->registerEventHandler([mw = window.get()](const MEvent& ev) {
        std::cout << "Window event: " << ev << '\n';
        if(std::holds_alternative<MCloseRequestEvent>(ev))
            mw->close();
        return MEventResult::Consumed;
    });

    printWindowApi(*window);

    for (int i = 0; i < 20 && window->isAlive(); ++i) {
        poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    window->close();
    poll();

    unregisterGlobalEventHandler(globalHandler);
    shutdown();
    return 0;
}