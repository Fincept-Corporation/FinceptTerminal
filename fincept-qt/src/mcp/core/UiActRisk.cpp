// UiActRisk.cpp — ui_act: is this step something the user must approve?
//
// THE ORDER-PATH SAFETY OF THE GENERIC UI TOOLS LIVES HERE (with UiRisk.cpp for
// the word lists and markers, and UiAct.cpp's run for the batch rule). What it
// guarantees, for every way an agent can reach a control:
//   * a click / double click / check / select / Enter / Space / mnemonic /
//     shortcut / a click at x/y / a web control / a table-row button / a cell
//     edit is judged as what it would COMMIT — by the control's own words (text,
//     tooltip, accessible name AND accessible description: the crypto ticket's
//     submit button describes the exact order there), the dialog it sits in, the
//     table row it belongs to, and the fincept_agent_risk marker a screen sets;
//   * an order is never auto-confirmed: an approval carries over to the app's
//     own confirmation dialog only for non-money risks (carry_covers), so the
//     app's prepare → confirm order flow (the crypto ticket's confirm_hash
//     check in CryptoTradingScreen_Handlers.cpp) always shows the user the
//     prepared order before the agent can confirm it;
//   * a PAPER workflow run is refused any money step outright (UiAct.cpp);
//   * one ui_act call can hold at most one money step (UiAct.cpp): an agent
//     cannot prepare and confirm an order in a single batch.

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiActInternal.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTabBar>

#include <algorithm>

