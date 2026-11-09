// UiAct.cpp — ui_act: operate the terminal's controls like a person.
//
// One call runs up to ten steps in order and stops at the first failure. Each
// step resolves its element (ref, or the words on it), refuses what a person
// could not do (disabled / hidden / behind a modal dialog / a secret field),
// asks the user before anything risky (orders, money, deletes, spends, sign
// out), then performs the action ASYNCHRONOUSLY: the widget operation is posted
// to the event loop, so a click that opens a modal dialog (QDialog::exec) or a
// menu (QMenu::exec) never blocks the call — the settle watcher runs inside
// that nested loop, sees the dialog and reports it with its refs.
//
// The answer says what changed: dialogs or menus that opened or closed, the
// element's new value, row counts of tables in the same panel, new status
// texts, a different focused panel, and work still loading.

#include "app/WindowFrame.h"
#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"
#include "mcp/core/CoreApproval.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"
#include "mcp/server/CallApproval.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QSpinBox>
#include <QTabBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeView>
#include <QWheelEvent>

#include <DockWidget.h>
#include <algorithm>
#include <memory>

namespace fincept::mcp::core::ui {

namespace ui_act_detail {

constexpr int kMaxSteps = 10;
constexpr int kMinSettleMs = 150;
constexpr int kSettleMs = 400;
constexpr int kCarryOverMs = 45000;

struct Step {
    QString action;
    QString ref;
    QString name;
    QString role;
    QString target;
    QString text;
    QJsonValue value;
    bool append = false;
    bool submit = false;
    int amount = 0;
    bool has_xy = false;
    int x = 0;
    int y = 0;
    QString column; // row refs: the cell to act on (header words or a column number)
};

Step parse_step(const QJsonObject& o, const QString& default_target) {
    Step s;
    s.action = o.value(QStringLiteral("action")).toString().trimmed().toLower();
    s.ref = o.value(QStringLiteral("ref")).toString().trimmed();
    s.name = o.value(QStringLiteral("name")).toString().trimmed();
    s.role = o.value(QStringLiteral("role")).toString().trimmed().toLower();
    s.target = o.contains(QStringLiteral("target")) ? o.value(QStringLiteral("target")).toString() : default_target;
    s.text = o.value(QStringLiteral("text")).toString();
    s.value = o.value(QStringLiteral("value"));
    s.append = o.value(QStringLiteral("append")).toBool(false);
    s.submit = o.value(QStringLiteral("submit")).toBool(false);
    s.amount = clamp_int(o.value(QStringLiteral("amount")), 0, -100, 100);
    if (o.contains(QStringLiteral("x")) && o.contains(QStringLiteral("y"))) {
        s.has_xy = true;
        s.x = clamp_int(o.value(QStringLiteral("x")), 0, 0, 20000);
        s.y = clamp_int(o.value(QStringLiteral("y")), 0, 0, 20000);
    }
    const QJsonValue col = o.value(QStringLiteral("column"));
    s.column = col.isDouble() ? QString::number(col.toInt()) : col.toString().trimmed();
    return s;
}

const QStringList& known_actions() {
    static const QStringList k = {
        QStringLiteral("click"),     QStringLiteral("double_click"), QStringLiteral("right_click"),
        QStringLiteral("hover"),     QStringLiteral("type"),         QStringLiteral("clear"),
        QStringLiteral("select"),    QStringLiteral("check"),        QStringLiteral("uncheck"),
        QStringLiteral("set_value"), QStringLiteral("press"),        QStringLiteral("scroll"),
        QStringLiteral("focus"),     QStringLiteral("expand"),       QStringLiteral("collapse"),
        QStringLiteral("close"),
    };
    return k;
}

/// What a step acts on.
struct Target {
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    QPointer<ads::CDockWidget> dock;
    int sub = -1;
    int row = -1;
    int listed_row = -1;  // the row number the caller used, when the row has moved since it was listed
    int column = -1;      // a cell's model column (from `column`, or a click at x/y on a row)
    bool has_hit = false; // the step aims at a point: `hit_pos`, in the target widget's coordinates
    QPoint hit_pos;
    QString ref;
    QString role;
    QString name;
    // ── Inside a web page (then `widget` is the web view) ──
    QPointer<QWidget> web_view;
    int web = -1;           // control N of the page (a ":w" ref)
    QString web_key;        // that control as it was listed
    bool web_point = false; // a point in the page (x/y)
    bool web_focus = false; // keys for whatever has focus in the page
    bool web_located = false;
    QJsonObject web_info; // what the page script found there (role, name, states, words, …)
};

/// A column named by header words ("Price", exact first, then contained) or by
/// its number among the visible columns. -1 when there is no such column.
int resolve_column(QAbstractItemView* view, const QString& wanted) {
    if (!view || wanted.isEmpty())
        return -1;
    const QList<int> cols = visible_columns(view);
    bool is_num = false;
    const int n = wanted.toInt(&is_num);
    if (is_num)
        return n >= 0 && n < cols.size() ? cols[n] : -1;
    const QStringList headers = column_headers(view, cols);
    const QString w = wanted.toLower();
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].toLower() == w)
            return cols[i];
    }
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].toLower().contains(w))
            return cols[i];
    }
    return -1;
}

QString describe_target(const Target& t) {
    QString s = t.ref.isEmpty() ? t.role : t.ref + QLatin1Char(' ') + t.role;
    if (!t.name.isEmpty())
        s += QLatin1Char(' ') + common::quoted(common::clip(t.name, 60));
    return s;
}

QWidget* host_widget(const Target& t) {
    if (t.widget)
        return t.widget;
    if (t.dock)
        return t.dock;
    return nullptr;
}

/// Fill role/name for a target found by ref.
void classify_target(Target& t) {
    if (t.action) {
        t.role = QStringLiteral("menuitem");
        t.name = t.action->text().remove(QLatin1Char('&')).simplified();
        return;
    }
    if (t.dock && !t.widget) {
        t.role = QStringLiteral("panel");
        t.name = t.dock->windowTitle();
        return;
    }
    if (!t.widget)
        return;
    if (auto* bar = qobject_cast<QTabBar*>(t.widget.data()); bar && t.sub >= 0) {
        t.role = QStringLiteral("tab");
        t.name = bar->tabText(t.sub).remove(QLatin1Char('&')).simplified();
        return;
    }
    if (t.widget->isWindow() && !qobject_cast<QMenu*>(t.widget.data())) {
        t.role = QStringLiteral("dialog");
        t.name = window_title_of(t.widget);
        return;
    }
    if (qobject_cast<QMenu*>(t.widget.data())) {
        t.role = QStringLiteral("menu");
        return;
    }
    if (auto n = describe_widget(t.widget, Detail::Brief)) {
        t.role = n->role;
        t.name = n->name;
    } else {
        t.role = QStringLiteral("element");
        t.name = humanize_object_name(t.widget->objectName());
    }
    if (t.row >= 0)
        t.role += QStringLiteral(" row");
}

// ── Element resolution ──────────────────────────────────────────────────────

std::optional<Target> resolve_step_target(const Step& s, QString* error) {
    Target t;
    if (!s.ref.isEmpty()) {
        QString why;
        auto r = Refs::instance().resolve(s.ref, &why);
        if (!r) {
            *error =
                why.isEmpty()
                    ? QStringLiteral("%1 is unknown or gone (closed or rebuilt) — take a fresh ui_snapshot.").arg(s.ref)
                    : why;
            return std::nullopt;
        }
        t.widget = r->widget;
        t.action = r->action;
        t.dock = r->dock;
        t.sub = r->sub;
        t.row = r->row;
        t.listed_row = r->listed_row;
        t.ref = r->ref;
        if (r->web >= 0) {
            // A control on a web page: the page script finds it when the step runs.
            t.web_view = web_view_of(r->widget);
            t.widget = t.web_view;
            t.web = r->web;
            t.web_key = r->web_key;
            t.role = r->web_key.section(QLatin1Char('|'), 0, 0);
            t.name = r->web_key.section(QLatin1Char('|'), 1);
            return t;
        }
        if (r->dock)
            t.widget = nullptr; // a panel ref: the panel itself is focused / closed / scrolled
        classify_target(t);
        return t;
    }
    if (!s.name.isEmpty()) {
        QList<QWidget*> scopes;
        if (!s.target.trimmed().isEmpty()) {
            QString label;
            QWidget* scope = resolve_scope(s.target, &label, error);
            if (!scope)
                return std::nullopt;
            scopes << scope;
        } else {
            if (QWidget* modal = blocking_dialog())
                scopes << modal;
            for (QWidget* d : open_dialogs()) {
                if (!scopes.contains(d))
                    scopes << d;
            }
            if (WindowFrame* f = active_frame()) {
                if (ads::CDockWidget* dock = focused_dock(f))
                    scopes << dock;
                scopes << f;
            }
        }
        for (QWidget* scope : scopes) {
            const QList<NameMatch> matches = find_by_name(scope, s.name, s.role);
            if (matches.isEmpty())
                continue;
            const NameMatch& best = matches.first();
            if (matches.size() > 1 && matches[1].score == best.score && matches[1].widget != best.widget &&
                location_label(matches[1].widget) != location_label(best.widget)) {
                QStringList cands;
                for (int i = 0; i < matches.size() && i < 5; ++i) {
                    if (matches[i].score != best.score)
                        break;
                    cands << QStringLiteral("%1 %2 %3 in %4")
                                 .arg(matches[i].ref, matches[i].role, common::quoted(matches[i].name),
                                      location_label(matches[i].widget));
                }
                *error = QStringLiteral("\"%1\" matches several elements: %2. Use a ref or pass `target`.")
                             .arg(s.name, cands.join(QStringLiteral("; ")));
                return std::nullopt;
            }
            t.widget = best.widget;
            t.action = best.action;
            t.sub = best.sub;
            t.ref = best.ref;
            t.role = best.role;
            t.name = best.name;
            if (best.action && best.widget && qobject_cast<QMenuBar*>(best.widget.data()))
                t.widget = best.widget;
            return t;
        }
        *error = QStringLiteral("Nothing named \"%1\"%2 is showing. ui_snapshot lists what is there.")
                     .arg(s.name, s.role.isEmpty() ? QString() : QStringLiteral(" (role %1)").arg(s.role));
        return std::nullopt;
    }
    // No element: press / scroll / type act on the focused widget.
    if (s.action == QLatin1String("press") || s.action == QLatin1String("type") ||
        s.action == QLatin1String("scroll")) {
        QWidget* w = QApplication::focusWidget();
        if (!w) {
            // The terminal is not the foreground app (the agent's own terminal
            // is): there is no application focus widget, but each window still
            // remembers its focus child — the field the agent just typed into.
            if (QWidget* modal = blocking_dialog())
                w = modal->focusWidget();
            else if (WindowFrame* f = active_frame())
                w = f->focusWidget();
        }
        if (QWidget* popup = open_popup())
            w = popup;
        if (!w && s.action == QLatin1String("scroll")) {
            if (WindowFrame* f = active_frame())
                w = focused_dock(f);
        }
        if (!w) {
            if (QWidget* modal = blocking_dialog())
                w = modal;
            else if (WindowFrame* f = active_frame())
                w = f;
        }
        if (!w) {
            *error = QStringLiteral("Nothing has keyboard focus — pass a ref.");
            return std::nullopt;
        }
        t.widget = w;
        t.ref = QString();
        classify_target(t);
        // Focus inside a web page: the keys go to whatever the page has focused.
        if (QWidget* view = web_view_of(w); view && (w == view || view->isAncestorOf(w))) {
            t.widget = view;
            t.web_view = view;
            t.web_focus = true;
            t.ref = Refs::instance().ref_for(view, RefKind::Element);
            t.role = QStringLiteral("web page");
        }
        return t;
    }
    if (s.has_xy) {
        // A point in a panel / dialog / the window ("target"; default: the modal
        // dialog if one is open, else the active window) — anything on screen.
        QWidget* scope = nullptr;
        QString label;
        if (s.target.trimmed().isEmpty() && blocking_dialog())
            scope = blocking_dialog();
        else
            scope = resolve_scope(s.target, &label, error);
        if (!scope)
            return std::nullopt;
        if (auto* dock = qobject_cast<ads::CDockWidget*>(scope)) {
            t.dock = dock;
            t.ref = Refs::instance().ref_for(dock, RefKind::Panel);
        } else {
            t.widget = scope;
        }
        classify_target(t);
        return t;
    }
    *error = QStringLiteral("%1 needs `ref`, `name`, or a point (x, y with an optional target).").arg(s.action);
    return std::nullopt;
}

