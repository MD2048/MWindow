#ifndef M_DEVICES_H
#define M_DEVICES_H

#include <bitset>
#include <unordered_map>
#include <variant>
#include <string>

#include "MWindow/MDef.h"
#include "MWindow/MKey.h"

namespace MW
{
    using MDeviceID = uint64_t;

    enum class MDeviceType   { Unknown = -1, Keyboard, Mouse, Touchscreen, Stylus, Gamepad };
    // Gamepad = Controller

    struct MMods {
        bool shift : 1;
        bool ctrl  : 1;
        bool alt   : 1;
        bool super : 1;   // Win / Cmd
        bool caps  : 1;   // CapsLock state, not the key itself
        bool num   : 1;   // NumLock state

        bool none() const { return !shift && !ctrl && !alt && !super; }
        void update(MKey k, bool press)
        {
            /*LeftShift,  RightShift,
        LeftCtrl,   RightCtrl,
        LeftAlt,    RightAlt,   // RightAlt = AltGr on European keyboards —
                                // generates RI_KEY_E0 on the same MakeCode as LeftAlt
        LeftGui,    RightGui,*/
            switch(k)
            {
                case MKey::LeftShift:
                case MKey::RightShift:
                    shift = press; return;
                case MKey::LeftCtrl:
                case MKey::RightCtrl:
                    ctrl = press; return;
                case MKey::LeftAlt:
                case MKey::RightAlt:
                    alt = press; return;
                case MKey::LeftGui:
                case MKey::RightGui:
                    super = press; return;
                case MKey::CapsLock:
                    if(press) caps = !caps; return;
                case MKey::NumLock:
                    if(press) num = !num; return;
            }
        }
    };

    struct MKeyboardState {
        std::bitset<static_cast<size_t>(MKey::Count)> held;
        MMods mods;
    };

    struct MMouseState {
        MPoint p;
        std::bitset<static_cast<size_t>(MMouseButton::Count)> buttons;
    };

    struct MTouchState {
        std::unordered_map<uint32_t, MPoint> activePoints; // touch id → point
    };

    // Reserved — no fields yet, just a placeholder so the variant compiles
    struct MGamepadState {};
    struct MStylusState  {
        bool    inRange   = false;
        bool    inContact = false;
        MPoint  pos       = {0,0};
    };

    using MDeviceState = std::variant<
        MKeyboardState,
        MMouseState,
        MTouchState,
        MGamepadState,
        MStylusState
    >;

    inline const char* toString(MDeviceType type) {
        switch (type) {
            case MDeviceType::Keyboard:    return "Keyboard";
            case MDeviceType::Mouse:       return "Mouse";
            case MDeviceType::Touchscreen: return "Touchscreen";
            case MDeviceType::Gamepad:     return "Gamepad";
            case MDeviceType::Stylus:      return "Stylus";
            default:                       return "Unknown";
        }
    }

    // 3. MMods Formatter
    inline std::ostream& operator<<(std::ostream& os, const MMods& mods) {
        os << "[ ";
        if (mods.shift) os << "Shift ";
        if (mods.ctrl)  os << "Ctrl ";
        if (mods.alt)   os << "Alt ";
        if (mods.super) os << "Super ";
        if (mods.caps)  os << "Caps ";
        if (mods.num)   os << "Num ";
        
        // Print "None" if all action modifiers are false and toggles are off
        if (mods.none() && !mods.caps && !mods.num) os << "None ";
        os << "]";
        return os;
    }

    // 4. State Formatters
    inline std::ostream& operator<<(std::ostream& os, const MKeyboardState& state) {
        os << "  Keyboard State:\n"
        << "    Keys Held:  " << state.held.count() << " active\n"
        << "    Modifiers:  " << state.mods;
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MMouseState& state) {
        os << "  Mouse State:\n"
        << "    Position:   " << state.p << "\n"
        << "    Btns Held:  " << state.buttons.count() << " active";
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MTouchState& state) {
        os << "  Touch State:\n"
        << "    Active Pts: " << state.activePoints.size();
        
        if (!state.activePoints.empty()) {
            os << "\n    Coordinates:";
            for (const auto& [id, pt] : state.activePoints) {
                os << "\n      [ID " << id << "] -> " << pt;
            }
        }
        return os;
    }

    // 5. MDeviceState (Variant) Formatter
    template<class... Ts>
    struct OVL : Ts...
    {
        using Ts::operator()...;
    };

    template<class... Ts>
    OVL(Ts...) -> OVL<Ts...>;
    inline std::ostream& operator<<(std::ostream& os, const MDeviceState& state) {
        std::visit(OVL{
            [&os](const MKeyboardState& s) { os << s; },
            [&os](const MMouseState& s)    { os << s; },
            [&os](const MTouchState& s)    { os << s; },
            [&os](const MGamepadState&)    { os << "  Gamepad State:\n    [Reserved / No Data]"; },
            [&os](const MStylusState&)     { os << "  Stylus State:\n    [Reserved / No Data]"; }
        }, state);
        return os;
    }

} // namespace MW

#endif