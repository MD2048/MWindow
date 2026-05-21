#include "windows/MGlobalImpl.h"
#include "windows/MWindowImpl.h"
#include "windows/MWindowsHelpers.h"

#include <windows.h>
#include <shellscalingapi.h>

LRESULT CALLBACK MWindowWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* win = reinterpret_cast<MW::MWindowImpl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!win) return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

        ptr->createNotificationWindow();
        ptr->registerRawInputDevices();
        ptr->enumerateInputDevices();
        ptr->enumerateMonitors();

        ptr->base.setup_finished.store(true,std::memory_order_release);

        return ptr;
    }

    MGlobal& MGlobal::Get() { return *ptr; }

    MGlobal::MGlobal(const MInitConfig& config) : base{config, WINDOWS_DEVICE_COUNT, WINDOWS_MONITOR_COUNT, WINDOWS_GLOBAL_HANDLER_COUNT}
    {
        monitorHandles.reserve(WINDOWS_MONITOR_COUNT);
        deviceHandles.reserve(WINDOWS_DEVICE_COUNT);
    }

    MGlobal::~MGlobal() = default;

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
        base.consumeAll();
    }

    //bool isRunning();

    // Device query
    std::vector<MDeviceInfo> MGlobal::getConnectedDevices() {
        std::shared_lock lock(base.dev_info_lock);
        return base.devices;
    }

    std::optional<MDeviceState> MGlobal::getDeviceState(MDeviceID id) {
        std::shared_lock lock(base.dev_info_lock);
        auto* ptr = getStatePtr();
        auto it   = std::find_if(ptr->begin(), ptr->end(), [id](const std::pair<MDeviceID, MDeviceState>& p){
                                                                return id == p.first;
                                                            });
        if(it == ptr->end())
            return std::nullopt;
        return (*it).second;
    }
    bool MGlobal::isDeviceConnected(MDeviceID id) {
        std::shared_lock lock(base.dev_info_lock);
        auto it = std::find_if(base.devices.begin(),base.devices.end(), [id](const MDeviceInfo& d) {
                                                                                return id == d.id;
                                                                          });
        return !(it == base.devices.end());
    }

    // Monitor query
    std::vector<MMonitor>        MGlobal::getConnectedMonitors() {
        std::shared_lock lock(base.monitor_lock);
        return base.monitors;
    }

    std::optional<MMonitor> MGlobal::getMonitor(MMonitorID id) {
        std::shared_lock lock(base.monitor_lock);
        auto it   = std::find_if(base.monitors.begin(), base.monitors.end(), [id](const MMonitor& m){
                                                                return id == m.id;
                                                            });
        if(it == base.monitors.end())
            return std::nullopt;
        return *it;
    }

    bool MGlobal::isMonitorConnected(MMonitorID id) {
        std::shared_lock lock(base.monitor_lock);
        auto it = std::find_if(base.monitors.begin(),base.monitors.end(), [id](const MMonitor& m) {
                                                                                return id == m.id;
                                                                          });
        return !(it == base.monitors.end());
    }

    const MMonitor&              MGlobal::getPrimaryMonitor() {
        std::shared_lock lock(base.monitor_lock);
        return *std::find_if(base.monitors.begin(), base.monitors.end(),[](const MMonitor& mm){
                                                        return mm.isPrimary;
                                                        });
    }

    std::vector<std::pair<MDeviceID, MDeviceState>>* MGlobal::getStatePtr() const { return base.front_buf.load(std::memory_order_acquire); }

    MEventHandlerID MGlobal::registerGlobalEventHandler(MEventHandler ha)
    {
        std::lock_guard<std::mutex> lock(base.consumerLock);

        if(base.handlerCount >= base.maxHandlers)
        {
            base.global_handlers.push_back(ha);
            base.id_stack.push_back(base.nextID++);
            base.handlerCount++;
            base.maxHandlers++;
        }
        else
        {
            base.global_handlers[base.handlerCount] = ha;
            base.id_stack[base.handlerCount++] = base.nextID++;
        }
        return base.nextID-1;
    }

    void MGlobal::unregisterGlobalEventHandler(MEventHandlerID id)
    {
        std::lock_guard<std::mutex> lock(base.consumerLock);
        for(size_t i{0};i < base.handlerCount;++i)
        {
            if(base.id_stack[i] == id)
            {
                for(size_t j{i};j < base.handlerCount-1;++j)
                    base.global_handlers[j] = base.global_handlers[j+1];
                
                for(size_t j{i};j < base.handlerCount-1;++j)
                {
                    base.id_stack[j] = base.id_stack[j+1];
                }
                base.handlerCount--;
                base.global_handlers[base.handlerCount] = nullptr;
                return;
            }
        }
        return;
    }


    float MGlobal::getCursorX() {}
    float MGlobal::getCursorY() {}

    // Key state query for between-event polling
    bool MGlobal::isKeyHeld(MKey key) {}

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

        auto* buf { base.back_buf.load(std::memory_order_acquire) };
        for (auto& entry : list)
        {
            MDeviceInfo di{};
            di.type = toMDeviceType(entry);
            
            if(di.type == MDeviceType::Unknown)
                continue;

            di.id = base.nextDevID;
            di.name = GetDeviceName(entry.hDevice);
            base.devices.push_back(di);

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
            buf->push_back({base.nextDevID, state});
            deviceHandles.push_back({reinterpret_cast<void*>(entry.hDevice), base.nextDevID++});
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
                mon.id                 = self->base.nextMonID;
                mon.name               = std::string(info.szDevice, info.szDevice + wcslen(info.szDevice));
                mon.rect               = rect;
                mon.dpiScale           = scale;
                mon.isPrimary          = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                mon.currentModeIndex   = currentIndex;
                mon.availableModes     = std::move(availableModes);
                mon.hdr                = queryHDRInfo(hMon, info.szDevice);

                std::unique_lock lock(self->base.monitor_lock); // lock for write
                
                self->monitorHandles.push_back({reinterpret_cast<void*>(hMon),self->base.nextMonID++});
                self->base.monitors.push_back(std::move(mon));

                return TRUE;
            },
            reinterpret_cast<LPARAM>(this)
        );
}
}