/// A step that names a point: aim it at what is actually under that point — the
/// deepest widget, the row of a table, the tab, the menu item — so the click
/// lands where a person's would AND is judged for risk as that element (a click
/// at x/y on a panel that hits "Place order" is a click on "Place order").
bool aim_at_point(Target& t, const Step& s, QString* error) {
    if (!s.has_xy || t.action || t.row >= 0 || t.web >= 0)
        return true; // a row / menu item / web control ref with x/y: x/y are ignored
    QWidget* host = t.widget ? t.widget.data() : (t.dock ? static_cast<QWidget*>(t.dock.data()) : nullptr);
    if (!host)
        return true;
    const QPoint pos(s.x, s.y);
    if (!host->rect().contains(pos)) {
        *error = QStringLiteral("(%1, %2) is outside %3 (%4×%5).")
                     .arg(s.x)
                     .arg(s.y)
                     .arg(describe_target(t))
                     .arg(host->width())
                     .arg(host->height());
        return false;
    }
    QWidget* hit = host->childAt(pos);
    if (!hit) {
        t.has_hit = true;
        t.hit_pos = pos;
        return true;
    }
    const QPoint local = hit->mapFrom(host, pos);
    Target aimed;
    aimed.has_hit = true;
    aimed.hit_pos = local;
    // A web page: the page script names what is under the point when the step runs.
    if (QWidget* view = web_view_of(hit); view && (hit == view || view->isAncestorOf(hit))) {
        aimed.widget = view;
        aimed.web_view = view;
        aimed.web_point = true;
        aimed.hit_pos = view->mapFrom(host, pos);
        // Keep the ref the caller used for the page (a chart's), else the view's own.
        aimed.ref = t.widget && web_view_of(t.widget) == view && !t.ref.isEmpty()
                        ? t.ref.section(QLatin1Char(':'), 0, 0)
                        : Refs::instance().ref_for(view, RefKind::Element);
        aimed.role = QStringLiteral("web page");
        t = aimed;
        return true;
    }
    // The viewport of a table / list / tree: the row (and cell) under the point.
    if (auto* view = qobject_cast<QAbstractItemView*>(hit->parentWidget()); view && view->viewport() == hit) {
        const QModelIndex idx = view->indexAt(local);
        aimed.widget = view;
        aimed.ref = Refs::instance().ref_for(view, RefKind::Element);
        if (idx.isValid()) {
            aimed.row = row_of_index(view, idx);
            aimed.column = idx.column();
            aimed.ref += QStringLiteral(":r%1").arg(aimed.row);
        }
        classify_target(aimed);
        t = aimed;
        return true;
    }
    if (auto* bar = qobject_cast<QTabBar*>(hit); bar && bar->tabAt(local) >= 0) {
        aimed.widget = bar;
        aimed.sub = bar->tabAt(local);
        aimed.ref = Refs::instance().ref_for(bar, RefKind::Element, aimed.sub);
        aimed.has_hit = false; // a tab is selected, not clicked at a pixel
        classify_target(aimed);
        t = aimed;
        return true;
    }
    if (auto* mb = qobject_cast<QMenuBar*>(hit)) {
        if (QAction* a = mb->actionAt(local)) {
            aimed.widget = mb;
            aimed.action = a;
            aimed.ref = Refs::instance().ref_for(a, RefKind::Element);
            aimed.has_hit = false;
            classify_target(aimed);
            t = aimed;
            return true;
        }
    }
    if (auto* menu = qobject_cast<QMenu*>(hit)) {
        if (QAction* a = menu->actionAt(local)) {
            aimed.widget = menu;
            aimed.action = a;
            aimed.ref = Refs::instance().ref_for(a, RefKind::Element);
            aimed.has_hit = false;
            classify_target(aimed);
            t = aimed;
            return true;
        }
    }
    // Labels and icons inside a button / clickable card belong to it.
    QWidget* target = hit;
    for (QWidget* p = hit; p && p != host; p = p->parentWidget()) {
        if (qobject_cast<QAbstractButton*>(p) || qobject_cast<QComboBox*>(p) || qobject_cast<QAbstractSpinBox*>(p) ||
            qobject_cast<QLineEdit*>(p) ||
            (p->testAttribute(Qt::WA_SetCursor) && p->cursor().shape() == Qt::PointingHandCursor)) {
            target = p;
            break;
        }
    }
    aimed.widget = target;
    aimed.hit_pos = target->mapFrom(host, pos);
    aimed.ref = Refs::instance().ref_for(target, RefKind::Element);
    classify_target(aimed);
    t = aimed;
    return true;
}

struct KeyChord {
    int key = 0;
    Qt::KeyboardModifiers mods;
    QString text;
    QKeySequence seq;
};

std::optional<KeyChord> parse_key(const QString& raw) {
    static const QHash<QString, QString> alias = {
        {QStringLiteral("enter"), QStringLiteral("Return")},
        {QStringLiteral("return"), QStringLiteral("Return")},
        {QStringLiteral("escape"), QStringLiteral("Esc")},
        {QStringLiteral("esc"), QStringLiteral("Esc")},
        {QStringLiteral("pageup"), QStringLiteral("PgUp")},
        {QStringLiteral("pagedown"), QStringLiteral("PgDown")},
        {QStringLiteral("page up"), QStringLiteral("PgUp")},
        {QStringLiteral("page down"), QStringLiteral("PgDown")},
        {QStringLiteral("control"), QStringLiteral("Ctrl")},
        {QStringLiteral("ctrl"), QStringLiteral("Ctrl")},
        {QStringLiteral("cmd"), QStringLiteral("Ctrl")},
        {QStringLiteral("command"), QStringLiteral("Ctrl")},
        {QStringLiteral("option"), QStringLiteral("Alt")},
        {QStringLiteral("alt"), QStringLiteral("Alt")},
        {QStringLiteral("shift"), QStringLiteral("Shift")},
        {QStringLiteral("meta"), QStringLiteral("Meta")},
        {QStringLiteral("del"), QStringLiteral("Del")},
        {QStringLiteral("delete"), QStringLiteral("Del")},
        {QStringLiteral("backspace"), QStringLiteral("Backspace")},
        {QStringLiteral("space"), QStringLiteral("Space")},
        {QStringLiteral("tab"), QStringLiteral("Tab")},
        {QStringLiteral("up"), QStringLiteral("Up")},
        {QStringLiteral("down"), QStringLiteral("Down")},
        {QStringLiteral("left"), QStringLiteral("Left")},
        {QStringLiteral("right"), QStringLiteral("Right")},
        {QStringLiteral("home"), QStringLiteral("Home")},
        {QStringLiteral("end"), QStringLiteral("End")},
        {QStringLiteral("ins"), QStringLiteral("Ins")},
    };
    QStringList parts;
    for (const QString& p : raw.split(QLatin1Char('+'), Qt::SkipEmptyParts)) {
        const QString t = p.trimmed();
        parts << alias.value(t.toLower(), t.size() == 1 ? t.toUpper() : t);
    }
    if (parts.isEmpty())
        return std::nullopt;
    const QKeySequence seq = QKeySequence::fromString(parts.join(QLatin1Char('+')), QKeySequence::PortableText);
    if (seq.isEmpty() || seq[0].key() == Qt::Key_unknown)
        return std::nullopt;
    KeyChord k;
    k.key = seq[0].key();
    k.mods = seq[0].keyboardModifiers();
    k.seq = QKeySequence(seq[0]);
    if (!(k.mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        if (k.key >= Qt::Key_Space && k.key <= Qt::Key_AsciiTilde) {
            QChar c(k.key);
            k.text = (k.mods & Qt::ShiftModifier) ? QString(c.toUpper()) : QString(c.toLower());
        } else if (k.key == Qt::Key_Return || k.key == Qt::Key_Enter) {
            k.text = QStringLiteral("\r");
        } else if (k.key == Qt::Key_Tab) {
            k.text = QStringLiteral("\t");
        }
    }
    return k;
}

// ── Risk ────────────────────────────────────────────────────────────────────

struct Risk {
    bool risky = false;
    bool money = false;
    QString why;
};

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

Risk classify_words(const QString& words) {
    static const QRegularExpression money(QStringLiteral(
        R"(\b(buy|sell|short|cover)\b|\b(place|submit|send|execute|confirm|modify|amend)\b.{0,24}\b(orders?|trades?|bets?|transactions?|swaps?)\b|\b(orders?|trades?)\b.{0,16}\b(place|submit|send|execute|confirm)|\bsquare[ -]?off\b|\b(close|exit) (all )?positions?\b|\bclose all\b|\bflatten\b|\bliquidate\b|\bcancel (all )?orders?\b|\b(withdraw|transfer|deposit|swap|bridge|stake|unstake|redeem|mint|burn)\b|\bsend (funds|crypto|tokens?)\b|\bsign (and send|transaction)\b|\blive (orders?|trades?|trading|mode)\b|ordersend|orderconfirm|confirmplace|placeorder|submitorder)"));
    static const QRegularExpression destroy(QStringLiteral(
        R"(\b(delete|remove|erase|wipe|purge|drop|uninstall|revoke|regenerate|discard|reset)\b|\bclear (all|history|data|cache|everything)\b|danger|destructive)"));
    static const QRegularExpression benign(QStringLiteral(
        R"(\b(reset|clear|remove|delete)\b.{0,14}\b(zoom|view|filters?|search|selection|chart|columns?|indicators?|stud(y|ies)|overlays?|form|fields?|input|text|sort|highlights?|query)\b)"));
    static const QRegularExpression spend(
        QStringLiteral(R"(\b(credits?|spend|purchase|pay|payment|checkout|subscribe|upgrade|top[ -]?up|recharge)\b)"));
    static const QRegularExpression account(QStringLiteral(R"(\b(log ?out|sign ?out|delete account)\b)"));

    Risk r;
    if (money.match(words).hasMatch()) {
        r.risky = true;
        r.money = true;
        r.why = QStringLiteral("places, changes or cancels an order, or moves money");
        return r;
    }
    if (destroy.match(words).hasMatch() && !benign.match(words).hasMatch()) {
        r.risky = true;
        r.why = QStringLiteral("deletes or resets data");
        return r;
    }
    if (spend.match(words).hasMatch()) {
        r.risky = true;
        r.money = true; // refused in a PAPER run, and never covered by a blanket grant
        r.why = QStringLiteral("spends money or credits");
        return r;
    }
    if (account.match(words).hasMatch()) {
        r.risky = true;
        r.why = QStringLiteral("signs out");
        return r;
    }
    return r;
}

/// A bare refusal ("Cancel", "No", "Close (Esc)") — never a risk, even in a
/// dialog that is about an order. Only the WHOLE name counts: a prefix match let
/// "CANCEL ALL ORDERS" and "Close position (BTC) at market" skip the check.
bool is_negative_choice(const QString& name) {
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^\s*(no|cancel|close|back|dismiss|abort|don'?t|keep|skip|not now|later|go back)\s*(\([^)]*\))?\s*[.!]?\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(name).hasMatch();
}

/// The words of whatever a key sequence would fire in the window around `w`
/// (the same search trigger_shortcut() does). False when nothing owns it.
bool shortcut_words(QWidget* w, const QKeySequence& seq, QString* words) {
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
                return true;
            }
        }
    }
    return false;
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

