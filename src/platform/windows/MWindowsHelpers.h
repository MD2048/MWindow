#ifndef M_WINDOWSHELPERS_H
#define M_WINDOWSHELPERS_H

#include "MWindow/MDevices.h"
#include "MWindow/MMonitor.h"

#include <windows.h>
#include <dxgi1_6.h>

using namespace MW;

inline MDeviceType toMDeviceType(const RAWINPUTDEVICELIST& entry) {
    switch (entry.dwType) {
        case RIM_TYPEKEYBOARD: return MDeviceType::Keyboard;
        case RIM_TYPEMOUSE:    return MDeviceType::Mouse;
        case RIM_TYPEHID: {
            RID_DEVICE_INFO info{};
            info.cbSize = sizeof(info);
            UINT size = sizeof(info);
            GetRawInputDeviceInfoW(entry.hDevice, RIDI_DEVICEINFO, &info, &size);

            switch (info.hid.usUsagePage) {
                case 0x01: // Generic Desktop
                    switch (info.hid.usUsage) {
                        case 0x04: return MDeviceType::Gamepad;    // Joystick
                        case 0x05: return MDeviceType::Gamepad;    // Gamepad
                        case 0x08: return MDeviceType::Mouse;      // Multi-axis (trackball etc)
                        default:   return MDeviceType::Unknown;
                    }
                case 0x0D: // Digitizer
                    switch (info.hid.usUsage) {
                        case 0x01: return MDeviceType::Stylus;      // Digitizer
                        case 0x02: return MDeviceType::Stylus;      // Pen
                        case 0x04: return MDeviceType::Touchscreen; // Touchscreen
                        case 0x05: return MDeviceType::Touchscreen; // Touch pad
                        default:   return MDeviceType::Unknown;
                    }
                default: return MDeviceType::Unknown;
            }
        }
        default: return MDeviceType::Unknown;
    }
}

inline std::string GetDeviceName(HANDLE device)
{
    UINT size = 0;

    GetRawInputDeviceInfoA(
        device,
        RIDI_DEVICENAME,
        nullptr,
        &size);

    if (size == 0)
        return {};

    std::string name(size, '\0');

    if (GetRawInputDeviceInfoA(
            device,
            RIDI_DEVICENAME,
            name.data(),
            &size) == static_cast<UINT>(-1))
    {
        return {};
    }

    // remove trailing null
    if (!name.empty() && name.back() == '\0')
        name.pop_back();

    return name;
}

inline uint32_t dmBitsPerChannel(DWORD dmBitsPerPel) {
    switch (dmBitsPerPel) {
        case 30: return 10;  // HDR10
        case 48: return 16;  // scRGB / deep color
        default: return 8;   // standard 32bpp → 8 bpc
    }
}

// Uses DisplayConfig to check whether HDR is currently enabled in OS settings.
// Matched via GDI device name (e.g. L"\\.\DISPLAY1") rather than HMONITOR
// to avoid a second HMONITOR → path lookup.
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
        // Match path to our monitor via GDI source name
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

// Uses IDXGIOutput6 to query HDR capabilities.
// Falls back to a zeroed MHDRInfo if DXGI or the interface is unavailable.
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

                    // Color gamut from DXGI color space.
                    // DCI-P3 has no dedicated DXGI color space constant —
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