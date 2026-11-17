// UiActDiff.cpp — what a step changed: dialogs and menus that opened or closed,
// the element's new value, row counts of tables in the same panel, new texts and
// toasts, a different focused panel.

#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiActInternal.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>

#include <algorithm>

namespace fincept::mcp::core::ui::act {

QWidget* change_scope(QWidget* w) {
    if (!w)
        return active_frame();
    QWidget* top = w->window();
    if (top && top != frame_of(w))
        return top; // a dialog / menu
    if (QWidget* d = dock_of(w))
        return d;
    return top;
}

namespace {

/// Short-lived notifications (toasts) are top-level windows of their own.
bool is_toast(const QWidget* w) {
    return QByteArray(w->metaObject()->className()).contains("Toast");
}

} // namespace

UiState capture(QWidget* scope, QWidget* target) {
    UiState s;
    for (QWidget* d : open_dialogs())
        s.dialogs << d;
    s.popup = open_popup();
    if (QWidget* f = active_frame()) {
        s.focused = focused_dock(f);
        for (const PanelInfo& p : panels_of(f))
            s.panel_ids << p.screen_id;
    }
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (!top->isVisible() || !is_toast(top) || is_hidden_from_agents(top))
            continue;
        QStringList parts;
        for (QLabel* l : top->findChildren<QLabel*>()) {
            const QString t = plain_text(l->text()).simplified();
            if (l->isVisible() && !t.isEmpty())
                parts << t;
        }
        if (!parts.isEmpty())
            s.toasts.insert(common::clip(parts.join(QStringLiteral(" · ")), 160));
    }
    if (scope) {
        for (QAbstractItemView* v : scope->findChildren<QAbstractItemView*>()) {
            if (!v->isVisible())
                continue;
            // A preview count (trees stop at kPreviewTreeRows): this runs twice per step.
            s.views.append({QPointer<QAbstractItemView>(v), view_row_count(v, nullptr, kPreviewTreeRows),
                            Refs::instance().ref_for(v, RefKind::Element)});
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
    // Tables are matched by the live object (a QPointer), never by an address a
    // new table may have reused.
    for (const ViewCount& now : after.views) {
        if (!now.view)
            continue;
        int was = -1;
        for (const ViewCount& b : before.views) {
            if (b.view && b.view == now.view) {
                was = b.rows;
                break;
            }
        }
        if (was >= 0 && was != now.rows)
            out << QStringLiteral("table %1 rows %2 → %3").arg(now.ref).arg(was).arg(now.rows);
        else if (was < 0)
            out << QStringLiteral("table %1 appeared (%2 rows)").arg(now.ref).arg(now.rows);
    }
    for (const QString& t : after.toasts) {
        if (!before.toasts.contains(t))
            out << QStringLiteral("toast %1").arg(common::quoted(t));
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

} // namespace fincept::mcp::core::ui::act
