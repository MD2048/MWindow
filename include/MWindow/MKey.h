#ifndef M_KEY_H
#define M_KEY_H

#include <cstdint>

namespace MW {

    enum class MMouseButton : uint8_t {
        Unknown = 0,

        Left,
        Right,
        Middle,

        X1,
        X2,
        Count
    };

    enum class MGamepadButton : uint32_t {
        DpadUp, DpadDown, DpadLeft, DpadRight,
        ActionBottom, ActionRight, ActionLeft, ActionTop, // Xbox: A, B, X, Y  PS: Cross, Circle, Square, Triangle
        BumperLeft, BumperRight,
        ThumbLeft, ThumbRight, // Stick clicks
        Start, Select, Guide,
        Count
    };

    enum class MKey : uint32_t {
        Unknown = 0,

        // Alphanumeric
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        Num0, Num1, Num2, Num3, Num4,
        Num5, Num6, Num7, Num8, Num9,

        // Function keys
        F1,  F2,  F3,  F4,  F5,  F6,
        F7,  F8,  F9,  F10, F11, F12,
        F13, F14, F15, F16, F17, F18,  // F13–F24 appear on extended keyboards
        F19, F20, F21, F22, F23, F24, 

        // Modifier
        LeftShift,  RightShift,
        LeftCtrl,   RightCtrl,
        LeftAlt,    RightAlt,   // RightAlt = AltGr on European keyboards 
        LeftGui,    RightGui,   // Windows key / Cmd key

        // Control & editing
        Escape,
        Tab,
        CapsLock,
        Enter,
        Backspace,
        Space,
        Insert,
        Delete,
        Home,
        End,
        PageUp,
        PageDown,

        // Arrow key
        Up,
        Down,
        Left,
        Right,

        // Lock / system
        NumLock,
        ScrollLock,
        PrintScreen,
        Pause,          // Arrives with RI_KEY_E1 flag — unique among all keys
        App,            // Application/Menu key — right of RightGui, opens context menu

        // Media key
        VolumeUp,       // Dedicated key on the top-right cluster of some keyboards
        VolumeDown,
        Mute,

        // Punctuation (position-based, US layout names)
        Grave,          // ` ~
        Minus,          // - _
        Equal,          // = +
        LeftBracket,    // [ {
        RightBracket,   // ] }
        Backslash,      // \ |   present on all ANSI and ISO keyboards
        Semicolon,      // ; :
        Apostrophe,     // ' "
        Comma,          // , 
        Period,         // . >
        Slash,          // / ?

        // ISO-only keys (absent on ANSI/US keyboards)─
        NonUSBackslash, // Extra key between LeftShift and Z on ISO keyboards.
                        // Typically < > on most European layouts, \ | on some others.
                        // Same MakeCode as Backslash but carries RI_KEY_E0.

        NonUSHash,      // Extra key between Apostrophe and Enter on ISO keyboards.
                        // Typically # ~ on UK layout.
                        // The larger ISO Enter key fills the space this occupies on ANSI.

        // Japanese layout keys (JIS keyboards only)
        Yen, Ro, Muhenkan,
        Henkan, KatakanaHiragana, 

        // Numpad
        KP0, KP1, KP2, KP3, KP4,
        KP5, KP6, KP7, KP8, KP9,
        KPDecimal,      // .  (comma on Brazilian and some European layouts — see KPComma)
        KPDivide,       // /  shares MakeCode with main Slash, distinguished by RI_KEY_E0
        KPMultiply,     // *
        KPSubtract,     // -
        KPAdd,          // +
        KPEnter,        // shares MakeCode with main Enter, distinguished by RI_KEY_E0
        KPEqual,        // =  found on Mac keyboards and some extended layouts
        KPComma,        // ,  used as decimal separator on Brazilian/some European numpad layouts

        Count
    };

    #ifdef MWINDOW_BUILD_PRINTS
    constexpr const char* ToString(MMouseButton button) {
        switch (button) {
            case MMouseButton::Unknown: return "Unknown";
            case MMouseButton::Left:    return "Left";
            case MMouseButton::Right:   return "Right";
            case MMouseButton::Middle:  return "Middle";
            case MMouseButton::X1:      return "X1";
            case MMouseButton::X2:      return "X2";
            case MMouseButton::Count:   return "Count";
            default:                    return "Invalid";
        }
    }

    constexpr const char* ToString(MGamepadButton button) {
        switch (button) {
            case MGamepadButton::DpadUp:      return "DpadUp";
            case MGamepadButton::DpadDown:    return "DpadDown";
            case MGamepadButton::DpadLeft:    return "DpadLeft";
            case MGamepadButton::DpadRight:   return "DpadRight";
            case MGamepadButton::ActionBottom:return "ActionBottom";
            case MGamepadButton::ActionRight: return "ActionRight";
            case MGamepadButton::ActionLeft:  return "ActionLeft";
            case MGamepadButton::ActionTop:   return "ActionTop";
            case MGamepadButton::BumperLeft:  return "BumperLeft";
            case MGamepadButton::BumperRight: return "BumperRight";
            case MGamepadButton::ThumbLeft:   return "ThumbLeft";
            case MGamepadButton::ThumbRight:  return "ThumbRight";
            case MGamepadButton::Start:       return "Start";
            case MGamepadButton::Select:      return "Select";
            case MGamepadButton::Guide:       return "Guide";
            case MGamepadButton::Count:       return "Count";
            default:                          return "Invalid";
        }
    }
    #endif

