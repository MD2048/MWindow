#include "windows/MGlobalImpl.h"
#include "windows/MWindowImpl.h"
#include "windows/MWindowsHelpers.h"
#include "windows/MKeyTranslation.h"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <Dbt.h>
#include <Xinput.h>
#include <GameInput.h>

#include <limits>
#include <cassert>
#include <cmath>

#define MI_WP_SIGNATURE      0xFF515700
#define MI_WP_SIGNATURE_MASK 0xFFFFFF00
#define MI_WP_EVENTMASK      0x000000FF

namespace {
    struct GameInputButtonMap {
        GameInputGamepadButtons flag;
        MW::MGamepadButton button;
    };

    struct XInputButtonMap {
        USHORT flag;
        MW::MGamepadButton button;
    };

    void initializeDeviceStateBuffer(std::vector<MW::MDeviceState>& states) {
        states.assign(MW::WINDOWS_DEVICE_COUNT, {});
        states[static_cast<size_t>(MW::MDeviceType::Keyboard)] = {MW::MKeyboardState{}};
        states[static_cast<size_t>(MW::MDeviceType::Mouse)] = {MW::MMouseState{}};
        states[static_cast<size_t>(MW::MDeviceType::Touchscreen)] = {MW::MTouchState{}};
        states[static_cast<size_t>(MW::MDeviceType::Stylus)] = {MW::MStylusState{}};
        for (size_t i = static_cast<size_t>(MW::MDeviceType::Gamepad); i < MW::WINDOWS_DEVICE_COUNT; ++i) {
            states[i] = {MW::MGamepadState{}};
        }
        for (size_t i = 0; i < MW::WINDOWS_DEVICE_COUNT; ++i) {
            initializeDeviceState(states[i]);
        }
    }
}

static constexpr float TRIGGER_THRESHOLD {XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 255.f};
static constexpr float LEFT_STICK_THRESHOLD_POS {XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767.f};
static constexpr float LEFT_STICK_THRESHOLD_NEG {-(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32768.f)};
static constexpr float RIGHT_STICK_THRESHOLD_POS {XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32767.f};
static constexpr float RIGHT_STICK_THRESHOLD_NEG {-(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32768.f)};

static void CALLBACK OnDeviceConnected(
    _In_ GameInputCallbackToken callbackToken,
    _In_ void* context,
    _In_ IGameInputDevice* device,
    _In_ uint64_t timestamp,
    _In_ GameInputDeviceStatus currentStatus,
    _In_ GameInputDeviceStatus previousStatus) 
{
    MW::MGlobal* global {MW::MGlobal::Get()};
    if(!global) return;
    if (currentStatus & GameInputDeviceConnected) {
        global->pushDevChange({device->GetDeviceInfo()->deviceId,device,true});
    } else {
        global->pushDevChange({device->GetDeviceInfo()->deviceId,device,false});
    }
}

