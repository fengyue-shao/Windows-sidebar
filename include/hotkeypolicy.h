#pragma once
#include <QKeySequence>
#include <QString>
#include <Windows.h>
namespace SidebarHotkey {
struct Parsed { bool valid = false; UINT modifiers = 0, key = 0; QString text; };
inline Parsed parse(const QString &text) {
    const auto sequence = QKeySequence::fromString(text.trimmed(), QKeySequence::PortableText);
    if (sequence.count() != 1 || sequence.isEmpty()) return {};
    const auto combination = sequence[0]; const auto flags = combination.keyboardModifiers();
    if (!(flags & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) return {};
    UINT modifiers = MOD_NOREPEAT;
    if (flags & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (flags & Qt::AltModifier) modifiers |= MOD_ALT;
    if (flags & Qt::ShiftModifier) modifiers |= MOD_SHIFT;
    if (flags & Qt::MetaModifier) modifiers |= MOD_WIN;
    const int key = combination.key(); UINT native = 0;
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9)) native = key;
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) native = VK_F1 + key - Qt::Key_F1;
    else if (key == Qt::Key_Space) native = VK_SPACE;
    if (!native) return {};
    return {true, modifiers, native, sequence.toString(QKeySequence::PortableText)};
}
struct SwapResult { bool accepted; DWORD error; };
template<class Register, class RemoveOld, class Undo>
SwapResult replace(bool oldActive, Register add, RemoveOld removeOld, Undo undo) {
    const DWORD error = add(); if (error) return {false, error};
    if (oldActive) {
        const DWORD removed = removeOld();
        if (removed) { undo(); return {false, removed}; }
    }
    return {true, 0};
}
}