/// The button Enter commits in dialog `top`: the default one, else an auto-default.
QPushButton* default_button(QWidget* top) {
    if (!top || !qobject_cast<QDialog*>(top))
        return nullptr;
    QPushButton* fallback = nullptr;
    for (QPushButton* b : top->findChildren<QPushButton*>()) {
        if (!b->isVisible() || !b->isEnabled())
            continue;
        if (b->isDefault())
            return b;
        if (!fallback && b->autoDefault())
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
    const bool command_mods = k.mods.testAnyFlags(Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (command_mods || (k.key >= Qt::Key_F1 && k.key <= Qt::Key_F35)) {
        QString words;
        if (shortcut_words(w, k.seq, &words)) {
            const Risk r = classify_words(words);
            if (r.risky)
                return r;
        }
        if (k.mods.testFlag(Qt::AltModifier) && top) {
            if (QAbstractButton* b = mnemonic_button(top, k.key))
                return judge_button(b);
        }
    }
    if (k.key == Qt::Key_Return || k.key == Qt::Key_Enter || k.key == Qt::Key_Space || k.key == Qt::Key_Select) {
        QAbstractButton* button = qobject_cast<QAbstractButton*>(w);
        if (!button && (k.key == Qt::Key_Return || k.key == Qt::Key_Enter))
            button = default_button(top);
        return button ? judge_button(button) : Risk{};
    }
    if (!command_mods && top && qobject_cast<QDialog*>(top)) {
        if (QAbstractButton* b = mnemonic_button(top, k.key))
            return judge_button(b);
    }
    return {};
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

Risk assess(const Step& requested, const Target& t) {
    if (t.web_view)
        return assess_web(requested, t);
    static const QStringList kCommitting = {
        QStringLiteral("click"), QStringLiteral("double_click"), QStringLiteral("press"), QStringLiteral("select"),
        QStringLiteral("check"), QStringLiteral("uncheck"),      QStringLiteral("type"),
    };
    QWidget* w = host_widget(t);
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
    if (is_negative_choice(t.name))
        return {};
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
            const QModelIndex cell = first.siblingAtColumn(column);
            words += QLatin1Char(' ') + cell.data(Qt::DisplayRole).toString().toLower() + QLatin1Char(' ') +
                     view->model()->headerData(column, Qt::Horizontal).toString().toLower();
            if (QWidget* cw = view->indexWidget(cell))
                words += QLatin1Char(' ') + cell_widget_text(cw, false).toLower();
        }
    }
    Risk r = classify_words(words);
    if (r.risky || t.role == QLatin1String("tab"))
        return t.role == QLatin1String("tab") ? Risk{} : r;
    // An affirmative button in a dialog inherits the dialog's meaning
    // ("Yes" in "Delete portfolio?", "OK" in "Confirm live order").
    const QString ctx = dialog_context(w);
    if (!ctx.isEmpty())
        r = classify_words(ctx);
    return r;
}

/// After the user approves a risky step, the confirmation dialog that step
/// opens (the app's own "Confirm order" / "Delete?") is part of the same intent:
/// one answer in that dialog, by the same caller, within 45 s, needs no second
/// approval. Consumed on use.
struct ApprovalCarry {
    QString origin;
    qint64 at_ms = 0;
    QList<QPointer<QWidget>> dialogs_before;
    bool armed = false;
    bool money = false; // the kind of risk the user approved — a carry never crosses kinds
};

ApprovalCarry& carry() {
    static ApprovalCarry c;
    return c;
}

bool carry_covers(const QString& origin, QWidget* w, const Risk& risk) {
    ApprovalCarry& c = carry();
    if (!c.armed || c.origin != origin || QDateTime::currentMSecsSinceEpoch() - c.at_ms > kCarryOverMs || !w)
        return false;
    // An approved delete must not cover a "Confirm live order" that pops up next.
    if (c.money != risk.money)
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

void arm_carry(const QString& origin, bool money) {
    ApprovalCarry& c = carry();
    c.origin = origin;
    c.money = money;
    c.at_ms = QDateTime::currentMSecsSinceEpoch();
    c.dialogs_before.clear();
    for (QWidget* d : open_dialogs())
        c.dialogs_before << d;
    c.armed = true;
}

// ── Event synthesis ─────────────────────────────────────────────────────────

void send_mouse(QWidget* w, QPoint pos, Qt::MouseButton button, bool dbl) {
    QPointer<QWidget> guard(w);
    auto send = [&](QEvent::Type type, Qt::MouseButton b, Qt::MouseButtons held) {
        if (!guard)
            return;
        const QPointF local(pos);
        const QPointF scene = guard->mapTo(guard->window(), pos);
        const QPointF global = guard->mapToGlobal(pos);
        QMouseEvent ev(type, local, scene, global, b, held, Qt::NoModifier);
        QCoreApplication::sendEvent(guard, &ev);
    };
    send(QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
    send(QEvent::MouseButtonPress, button, button);
    send(QEvent::MouseButtonRelease, button, Qt::NoButton);
    if (dbl) {
        send(QEvent::MouseButtonDblClick, button, button);
        send(QEvent::MouseButtonRelease, button, Qt::NoButton);
    }
}

void send_key(QWidget* w, int key, Qt::KeyboardModifiers mods, const QString& text) {
    QPointer<QWidget> guard(w);
    QKeyEvent press(QEvent::KeyPress, key, mods, text);
    QCoreApplication::sendEvent(guard, &press);
    if (!guard)
        return;
    QKeyEvent release(QEvent::KeyRelease, key, mods, text);
    QCoreApplication::sendEvent(guard, &release);
}

/// Trigger an action / QShortcut bound to `seq` in the window around `w`.
bool trigger_shortcut(QWidget* w, const QKeySequence& seq) {
    QWidget* top = w ? w->window() : nullptr;
    if (!top)
        return false;
    QList<QAction*> actions = top->actions();
    actions += top->findChildren<QAction*>();
    for (QAction* a : actions) {
        if (!a->isEnabled())
            continue;
        for (const QKeySequence& s : a->shortcuts()) {
            if (s == seq) {
                a->trigger();
                return true;
            }
        }
    }
    for (QShortcut* sc : top->findChildren<QShortcut*>()) {
        if (!sc->isEnabled())
            continue;
        for (const QKeySequence& s : sc->keys()) {
            if (s == seq) {
                emit sc->activated();
                return true;
            }
        }
    }
    return false;
}

int find_option(QComboBox* cb, const QString& wanted) {
    const QString w = wanted.trimmed();
    if (w.startsWith(QLatin1Char('#'))) {
        bool ok = false;
        const int i = w.mid(1).toInt(&ok);
        if (ok && i >= 0 && i < cb->count())
            return i;
    }
    for (int i = 0; i < cb->count(); ++i) {
        if (cb->itemText(i).compare(w, Qt::CaseInsensitive) == 0)
            return i;
    }
    for (int i = 0; i < cb->count(); ++i) {
        if (cb->itemText(i).startsWith(w, Qt::CaseInsensitive))
            return i;
    }
    for (int i = 0; i < cb->count(); ++i) {
        if (cb->itemText(i).contains(w, Qt::CaseInsensitive))
            return i;
    }
    const int data_idx = cb->findData(w);
    return data_idx;
}

int find_tab(QTabBar* bar, const QString& wanted) {
    for (int i = 0; i < bar->count(); ++i) {
        if (bar->tabText(i).remove(QLatin1Char('&')).compare(wanted.trimmed(), Qt::CaseInsensitive) == 0)
            return i;
    }
    for (int i = 0; i < bar->count(); ++i) {
        if (bar->tabText(i).remove(QLatin1Char('&')).contains(wanted.trimmed(), Qt::CaseInsensitive))
            return i;
    }
    return -1;
}

QAction* find_menu_action(QWidget* menu_or_bar, const QString& wanted) {
    QList<QAction*> actions = menu_or_bar->actions();
    const QString w = wanted.trimmed();
    for (QAction* a : actions) {
        if (a->isVisible() && !a->isSeparator() &&
            a->text().remove(QLatin1Char('&')).compare(w, Qt::CaseInsensitive) == 0)
            return a;
    }
    for (QAction* a : actions) {
        if (a->isVisible() && !a->isSeparator() && a->text().remove(QLatin1Char('&')).contains(w, Qt::CaseInsensitive))
            return a;
    }
    return nullptr;
}

QMenu* menu_showing(QAction* a) {
    if (!a)
        return nullptr;
    for (QObject* o : a->associatedObjects()) {
        if (auto* m = qobject_cast<QMenu*>(o); m && m->isVisible())
            return m;
    }
    return nullptr;
}

void click_view_row(QAbstractItemView* view, int row, bool dbl, Qt::MouseButton button, int column = -1) {
    const QModelIndex idx = view_index(view, row, column >= 0 ? column : visible_columns(view).value(0, 0));
    if (!idx.isValid())
        return;
    view->scrollTo(idx);
    const QRect rect = view->visualRect(idx);
    if (button == Qt::RightButton) {
        if (QItemSelectionModel* sel = view->selectionModel())
            sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, rect.center(), view->viewport()->mapToGlobal(rect.center()));
        QCoreApplication::sendEvent(view->viewport(), &ev);
        return;
    }
    send_mouse(view->viewport(), rect.center(), button, dbl);
}

// ── Prepare: validate a step and build the closure that performs it ─────────

using Perform = std::function<void()>;

struct Prepared {
    Perform perform;
    QString note; // what will be reported for the step
};

constexpr int kWebLocateMs = 5000;

/// What finds the step's element in the page again (by index + key, at a point,
/// or the focused one), for page operations that run when the step performs.
QJsonObject web_element_spec(const Target& t) {
    if (t.web >= 0)
        return {{QStringLiteral("mode"), QStringLiteral("control")},
                {QStringLiteral("i"), t.web_info.value(QStringLiteral("i")).toInt(t.web)},
                {QStringLiteral("k"), t.web_key}};
    if (t.web_point) {
        const double z = web_zoom(t.web_view);
        return {{QStringLiteral("mode"), QStringLiteral("point")},
                {QStringLiteral("x"), t.hit_pos.x() / z},
                {QStringLiteral("y"), t.hit_pos.y() / z}};
    }
    return {{QStringLiteral("mode"), QStringLiteral("focus")}};
}

/// Run a page operation on the step's element; the answer is not needed (the
/// step's report comes from re-reading).
void web_page_op(const QPointer<QWidget>& view, QJsonObject spec, const QJsonObject& op) {
    if (!view)
        return;
    for (auto it = op.constBegin(); it != op.constEnd(); ++it)
        spec.insert(it.key(), it.value());
    run_web_script(view, web_target_script(spec), false, kWebLocateMs, [](const QJsonObject&, const QString&) {});
}

/// Mouse at a point of the page, delivered where a person's would land: the
/// view's render child.
void web_mouse(const QPointer<QWidget>& view, QPoint pos, Qt::MouseButton button, bool dbl, bool move_only = false) {
    if (!view)
        return;
    QWidget* in = web_input_widget(view);
    const QPoint local = in == view.data() ? pos : in->mapFrom(view.data(), pos);
    if (move_only) {
        QMouseEvent move(QEvent::MouseMove, QPointF(local), in->mapTo(in->window(), QPointF(local)),
                         in->mapToGlobal(QPointF(local)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(in, &move);
        return;
    }
    send_mouse(in, local, button, dbl);
}

void web_keys(const QPointer<QWidget>& view, const QString& text) {
    for (const QChar c : text) {
        if (!view)
            return;
        send_key(web_input_widget(view), c.toUpper().unicode(), c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier,
                 QString(c));
    }
}

/// A step on a web page. The page script already found the element
/// (`t.web_info`); clicks and keys go in as a person's would — at its centre,
/// through the view — and a control the page keeps hidden (a closed menu's
/// item) is clicked through the page instead.
std::optional<Prepared> prepare_web(const Step& s, const Target& t, QString* error) {
    QPointer<QWidget> vg(t.web_view);
    const QJsonObject& in = t.web_info;
    const QString what =
        t.web_focus   ? QStringLiteral("the focused element of %1").arg(t.ref)
        : t.web_point ? QStringLiteral("%1 %2 at (%3, %4) of %5")
                            .arg(in.value(QStringLiteral("r")).toString(), common::quoted(common::clip(t.name, 60)))
                            .arg(t.hit_pos.x())
                            .arg(t.hit_pos.y())
                            .arg(t.ref)
                      : describe_target(t);
    if (!vg || !vg->isVisible()) {
        *error = QStringLiteral("%1: the web page is not showing.").arg(what);
        return std::nullopt;
    }
    QStringList states;
    for (const QJsonValue& v : in.value(QStringLiteral("s")).toArray())
        states << v.toString();
    const bool secret = in.value(QStringLiteral("secret")).toBool();
    const bool hidden = t.web >= 0 && !t.has_hit; // no box on screen: a closed menu's item, a collapsed section
    const QPoint pos = t.has_hit ? t.hit_pos : vg->rect().center();
    const QJsonObject spec = web_element_spec(t);
    const QString act = s.action;
    Prepared p;
    p.note = QStringLiteral("%1 %2").arg(act, what);

    if (act == QLatin1String("close")) {
        *error = QStringLiteral("%1 is part of a web page — click its own close button instead.").arg(what);
        return std::nullopt;
    }
    if (act == QLatin1String("scroll")) {
        // A wheel turn at the point (or the page centre), as a person scrolls.
        const int notches = s.amount == 0 ? 5 : s.amount;
        p.perform = [vg, pos, notches]() {
            if (!vg)
                return;
            QWidget* w = web_input_widget(vg);
            const QPoint local = w == vg.data() ? pos : w->mapFrom(vg.data(), pos);
            QWheelEvent wheel(QPointF(local), w->mapToGlobal(QPointF(local)), QPoint(), QPoint(0, -120 * notches),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(w, &wheel);
        };
        return p;
    }
    if (t.web_focus && act != QLatin1String("press") && act != QLatin1String("type") && act != QLatin1String("focus")) {
        *error = QStringLiteral("Say which element of the page: a ref like %1:w3 from ui_read(ref=\"%1\"), or x / y.")
                     .arg(t.ref);
        return std::nullopt;
    }
    const bool needs_enabled = act != QLatin1String("hover") && act != QLatin1String("focus");
    if (needs_enabled && states.contains(QStringLiteral("disabled"))) {
        *error = QStringLiteral("%1 is disabled.").arg(what);
        return std::nullopt;
    }
    if (in.value(QStringLiteral("covered")).toBool() && act != QLatin1String("focus") && !t.web_point) {
        *error = QStringLiteral("%1 is covered by %2 — a click would land on that. Close it first, or look: "
                                "ui_screenshot(target=\"%3\").")
                     .arg(what, common::quoted(in.value(QStringLiteral("cover")).toString()),
                          t.ref.section(QLatin1Char(':'), 0, 0));
        return std::nullopt;
    }

    // ── Typing ──
    if (act == QLatin1String("type") || act == QLatin1String("clear") ||
        (act == QLatin1String("set_value") && in.value(QStringLiteral("editable")).toBool())) {
        if (secret) {
            *error =
                QStringLiteral("%1 is a secret field (password). Ask the user to type it — agents never type secrets.")
                    .arg(what);
            return std::nullopt;
        }
        if (t.web_focus && in.value(QStringLiteral("none")).toBool()) {
            *error = QStringLiteral("Nothing on the page has focus — type with a ref to the field (ui_read(ref=\"%1\") "
                                    "lists them).")
                         .arg(t.ref);
            return std::nullopt;
        }
        if (!in.value(QStringLiteral("editable")).toBool()) {
            *error = QStringLiteral("%1 is not a text field — click or select it instead.").arg(what);
            return std::nullopt;
        }
        if (hidden) {
            *error = QStringLiteral("%1 is hidden on the page — open what holds it first.").arg(what);
            return std::nullopt;
        }
        const QString text =
            act == QLatin1String("clear") ? QString()
            : act == QLatin1String("set_value")
                ? (s.value.isString() ? s.value.toString() : QString::number(s.value.toDouble(), 'g', 15))
                : s.text;
        if (text.size() > 2000) {
            *error = QStringLiteral("At most 2000 characters are typed into a web field per step.");
            return std::nullopt;
        }
        const bool append = s.append && act == QLatin1String("type");
        const bool submit = s.submit && act == QLatin1String("type");
        const bool click_first = !t.web_focus; // a click puts the caret in the field, as a person's does
        p.perform = [vg, pos, text, append, submit, click_first]() {
            if (!vg)
                return;
            if (click_first)
                web_mouse(vg, pos, Qt::LeftButton, false);
            if (!vg)
                return;
            QWidget* w = web_input_widget(vg);
            w->setFocus(Qt::OtherFocusReason);
            if (!append) {
                send_key(w, Qt::Key_A, Qt::ControlModifier, QString()); // select what is there
                send_key(w, Qt::Key_Delete, Qt::NoModifier, QString());
            }
            web_keys(vg, text);
            if (submit && vg)
                send_key(web_input_widget(vg), Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        };
        p.note = act == QLatin1String("clear") ? QStringLiteral("clear %1").arg(what)
                                               : QStringLiteral("type %1 into %2%3")
                                                     .arg(common::quoted(common::clip(text, 60)), what,
                                                          submit ? QStringLiteral(" + Enter") : QString());
        return p;
    }

    // ── Keys ──
    if (act == QLatin1String("press")) {
        const auto chord = parse_key(s.text);
        if (!chord) {
            *error = QStringLiteral("\"%1\" is not a key. Examples: Enter, Escape, Tab, Down, PageDown, Ctrl+S, F5.")
                         .arg(s.text);
            return std::nullopt;
        }
        const KeyChord k = *chord;
        const bool ctrl_like = k.mods.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier);
        if ((ctrl_like && k.key == Qt::Key_V) || (k.mods.testFlag(Qt::ShiftModifier) && k.key == Qt::Key_Insert)) {
            *error = QStringLiteral("Pasting is not available to agents (the clipboard is the user's). Type the text "
                                    "with action=\"type\".");
            return std::nullopt;
        }
        if (secret) {
            *error = QStringLiteral("%1 is a secret field (password) — agents never operate it.").arg(what);
            return std::nullopt;
        }
        const bool focus_first = t.web >= 0;
        p.perform = [vg, spec, k, focus_first]() {
            if (!vg)
                return;
            auto send = [vg, k]() {
                if (vg)
                    send_key(web_input_widget(vg), k.key, k.mods, k.text);
            };
            web_input_widget(vg)->setFocus(Qt::OtherFocusReason);
            if (!focus_first) {
                send();
                return;
            }
            // Focus the control through the page, then press once that has landed.
            web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("focus")}});
            QTimer::singleShot(80, vg, send);
        };
        p.note = QStringLiteral("press %1 on %2").arg(s.text, what);
        return p;
    }

    if (act == QLatin1String("focus")) {
        p.perform = [vg, spec]() {
            if (!vg)
                return;
            web_input_widget(vg)->setFocus(Qt::OtherFocusReason);
            web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("focus")}});
        };
        return p;
    }

    // ── Options of a <select> ──
    const bool is_select = in.value(QStringLiteral("tag")).toString() == QLatin1String("select");
    if (act == QLatin1String("select") || (act == QLatin1String("set_value") && is_select)) {
        const QString want = act == QLatin1String("set_value") && s.value.isString() ? s.value.toString() : s.text;
        if (!is_select) {
            const QString role = in.value(QStringLiteral("r")).toString();
            if (role == QLatin1String("option") || role == QLatin1String("tab") || role == QLatin1String("radio") ||
                role.startsWith(QLatin1String("menuitem"))) {
                Step click = s;
                click.action = QStringLiteral("click");
                return prepare_web(click, t, error);
            }
            *error = QStringLiteral("%1 has no options to select — click it, or use type.").arg(what);
            return std::nullopt;
        }
        const QString picked = in.value(QStringLiteral("picked")).toString();
        if (picked.isEmpty()) {
            QStringList options;
            for (const QJsonValue& o : in.value(QStringLiteral("options")).toArray())
                options << o.toString();
            *error = QStringLiteral("%1 has no option \"%2\" — options: %3.")
                         .arg(what, want, common::clip(options.join(QStringLiteral(" | ")), 400));
            return std::nullopt;
        }
        p.perform = [vg, spec, want]() {
            web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("select")}, {QStringLiteral("text"), want}});
        };
        p.note = QStringLiteral("select %1 in %2").arg(common::quoted(picked), what);
        return p;
    }
    if (act == QLatin1String("set_value")) {
        *error = QStringLiteral("%1 does not hold a value — use click / type / select.").arg(what);
        return std::nullopt;
    }

    // ── Check / uncheck: a click unless it already is ──
    if (act == QLatin1String("check") || act == QLatin1String("uncheck")) {
        const bool want = act == QLatin1String("check");
        if (!states.contains(QStringLiteral("checked")) && !states.contains(QStringLiteral("unchecked"))) {
            *error = QStringLiteral("%1 cannot be checked — click it.").arg(what);
            return std::nullopt;
        }
        if (states.contains(QStringLiteral("checked")) == want) {
            p.perform = []() {};
            p.note +=
                QStringLiteral(" (already %1)").arg(want ? QStringLiteral("checked") : QStringLiteral("unchecked"));
            return p;
        }
    }

    // ── Clicks and hover ──
    if (act == QLatin1String("hover")) {
        if (hidden) {
            *error = QStringLiteral("%1 is hidden on the page — nothing to point at.").arg(what);
            return std::nullopt;
        }
        p.perform = [vg, pos]() { web_mouse(vg, pos, Qt::NoButton, false, true); };
        return p;
    }
    if (act == QLatin1String("click") || act == QLatin1String("double_click") || act == QLatin1String("right_click") ||
        act == QLatin1String("expand") || act == QLatin1String("collapse") || act == QLatin1String("check") ||
        act == QLatin1String("uncheck")) {
        if (hidden) {
            if (act == QLatin1String("double_click") || act == QLatin1String("right_click")) {
                *error = QStringLiteral("%1 is hidden on the page — open the menu or section holding it first, or "
                                        "click it (a click goes through the page).")
                             .arg(what);
                return std::nullopt;
            }
            p.perform = [vg, spec]() { web_page_op(vg, spec, {{QStringLiteral("op"), QStringLiteral("click")}}); };
            p.note += QStringLiteral(" (hidden on the page — clicked through it)");
            return p;
        }
        const bool dbl = act == QLatin1String("double_click");
        const Qt::MouseButton button = act == QLatin1String("right_click") ? Qt::RightButton : Qt::LeftButton;
        p.perform = [vg, pos, button, dbl]() { web_mouse(vg, pos, button, dbl); };
        return p;
    }
    *error =
        QStringLiteral("Unknown action \"%1\". Use one of: %2.").arg(act, known_actions().join(QStringLiteral(", ")));
    return std::nullopt;
}

