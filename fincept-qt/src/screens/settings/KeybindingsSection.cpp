// src/screens/settings/KeybindingsSection.cpp
#include "screens/settings/KeybindingsSection.h"

#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/actions/builtin_actions.h"
#include "core/keys/KeyConfigManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace fincept::screens {

namespace keybindings_detail {

/// Where a binding can fire. Global / Windows actions are QActions on every
/// WindowFrame (window scope), so they collide with everything; the per-screen
/// groups only collide with themselves — News "Esc" and command-bar "Esc" can
/// never both be live for the same key press.
enum class Scope { Window, CommandBar, News, CodeEditor };

Scope scope_of(KeyAction a) {
    switch (a) {
        case KeyAction::NavNext:
        case KeyAction::NavPrev:
        case KeyAction::NavAccept:
        case KeyAction::NavEscape:
            return Scope::CommandBar;
        case KeyAction::NewsNext:
        case KeyAction::NewsPrev:
        case KeyAction::NewsOpen:
        case KeyAction::NewsClose:
            return Scope::News;
        case KeyAction::RunCell:
        case KeyAction::RunAndNext:
        case KeyAction::RenameCell:
            return Scope::CodeEditor;
        default:
            return Scope::Window;
    }
}

bool scopes_overlap(Scope a, Scope b) {
    return a == b || a == Scope::Window || b == Scope::Window;
}

QString scope_text(Scope s) {
    switch (s) {
        case Scope::Window:
            return KeybindingsSection::tr("Active in every terminal window.");
        case Scope::CommandBar:
            return KeybindingsSection::tr("Active while the command bar's suggestion list is open.");
        case Scope::News:
            return KeybindingsSection::tr("Active while the News screen has focus.");
        case Scope::CodeEditor:
            return KeybindingsSection::tr("Active while a notebook cell or the cell list has focus.");
    }
    return {};
}

/// Hotkeys the ActionRegistry binds on every window that are NOT KeyActions
/// (command bar toggle Ctrl+\, command palette Ctrl+Shift+P). KeyConfigManager
/// does not know them, so its own conflict check missed them.
QList<QPair<QString, QKeySequence>> registry_hotkeys() {
    QSet<QString> key_bound;
    for (KeyAction a : KeyConfigManager::instance().all_actions()) {
        const QString id = actions::action_id_for(a);
        if (!id.isEmpty())
            key_bound.insert(id);
    }
    QList<QPair<QString, QKeySequence>> out;
    auto& reg = ActionRegistry::instance();
    for (const QString& id : reg.all_ids()) {
        if (key_bound.contains(id))
            continue;
        const ActionDef* def = reg.find(id);
        if (!def || def->default_hotkey.isEmpty())
            continue;
        out.append({def->display.isEmpty() ? id : def->display, def->default_hotkey});
    }
    return out;
}

/// Display name of the binding that `seq` would collide with if given to `a`,
/// or an empty string when it is free where `a` fires.
QString collision_for(KeyAction a, const QKeySequence& seq) {
    if (seq.isEmpty())
        return {};
    auto& km = KeyConfigManager::instance();
    const Scope sa = scope_of(a);
    for (KeyAction other : km.all_actions()) {
        if (other == a)
            continue;
        if (km.key(other) == seq && scopes_overlap(sa, scope_of(other)))
            return km.display_name(other);
    }
    // Registry hotkeys are window-scoped, so they collide with every scope.
    for (const auto& hk : registry_hotkeys()) {
        if (hk.second == seq)
            return hk.first;
    }
    return {};
}

/// Bare editing / navigation keys. As a window-wide shortcut they would fire
/// instead of reaching text fields and dialogs (Enter would stop submitting
/// forms everywhere), so a global action needs a Ctrl/Alt modifier with them.
bool is_bare_editing_key(const QKeySequence& seq) {
    if (seq.count() != 1)
        return false;
    const QKeyCombination kc = seq[0];
    const Qt::KeyboardModifiers m = kc.keyboardModifiers() & ~Qt::KeypadModifier;
    if (m != Qt::NoModifier && m != Qt::ShiftModifier)
        return false;
    switch (kc.key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Escape:
        case Qt::Key_Space:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            return true;
        default:
            return false;
    }
}

void set_flag(QWidget* w, const char* name, bool on) {
    if (!w || w->property(name).toBool() == on)
        return;
    w->setProperty(name, on);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// Group titles KeyConfigManager reports in English; shown translated.
QString group_title(const QString& group) {
    if (group == QLatin1String("Global"))
        return KeybindingsSection::tr("GLOBAL");
    if (group == QLatin1String("Navigation"))
        return KeybindingsSection::tr("COMMAND BAR");
    if (group == QLatin1String("News"))
        return KeybindingsSection::tr("NEWS");
    if (group == QLatin1String("Code Editor"))
        return KeybindingsSection::tr("CODE EDITOR");
    if (group == QLatin1String("Windows"))
        return KeybindingsSection::tr("WINDOWS");
    return group.toUpper();
}

} // namespace keybindings_detail

namespace kbd = keybindings_detail;

// ── KeyCaptureDialog ──────────────────────────────────────────────────────────

KeyCaptureDialog::KeyCaptureDialog(KeyAction action, const QKeySequence& current, QWidget* parent)
    : QDialog(parent), action_(action), current_(current) {
    setObjectName(QStringLiteral("kbCaptureDlg"));
    setWindowTitle(tr("Rebind: %1").arg(KeyConfigManager::instance().display_name(action)));
    setFixedSize(420, 230);
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    current_label_ = new QLabel(tr("Current: %1").arg(current.toString(QKeySequence::NativeText)));
    current_label_->setObjectName(QStringLiteral("kbCaptureCurrent"));

    scope_label_ = new QLabel(kbd::scope_text(kbd::scope_of(action)));
    scope_label_->setObjectName(QStringLiteral("kbCaptureScope"));
    scope_label_->setWordWrap(true);

    hint_label_ = new QLabel(tr("Press new key combination..."));
    hint_label_->setObjectName(QStringLiteral("kbCaptureHint"));

    captured_label_ = new QLabel;
    captured_label_->setObjectName(QStringLiteral("kbCaptured"));
    captured_label_->setAlignment(Qt::AlignCenter);

    conflict_label_ = new QLabel;
    conflict_label_->setObjectName(QStringLiteral("kbCaptureProblem"));
    conflict_label_->setWordWrap(true);
    conflict_label_->hide();

    auto* btn_box = new QDialogButtonBox(Qt::Horizontal);
    apply_btn_ = btn_box->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    cancel_btn_ = btn_box->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    apply_btn_->setEnabled(false);
    // Every key — Enter and Space included — belongs to the capture, so the
    // buttons must never take keyboard focus (or Enter would press Apply).
    for (QPushButton* b : {apply_btn_, cancel_btn_}) {
        b->setFocusPolicy(Qt::NoFocus);
        b->setAutoDefault(false);
        b->setDefault(false);
    }

    connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(current_label_);
    layout->addWidget(scope_label_);
    layout->addWidget(hint_label_);
    layout->addWidget(captured_label_);
    layout->addWidget(conflict_label_);
    layout->addStretch();
    layout->addWidget(btn_box);

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

void KeyCaptureDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void KeyCaptureDialog::retranslateUi() {
    setWindowTitle(tr("Rebind: %1").arg(KeyConfigManager::instance().display_name(action_)));
    if (current_label_)
        current_label_->setText(tr("Current: %1").arg(current_.toString(QKeySequence::NativeText)));
    if (scope_label_)
        scope_label_->setText(kbd::scope_text(kbd::scope_of(action_)));
    if (hint_label_)
        hint_label_->setText(tr("Press new key combination..."));
    if (apply_btn_)
        apply_btn_->setText(tr("Apply"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("Cancel"));
    // captured_label_ / conflict_label_ hold dynamically-built text reflecting
    // the last key press; left as-is (a modal capture dialog cannot outlive a
    // language switch in practice).
}

void KeyCaptureDialog::keyPressEvent(QKeyEvent* event) {
    // Ignore lone modifier keys
    const int key = event->key();
    if (key == 0 || key == Qt::Key_unknown || key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt || key == Qt::Key_AltGr || key == Qt::Key_Meta)
        return;

    // Drop the keypad flag: Qt's shortcut map retries without it, so "Ctrl+1"
    // fires from either row of digits, while a captured "Ctrl+Num+1" would only
    // ever fire from the numpad.
    captured_ = QKeySequence(QKeyCombination(event->modifiers() & ~Qt::KeypadModifier, Qt::Key(key)));
    captured_label_->setText(captured_.toString(QKeySequence::NativeText));

    QString problem;
    if (captured_ == current_) {
        problem = tr("That is already the current binding.");
    } else if (kbd::scope_of(action_) == kbd::Scope::Window && kbd::is_bare_editing_key(captured_)) {
        problem = tr("A global shortcut needs Ctrl or Alt with this key — on its own it would stop working in every "
                     "text field and dialog.");
    } else {
        const QString other = kbd::collision_for(action_, captured_);
        // Two bindings on one key in the same window are ambiguous to Qt and then
        // NEITHER fires — so a conflict blocks Apply instead of just warning.
        if (!other.isEmpty())
            problem = tr("Already used by \"%1\". Choose another combination, or rebind \"%1\" first.").arg(other);
    }
    conflict_label_->setText(problem);
    conflict_label_->setVisible(!problem.isEmpty());
    apply_btn_->setEnabled(problem.isEmpty());
}

// ── KeybindingsSection ────────────────────────────────────────────────────────

KeybindingsSection::KeybindingsSection(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });

    // Rebuild rows whenever any key changes (live update). "Reset All" emits one
    // key_changed per action (~40); coalesce them into a single rebuild instead
    // of tearing down and recreating every row each time.
    connect(&KeyConfigManager::instance(), &KeyConfigManager::key_changed, this, [this](KeyAction, QKeySequence) {
        if (rebuild_pending_)
            return;
        rebuild_pending_ = true;
        QTimer::singleShot(0, this, [this]() {
            rebuild_pending_ = false;
            rebuild_rows();
        });
    });
}

void KeybindingsSection::apply_theme() {
    using namespace ui::colors;
    setStyleSheet(
        QString("QLabel{background:transparent;}"
                "QLineEdit#kbSearch{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                "QLineEdit#kbSearch:focus{border:1px solid %4;}"
                "QLabel#kbHint{color:%5;}"
                "QScrollArea#kbScroll{border:none;background:transparent;}"
                "QWidget#kbContainer{background:transparent;}"
                "QLabel#kbGroupTitle{color:%4;font-weight:bold;letter-spacing:0.5px;padding:4px 0 0 0;}"
                "QLabel#kbGroupNote{color:%6;}"
                "QFrame#kbSep{background:%7;border:none;}"
                "QWidget#kbRow{background:transparent;}"
                "QWidget#kbRow:hover{background:%8;}")
            .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED(), AMBER(), TEXT_SECONDARY(), TEXT_TERTIARY(), BORDER_DIM(),
                 BG_HOVER()) +
        QString("QLabel#kbActionName{color:%1;}"
                "QLabel#kbKey{color:%2;}"
                "QLabel#kbKey[custom=\"true\"]{color:%3;font-weight:700;}"
                "QLabel#kbKey[conflict=\"true\"]{color:%4;font-weight:700;}"
                "QLabel#kbConflict{color:%4;font-weight:700;}"
                "QPushButton#kbResetBtn{background:%5;color:%2;border:1px solid %6;}"
                "QPushButton#kbResetBtn:hover{background:%7;}"
                "QPushButton#kbResetBtn:disabled{background:transparent;color:%8;border-color:%6;}"
                "QPushButton#kbResetAllBtn{background:%5;color:%1;border:1px solid %9;padding:0 12px;"
                "min-height:32px;}"
                "QPushButton#kbResetAllBtn:hover{background:%7;}")
            .arg(TEXT_PRIMARY(), TEXT_SECONDARY(), AMBER(), NEGATIVE(), BG_RAISED(), BORDER_DIM(), BG_HOVER(),
                 TEXT_DIM(), BORDER_MED()) +
        QString("QDialog#kbCaptureDlg{background:%1;}"
                "QLabel#kbCaptureCurrent{color:%2;}"
                "QLabel#kbCaptureScope{color:%3;}"
                "QLabel#kbCaptureHint{color:%4;font-weight:bold;}"
                "QLabel#kbCaptured{color:%5;font-size:16px;font-weight:700;}"
                "QLabel#kbCaptureProblem{color:%6;}")
            .arg(BG_SURFACE(), TEXT_SECONDARY(), TEXT_TERTIARY(), TEXT_PRIMARY(), AMBER(), NEGATIVE()));
}

void KeybindingsSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    hint_label_ = new QLabel(tr("Click a row to rebind it. Changes apply immediately in every window and are "
                                "saved — no restart needed. Amber keys are your custom bindings."));
    hint_label_->setObjectName(QStringLiteral("kbHint"));
    hint_label_->setWordWrap(true);

    // Search bar
    search_input_ = new QLineEdit;
    search_input_->setObjectName(QStringLiteral("kbSearch"));
    search_input_->setPlaceholderText(tr("Search actions or keys (e.g. refresh, Ctrl+K)..."));
    search_input_->setAccessibleName(tr("Search keyboard actions"));
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::textChanged, this, [this](const QString&) { rebuild_rows(); });

    // Scrollable groups area
    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("kbScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget;
    container->setObjectName(QStringLiteral("kbContainer"));
    groups_layout_ = new QVBoxLayout(container);
    groups_layout_->setContentsMargins(0, 0, 0, 0);
    groups_layout_->setSpacing(16);
    scroll->setWidget(container);

    rebuild_rows();

    // Reset All button
    reset_all_btn_ = new QPushButton(tr("Reset All to Defaults"));
    reset_all_btn_->setObjectName(QStringLiteral("kbResetAllBtn"));
    reset_all_btn_->setAccessibleName(tr("Reset all keybindings to defaults"));
    connect(reset_all_btn_, &QPushButton::clicked, this, [this]() {
        // Wipes every custom binding the user has made, with no undo — the one
        // destructive action on this page and it had no confirmation.
        const auto reply = QMessageBox::question(
            this, tr("Reset Keybindings"),
            tr("Reset every keyboard shortcut to its default?\n\nAll of your custom bindings will be lost."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        KeyConfigManager::instance().reset_all();
    });

    root->addWidget(hint_label_);
    root->addWidget(search_input_);
    root->addWidget(scroll, 1);
    root->addWidget(reset_all_btn_);
}

void KeybindingsSection::rebuild_rows() {
    // Clear existing group widgets
    while (QLayoutItem* item = groups_layout_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QString filter = search_input_ ? search_input_->text().trimmed() : QString{};
    auto& km = KeyConfigManager::instance();

    // Collect actions by group, preserving order. The filter matches the action
    // name, its group, and the bound key ("ctrl+k", "F5").
    QMap<QString, QList<KeyAction>> groups;
    for (KeyAction a : km.all_actions()) {
        const QString group = km.group_name(a);
        if (!filter.isEmpty()) {
            const QKeySequence seq = km.key(a);
            const bool hit = km.display_name(a).contains(filter, Qt::CaseInsensitive) ||
                             kbd::group_title(group).contains(filter, Qt::CaseInsensitive) ||
                             seq.toString(QKeySequence::NativeText).contains(filter, Qt::CaseInsensitive) ||
                             seq.toString(QKeySequence::PortableText).contains(filter, Qt::CaseInsensitive);
            if (!hit)
                continue;
        }
        groups[group].append(a);
    }

    // Known groups first, in a fixed order; then anything else KeyConfigManager
    // reports. The old hard-coded list omitted "Windows" (focus / cycle / move-to-
    // monitor shortcuts), so those ~25 actions could never be rebound here.
    QStringList group_order = {"Global", "Navigation", "News", "Code Editor", "Windows"};
    const QStringList extra_groups = groups.keys();
    for (const QString& g : extra_groups) {
        if (!group_order.contains(g))
            group_order.append(g);
    }
    for (const QString& group : std::as_const(group_order)) {
        if (!groups.contains(group))
            continue;
        groups_layout_->addWidget(build_group(group, groups[group]));
    }
    if (groups.isEmpty() && !filter.isEmpty()) {
        auto* none = new QLabel(tr("No action or key matches \"%1\".").arg(filter));
        none->setObjectName(QStringLiteral("kbGroupNote"));
        groups_layout_->addWidget(none);
    }
    groups_layout_->addStretch();
}

QWidget* KeybindingsSection::build_group(const QString& group_name, const QList<KeyAction>& actions) {
    auto* group_widget = new QWidget;
    group_widget->setObjectName(QStringLiteral("kbContainer"));
    auto* vbox = new QVBoxLayout(group_widget);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(2);

    auto* title = new QLabel(kbd::group_title(group_name));
    title->setObjectName(QStringLiteral("kbGroupTitle"));
    vbox->addWidget(title);
    if (!actions.isEmpty()) {
        auto* note = new QLabel(kbd::scope_text(kbd::scope_of(actions.first())));
        note->setObjectName(QStringLiteral("kbGroupNote"));
        vbox->addWidget(note);
    }

    auto* sep = new QFrame;
    sep->setObjectName(QStringLiteral("kbSep"));
    sep->setFixedHeight(1);
    vbox->addWidget(sep);

    auto& km = KeyConfigManager::instance();
    for (KeyAction a : actions) {
        auto* row = new QWidget;
        row->setObjectName(QStringLiteral("kbRow"));
        row->setAttribute(Qt::WA_Hover); // :hover on a plain QWidget needs hover events
        row->setProperty("key_action_set", true);
        row->setProperty("key_action", static_cast<int>(a));
        row->setCursor(Qt::PointingHandCursor);
        row->setToolTip(tr("Click to rebind"));
        row->installEventFilter(this);

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(4, 4, 4, 4);

        auto* name_lbl = new QLabel(km.display_name(a));
        name_lbl->setObjectName(QStringLiteral("kbActionName"));

        const QKeySequence seq = km.key(a);
        const bool custom = seq != km.default_key(a);
        const QString clash = kbd::collision_for(a, seq);

        auto* key_lbl = new QLabel(seq.isEmpty() ? tr("(unbound)") : seq.toString(QKeySequence::NativeText));
        key_lbl->setObjectName(QStringLiteral("kbKey"));
        key_lbl->setMinimumWidth(120);
        key_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        kbd::set_flag(key_lbl, "custom", custom);
        kbd::set_flag(key_lbl, "conflict", !clash.isEmpty());
        if (custom)
            key_lbl->setToolTip(tr("Default: %1").arg(km.default_key(a).toString(QKeySequence::NativeText)));

        hl->addWidget(name_lbl, 1);
        if (!clash.isEmpty()) {
            // Left over from an older build that allowed it: on a shared key Qt
            // fires neither binding, so say so instead of failing silently.
            auto* warn = new QLabel(tr("CONFLICT"));
            warn->setObjectName(QStringLiteral("kbConflict"));
            warn->setToolTip(tr("Also bound to \"%1\" — neither fires until one is changed.").arg(clash));
            hl->addWidget(warn);
        }
        hl->addWidget(key_lbl);

        auto* reset_btn = new QPushButton(tr("Reset"));
        reset_btn->setObjectName(QStringLiteral("kbResetBtn"));
        reset_btn->setFixedSize(56, 24);
        reset_btn->setEnabled(custom);
        reset_btn->setToolTip(custom ? tr("Restore %1").arg(km.default_key(a).toString(QKeySequence::NativeText))
                                     : tr("Already the default"));
        connect(reset_btn, &QPushButton::clicked, this, [a]() { KeyConfigManager::instance().reset_key(a); });
        hl->addWidget(reset_btn);

        vbox->addWidget(row);
    }

    return group_widget;
}

void KeybindingsSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void KeybindingsSection::retranslateUi() {
    if (hint_label_)
        hint_label_->setText(tr("Click a row to rebind it. Changes apply immediately in every window and are "
                                "saved — no restart needed. Amber keys are your custom bindings."));
    if (search_input_)
        search_input_->setPlaceholderText(tr("Search actions or keys (e.g. refresh, Ctrl+K)..."));
    if (reset_all_btn_)
        reset_all_btn_->setText(tr("Reset All to Defaults"));
    // Per-row "Reset" buttons live in dynamically-built groups; rebuild_rows()
    // recreates them (and re-reads action display names from KeyConfigManager).
    rebuild_rows();
}

void KeybindingsSection::set_search(const QString& text) {
    if (search_input_)
        search_input_->setText(text); // textChanged → rebuild_rows()
}

void KeybindingsSection::open_capture(KeyAction a) {
    auto& km = KeyConfigManager::instance();
    KeyCaptureDialog dlg(a, km.key(a), this);
    if (dlg.exec() == QDialog::Accepted && !dlg.captured().isEmpty())
        km.set_key(a, dlg.captured());
}

bool KeybindingsSection::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w->property("key_action_set").toBool()) {
            const auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // Defer: the press is still being delivered to the row this
                // dialog's result can make rebuild_rows() delete.
                const KeyAction a = static_cast<KeyAction>(w->property("key_action").toInt());
                QTimer::singleShot(0, this, [this, a]() { open_capture(a); });
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace fincept::screens
