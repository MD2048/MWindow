#ifndef M_WINDOWSHELPERS_H
#define M_WINDOWSHELPERS_H

#include "MWindow/MDevices.h"
#include "MWindow/MMonitor.h"

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

inline MKey translateVK(USHORT vk)
{
    
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