std::optional<Prepared> prepare(const Step& s, const Target& t, QString* error) {
    if (t.web_view)
        return prepare_web(s, t, error);
    QWidget* w = host_widget(t);
    QAction* a = t.action;
    const QString what = describe_target(t);
    QPointer<QWidget> wg(w);
    QPointer<QAction> ag(a);
    QPointer<ads::CDockWidget> dock(t.dock);
    Prepared p;
    p.note = QStringLiteral("%1 %2").arg(s.action, what);

    // ── Panel-level actions ──
    if (t.role == QLatin1String("panel")) {
        if (s.action == QLatin1String("close")) {
            p.perform = [dock]() {
                if (dock)
                    dock->toggleView(false);
            };
            return p;
        }
        if (s.action == QLatin1String("focus")) {
            p.perform = [dock]() {
                if (!dock)
                    return;
                dock->raise();
                dock->setAsCurrentTab();
                if (QWidget* c = dock->widget())
                    c->setFocus(Qt::OtherFocusReason);
            };
            return p;
        }
        if (s.action == QLatin1String("scroll")) {
            QAbstractScrollArea* area = nullptr;
            if (t.dock) {
                for (QAbstractScrollArea* sa : t.dock->findChildren<QAbstractScrollArea*>()) {
                    if (sa->isVisible() && sa->verticalScrollBar() && sa->verticalScrollBar()->maximum() > 0) {
                        area = sa;
                        break;
                    }
                }
            }
            if (!area) {
                *error = QStringLiteral("Nothing scrolls in %1.").arg(what);
                return std::nullopt;
            }
            QPointer<QAbstractScrollArea> sg(area);
            const int amount = s.amount;
            p.perform = [sg, amount]() {
                if (!sg)
                    return;
                QScrollBar* sb = sg->verticalScrollBar();
                sb->setValue(sb->value() + (amount == 0 ? sb->pageStep() : amount * sb->singleStep() * 3));
            };
            return p;
        }
        *error = QStringLiteral("%1: a panel can be focused, closed or scrolled; act on its elements instead "
                                "(ui_snapshot(target=\"%2\")).")
                     .arg(what, t.ref);
        return std::nullopt;
    }

    // ── Menu items ──
    if (a) {
        if (s.action != QLatin1String("click") && s.action != QLatin1String("select") &&
            s.action != QLatin1String("check") && s.action != QLatin1String("uncheck") &&
            s.action != QLatin1String("expand") && s.action != QLatin1String("hover")) {
            *error = QStringLiteral("%1 is a menu item: click / select / check / uncheck / expand it.").arg(what);
            return std::nullopt;
        }
        if (!a->isEnabled()) {
            *error = QStringLiteral("%1 is disabled.").arg(what);
            return std::nullopt;
        }
        if ((s.action == QLatin1String("check") && a->isChecked()) ||
            (s.action == QLatin1String("uncheck") && !a->isChecked())) {
            p.perform = []() {};
            p.note += QStringLiteral(" (already %1)")
                          .arg(a->isChecked() ? QStringLiteral("checked") : QStringLiteral("unchecked"));
            return p;
        }
        QPointer<QWidget> host(t.widget);
        p.perform = [ag, host]() {
            if (!ag)
                return;
            if (auto* bar = qobject_cast<QMenuBar*>(host.data())) {
                if (ag->menu())
                    bar->setActiveAction(ag); // opens its menu (non-blocking popup)
                else
                    ag->trigger();
                return;
            }
            if (QMenu* menu = menu_showing(ag)) {
                // As a person does it: highlight, then Enter — the menu closes and
                // an exec()'d menu returns this action to its caller.
                menu->setActiveAction(ag);
                if (ag->menu())
                    return; // a submenu opens on activation
                send_key(menu, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
                return;
            }
            ag->trigger();
        };
        return p;
    }

    if (!w) {
        *error = QStringLiteral("%1 is gone — take a fresh ui_snapshot.").arg(what);
        return std::nullopt;
    }

    // ── Dialog-level ──
    if (s.action == QLatin1String("close")) {
        if (auto* menu = qobject_cast<QMenu*>(w)) {
            p.perform = [wg]() {
                if (wg)
                    wg->close();
            };
            return p;
        }
        QWidget* top = w->window();
        if (auto* dlg = qobject_cast<QDialog*>(top)) {
            QPointer<QDialog> dg(dlg);
            p.perform = [dg]() {
                if (dg)
                    dg->reject();
            };
            p.note = QStringLiteral("close %1").arg(location_label(dlg));
            return p;
        }
        if (top && top->windowType() == Qt::Dialog) {
            QPointer<QWidget> tg(top);
            p.perform = [tg]() {
                if (tg)
                    tg->close();
            };
            return p;
        }
        if (QComboBox* cb = qobject_cast<QComboBox*>(w)) {
            QPointer<QComboBox> cg(cb);
            p.perform = [cg]() {
                if (cg)
                    cg->hidePopup();
            };
            return p;
        }
        *error = QStringLiteral("%1 is not a dialog, menu or popup. Close a panel with ui_workspace(action=\"close\").")
                     .arg(what);
        return std::nullopt;
    }

    const bool needs_enabled =
        s.action != QLatin1String("hover") && s.action != QLatin1String("scroll") && s.action != QLatin1String("focus");
    if (needs_enabled && !w->isEnabled()) {
        *error = QStringLiteral("%1 is disabled.").arg(what);
        return std::nullopt;
    }
    if (!w->isVisible()) {
        // A control in a table row that is scrolled out of view: bring the row
        // in and go on (clicks, checks, selects and typing reach it either way).
        QModelIndex cell;
        if (QAbstractItemView* owner = cell_widget_owner(w, &cell)) {
            owner->scrollTo(cell);
        } else {
            *error = QStringLiteral("%1 is not visible (another tab, or scrolled into a hidden section).").arg(what);
            return std::nullopt;
        }
    }

    // ── Tabs ──
    if (auto* bar = qobject_cast<QTabBar*>(w)) {
        int index = t.sub;
        if ((s.action == QLatin1String("select") || index < 0) && !s.text.isEmpty())
            index = find_tab(bar, s.text);
        if (index < 0 || index >= bar->count()) {
            *error = QStringLiteral("No tab \"%1\" in %2.").arg(s.text, what);
            return std::nullopt;
        }
        if (!bar->isTabEnabled(index)) {
            *error = QStringLiteral("Tab \"%1\" is disabled.").arg(bar->tabText(index));
            return std::nullopt;
        }
        if (s.action == QLatin1String("close")) {
            *error = QStringLiteral("Tabs are switched, not closed, here.");
            return std::nullopt;
        }
        QPointer<QTabBar> bg(bar);
        p.perform = [bg, index]() {
            if (!bg)
                return;
            emit bg->tabBarClicked(index);
            bg->setCurrentIndex(index);
        };
        p.note = QStringLiteral("select tab %1").arg(common::quoted(bar->tabText(index).remove(QLatin1Char('&'))));
        return p;
    }

    const QString act = s.action;

    // ── Item views (tables, lists, trees) ──
    if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        QPointer<QAbstractItemView> vg(view);
        int row = t.row;
        if (row < 0 && !s.text.isEmpty())
            row = view_find_row(view, s.text);
        if (act == QLatin1String("scroll")) {
            const int amount = s.amount;
            p.perform = [vg, amount]() {
                if (!vg)
                    return;
                QScrollBar* sb = vg->verticalScrollBar();
                sb->setValue(sb->value() + (amount == 0 ? sb->pageStep() : amount * sb->singleStep() * 3));
            };
            return p;
        }
        if (act == QLatin1String("focus")) {
            p.perform = [vg]() {
                if (vg)
                    vg->setFocus(Qt::OtherFocusReason);
            };
            return p;
        }
        if (row < 0) {
            *error = s.text.isEmpty()
                         ? QStringLiteral("%1: say which row — a row ref like %2:r0, or `text` the row holds.")
                               .arg(what, t.ref)
                         : QStringLiteral("No row of %1 holds \"%2\".").arg(what, s.text);
            return std::nullopt;
        }
        if (!view_index(view, row, 0).isValid()) {
            *error = QStringLiteral("%1 has no row %2 (it has %3).").arg(what).arg(row).arg(view_row_count(view));
            return std::nullopt;
        }
        int column = t.column;
        if (!s.column.isEmpty()) {
            column = resolve_column(view, s.column);
            if (column < 0) {
                *error = QStringLiteral("%1 has no column \"%2\" — columns: %3.")
                             .arg(what, s.column,
                                  common::clip(column_headers(view, visible_columns(view)).join(QStringLiteral(" | ")),
                                               400));
                return std::nullopt;
            }
        }
        p.note = QStringLiteral("%1 row r%2%3%4 of %5: %6")
                     .arg(act, QString::number(row),
                          t.listed_row >= 0 ? QStringLiteral(" (listed as r%1; the list moved)").arg(t.listed_row)
                                            : QString(),
                          column >= 0
                              ? QStringLiteral(", column %1")
                                    .arg(common::quoted(view->model()->headerData(column, Qt::Horizontal).toString()))
                              : QString(),
                          what, common::quoted(common::clip(view_row_text(view, row, visible_columns(view), 30), 90)));
        const bool at_point = t.has_hit; // a click at x/y that landed on this row
        const QPoint point = t.hit_pos;
        auto mouse = [vg, row, column, at_point, point](bool dbl, Qt::MouseButton button) {
            if (!vg)
                return;
            if (at_point && button == Qt::LeftButton) {
                send_mouse(vg->viewport(), point, button, dbl);
                return;
            }
            click_view_row(vg, row, dbl, button, column);
        };
        if (act == QLatin1String("click") || act == QLatin1String("select")) {
            p.perform = [mouse]() { mouse(false, Qt::LeftButton); };
            return p;
        }
        if (act == QLatin1String("double_click")) {
            p.perform = [mouse]() { mouse(true, Qt::LeftButton); };
            return p;
        }
        if (act == QLatin1String("right_click")) {
            p.perform = [mouse]() { mouse(false, Qt::RightButton); };
            return p;
        }
        if (act == QLatin1String("expand") || act == QLatin1String("collapse")) {
            auto* tree = qobject_cast<QTreeView*>(view);
            if (!tree) {
                *error = QStringLiteral("%1 is not a tree.").arg(what);
                return std::nullopt;
            }
            const bool expand = act == QLatin1String("expand");
            QPointer<QTreeView> tg(tree);
            p.perform = [tg, row, expand]() {
                if (!tg)
                    return;
                const QModelIndex idx = view_index(tg, row, 0);
                if (idx.isValid())
                    tg->setExpanded(idx, expand);
            };
            return p;
        }
        if (act == QLatin1String("check") || act == QLatin1String("uncheck")) {
            const QModelIndex idx = view_index(view, row, visible_columns(view).value(0, 0));
            if (!(view->model()->flags(idx) & Qt::ItemIsUserCheckable)) {
                *error = QStringLiteral("Row r%1 of %2 has no checkbox.").arg(row).arg(what);
                return std::nullopt;
            }
            const Qt::CheckState want = act == QLatin1String("check") ? Qt::Checked : Qt::Unchecked;
            p.perform = [vg, row, want]() {
                if (!vg)
                    return;
                const QModelIndex i = view_index(vg, row, visible_columns(vg).value(0, 0));
                if (i.isValid())
                    vg->model()->setData(i, static_cast<int>(want), Qt::CheckStateRole);
            };
            return p;
        }
        *error = QStringLiteral("%1: rows can be clicked, double-clicked, right-clicked, selected, checked or "
                                "expanded.")
                     .arg(what);
        return std::nullopt;
    }

    // ── Typing ──
    if (act == QLatin1String("type") || act == QLatin1String("clear")) {
        if (is_secret(w)) {
            *error =
                QStringLiteral("%1 is a secret field (password / API key / token). Ask the user to type it — agents "
                               "never type secrets.")
                    .arg(what);
            return std::nullopt;
        }
        const QString text = act == QLatin1String("clear") ? QString() : s.text;
        const bool append = s.append && act == QLatin1String("type");
        const bool submit = s.submit && act == QLatin1String("type");
        QLineEdit* le = qobject_cast<QLineEdit*>(w);
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            if (cb->isEditable())
                le = cb->lineEdit();
            else if (act == QLatin1String("type")) {
                const int idx = find_option(cb, text);
                if (idx < 0) {
                    *error = QStringLiteral("%1 has no option \"%2\" (ui_read lists them).").arg(what, text);
                    return std::nullopt;
                }
                QPointer<QComboBox> cg(cb);
                p.perform = [cg, idx]() {
                    if (!cg)
                        return;
                    cg->setCurrentIndex(idx);
                    emit cg->activated(idx);
                    emit cg->textActivated(cg->itemText(idx));
                };
                p.note = QStringLiteral("select %1 in %2").arg(common::quoted(cb->itemText(idx)), what);
                return p;
            }
        }
        if (auto* sb = qobject_cast<QAbstractSpinBox*>(w); sb && !le) {
            le = sb->findChild<QLineEdit*>();
        }
        if (le) {
            if (le->isReadOnly()) {
                *error = QStringLiteral("%1 is read-only.").arg(what);
                return std::nullopt;
            }
            QPointer<QLineEdit> lg(le);
            p.perform = [lg, text, append, submit]() {
                if (!lg)
                    return;
                lg->setFocus(Qt::OtherFocusReason);
                if (append)
                    lg->end(false);
                else
                    lg->selectAll();
                if (text.isEmpty() && !append)
                    lg->del();
                else
                    lg->insert(text); // emits textEdited, like typing
                if (submit && lg)
                    send_key(lg, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
            };
            p.note = act == QLatin1String("clear") ? QStringLiteral("clear %1").arg(what)
                                                   : QStringLiteral("type %1 into %2%3")
                                                         .arg(common::quoted(common::clip(text, 60)), what,
                                                              submit ? QStringLiteral(" + Enter") : QString());
            return p;
        }
        if (qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)) {
            auto* te = qobject_cast<QTextEdit*>(w);
            auto* pe = qobject_cast<QPlainTextEdit*>(w);
            if ((te && te->isReadOnly()) || (pe && pe->isReadOnly())) {
                *error = QStringLiteral("%1 is read-only.").arg(what);
                return std::nullopt;
            }
            QPointer<QTextEdit> tg(te);
            QPointer<QPlainTextEdit> pg(pe);
            p.perform = [tg, pg, text, append]() {
                if (tg) {
                    tg->setFocus(Qt::OtherFocusReason);
                    if (append) {
                        tg->moveCursor(QTextCursor::End);
                        tg->insertPlainText(text);
                    } else {
                        tg->setPlainText(text);
                    }
                } else if (pg) {
                    pg->setFocus(Qt::OtherFocusReason);
                    if (append) {
                        pg->moveCursor(QTextCursor::End);
                        pg->insertPlainText(text);
                    } else {
                        pg->setPlainText(text);
                    }
                }
            };
            p.note = QStringLiteral("type %1 chars into %2").arg(text.size()).arg(what);
            return p;
        }
        // Anything else that takes keys: type it key by key — but never into a
        // button (Space clicks it), a dialog (a letter answers "&Yes") or a menu.
        if (qobject_cast<QAbstractButton*>(w) || qobject_cast<QDialog*>(w) || qobject_cast<QMenu*>(w) ||
            qobject_cast<QDialogButtonBox*>(w)) {
            *error =
                QStringLiteral("%1 is not a text field — use click (or select / press) instead of type.").arg(what);
            return std::nullopt;
        }
        if (text.size() > 2000) {
            *error = QStringLiteral("%1 is not a text field; typing more than 2000 keys into it is refused.").arg(what);
            return std::nullopt;
        }
        p.perform = [wg, text, submit]() {
            if (!wg)
                return;
            wg->setFocus(Qt::OtherFocusReason);
            for (const QChar c : text) {
                if (!wg)
                    return;
                send_key(wg, c.toUpper().unicode(), c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier, QString(c));
            }
            if (submit && wg)
                send_key(wg, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        };
        return p;
    }

    // ── Selecting an option ──
    if (act == QLatin1String("select")) {
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            const int idx = find_option(cb, s.text.isEmpty() ? s.value.toString() : s.text);
            if (idx < 0) {
                *error = QStringLiteral("%1 has no option \"%2\" (ui_read lists them).").arg(what, s.text);
                return std::nullopt;
            }
            QPointer<QComboBox> cg(cb);
            p.perform = [cg, idx]() {
                if (!cg)
                    return;
                cg->setCurrentIndex(idx);
                emit cg->activated(idx);
                emit cg->textActivated(cg->itemText(idx));
            };
            p.note = QStringLiteral("select %1 in %2").arg(common::quoted(cb->itemText(idx)), what);
            return p;
        }
        if (auto* menu = qobject_cast<QMenu*>(w)) {
            QAction* found = find_menu_action(menu, s.text);
            if (!found) {
                *error = QStringLiteral("The menu has no item \"%1\".").arg(s.text);
                return std::nullopt;
            }
            Target mt;
            mt.action = found;
            mt.widget = menu;
            classify_target(mt);
            Step click = s;
            click.action = QStringLiteral("click");
            return prepare(click, mt, error);
        }
        if (qobject_cast<QRadioButton*>(w) || qobject_cast<QCheckBox*>(w)) {
            Step click = s;
            click.action = QStringLiteral("check");
            return prepare(click, t, error);
        }
        // A radio group: the button holding the text.
        if (!s.text.isEmpty()) {
            for (QRadioButton* rb : w->findChildren<QRadioButton*>()) {
                if (rb->isVisible() && rb->text().remove(QLatin1Char('&')).contains(s.text, Qt::CaseInsensitive)) {
                    QPointer<QRadioButton> rg(rb);
                    p.perform = [rg]() {
                        if (rg && !rg->isChecked())
                            rg->click();
                    };
                    p.note = QStringLiteral("select %1").arg(common::quoted(rb->text().remove(QLatin1Char('&'))));
                    return p;
                }
            }
        }
        *error = QStringLiteral("%1 has no options to select — click it, or use type / set_value.").arg(what);
        return std::nullopt;
    }

    // ── Check / uncheck ──
    if (act == QLatin1String("check") || act == QLatin1String("uncheck")) {
        const bool want = act == QLatin1String("check");
        if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            if (!b->isCheckable()) {
                *error = QStringLiteral("%1 is not a checkbox or toggle — click it.").arg(what);
                return std::nullopt;
            }
            if (b->isChecked() == want) {
                p.perform = []() {};
                p.note +=
                    QStringLiteral(" (already %1)").arg(want ? QStringLiteral("checked") : QStringLiteral("unchecked"));
                return p;
            }
            QPointer<QAbstractButton> bg(b);
            p.perform = [bg]() {
                if (bg)
                    bg->click(); // emits clicked + toggled, like a person's click
            };
            return p;
        }
        if (auto* gb = qobject_cast<QGroupBox*>(w); gb && gb->isCheckable()) {
            QPointer<QGroupBox> gg(gb);
            p.perform = [gg, want]() {
                if (gg) {
                    gg->setChecked(want);
                    emit gg->clicked(want);
                }
            };
            return p;
        }
        *error = QStringLiteral("%1 cannot be checked.").arg(what);
        return std::nullopt;
    }

    // ── Values ──
    if (act == QLatin1String("set_value")) {
        const QString v = s.value.isDouble() ? QString::number(s.value.toDouble(), 'g', 15)
                                             : (s.value.isString() ? s.value.toString() : s.text);
        if (auto* dt = qobject_cast<QDateTimeEdit*>(w)) {
            QDateTime when = QDateTime::fromString(v, Qt::ISODate);
            const QDate date = QDate::fromString(v, Qt::ISODate);
            const QTime time = QTime::fromString(v, Qt::ISODate);
            QPointer<QDateTimeEdit> dg(dt);
            if (when.isValid()) {
                p.perform = [dg, when]() {
                    if (dg)
                        dg->setDateTime(when);
                };
            } else if (date.isValid()) {
                p.perform = [dg, date]() {
                    if (dg)
                        dg->setDate(date);
                };
            } else if (time.isValid()) {
                p.perform = [dg, time]() {
                    if (dg)
                        dg->setTime(time);
                };
            } else {
                *error =
                    QStringLiteral("\"%1\" is not an ISO date / time (e.g. 2026-11-08, 14:30, 2026-11-08T14:30:00).")
                        .arg(v);
                return std::nullopt;
            }
            p.note = QStringLiteral("set %1 to %2").arg(what, v);
            return p;
        }
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (auto* is = qobject_cast<QSpinBox*>(w)) {
            if (!ok) {
                *error = QStringLiteral("%1 takes a number.").arg(what);
                return std::nullopt;
            }
            QPointer<QSpinBox> sg(is);
            const int n = qRound(d);
            p.perform = [sg, n]() {
                if (sg)
                    sg->setValue(n);
            };
            p.note = QStringLiteral("set %1 to %2").arg(what).arg(n);
            return p;
        }
        if (auto* ds = qobject_cast<QDoubleSpinBox*>(w)) {
            if (!ok) {
                *error = QStringLiteral("%1 takes a number.").arg(what);
                return std::nullopt;
            }
            QPointer<QDoubleSpinBox> sg(ds);
            p.perform = [sg, d]() {
                if (sg)
                    sg->setValue(d);
            };
            p.note = QStringLiteral("set %1 to %2").arg(what, v);
            return p;
        }
        if (auto* sl = qobject_cast<QAbstractSlider*>(w)) {
            if (!ok) {
                *error = QStringLiteral("%1 takes a number.").arg(what);
                return std::nullopt;
            }
            QPointer<QAbstractSlider> sg(sl);
            const int n = qRound(d);
            p.perform = [sg, n]() {
                if (sg)
                    sg->setValue(n);
            };
            p.note = QStringLiteral("set %1 to %2").arg(what).arg(n);
            return p;
        }
        if (qobject_cast<QLineEdit*>(w) || qobject_cast<QComboBox*>(w)) {
            Step type = s;
            type.action = QStringLiteral("type");
            type.text = v;
            return prepare(type, t, error);
        }
        *error = QStringLiteral("%1 does not hold a value — use click / type / select.").arg(what);
        return std::nullopt;
    }

    // ── Keys ──
    if (act == QLatin1String("press")) {
        const auto chord = parse_key(s.text);
        if (!chord) {
            *error = QStringLiteral("\"%1\" is not a key. Examples: Enter, Escape, Tab, Down, PageDown, Ctrl+S, F5.")
                         .arg(s.text);
            return std::nullopt;
        }
        const KeyChord k = *chord;
        // The clipboard is the user's: pasting it (or copying a secret field
        // into it) would read whatever they copied — a password, the MCP token.
        const bool ctrl_like = k.mods.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier);
        const bool paste =
            (ctrl_like && k.key == Qt::Key_V) || (k.mods.testFlag(Qt::ShiftModifier) && k.key == Qt::Key_Insert);
        if (paste) {
            *error = QStringLiteral("Pasting is not available to agents (the clipboard is the user's). Type the text "
                                    "with action=\"type\".");
            return std::nullopt;
        }
        if (is_secret(w)) {
            *error = QStringLiteral("%1 is a secret field (password / API key / token) — agents never operate it.")
                         .arg(what);
            return std::nullopt;
        }
        p.perform = [wg, k]() {
            if (!wg)
                return;
            // Shortcuts are matched first, as the window system would.
            if ((k.mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) ||
                (k.key >= Qt::Key_F1 && k.key <= Qt::Key_F35)) {
                if (trigger_shortcut(wg, k.seq))
                    return;
            }
            if (k.key == Qt::Key_Escape) {
                if (auto* menu = qobject_cast<QMenu*>(wg.data())) {
                    menu->close();
                    return;
                }
            }
            send_key(wg, k.key, k.mods, k.text);
        };
        p.note = QStringLiteral("press %1%2").arg(s.text, t.ref.isEmpty() ? QString() : QStringLiteral(" on ") + what);
        return p;
    }

    if (act == QLatin1String("scroll")) {
        const int amount = s.amount;
        QAbstractScrollArea* area = qobject_cast<QAbstractScrollArea*>(w);
        if (!area) {
            for (QWidget* p2 = w; p2 && !area; p2 = p2->parentWidget())
                area = qobject_cast<QAbstractScrollArea*>(p2);
        }
        if (!area) {
            for (QAbstractScrollArea* sa : w->findChildren<QAbstractScrollArea*>()) {
                if (sa->isVisible()) {
                    area = sa;
                    break;
                }
            }
        }
        if (!area) {
            *error = QStringLiteral("Nothing scrollable at %1.").arg(what);
            return std::nullopt;
        }
        QPointer<QAbstractScrollArea> ag2(area);
        p.perform = [ag2, amount]() {
            if (!ag2)
                return;
            QScrollBar* sb = ag2->verticalScrollBar();
            sb->setValue(sb->value() + (amount == 0 ? sb->pageStep() : amount * sb->singleStep() * 3));
        };
        return p;
    }

    if (act == QLatin1String("focus")) {
        p.perform = [wg]() {
            if (!wg)
                return;
            if (ads::CDockWidget* d = dock_of(wg)) {
                d->raise();
                d->setAsCurrentTab();
            }
            wg->setFocus(Qt::OtherFocusReason);
        };
        return p;
    }

    if (act == QLatin1String("expand") || act == QLatin1String("collapse")) {
        const bool expand = act == QLatin1String("expand");
        if (auto* cb = qobject_cast<QComboBox*>(w)) {
            QPointer<QComboBox> cg(cb);
            p.perform = [cg, expand]() {
                if (!cg)
                    return;
                if (expand)
                    cg->showPopup();
                else
                    cg->hidePopup();
            };
            return p;
        }
        if (auto* gb = qobject_cast<QGroupBox*>(w); gb && gb->isCheckable()) {
            QPointer<QGroupBox> gg(gb);
            p.perform = [gg, expand]() {
                if (gg)
                    gg->setChecked(expand);
            };
            return p;
        }
        Step click = s;
        click.action = QStringLiteral("click");
        return prepare(click, t, error); // a collapsible section header toggles on click
    }

    // ── Clicks ──
    if (act == QLatin1String("click") || act == QLatin1String("double_click") || act == QLatin1String("right_click") ||
        act == QLatin1String("hover")) {
        const bool dbl = act == QLatin1String("double_click");
        const bool at_point = t.has_hit || s.has_xy;
        const QPoint pos = t.has_hit ? t.hit_pos : s.has_xy ? QPoint(s.x, s.y) : w->rect().center();
        if (at_point && !w->rect().contains(pos)) {
            *error = QStringLiteral("(%1, %2) is outside %3 (%4×%5).")
                         .arg(s.x)
                         .arg(s.y)
                         .arg(what)
                         .arg(w->width())
                         .arg(w->height());
            return std::nullopt;
        }
        if (act == QLatin1String("hover")) {
            p.perform = [wg, pos]() {
                if (!wg)
                    return;
                QEnterEvent enter(QPointF(pos), wg->mapTo(wg->window(), QPointF(pos)), wg->mapToGlobal(QPointF(pos)));
                QCoreApplication::sendEvent(wg, &enter);
                if (!wg)
                    return;
                QMouseEvent move(QEvent::MouseMove, QPointF(pos), wg->mapTo(wg->window(), QPointF(pos)),
                                 wg->mapToGlobal(QPointF(pos)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(wg, &move);
            };
            return p;
        }
        if (act == QLatin1String("right_click")) {
            p.perform = [wg, pos]() {
                if (!wg)
                    return;
                QContextMenuEvent ev(QContextMenuEvent::Mouse, pos, wg->mapToGlobal(pos));
                QCoreApplication::sendEvent(wg, &ev);
            };
            return p;
        }
        if (auto* b = qobject_cast<QAbstractButton*>(w); b && !at_point && !dbl) {
            QPointer<QAbstractButton> bg(b);
            auto* tb = qobject_cast<QToolButton*>(b);
            const bool instant_menu = tb && tb->menu() && tb->popupMode() == QToolButton::InstantPopup;
            p.perform = [bg, instant_menu]() {
                if (!bg)
                    return;
                if (instant_menu) {
                    if (auto* t2 = qobject_cast<QToolButton*>(bg.data()))
                        t2->showMenu();
                    return;
                }
                bg->click();
            };
            return p;
        }
        if (auto* cb = qobject_cast<QComboBox*>(w); cb && !at_point) {
            if (!s.text.isEmpty()) {
                Step sel = s;
                sel.action = QStringLiteral("select");
                return prepare(sel, t, error);
            }
            QPointer<QComboBox> cg(cb);
            p.perform = [cg]() {
                if (cg)
                    cg->showPopup();
            };
            p.note += QStringLiteral(" (its options are listed by ui_read)");
            return p;
        }
        if (auto* gb = qobject_cast<QGroupBox*>(w); gb && gb->isCheckable() && !at_point) {
            Step toggle = s;
            toggle.action = gb->isChecked() ? QStringLiteral("uncheck") : QStringLiteral("check");
            return prepare(toggle, t, error);
        }
        if (auto* l = qobject_cast<QLabel*>(w);
            l && !at_point && l->text().contains(QLatin1String("<a "), Qt::CaseInsensitive)) {
            // A link: activate its first anchor the way QLabel does.
            static const QRegularExpression href(QStringLiteral("href\\s*=\\s*['\"]([^'\"]+)['\"]"),
                                                 QRegularExpression::CaseInsensitiveOption);
            const QString link = href.match(l->text()).captured(1);
            QPointer<QLabel> lg(l);
            p.perform = [lg, link]() {
                if (lg)
                    emit lg->linkActivated(link);
            };
            return p;
        }
        p.perform = [wg, pos, dbl]() {
            if (wg)
                send_mouse(wg, pos, Qt::LeftButton, dbl);
        };
        return p;
    }

    *error =
        QStringLiteral("Unknown action \"%1\". Use one of: %2.").arg(act, known_actions().join(QStringLiteral(", ")));
    return std::nullopt;
}

// ── What changed ────────────────────────────────────────────────────────────

struct UiState {
    QList<QPointer<QWidget>> dialogs;
    QPointer<QWidget> popup;
    QPointer<ads::CDockWidget> focused;
    QStringList panel_ids;
    QHash<QAbstractItemView*, int> rows;
    QHash<QAbstractItemView*, QString> view_names;
    QSet<QString> texts;
    QString value;
};

QWidget* change_scope(QWidget* w) {
    if (!w)
        return active_frame();
    QWidget* top = w->window();
    if (top && top != frame_of(w))
        return top; // a dialog / menu
    if (ads::CDockWidget* d = dock_of(w))
        return d;
    return top;
}

UiState capture(QWidget* scope, QWidget* target) {
    UiState s;
    for (QWidget* d : open_dialogs())
        s.dialogs << d;
    s.popup = open_popup();
    if (WindowFrame* f = active_frame()) {
        s.focused = focused_dock(f);
        for (const PanelInfo& p : panels_of(f))
            s.panel_ids << p.screen_id;
    }
    if (scope) {
        for (QAbstractItemView* v : scope->findChildren<QAbstractItemView*>()) {
            if (!v->isVisible())
                continue;
            s.rows.insert(v, view_row_count(v));
            s.view_names.insert(v, Refs::instance().ref_for(v, RefKind::Element));
        }
        int n = 0;
        for (QLabel* l : scope->findChildren<QLabel*>()) {
            if (!l->isVisible() || is_secret(l))
                continue;
            const QString t = plain_text(l->text()).simplified();
            if (t.isEmpty() || t.size() > 200)
                continue;
            s.texts.insert(t);
            if (++n >= 400)
                break;
        }
    }
    if (target && !is_secret(target))
        s.value = widget_text(target).left(200);
    return s;
}

QStringList diff(const UiState& before, const UiState& after, QWidget* target, const QString& target_label) {
    QStringList out;
    for (const QPointer<QWidget>& d : after.dialogs) {
        if (!d)
            continue;
        bool existed = false;
        for (const QPointer<QWidget>& b : before.dialogs)
            existed = existed || b == d;
        if (existed)
            continue;
        QString line = QStringLiteral("dialog %1 %2 opened")
                           .arg(Refs::instance().ref_for(d, RefKind::Dialog), common::quoted(window_title_of(d)));
        if (auto* mb = qobject_cast<QMessageBox*>(d.data())) {
            line += QStringLiteral(": ") + common::clip(plain_text(mb->text()), 200);
        }
        WalkOptions opt;
        opt.detail = Detail::Brief;
        opt.max_nodes = 30;
        bool truncated = false;
        QStringList controls;
        for (const Node& n : collect_nodes(d, opt, &truncated)) {
            if (!n.ref.isEmpty())
                controls << QStringLiteral("%1 %2 %3").arg(n.ref, n.role, common::quoted(common::clip(n.name, 40)));
        }
        if (!controls.isEmpty())
            line += QStringLiteral(" — controls: ") + controls.join(QStringLiteral(", "));
        out << line;
    }
    int closed = 0;
    for (const QPointer<QWidget>& b : before.dialogs) {
        bool still = false;
        for (const QPointer<QWidget>& a : after.dialogs)
            still = still || (a && a == b);
        if (!still)
            ++closed;
    }
    if (closed > 0)
        out << QStringLiteral("%1 dialog%2 closed").arg(closed).arg(closed == 1 ? QString() : QStringLiteral("s"));
    if (after.popup && after.popup != before.popup) {
        QStringList items;
        if (auto* menu = qobject_cast<QMenu*>(after.popup.data())) {
            for (QAction* a : menu->actions()) {
                if (a->isSeparator() || !a->isVisible())
                    continue;
                items << QStringLiteral("%1 %2").arg(Refs::instance().ref_for(a, RefKind::Element),
                                                     common::quoted(a->text().remove(QLatin1Char('&')).simplified()));
                if (items.size() >= 25)
                    break;
            }
        }
        out << QStringLiteral("menu %1 opened%2")
                   .arg(Refs::instance().ref_for(after.popup.data(), RefKind::Menu),
                        items.isEmpty() ? QString() : QStringLiteral(": ") + items.join(QStringLiteral(", ")));
    } else if (before.popup && !after.popup) {
        out << QStringLiteral("menu closed");
    }
    if (after.focused && after.focused != before.focused) {
        out << QStringLiteral("now on %1").arg(location_label(after.focused));
    }
    for (const QString& id : after.panel_ids) {
        if (!before.panel_ids.contains(id))
            out << QStringLiteral("panel %1 opened").arg(id);
    }
    for (const QString& id : before.panel_ids) {
        if (!after.panel_ids.contains(id))
            out << QStringLiteral("panel %1 closed").arg(id);
    }
    if (target && !is_secret(target)) {
        const QString now = widget_text(target).left(200);
        if (now != before.value)
            out << QStringLiteral("%1 now %2").arg(target_label, common::quoted(common::clip(now, 120)));
    }
    for (auto it = after.rows.constBegin(); it != after.rows.constEnd(); ++it) {
        const int was = before.rows.value(it.key(), -1);
        if (was >= 0 && was != it.value())
            out << QStringLiteral("table %1 rows %2 → %3")
                       .arg(after.view_names.value(it.key()))
                       .arg(was)
                       .arg(it.value());
        else if (was < 0)
            out << QStringLiteral("table %1 appeared (%2 rows)").arg(after.view_names.value(it.key())).arg(it.value());
    }
    QStringList fresh;
    for (const QString& t : after.texts) {
        if (!before.texts.contains(t))
            fresh << common::quoted(common::clip(t, 100));
    }
    std::sort(fresh.begin(), fresh.end());
    if (!fresh.isEmpty()) {
        const int shown = std::min(5, static_cast<int>(fresh.size()));
        out << QStringLiteral("new text: ") + fresh.mid(0, shown).join(QStringLiteral(", ")) +
                   (fresh.size() > shown ? QStringLiteral(" (+%1)").arg(fresh.size() - shown) : QString());
    }
    return out;
}

// ── Web pages ───────────────────────────────────────────────────────────────

using Located = std::function<void(std::optional<Target> target, const QString& error)>;

/// Ask the page what the step's element is now: a listed control found again
/// (and scrolled into view), what is under a point, or the focused element.
void locate_web(const Step& s, const Target& t, Located done) {
    QJsonObject spec;
    if (t.web >= 0) {
        spec = {{QStringLiteral("mode"), QStringLiteral("control")},
                {QStringLiteral("i"), t.web},
                {QStringLiteral("k"), t.web_key},
                {QStringLiteral("reveal"), true}};
    } else {
        spec = web_element_spec(t);
    }
    if (s.action == QLatin1String("select") || s.action == QLatin1String("set_value")) {
        // Find the option now (dry run), so a missing one fails before anything is done.
        spec.insert(QStringLiteral("op"), QStringLiteral("select"));
        spec.insert(QStringLiteral("dry"), true);
        spec.insert(QStringLiteral("text"),
                    s.action == QLatin1String("set_value") && s.value.isString() ? s.value.toString() : s.text);
    }
    const QString what = describe_target(t);
    const QString base = t.ref.section(QLatin1Char(':'), 0, 0);
    QPointer<QWidget> vg(t.web_view);
    run_web_script(
        t.web_view, web_target_script(spec), false, kWebLocateMs,
        [t, vg, what, base, done](const QJsonObject& in, const QString& error) {
            if (!vg) {
                done(std::nullopt, QStringLiteral("%1: the web page closed.").arg(what));
                return;
            }
            if (!error.isEmpty() || in.isEmpty()) {
                done(std::nullopt, QStringLiteral("%1: the web page did not answer (%2) — ui_screenshot(target=\"%3\") "
                                                  "shows it.")
                                       .arg(what, error.isEmpty() ? QStringLiteral("no answer") : error, base));
                return;
            }
            const QString err = in.value(QStringLiteral("error")).toString();
            if (err == QLatin1String("gone")) {
                done(std::nullopt, QStringLiteral("%1 is no longer on the page — it was removed or renamed. Read the "
                                                  "page again: ui_read(ref=\"%2\").")
                                       .arg(what, base));
                return;
            }
            if (err == QLatin1String("several")) {
                done(std::nullopt, QStringLiteral("The page changed: %1 controls are now called %2 — read it again: "
                                                  "ui_read(ref=\"%3\").")
                                       .arg(in.value(QStringLiteral("count")).toInt())
                                       .arg(common::quoted(t.name), base));
                return;
            }
            if (err == QLatin1String("nothing")) {
                done(std::nullopt,
                     QStringLiteral("Nothing on the page at (%1, %2).").arg(t.hit_pos.x()).arg(t.hit_pos.y()));
                return;
            }
            if (err.startsWith(QLatin1String("script")) || err == QLatin1String("mode")) {
                done(std::nullopt, QStringLiteral("%1: the page script failed (%2).").arg(what, err));
                return;
            }
            Target out = t;
            out.web_located = true;
            out.web_info = in;
            if (!in.value(QStringLiteral("none")).toBool()) {
                out.role = in.value(QStringLiteral("r")).toString();
                out.name = in.value(QStringLiteral("n")).toString();
            }
            if (t.web >= 0) {
                // Its centre now (it may have been scrolled into view); no box: hidden.
                out.has_hit = false;
                if (in.contains(QStringLiteral("x"))) {
                    const double z = web_zoom(vg);
                    const QPoint centre(qRound(in.value(QStringLiteral("x")).toDouble() * z),
                                        qRound(in.value(QStringLiteral("y")).toDouble() * z));
                    if (vg->rect().contains(centre)) {
                        out.has_hit = true;
                        out.hit_pos = centre;
                    }
                }
            }
            done(out, QString());
        });
}

// ── The run ─────────────────────────────────────────────────────────────────

struct ActRun : std::enable_shared_from_this<ActRun> {
    QList<Step> steps;
    QString origin;
    CancelCheck cancelled; // the caller gave up (client cancel / watchdog): stop before the next step
    bool snapshot = false;
    Reply reply;
    QStringList report;
    int index = 0;
    QPointer<QWidget> last_scope;

    void next() {
        if (index >= steps.size()) {
            finish(true, {});
            return;
        }
        const Step s = steps[index];
        if (cancelled && cancelled()) {
            finish(false, QStringLiteral("cancelled"));
            return;
        }
        if (auth::InactivityGuard::instance().is_terminal_locked()) {
            finish(false, QStringLiteral("The terminal is locked — the user must unlock it first."));
            return;
        }
        if (!known_actions().contains(s.action)) {
            finish(false, QStringLiteral("Unknown action \"%1\". Use one of: %2.")
                              .arg(s.action, known_actions().join(QStringLiteral(", "))));
            return;
        }
        QString error;
        std::optional<Target> found = resolve_step_target(s, &error);
        if (!found || !aim_at_point(*found, s, &error)) {
            finish(false, error);
            return;
        }
        if (found->web_view) {
            // On a web page: ask the page what the element is first (async).
            auto self = shared_from_this();
            locate_web(s, *found, [self, s](std::optional<Target> located, const QString& err) {
                if (self->cancelled && self->cancelled()) {
                    self->finish(false, QStringLiteral("cancelled"));
                    return;
                }
                if (!located) {
                    self->finish(false, err);
                    return;
                }
                self->proceed(s, *located);
            });
            return;
        }
        proceed(s, *found);
    }

    void proceed(const Step& s, const Target& found) {
        QString error;
        const std::optional<Target> t = found;
        QWidget* host = host_widget(*t);
        if (!host && t->action) {
            // A menu / toolbar item ref: check where it shows, like any widget.
            // Triggering an item of a closed menu skipped the modal check below.
            for (QObject* o : t->action->associatedObjects()) {
                if (auto* aw = qobject_cast<QWidget*>(o); aw && aw->isVisible()) {
                    host = aw;
                    break;
                }
            }
            if (!host) {
                finish(false, QStringLiteral("%1 is not showing — open its menu first (ui_snapshot lists what is "
                                             "there).")
                                  .arg(describe_target(*t)));
                return;
            }
        }
        if (host && is_hidden_from_agents(host)) {
            finish(false, QStringLiteral("That element cannot be operated by an agent."));
            return;
        }
        // A modal dialog blocks everything outside it, for agents as for people.
        if (QWidget* modal = blocking_dialog(); modal && host) {
            QWidget* top = host->window();
            const bool inside = top == modal || modal->isAncestorOf(host);
            const bool in_popup = qobject_cast<QMenu*>(top) != nullptr;
            if (!inside && !in_popup) {
                finish(false, QStringLiteral("%1 is open and blocks the rest of the terminal — answer it first "
                                             "(ui_snapshot(target=\"dialog\")).")
                                  .arg(location_label(modal)));
                return;
            }
        }
        std::optional<Prepared> prepared = prepare(s, *t, &error);
        if (!prepared) {
            finish(false, error);
            return;
        }
        const Risk risk = assess(s, *t);
        auto self = shared_from_this();
        const Target target = *t;
        const Prepared work = *prepared;
        if (risk.money && is_paper_run_origin(origin)) {
            // A PAPER workflow run: refused outright, never offered for approval.
            finish(false, QStringLiteral("Refused: %1 — this workflow runs in PAPER mode, so it cannot place, change "
                                         "or cancel real orders or move money. Arm LIVE in the workflow's SAVE & RUN "
                                         "to allow it.")
                              .arg(work.note));
            return;
        }
        if (risk.risky && !carry_covers(origin, host, risk)) {
            ToolApprovalRequest req;
            req.client = origin;
            // The risk is part of what is approved: a 15-minute grant for "click Yes
            // (deletes data)" must never cover "click Yes" on a live order.
            req.tool = QStringLiteral("ui_act: %1 %2 (%3)").arg(s.action, common::clip(target.name, 60), risk.why);
            req.description = QStringLiteral("%1 in %2 — this %3.").arg(work.note, location_label(host), risk.why);
            QJsonObject preview{{QStringLiteral("action"), s.action},
                                {QStringLiteral("element"), describe_target(target)},
                                {QStringLiteral("where"), location_label(host)}};
            if (!s.text.isEmpty())
                preview.insert(QStringLiteral("text"), common::clip(s.text, 200));
            const QString dctx = dialog_context(host);
            if (!dctx.isEmpty())
                preview.insert(QStringLiteral("dialog says"), common::clip(dctx, 400));
            req.args = preview;
            req.destructive = true;
            req.live_trading = risk.money;
            const QString fingerprint = row_fingerprint(target);
            const bool money = risk.money;
            ask_user_approval(req, [self, s, target, work, fingerprint, money](ToolApprovalVerdict v) {
                if (self->cancelled && self->cancelled()) {
                    self->finish(false, QStringLiteral("cancelled — the step was not run"));
                    return;
                }
                if (v != ToolApprovalVerdict::Approved) {
                    self->finish(false, approval_refusal_text(v, work.note));
                    return;
                }
                // Up to 50 s passed: re-find the element and make sure it is still
                // the one the user approved (a live blotter re-sorts under a row ref,
                // a web page changes under a control ref).
                auto go = [self, s, target, fingerprint, money](std::optional<Target> now, const QString&) {
                    if (!now || host_widget(*now) != host_widget(target) || now->action != target.action ||
                        now->row != target.row || row_fingerprint(*now) != fingerprint ||
                        (target.web_view && (now->role != target.role || now->name != target.name))) {
                        self->finish(false,
                                     QStringLiteral("%1 changed while the user was approving — nothing was done. "
                                                    "Take a fresh ui_snapshot and try again.")
                                         .arg(describe_target(target)));
                        return;
                    }
                    QString err;
                    std::optional<Prepared> fresh = prepare(s, *now, &err);
                    if (!fresh) {
                        self->finish(false, err);
                        return;
                    }
                    arm_carry(self->origin, money);
                    self->execute(s, *now, *fresh, QStringLiteral(" (approved by the user)"));
                };
                QString err;
                std::optional<Target> now = resolve_step_target(s, &err);
                if (now && !aim_at_point(*now, s, &err))
                    now.reset();
                if (now && now->web_view) {
                    locate_web(s, *now, go);
                    return;
                }
                go(now, err);
            });
            return;
        }
        execute(s, target, work, QString());
    }

    void execute(const Step& s, const Target& t, const Prepared& work, const QString& suffix) {
        QWidget* host = host_widget(t);
        QWidget* scope = change_scope(host);
        last_scope = scope;
        const QString target_label = t.ref.isEmpty() ? describe_target(t) : t.ref;
        // Values are read back only for value-bearing steps.
        const bool track_value = s.action == QLatin1String("type") || s.action == QLatin1String("clear") ||
                                 s.action == QLatin1String("select") || s.action == QLatin1String("set_value") ||
                                 s.action == QLatin1String("check") || s.action == QLatin1String("uncheck");
        QPointer<QWidget> value_widget(track_value && !qobject_cast<QAbstractItemView*>(host) ? host : nullptr);
        const UiState before = capture(scope, value_widget);
        // Close a popup the step is not about (a click elsewhere closes it for a person too).
        if (QWidget* popup = open_popup(); popup && host && popup != host->window() && !t.action) {
            popup->close();
        }
        // Posted, never inline: a modal dialog or exec()'d menu opened by this
        // step must not hold the call (see the file comment).
        QTimer::singleShot(0, QCoreApplication::instance(), work.perform);

        auto self = shared_from_this();
        auto clock = std::make_shared<QElapsedTimer>();
        clock->start();
        QPointer<QWidget> scope_guard(scope);
        // What a web page shows now is in the page, not in widgets: say where to read it.
        const QString web_hint =
            t.web_view
                ? QStringLiteral(" → ui_read(ref=\"%1\") shows the page now").arg(t.ref.section(QLatin1Char(':'), 0, 0))
                : QString();
        common::poll_until(
            50, kSettleMs,
            [clock, before]() {
                if (clock->elapsed() < kMinSettleMs)
                    return false;
                // A new dialog or menu settles the step at once.
                if (open_popup() != before.popup.data())
                    return true;
                return open_dialogs().size() != before.dialogs.size();
            },
            [self, before, scope_guard, value_widget, target_label, work, suffix, web_hint](bool) {
                const UiState after = capture(scope_guard, value_widget);
                QStringList changes = diff(before, after, value_widget, target_label);
                QString line = QStringLiteral("%1. %2%3").arg(self->index + 1).arg(work.note, suffix);
                if (!changes.isEmpty())
                    line += QStringLiteral(" → ") + changes.join(QStringLiteral("; "));
                else
                    line += web_hint;
                self->report << line;
                ++self->index;
                self->next();
            });
    }

    void finish(bool ok, const QString& error) {
        QStringList lines = report;
        if (!ok) {
            lines << (steps.size() > 1 ? QStringLiteral("step %1 failed: %2").arg(index + 1).arg(error) : error);
            if (index + 1 < steps.size())
                lines << QStringLiteral("(steps %1–%2 not run)").arg(index + 2).arg(steps.size());
        }
        const BusyState busy = busy_state(last_scope ? last_scope.data() : nullptr);
        if (busy.busy())
            lines
                << QStringLiteral("loading: %1 — ui_wait(until=\"idle\") before reading results").arg(busy.describe());
        if (snapshot && ok) {
            QWidget* scope = open_popup();
            if (!scope)
                scope = blocking_dialog();
            if (!scope)
                scope = last_scope;
            if (scope) {
                WalkOptions opt;
                opt.detail = Detail::Normal;
                opt.max_nodes = 120;
                opt.base_depth = 1;
                bool truncated = false;
                lines << QStringLiteral("— now: %1").arg(location_label(scope));
                for (const Node& n : collect_nodes(scope, opt, &truncated))
                    lines += format_node(n, Detail::Normal);
                if (truncated)
                    lines << QStringLiteral("  … more — ui_snapshot for the rest");
            }
        }
        const QString text = lines.join(QLatin1Char('\n'));
        reply(ok ? ToolResult::ok(text) : ToolResult::fail(text));
    }
};

} // namespace ui_act_detail

