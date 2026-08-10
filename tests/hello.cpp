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

    // 8x8-block RGBA8 checkerboard, easy to eyeball in the titlebar/taskbar/Alt-Tab.
    MIconData makeCheckerIcon(uint32_t size, uint8_t r1, uint8_t g1, uint8_t b1,
                                             uint8_t r2, uint8_t g2, uint8_t b2)
    {
        MIconData icon;
        icon.width = size;
        icon.height = size;
        icon.pixels.resize(static_cast<size_t>(size) * size * 4);

        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                bool useFirst = ((x / 8) + (y / 8)) % 2 == 0;
                size_t i = (static_cast<size_t>(y) * size + x) * 4;
                icon.pixels[i + 0] = useFirst ? r1 : r2;
                icon.pixels[i + 1] = useFirst ? g1 : g2;
                icon.pixels[i + 2] = useFirst ? b1 : b2;
                icon.pixels[i + 3] = 255;
            }
        }
        return icon;
    }

    MIconData makeSolidIcon(uint32_t size, uint8_t r, uint8_t g, uint8_t b)
    {
        MIconData icon;
        icon.width = size;
        icon.height = size;
        icon.pixels.resize(static_cast<size_t>(size) * size * 4);
        for (size_t i = 0; i < icon.pixels.size(); i += 4) {
            icon.pixels[i + 0] = r;
            icon.pixels[i + 1] = g;
            icon.pixels[i + 2] = b;
            icon.pixels[i + 3] = 255;
        }
        return icon;
    }

    // Crosshair cursor, hotspot at the exact center (the pixel the OS tracks as "the pointer").
    MCursorData makeCrosshairCursor(uint32_t size, uint8_t r, uint8_t g, uint8_t b)
    {
        MCursorData cursor;
        cursor.width = size;
        cursor.height = size;
        cursor.pixels.assign(static_cast<size_t>(size) * size * 4, 0); // transparent background
        cursor.hotspotX = size / 2;
        cursor.hotspotY = size / 2;

        for (uint32_t i = 0; i < size; ++i) {
            for (uint32_t t = 0; t < 2; ++t) { // 2px thick lines
                size_t rowPix = (static_cast<size_t>(size / 2 + t) * size + i) * 4;
                size_t colPix = (static_cast<size_t>(i) * size + size / 2 + t) * 4;
                cursor.pixels[rowPix + 0] = r; cursor.pixels[rowPix + 1] = g;
                cursor.pixels[rowPix + 2] = b; cursor.pixels[rowPix + 3] = 255;
                cursor.pixels[colPix + 0] = r; cursor.pixels[colPix + 1] = g;
                cursor.pixels[colPix + 2] = b; cursor.pixels[colPix + 3] = 255;
            }
        }
        return cursor;
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
    desc.icon = makeCheckerIcon(32, 255, 0, 0, 0, 0, 255); // red/blue checkerboard
    desc.cursor = makeCrosshairCursor(32, 255, 255, 0);    // yellow crosshair

    if (auto primary = getPrimaryMonitor()) {
        desc.monitor = primary->id;
    }
    desc.monitor = 0;

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
        if (auto* kp = std::get_if<MKeyPressEvent>(&ev)) {
            if (kp->key == MKey::C) {
                if (mw->isMouseCaptured()) {
                    mw->endMouseCapture();
                    std::cout << "endMouseCapture() called\n";
                } else {
                    bool ok = mw->startMouseCapture(true);
                    std::cout << "startMouseCapture() -> " << (ok ? "true" : "false") << '\n';
                }
            }
            // Manual test: press T to toggle text input. While disabled, typing (any key,
            // including held/repeating ones) should produce no "MCharEvent" prints below,
            // while MKeyPressEvent/MKeyReleaseEvent keep arriving regardless.
            if (kp->key == MKey::T) {
                bool enabled = !mw->isTextInputEnabled();
                mw->setTextInputEnabled(enabled);
                std::cout << "setTextInputEnabled(" << (enabled ? "true" : "false") << ")\n";
            }
        }
        return MEventResult::Consumed;
    });

    printWindowApi(*window);

    const MIconData greenIcon = makeSolidIcon(32, 0, 255, 0);
    const MIconData checkerIcon = makeCheckerIcon(32, 255, 0, 0, 0, 0, 255);
    const MCursorData magentaCrosshair = makeCrosshairCursor(32, 255, 0, 255);

    for (int i = 0; window->isAlive(); ++i) {
        poll();

        if (i == 20) {
            window->setIcon(greenIcon);
        }
        if (i >= 30 && i < 80) {
            window->setIcon((i % 2 == 0) ? greenIcon : checkerIcon);
        }
        if (i == 90) {
            window->setCursor(magentaCrosshair); // runtime setCursor(): should update live while hovering
        }
        if (i % 20 == 0) {
            std::cout << "cursor pos: " << getCursorPos()
                      << " captured: " << window->isMouseCaptured() << '\n';
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    window->close();

    unregisterGlobalEventHandler(globalHandler);
    shutdown();
    return 0;
}