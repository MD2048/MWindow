#ifndef M_KEYTRANSLATION_H
#define M_KEYTRANSLATION_H

#include "MWindow/MEvents.h"

namespace MW {

    static MKey sScanToMKey[512] = {};

    static void buildScanTable() {
        auto reg = [](uint32_t code, MKey key) { sScanToMKey[code] = key; };

        // No E0 prefix
        reg(0x01, MKey::Escape);
        reg(0x02, MKey::Num1);          reg(0x03, MKey::Num2);
        reg(0x04, MKey::Num3);          reg(0x05, MKey::Num4);
        reg(0x06, MKey::Num5);          reg(0x07, MKey::Num6);
        reg(0x08, MKey::Num7);          reg(0x09, MKey::Num8);
        reg(0x0A, MKey::Num9);          reg(0x0B, MKey::Num0);
        reg(0x0C, MKey::Minus);         reg(0x0D, MKey::Equal);
        reg(0x0E, MKey::Backspace);     reg(0x0F, MKey::Tab);
        reg(0x10, MKey::Q);             reg(0x11, MKey::W);
        reg(0x12, MKey::E);             reg(0x13, MKey::R);
        reg(0x14, MKey::T);             reg(0x15, MKey::Y);
        reg(0x16, MKey::U);             reg(0x17, MKey::I);
        reg(0x18, MKey::O);             reg(0x19, MKey::P);
        reg(0x1A, MKey::LeftBracket);   reg(0x1B, MKey::RightBracket);
        reg(0x1C, MKey::Enter);
        reg(0x1D, MKey::LeftCtrl);
        reg(0x1E, MKey::A);             reg(0x1F, MKey::S);
        reg(0x20, MKey::D);             reg(0x21, MKey::F);
        reg(0x22, MKey::G);             reg(0x23, MKey::H);
        reg(0x24, MKey::J);             reg(0x25, MKey::K);
        reg(0x26, MKey::L);
        reg(0x27, MKey::Semicolon);     reg(0x28, MKey::Apostrophe);
        reg(0x29, MKey::Grave);
        reg(0x2A, MKey::LeftShift);
        reg(0x2B, MKey::Backslash);
        reg(0x2C, MKey::Z);             reg(0x2D, MKey::X);
        reg(0x2E, MKey::C);             reg(0x2F, MKey::V);
        reg(0x30, MKey::B);             reg(0x31, MKey::N);
        reg(0x32, MKey::M);
        reg(0x33, MKey::Comma);         reg(0x34, MKey::Period);
        reg(0x35, MKey::Slash);
        reg(0x36, MKey::RightShift);
        reg(0x37, MKey::KPMultiply);
        reg(0x38, MKey::LeftAlt);
        reg(0x39, MKey::Space);
        reg(0x3A, MKey::CapsLock);
        reg(0x3B, MKey::F1);            reg(0x3C, MKey::F2);
        reg(0x3D, MKey::F3);            reg(0x3E, MKey::F4);
        reg(0x3F, MKey::F5);            reg(0x40, MKey::F6);
        reg(0x41, MKey::F7);            reg(0x42, MKey::F8);
        reg(0x43, MKey::F9);            reg(0x44, MKey::F10);
        reg(0x45, MKey::NumLock);
        reg(0x46, MKey::ScrollLock);
        reg(0x47, MKey::KP7);           reg(0x48, MKey::KP8);
        reg(0x49, MKey::KP9);           reg(0x4A, MKey::KPSubtract);
        reg(0x4B, MKey::KP4);           reg(0x4C, MKey::KP5);
        reg(0x4D, MKey::KP6);           reg(0x4E, MKey::KPAdd);
        reg(0x4F, MKey::KP1);           reg(0x50, MKey::KP2);
        reg(0x51, MKey::KP3);           reg(0x52, MKey::KP0);
        reg(0x53, MKey::KPDecimal);
        reg(0x57, MKey::F11);           reg(0x58, MKey::F12);
        reg(0x56, MKey::NonUSBackslash);
        reg(0x59, MKey::F13);           reg(0x5A, MKey::F14);
        reg(0x5B, MKey::F15);           reg(0x5C, MKey::F16);
        reg(0x5D, MKey::F17);           reg(0x5E, MKey::F18);
        reg(0x5F, MKey::F19);           reg(0x60, MKey::F20);
        reg(0x61, MKey::F21);           reg(0x62, MKey::F22);
        reg(0x63, MKey::F23);           reg(0x64, MKey::F24);
        // Japanese
        reg(0x70, MKey::KatakanaHiragana);
        reg(0x73, MKey::Ro);
        reg(0x79, MKey::Henkan);
        reg(0x7B, MKey::Muhenkan);
        reg(0x7D, MKey::Yen);
        // ISO
        reg(0x2B, MKey::NonUSHash);     // same position, ISO keyboards only

        // E0-prefixed (index | 0x100)
        reg(0x100 | 0x10, MKey::VolumeDown); // media cluster varies by keyboard,
        reg(0x100 | 0x20, MKey::VolumeUp);   // these cover common mappings
        reg(0x100 | 0x1C, MKey::KPEnter);
        reg(0x100 | 0x1D, MKey::RightCtrl);
        reg(0x100 | 0x35, MKey::KPDivide);
        reg(0x100 | 0x38, MKey::RightAlt);
        reg(0x100 | 0x47, MKey::Home);
        reg(0x100 | 0x48, MKey::Up);
        reg(0x100 | 0x49, MKey::PageUp);
        reg(0x100 | 0x4B, MKey::Left);
        reg(0x100 | 0x4D, MKey::Right);
        reg(0x100 | 0x4F, MKey::End);
        reg(0x100 | 0x50, MKey::Down);
        reg(0x100 | 0x51, MKey::PageDown);
        reg(0x100 | 0x52, MKey::Insert);
        reg(0x100 | 0x53, MKey::Delete);
        reg(0x100 | 0x5B, MKey::LeftGui);
        reg(0x100 | 0x5C, MKey::RightGui);
        reg(0x100 | 0x5D, MKey::App);
        reg(0x100 | 0x5E, MKey::Mute);
        reg(0x100 | 0x30, MKey::VolumeUp);
        reg(0x100 | 0x2E, MKey::VolumeDown);
        reg(0x100 | 0x37, MKey::PrintScreen);
    }

    MKey translateMakeCode(unsigned short makeCode, bool e0) {
        uint32_t index = makeCode | (e0 ? 0x100u : 0u);
        if (index >= 512) return MKey::Unknown;
        return sScanToMKey[index];
    }
}

#endif