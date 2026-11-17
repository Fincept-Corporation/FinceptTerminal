// KeyRouting — the application-wide ShortcutOverride router (see KeyRouting.h).

#include "ui/navigation/KeyRouting.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QShortcut>
#include <QTextEdit>
#include <QVariant>
#include <QWidget>

namespace fincept::ui {

namespace key_routing_detail {

constexpr const char kClaimProperty[] = "_fincept_claimed_keys";

QKeySequence kr_pressed(const QKeyEvent* ke) {
    const Qt::KeyboardModifiers mods = ke->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
    int key = ke->key();
    if (key == Qt::Key_Backtab)
        key = Qt::Key_Tab; // Shift+Tab arrives as Backtab; bindings say Shift+Tab
    return QKeySequence(QKeyCombination(mods, Qt::Key(key)));
}

bool kr_matches(const QList<QKeySequence>& seqs, const QKeySequence& pressed) {
    for (const QKeySequence& s : seqs) {
        if (s.count() == 1 && s == pressed)
            return true;
    }
    return false;
}

KeyFocusKind kr_focus_kind(QWidget* w) {
    if (!w)
        return KeyFocusKind::None;
    if (qobject_cast<QLineEdit*>(w) || qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w) ||
        qobject_cast<QAbstractSpinBox*>(w))
        return KeyFocusKind::TextInput;
    if (auto* combo = qobject_cast<QComboBox*>(w); combo && combo->isEditable())
        return KeyFocusKind::TextInput;
    if (qobject_cast<QAbstractItemView*>(w))
        return KeyFocusKind::ItemView;
    return KeyFocusKind::Other;
}

/// One shortcut that matches the pressed key.
struct Candidate {
    QPointer<QObject> object; // QShortcut or QAction
    bool narrow = false;
    bool auto_repeat = true;
};

void kr_consider(QObject* obj, const QList<QKeySequence>& keys, Qt::ShortcutContext ctx, bool enabled,
                 bool auto_repeat, QWidget* owner, QWidget* focus, QWidget* window, const QKeySequence& pressed,
                 QList<Candidate>& narrow, QList<Candidate>& broad) {
    if (!enabled || !kr_matches(keys, pressed))
        return;
    switch (ctx) {
        case Qt::WidgetShortcut:
            if (owner == focus)
                narrow.append({obj, true, auto_repeat});
            return;
        case Qt::WidgetWithChildrenShortcut:
            narrow.append({obj, true, auto_repeat});
            return;
        case Qt::WindowShortcut:
            if (owner == window)
                broad.append({obj, false, auto_repeat});
            else
                narrow.append({obj, true, auto_repeat});
            return;
        case Qt::ApplicationShortcut:
            broad.append({obj, false, auto_repeat});
            return;
    }
}

/// Matching shortcuts on the focus chain (focus widget → window), narrowest first.
void kr_collect(QWidget* focus, const QKeySequence& pressed, QList<Candidate>& narrow, QList<Candidate>& broad,
                bool& claimed) {
    QWidget* window = focus ? focus->window() : nullptr;
    for (QWidget* w = focus; w; w = w->parentWidget()) {
        if (!w->isVisible() || !w->isEnabled()) {
            if (w == window)
                break;
            continue;
        }
        if (!claimed) {
            const QVariant c = w->property(kClaimProperty);
            if (c.isValid() && kr_matches(c.value<QList<QKeySequence>>(), pressed))
                claimed = true;
        }
        for (QShortcut* s : w->findChildren<QShortcut*>(Qt::FindDirectChildrenOnly))
            kr_consider(s, s->keys(), s->context(), s->isEnabled(), s->autoRepeat(), w, focus, window, pressed,
                        narrow, broad);
        for (QAction* a : w->actions())
            kr_consider(a, a->shortcuts(), a->shortcutContext(), a->isEnabled(), a->autoRepeat(), w, focus, window,
                        pressed, narrow, broad);
        if (w == window || w->isWindow())
            break;
    }
}

void kr_trigger(const Candidate& c, bool auto_repeat_event) {
    if (!c.object || (auto_repeat_event && !c.auto_repeat))
        return;
    if (auto* a = qobject_cast<QAction*>(c.object.data())) {
        a->trigger();
        return;
    }
    if (auto* s = qobject_cast<QShortcut*>(c.object.data()))
        Q_EMIT s->activated();
}

class ShellKeyRouter final : public QObject {
  public:
    explicit ShellKeyRouter(QObject* parent) : QObject(parent) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::ShortcutOverride)
            return on_override(watched, static_cast<QKeyEvent*>(event));
        if (event->type() == QEvent::KeyPress)
            return on_key_press(static_cast<QKeyEvent*>(event));
        return false;
    }

  private:
    bool on_override(QObject* watched, QKeyEvent* ke) {
        if (dispatching_ || !watched->isWidgetType())
            return false;
        auto* focus = static_cast<QWidget*>(watched);
        // QApplication offers an unaccepted override to each parent in turn.
        // The decision is made once, where the event starts (the focus widget,
        // or the window when nothing has focus); the inner dispatch below has
        // already shown it to every parent.
        QWidget* const fw = QApplication::focusWidget();
        if (fw ? focus != fw : !focus->isWindow())
            return true;
        pending_ = {};

        // Rule 1: the focused widget (and its parents) first.
        ke->ignore();
        dispatching_ = true;
        QCoreApplication::sendEvent(focus, ke);
        dispatching_ = false;
        if (ke->isAccepted())
            return true;

        const QKeySequence pressed = kr_pressed(ke);
        QList<Candidate> narrow;
        QList<Candidate> broad;
        bool claimed = false;
        kr_collect(focus, pressed, narrow, broad, claimed);

        KeyRouteInput in;
        in.key = ke->key();
        in.command_modifier = (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != 0;
        in.printable = !in.command_modifier && !ke->text().isEmpty() && ke->text().at(0).isPrint();
        in.popup_open = QApplication::activePopupWidget() != nullptr;
        in.focus = kr_focus_kind(focus);
        in.narrow_matches = static_cast<int>(narrow.size());
        in.broad_matches = static_cast<int>(broad.size());
        in.claimed = claimed;

        switch (route_key(in)) {
            case KeyRoute::Qt:
                return true; // not accepted: QShortcutMap proceeds as usual
            case KeyRoute::Widget:
                ke->accept(); // the key press goes to the focused widget
                return true;
            case KeyRoute::Narrowest:
                pending_ = {narrow.value(0), pressed, true};
                ke->accept();
                return true;
            case KeyRoute::Broadest:
                pending_ = {broad.value(0), pressed, true};
                ke->accept();
                return true;
        }
        return true;
    }

    bool on_key_press(QKeyEvent* ke) {
        if (!pending_.armed)
            return false;
        if (kr_pressed(ke) != pending_.key) {
            pending_ = {};
            return false;
        }
        const Pending p = pending_;
        pending_ = {};
        ke->accept(); // accepted + handled: QApplication offers it to no parent
        kr_trigger(p.target, ke->isAutoRepeat());
        return true;
    }

    struct Pending {
        Candidate target;
        QKeySequence key;
        bool armed = false;
    };

    bool dispatching_ = false;
    Pending pending_;
};

} // namespace key_routing_detail

void claim_keys(QWidget* scope, const QList<QKeySequence>& keys) {
    if (scope)
        scope->setProperty(key_routing_detail::kClaimProperty, QVariant::fromValue(keys));
}

void claim_keys(QWidget* scope, std::initializer_list<QKeySequence> keys) {
    claim_keys(scope, QList<QKeySequence>(keys));
}

void install_shell_key_router(QApplication* app) {
    if (!app || app->findChild<QObject*>(QStringLiteral("fincept_shell_key_router"), Qt::FindDirectChildrenOnly))
        return;
    auto* router = new key_routing_detail::ShellKeyRouter(app);
    router->setObjectName(QStringLiteral("fincept_shell_key_router"));
    app->installEventFilter(router);
}

} // namespace fincept::ui