    #ifdef MWINDOW_BUILD_PRINTS
    constexpr const char* ToString(MKey key) {
        switch (key) {
            case MKey::Unknown: return "Unknown";

            // Alphanumeric
            case MKey::A: return "A"; case MKey::B: return "B"; case MKey::C: return "C";
            case MKey::D: return "D"; case MKey::E: return "E"; case MKey::F: return "F";
            case MKey::G: return "G"; case MKey::H: return "H"; case MKey::I: return "I";
            case MKey::J: return "J"; case MKey::K: return "K"; case MKey::L: return "L";
            case MKey::M: return "M"; case MKey::N: return "N"; case MKey::O: return "O";
            case MKey::P: return "P"; case MKey::Q: return "Q"; case MKey::R: return "R";
            case MKey::S: return "S"; case MKey::T: return "T"; case MKey::U: return "U";
            case MKey::V: return "V"; case MKey::W: return "W"; case MKey::X: return "X";
            case MKey::Y: return "Y"; case MKey::Z: return "Z";

            case MKey::Num0: return "Num0"; case MKey::Num1: return "Num1";
            case MKey::Num2: return "Num2"; case MKey::Num3: return "Num3";
            case MKey::Num4: return "Num4"; case MKey::Num5: return "Num5";
            case MKey::Num6: return "Num6"; case MKey::Num7: return "Num7";
            case MKey::Num8: return "Num8"; case MKey::Num9: return "Num9";

            // Function keys
            case MKey::F1: return "F1"; case MKey::F2: return "F2"; case MKey::F3: return "F3";
            case MKey::F4: return "F4"; case MKey::F5: return "F5"; case MKey::F6: return "F6";
            case MKey::F7: return "F7"; case MKey::F8: return "F8"; case MKey::F9: return "F9";
            case MKey::F10: return "F10"; case MKey::F11: return "F11"; case MKey::F12: return "F12";
            case MKey::F13: return "F13"; case MKey::F14: return "F14"; case MKey::F15: return "F15";
            case MKey::F16: return "F16"; case MKey::F17: return "F17"; case MKey::F18: return "F18";
            case MKey::F19: return "F19"; case MKey::F20: return "F20"; case MKey::F21: return "F21";
            case MKey::F22: return "F22"; case MKey::F23: return "F23"; case MKey::F24: return "F24";

            // Modifiers
            case MKey::LeftShift: return "LeftShift"; case MKey::RightShift: return "RightShift";
            case MKey::LeftCtrl:  return "LeftCtrl";  case MKey::RightCtrl:  return "RightCtrl";
            case MKey::LeftAlt:   return "LeftAlt";   case MKey::RightAlt:   return "RightAlt";
            case MKey::LeftGui:   return "LeftGui";   case MKey::RightGui:   return "RightGui";

            // Control & editing
            case MKey::Escape:    return "Escape";
            case MKey::Tab:       return "Tab";
            case MKey::CapsLock:  return "CapsLock";
            case MKey::Enter:     return "Enter";
            case MKey::Backspace: return "Backspace";
            case MKey::Space:     return "Space";
            case MKey::Insert:    return "Insert";
            case MKey::Delete:    return "Delete";
            case MKey::Home:      return "Home";
            case MKey::End:       return "End";
            case MKey::PageUp:    return "PageUp";
            case MKey::PageDown:  return "PageDown";

            // Arrow keys
            case MKey::Up:    return "Up";
            case MKey::Down:  return "Down";
            case MKey::Left:  return "Left";
            case MKey::Right: return "Right";

            // Lock / system
            case MKey::NumLock:     return "NumLock";
            case MKey::ScrollLock:  return "ScrollLock";
            case MKey::PrintScreen: return "PrintScreen";
            case MKey::Pause:       return "Pause";
            case MKey::App:         return "App";

            // Media keys
            case MKey::VolumeUp:   return "VolumeUp";
            case MKey::VolumeDown: return "VolumeDown";
            case MKey::Mute:       return "Mute";

            // Punctuation
            case MKey::Grave:        return "Grave";
            case MKey::Minus:        return "Minus";
            case MKey::Equal:        return "Equal";
            case MKey::LeftBracket:  return "LeftBracket";
            case MKey::RightBracket: return "RightBracket";
            case MKey::Backslash:    return "Backslash";
            case MKey::Semicolon:    return "Semicolon";
            case MKey::Apostrophe:   return "Apostrophe";
            case MKey::Comma:        return "Comma";
            case MKey::Period:       return "Period";
            case MKey::Slash:        return "Slash";

            // ISO-only keys
            case MKey::NonUSBackslash: return "NonUSBackslash";
            case MKey::NonUSHash:      return "NonUSHash";

            // Japanese layout keys
            case MKey::Yen:              return "Yen";
            case MKey::Ro:               return "Ro";
            case MKey::Muhenkan:         return "Muhenkan";
            case MKey::Henkan:           return "Henkan";
            case MKey::KatakanaHiragana: return "KatakanaHiragana";

            // Numpad
            case MKey::KP0:        return "KP0";
            case MKey::KP1:        return "KP1";
            case MKey::KP2:        return "KP2";
            case MKey::KP3:        return "KP3";
            case MKey::KP4:        return "KP4";
            case MKey::KP5:        return "KP5";
            case MKey::KP6:        return "KP6";
            case MKey::KP7:        return "KP7";
            case MKey::KP8:        return "KP8";
            case MKey::KP9:        return "KP9";
            case MKey::KPDecimal:  return "KPDecimal";
            case MKey::KPDivide:   return "KPDivide";
            case MKey::KPMultiply: return "KPMultiply";
            case MKey::KPSubtract: return "KPSubtract";
            case MKey::KPAdd:      return "KPAdd";
            case MKey::KPEnter:    return "KPEnter";
            case MKey::KPEqual:    return "KPEqual";
            case MKey::KPComma:    return "KPComma";

            case MKey::Count:      return "Count";
            default:               return "Invalid";
        }
    }
    #endif
}


#endif