LRESULT CALLBACK MWindowWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* global = reinterpret_cast<MW::MGlobal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!global) return DefWindowProcW(hwnd, uMsg, wParam, lParam);

    auto opt = global->ptrFromHWND(hwnd);
    if(!opt) return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    auto* mw = opt.value();
    auto* state = mw->getBackStatePtr();

    switch (uMsg) {

        case WM_CLOSE:
        {
            global->push(MCloseRequestEvent{}, hwnd);
            return 1;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 1; // MWindowImpl::close() has been already called at this point
        }
        case WM_MOUSEMOVE: {
            if (!global->mouseTracking) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize    = sizeof(tme);
                tme.dwFlags   = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);

                global->mouseTracking = true;
                global->push(MMouseEnterEvent{}, hwnd);
            }
            return 0;
        }

    case WM_MOUSELEAVE: {
        global->mouseTracking = false;
        global->push(MMouseLeaveEvent{}, hwnd);
        return 0;
    }
        case WM_CHAR: {
            wchar_t utf16 = static_cast<wchar_t>(wParam);

            if (utf16 == 0)
                return 0;

            if (IS_HIGH_SURROGATE(utf16)) {
                global->pendingSurrogate = utf16;
                return 0;
            }

            std::wstring utf16str;
            if (IS_LOW_SURROGATE(utf16)) {
                if (global->pendingSurrogate != 0) {
                    utf16str += global->pendingSurrogate;
                    utf16str += utf16;
                    global->pendingSurrogate = 0;
                } else {
                    // Orphaned low surrogate: discard
                    return 0;
                }
            } else {
                global->pendingSurrogate = 0;
                utf16str += utf16;
            }

            int byteCount = WideCharToMultiByte(
                CP_UTF8, 0,
                utf16str.c_str(), static_cast<int>(utf16str.size()),
                nullptr, 0,
                nullptr, nullptr);

            if (byteCount <= 0) return 0;

            std::string utf8(byteCount, '\0');
            WideCharToMultiByte(
                CP_UTF8, 0,
                utf16str.c_str(), static_cast<int>(utf16str.size()),
                utf8.data(), byteCount,
                nullptr, nullptr);

            global->push(MCharEvent{ std::move(utf8) }, hwnd);
            return 0;
        }

        case WM_SYSCHAR:
            break;
        case WM_SIZE:
        {
            // LOWORD/HIWORD(lParam) = physical w/h
            UINT flag = (UINT)wParam;

            switch(flag)
            {
                case SIZE_MINIMIZED:
                    if(state->desc.visible)
                    {
                        state->desc.visible = false;
                        mw->setStateChange();
                        global->push(MVisibilityChangeEvent{false}, hwnd);
                    }
                    break;

                case SIZE_RESTORED:
                case SIZE_MAXIMIZED:
                    if(!state->desc.visible)
                    {
                        state->desc.visible=true;
                        mw->setStateChange();
                        global->push(MVisibilityChangeEvent{true}, hwnd);
            
                    }
                    break;
            }
            
            RECT& r {state->currentRect};
            LONG w = LOWORD(lParam);
            LONG h = HIWORD(lParam);

            if(!(r.right-r.left == w && r.bottom-r.top == h))
            {
                r.right += w - (r.right - r.left);
                r.bottom += h - (r.bottom - r.top);

                MMonitor new_mon = global->monitorFromHandle(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)).value();
                if(new_mon.id != state->desc.monitor)
                {
                    global->push(MMonitorChangeEvent{state->desc.monitor,new_mon.id}, hwnd);
                    state->desc.monitor = new_mon.id;
                    mw->setStateChange();
                }
                state->desc.rect.width = (float)w / new_mon.dpiScale;
                state->desc.rect.height = (float)h / new_mon.dpiScale;
                mw->setStateChange();
                global->push(MResizeEvent{state->desc.rect.size()}, hwnd);

            }
            
            return 1;
        }
        case WM_MOVE:
        {
            RECT& r {state->currentRect};
            RECT rc;
            GetWindowRect(hwnd, &rc);
            int x = rc.left;
            int y = rc.top;
            if(!(r.left == x && r.top == y))
            {
                LONG dfx = x - r.left;
                LONG dfy = y - r.top;
                r.left += dfx; r.right  += dfx;
                r.top  += dfy; r.bottom += dfy;
                
                MMonitor new_mon = global->monitorFromHandle(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)).value();
                if(new_mon.id != state->desc.monitor)
                {
                    global->push(MMonitorChangeEvent{state->desc.monitor,new_mon.id}, hwnd);
                    state->desc.monitor = new_mon.id;
                    mw->setStateChange();
                }
                state->desc.rect.x = (float)x / new_mon.dpiScale;
                state->desc.rect.y = (float)y / new_mon.dpiScale;
                mw->setStateChange();
                global->push(MMoveEvent{state->desc.rect.topLeft()}, hwnd);
            }

            return 1;
        }
        case WM_WINDOWPOSCHANGED:
            break;

        case WM_DPICHANGED:
        {
            float dpiScale = (float)LOWORD(wParam) / 96.0f;
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);

            SetWindowPos(hwnd, nullptr, 
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
            return 1;
        }
        case WM_SETFOCUS:
        {
            if(state->focused != true)
            {
                state->focused = true;
                mw->setStateChange();
                global->push(MFocusChangeEvent{true}, hwnd);   
            }

            return 1;
        }
        case WM_KILLFOCUS:
        {
            if(state->focused != false)
            {
                state->focused = false;
                mw->setStateChange();
                global->push(MFocusChangeEvent{false}, hwnd);   
            }

            return 1;
        }
        case WM_SHOWWINDOW:
        {
            bool vis = wParam;
            
            if(state->desc.visible != vis)
            {
                state->desc.visible = vis;
                mw->setStateChange();
                global->push(MVisibilityChangeEvent{vis}, hwnd);
            }

            return 1;
        }
        case WM_NCHITTEST:
            // DND
            break;
        case WM_ERASEBKGND:
            // Return 1 to prevent GDI clearing the window each frame
            return 1;

        case WM_POINTERENTER: {
            POINTER_INFO pi{};
            if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pi)) return 0;
            MMicroSec ts = global->getTimeNow();

            if (pi.pointerType == PT_PEN) {
                float scale = global->dpiFromHandle(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST));
                MPoint pos  = { pi.ptPixelLocation.x / scale,
                                pi.ptPixelLocation.y / scale };
                
                auto& stylusst = std::get<MStylusState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Stylus)]);
                stylusst.inRange = true;
                stylusst.pos     = pos;

                global->push(MStylusEnterEvent{ts,pos}, hwnd);
            }
            return 0;
        }
        case WM_POINTERLEAVE: {
            POINTER_INFO pi{};
            if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pi)) return 0;
            MMicroSec ts = global->getTimeNow();

            if (pi.pointerType == PT_PEN) {
                float scale = global->dpiFromHandle(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST));
                MPoint pos  = { pi.ptPixelLocation.x / scale,
                                pi.ptPixelLocation.y / scale };
                
                auto& stylusst = std::get<MStylusState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Stylus)]);
                stylusst.inRange   = false;
                stylusst.inContact = false;
                stylusst.pos       = pos;

                global->push(MStylusLeaveEvent{ts,pos},hwnd);
            }
            return 0;
        }
        case WM_POINTERDOWN: {
            POINTER_INFO pi{};
            if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pi)) return 0;
            MMicroSec ts = global->getTimeNow();

            float scale = global->dpiFromHandle(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST));
            MPoint  pos   = { pi.ptPixelLocation.x / scale,
                            pi.ptPixelLocation.y / scale };

            if (pi.pointerType == PT_TOUCH) {
                auto& touchst = std::get<MTouchState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Touchscreen)]);
                
                uint32_t id = pi.pointerId;

                touchst.activePoints[id] = pos;

                global->push(MTouchBeginEvent{ts,id,pos},hwnd);
            }
            else if (pi.pointerType == PT_PEN) {
                auto& stylusst = std::get<MStylusState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Stylus)]);

                stylusst.inContact = true;
                stylusst.inRange   = true;
                stylusst.pos       = pos;

                global->push(MStylusDownEvent{ts,pos},hwnd);
            }
            return 0;
        }
        case WM_POINTERUPDATE: {
            POINTER_INFO pi{};
            if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pi)) return 0;
            MMicroSec ts = global->getTimeNow();

            float scale = global->dpiFromHandle(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST));
            MPoint   pos   = { pi.ptPixelLocation.x / scale,
                            pi.ptPixelLocation.y / scale };

            if (pi.pointerType == PT_TOUCH) {
                auto& touchst = std::get<MTouchState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Touchscreen)]);

                uint32_t id  = pi.pointerId;
                auto     it  = touchst.activePoints.find(id);
                if (it == touchst.activePoints.end()) return 0;

                MPoint& old = (*it).second;
                float dx = pos.x - old.x;
                float dy = pos.y - old.y;

                old = pos;

                global->push(MTouchMoveEvent{ts,id,pos,dx,dy},hwnd);
            }
            else if (pi.pointerType == PT_PEN) {
                auto& stylusst = std::get<MStylusState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Stylus)]);

                float dx = pos.x - stylusst.pos.x;
                float dy = pos.y - stylusst.pos.y;
                stylusst.pos = pos;

                bool inContact = (pi.pointerFlags & POINTER_FLAG_INCONTACT);

                if (inContact)
                    global->push(MStylusMoveEvent{ts,pos,dx,dy},hwnd);
                else
                    global->push(MStylusHoverEvent{ts,pos,dx,dy},hwnd);
            }
            return 0;
        }
        case WM_POINTERUP: {
            POINTER_INFO pi{};
            if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pi)) return 0;
            MMicroSec ts = global->getTimeNow();

            float scale = global->dpiFromHandle(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST));
            MPoint   pos   = { pi.ptPixelLocation.x / scale,
                            pi.ptPixelLocation.y / scale };

            if (pi.pointerType == PT_TOUCH) {
                auto& touchst = std::get<MTouchState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Touchscreen)]);

                uint32_t id = pi.pointerId;
                touchst.activePoints.erase(id);

                global->push(MTouchEndEvent{ts,id,pos},hwnd);
            }
            else if (pi.pointerType == PT_PEN) {
                auto& stylusst = std::get<MStylusState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Stylus)]);

                stylusst.inContact = false;
                stylusst.pos       = pos;

                global->push(MStylusUpEvent{ts,pos},hwnd);
            }
            return 0;
        }
        case WM_POINTERCAPTURECHANGED: {
            POINTER_INFO pi{};
            if (!GetPointerInfo(GET_POINTERID_WPARAM(wParam), &pi)) return 0;
    MMicroSec ts = global->getTimeNow();

            if (pi.pointerType == PT_TOUCH) {
                auto& touchst = std::get<MTouchState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Touchscreen)]);
                uint32_t id = pi.pointerId;
                touchst.activePoints.erase(id);

                global->push(MTouchCancelEvent{ts,id},hwnd);
            }
            else if (pi.pointerType == PT_PEN) {
                auto& stylusst = std::get<MStylusState>((*global->getBackStatePtr())[static_cast<size_t>(MDeviceType::Stylus)]);
                stylusst.inContact = false;
                stylusst.inRange   = false;

                global->push(MStylusCancelEvent{ts},hwnd);
            }
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* global = reinterpret_cast<MW::MGlobal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!global) return DefWindowProcW(hwnd, uMsg, wParam, lParam);

    switch(uMsg)
    {
        case WM_INPUT:
        {
            UINT size = 0;
            GetRawInputData(
                reinterpret_cast<HRAWINPUT>(lParam),
                RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));

            std::vector<BYTE> buf(size);
            if (GetRawInputData(
                    reinterpret_cast<HRAWINPUT>(lParam),
                    RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) != size)
                return 0;

            auto* raw = reinterpret_cast<RAWINPUT*>(buf.data());
            //if(raw->header.hDevice == nullptr) return 0;
            
            MMicroSec ts = global->getTimeNow();

            switch (raw->header.dwType) {

                case RIM_TYPEKEYBOARD: {
                    const auto& kb = raw->data.keyboard;

                    MKey key;
                    if (kb.Flags & RI_KEY_E1) 
                        key = MKey::Pause;
                    else
                        key = translateMakeCode(kb.MakeCode,kb.Flags & RI_KEY_E0);
                    if(key == MKey::Unknown) return 1; // NOTE: Unknown keys are ignored!!

                    auto* state = global->getBackStatePtr();

                    auto& kbs = std::get<MKeyboardState>((*state)[(size_t)MW::MDeviceType::Keyboard]);
                    bool pressed = !(kb.Flags & RI_KEY_BREAK);
                    if(!pressed && !kbs.held.test(static_cast<size_t>(key)))
                        return 1;
                    if(pressed && kbs.held.test(static_cast<size_t>(key)))
                        return 1;
                    kbs.mods.update(key,pressed);
                    if(pressed)
                    {
                        kbs.held.set(static_cast<size_t>(key));
                        global->push(MKeyPressEvent{ ts, key, kbs.mods}, nullptr);
                    }
                    else
                    {
                        kbs.held.reset(static_cast<size_t>(key));
                        global->push(MKeyReleaseEvent{ ts, key, kbs.mods}, nullptr);
                    }
                    break;
                }

                case RIM_TYPEMOUSE: {
                    const auto& ms = raw->data.mouse;

                    auto* state = global->getBackStatePtr();

                    auto& msst = std::get<MMouseState>((*state)[(size_t)MW::MDeviceType::Mouse]);

                    if (ms.lLastX != 0 || ms.lLastY != 0) {
                        float dx, dy;

                        if (ms.usFlags & MOUSE_MOVE_ABSOLUTE) {
                            // Normalize [0,65535] → virtual desktop physical coords
                            bool isVirtualDesktop = (ms.usFlags & MOUSE_VIRTUAL_DESKTOP);
                            int desktopW = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
                            int desktopH = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

                            float physX = (ms.lLastX  / 65535.f) * desktopW;
                            float physY = (ms.lLastY  / 65535.f) * desktopH;

                            POINT p{ (LONG)physX, (LONG)physY };
                            HMONITOR hMon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
                            float scale   = global->dpiFromHandle(hMon);

                            float newX = physX / scale;
                            float newY = physY / scale;

                            dx = newX - msst.p.x;
                            dy = newY - msst.p.y;
                            msst.p.x = newX;
                            msst.p.y = newY;
                        } else {
                            float scale = global->monitorFromPoint(msst.p).dpiScale;

                            dx = ms.lLastX / scale;
                            dy = ms.lLastY / scale;
                        }

                        global->push(MMouseMoveEvent{ts, dx, dy}, nullptr);
                    }

                    // multiple buttons can change in one message
                    struct ButtonMapping {
                        USHORT      downFlag;
                        USHORT      upFlag;
                        MMouseButton button;
                    };

                    static constexpr ButtonMapping kButtonMap[] = {
                        { RI_MOUSE_LEFT_BUTTON_DOWN,   RI_MOUSE_LEFT_BUTTON_UP,   MMouseButton::Left   },
                        { RI_MOUSE_RIGHT_BUTTON_DOWN,  RI_MOUSE_RIGHT_BUTTON_UP,  MMouseButton::Right  },
                        { RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, MMouseButton::Middle },
                        { RI_MOUSE_BUTTON_4_DOWN,      RI_MOUSE_BUTTON_4_UP,      MMouseButton::X1     },
                        { RI_MOUSE_BUTTON_5_DOWN,      RI_MOUSE_BUTTON_5_UP,      MMouseButton::X2     },
                    };

                    for (auto& [downFlag, upFlag, button] : kButtonMap) {
                        if (ms.usButtonFlags & downFlag) {
                            msst.buttons.set(static_cast<size_t>(button));
                            global->push(MMouseButtonPressEvent{ ts, button, msst.p,
                                std::get<MKeyboardState>((*state)[(size_t)MW::MDeviceType::Keyboard]).mods}, nullptr);
                        }
                        if (ms.usButtonFlags & upFlag) {
                            msst.buttons.reset(static_cast<size_t>(button));
                            global->push(MMouseButtonReleaseEvent{ ts, button, msst.p,
                            std::get<MKeyboardState>((*state)[(size_t)MW::MDeviceType::Keyboard]).mods}, nullptr);
                        }
                    }

                    if (ms.usButtonFlags & RI_MOUSE_WHEEL) {
                        float dy = static_cast<short>(ms.usButtonData) / (float)WHEEL_DELTA;
                        global->push(MMouseScrollEvent{ts, 0.f, dy,
                        std::get<MKeyboardState>((*state)[(size_t)MW::MDeviceType::Keyboard]).mods}, nullptr);
                    }

                    if (ms.usButtonFlags & RI_MOUSE_HWHEEL) {
                        float dx = static_cast<short>(ms.usButtonData) / (float)WHEEL_DELTA;
                        global->push(MMouseScrollEvent{ts, dx, 0.f,
                        std::get<MKeyboardState>((*state)[(size_t)MW::MDeviceType::Keyboard]).mods}, nullptr);
                    }
                    break;
                }
                case RIM_TYPEHID: {
                    // handled in WM_POINTER
                    break;
            }
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
        case WM_DISPLAYCHANGE:
        {
            std::vector<MMonitor> newM;
            global->enumerateMonitors(newM);

            std::vector<MMonitor>& oldM {global->monitors};

            for (auto& curr : newM) {
                auto it = std::find_if(oldM.begin(), oldM.end(),
                    [&](const MMonitor& p) { return p.id == curr.id; });

                if (it == oldM.end()) {
                    continue;
                }

                auto& prev = *it;
                MDisplayChangeFlags flags{};
                flags.scale       = (curr.dpiScale != prev.dpiScale);
                flags.rect        = (curr.rect != prev.rect);
                flags.resolution  = (curr.currentMode().widthPx     != prev.currentMode().widthPx ||
                                    curr.currentMode().heightPx    != prev.currentMode().heightPx);
                flags.refreshRate = (curr.currentMode().refreshRate  != prev.currentMode().refreshRate);
                flags.bitDepth    = (curr.currentMode().bitsPerChannel != prev.currentMode().bitsPerChannel);
                flags.hdrState    = (curr.hdr.active               != prev.hdr.active);

                // Only push if something actually changed
                if (flags.resolution || flags.refreshRate || flags.bitDepth || flags.hdrState || flags.scale || flags.rect)
                {
                    if(flags.resolution || flags.refreshRate || flags.bitDepth)
                    {
                        for(size_t i{0};i < prev.availableModes.size();++i)
                        {
                            if(prev.availableModes[i] == curr.currentMode())
                            {
                                prev.currentModeIndex = i;
                                break;
                            }
                        }
                    }

                    prev.hdr.active = curr.hdr.active;
                    prev.rect = curr.rect;
                    prev.dpiScale = curr.dpiScale;

                    global->push(MDisplaySettingChangeEvent{prev, flags, curr.id}, nullptr);
                }
            }
        }
        global->onMonitorChange();
        break;

        case WM_DEVICECHANGE:
        {
            std::vector<MMonitor> newM;
            global->enumerateMonitors(newM);

            std::vector<MMonitor>& oldM {global->monitors};

            switch(wParam)
            {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVNODES_CHANGED:
                    global->gamepadMightHaveConnected = true;
                case DBT_DEVICEREMOVECOMPLETE:
                {
                    for (const auto& old_mon : oldM) {
                        auto it = std::find_if(newM.begin(), newM.end(),
                            [&old_mon](const MMonitor& m) { return m.id == old_mon.id; });
                        if (it == newM.end()) {
                            global->push(MDisplayDisconnectedEvent{old_mon.id}, nullptr);
                        }
                    }

                    for (const auto& new_mon : newM) {
                        auto it = std::find_if(oldM.begin(), oldM.end(),
                            [&new_mon](const MMonitor& m) { return m.id == new_mon.id; });
                        if (it == oldM.end()) {
                            global->push(MDisplayConnectedEvent{new_mon.id}, nullptr);
                        }
                    }
                    global->onMonitorChange();
                    oldM = std::move(newM);
                    return 0;
                }
            }
        }
        default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

namespace MW {

    MGlobal* MGlobal::ptr = nullptr;

    MGlobal* MGlobal::init(const MInitConfig& config) {
        if(ptr) return ptr;

        ptr = new MGlobal{config};

        BOOL ok = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (!ok)
        {
            DWORD err = GetLastError();
            assert(ok);
        }
        EnableMouseInPointer(FALSE);
        buildScanTable();

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = MWindowWndProc;
        wc.lpszClassName = L"MWindow";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExW(&wc);

        ptr->initClock();
        ptr->createNotificationWindow();
        ptr->registerRawInputDevices();
        ptr->enumerateMonitors(ptr->monitors);
        if(!config.ignoreGamepads)
            ptr->decideGamepadBackend();

        ptr->statebuf1 = ptr->statebuf2;

        setup_finished.store(true,std::memory_order_release);

        return ptr;
    }

    MGlobal* MGlobal::Get() { return ptr; }

    MGlobal::MGlobal(const MInitConfig& config) 
    : nextWinID{0}
    , nextMonID{0}
    , nextHandlerID{0}
    , head{0}
    , tail{0}
    , notificationHWND{nullptr}
    , mouseTracking{false}
    , gamepadMightHaveConnected{false}
    , pendingSurrogate{}
    {
        settings = config;
        mask = settings.eventQueueCapacity - 1;
        buffer = std::make_unique<MEventSlot[]>(settings.eventQueueCapacity);

        statebuf2 = std::vector<MDeviceState>(WINDOWS_DEVICE_COUNT);
        initializeDeviceStateBuffer(statebuf2);
        statebuf1 = statebuf2;

        devChangeBuffer = std::make_unique<MGameInputEntry[]>(DEVICE_CHANGE_BUFFER_CAPACITY);
        d_head.store(0,std::memory_order_release);
        d_tail.store(0,std::memory_order_release);
        d_mask = DEVICE_CHANGE_BUFFER_CAPACITY - 1;

        front_buf.store(&statebuf1, std::memory_order_release);
        back_buf.store(&statebuf2, std::memory_order_release);

        setup_finished.store(false, std::memory_order_release);

        global_handlers.reserve(WINDOWS_GLOBAL_HANDLER_COUNT);
        monitorEntrys.reserve(WINDOWS_MONITOR_COUNT);
        monitors.reserve(WINDOWS_MONITOR_COUNT);
        windows.reserve(WINDOWS_WINDOW_COUNT);
    }

    MGlobal::~MGlobal() {
        uint64_t temp{};
        if(deviceToken)
            gameInput->UnregisterCallback(deviceToken, temp);
        if(gameInput) {
            gameInput->Release();
            gameInput = nullptr;
        }
        PostMessageW(reinterpret_cast<HWND>(notificationHWND), WM_CLOSE, 0, 0);
        for(auto& entry : windows)
        {
            delete entry.window;
        }
    }

    void MGlobal::shutdown() {
        delete ptr;
    }

    void MGlobal::poll() {

        for(auto& entry : windows)
        {
            entry.window->handleStateRequests();
        }
        
        assert(IsWindow(reinterpret_cast<HWND>(notificationHWND)));

        POINT p{};
        GetCursorPos(&p);
        HANDLE hMon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
        float scale = dpiFromHandle(hMon);
        auto* state = getBackStatePtr();
        auto& msst = std::get<MMouseState>((*state)[(size_t)MW::MDeviceType::Mouse]);
        msst.p.x = (float)p.x / scale;
        msst.p.y = (float)p.y / scale;

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if(!settings.ignoreGamepads)
        {
            if(usingXInput)
                getGamepadInputX();
            else
                getGamepadInputG();
        }
            
        switchBuffers();
        consumeAll();
    }

    std::vector<MMonitor> MGlobal::getConnectedMonitors() {
        return monitors;
    }

    std::optional<MMonitor> MGlobal::getMonitor(MMonitorID id) {
        auto it   = std::find_if(monitors.begin(), monitors.end(), [id](const MMonitor& m){
                                                                return id == m.id;
                                                            });
        if(it == monitors.end())
            return std::nullopt;
        return *it;
    }

    bool MGlobal::isMonitorConnected(MMonitorID id) {
        auto it = std::find_if(monitors.begin(),monitors.end(), [id](const MMonitor& m) {
                                                                                return id == m.id;
                                                                          });
        return !(it == monitors.end());
    }

    const MMonitor&              MGlobal::getPrimaryMonitor() {
        return *std::find_if(monitors.begin(), monitors.end(),[](const MMonitor& mm){
                                return mm.isPrimary;
                                });
    }

    std::vector<MDeviceState>* MGlobal::getFrontStatePtr() const { return front_buf.load(std::memory_order_acquire); }
    std::vector<MDeviceState>* MGlobal::getBackStatePtr()  const { return back_buf.load(std::memory_order_acquire); }

    bool MGlobal::isGamepadSlotActive(MGamepadSlot id) const {
        if(settings.ignoreGamepads) return false;
        if(usingXInput)
            return gamepadsXI[id].connected;
        else
            return gamepadsGI[id].connected;
    }

    std::vector<MGamepadSlot> MGlobal::getActiveGamepadSlots() const {
        std::vector<MGamepadSlot> v;
        if(usingXInput) {
            for(size_t i{0};i < gamepadsXI.size();++i) {
                if(gamepadsXI[i].connected)
                    v.push_back(i);
            }
        }
        else {
            for(size_t i{0};i < gamepadsGI.size();++i) {
                if(gamepadsGI[i].connected)
                    v.push_back(i);
            }
        }
        return v;
    }

    std::optional<MGamepadSlot> MGlobal::toGamepadID(const APP_LOCAL_DEVICE_ID& appId) {
        for(size_t i{0};i < gamepadsGI.size();++i) {
            if(!std::memcmp(gamepadsGI[i].appId.value,appId.value,APP_LOCAL_DEVICE_ID_SIZE))
                return i;
        }
        return std::nullopt;
    }

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

    std::optional<MWindowID> MGlobal::getFocusedID() {
        std::shared_lock lock(window_lock);
        for(auto[id,hw,ptr] : windows)
        {
            if(ptr->getBackStatePtr()->focused)
                return id;
        }
        return std::nullopt;
    }

    std::optional<MMonitorID> MGlobal::monIDFromHandle(void* hMon) {

        for(auto&[id,ha,s] : monitorEntrys)
        {
            if(hMon == ha)
                return id;
        }
        return std::nullopt;
    }
    std::optional<void*> MGlobal::handleFromID(MWindowID id)
    {

        for(auto&[i,ha,s] : monitorEntrys)
        {
            if(id == i)
                return ha;
        }
        return std::nullopt;
    }

    std::optional<MMonitor>  MGlobal::monitorFromHandle(void* hMon) {

        MMonitorID mon_id{0};
        for(auto&[id,ha,s] : monitorEntrys)
        {
            if(ha == hMon)
            {
                mon_id = id;
                break;
            }
        }
        for(auto& mon : monitors)
        {
            if(mon.id == mon_id)
                return mon;
        }
        return std::nullopt;
    }

    MMonitor MGlobal::monitorFromPoint(MPoint pt) {

        assert(monitors.size() != 0 && "MWindow: Monitor list is empty, monitorFromPoint() has been called!");
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

    float MGlobal::dpiFromHandle(void* hMon) {

        for(auto&[id,ha,s] : monitorEntrys)
        {
            if(ha == hMon)
            {
                for(auto& mon : monitors)
                {
                    if(mon.id == id)
                        return mon.dpiScale;
                }
            }
        }
        return 1.f;
    }

    MPoint MGlobal::getCursorPos() {
        return std::get<MMouseState>((*getBackStatePtr())[(size_t)MW::MDeviceType::Mouse]).p;
    }

    MMods MGlobal::getMods() {
        return std::get<MKeyboardState>((*getBackStatePtr())[(size_t)MW::MDeviceType::Keyboard]).mods;
    }

    bool MGlobal::isKeyHeld(MKey key) {
        return std::get<MKeyboardState>((*getBackStatePtr())[(size_t)MW::MDeviceType::Keyboard]).held.test(static_cast<size_t>(key));
    }

    bool MGlobal::push(MEvent&& ev, void* hwnd)
    {
        const std::size_t h = head;
        const std::size_t t = tail;

        if((h - t) >= settings.eventQueueCapacity)
            return false; // buffer is full, drop

        MEventSlot slot { std::move(ev), false, 0 };
        if(hwnd) {
            auto opt = idFromHWND(hwnd);
            if(!opt)
                return false;
            slot.id = *opt;
            slot.global = false;
        }
        else {
            if(shouldBeDeliveredToFocused(ev))
            {
                auto opt = getFocusedID();
                if(!opt)
                    return false;
                slot.id = *opt;
                slot.global = false;
            }
            else {
                slot.id = 0;
                slot.global = true;
            }
        }

        if(shouldCoalesce(ev))
        {
            size_t index = findCoalescableEventIndex(slot, hwnd);
            if(index != std::numeric_limits<size_t>::max())
            {
                coalesceEvent(index, ev);
                return true;
            }
        }
        MEventSlot& s { buffer[h & mask] };

        s = slot;
        
        ++head;
        return true;
    }

    void MGlobal::consumeAll()
    {
        const std::size_t h = head;
        std::size_t t = tail;
        
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
            ++t;
            ++tail;
            slot.event = std::monostate{};
            slot.global = false;
            slot.id = 0;
        }
    }

    bool MGlobal::shouldCoalesce(const MEvent& a) {
        return std::visit(Overloaded {
            [b = settings.mouseMoveCoalescing](const MMouseMoveEvent& ev)   { return b; },
            [b = settings.scrollCoalescing](const MMouseScrollEvent& ev)    { return b; },
            [b = settings.touchMoveCoalescing](const MTouchMoveEvent& ev)   { return b; },
            [b = settings.stylusMoveCoalescing](const MStylusMoveEvent& ev) { return b; },
            [b = settings.stylusMoveCoalescing](const MStylusHoverEvent& ev){ return b; },

            [](const MResizeEvent& ev) { return true; },
            [](const MMoveEvent& ev) { return true; },
            [](const MVisibilityChangeEvent& ev) { return true; },
            [](const MFocusChangeEvent& ev) { return true; },
            [](const MCharEvent& ev) { return true; },

            [b = settings.gamepadCoalescing](const MGamepadTriggerEvent& ev) { return true; },
            [b = settings.gamepadCoalescing](const MGamepadStickEvent& ev) { return true; },

            [](const auto&) { return false; }
        }, a);
    }
    bool MGlobal::canCoalesce(const MEventSlot& a, const MEventSlot& b) {
        if(a.global != b.global)
            return false;
        if(a.id != b.id)
            return false;
        return std::visit(Overloaded {
            [](const MMouseMoveEvent& ev1, const MMouseMoveEvent& ev2) { return true; },
            [](const MMouseScrollEvent& ev1, const MMouseScrollEvent& ev2) { return true; },
            [](const MTouchMoveEvent& ev1, const MTouchMoveEvent& ev2) { return ev1.id == ev2.id; },
            [](const MStylusMoveEvent& ev1, const MStylusMoveEvent& ev2) { return true; },
            [](const MStylusHoverEvent& ev1, const MStylusHoverEvent& ev2) { return true; },

            [](const MResizeEvent& ev1, const MResizeEvent& ev2) { return true; },
            [](const MMoveEvent& ev1, const MMoveEvent& ev2) { return true; },
            [](const MVisibilityChangeEvent& ev1, const MVisibilityChangeEvent& ev2) { return true; },
            [](const MFocusChangeEvent& ev1, const MFocusChangeEvent& ev2) { return true; },
            [](const MCharEvent& ev1, const MCharEvent& ev2) { return true; },

            [](const MGamepadTriggerEvent& ev1, const MGamepadTriggerEvent& ev2) { return (ev1.id == ev2.id) && (ev1.left == ev2.left); },
            [](const MGamepadStickEvent& ev1, const MGamepadStickEvent& ev2) { return (ev1.id == ev2.id) && (ev1.left == ev2.left); },

            [](const auto&, const auto&) { return false; }
        }, a.event, b.event);
    }
    size_t MGlobal::findCoalescableEventIndex(const MEventSlot& ev, void* hwnd) {
        std::size_t h = head-1;
        std::size_t t = tail-1;
        while(h != t)
        {
            MEventSlot& slot { buffer[h & mask] };
            if(canCoalesce(slot, ev))
                return h & mask;
            h--;
        }
        return std::numeric_limits<size_t>::max();
    }
    void MGlobal::coalesceEvent(size_t index, const MEvent& ev) {
        MEventSlot& slot { buffer[index] };
        std::visit(Overloaded {
            [ev](MMouseMoveEvent& e) { e.timestamp = std::get<MMouseMoveEvent>(ev).timestamp; 
                e.dx += std::get<MMouseMoveEvent>(ev).dx; 
                e.dy += std::get<MMouseMoveEvent>(ev).dy; 
            },
            [ev](MMouseScrollEvent& e) { e.timestamp = std::get<MMouseScrollEvent>(ev).timestamp;
                e.dx = std::get<MMouseScrollEvent>(ev).dx;
                e.dy = std::get<MMouseScrollEvent>(ev).dy;
                e.mods = std::get<MMouseScrollEvent>(ev).mods;
            },
            [ev](MTouchMoveEvent& e) { e.timestamp = std::get<MTouchMoveEvent>(ev).timestamp;
                e.new_pos = std::get<MTouchMoveEvent>(ev).new_pos;
                e.dx += std::get<MTouchMoveEvent>(ev).dx;
                e.dy += std::get<MTouchMoveEvent>(ev).dy;
            },
            [ev](MStylusMoveEvent& e) { e.timestamp = std::get<MStylusMoveEvent>(ev).timestamp;
                e.new_pos = std::get<MStylusMoveEvent>(ev).new_pos;
                e.dx += std::get<MStylusMoveEvent>(ev).dx;
                e.dy += std::get<MStylusMoveEvent>(ev).dy;
            },
            [ev](MStylusHoverEvent& e) { e.timestamp = std::get<MStylusHoverEvent>(ev).timestamp;
                e.new_pos = std::get<MStylusHoverEvent>(ev).new_pos;
                e.dx += std::get<MStylusHoverEvent>(ev).dx;
                e.dy += std::get<MStylusHoverEvent>(ev).dy;
            },

            [ev](MResizeEvent& e) { e.new_size = std::get<MResizeEvent>(ev).new_size; },
            [ev](MMoveEvent& e) { e.new_pos = std::get<MMoveEvent>(ev).new_pos; },
            [ev](MVisibilityChangeEvent& e) { e.isVisible = std::get<MVisibilityChangeEvent>(ev).isVisible; },
            [ev](MFocusChangeEvent& e) { e.focused = std::get<MFocusChangeEvent>(ev).focused; },
            [ev](MCharEvent& e) { e.input += std::get<MCharEvent>(ev).input; },

            [ev](MGamepadTriggerEvent& e) { auto& gt = std::get<MGamepadTriggerEvent>(ev);
                e.timestamp = gt.timestamp;
                e.new_val = gt.new_val;
                e.d += gt.d;
            },
            [ev](MGamepadStickEvent& e) { auto& gs = std::get<MGamepadStickEvent>(ev);
                e.timestamp = gs.timestamp;
                e.new_val = gs.new_val;
                e.dx += gs.dx;
                e.dy += gs.dy;
            },

            [](auto&) {}
        }, slot.event);
    }

    void MGlobal::switchBuffers() {
        auto* f = front_buf.load(std::memory_order_acquire);
        auto* b = back_buf.exchange(f, std::memory_order_acquire);
        front_buf.store(b, std::memory_order_release);
        *f = *b;
        for(auto& entry : windows)
            entry.window->syncState();
    }

    MMicroSec MGlobal::getTimeNow() {
        LARGE_INTEGER l;
        QueryPerformanceCounter(&l);
        return (l.QuadPart - qpcStart) * microSecConstant / qpcFreq;
    }

    void MGlobal::executeGlobalHandlerChain(const MEvent& ev) {
        std::lock_guard<std::mutex> lock(handler_lock);

        if(!global_handlers.size()) return;

        for(int i{static_cast<int>(global_handlers.size()-1)};i >= 0;--i)
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

    void MGlobal::onMonitorChange() {
        std::shared_lock lock(window_lock);

        for(auto& entry : windows)
        {
            entry.window->onMonitorChange();
        }
    }
    
    void MGlobal::initClock() {
        LARGE_INTEGER temp;
        QueryPerformanceFrequency(&temp);
        qpcFreq = temp.QuadPart;

        QueryPerformanceCounter(&temp);
        qpcStart = temp.QuadPart;
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
            WS_OVERLAPPED,
            nullptr, nullptr, GetModuleHandle(nullptr)
        );

        SetWindowLongPtrW(reinterpret_cast<HWND>(notificationHWND), GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(this));
    }
    void MGlobal::registerRawInputDevices()
    {
        RAWINPUTDEVICE rids[2]{};

        // Keyboard
        rids[0].usUsagePage = 0x01; // HID Generic Desktop
        rids[0].usUsage     = 0x06; // Keyboard
        rids[0].dwFlags     = RIDEV_INPUTSINK;
        rids[0].hwndTarget  = reinterpret_cast<HWND>(notificationHWND);

        // Mouse
        rids[1].usUsagePage = 0x01;
        rids[1].usUsage     = 0x02; // Mouse
        rids[1].dwFlags     = RIDEV_INPUTSINK;
        rids[1].hwndTarget  = reinterpret_cast<HWND>(notificationHWND);

        BOOL ok = RegisterRawInputDevices(rids, 2, sizeof(RAWINPUTDEVICE));

        if (!ok)
        {
            #ifdef MWINDOW_BUILD_PRINTS
            std::cout << GetLastError() << '\n';
            #endif
            assert(ok);
        }
    }

    void MGlobal::pushDevChange(MGameInputEntry&& dc) {
        const size_t h = d_head.load(std::memory_order_relaxed);
        const size_t t = d_tail.load(std::memory_order_acquire);

        if((h - t) >= DEVICE_CHANGE_BUFFER_CAPACITY) {
            return;
        }

        devChangeBuffer[h & d_mask] = std::move(dc);

        d_head.store(h + 1, std::memory_order_release);
    }

    bool MGlobal::popDevChange(MGameInputEntry& out) {
        const size_t t = d_tail.load(std::memory_order_relaxed);
        const size_t h = d_head.load(std::memory_order_acquire);

        if(t == h) {
            return false;
        }

        out = devChangeBuffer[t & d_mask];

        d_tail.store(t + 1, std::memory_order_release);
        return true;
    }

    void MGlobal::decideGamepadBackend() {
        HRESULT hr = GameInputCreate(&gameInput);
        if (SUCCEEDED(hr)) {
            gamepadsGI = std::vector<MGameInputEntry>(DEFAULT_GAMEPAD_COUNT);
            usingXInput = false;
            registerDeviceCallback();
        }
        else {
            gamepadsXI = std::vector<MXInputEntry>(DEFAULT_GAMEPAD_COUNT);
            usingXInput = true;
            checkConnectedGamepads();
        }
    }

    void MGlobal::registerDeviceCallback() {
        gameInput->RegisterDeviceCallback(
            nullptr,
            GameInputKindGamepad,
            GameInputDeviceAnyStatus,
            GameInputBlockingEnumeration,
            this,
            OnDeviceConnected,
            &deviceToken
        );
    }

    void MGlobal::getGamepadInputG() {
        MMicroSec ts = getTimeNow();
        auto* state = getBackStatePtr();
        MGameInputEntry c{};
        while(popDevChange(c))
        {
            if(c.connected) {
                size_t i{0};
                for(;i < gamepadsGI.size()+1;++i)
                {
                    if(i == gamepadsGI.size()) {
                        gamepadsGI.push_back(c);
                        (*state).push_back(MGamepadState{true,{},{},{},0.f,0.f});
                        break;
                    }
                    if(!gamepadsGI[i].connected)
                    {
                        gamepadsGI[i].connected = true;
                        std::get<MGamepadState>((*state)[static_cast<size_t>(MDeviceType::Gamepad)+i]).connected = true;
                        break;
                    }
                }
                push(MGamepadConnectedEvent{i},nullptr);
            }
            else {
                auto opt = toGamepadID(c.appId);
                if(!opt) continue;
                MGamepadSlot id = opt.value();

                push(MGamepadDisconnectedEvent{id}, nullptr);
                gamepadsGI[id] = {{},nullptr,false};
                std::get<MGamepadState>((*state)[static_cast<size_t>(MDeviceType::Gamepad)+id]) = {false,{},{},{},0.f,0.f};
            }
        }

        for(size_t i{0};i < gamepadsGI.size();++i)
        {
            IGameInputReading* reading = nullptr;

            if (SUCCEEDED(gameInput->GetCurrentReading(GameInputKindGamepad, gamepadsGI[i].device, &reading))) {
                
                GameInputGamepadState gamepadState;
                if (reading->GetGamepadState(&gamepadState)) {
                    
                    static constexpr GameInputButtonMap bMap[] = {
                        { GameInputGamepadDPadUp,              MGamepadButton::DpadUp },
                        { GameInputGamepadDPadDown,            MGamepadButton::DpadDown },
                        { GameInputGamepadDPadLeft,            MGamepadButton::DpadLeft },
                        { GameInputGamepadDPadRight,           MGamepadButton::DpadRight },
                        { GameInputGamepadMenu,                MGamepadButton::Start },
                        { GameInputGamepadView,                MGamepadButton::Select },
                        { GameInputGamepadA,                   MGamepadButton::ActionBottom },
                        { GameInputGamepadB,                   MGamepadButton::ActionRight },
                        { GameInputGamepadX,                   MGamepadButton::ActionLeft },
                        { GameInputGamepadY,                   MGamepadButton::ActionTop },
                        { GameInputGamepadLeftShoulder,       MGamepadButton::BumperLeft },
                        { GameInputGamepadRightShoulder,      MGamepadButton::BumperRight },
                        { GameInputGamepadLeftThumbstick,     MGamepadButton::ThumbLeft },
                        { GameInputGamepadRightThumbstick,    MGamepadButton::ThumbRight }
                    };

                    auto& gpst = std::get<MGamepadState>((*state)[static_cast<size_t>(MDeviceType::Gamepad) + i]);
                    const uint64_t buttonBits = static_cast<uint64_t>(gamepadState.buttons);
                    for (auto [flag, bt] : bMap) {
                        const bool isPressed = (buttonBits & static_cast<uint64_t>(flag)) != 0;
                        if (isPressed != gpst.held[static_cast<size_t>(bt)]) {
                            if (isPressed) {
                                push(MGamepadButtonPressEvent{ts, i, bt}, nullptr);
                                gpst.held.set(static_cast<size_t>(bt));
                            } else {
                                push(MGamepadButtonReleaseEvent{ts, i, bt}, nullptr);
                                gpst.held.reset(static_cast<size_t>(bt));
                            }
                        }
                    }
                    auto normalizeTrigger = [](float tr){
                        if(tr < TRIGGER_THRESHOLD)          // using the thresholds from XInput
                            return 0.f;
                        return tr;
                    };
                    float leftTrigger  = normalizeTrigger(gamepadState.leftTrigger);
                    float rightTrigger = normalizeTrigger(gamepadState.rightTrigger);

                    if(leftTrigger != gpst.leftTrigger)
                    {
                        push(MGamepadTriggerEvent{ts,i,true,leftTrigger,leftTrigger-gpst.leftTrigger}, nullptr);
                        gpst.leftTrigger = leftTrigger;
                    }
                    if(rightTrigger != gpst.rightTrigger)
                    {
                        push(MGamepadTriggerEvent{ts,i,false,rightTrigger,rightTrigger-gpst.rightTrigger}, nullptr);
                        gpst.rightTrigger = rightTrigger;
                    }

                    MStick left  {gamepadState.leftThumbstickX,  gamepadState.leftThumbstickY};
                    MStick right {gamepadState.rightThumbstickX, gamepadState.rightThumbstickY};

                    float len = sqrt(left.x*left.x + left.y*left.y);
                    if(LEFT_STICK_THRESHOLD_NEG < len && len < LEFT_STICK_THRESHOLD_POS)
                        left = {0,0};
                    
                    len = sqrt(right.x*right.x + right.y*right.y);
                    if(RIGHT_STICK_THRESHOLD_NEG < len && len < RIGHT_STICK_THRESHOLD_POS)
                        right = {0,0};
                    
                    if(left != gpst.left)
                    {
                        push(MGamepadStickEvent{ts,i,true,left,
                            left.x-gpst.left.x, left.y-gpst.left.y},nullptr);
                        gpst.left = left;
                    }
                    if(right != gpst.right)
                    {
                        push(MGamepadStickEvent{ts,i,false,right,
                            right.x-gpst.right.x, right.y-gpst.right.y},nullptr);
                        gpst.right = right;
                    }
                }
                reading->Release(); 
            }
        }
    }

    void MGlobal::checkConnectedGamepads() {
        for(size_t i{0};i < DEFAULT_GAMEPAD_COUNT;++i) {
            XINPUT_STATE currentState;
            ZeroMemory(&currentState, sizeof(XINPUT_STATE));

            DWORD result = XInputGetState(static_cast<DWORD>(i), &currentState);

            if (result == ERROR_SUCCESS) {
                push( MGamepadConnectedEvent{i}, nullptr);
                std::get<MW::MGamepadState>((*getBackStatePtr())[static_cast<size_t>(MW::MDeviceType::Gamepad) + i]).connected = true;
                gamepadsXI[i].connected = true;
                gamepadsXI[i].prevPacket = (unsigned long)currentState.dwPacketNumber;
            }
        }
        gamepadMightHaveConnected = false;
    }

    void MGlobal::getGamepadInputX() {
        for(size_t i{0};i < DEFAULT_GAMEPAD_COUNT;++i)
        {
            if(!gamepadsXI[i].connected && !gamepadMightHaveConnected) continue;

            XINPUT_STATE currentState;
            ZeroMemory(&currentState, sizeof(XINPUT_STATE));

            DWORD result = XInputGetState(static_cast<DWORD>(i), &currentState);

            if(result == ERROR_DEVICE_NOT_CONNECTED)
            {   
                if(gamepadsXI[i].connected) {
                    push(MGamepadDisconnectedEvent{i},nullptr);
                    gamepadsXI[i].connected = false;
                    std::get<MW::MGamepadState>((*getBackStatePtr())[static_cast<size_t>(MW::MDeviceType::Gamepad) + i]) = {};
                }
                continue;
            }
            if(!gamepadsXI[i].connected)
            {
                push(MGamepadConnectedEvent{i},nullptr);
                gamepadsXI[i].connected = true;
            }
            if((DWORD)gamepadsXI[i].prevPacket == currentState.dwPacketNumber)
                continue;

            MMicroSec ts = getTimeNow();

            static constexpr XInputButtonMap xMap[] = {
                { XINPUT_GAMEPAD_DPAD_UP,        MGamepadButton::DpadUp },
                { XINPUT_GAMEPAD_DPAD_DOWN,      MGamepadButton::DpadDown },
                { XINPUT_GAMEPAD_DPAD_LEFT,      MGamepadButton::DpadLeft },
                { XINPUT_GAMEPAD_DPAD_RIGHT,     MGamepadButton::DpadRight },
                { XINPUT_GAMEPAD_START,          MGamepadButton::Start },
                { XINPUT_GAMEPAD_BACK,           MGamepadButton::Select },
                { XINPUT_GAMEPAD_LEFT_THUMB,     MGamepadButton::ThumbLeft },
                { XINPUT_GAMEPAD_RIGHT_THUMB,    MGamepadButton::ThumbRight },
                { XINPUT_GAMEPAD_LEFT_SHOULDER,  MGamepadButton::BumperLeft },
                { XINPUT_GAMEPAD_RIGHT_SHOULDER, MGamepadButton::BumperRight },
                { XINPUT_GAMEPAD_A,              MGamepadButton::ActionBottom },
                { XINPUT_GAMEPAD_B,              MGamepadButton::ActionRight },
                { XINPUT_GAMEPAD_X,              MGamepadButton::ActionLeft },
                { XINPUT_GAMEPAD_Y,              MGamepadButton::ActionTop },
            };

            auto& gpst = std::get<MGamepadState>((*getBackStatePtr())[static_cast<size_t>(MDeviceType::Gamepad)+i]);
            auto& input = currentState.Gamepad;
            for(auto[flag, bt] : xMap) {
                bool b = (input.wButtons & flag);
                if(b != gpst.held[static_cast<size_t>(bt)]) {
                    if(b)
                    {
                        push(MGamepadButtonPressEvent{ts, i, bt},nullptr);
                        gpst.held.set(static_cast<size_t>(bt));
                    }
                    else {
                        push(MGamepadButtonReleaseEvent{ts, i, bt},nullptr);
                        gpst.held.reset(static_cast<size_t>(bt));
                    }
                }
            }
            auto normalizeTrigger = [](BYTE tr){
                if(tr < XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
                    tr = 0;
                return (float)tr / 255.f;
            };
            float leftTrigger = normalizeTrigger(input.bLeftTrigger);
            float rightTrigger = normalizeTrigger(input.bRightTrigger);

            if(leftTrigger != gpst.leftTrigger)
            {
                push(MGamepadTriggerEvent{ts,i,true,leftTrigger,leftTrigger-gpst.leftTrigger}, nullptr);
                gpst.leftTrigger = leftTrigger;
            }
            if(rightTrigger != gpst.rightTrigger)
            {
                push(MGamepadTriggerEvent{ts,i,false,rightTrigger,rightTrigger-gpst.rightTrigger}, nullptr);
                gpst.rightTrigger = rightTrigger;
            }

            auto normalizeStick = [](float x, float y) {
                if     (x > 0) x /= 32767.f;
                else if(x < 0) x /= 32768.f;
                if     (y > 0) y /= 32767.f;
                else if(y < 0) y /= 32768.f;
                return MStick{x, y};
            };
            MStick left  = normalizeStick((float)input.sThumbLX,(float)input.sThumbLY);
            MStick right = normalizeStick((float)input.sThumbRX,(float)input.sThumbRY);

            float len = sqrt(left.x*left.x + left.y*left.y);
            if(LEFT_STICK_THRESHOLD_NEG < len && len < LEFT_STICK_THRESHOLD_POS)
                left = {0,0};
            
            len = sqrt(right.x*right.x + right.y*right.y);
            if(RIGHT_STICK_THRESHOLD_NEG < len && len < RIGHT_STICK_THRESHOLD_POS)
                right = {0,0};
            
            if(left != gpst.left)
            {
                push(MGamepadStickEvent{ts,i,true,left,
                    left.x-gpst.left.x, left.y-gpst.left.y},nullptr);
                gpst.left = left;
            }
            if(right != gpst.right)
            {
                push(MGamepadStickEvent{ts,i,false,right,
                    right.x-gpst.right.x, right.y-gpst.right.y},nullptr);
                gpst.right = right;
            }
        }
        gamepadMightHaveConnected = false;
    }

    void MGlobal::enumerateMonitors(std::vector<MMonitor>& vec)
    {
        EnumDisplayMonitors(
            nullptr, nullptr,
            [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* vec = reinterpret_cast<std::vector<MMonitor>*>(lParam);
                auto* self = MGlobal::Get();

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

                // deduplicate on all four fields
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
                mon.name               = toNarrow(std::wstring(info.szDevice, info.szDevice + wcslen(info.szDevice)));
                mon.rect               = rect;
                mon.dpiScale           = scale;
                mon.isPrimary          = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                mon.currentModeIndex   = currentIndex;
                mon.availableModes     = std::move(availableModes);
                mon.hdr                = queryHDRInfo(hMon, info.szDevice);

                auto it = std::find_if(self->monitorEntrys.begin(),self->monitorEntrys.end(),
                    [name = mon.name](MMonitorEntry& e) {
                        return name == e.name;
                    }
                );
                if(it == self->monitorEntrys.end())
                {
                    mon.id = self->nextMonID++;
                    self->monitorEntrys.push_back({mon.id, reinterpret_cast<void*>(hMon), mon.name});
                }
                else
                    mon.id = (*it).id;
                
                vec->push_back(std::move(mon));

                return TRUE;
            },
            reinterpret_cast<LPARAM>(&vec)
        );
    }
}