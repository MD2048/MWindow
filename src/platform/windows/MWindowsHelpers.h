#ifndef M_WINDOWSHELPERS_H
#define M_WINDOWSHELPERS_H

#include "MWindow/MDevices.h"
#include "MWindow/MMonitor.h"
#include "MWindow/MIcon.h"
#include "MWindow/MCursor.h"

#include <algorithm>
#include <limits>
#include <cmath>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellscalingapi.h>
#include <dxgi1_6.h>

using namespace MW;
    
inline bool operator==(const RECT& r1, const RECT& r2)
{
    return r1.left == r2.left && r1.top == r2.top && r1.right == r2.right && r1.bottom == r2.bottom;
}

inline bool operator!=(const RECT& r1, const RECT& r2)
{
    return !(r1 == r2);
}

inline RECT MRectToRECT(const MRect& mr, float scale)
{
    return RECT{
        static_cast<LONG>(mr.x*scale),
        static_cast<LONG>(mr.y*scale),
        static_cast<LONG>((mr.x + mr.width)*scale),
        static_cast<LONG>((mr.y + mr.height)*scale)
    };
}

inline MRect RECTToMRect(const RECT& r, float scale)
{
    return MRect{
        static_cast<float>(r.left/scale),
        static_cast<float>(r.top/scale),
        static_cast<float>((r.right - r.left)/scale),
        static_cast<float>((r.bottom - r.top)/scale)
    };
}

inline std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    return w;
}
inline std::string toNarrow(const std::wstring& wstr)
{
    if (wstr.empty())
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0,wstr.data(),(int)wstr.size(),nullptr,0,nullptr,nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), result.data(), size, nullptr, nullptr);
    return result;
}

inline HICON createHIconFromRGBA(const MIconData& icon)
{
    if (icon.width == 0 || icon.height == 0) return nullptr;
    if (icon.pixels.size() != static_cast<size_t>(icon.width) * icon.height * 4) return nullptr;

    BITMAPV5HEADER bi{};
    bi.bV5Size        = sizeof(bi);
    bi.bV5Width       = static_cast<LONG>(icon.width);
    bi.bV5Height      = -static_cast<LONG>(icon.height); // negative => top-down DIB
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    void* bits = nullptr;
    HDC screenDC = GetDC(nullptr);
    HBITMAP hbmColor = CreateDIBSection(screenDC, reinterpret_cast<BITMAPINFO*>(&bi),
                                         DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screenDC);
    if (!hbmColor) return nullptr;

    auto* dst = static_cast<uint8_t*>(bits);
    const uint8_t* src = icon.pixels.data();
    const size_t count = static_cast<size_t>(icon.width) * icon.height;
    for (size_t i = 0; i < count; ++i) {
        dst[i*4 + 0] = src[i*4 + 2]; // B
        dst[i*4 + 1] = src[i*4 + 1]; // G
        dst[i*4 + 2] = src[i*4 + 0]; // R
        dst[i*4 + 3] = src[i*4 + 3]; // A
    }

    // All-zero (opaque) 1bpp AND mask — standard technique for 32bpp icons
    // whose alpha channel already carries real transparency.
    const int strideBytes = ((icon.width + 15) / 16) * 2; // word-aligned 1bpp rows
    std::vector<uint8_t> maskBits(static_cast<size_t>(strideBytes) * icon.height, 0);
    HBITMAP hbmMask = CreateBitmap(static_cast<int>(icon.width), static_cast<int>(icon.height),
                                    1, 1, maskBits.data());
    if (!hbmMask) { DeleteObject(hbmColor); return nullptr; }

    ICONINFO ii{};
    ii.fIcon    = TRUE;
    ii.hbmMask  = hbmMask;
    ii.hbmColor = hbmColor;

    HICON hIcon = CreateIconIndirect(&ii);

    // CreateIconIndirect copies both bitmaps internally — we still own these handles.
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hIcon; // may be nullptr on failure — caller must check
}

inline HCURSOR createHCursorFromRGBA(const MCursorData& cursor)
{
    if (cursor.width == 0 || cursor.height == 0) return nullptr;
    if (cursor.pixels.size() != static_cast<size_t>(cursor.width) * cursor.height * 4) return nullptr;

    BITMAPV5HEADER bi{};
    bi.bV5Size        = sizeof(bi);
    bi.bV5Width       = static_cast<LONG>(cursor.width);
    bi.bV5Height      = -static_cast<LONG>(cursor.height); // negative => top-down DIB
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    void* bits = nullptr;
    HDC screenDC = GetDC(nullptr);
    HBITMAP hbmColor = CreateDIBSection(screenDC, reinterpret_cast<BITMAPINFO*>(&bi),
                                         DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screenDC);
    if (!hbmColor) return nullptr;

    auto* dst = static_cast<uint8_t*>(bits);
    const uint8_t* src = cursor.pixels.data();
    const size_t count = static_cast<size_t>(cursor.width) * cursor.height;
    for (size_t i = 0; i < count; ++i) {
        dst[i*4 + 0] = src[i*4 + 2]; // B
        dst[i*4 + 1] = src[i*4 + 1]; // G
        dst[i*4 + 2] = src[i*4 + 0]; // R
        dst[i*4 + 3] = src[i*4 + 3]; // A
    }

    const int strideBytes = ((cursor.width + 15) / 16) * 2; // word-aligned 1bpp rows
    std::vector<uint8_t> maskBits(static_cast<size_t>(strideBytes) * cursor.height, 0);
    HBITMAP hbmMask = CreateBitmap(static_cast<int>(cursor.width), static_cast<int>(cursor.height),
                                    1, 1, maskBits.data());
    if (!hbmMask) { DeleteObject(hbmColor); return nullptr; }

    ICONINFO ii{};
    ii.fIcon    = FALSE;
    ii.xHotspot = cursor.hotspotX;
    ii.yHotspot = cursor.hotspotY;
    ii.hbmMask  = hbmMask;
    ii.hbmColor = hbmColor;

    HCURSOR hCursor = static_cast<HCURSOR>(CreateIconIndirect(&ii));

    // CreateIconIndirect copies both bitmaps internally — we still own these handles.
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);

    return hCursor; // may be nullptr on failure — caller must check
}

