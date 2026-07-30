#include "windows/MWindowImpl.h"
#include "windows/MGlobalImpl.h"
#include "windows/MWindowsHelpers.h"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>

#include <cassert>

namespace MW
{
    MWindowImpl::MWindowImpl(const MWindowDesc& desc)
    : global{MGlobal::Get()}
    , state1{}
    , state2{}
    {
        state_change.store(false,std::memory_order_release);
        alive.store(true,std::memory_order_release);
        front_state = &state1;
        back_state = &state2;

        state2.desc = desc;
        state2.focused = desc.visible;;
        float scale = 0;

        state2.windowStyle |= WS_OVERLAPPED;
        if(state2.desc.resizable)
            state2.windowStyle |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        if(state2.desc.decorated)
            state2.windowStyle |= (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

        DWORD curStyle = 0;

        if(desc.mode == MWindowMode::Windowed)
        {
            curStyle = state2.windowStyle;
            scale = global->monitorFromPoint(desc.rect.topLeft()).dpiScale;

            state2.currentRect = MRectToRECT(desc.rect, scale);
        }
        else
        {
            auto m = global->getMonitor(desc.monitor);
            assert(m && "Fatal: Invalid Monitor ID passed at creation!");
            MMonitor& mon { m.value() };
            scale = mon.dpiScale;
            state2.desc.rect = mon.rect;
            state2.currentRect = MRectToRECT(mon.rect,scale);

            scale = mon.dpiScale;

            curStyle = WS_POPUP;

            state2.preFullscreenRect = MRectToRECT(desc.rect, scale);
        }

        if(desc.visible)
            curStyle |= WS_VISIBLE;

        HWND hw = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"MWindow",
            toWide(desc.title).c_str(),
            curStyle,
            static_cast<int>(state2.desc.rect.x*scale),
            static_cast<int>(state2.desc.rect.y*scale),
            static_cast<int>(state2.desc.rect.width*scale),
            static_cast<int>(state2.desc.rect.height*scale),
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        id = global->registerWindow(reinterpret_cast<void*>(hw), this);

        SetWindowLongPtr(hw, GWLP_USERDATA, (LONG_PTR)global);
        
        MMonitor mon = global->monitorFromHandle(MonitorFromWindow(hw, MONITOR_DEFAULTTONEAREST)).value();
        state2.desc.monitor = mon.id;

        if(state2.desc.centered)
        {   
            RECT mon_rc = MRectToRECT(mon.rect,mon.dpiScale);
            RECT wa{ MRectToRECT(desc.rect,mon.dpiScale) };

            int x = wa.left + ((mon_rc.right - mon_rc.left) - (wa.right - wa.left)) / 2;
            int y = wa.top  + ((mon_rc.bottom - mon_rc.top) - (wa.bottom - wa.top)) / 2;
            wa.left += x; wa.right += x; wa.top += y; wa.bottom += y;
            
            if(desc.mode == MWindowMode::Windowed)
            {
                SetWindowPos(hw,nullptr,
                    wa.left, wa.top,
                    0,0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                state2.desc.rect = RECTToMRect(wa, scale);
                state2.currentRect = wa;
            }
            else {
                state2.preFullscreenRect = wa;
            }
            
        }

        if(desc.mode == MWindowMode::Windowed)
        {
            RECT r{ MRectToRECT(state2.desc.rect,scale) };
            r.right -= r.left;
            r.bottom -= r.top;
            r.left = 0; r.top = 0;

            AdjustWindowRectExForDpi(
                &r,
                state2.windowStyle,
                FALSE,
                0,
                static_cast<UINT>(global->getMonitor(id).value().dpiScale*96.0f)
            );
            SetWindowPos(hw,nullptr,
                0,0,
                r.right-r.left,r.bottom-r.top,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if(state2.desc.icon.has_value())
        {
            icon_handle = createHIconFromRGBA(*state2.desc.icon);
            if(icon_handle)
            {
                SendMessageW(hw, WM_SETICON, ICON_BIG,   (LPARAM)icon_handle);
                SendMessageW(hw, WM_SETICON, ICON_SMALL, (LPARAM)icon_handle);
            }
        }
        if(state2.desc.cursor.has_value())
        {
            // No message to send — the OS pulls the cursor via WM_SETCURSOR on every
            // mouse move over the client area, so just having cursor_handle ready is enough.
            cursor_handle = createHCursorFromRGBA(*state2.desc.cursor);
        }

        state1 = state2;
        hwnd = hw;
    }

    MWindowImpl::~MWindowImpl() {
        if(mouseCaptured) releaseMouseCaptureInternal();
        alive.store(false,std::memory_order_release);
        if(hwnd)
        {
            global->unregisterWindow(id);
            DestroyWindow(hwnd);
        }
        hwnd = nullptr;
        if(icon_handle)
        {
            DestroyIcon(icon_handle);
            icon_handle = nullptr;
        }
        if(cursor_handle)
        {
            DestroyCursor(cursor_handle);
            cursor_handle = nullptr;
        }
    }

    MWindowID MWindowImpl::getId() const {
        return id;
    }

    MWindowImpl::MWindowState const* MWindowImpl::getFrontStatePtr() const {
        return front_state;
    }

    MWindowImpl::MWindowState* MWindowImpl::getBackStatePtr() const {
        return back_state;
    }

    void MWindowImpl::setStateChange() { state_change.store(true,std::memory_order_release); }
        
    void MWindowImpl::handleStateRequests() {
        if(!state_change.load(std::memory_order_acquire)) return;
        state_change.store(false, std::memory_order_release);

        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& back = *getBackStatePtr();
        MWindowState& front = *front_state;      // Only time this is allowed

        auto recalculateMonitor = 
        [](HANDLE& current,const RECT& rc)
        {
            HANDLE hMon = MonitorFromRect(&rc,MONITOR_DEFAULTTONEAREST);
            if(hMon == current)
                return false;
            else {
                current = hMon;
                return true;
            }
        };

        if(!alive.load(std::memory_order_acquire))
        {
            if(mouseCaptured) releaseMouseCaptureInternal();
            if(hwnd)
            {
                global->unregisterWindow(id);
                DestroyWindow(hwnd);
            }
            hwnd = nullptr;
            return;
        }
        if(back.desc.visible != front.desc.visible)
        {
            if(back.desc.visible) 
            {
                ShowWindow(hwnd, SW_SHOW);
                front.desc.visible = true;
            }
            else
            {
                ShowWindow(hwnd, SW_HIDE);
                front.desc.visible = false;
            }
        }
        if(back.desc.title != front.desc.title)
        {
            SetWindowTextW(hwnd, toWide(back.desc.title).c_str());
            front.desc.title = back.desc.title;
        }
        if(back.iconVersion != front.iconVersion)
        {
            HICON newIcon = back.desc.icon.has_value() ? createHIconFromRGBA(*back.desc.icon) : nullptr;
            if(newIcon)
            {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)newIcon);
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)newIcon);

                if(icon_handle) DestroyIcon(icon_handle);
                icon_handle = newIcon;
            }
            front.desc.icon   = back.desc.icon;
            front.iconVersion = back.iconVersion;
        }
        if(back.cursorVersion != front.cursorVersion)
        {
            HCURSOR newCursor = back.desc.cursor.has_value() ? createHCursorFromRGBA(*back.desc.cursor) : nullptr;
            if(newCursor)
            {
                if(cursor_handle) DestroyCursor(cursor_handle);
                cursor_handle = newCursor;

                POINT p{};
                if(GetCursorPos(&p) && WindowFromPoint(p) == hwnd)
                    SetCursor(cursor_handle); // re-apply immediately if already hovered
            }
            front.desc.cursor   = back.desc.cursor;
            front.cursorVersion = back.cursorVersion;
        }
        if(back.desc.rect.size() != front.desc.rect.size())
        {   
            front.desc.rect = back.desc.rect;
            RECT r{0,0,0,0};
            UINT dpi = static_cast<UINT>((float)global->getMonitor(back.desc.monitor).value().dpiScale*96.0f);
            if(back.desc.mode == MWindowMode::Windowed)
            {
                r = back.currentRect;

                AdjustWindowRectExForDpi(&r, back.windowStyle, FALSE, 0, dpi);

                HANDLE h = global->handleFromID(back.desc.monitor).value();
                if(recalculateMonitor(h,r))
                {
                    MMonitor new_mon = global->monitorFromHandle(h).value();
                    dpi = static_cast<UINT>((float)new_mon.dpiScale*96.0f);
                    r = MRectToRECT(back.desc.rect,dpi/96.0f);
                    front.currentRect = r;
                    back.currentRect = r;
                    AdjustWindowRectExForDpi(&r, back.windowStyle, FALSE, 0, dpi);

                    global->push(MMonitorChangeEvent{back.desc.monitor,new_mon.id},hwnd);
                    back.desc.monitor = new_mon.id;
                    front.desc.monitor = back.desc.monitor;
                }

                SetWindowPos(hwnd, nullptr, 0, 0,
                    r.right - r.left, r.bottom - r.top,
                    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        if(back.preFullscreenRect != front.preFullscreenRect)
        {
            front.preFullscreenRect = back.preFullscreenRect;
        }
        if(back.desc.rect.topLeft() != front.desc.rect.topLeft())
        {
            RECT r{0,0,0,0};
            UINT dpi = static_cast<UINT>((float)global->getMonitor(back.desc.monitor).value().dpiScale*96.0f);
            if(back.desc.mode == MWindowMode::Windowed)
            {
                front.desc.rect = back.desc.rect;
                r = back.currentRect;

                HANDLE h = global->handleFromID(back.desc.monitor).value();
                if(recalculateMonitor(h,r))
                {
                    MMonitor new_mon = global->monitorFromHandle(h).value();
                    dpi = static_cast<UINT>((float)new_mon.dpiScale*96.0f);
                    r = MRectToRECT(back.desc.rect,dpi/96.0f);
                    front.currentRect = r;
                    back.currentRect = r;
                    AdjustWindowRectExForDpi(&r, back.windowStyle, FALSE, 0, dpi);
                    SetWindowPos(hwnd, nullptr,
                        0,0,
                        r.right-r.left, r.bottom-r.left,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);


                    global->push(MMonitorChangeEvent{back.desc.monitor,new_mon.id}, hwnd);
                    back.desc.monitor = new_mon.id;
                    front.desc.monitor = back.desc.monitor;
                }
            
                SetWindowPos(hwnd, nullptr,
                    static_cast<int>(back.currentRect.left), static_cast<int>(back.currentRect.top),
                    0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        if(back.desc.mode != front.desc.mode)
        {
            front.desc.mode = back.desc.mode;
            UINT dpi = static_cast<UINT>(global->getMonitor(back.desc.monitor).value().dpiScale*96.0f);
            DWORD style = back.desc.visible ? WS_VISIBLE : 0;
            if(back.desc.mode == MWindowMode::Windowed)
            {
                RECT& r = back.preFullscreenRect;
                RECT rectSz{0,0,r.right-r.left,r.bottom-r.top};
                style |= back.windowStyle;
                AdjustWindowRectExForDpi(&rectSz, style, FALSE, 0, dpi);
                SetWindowLongW(hwnd, GWL_STYLE, style);
                SetWindowPos(hwnd, nullptr,
                    r.left,
                    r.top,
                    rectSz.right - rectSz.left,
                    rectSz.bottom - rectSz.top,
                    SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
                back.currentRect = back.preFullscreenRect;
                front.currentRect = back.preFullscreenRect;
                front.desc.rect = RECTToMRect(back.preFullscreenRect,dpi/96.0f);
                back.desc.rect = front.desc.rect;
            }
            else
            {
                style |= WS_POPUP;
                back.preFullscreenRect = back.currentRect;
                front.preFullscreenRect = back.preFullscreenRect;
                front.desc.rect = global->getMonitor(back.desc.monitor).value().rect;
                back.desc.rect = front.desc.rect;
                RECT rect = MRectToRECT(back.desc.rect,dpi/96.0f);
                back.currentRect = rect;
                front.currentRect = rect;
                SetWindowLongW(hwnd, GWL_STYLE, style);
                SetWindowPos(hwnd, nullptr,
                    rect.left,
                    rect.top,
                    rect.right - rect.left,
                    rect.bottom - rect.top,
                    SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        if(mouseCaptured) updateCaptureClip();
    }

    void MWindowImpl::syncState() {
        if(!state_change.load(std::memory_order_acquire)) return;
        state_change.store(false, std::memory_order_release);

        std::lock_guard<std::mutex> lock(back_state_lock);

        auto* temp = back_state;
        back_state = front_state;
        front_state = temp;

        *back_state = *front_state;
    }

    void MWindowImpl::onMonitorChange() {
        auto* state = getBackStatePtr();
        MMonitor new_mon = global->monitorFromHandle(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST)).value();

        if(new_mon.id != state->desc.monitor)
        {
            global->push(MMonitorChangeEvent{state->desc.monitor, new_mon.id}, hwnd);
            state->desc.monitor = new_mon.id;
        }
        if(state->desc.mode == MWindowMode::Windowed)
        {
            RECT r{MRectToRECT(state->desc.rect,new_mon.dpiScale)};
            if(r != state->currentRect) {
                state->currentRect = r;
                AdjustWindowRectExForDpi(&r, state->windowStyle,FALSE, 0,static_cast<UINT>(new_mon.dpiScale*96.f));
                SetWindowPos(hwnd, nullptr,0,0,
                        r.right-r.left, r.bottom-r. top,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        else {
            if(new_mon.rect != state->desc.rect)
            {
                if(new_mon.rect.size() != state->desc.rect.size())
                {
                    state->desc.rect.width = new_mon.rect.width;
                    state->desc.rect.height = new_mon.rect.height;
                    global->push(MResizeEvent{state->desc.rect.size()}, hwnd);
                    state->currentRect = MRectToRECT(state->desc.rect,new_mon.dpiScale);
                    RECT& r = state->currentRect;
                    SetWindowPos(hwnd, nullptr, 0,0,
                        r.right - r.left,
                        r.bottom - r.top,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
                    );
                }
                if(new_mon.rect.topLeft() != state->desc.rect.topLeft())
                {
                    state->desc.rect.x = new_mon.rect.x;
                    state->desc.rect.y = new_mon.rect.y;
                    global->push(MMoveEvent{state->desc.rect.topLeft()}, hwnd);
                    state->currentRect = MRectToRECT(state->desc.rect,new_mon.dpiScale);
                    RECT& r = state->currentRect;
                    SetWindowPos(hwnd, nullptr,r.left,r.top,0,0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                    );
                }
            }
        }
    }

    // Lifecycle

    void MWindowImpl::show() {
        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();

        if(state.desc.visible == true) return;
        state.desc.visible = true;
        state_change.store(true,std::memory_order_release);
    }

    void MWindowImpl::hide() {
        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();

        if(state.desc.visible == false) return;
        state.desc.visible = false;
        state_change.store(true,std::memory_order_release);
    }

    void MWindowImpl::close() {
        alive.store(false,std::memory_order_release);
    }

    bool MWindowImpl::isAlive() const {
        return alive.load(std::memory_order_acquire);
    }

    bool MWindowImpl::isVisible() const {
        return getFrontStatePtr()->desc.visible;
    }

    // Properties

    void MWindowImpl::setTitle(const std::string& title) {
        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();

        if(state.desc.title == title) return;
        state.desc.title = title;
        state_change.store(true,std::memory_order_release);
    }

    const std::string& MWindowImpl::getTitle() const {
        return getFrontStatePtr()->desc.title;
    }

    void MWindowImpl::setIcon(const MIconData& icon) {
        assert(icon.pixels.size() == static_cast<size_t>(icon.width) * icon.height * 4
            && "MWindowImpl::setIcon(): pixel buffer size must equal width * height * 4 (RGBA8)!");

        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();

        state.desc.icon = icon;   // copy — caller retains ownership of their own buffer
        state.iconVersion++;      // always bump; setIcon() is an explicit, deliberate call
        state_change.store(true,std::memory_order_release);
    }

    void MWindowImpl::setCursor(const MCursorData& cursor) {
        assert(cursor.pixels.size() == static_cast<size_t>(cursor.width) * cursor.height * 4
            && "MWindowImpl::setCursor(): pixel buffer size must equal width * height * 4 (RGBA8)!");

        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();

        state.desc.cursor = cursor; // copy — caller retains ownership of their own buffer
        state.cursorVersion++;      // always bump; setCursor() is an explicit, deliberate call
        state_change.store(true,std::memory_order_release);
    }

    void MWindowImpl::resize(MSize sz) {
        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();
        float scale = global->getMonitor(state.desc.monitor).value().dpiScale;

        if(state.desc.mode == MWindowMode::Windowed)
        {
            if(sz == state.desc.rect.size()) return;

            state.desc.rect.width = sz.width;
            state.desc.rect.height = sz.height;

            state.currentRect = MRectToRECT(state.desc.rect,scale);
        }
        else
        {
            state.preFullscreenRect = {state.preFullscreenRect.left,state.preFullscreenRect.top,
                state.preFullscreenRect.left + static_cast<LONG>(sz.width*scale),
                state.preFullscreenRect.top + static_cast<LONG>(sz.height*scale)};
        }
        
        state_change.store(true,std::memory_order_release);
    }

    MSize MWindowImpl::getSize() const {
        return getFrontStatePtr()->desc.rect.size();
    }

    void MWindowImpl::setTopLeftCorner(MPoint p) {
        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();
        float scale = global->getMonitor(state.desc.monitor).value().dpiScale;

        if(state.desc.mode == MWindowMode::Windowed)
        {
            if(state.desc.rect.topLeft() == p) return;

            state.desc.rect.x = p.x;
            state.desc.rect.y = p.y;

            state.currentRect = MRectToRECT(state.desc.rect,scale);
        }
        else
        {
            state.preFullscreenRect = {static_cast<LONG>(p.x*scale),static_cast<LONG>(p.y*scale),
                state.preFullscreenRect.right + static_cast<LONG>(p.x*scale),
                state.preFullscreenRect.bottom + static_cast<LONG>(p.y*scale)};
        }
        
        state_change.store(true,std::memory_order_release);
    }

    MPoint MWindowImpl::getTopLeftCorner() const {
        return getFrontStatePtr()->desc.rect.topLeft();
    }

    MRect MWindowImpl::getRect() const {
        return getFrontStatePtr()->desc.rect;
    }

    void MWindowImpl::setWindowMode(MWindowMode mode) {
        std::lock_guard<std::mutex> lock(back_state_lock);

        MWindowState& state = *getBackStatePtr();

        if(state.desc.mode == mode) return;
        if(state.desc.mode != MWindowMode::Windowed && mode != MWindowMode::Windowed) return;
        
        state.desc.mode = mode;
        state_change.store(true,std::memory_order_release);
    }
    MWindowMode MWindowImpl::getWindowMode() const {
        return getFrontStatePtr()->desc.mode;
    }

    MMonitorID MWindowImpl::getCurrentMonitorID() const {
        return getFrontStatePtr()->desc.monitor;
    }

    float MWindowImpl::getDpiScale() const {
        return global->getMonitor(getFrontStatePtr()->desc.monitor).value().dpiScale;
    }

    // Rendering

    MSize MWindowImpl::getPhysicalSize() const {
        auto& rc = getFrontStatePtr()->currentRect;
        return {static_cast<float>(rc.right-rc.left), static_cast<float>(rc.bottom-rc.top)};
    }

    MNativeWindow MWindowImpl::getNativeWindow() const {
        return MWindowsNativeWindow{reinterpret_cast<MOpaqueHandle>(hwnd),
            reinterpret_cast<MOpaqueHandle>(GetModuleHandle(nullptr))};
    }

    // Mouse capture — runs synchronously on the calling thread (main thread only),
    // unlike every other state-mutating method above which queues via back_state/state_change.

    bool MWindowImpl::startMouseCapture(bool hideCursor) {
        if(mouseCaptured) return true;                            // idempotent
        if(global->capturingWindowId.has_value()) return false;    // mutual exclusion: fail, no steal
        if(!getFrontStatePtr()->focused) return false;

        POINT p{};
        if(!GetCursorPos(&p)) return false;

        RECT client{};
        GetClientRect(hwnd, &client);
        POINT local = p;
        ScreenToClient(hwnd, &local);
        if(!PtInRect(&client, local)) return false;                // cursor-inside-client-area check

        POINT tl{client.left, client.top}, br{client.right, client.bottom};
        ClientToScreen(hwnd, &tl);
        ClientToScreen(hwnd, &br);
        RECT clip{tl.x, tl.y, br.x, br.y};
        if(!ClipCursor(&clip)) return false;

        captureHidCursor = hideCursor;
        if(hideCursor) ShowCursor(FALSE);

        mouseCaptured = true;
        global->capturingWindowId = id;
        return true;
    }

    void MWindowImpl::endMouseCapture() {
        if(mouseCaptured) releaseMouseCaptureInternal();
    }

    bool MWindowImpl::isMouseCaptured() const {
        return mouseCaptured;
    }

    void MWindowImpl::releaseMouseCaptureInternal() {
        ClipCursor(nullptr);
        if(captureHidCursor)
        {
            ShowCursor(TRUE);
            captureHidCursor = false;
        }
        mouseCaptured = false;
        if(global->capturingWindowId == id)
            global->capturingWindowId.reset();
    }

    void MWindowImpl::updateCaptureClip() {
        if(!mouseCaptured) return;

        RECT client{};
        GetClientRect(hwnd, &client);
        POINT tl{client.left, client.top}, br{client.right, client.bottom};
        ClientToScreen(hwnd, &tl);
        ClientToScreen(hwnd, &br);
        RECT clip{tl.x, tl.y, br.x, br.y};
        ClipCursor(&clip);
    }

} // namespace MW