using namespace ui_act_detail;

void ui_act(const QJsonObject& args, const QString& origin, CancelCheck cancelled, Reply reply) {
    auto run = std::make_shared<ActRun>();
    run->origin = origin;
    run->cancelled = std::move(cancelled);
    run->reply = std::move(reply);
    run->snapshot = args.value(QStringLiteral("snapshot")).toBool(false);
    const QString default_target = args.value(QStringLiteral("target")).toString();
    const QJsonArray steps = args.value(QStringLiteral("steps")).toArray();
    if (!steps.isEmpty()) {
        if (steps.size() > kMaxSteps) {
            run->reply(ToolResult::fail(QStringLiteral("At most %1 steps per call.").arg(kMaxSteps)));
            return;
        }
        for (const QJsonValue& v : steps) {
            if (!v.isObject()) {
                run->reply(ToolResult::fail(
                    QStringLiteral("Each step is an object like {\"action\":\"click\",\"ref\":\"e5\"}.")));
                return;
            }
            run->steps << parse_step(v.toObject(), default_target);
        }
    } else {
        if (args.value(QStringLiteral("action")).toString().isEmpty()) {
            run->reply(ToolResult::fail(QStringLiteral("Pass `action` (and `ref` or `name`), or `steps`.")));
            return;
        }
        run->steps << parse_step(args, default_target);
    }
    // Refuse a batch with an unknown action before any step runs, not halfway.
    for (int i = 0; i < run->steps.size(); ++i) {
        if (!known_actions().contains(run->steps[i].action)) {
            run->reply(
                ToolResult::fail(QStringLiteral("Step %1: unknown action \"%2\". Use one of: %3. Nothing was run.")
                                     .arg(i + 1)
                                     .arg(run->steps[i].action, known_actions().join(QStringLiteral(", ")))));
            return;
        }
    }
    run->next();
}

} // namespace fincept::mcp::core::ui
