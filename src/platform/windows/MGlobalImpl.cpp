#include "windows/MGlobalImpl.h"
#include "windows/MWindowImpl.h"
#include "windows/MWindowsHelpers.h"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellscalingapi.h>

#include <limits>
#include <cassert>

LRESULT CALLBACK MWindowWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* global = reinterpret_cast<MW::MGlobal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!global) return DefWindowProc(hwnd, uMsg, wParam, lParam);

    switch (uMsg) {

        case WM_CLOSE:
            global->push({MW::MEvent{std::in_place_type<MW::MCloseEvent>}, false, global->idFromHWND(hwnd).value()});
            return 0;
        case WM_DESTROY:
            return 0; // MWindowImpl::close() has been already called at this point

        case WM_SIZE:
            auto* mw = global->ptrFromHWND(hwnd).value();
            auto* state = mw->getBackStatePtr();
            // LOWORD/HIWORD(lParam) = physical w/h
            UINT flag = (UINT)wParam;

            switch(flag)
            {
                case SIZE_MINIMIZED:
                    state->desc.visible=false;
                    global->push({MW::MEvent{std::in_place_type<MW::MVisibilityChangeEvent>, MW::MVisibilityChangeEvent{false}},
                         false, global->idFromHWND(hwnd).value()});
                    return 0;

                case SIZE_RESTORED:
                case SIZE_MAXIMIZED:
                    state->desc.visible=true;
            }
            global->push({MW::MEvent{std::in_place_type<MW::MVisibilityChangeEvent>, MW::MVisibilityChangeEvent{true}},
                         false, global->idFromHWND(hwnd).value()});
            
            float dpi = global->getMonitor(mw->getCurrentMonitorID()).value().dpiScale;
            float w = static_cast<float>(LOWORD(lParam))/dpi;
            float h = static_cast<float>(HIWORD(lParam))/dpi;

            if()
            
            global->push({MW::MEvent{std::in_place_type<MW::MResizeEvent>, MW::MResizeEvent{}},
                         false, global->idFromHWND(hwnd).value()});
            

        case WM_MOVE:
            // LOWORD/HIWORD(lParam) = physical x/y
            // → update state.topLeft (divide by scale)
            // → push MWindowMoveEvent

        case WM_WINDOWPOSCHANGED:
            // Fires after any size/position/zorder change
            // → update state.monitorId (MonitorFromWindow)

        case WM_DPICHANGED:
            // HIWORD(wParam) = new DPI
            // lParam = suggested physical RECT
            // → update state.dpiScale
            // → SetWindowPos with suggested rect
            // → push MWindowDpiChangedEvent

        case WM_SETFOCUS:
            // → push MWindowFocusEvent{ true }

        case WM_KILLFOCUS:
            // → push MWindowFocusEvent{ false }

        case WM_SHOWWINDOW:
            // wParam = TRUE/FALSE
            // → update state.visible
            // → push MWindowVisibilityEvent

        case WM_NCHITTEST:
            // Needed if you implement custom chrome / drag regions
            // → return HTCLIENT or custom regions

        case WM_ERASEBKGND:
            // Return 1 to prevent GDI clearing the window each frame
            return 1;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* win = reinterpret_cast<MW::MWindowImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!win) return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

namespace MW {

    MGlobal* MGlobal::init(const MInitConfig& config) {
        if(ptr) return ptr;

        ptr = new MGlobal{config};

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = MWindowWndProc;
        wc.lpszClassName = L"MWindow";
        RegisterClassExW(&wc);


        ptr->createNotificationWindow();
        ptr->registerRawInputDevices();
        ptr->enumerateInputDevices();
        ptr->enumerateMonitors();

        setup_finished.store(true,std::memory_order_release);

        return ptr;
    }

    MGlobal* MGlobal::Get() { return ptr; }

    MGlobal::MGlobal(const MInitConfig& config) : nextWinID{0}
    {
        // initialize merged MGlobalBase members
        settings = config;
        mask = settings.eventQueueCapacity - 1;
        head.store(0);
        tail.store(0);
        buffer = std::make_unique<MEventSlot[]>(settings.eventQueueCapacity);
        nextMonID = 0;
        nextDevID = 0;
        nextHandlerID = 0;

        devices.reserve(WINDOWS_DEVICE_COUNT);
        statebuf1.reserve(WINDOWS_DEVICE_COUNT);
        statebuf2.reserve(WINDOWS_DEVICE_COUNT);
        monitors.reserve(WINDOWS_MONITOR_COUNT);

        front_buf.store(&statebuf1, std::memory_order_release);
        back_buf.store(&statebuf2, std::memory_order_release);

        setup_finished.store(false, std::memory_order_release);

        global_handlers.reserve(WINDOWS_GLOBAL_HANDLER_COUNT);

        monitorHandles.reserve(WINDOWS_MONITOR_COUNT);
        deviceHandles.reserve(WINDOWS_DEVICE_COUNT);
        windows.reserve(WINDOWS_WINDOW_COUNT);
    }