namespace fincept::mcp::core::ui::act {

QString words_of(QWidget* w, QAction* a) {
    QStringList parts;
    if (a) {
        parts << a->text() << a->toolTip() << a->objectName() << humanize_object_name(a->objectName());
    }
    if (w) {
        parts << widget_text(w) << w->toolTip() << w->accessibleName() << w->accessibleDescription()
              << humanize_object_name(w->objectName()) << w->objectName();
    }
    return parts.join(QLatin1Char(' ')).remove(QLatin1Char('&')).toLower();
}

QString dialog_context(QWidget* w) {
    QWidget* top = w ? w->window() : nullptr;
    if (!top || !(qobject_cast<QDialog*>(top) || top->windowType() == Qt::Dialog))
        return {};
    QStringList parts{top->windowTitle(), humanize_object_name(top->objectName())};
    if (auto* mb = qobject_cast<QMessageBox*>(top))
        parts << plain_text(mb->text()) << plain_text(mb->informativeText());
    else {
        int n = 0;
        for (QLabel* l : top->findChildren<QLabel*>()) {
            if (!l->isVisible())
                continue;
            parts << plain_text(l->text());
            if (++n >= 6)
                break;
        }
    }
    return parts.join(QLatin1Char(' ')).toLower();
}

QString trading_mode_words(QWidget* w) {
    // The screen's own mode indicator: a visible label / button whose whole text
    // is "LIVE" or "PAPER", in the control's panel or dialog — for a dialog, also
    // the panel that opened it. Both seen → both returned (the mode is unclear).
    QStringList seen;
    auto scan = [&seen](QWidget* scope) {
        if (!scope)
            return;
        int budget = 3000;
        for (QWidget* c : scope->findChildren<QWidget*>()) {
            if (--budget < 0)
                break;
            if (!c->isVisible())
                continue;
            QString t;
            if (auto* l = qobject_cast<QLabel*>(c))
                t = l->text();
            else if (auto* b = qobject_cast<QAbstractButton*>(c))
                t = b->text();
            else
                continue;
            t = t.remove(QLatin1Char('&')).trimmed();
            if (t == QLatin1String("LIVE") || t == QLatin1String("PAPER"))
                seen << t;
        }
    };
    QWidget* scope = location_scope(w);
    scan(scope);
    if (seen.isEmpty() && scope && scope->isWindow() && scope->parentWidget())
        scan(location_scope(scope->parentWidget()));
    seen.removeDuplicates();
    return seen.join(QLatin1Char(' '));
}

/// What a key sequence would fire in the window around `w` (the same search
/// trigger_shortcut() does): its words and its marker — a QAction's own, or a
/// QShortcut's own / its parent action's or button's. False when nothing owns it.
bool shortcut_words(QWidget* w, const QKeySequence& seq, QString* words, Risk* marked) {
    QWidget* top = w ? w->window() : nullptr;
    if (!top)
        return false;
    QList<QAction*> actions = top->actions();
    actions += top->findChildren<QAction*>();
    for (QAction* a : actions) {
        if (!a->isEnabled())
            continue;
        for (const QKeySequence& k : a->shortcuts()) {
            if (k == seq) {
                *words = QStringList{a->text().remove(QLatin1Char('&')), a->toolTip(), a->statusTip(), a->objectName()}
                             .join(QLatin1Char(' '))
                             .toLower();
                *marked = marker_risk(a);
                return true;
            }
        }
    }
    for (QShortcut* sc : top->findChildren<QShortcut*>()) {
        if (!sc->isEnabled())
            continue;
        for (const QKeySequence& k : sc->keys()) {
            if (k == seq) {
                QStringList parts{sc->objectName(), sc->whatsThis()};
                if (QWidget* pw = qobject_cast<QWidget*>(sc->parent()))
                    parts << pw->accessibleName() << pw->objectName();
                *words = parts.join(QLatin1Char(' ')).toLower();
                *marked = stronger(own_marker_risk(sc), own_marker_risk(sc->parent()));
                if (!marked->risky)
                    *marked = marker_risk(sc->parent());
                return true;
            }
        }
    }
    return false;
}

/// The visible, enabled button in `top` whose own shortcut is exactly `seq`
/// (any modifiers: the crypto ticket's Ctrl+Return is its submit button's).
QAbstractButton* shortcut_button(QWidget* top, const QKeySequence& seq) {
    if (!top || seq.isEmpty())
        return nullptr;
    for (QAbstractButton* b : top->findChildren<QAbstractButton*>()) {
        if (b->isVisible() && b->isEnabled() && !is_hidden_from_agents(b) && !b->shortcut().isEmpty() &&
            b->shortcut()[0] == seq[0])
            return b;
    }
    return nullptr;
}

/// The visible, enabled button in `top` whose mnemonic is `key` ("&Yes" → Y):
/// a plain letter answers a QMessageBox that way, Alt+letter any dialog.
QAbstractButton* mnemonic_button(QWidget* top, int key) {
    if (!top || key < Qt::Key_0 || key > Qt::Key_Z)
        return nullptr;
    for (QAbstractButton* b : top->findChildren<QAbstractButton*>()) {
        if (!b->isVisible() || !b->isEnabled() || is_hidden_from_agents(b))
            continue;
        const QKeySequence sc = b->shortcut();
        if (!sc.isEmpty() && sc[0].key() == key)
            return b;
        const QString text = b->text();
        const qsizetype amp = text.indexOf(QLatin1Char('&'));
        if (amp >= 0 && amp + 1 < text.size() && text.at(amp + 1) != QLatin1Char('&') &&
            text.at(amp + 1).toUpper().unicode() == key)
            return b;
    }
    return nullptr;
}

/// The button Enter commits in window `top`: its default button (any window —
/// screens that handle Enter themselves mark one default), else, in a dialog,
/// an auto-default one.
QPushButton* default_button(QWidget* top) {
    if (!top)
        return nullptr;
    const bool dialog = qobject_cast<QDialog*>(top) != nullptr;
    QPushButton* fallback = nullptr;
    for (QPushButton* b : top->findChildren<QPushButton*>()) {
        if (!b->isVisible() || !b->isEnabled())
            continue;
        if (b->isDefault())
            return b;
        if (dialog && !fallback && b->autoDefault())
            fallback = b;
    }
    return fallback;
}

/// Text of an item-view row target, to notice a live table re-sorting under a
/// row ref while an approval was pending.
QString row_fingerprint(const Target& t) {
    auto* view = qobject_cast<QAbstractItemView*>(t.widget.data());
    if (!view || t.row < 0 || !view->model())
        return {};
    QStringList cells;
    for (int c : visible_columns(view)) {
        const QModelIndex i = view_index(view, t.row, c);
        if (i.isValid())
            cells << i.data(Qt::DisplayRole).toString();
        if (cells.size() >= 12)
            break;
    }
    return cells.join(QLatin1Char('|'));
}

Risk assess(const Step& s, const Target& t);

/// What a key press would commit: the shortcut it fires, the button Enter /
/// Space / Select presses (with ANY modifier — QPushButton ignores them), or the
/// dialog button whose mnemonic it is ("y" → &Yes, Alt+S → &Submit).
Risk assess_key(const KeyChord& k, const Step& s, QWidget* w) {
    QWidget* top = w ? w->window() : nullptr;
    auto judge_button = [&s](QAbstractButton* b) -> Risk {
        Target bt;
        bt.widget = b;
        bt.name = b->text().remove(QLatin1Char('&')).simplified();
        Step click = s;
        click.action = QStringLiteral("click");
        return assess(click, bt);
    };
    // Keys a widget (or a pane around the focused widget) handles itself, as its
    // screen declared them: "Ctrl+Return=code;Delete=delete".
    const Risk declared = keys_risk(w, k.seq);
    // A button's own shortcut, whatever its modifiers.
    if (QAbstractButton* b = shortcut_button(top, k.seq))
        return stronger(judge_button(b), declared);
    const bool command_mods = k.mods.testAnyFlags(Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (command_mods || (k.key >= Qt::Key_F1 && k.key <= Qt::Key_F35)) {
        QString words;
        Risk marked;
        if (shortcut_words(w, k.seq, &words, &marked)) {
            const Risk r = stronger(classify_words(words), marked);
            if (r.risky)
                return stronger(r, declared);
        }
        if (k.mods.testFlag(Qt::AltModifier) && top) {
            if (QAbstractButton* b = mnemonic_button(top, k.key))
                return stronger(judge_button(b), declared);
        }
    }
    if (k.key == Qt::Key_Return || k.key == Qt::Key_Enter || k.key == Qt::Key_Space || k.key == Qt::Key_Select) {
        const bool enter = k.key == Qt::Key_Return || k.key == Qt::Key_Enter;
        if (auto* button = qobject_cast<QAbstractButton*>(w))
            return stronger(judge_button(button), declared);
        Risk r = declared;
        // Enter activates a table's current cell.
        if (auto* view = qobject_cast<QAbstractItemView*>(w))
            r = stronger(r, cell_risk(view, view->currentIndex()));
        if (!enter)
            return r;
        // Submitting a field: its own marker ("Enter here sends"), a marked ticket
        // around it that says inherit, and the window's default button.
        r = stronger(r, marker_risk(w));
        if (QPushButton* def = default_button(top))
            r = stronger(r, judge_button(def));
        return r;
    }
    if (!command_mods && top && qobject_cast<QDialog*>(top)) {
        if (QAbstractButton* b = mnemonic_button(top, k.key))
            return stronger(judge_button(b), declared);
    }
    return declared;
}

/// A step on a web page, judged by what the page script found there: the
/// element's own words (its text, title, id, link, click handler) and, for an
/// Enter that submits a form, the form's submit buttons.
Risk assess_web(const Step& requested, const Target& t) {
    const QJsonObject& in = t.web_info;
    const QString own = (in.value(QStringLiteral("words")).toString() + QLatin1Char(' ') + t.name).toLower();
    const QString form = in.value(QStringLiteral("form")).toString().toLower();
    QString act = requested.action;
    if (act == QLatin1String("expand") || act == QLatin1String("collapse") || act == QLatin1String("check") ||
        act == QLatin1String("uncheck"))
        act = QStringLiteral("click");
    else if (act == QLatin1String("set_value"))
        act = in.value(QStringLiteral("tag")).toString() == QLatin1String("select") ? QStringLiteral("select")
                                                                                    : QStringLiteral("type");
    if (act == QLatin1String("type"))
        return requested.submit ? classify_words(own + QLatin1Char(' ') + form) : Risk{};
    if (act == QLatin1String("press")) {
        const auto chord = parse_key(requested.text);
        if (!chord || !(chord->key == Qt::Key_Return || chord->key == Qt::Key_Enter || chord->key == Qt::Key_Space))
            return {};
        QString words = own;
        if (in.value(QStringLiteral("editable")).toBool())
            words += QLatin1Char(' ') + form; // Enter in a field submits its form
        return classify_words(words);
    }
    if (act != QLatin1String("click") && act != QLatin1String("double_click") && act != QLatin1String("select"))
        return {};
    if (is_negative_choice(t.name))
        return {};
    QString words = own;
    if (act == QLatin1String("select"))
        words += QLatin1Char(' ') + requested.text.toLower() + QLatin1Char(' ') +
                 in.value(QStringLiteral("picked")).toString().toLower();
    Risk r = classify_words(words);
    if (!r.risky) {
        const QString ctx = dialog_context(t.web_view);
        if (!ctx.isEmpty())
            r = classify_words(ctx);
    }
    return r;
}

namespace {

/// The words around a control placed in a table row: the row's cells, the
/// column header and the table's own name ("open orders").
QString row_context(QAbstractItemView* view, const QModelIndex& cell) {
    QStringList parts;
    if (view->model())
        parts << view->model()->headerData(cell.column(), Qt::Horizontal).toString();
    parts << view->accessibleName() << humanize_object_name(view->objectName());
    if (auto n = describe_widget(view, Detail::Brief))
        parts << n->name;
    const int row = row_of_index(view, cell);
    if (row >= 0)
        parts << view_row_text(view, row, visible_columns(view), 40);
    return parts.join(QLatin1Char(' ')).toLower();
}

/// A save dialog's commit writes a file wherever its path points.
Risk file_save_risk(QWidget* w, const Step& s) {
    auto* fd = w ? qobject_cast<QFileDialog*>(w->window()) : nullptr;
    if (!fd || fd->acceptMode() != QFileDialog::AcceptSave)
        return {};
    bool commits = false;
    if (s.action == QLatin1String("press") || (s.action == QLatin1String("type") && s.submit)) {
        const auto chord = parse_key(s.action == QLatin1String("press") ? s.text : QStringLiteral("Enter"));
        commits = chord && (chord->key == Qt::Key_Return || chord->key == Qt::Key_Enter);
    } else if (auto* b = qobject_cast<QAbstractButton*>(w)) {
        commits = (s.action == QLatin1String("click") || s.action == QLatin1String("double_click")) &&
                  !is_negative_choice(strip_mnemonic(b->text()));
    }
    if (!commits)
        return {};
    return {true, false,
            QStringLiteral("writes a file in %1").arg(QDir::toNativeSeparators(fd->directory().absolutePath())),
            QStringLiteral("file")};
}

/// A button that accepts its dialog: an accept / yes / apply role in its button
/// box or message box, or a dialog's default button.
bool is_accept_button(QAbstractButton* b) {
    for (QWidget* p = b->parentWidget(); p; p = p->parentWidget()) {
        if (auto* box = qobject_cast<QDialogButtonBox*>(p)) {
            const auto role = box->buttonRole(b);
            return role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole ||
                   role == QDialogButtonBox::ApplyRole;
        }
        if (auto* mb = qobject_cast<QMessageBox*>(p)) {
            const auto role = mb->buttonRole(b);
            return role == QMessageBox::AcceptRole || role == QMessageBox::YesRole || role == QMessageBox::ApplyRole;
        }
        if (p->isWindow())
            break;
    }
    auto* pb = qobject_cast<QPushButton*>(b);
    return pb && pb->isDefault() && qobject_cast<QDialog*>(b->window());
}

/// Dialogs a risky step opened, with that step's risk: their accept-role buttons
/// are judged as the opener ("Yes" under a marked "Delete board" is a delete).
struct OpenedDialog {
    QPointer<QWidget> dialog;
    Risk risk;
};

QList<OpenedDialog>& opened_dialogs() {
    static QList<OpenedDialog> list;
    return list;
}

} // namespace

void adopt_opened_dialogs(const Risk& opener, const QList<QPointer<QWidget>>& before) {
    QList<OpenedDialog>& list = opened_dialogs();
    list.erase(std::remove_if(list.begin(), list.end(), [](const OpenedDialog& o) { return !o.dialog; }), list.end());
    if (!opener.risky)
        return;
    for (QWidget* d : open_dialogs()) {
        bool existed = false;
        for (const QPointer<QWidget>& b : before)
            existed = existed || b == d;
        if (existed)
            continue;
        list.append({QPointer<QWidget>(d), opener});
        while (list.size() > 16)
            list.removeFirst();
    }
}

Risk opened_dialog_risk(QWidget* w) {
    auto* b = qobject_cast<QAbstractButton*>(w);
    if (!b || !is_accept_button(b))
        return {};
    QWidget* top = b->window();
    for (const OpenedDialog& o : opened_dialogs()) {
        if (o.dialog && o.dialog == top)
            return o.risk;
    }
    return {};
}

Risk assess(const Step& requested, const Target& t) {
    if (t.web_view)
        return assess_web(requested, t);
    static const QStringList kCommitting = {
        QStringLiteral("click"), QStringLiteral("double_click"), QStringLiteral("press"), QStringLiteral("select"),
        QStringLiteral("check"), QStringLiteral("uncheck"),      QStringLiteral("type"),
    };
    QWidget* w = host_widget(t);
    // A save dialog's "Save" (or Enter in it) writes a file.
    if (const Risk f = file_save_risk(w, requested); f.risky)
        return f;
    // A table cell's value edited in place: an edit in an orders / positions
    // table can modify an order, so the row decides — and the cell's own kind
    // (kAgentRiskRole, the view's fincept_agent_risk_columns).
    if (auto* view = qobject_cast<QAbstractItemView*>(w);
        view && t.row >= 0 &&
        (requested.action == QLatin1String("type") || requested.action == QLatin1String("set_value") ||
         requested.action == QLatin1String("clear"))) {
        const int column = !requested.column.isEmpty() ? resolve_column(view, requested.column) : t.column;
        const QModelIndex cell = view_index(view, t.row, column >= 0 ? column : visible_columns(view).value(0, 0));
        Risk r = cell.isValid() ? classify_row_action(QStringLiteral("edit"), row_context(view, cell)) : Risk{};
        return stronger(stronger(r, cell_risk(view, cell)), marker_risk(view));
    }
    // Judge what prepare() will really do, not the verb that was asked for:
    // expand / collapse click a section header or item, hover on a menu item
    // triggers it, set_value becomes a selection or typing (submit kept), and
    // typing into a non-editable combo selects an option.
    Step s = requested;
    if (s.action == QLatin1String("expand") || s.action == QLatin1String("collapse")) {
        s.action = QStringLiteral("click");
    } else if (s.action == QLatin1String("hover") && t.action) {
        s.action = QStringLiteral("click");
    } else if (s.action == QLatin1String("set_value")) {
        s.text = s.value.isString() ? s.value.toString() : QString::number(s.value.toDouble(), 'g', 15);
        s.action = qobject_cast<QComboBox*>(w) ? QStringLiteral("select") : QStringLiteral("type");
    }
    if (s.action == QLatin1String("type")) {
        if (auto* cb = qobject_cast<QComboBox*>(w); cb && !cb->isEditable())
            s.action = QStringLiteral("select");
    }
    if (!kCommitting.contains(s.action))
        return {};
    // Typing only changes a text field; prepare() refuses key-by-key typing into
    // buttons, dialogs and menus, where a letter or a space would commit things.
    if (s.action == QLatin1String("type") && !s.submit)
        return {};
    if (s.action == QLatin1String("press") || s.action == QLatin1String("type")) {
        KeyChord k;
        if (s.action == QLatin1String("type")) {
            k.key = Qt::Key_Return;
        } else {
            const auto chord = parse_key(s.text);
            if (!chord)
                return {}; // prepare() refuses it
            k = *chord;
        }
        return assess_key(k, s, w);
    }
    // The screen's own marker on the control (or a pane around it that says
    // inherit) always counts; so does the step that opened this dialog, for its
    // accept-role buttons.
    Risk marked = marker_risk(t.action ? static_cast<QObject*>(t.action.data()) : static_cast<QObject*>(w));
    if (!t.action)
        marked = stronger(marked, opened_dialog_risk(w));
    // A button inside a table row acts on that row: "Cancel" on an order row
    // cancels the order — it is not a dialog's "Cancel".
    QModelIndex cell;
    if (QAbstractItemView* owner = w && !t.action ? cell_widget_owner(w, &cell) : nullptr) {
        const Risk r = classify_row_action(words_of(w, nullptr), row_context(owner, cell));
        return stronger(r, stronger(marked, marker_risk(owner)));
    }
    if (is_negative_choice(t.name))
        return marked;
    QString words = words_of(w, t.action);
    if (auto* bar = qobject_cast<QTabBar*>(w); bar && t.sub >= 0)
        words = bar->tabText(t.sub).toLower(); // a tab only switches the view
    if (s.action == QLatin1String("select") || (t.row >= 0 && qobject_cast<QAbstractItemView*>(w)))
        words += QLatin1Char(' ') + s.text.toLower();
    if (auto* view = qobject_cast<QAbstractItemView*>(w); view && t.row >= 0) {
        // The row's own words ("RELIANCE · BUY · OPEN") and, for a cell click,
        // that cell and its header ("Cancel", "Action").
        const QList<int> cols = visible_columns(view);
        const QModelIndex first = view_index(view, t.row, cols.value(0, 0));
        const int column = !s.column.isEmpty() ? resolve_column(view, s.column) : t.column;
        if (first.isValid() && column >= 0) {
            const QModelIndex hit_cell = first.siblingAtColumn(column);
            words += QLatin1Char(' ') + hit_cell.data(Qt::DisplayRole).toString().toLower() + QLatin1Char(' ') +
                     view->model()->headerData(column, Qt::Horizontal).toString().toLower();
            if (QWidget* cw = view->indexWidget(hit_cell))
                words += QLatin1Char(' ') + cell_widget_text(cw, false).toLower();
            // A marked cell / column: a click on it, a double click, an activation.
            marked = stronger(marked, cell_risk(view, hit_cell));
        } else if (first.isValid() && s.action == QLatin1String("double_click")) {
            // A double click on a row activates its first cell.
            marked = stronger(marked, cell_risk(view, first));
        }
    }
    Risk r = classify_words(words);
    if (t.role == QLatin1String("tab"))
        return marked; // a tab only switches the view
    if (r.risky)
        return stronger(r, marked);
    // An affirmative button in a dialog inherits the dialog's meaning
    // ("Yes" in "Delete portfolio?", "OK" in "Confirm live order").
    const QString ctx = dialog_context(w);
    if (!ctx.isEmpty())
        r = classify_words(ctx);
    return stronger(r, marked);
}

/// After the user approves a risky step, the confirmation dialog that step
/// opens (the app's own "Delete?") is part of the same intent: one answer in
/// that dialog, by the same caller, within 45 s, needs no second approval.
/// Consumed on use. Never for money: an approved "BUY" click must not answer
/// the app's own order confirmation — that dialog is where the exact market,
/// size in exchange units and price are shown, so the user approves it again
/// with that text in front of them ("dialog says" in the request).
struct ApprovalCarry {
    QString origin;
    qint64 at_ms = 0;
    QList<QPointer<QWidget>> dialogs_before;
    bool armed = false;
    QString kind; // the kind of risk the user approved — a carry never crosses kinds
};

ApprovalCarry& carry() {
    static ApprovalCarry c;
    return c;
}

bool carry_covers(const QString& origin, QWidget* w, const Risk& risk) {
    ApprovalCarry& c = carry();
    if (!c.armed || c.origin != origin || QDateTime::currentMSecsSinceEpoch() - c.at_ms > kCarryOverMs || !w)
        return false;
    // An approved delete must not cover a "Confirm live order" that pops up next,
    // and nothing covers an order / money confirmation (see above).
    if (c.kind != risk.kind || is_order_kind(risk) || c.kind == QLatin1String("order") ||
        c.kind == QLatin1String("money"))
        return false;
    QWidget* top = w->window();
    if (!top || !(qobject_cast<QDialog*>(top) || top->windowType() == Qt::Dialog))
        return false;
    for (const QPointer<QWidget>& d : c.dialogs_before) {
        if (d == top)
            return false; // that dialog was already open when the user approved
    }
    c.armed = false;
    return true;
}

void arm_carry(const QString& origin, const Risk& approved) {
    ApprovalCarry& c = carry();
    c.origin = origin;
    c.kind = approved.kind;
    c.at_ms = QDateTime::currentMSecsSinceEpoch();
    c.dialogs_before.clear();
    for (QWidget* d : open_dialogs())
        c.dialogs_before << d;
    c.armed = true;
}

} // namespace fincept::mcp::core::ui::act
