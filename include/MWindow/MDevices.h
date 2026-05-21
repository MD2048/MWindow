#ifndef M_DEVICES_H
#define M_DEVICES_H

#include <bitset>
#include <unordered_map>
#include <variant>
#include <string>

#include "MWindow/MDef.h"

namespace MW
{
    using MDeviceID = uint64_t;

    enum class MDeviceType   { Unknown, Keyboard, Mouse, Touchscreen, Gamepad, Stylus };
    // Gamepad = Controller

    struct MMods {
        bool shift : 1;
        bool ctrl  : 1;
        bool alt   : 1;
        bool super : 1;   // Win / Cmd
        bool caps  : 1;   // CapsLock state, not the key itself
        bool num   : 1;   // NumLock state

        bool none() const { return !shift && !ctrl && !alt && !super; }
    };

    struct MKeyboardState {
        std::bitset<256> held;   // true while key is physically down
        MMods            mods;   // current modifier + lock state
    };

    struct MMouseState {
        float x = 0, y = 0;          // last known logical position
        std::bitset<8> buttons;       // indexed by MMouseButton
    };

    struct MTouchState {
        std::unordered_map<uint32_t, MPoint> activePoints; // touch id → point
    };

    // Reserved — no fields yet, just a placeholder so the variant compiles
    struct MGamepadState {};
    struct MStylusState  {};

    using MDeviceState = std::variant<
        MKeyboardState,
        MMouseState,
        MTouchState,
        MGamepadState,
        MStylusState
    >;

    inline MDeviceState zeroInit(MDeviceType ty)
    {
        switch(ty)
        {
            case MDeviceType::Keyboard   : return MDeviceState{std::in_place_type<MKeyboardState>};
            case MDeviceType::Mouse      : return MDeviceState{std::in_place_type<MMouseState>};
            case MDeviceType::Touchscreen: return MDeviceState{std::in_place_type<MTouchState>};
            case MDeviceType::Gamepad    : return MDeviceState{std::in_place_type<MGamepadState>};
            case MDeviceType::Stylus     : return MDeviceState{std::in_place_type<MStylusState>};
            default: MDeviceState{std::in_place_type<MStylusState>};
        }
    }

    struct MDeviceInfo {
        MDeviceID   id;
        std::string name;
        MDeviceType type;
    };

    } // namespace MW


#endif