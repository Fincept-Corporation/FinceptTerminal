#pragma once
// KeyRouting — who gets a key press when the shell and a screen both want it.
//
// ── The rule (for screen authors) ─────────────────────────────────────────────
//  1. The focused widget first. If the focused widget (or an ancestor up to the
//     window) ACCEPTS the QEvent::ShortcutOverride for the key, it gets the key
//     press — no shortcut fires. QLineEdit does this for typing keys; a pop-up
//     list such as ui::SymbolSearchBox does it for Esc / Up / Down / Enter while
//     its list is open. Do the same in your own widget when it needs a key that a
//     shortcut might also be bound to:
//        bool event(QEvent* e) override {
//            if (e->type() == QEvent::ShortcutOverride && wants(static_cast<QKeyEvent*>(e))) {
//                e->accept(); return true;
//            }
//            return QWidget::event(e);
//        }
//  2. Typing and list keys stay with inputs, lists and pop-ups. With focus in a
//     text input (line edit, text edit, spin box, editable combo) the editing keys
//     — Esc, arrows, Home/End, PgUp/PgDn, Enter, Backspace/Delete, printable keys
//     — are delivered to it; in an item view the navigation keys and Esc are.
//     Unhandled, the key press then travels up the parent chain (keyPressEvent),
//     so a panel's "Esc closes the inline add row" / "Esc un-maximises" works.
//  3. The narrowest shortcut wins. Among shortcuts bound to the same key, the one
//     closest to the focused widget is triggered: Qt::WidgetShortcut on the focus
//     widget, then Qt::WidgetWithChildrenShortcut / Qt::WindowShortcut on its
//     ancestors (your screen), then the window's and the application's own
//     (WindowFrame actions, KeyConfigManager). Qt alone would call two matches
//     "ambiguous" and fire neither.
//  4. Reserved keys stay global. Shortcuts with Ctrl / Alt / Meta and the
//     function keys F1–F35 belong to the shell (F5 Refresh, F9 Chat Mode, F10
//     Focus Mode, F11 Fullscreen, Ctrl+L Lock, Ctrl+1..9 windows …): when the
//     shell binds the key it wins over a screen's shortcut — unless the screen
//     claims the key explicitly:
//        ui::claim_keys(this, {QKeySequence(Qt::Key_F9)});   // focus inside `this` → yours
//     A claimed key goes to the screen's narrowest shortcut for it, or, without
//     one, to the focused widget as a key press (handle it in keyPressEvent).
//  5. App-wide single-key shortcuts use Qt::WindowShortcut or narrower — never
//     Qt::ApplicationShortcut (a key with no modifier must not act in another
//     window or behind a dialog).
//
// Installed once on the QApplication (install_shell_key_router, main.cpp). The
// decision itself is route_key() below: pure, unit-tested (tst_key_routing).

#include <QKeySequence>
#include <QList>

#include <initializer_list>

class QApplication;
class QWidget;

namespace fincept::ui {

/// What has focus, as far as key routing is concerned.
enum class KeyFocusKind { None, TextInput, ItemView, Other };

/// Everything route_key() needs to know about one key press (the focused widget
/// did NOT accept the ShortcutOverride itself — rule 1 is applied before).
struct KeyRouteInput {
    int key = 0;                       ///< Qt::Key
    bool command_modifier = false;     ///< Ctrl, Alt or Meta held
    bool printable = false;            ///< produces text (a letter, digit, punctuation, space)
    bool popup_open = false;           ///< an application pop-up (menu, completer, combo list) is open
    KeyFocusKind focus = KeyFocusKind::None;
    int narrow_matches = 0;            ///< enabled shortcuts for the key scoped to the focus chain below the window
    int broad_matches = 0;             ///< enabled shortcuts for the key on the window itself or application-wide
    bool claimed = false;              ///< a widget in the focus chain claim_keys()'d this key
};

enum class KeyRoute {
    Qt,        ///< leave it to Qt's normal shortcut processing (no conflict to resolve)
    Widget,    ///< deliver the key press to the focused widget (it propagates to its parents)
    Narrowest, ///< trigger the shortcut closest to the focused widget
    Broadest,  ///< trigger the window / application shortcut (the shell's)
};

inline bool is_function_key(int key) {
    return key >= Qt::Key_F1 && key <= Qt::Key_F35;
}

/// Keys a text input uses for editing (besides printable text).
inline bool is_text_editing_key(int key) {
    switch (key) {
        case Qt::Key_Escape:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            return true;
        default:
            return false;
    }
}

/// Keys a list / table / tree / pop-up list uses.
inline bool is_list_key(int key) {
    switch (key) {
        case Qt::Key_Escape:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            return true;
        default:
            return false;
    }
}

/// The routing rule (see the header comment). Pure.
inline KeyRoute route_key(const KeyRouteInput& in) {
    const bool conflict = in.narrow_matches > 0 && in.broad_matches > 0;
    // Rule 4: modifier shortcuts and function keys are the shell's unless claimed.
    if (in.command_modifier || is_function_key(in.key)) {
        if (in.claimed)
            return in.narrow_matches > 0 ? KeyRoute::Narrowest : KeyRoute::Widget;
        if (conflict)
            return KeyRoute::Broadest;
        return KeyRoute::Qt;
    }
    // Rule 2 (pop-up): an open pop-up list owns its navigation keys.
    if (in.popup_open && is_list_key(in.key))
        return KeyRoute::Widget;
    // Explicit claim of a plain key.
    if (in.claimed)
        return in.narrow_matches > 0 ? KeyRoute::Narrowest : KeyRoute::Widget;
    // Rule 3: the screen's own (narrower) shortcut beats the shell's — and a
    // sibling screen's same-key Qt::WindowShortcut, which Qt would otherwise
    // call ambiguous.
    if (in.narrow_matches > 0)
        return KeyRoute::Narrowest;
    // Rule 2: typing / list keys stay with the focused input or list — but only
    // when a broad shortcut would otherwise take them (nothing to change if not).
    if (in.broad_matches > 0) {
        if (in.focus == KeyFocusKind::TextInput && (in.printable || is_text_editing_key(in.key)))
            return KeyRoute::Widget;
        if (in.focus == KeyFocusKind::ItemView && is_list_key(in.key))
            return KeyRoute::Widget;
    }
    return KeyRoute::Qt;
}

/// Declare that, while focus is inside `scope`, the keys in `keys` belong to
/// `scope` (rule 4). Replaces earlier claims of `scope`. Safe on any widget.
void claim_keys(QWidget* scope, std::initializer_list<QKeySequence> keys);
void claim_keys(QWidget* scope, const QList<QKeySequence>& keys);

/// Install the application-wide router (once; later calls are no-ops).
void install_shell_key_router(QApplication* app);

} // namespace fincept::ui
