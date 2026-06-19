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
        alive.store(true,std::memory_order_release);
        back_state.store(&state1,std::memory_order_release);
        front_state.store(&state2,std::memory_order_release);

        state2.desc = desc;
        float scale = 0;

        if(desc.mode == MWindowMode::Windowed)
        {
            state2.currentStyle |= WS_OVERLAPPED;
            if(state2.desc.resizable)
                state2.currentStyle |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
            if(state2.desc.decorated)
                state2.currentStyle |= (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
            if(state2.desc.visible)
                state2.currentStyle |= WS_VISIBLE;

            scale = global->monitorFromPoint(desc.rect.topLeft()).dpiScale;

            state2.currentRect = MRectToRECT(desc.rect, scale);
        }
        else
        {
            auto m = global->getMonitor(desc.monitor);
            assert(m && "Fatal: Invalid Monitor ID passed at creation!");
            MMonitor& mon { m.value() };
            state2.desc.rect.x = mon.rect.x;
            state2.desc.rect.y = mon.rect.y;
            state2.desc.rect.width = mon.rect.width;
            state2.desc.rect.height = mon.rect.height;

            scale = mon.dpiScale;

            state2.currentStyle |= WS_POPUP;

            state2.preFullscreenRect = MRectToRECT(state2.desc.rect, scale);
            state2.preFullscreenRect = MRectToRECT(mon.rect, scale);
            
            state2.preFullScreenStyle |= WS_OVERLAPPED;
            if(state2.desc.resizable)
                state2.preFullScreenStyle |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
            if(state2.desc.decorated)
                state2.preFullScreenStyle |= (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
            if(state2.desc.visible)
            {
                state2.preFullScreenStyle |= WS_VISIBLE;
                state2.currentStyle |= WS_VISIBLE;
            }
        }
        
        HWND hw = CreateWindowExW(
            (desc.mode != MWindowMode::Windowed) ? WS_EX_APPWINDOW : 0,
            L"MWindow",
            toWide(desc.title).data(),
            state2.currentStyle,
            static_cast<int>(state2.desc.rect.x*scale),
            static_cast<int>(state2.desc.rect.y*scale),
            static_cast<int>(state2.desc.rect.width*scale),
            static_cast<int>(state2.desc.rect.height*scale),
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        SetWindowLongPtr(hw, GWLP_USERDATA, (LONG_PTR)global);
        
        MMonitorID id = global->monIDFromHandle(MonitorFromWindow(hw, MONITOR_DEFAULTTONEAREST)).value_or(0);
        state2.desc.monitor = id;

        if(desc.mode == MWindowMode::Windowed)
        {
            RECT r{ MRectToRECT(state2.desc.rect,scale) };
            r.left = 0; r.top = 0;

            AdjustWindowRectExForDpi(
                &r,
                state2.currentStyle,
                FALSE,
                0,
                static_cast<UINT>(global->monitorFromID(id).value().dpiScale*96.0f)
            );
            SetWindowPos(hw,nullptr,
                0,0,
                r.right-r.left,r.bottom-r.top,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        state1 = state2;
        hwnd.store(hw,std::memory_order_release);
        id = global->registerWindow(reinterpret_cast<void*>(hw), this);
    }

    MWindowImpl::~MWindowImpl() {
        close();
    }

    MWindowID getId() const {
        return id;
    }

    MWindowImpl::MWindowState const* MWindowImpl::getFrontStatePtr() const {
        return front_state.load(std::memory_order_acquire);
    }

    MWindowImpl::MWindowState* MWindowImpl::getBackStatePtr() const {
        return back_state.load(std::memory_order_acquire);
    }
        
    void MWindowImpl::switchBuffers() {
        auto* b = back_state.load(std::memory_order_acquire);
        auto* f =  front_state.exchange(b,std::memory_order_acquire);
        back_state.store(f,std::memory_order_release);
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────────

    void MWindowImpl::show() {
        HWND h = hwnd.load(std::memory_order_acquire);
        {
            std::lock_guard<std::mutex> lock(back_state_lock);
            MWindowState& state = *back_state.load(std::memory_order_acquire);

            if(state.desc.visible == true) return;
            state.desc.visible = true;
        }
        ShowWindow(h, SW_SHOW);
    }

    void MWindowImpl::hide() {
        HWND h = hwnd.load(std::memory_order_acquire);
        {
            std::lock_guard<std::mutex> lock(back_state_lock);
            MWindowState& state = *back_state.load(std::memory_order_acquire);

            if(state.desc.visible == false) return;
            state.desc.visible = false;
        }
        ShowWindow(h, SW_HIDE);
    }

    void MWindowImpl::close() {
        if (!alive.exchange(false,std::memory_order_acq_rel)) return;

        global->unregisterWindow(id);

        auto h = hwnd.load(std::memory_order_acquire);
        if(h)
            DestroyWindow(h);

        hwnd.store(nullptr,std::memory_order_release);
    }

    bool MWindowImpl::isVisible() const {
        return getFrontStatePtr()->desc.visible;
    }

    // ── Properties ────────────────────────────────────────────────────────────────

    void MWindowImpl::setTitle(const std::string& title) {
        HWND h = hwnd.load(std::memory_order_acquire);
        {
            std::lock_guard<std::mutex> lock(back_state_lock);
            MWindowState& state = *back_state.load(std::memory_order_acquire);
            if(state.desc.title == title) return;
            state.desc.title = title;
        }
        SetWindowTextW(h, toWide(title).c_str());
    }

    const std::string& MWindowImpl::getTitle() const {
        return getFrontStatePtr()->desc.title;
    }

    void MWindowImpl::resize(MSize sz) {
        DWORD style {0};
        RECT r{0,0,0,0};
        UINT dpi;
        HWND h = hwnd.load(std::memory_order_acquire);
        {
            std::lock_guard<std::mutex> lock(back_state_lock);
            MWindowState& state = *back_state.load(std::memory_order_acquire);

            if(sz == state.desc.rect.size()) return;

            dpi = static_cast<UINT>(global->getMonitor(state.desc.monitor).value().dpiScale*96.0f);
            if(state.desc.mode != MWindowMode::Windowed)
            {
                style = state.preFullScreenStyle;
                r = state.preFullscreenRect;
            }
            else
            {
                style = state.currentStyle;
                r = MRectToRECT(state.desc.rect,(float)dpi/96.0f);
            }
            state.desc.rect.width = sz.width;
            state.desc.rect.width = sz.height;
        }
        AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);

        SetWindowPos(h, nullptr, 0, 0,
            r.right - r.left, r.bottom - r.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    MSize MWindowImpl::getSize() const {
        return getFrontStatePtr()->desc.rect.size();
    }

    void MWindowImpl::setTopLeftCorner(MPoint p) {
        HWND h = hwnd.load(std::memory_order_acquire);
        float scale = 0;
        {
            std::lock_guard<std::mutex> lock(back_state_lock);
            MWindowState& state = *back_state.load(std::memory_order_acquire);
            if(state.desc.rect.topLeft() == p) return;

            scale = global->getMonitor(state.desc.monitor).value().dpiScale;

            state.desc.rect.x = p.x;
            state.desc.rect.y = p.y;
        }

        SetWindowPos(h, nullptr,
            static_cast<int>(p.x*scale), static_cast<int>(p.y*scale),
            0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    MPoint MWindowImpl::getTopLeftCorner() const {
        return getFrontStatePtr()->desc.rect.topLeft();
    }

    MRect MWindowImpl::getRect() const {
        return getFrontStatePtr()->desc.rect;
    }

    void MWindowImpl::setWindowMode(MWindowMode mode) {
        RECT rect{0,0,0,0};
        DWORD style{0};
        HWND h = hwnd.load(std::memory_order_acquire);
        UINT dpi;
        {
            std::lock_guard<std::mutex> lock(back_state_lock);
            MWindowState& state = *back_state.load(std::memory_order_acquire);
            if(state.desc.mode == mode) return;
            if(state.desc.mode != MWindowMode::Windowed && mode != MWindowMode::Windowed) return;

            dpi = static_cast<UINT>(global->getMonitor(state.desc.monitor).value().dpiScale*96.0f);
            if(mode == MWindowMode::Windowed)
            {
                rect = state.preFullscreenRect;
                style = state.preFullScreenStyle;
            }
            else
            {
                state.preFullscreenRect = MRectToRECT(state.desc.rect,dpi/96.0f);
                state.preFullScreenStyle = state.currentStyle;
                rect = MRectToRECT(global->monitorFromID(state.desc.monitor).value().rect,dpi/96.0f);
            }
            state.desc.mode = mode;
        }

        switch (mode) {
            case MWindowMode::Windowed: {
                // Restore style and pre-fullscreen rect
                AdjustWindowRectExForDpi(&rect, style, FALSE, 0, dpi);
                SetWindowLongW(h, GWL_STYLE, style);
                SetWindowLongPtr(h, GWL_EXSTYLE, 0);
                SetWindowPos(h, nullptr,
                    rect.left,
                    rect.top,
                    rect.right - rect.left,
                    rect.bottom - rect.top,
                    SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
                break;
            }

            case MWindowMode::BorderlessFullscreen:
            case MWindowMode::Fullscreen: {

                SetWindowLongW(h, GWL_STYLE, WS_POPUP);
                SetWindowLongPtr(h, GWL_EXSTYLE, WS_EX_APPWINDOW);
                SetWindowPos(h, nullptr,
                    rect.left,
                    rect.top,
                    rect.right - rect.left,
                    rect.bottom - rect.top,
                    SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
                break;
            }
        }
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

    // ── Rendering ─────────────────────────────────────────────────────────────────

    MSize MWindowImpl::getPhysicalSize() const {
        MSize sz { getFrontStatePtr()->desc.rect.size() };
        float dpiscale { getDpiScale() };
        return {
            sz.width * dpiscale,
            sz.height * dpiscale
        };
    }

    MRenderSurface MWindowImpl::getRenderSurface() const { return {}; }
} // namespace MW
