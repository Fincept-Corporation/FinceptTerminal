// UiActTarget.cpp — ui_act: what a step acts on. Parses a step, finds its element
// (a ref, the words on it, the focused widget or a point), aims a point at what is
// really there, and checks a control can be operated (in view, not covered).

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiActInternal.h"
#include "mcp/core/UiErrors.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QScrollArea>
#include <QTabBar>

namespace fincept::mcp::core::ui::act {

Step parse_step(const QJsonObject& o, const QString& default_target) {
    Step s;
    s.raw = o;
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

bool is_observe_action(const QString& action) {
    return action == QLatin1String("wait") || action == QLatin1String("read");
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
            *error = why.isEmpty() ? err_text(Err::StaleRef, QStringLiteral("%1 is unknown or gone.").arg(s.ref),
                                              QStringLiteral("take a fresh ui_snapshot."))
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
            if (QWidget* f = active_frame()) {
                if (QWidget* dock = focused_dock(f))
                    scopes << dock;
                scopes << f;
            }
        }
        for (QWidget* scope : scopes) {
            const QList<NameMatch> matches = find_by_name(scope, s.name, s.role);
            if (matches.isEmpty())
                continue;
            const NameMatch& best = matches.first();
            // Two different elements that fit equally well — even in the same
            // panel (two "Refresh" buttons) — are not guessed between: the
            // answer lists both refs and the caller picks.
            auto same_element = [](const NameMatch& a, const NameMatch& b) {
                return a.widget == b.widget && a.action == b.action && a.sub == b.sub;
            };
            if (matches.size() > 1 && matches[1].score == best.score && !same_element(matches[1], best)) {
                QStringList cands;
                for (int i = 0; i < matches.size() && i < 5; ++i) {
                    if (matches[i].score != best.score)
                        break;
                    cands << QStringLiteral("%1 %2 %3 in %4")
                                 .arg(matches[i].ref, matches[i].role, common::quoted(common::clip(matches[i].name, 50)),
                                      location_label(matches[i].widget));
                }
                *error = err_text(Err::Ambiguous,
                                  QStringLiteral("\"%1\" matches %2 elements equally: %3.")
                                      .arg(s.name, QString::number(cands.size()), // one call: the agent's
                                           cands.join(QStringLiteral("; "))),     // text is not re-scanned
                                  QStringLiteral("pass one of those refs, or `target` / `role` to narrow it."));
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
        *error = err_text(Err::NotFound,
                          QStringLiteral("Nothing named \"%1\"%2 is showing.")
                              .arg(s.name, s.role.isEmpty() ? QString() : QStringLiteral(" (role %1)").arg(s.role)),
                          QStringLiteral("ui_snapshot(query=\"%1\") lists what is there.").arg(s.name));
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
            else if (QWidget* f = active_frame())
                w = f->focusWidget();
        }
        if (QWidget* popup = open_popup())
            w = popup;
        if (!w && s.action == QLatin1String("scroll")) {
            if (QWidget* f = active_frame())
                w = focused_dock(f);
        }
        if (!w) {
            if (QWidget* modal = blocking_dialog())
                w = modal;
            else if (QWidget* f = active_frame())
                w = f;
        }
        if (!w) {
            *error = err_text(Err::NotFound, QStringLiteral("Nothing has keyboard focus."), QStringLiteral("pass a ref."));
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
        if (host::is_panel(scope)) {
            t.dock = scope;
            t.ref = Refs::instance().ref_for(scope, RefKind::Panel);
        } else {
            t.widget = scope;
        }
        classify_target(t);
        return t;
    }
    *error = err_text(Err::InvalidValue,
                      QStringLiteral("%1 needs `ref`, `name`, or a point (x, y with an optional target).").arg(s.action));
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
        *error = err_text(Err::InvalidValue, QStringLiteral("(%1, %2) is outside %3 (%4×%5).")
                                                 .arg(s.x)
                                                 .arg(s.y)
                                                 .arg(describe_target(t))
                                                 .arg(host->width())
                                                 .arg(host->height()));
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

namespace {

/// A loading overlay / skeleton (the terminal's and any screen's own).
bool is_loading_cover(const QWidget* w) {
    const QByteArray cls = w->metaObject()->className();
    return cls.endsWith("LoadingOverlay") || cls.endsWith("LoadingSkeleton");
}

} // namespace

QString ensure_operable(QWidget* w, const QString& what_el) {
    if (!w || !w->isVisible())
        return {};
    QWidget* top = w->window();
    if (!top || top == w)
        return {};
    // Bring it into view inside every scroll area around it, as a person scrolls
    // to a field before typing into it.
    for (QWidget* p = w->parentWidget(); p && p != top; p = p->parentWidget()) {
        if (auto* sa = qobject_cast<QScrollArea*>(p); sa && sa->widget() && sa->widget()->isAncestorOf(w))
            sa->ensureWidgetVisible(w, 8, 8);
    }
    const QRect mine(w->mapTo(top, QPoint(0, 0)), w->size());
    if (mine.isEmpty())
        return {};
    QWidget* hit = top->childAt(mine.center());
    // Its own centre (or a part of it) is on top — or it is clipped by a
    // viewport, which the widget's own API reaches anyway.
    if (!hit || hit == w || w->isAncestorOf(hit) || hit->isAncestorOf(w))
        return {};
    // Something else is drawn over it: a loading overlay, or a widget that
    // covers it whole (a panel / pane laid over the form). A small badge that
    // overlaps an edge is not a cover.
    QWidget* cover = nullptr;
    for (QWidget* c = hit; c && c != top && !c->isAncestorOf(w); c = c->parentWidget()) {
        const QRect r(c->mapTo(top, QPoint(0, 0)), c->size());
        if (is_loading_cover(c) || r.contains(mine)) {
            cover = c;
            break;
        }
    }
    if (!cover)
        return {};
    QString what = is_loading_cover(cover) ? QStringLiteral("a loading indicator") : QString();
    if (what.isEmpty()) {
        if (auto n = describe_widget(cover, Detail::Brief); n && !n->name.isEmpty())
            what = QStringLiteral("%1 %2").arg(n->role, common::quoted(common::clip(n->name, 40)));
        else
            what = humanize_object_name(cover->objectName());
        if (what.isEmpty())
            what = QStringLiteral("another element");
    }
    return err_text(Err::Obscured, QStringLiteral("%1 is covered by %2, so a person could not reach it now.").arg(what_el, what),
                    is_loading_cover(cover) ? QStringLiteral("ui_wait(until=\"idle\") until it has loaded.")
                                            : QStringLiteral("close what covers it first (ui_snapshot shows it)."));
}

} // namespace fincept::mcp::core::ui::act