    MGlobal::~MGlobal() {
        PostMessageW(reinterpret_cast<HWND>(notificationHWND), WM_CLOSE, 0, 0);
        for(auto& entry : windows)
        {
            delete entry.window;
        }
    }

    void MGlobal::shutdown() {
        delete ptr;
    }

    // Drains the event queue, walks each event through registered handler chains
    void MGlobal::poll() {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); // generates WM_CHAR from WM_KEYDOWN
            DispatchMessage(&msg);  // routes to WndProc
        }
        // After OS messages are drained, walk our event queue through handler chains
        consumeAll();
    }

    //bool isRunning();

    // Device query
    std::vector<MDeviceInfo> MGlobal::getConnectedDevices() {
        std::shared_lock lock(dev_info_lock);
        return devices;
    }

    std::optional<MDeviceState const*> MGlobal::getDeviceState(MDeviceID id) {
        auto* ptr = getStatePtr();
        auto it   = std::find_if(ptr->begin(), ptr->end(), [id](const std::pair<MDeviceID, MDeviceState>& p){
                                                                return id == p.first;
                                                            });
        if(it == ptr->end())
            return std::nullopt;
        return &((*it).second);
    }
    bool MGlobal::isDeviceConnected(MDeviceID id) {
        std::shared_lock lock(dev_info_lock);
          auto it = std::find_if(devices.begin(),devices.end(), [id](const MDeviceInfo& d) {
                                                          return id == d.id;
                                                      });
          return it != devices.end();
    }

    // Monitor query
    std::vector<MMonitor>        MGlobal::getConnectedMonitors() {
        std::shared_lock lock(monitor_lock);
        return monitors;
    }

    std::optional<MMonitor> MGlobal::getMonitor(MMonitorID id) {
        std::shared_lock lock(monitor_lock);
        auto it   = std::find_if(monitors.begin(), monitors.end(), [id](const MMonitor& m){
                                                                return id == m.id;
                                                            });
        if(it == monitors.end())
            return std::nullopt;
        return *it;
    }

    bool MGlobal::isMonitorConnected(MMonitorID id) {
        std::shared_lock lock(monitor_lock);
        auto it = std::find_if(monitors.begin(),monitors.end(), [id](const MMonitor& m) {
                                                                                return id == m.id;
                                                                          });
        return !(it == monitors.end());
    }

    const MMonitor&              MGlobal::getPrimaryMonitor() {
        std::shared_lock lock(monitor_lock);
        return *std::find_if(monitors.begin(), monitors.end(),[](const MMonitor& mm){
                                return mm.isPrimary;
                                });
    }

    std::vector<std::pair<MDeviceID, MDeviceState>>* MGlobal::getStatePtr() const { return front_buf.load(std::memory_order_acquire); }

    MWindowID MGlobal::registerWindow(void* hwnd, MWindowImpl* ptr) {
        std::unique_lock lock(window_lock);

        windows.push_back(MWindowEntry{nextWinID,hwnd,ptr});

        return nextWinID++;
    }
    void MGlobal::unregisterWindow(MWindowID id) {
        std::unique_lock lock(window_lock);

        auto it = std::find_if(windows.begin(), windows.end(),[id](const MWindowEntry& e) {
                                                            return e.id == id;
                                                        });

        if(it != windows.end())
            windows.erase(it);
    }

    std::optional<MWindowID> MGlobal::idFromHWND(void* hwnd) {
        std::shared_lock lock(window_lock);
        for(auto[id,hw,ptr] : windows)
        {
            if(hwnd == hw)
                return id;
        }
        return std::nullopt;
    }
    std::optional<MWindowImpl*> MGlobal::ptrFromHWND(void* hwnd) {
        std::shared_lock lock(window_lock);
        for(auto[id,hw,ptr] : windows)
        {
            if(hwnd == hw)
                return ptr;
        }
        return std::nullopt;
    }
    std::optional<MWindowImpl*> MGlobal::ptrFromID(MWindowID id) {
        std::shared_lock lock(window_lock);
        for(auto[i,hw,ptr] : windows)
        {
            if(i == id)
                return ptr;
        }
        return std::nullopt;
    }

    std::optional<MMonitorID> MGlobal::monIDFromHandle(void* hMon) {
        std::shared_lock lock(monitor_lock);

        for(auto[ha,id] : monitorHandles)
        {
            if(hMon == ha)
                return id;
        }
        return std::nullopt;
    }

    std::optional<MMonitor>  MGlobal::monitorFromHandle(void* hMon) {
        std::shared_lock lock(monitor_lock);

        MMonitorID id{0};
        for(auto[h,i] : monitorHandles)
        {
            if(h == hMon)
            {
                id = i;
                break;
            }
        }
        for(auto& mon : monitors)
        {
            if(mon.id == id)
                return mon;
        }
        return std::nullopt;
    }

    std::optional<MMonitor> MGlobal::monitorFromID(MMonitorID id) {
        std::shared_lock lock(monitor_lock);
        for(auto& mon : monitors)
        {
            if(mon.id == id)
                return mon;
        }
        return std::nullopt;
    }

    MMonitor MGlobal::monitorFromPoint(MPoint pt) {
        std::shared_lock lock(monitor_lock);
        assert(monitors.size() < 1 && "MWindow: Monitor list is empty, monitorFromPoint() has been called!");
        MMonitor& ans = monitors[0];
        float minDist = std::numeric_limits<float>::max();

        for(auto& mon : monitors)
        {
            if(mon.rect.contains(pt))
                return mon;
            
            float dx = std::max(0.0f, std::max(mon.rect.x - pt.x, pt.x - mon.rect.x + mon.rect.width));
            float dy = std::max(0.0f, std::max(mon.rect.y - pt.y, pt.y - mon.rect.y + mon.rect.height));

            float dist2 = dx*dx + dy*dy;
            if(dist2 < minDist)
            {
                minDist = dist2;
                ans = mon;
            }
        }
        return ans;
    }

    float MGlobal::getCursorX() {}
    float MGlobal::getCursorY() {}

    // Key state query for between-event polling
    bool MGlobal::isKeyHeld(MKey key) {}

    // Merged methods from MGlobalBase
    bool MGlobal::push(MEventSlot&& ev)
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

    void MGlobal::consumeAll()
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
                auto opt = ptrFromID(slot.id);
                if(opt)
                    (*opt)->executeHandlerChain(slot.event);
            }
            t++;
            tail.fetch_add(1, std::memory_order_release);
        }

        switchBuffers();
    }

    void MGlobal::switchBuffers() {
        auto* f = front_buf.load(std::memory_order_acquire);
        auto* b = back_buf.exchange(f, std::memory_order_acquire);
        front_buf.store(b, std::memory_order_release);
    }


    void MGlobal::executeGlobalHandlerChain(const MEvent& ev) {
        std::lock_guard<std::mutex> lock(handler_lock);

        for(size_t i{global_handlers.size()-1};i >= 0;--i)
        {
            if(global_handlers[i].handler(ev) == MEventResult::Consumed)
                break;
        }
    }

    MEventHandlerID MGlobal::registerGlobalEventHandler(MEventHandler ha) {
        std::lock_guard<std::mutex> lock(handler_lock);
        global_handlers.push_back(MEventHandlerEntry{nextHandlerID, ha});

        return nextHandlerID++;
    }
    
    void MGlobal::unregisterGlobalEventHandler(MEventHandlerID id) {
        std::lock_guard<std::mutex> lock(handler_lock);

        for(size_t i{0};i < global_handlers.size();++i)
        {
            if(global_handlers[i].id == id)
            {
                global_handlers.erase(global_handlers.begin() + i);
            }
        }
    }

    void MGlobal::createNotificationWindow()
    {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = NotificationWndProc;
        wc.lpszClassName = L"MWindowNotification";
        RegisterClassExW(&wc);

        notificationHWND = CreateWindowExW(
            0, L"MWindowNotification", nullptr, 0,
            0, 0, 0, 0,
            HWND_MESSAGE,  // message-only, never shown
            nullptr, nullptr, nullptr
        );

        // Store Global* so notificationWndProc can reach it
        SetWindowLongPtrW(reinterpret_cast<HWND>(notificationHWND), GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(this));
    }
    void MGlobal::registerRawInputDevices()
    {
        RAWINPUTDEVICE rids[2]{};

        // Keyboard
        rids[0].usUsagePage = 0x01; // HID Generic Desktop
        rids[0].usUsage     = 0x06; // Keyboard
        rids[0].dwFlags     = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        rids[0].hwndTarget  = reinterpret_cast<HWND>(notificationHWND);

        // Mouse
        rids[1].usUsagePage = 0x01;
        rids[1].usUsage     = 0x02; // Mouse
        rids[1].dwFlags     = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
        rids[1].hwndTarget  = reinterpret_cast<HWND>(notificationHWND);
    }

    void MGlobal::enumerateInputDevices()
    {
        UINT count = 0;
        GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST));

        std::vector<RAWINPUTDEVICELIST> list(count);
        GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));

        auto* buf { back_buf.load(std::memory_order_acquire) };
        for (auto& entry : list)
        {
            MDeviceInfo di{};
            di.type = toMDeviceType(entry);
            
            if(di.type == MDeviceType::Unknown)
                continue;

            di.id = nextDevID;
            di.name = GetDeviceName(entry.hDevice);
            devices.push_back(di);

            MDeviceState state { zeroInit(di.type) };
            if(di.type == MDeviceType::Mouse)
            {
                POINT p{};
                if (GetCursorPos(&p)) {
                    HMONITOR hMon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
                    UINT dpiX, dpiY;
                    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
                    float scale = dpiX / 96.f;

                    std::get<MMouseState>(state).x = p.x / scale;
                    std::get<MMouseState>(state).y = p.y / scale;
                }
            }
            buf->push_back({nextDevID, state});
            deviceHandles.push_back({reinterpret_cast<void*>(entry.hDevice), nextDevID++});
        }
    }
    void MGlobal::enumerateMonitors()
    {
        EnumDisplayMonitors(
            nullptr, nullptr,
            [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* self = reinterpret_cast<MGlobal*>(lParam);

                MONITORINFOEXW info{};
                info.cbSize = sizeof(info);
                GetMonitorInfoW(hMon, &info);

                UINT dpiX, dpiY;
                GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
                float scale = static_cast<float>(dpiX) / 96.f;

                const RECT& r = info.rcMonitor;
                MRect rect{};

                rect.x      = static_cast<float>(r.left)            / scale;
                rect.y      = static_cast<float>(r.top)             / scale;
                rect.width  = static_cast<float>(r.right  - r.left) / scale;
                rect.height = static_cast<float>(r.bottom - r.top)  / scale;

                // Available modes — deduplicate on all four fields now
                std::vector<MVideoMode> availableModes;
                DEVMODEW mode{};
                mode.dmSize = sizeof(mode);
                for (DWORD i = 0; EnumDisplaySettingsW(info.szDevice, i, &mode); ++i) {
                    if (mode.dmDisplayFrequency == 0 ||
                        mode.dmPelsWidth < 640 || mode.dmPelsHeight < 480)
                        continue;

                    MVideoMode vm{};
                    vm.widthPx        = mode.dmPelsWidth;
                    vm.heightPx       = mode.dmPelsHeight;
                    vm.refreshRate    = mode.dmDisplayFrequency;
                    vm.bitsPerChannel = dmBitsPerChannel(mode.dmBitsPerPel);
                    auto dup = std::find_if(
                        availableModes.begin(), availableModes.end(),
                        [&vm](const MVideoMode& m) {
                            return m.widthPx        == vm.widthPx        &&
                                m.heightPx       == vm.heightPx       &&
                                m.refreshRate    == vm.refreshRate     &&
                                m.bitsPerChannel == vm.bitsPerChannel;
                        });
                    if (dup == availableModes.end())
                        availableModes.push_back(vm);
                }

                // Current mode
                DEVMODEW dm{};
                dm.dmSize = sizeof(dm);
                EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &dm);
                MVideoMode currentMode{};
                currentMode.widthPx        = dm.dmPelsWidth;
                currentMode.heightPx       = dm.dmPelsHeight;
                currentMode.refreshRate    = dm.dmDisplayFrequency;
                currentMode.bitsPerChannel = dmBitsPerChannel(dm.dmBitsPerPel);
                std::size_t currentIndex{0};
                for(size_t i{0};i < availableModes.size();++i)
                {
                    if(currentMode == availableModes[i])
                    {
                        currentIndex = i;
                        break;
                    }
                }


                MMonitor mon{};
                mon.id                 = self->nextMonID;
                mon.name               = toNarrow(std::wstring(info.szDevice, info.szDevice + wcslen(info.szDevice)));
                mon.rect               = rect;
                mon.dpiScale           = scale;
                mon.isPrimary          = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                mon.currentModeIndex   = currentIndex;
                mon.availableModes     = std::move(availableModes);
                mon.hdr                = queryHDRInfo(hMon, info.szDevice);
                
                self->monitorHandles.push_back({reinterpret_cast<void*>(hMon),self->nextMonID++});
                self->monitors.push_back(std::move(mon));

                return TRUE;
            },
            reinterpret_cast<LPARAM>(this)
        );
}
}