template<class... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};

template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

inline void initializeDeviceState(MDeviceState& st)
{
    std::visit(
        Overloaded{
            [](MKeyboardState& state) {},
            [](MMouseState& state)
            {
                POINT p{};
                if (GetCursorPos(&p)) {
                    HMONITOR hMon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
                    UINT dpiX, dpiY;
                    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
                    float scale = dpiX / 96.f;
                    state.p.x = p.x / scale;
                    state.p.y = p.y / scale;
                }
            },
            [](MTouchState& state) {},
            [](MGamepadState& state) {},
            [](MStylusState& state) {}
        },
        st
    );
}

inline uint32_t dmBitsPerChannel(DWORD dmBitsPerPel) {
    switch (dmBitsPerPel) {
        case 30: return 10;  // HDR10
        case 48: return 16;  // scRGB / deep color
        default: return 8;   // standard 32bpp → 8 bpc
    }
}

inline bool isHDRActive(const wchar_t* gdiDeviceName) {
    UINT32 pathCount = 0, modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,
                                    &pathCount, &modeCount) != ERROR_SUCCESS)
        return false;

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
                           &pathCount, paths.data(),
                           &modeCount, modes.data(),
                           nullptr) != ERROR_SUCCESS)
        return false;

    for (auto& path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME srcName{};
        srcName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        srcName.header.size      = sizeof(srcName);
        srcName.header.adapterId = path.sourceInfo.adapterId;
        srcName.header.id        = path.sourceInfo.id;

        if (DisplayConfigGetDeviceInfo(&srcName.header) != ERROR_SUCCESS)
            continue;
        if (wcscmp(srcName.viewGdiDeviceName, gdiDeviceName) != 0)
            continue;

        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO colorInfo{};
        colorInfo.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        colorInfo.header.size      = sizeof(colorInfo);
        colorInfo.header.adapterId = path.targetInfo.adapterId;
        colorInfo.header.id        = path.targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&colorInfo.header) == ERROR_SUCCESS)
            return colorInfo.advancedColorEnabled;
    }
    return false;
}

inline MHDRInfo queryHDRInfo(HMONITOR hMon, const wchar_t* gdiDeviceName) {
    MHDRInfo result{ };
        result.supported    = false;
        result.active       = false;
        result.maxLuminance = 0.f;
        result.minLuminance = 0.f;
        result.colorGamut   = MColorGamut::Unknown;

    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                   reinterpret_cast<void**>(&factory))))
        return result;

    IDXGIAdapter1* adapter = nullptr;
    for (UINT ai = 0;
         factory->EnumAdapters1(ai, &adapter) != DXGI_ERROR_NOT_FOUND; ++ai) {

        IDXGIOutput* output = nullptr;
        for (UINT oi = 0;
             adapter->EnumOutputs(oi, &output) != DXGI_ERROR_NOT_FOUND; ++oi) {

            DXGI_OUTPUT_DESC desc{};
            output->GetDesc(&desc);

            if (desc.Monitor == hMon) {
                IDXGIOutput6* out6 = nullptr;
                if (SUCCEEDED(output->QueryInterface(
                        __uuidof(IDXGIOutput6),
                        reinterpret_cast<void**>(&out6)))) {

                    DXGI_OUTPUT_DESC1 d{};
                    out6->GetDesc1(&d);

                    result.supported    = (d.MaxLuminance > 0.f);
                    result.maxLuminance = d.MaxLuminance;
                    result.minLuminance = d.MinLuminance;

                    // DCI-P3 has no dedicated DXGI color space constant
                    // detect it by inspecting the red primary x coordinate:
                    //   sRGB  red ≈ 0.640,  P3 red ≈ 0.680
                    switch (d.ColorSpace) {
                        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
                            result.colorGamut = MColorGamut::SRGB;    break;
                        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
                        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
                            result.colorGamut = MColorGamut::Rec2020;  break;
                        default:
                            if (d.RedPrimary[0] > 0.670f)
                                result.colorGamut = MColorGamut::DCI_P3;
                            break;
                    }

                    result.active = isHDRActive(gdiDeviceName);

                    out6->Release();
                }
                output->Release();
                adapter->Release();
                factory->Release();
                return result;
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return result;
}

#endif