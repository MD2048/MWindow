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

    enum class MKey : uint32_t {
        Unknown = 0,

        // ── Alphanumeric ──────────────────────────────────────────────────────
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        Num0, Num1, Num2, Num3, Num4,
        Num5, Num6, Num7, Num8, Num9,

        // ── Function keys ─────────────────────────────────────────────────────
        F1,  F2,  F3,  F4,  F5,  F6,
        F7,  F8,  F9,  F10, F11, F12,
        F13, F14, F15, F16, F17, F18,  // F13–F24 appear on extended keyboards,
        F19, F20, F21, F22, F23, F24,  // some Macs, and media keyboards

        // ── Modifiers ─────────────────────────────────────────────────────────
        LeftShift,  RightShift,
        LeftCtrl,   RightCtrl,
        LeftAlt,    RightAlt,   // RightAlt = AltGr on European keyboards —
                                // generates RI_KEY_E0 on the same MakeCode as LeftAlt
        LeftGui,    RightGui,   // Windows key / Cmd key

        // ── Control & editing ─────────────────────────────────────────────────
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

        // ── Arrow keys ────────────────────────────────────────────────────────
        Up,
        Down,
        Left,
        Right,

        // ── Lock / system ─────────────────────────────────────────────────────
        NumLock,
        ScrollLock,
        PrintScreen,
        Pause,          // Arrives with RI_KEY_E1 flag — unique among all keys
        App,            // Application/Menu key — right of RightGui, opens context menu

        // ── Media keys ────────────────────────────────────────────────────────
        VolumeUp,       // Dedicated key on the top-right cluster of some keyboards
        VolumeDown,
        Mute,

        // ── Punctuation (position-based, US layout names) ─────────────────────
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

        // ── ISO-only keys (absent on ANSI/US keyboards) ───────────────────────
        NonUSBackslash, // Extra key between LeftShift and Z on ISO keyboards.
                        // Typically < > on most European layouts, \ | on some others.
                        // Same MakeCode as Backslash but carries RI_KEY_E0.

        NonUSHash,      // Extra key between Apostrophe and Enter on ISO keyboards.
                        // Typically # ~ on UK layout.
                        // The larger ISO Enter key fills the space this occupies on ANSI.

        // ── Japanese layout keys (JIS keyboards only) ─────────────────────────
        Yen, Ro, Muhenkan,
        Henkan, KatakanaHiragana, 

        // ── Numpad ────────────────────────────────────────────────────────────
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
}


#endif