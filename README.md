# MWindow

MWindow is a thin, cross-platform C++17 window abstraction library for multi-threaded applications. It creates windows, receives input events, and hands native surface handles to your renderer. It does not render anything itself.

## Features

- Unified window API across desktop and mobile platforms
- Type-safe event system via `std::variant`
- Propagating event handler chains with explicit consume/continue control
- Logical coordinate system — DPI handling done internally
- Thread-safe state reads, command-queued state writes
- Window icon, set at creation or at runtime (raw RGBA8 pixel buffer, no codec deps)
- Custom cursor sprite, set at creation or at runtime (raw RGBA8 + hotspot, no codec deps)
- Mouse capture with optional cursor hiding (main-thread-only API — see docs/Threading.md)
- Gamepad support
- Touch and stylus support
- Monitor enumeration with HDR and DPI info
- Timestamps on input events in microseconds
- Configurable ring buffer with per-type coalescing

## Supported Platforms

| Platform | Backend | Status |
|---|---|---|
| Windows | Win32 | Implemented |
| macOS | Cocoa | Planned |
| Linux | Wayland + X11 fallback | Planned |
| iOS | UIKit | Planned |
| Android | ANativeActivity | Planned |

## Renderer Backends

MWindow exposes native surface handles — it does not render. Supported targets:

| Backend | Platforms |
|---|---|
| Vulkan | All |
| Metal | macOS, iOS |
| DirectX | Windows |
| OpenGL | All |

## Status

Experimental. Windows implementation complete. API may change as other platforms are added.

## License

Apache License Version 2.0

## Quick Start

```cpp
#include <MWindow/MWindow.h>

int main() {
    MW::init();

    MW::MWindowDesc desc;
    desc.title  = "Hello MWindow";
    desc.rect  = {0,0,1280,720};
    desc.centered = true;

    auto window = MW::MWindow::create(desc);
    window->show();

    window->registerEventHandler([](const MW::MEvent& e) {
        if (std::holds_alternative<MW::MCloseRequestEvent>(e))
            // handle close — destroy window or show save dialog
            return MW::MEventResult::Consumed;
        return MW::MEventResult::Continue;
    });

    while (window->isAlive()) {
        MW::poll();

        if (window->isVisible()) {
            // render using window->getNativeWindow()
        }
    }

    MW::shutdown();
}
```

## Installation

Add MWindow as a subdirectory:

```bash
cd your/project/dir
git clone https://github.com/MD2048/MWindow
```

Your CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.24)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(your_project)

add_subdirectory(MWindow)

add_executable(your_target main.cpp)
target_link_libraries(your_target PRIVATE MWindow)
```

Configure and build:

```bash
cmake -B build
cmake --build build
```

## See More

- [Architecture](docs/Architecture.md)
- [Events](docs/Events.md)
- [Coordinate System](docs/Coordinates.md)
- [Input](docs/Input.md)
- [Threading](docs/Threading.md)
