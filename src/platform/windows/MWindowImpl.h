#ifndef M_WINDOWIMPL_H
#define M_WINDOWIMPL_H

#ifndef MPLATFORM_WINDOWS
    #error "src/platform/windows/MWindowImpl.h should not be included on the current platform!"
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <mutex>

#include "MWindow/MWindow.h"

namespace MW {
    class MGlobal;

    class MWindowImpl : public MWindow {
    public:
        struct MWindowState {
            bool focused = false;
            DWORD windowStyle = 0;      // saved before entering fullscreen
            RECT currentRect{};         // avoids comparing floats
            RECT preFullscreenRect{};   // not adjusted for physical DPI
                                        // exstyle is always WS_EX_APPWINDOW
            MWindowDesc desc{};

            uint64_t iconVersion = 0;
            uint64_t cursorVersion = 0;
        };

    private:
        // Backend and identity
        MGlobal* global = nullptr;
        HWND hwnd = nullptr;
        MWindowID id = 0;

        // State synchronization
        std::atomic<bool> alive;
        std::atomic<bool> state_change;
        mutable std::mutex back_state_lock;

        // Double-buffered window state
        MWindowState* back_state = nullptr;
        MWindowState* front_state = nullptr;
        MWindowState state1{};
        MWindowState state2{};

        HICON icon_handle = nullptr;     // owned; DestroyIcon'd on replace/destroy
        HCURSOR cursor_handle = nullptr; // owned; DestroyCursor'd on replace/destroy

        bool mouseCaptured = false;
        bool captureHidCursor = false;   // whether *this* capture session called ShowCursor(FALSE)

        void releaseMouseCaptureInternal();

    public:
        MWindowImpl(const MWindowDesc& desc);
        ~MWindowImpl() override;

        MWindowID getId() const override;

        inline MWindowState const* getFrontStatePtr() const;
        inline MWindowState* getBackStatePtr() const;
        void setStateChange();
        void handleStateRequests();
        void syncState();
        void onMonitorChange();

        // Lifecycle
        void show() override;
        void hide() override;
        void close() override;

        [[nodiscard]] bool isAlive() const override;
        [[nodiscard]] bool isVisible() const override;  // false when minimized

        // Properties (all logical)
        void setTitle(const std::string& title) override;
        const std::string& getTitle() const override;

        void setIcon(const MIconData& icon) override;
        void setCursor(const MCursorData& cursor) override;
        HCURSOR getCursorHandle() const { return cursor_handle; }

        bool startMouseCapture(bool hideCursor) override;
        void endMouseCapture() override;
        [[nodiscard]] bool isMouseCaptured() const override;
        void updateCaptureClip();   // recomputes/re-applies ClipCursor; called from MWindowWndProc too

        void resize(MSize sz) override;
        MSize getSize() const override; 

        void setTopLeftCorner(MPoint p) override;
        MPoint getTopLeftCorner() const override;

        MRect getRect() const override;

        void setWindowMode(MWindowMode mode) override;
        MWindowMode getWindowMode() const override;

        // Which monitor this window currently lives on (dominant monitor)
        MMonitorID getCurrentMonitorID() const override;

        // DPI scale of the current monitor
        float getDpiScale() const override;

        // Rendering
        MSize getPhysicalSize() const override;

        MNativeWindow getNativeWindow() const override;
    };
}

#endif