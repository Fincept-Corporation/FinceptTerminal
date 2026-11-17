// src/app/DockScreenRouter_PanelOps.cpp
//
// Panel-level operations: cycle_focused_panel, duplicate_panel,
// show_tab_context_menu, tile_2x2, adopt_panel_instance,
// move_panel_to_frame, tear_off_to_new_frame.
//
// Part of the partial-class split of DockScreenRouter.cpp.

#include "app/DockScreenRouter.h"
#include "app/DockStateFilter.h"
#include "app/WindowFrame.h"
#include "auth/InactivityGuard.h"
#include "core/components/PopularityTracker.h"
#include "core/keys/WindowCycler.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelHandle.h"
#include "core/panel/PanelRegistry.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "core/symbol/SymbolRef.h"
#include "core/window/WindowRegistry.h"
#include "screens/common/IStatefulScreen.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/GroupBadge.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>
#include <algorithm>
#include <vector>

namespace fincept {

namespace {

// ── Pruning closed panels out of a saved ADS layout ──────────────────────────
// The layout XML (CDockManager::saveState) is a tree of
//   QtAdvancedDockingSystem[Containers] > Container[Floating] > Splitter[Count]
//     > (Splitter | Area[Tabs, Current] > Widget[Name, Closed]) ... , Sizes
// read into a small node tree, pruned, and written back.

struct XmlNode {
    QString name;
    QXmlStreamAttributes attrs;
    QString text; // character data (only <Sizes> has any)
    std::vector<XmlNode> children;
};

bool read_xml_node(QXmlStreamReader& r, XmlNode& node) {
    node.name = r.name().toString();
    node.attrs = r.attributes();
    while (!r.atEnd()) {
        const auto token = r.readNext();
        if (token == QXmlStreamReader::StartElement) {
            node.children.emplace_back();
            if (!read_xml_node(r, node.children.back()))
                return false;
        } else if (token == QXmlStreamReader::Characters && !r.isWhitespace()) {
            node.text += r.text();
        } else if (token == QXmlStreamReader::EndElement) {
            return true;
        }
    }
    return false;
}

void write_xml_node(QXmlStreamWriter& w, const XmlNode& node) {
    w.writeStartElement(node.name);
    w.writeAttributes(node.attrs);
    if (!node.text.isEmpty())
        w.writeCharacters(node.text);
    for (const XmlNode& c : node.children)
        write_xml_node(w, c);
    w.writeEndElement();
}

QString xml_attr(const XmlNode& node, const char* name) {
    return node.attrs.value(QLatin1String(name)).toString();
}

void set_xml_attr(XmlNode& node, const char* name, const QString& value) {
    QXmlStreamAttributes out;
    bool replaced = false;
    for (const QXmlStreamAttribute& a : node.attrs) {
        if (a.name() == QLatin1String(name)) {
            out.append(a.qualifiedName().toString(), value);
            replaced = true;
        } else {
            out.append(a);
        }
    }
    if (!replaced)
        out.append(QString::fromLatin1(name), value);
    node.attrs = out;
}

/// Prunes closed widgets below `node`. Returns false when the node is left
/// empty and should be dropped; sets `*malformed` on anything unexpected.
bool prune_closed(XmlNode& node, bool* malformed) {
    if (node.name == QLatin1String("Area")) {
        const QString current = xml_attr(node, "Current");
        QString first_open;
        bool current_open = false;
        int widgets = 0;
        std::vector<XmlNode> kept;
        for (XmlNode& c : node.children) {
            if (c.name == QLatin1String("Widget")) {
                if (xml_attr(c, "Closed") == QLatin1String("1"))
                    continue;
                const QString name = xml_attr(c, "Name");
                if (first_open.isEmpty())
                    first_open = name;
                current_open = current_open || name == current;
                ++widgets;
            }
            kept.push_back(std::move(c));
        }
        node.children = std::move(kept);
        if (widgets == 0)
            return false;
        set_xml_attr(node, "Tabs", QString::number(widgets));
        if (!current_open)
            set_xml_attr(node, "Current", first_open);
        return true;
    }
    if (node.name == QLatin1String("Splitter")) {
        QStringList sizes;
        for (const XmlNode& c : node.children)
            if (c.name == QLatin1String("Sizes"))
                sizes = c.text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        std::vector<XmlNode> kept;
        QStringList kept_sizes;
        int index = 0;
        for (XmlNode& c : node.children) {
            if (c.name == QLatin1String("Sizes"))
                continue; // re-written below
            if (c.name == QLatin1String("Splitter") || c.name == QLatin1String("Area")) {
                if (index >= sizes.size()) {
                    *malformed = true;
                    return true;
                }
                if (prune_closed(c, malformed)) {
                    kept.push_back(std::move(c));
                    kept_sizes << sizes.at(index);
                }
                ++index;
            } else {
                kept.push_back(std::move(c));
            }
        }
        if (index != sizes.size())
            *malformed = true;
        XmlNode sizes_node;
        sizes_node.name = QStringLiteral("Sizes");
        for (const QString& s : kept_sizes)
            sizes_node.text += s + QLatin1Char(' ');
        kept.push_back(std::move(sizes_node));
        node.children = std::move(kept);
        set_xml_attr(node, "Count", QString::number(kept_sizes.size()));
        return !kept_sizes.isEmpty();
    }
    return true;
}

} // namespace

QByteArray DockScreenRouter::save_state_without_closed() {
    if (!manager_)
        return {};
    const QByteArray state = manager_->saveState();
    const bool compressed = !state.startsWith("<?xml");
    const QByteArray xml = compressed ? qUncompress(state) : state;

    XmlNode root;
    bool ok = false;
    if (!xml.isEmpty()) {
        QXmlStreamReader reader(xml);
        while (!reader.atEnd() && reader.readNext() != QXmlStreamReader::StartElement) {
        }
        ok = reader.isStartElement() && read_xml_node(reader, root) && !reader.hasError() &&
             root.name == QLatin1String("QtAdvancedDockingSystem");
    }

    bool malformed = false;
    if (ok) {
        int containers = 0;
        std::vector<XmlNode> kept;
        for (XmlNode& c : root.children) {
            if (c.name != QLatin1String("Container")) {
                kept.push_back(std::move(c));
                continue;
            }
            bool has_content = false;
            for (XmlNode& cc : c.children)
                if (cc.name == QLatin1String("Splitter"))
                    has_content = prune_closed(cc, &malformed) || has_content;
            // A floating window left with nothing open is dropped; the main
            // container is always kept (an empty root splitter is valid).
            if (!has_content && xml_attr(c, "Floating") == QLatin1String("1"))
                continue;
            ++containers;
            kept.push_back(std::move(c));
        }
        root.children = std::move(kept);
        set_xml_attr(root, "Containers", QString::number(containers));
    }
    if (!ok || malformed) {
        LOG_WARN("DockRouter", "Layout state not in the expected form, pruning the live tree instead");
        prune_hidden_panels();
        return manager_->saveState();
    }

    QByteArray out;
    QXmlStreamWriter writer(&out);
    writer.writeStartDocument();
    write_xml_node(writer, root);
    writer.writeEndDocument();
    return compressed ? qCompress(out, 9) : out;
}

void DockScreenRouter::cycle_focused_panel(bool forward) {
    if (!manager_)
        return;
    // Build an ordered list of currently-visible dock widgets — we cycle
    // through those, not the entire registration catalogue.
    QList<ads::CDockWidget*> visible;
    visible.reserve(dock_widgets_.size());
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it) {
        ads::CDockWidget* dw = it.value();
        if (dw && !dw->isClosed())
            visible.append(dw);
    }
    if (visible.size() < 2)
        return;

    // Find the currently-focused dock widget. ADS exposes focusedDockWidget()
    // but it may be null if the focused QWidget isn't inside any dock —
    // fall back to the dock containing current_id_.
    ads::CDockWidget* current = manager_->focusedDockWidget();
    if (!current)
        current = dock_widgets_.value(current_id_, nullptr);

    int idx = visible.indexOf(current);
    if (idx < 0)
        idx = 0;
    else
        idx = (idx + (forward ? 1 : -1) + visible.size()) % visible.size();

    ads::CDockWidget* next = visible.at(idx);
    next->raise();
    next->setAsCurrentTab();
    // The screen itself, not the dock widget's content host around it.
    if (auto* w = screens_.value(next->objectName(), next->widget()))
        w->setFocus(Qt::ShortcutFocusReason);
    current_id_ = next->objectName();
    emit screen_changed(current_id_);
}

namespace dock_router_panel_ops_detail {
/// Screens that open once per window and are never duplicated: every type not
/// listed as multi-instance in DockInstanceGuard.h — among them the screens that
/// hold a live order ticket (a second copy would be two tickets for one account).
bool drp_is_single_instance_screen(const QString& id) {
    return dock_state::is_single_instance_screen(id); // one list, shared with layout restore
}
} // namespace dock_router_panel_ops_detail

ads::CDockWidget* DockScreenRouter::duplicate_panel(const QString& id) {
    if (!manager_)
        return nullptr;
    if (dock_router_panel_ops_detail::drp_is_single_instance_screen(id)) {
        LOG_DEBUG("DockRouter", QString("Panel not duplicated, single-instance screen screen=%1").arg(id));
        return nullptr;
    }

    // Strip any existing #dup<N> suffix so duplicates-of-duplicates all derive
    // from the same original factory — prevents unbounded id drift.
    QString base = id;
    const int hash = id.indexOf('#');
    if (hash > 0)
        base = id.left(hash);

    // We need a factory to produce a fresh widget — the eagerly-registered
    // path gives one instance that we can't clone safely.
    auto fit = factories_.find(base);
    // Special case: the original factory was consumed on first materialise,
    // so factories_ may no longer contain `base`. In that case reconstruct
    // from a registered rebuild helper (we don't have one yet) — fail with a
    // warning so the user sees a concrete reason rather than a silent no-op.
    if (fit == factories_.end()) {
        // Only eager (one-instance) screens reach here. Duplication is not
        // supported for them in this phase.
        LOG_WARN("DockRouter", QString("Panel could not be duplicated, no factory screen=%1").arg(base));
        return nullptr;
    }

    // Allocate a unique suffixed id. Scan existing ids to avoid collisions.
    int n = 2;
    QString dup_id;
    do {
        dup_id = QString("%1#dup%2").arg(base).arg(n++);
    } while (dock_widgets_.contains(dup_id) || factories_.contains(dup_id));

    // Build the duplicate via a fresh factory copy so navigate() can
    // materialize it on-demand through the existing path. Re-install the
    // base factory too so further duplicates remain possible.
    ScreenFactory factory_copy = fit.value();
    factories_[dup_id] = factory_copy;
    factories_[base] = factory_copy;

    // Title: "<Original Title> 2", "<Original Title> 3", ...
    const int dup_index = n - 1;
    // Populate title for the synthetic id so the tab label reads cleanly.
    // title_for_id() is static — we can't extend it at runtime, so we store
    // the custom title via the existing user-rename path.
    save_tab_title(dup_id, QString("%1 %2").arg(title_for_id(base)).arg(dup_index));

    // Create the dock widget. This path also wraps the widget in a
    // GroupBadge container if the source screen implements IGroupLinked,
    // materialises via the factory, and adds the dock widget to the manager.
    auto* dw = create_dock_widget(dup_id);
    if (!dw) {
        LOG_WARN("DockRouter", QString("Panel could not be duplicated, dock widget not created screen=%1").arg(dup_id));
        return nullptr;
    }

    // Place the duplicate next to the original as a tabbed sibling so the
    // user sees it immediately without manually docking. Placed BEFORE the
    // screen is built, so the built screen is never moved (see navigate()).
    auto* src_dw = dock_widgets_.value(base, nullptr);
    {
        const bool prev_suppress = suppress_visibility_materialize_;
        suppress_visibility_materialize_ = true;
        if (src_dw && src_dw->dockAreaWidget())
            manager_->addDockWidgetTabToArea(dw, src_dw->dockAreaWidget());
        else
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        suppress_visibility_materialize_ = prev_suppress;
    }
    dw->setAsCurrentTab();
    dw->raise();
    materialize_screen(dup_id);

    // Copy IStatefulScreen state from source → duplicate so the user doesn't
    // re-type the ticker etc. Source is the inner screen widget, not the
    // group-badge container — screens_ always stores the inner widget.
    auto* src_screen = screens_.value(base, nullptr);
    auto* dup_screen = screens_.value(dup_id, nullptr);
    if (src_screen && dup_screen) {
        auto* src_stateful = dynamic_cast<screens::IStatefulScreen*>(src_screen);
        auto* dup_stateful = dynamic_cast<screens::IStatefulScreen*>(dup_screen);
        if (src_stateful && dup_stateful)
            dup_stateful->restore_state(src_stateful->save_state());
    }

    LOG_INFO("DockRouter", QString("Duplicated '%1' → '%2'").arg(base, dup_id));
    return dw;
}

// ── Phase 12 — tab right-click context menu ──────────────────────────────────

void DockScreenRouter::show_tab_context_menu(const QString& id, const QPoint& global_pos) {
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return;

    QMenu menu;
    menu.setStyleSheet(QString("QMenu{background:%1;color:%2;border:1px solid %3;padding:4px 0;}"
                               "QMenu::item{padding:5px 24px 5px 12px;}"
                               "QMenu::item:selected{background:%4;color:%5;}"
                               "QMenu::item:disabled{color:%6;}"
                               "QMenu::separator{background:%3;height:1px;margin:4px 8px;}")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                                ui::colors::BG_HOVER(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY()));

    // Rename — reuses the inline editor code path. Simpler to pop a modal
    // prompt here; the inline editor requires the label's geometry which is
    // awkward to invoke from a menu click.
    auto* act_rename = menu.addAction("Rename Tab…");
    connect(act_rename, &QAction::triggered, this, [this, id, dw]() {
        bool ok = false;
        const QString name =
            QInputDialog::getText(nullptr, "Rename Tab", "New name:", QLineEdit::Normal, dw->windowTitle(), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            dw->setWindowTitle(name.trimmed());
            save_tab_title(id, name.trimmed());
        }
    });

    // Duplicate — only enabled for factory-registered screens. We can't tell
    // from here reliably without re-running the factory lookup, so we flag the
    // item as enabled and let duplicate_panel no-op with a log warning on
    // unsupported screens.
    auto* act_dup = menu.addAction("Duplicate Panel");
    connect(act_dup, &QAction::triggered, this, [this, id]() { duplicate_panel(id); });
    if (dock_router_panel_ops_detail::drp_is_single_instance_screen(id)) {
        act_dup->setEnabled(false);
        act_dup->setToolTip(dock_instance::holds_order_ticket(id)
                                ? tr("Trading screens open once: two order tickets for one account would be unsafe.")
                                : tr("This screen opens once per window."));
        menu.setToolTipsVisible(true);
    }

    menu.addSeparator();

    // Float or re-dock. ADS's setFloating() has no matching setDocked(); to
    // re-dock a floating widget we close its view and re-open it, which
    // causes ADS to re-attach it to the main container.
    const bool floating = dw->isFloating();
    auto* act_float = menu.addAction(floating ? "Re-dock Panel" : "Float Panel");
    connect(act_float, &QAction::triggered, this, [this, dw, floating]() {
        if (floating) {
            dw->toggleView(false);
            dw->toggleView(true);
            if (manager_ && manager_->dockAreaCount() == 0)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        } else {
            dw->setFloating();
        }
    });

    // Always-on-top — only meaningful when the dock is a top-level floating
    // window. For docked tabs, the flag lives on the WindowFrame; we don't
    // expose it here to avoid confusion.
    if (floating) {
        if (auto* fc = dw->floatingDockContainer()) {
            const bool on = fc->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
            auto* act_top = menu.addAction("Always on Top");
            act_top->setCheckable(true);
            act_top->setChecked(on);
            connect(act_top, &QAction::triggered, this, [fc, on]() {
                Qt::WindowFlags f = fc->windowFlags();
                if (on)
                    f &= ~Qt::WindowStaysOnTopHint;
                else
                    f |= Qt::WindowStaysOnTopHint;
                fc->setWindowFlags(f);
                fc->show();
            });
        }
    }

    menu.addSeparator();

    // Link to Group (Phase 1) — only for screens implementing IGroupLinked.
    // The inner screen is in screens_[id]; the container wrapping happens at
    // dw->widget(), so cast from the stored screen to test the interface.
    if (auto* screen = screens_.value(id, nullptr)) {
        if (auto* linked = dynamic_cast<IGroupLinked*>(screen)) {
            auto* group_menu = menu.addMenu("Link to Group");
            group_menu->setStyleSheet(menu.styleSheet());
            const SymbolGroup current = linked->group();
            auto& registry = SymbolGroupRegistry::instance();

            auto* unlink = group_menu->addAction("(None)");
            unlink->setCheckable(true);
            unlink->setChecked(current == SymbolGroup::None);
            connect(unlink, &QAction::triggered, this, [this, id]() {
                if (auto* s = screens_.value(id, nullptr)) {
                    if (auto* l = dynamic_cast<IGroupLinked*>(s))
                        l->set_group(SymbolGroup::None);
                }
                dock_router_link_defaults::mark_user_configured_links();
                QPointer<ui::GroupBadge> badge = group_badges_.value(id);
                if (badge)
                    badge->set_group_silent(SymbolGroup::None);
            });
            group_menu->addSeparator();

            // Only enabled slots are selectable; disabled ones are surfaced
            // as greyed-out entries so the full inventory stays visible.
            for (SymbolGroup g : all_symbol_groups()) {
                const bool en = registry.enabled(g);
                QString label = QStringLiteral("%1  (%2)").arg(registry.name(g)).arg(symbol_group_letter(g));
                if (!en)
                    label += QStringLiteral("  — disabled");
                auto* a = group_menu->addAction(label);
                a->setCheckable(true);
                a->setChecked(g == current);
                a->setEnabled(en);
                connect(a, &QAction::triggered, this, [this, id, g]() {
                    auto* s = screens_.value(id, nullptr);
                    auto* l = s ? dynamic_cast<IGroupLinked*>(s) : nullptr;
                    if (!l)
                        return;
                    l->set_group(g);
                    dock_router_link_defaults::mark_user_configured_links();
                    QPointer<ui::GroupBadge> badge = group_badges_.value(id);
                    if (badge)
                        badge->set_group_silent(g);
                    // Seed the newly-linked group with the screen's current
                    // symbol (if any) so the other group panels follow it.
                    const SymbolRef own = l->current_symbol();
                    if (own.is_valid())
                        SymbolContext::instance().set_group_symbol(g, own, dynamic_cast<QObject*>(l));
                });
            }
        }
    }

    menu.addSeparator();

    // Phase 5: tear off into a new WindowFrame. Always offered; a refusal is
    // shown to the user (a cancelled monitor picker is not an error).
    auto* act_tear_off = menu.addAction("Tear off into new window");
    connect(act_tear_off, &QAction::triggered, this, [this, id]() {
        QString error;
        if (!tear_off_to_new_frame(id, &error) && !error.isEmpty())
            QMessageBox::warning(qobject_cast<QWidget*>(parent()), "Tear Off Panel", error);
    });

    // Phase 5: move to another open WindowFrame. Submenu lists every
    // other frame by display number; current frame is excluded. Hidden
    // entirely if there's only one frame (nothing to move to).
    {
        const auto frames = WindowRegistry::instance().frames();
        WindowFrame* this_frame = qobject_cast<WindowFrame*>(parent());
        // Count peers other than `this` to decide if the submenu is worth showing.
        int peers = 0;
        for (auto* f : frames)
            if (f && f != this_frame)
                ++peers;
        if (peers > 0) {
            auto* move_menu = menu.addMenu("Move to window");
            move_menu->setStyleSheet(menu.styleSheet());
            int display_num = 1;
            for (auto* f : frames) {
                if (!f)
                    continue;
                if (f == this_frame) {
                    ++display_num;
                    continue;
                }
                const QString label = QString("Window %1").arg(display_num);
                auto* a = move_menu->addAction(label);
                connect(a, &QAction::triggered, this, [this, id, f]() {
                    if (!f || !f->dock_router())
                        return;
                    QString error;
                    if (!move_panel_to_frame(id, f->dock_router(), &error) && !error.isEmpty())
                        QMessageBox::warning(qobject_cast<QWidget*>(parent()), "Move Panel", error);
                });
                ++display_num;
            }
        }
    }

    menu.addSeparator();

    // Copy tab title to clipboard — small but handy for sharing a panel id
    // over chat / for scripting.
    auto* act_copy = menu.addAction("Copy Tab Title");
    connect(act_copy, &QAction::triggered, this, [dw]() { QApplication::clipboard()->setText(dw->windowTitle()); });

    // Close tab.
    auto* act_close = menu.addAction("Close Tab");
    connect(act_close, &QAction::triggered, this, [dw]() { dw->toggleView(false); });

    menu.exec(global_pos);
}

int DockScreenRouter::prune_hidden_panels() {
    if (!manager_)
        return 0;
    // dockWidgetsMap() returns a value copy, so removing while iterating the
    // snapshot is safe; collect first regardless to keep intent obvious.
    QList<ads::CDockWidget*> closed;
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (dw && dw->isClosed())
            closed.append(dw);
    }
    for (auto* dw : closed) {
        // removeDockWidget() detaches the widget, drops it from the manager's
        // map and collapses its now-empty area. It does NOT delete the widget,
        // so the screen and its IStatefulScreen state stay alive in our
        // dock_widgets_ map for re-add on the next navigate().
        manager_->removeDockWidget(dw);
    }
    if (!closed.isEmpty())
        LOG_DEBUG("DockRouter", QString("prune_hidden_panels: dropped %1 closed area(s)").arg(closed.size()));
    return static_cast<int>(closed.size());
}

void DockScreenRouter::tile_2x2() {
    // Public `layout.tile_2x2` command: re-grid every open panel in its current
    // reading order. Delegates to the shared auto-grid implementation.
    retile_grid(nullptr);
}

void DockScreenRouter::retile_grid(ads::CDockWidget* newest) {
    // Detach every open dock widget then re-add with explicit ADS area hints to
    // force the product's auto-grid. Quadrant fill order:
    //   1 panel  → full
    //   2 panels → left | right
    //   3 panels → top row [left | right] + full-width bottom
    //   4 panels → 2x2 (3rd → bottom-left, 4th → bottom-right)
    //   5+       → tab into bottom-right
    if (!manager_)
        return;

    // Collect open panels, excluding `newest` (appended last so it lands in the
    // next/last slot). Existing panels are ordered by current on-screen position
    // (top→bottom, then left→right) so a re-tile preserves their reading order
    // instead of reshuffling them alphabetically by id.
    QList<ads::CDockWidget*> open;
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (dw && !dw->isClosed() && dw != newest)
            open.append(dw);
    }
    std::sort(open.begin(), open.end(), [](ads::CDockWidget* a, ads::CDockWidget* b) {
        const QPoint pa = a->dockAreaWidget() ? a->dockAreaWidget()->mapToGlobal(QPoint(0, 0)) : QPoint();
        const QPoint pb = b->dockAreaWidget() ? b->dockAreaWidget()->mapToGlobal(QPoint(0, 0)) : QPoint();
        if (pa.y() != pb.y())
            return pa.y() < pb.y();
        return pa.x() < pb.x();
    });
    if (newest && !newest->isClosed())
        open.append(newest);

    if (open.isEmpty()) {
        LOG_DEBUG("DockRouter", "retile_grid: no open panels");
        return;
    }
    const int n = open.size();
    LOG_DEBUG("DockRouter", QString("retile_grid: arranging %1 panel(s)").arg(n));

    for (auto* dw : open)
        manager_->removeDockWidget(dw);

    ads::CDockAreaWidget* tl = nullptr;
    ads::CDockAreaWidget* tr = nullptr;
    ads::CDockAreaWidget* bl = nullptr;
    ads::CDockAreaWidget* br = nullptr;
    ads::CDockAreaWidget* bottom = nullptr;

    for (int i = 0; i < n; ++i) {
        ads::CDockWidget* dw = open.at(i);
        if (i == 0) {
            tl = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        } else if (i == 1) {
            tr = manager_->addDockWidget(ads::RightDockWidgetArea, dw, tl);
        } else if (i == 2) {
            if (n == 3)
                bottom = manager_->addDockWidget(ads::BottomDockWidgetArea, dw); // full-width bottom
            else
                bl = manager_->addDockWidget(ads::BottomDockWidgetArea, dw, tl); // bottom-left
        } else if (i == 3) {
            br = manager_->addDockWidget(ads::RightDockWidgetArea, dw, bl); // bottom-right
        } else {
            ads::CDockAreaWidget* target = br ? br : (bottom ? bottom : (bl ? bl : (tr ? tr : tl)));
            if (target)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target);
            else
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        }
        dw->toggleView(true);
    }
    (void)tr;
    (void)bottom;
}

// ── Phase 5: tear-off + cross-frame move ────────────────────────────────────

void DockScreenRouter::adopt_panel_instance(const QString& id, PanelInstanceId instance_id) {
    if (id.isEmpty() || instance_id.is_null())
        return;

    instance_ids_.insert(id, instance_id);

    // Update the existing PanelHandle so PanelRegistry::find_by_frame
    // returns the right results for this frame's owning WindowFrame.
    if (auto* handle = PanelRegistry::instance().find(instance_id)) {
        WindowId frame_id;
        if (auto* frame = qobject_cast<WindowFrame*>(parent()))
            frame_id = frame->frame_uuid();
        handle->set_frame_id(frame_id);
        // dock_widget pointer will be repointed when create_dock_widget
        // runs in this router and assigns the freshly-created widget.
        handle->set_dock_widget(nullptr);
        handle->set_state(PanelHandle::State::Spawning);
    }
}

bool DockScreenRouter::move_panel_to_frame(const QString& id, DockScreenRouter* target, QString* error) {
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };
    if (!target || target == this) {
        LOG_DEBUG("DockRouter", QString("move_panel_to_frame('%1'): target invalid (null=%2 same=%3)")
                                    .arg(id)
                                    .arg(target == nullptr)
                                    .arg(target == this));
        return fail(target ? QString() : QString("That window is no longer open."));
    }
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw) {
        LOG_WARN("DockRouter", QString("Panel move failed, no dock widget screen=%1").arg(id));
        return fail(QString("That panel is no longer open."));
    }
    if (is_busy(id) || target->is_busy(id)) {
        LOG_DEBUG("DockRouter", QString("Panel move refused, the panel is still opening screen=%1").arg(id));
        return fail(QString("'%1' is still opening. Try again in a moment.").arg(dw->windowTitle()));
    }
    const QString title = dw->windowTitle();
    // Only a panel that is OPEN in the target blocks the move. The target
    // normally also owns a closed shell for every screen — pre-registered by
    // ensure_all_registered() for layout restore, or opened and closed there
    // earlier (prune_hidden_panels keeps those) — and that shell is reused.
    ads::CDockWidget* target_dw = target->dock_widgets_.value(id, nullptr);
    if (target_dw && !target_dw->isClosed()) {
        LOG_WARN("DockRouter", QString("Panel move refused, already open in the target window screen=%1").arg(id));
        return fail(QString("'%1' is already open in that window.").arg(title));
    }
    if ((!factories_.contains(id) && !screens_.contains(id)) ||
        (!factories_.contains(id) && !target->factories_.contains(id) && !target->screens_.contains(id))) {
        // No way to build the screen on the target.
        LOG_WARN("DockRouter", QString("Panel move failed, no factory for the target window screen=%1").arg(id));
        return fail(QString("'%1' cannot be moved to another window.").arg(title));
    }

    LOG_DEBUG("DockRouter", QString("Moving panel '%1' to another frame").arg(id));

    // 1. Give the target a dock-widget shell for `id` (reusing its closed
    //    one when present) so the panel lives on under the TARGET's instance
    //    id. The target keeps its own factory when it has one — a factory
    //    may capture its frame (file_manager) — and gets a copy of ours only
    //    when it has none (e.g. a "#dup" id).
    if (!target->factories_.contains(id) && !target->screens_.contains(id))
        target->factories_.insert(id, factories_.value(id));
    const bool target_had_screen = target->screens_.contains(id);
    if (!target_dw)
        target_dw = target->create_dock_widget(id);
    const PanelInstanceId target_uuid = target->instance_ids_.value(id);

    // 2. Flush the panel's state and write it SYNCHRONOUSLY under the
    //    target's instance id, so the restore in step 4 reads this row.
    auto* screen = screens_.value(id, nullptr);
    if (auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen)) {
        stateful->flush_pending_state();
        if (!target_uuid.is_null())
            ScreenStateManager::instance().save_now_by_uuid_sync(stateful, target_uuid.to_string());
        else
            ScreenStateManager::instance().save_now_sync(stateful);
    }

    // 3. Tear down the panel on this router. removeDockWidget unregisters
    //    from the manager; deleteLater destroys the widget. We purposely
    //    skip our own save_screen_state() because we already did the
    //    UUID-keyed save above — the visibility-changed handler would
    //    otherwise try a second save against a half-torn-down widget.
    const PanelInstanceId panel_uuid = instance_ids_.value(id);
    manager_->removeDockWidget(dw);
    dw->deleteLater();
    dock_widgets_.remove(id);
    // The screen is about to be destroyed: drop its raw IGroupLinked* too, or the
    // next SymbolContext broadcast would call group() on a dangling pointer in
    // on_group_symbol_changed_external().
    group_linked_.remove(id);
    group_badges_.remove(id);
    if (auto* sw = screens_.take(id))
        sw->deleteLater();
    // Keep the factory so the screen can still be opened in this window
    // (materialize_screen keeps factories for the same reason). Only a
    // synthetic duplicate id goes away with its panel.
    if (id.contains(QLatin1String("#dup")))
        factories_.remove(id);
    instance_ids_.remove(id);
    // The panel continues under the target's instance id: drop this
    // window's handle so a later re-open here registers a fresh one.
    if (!panel_uuid.is_null() && panel_uuid != target_uuid)
        PanelRegistry::instance().unregister_panel(panel_uuid);

    // 4. Show it on the target. navigate() places the shell, materialises
    //    the screen and restore_screen_state reads the row written in step 2.
    //    A screen the target had already built (opened and closed there
    //    earlier) is only re-shown by navigate(), so load the state into it.
    target->navigate(id);
    if (target_had_screen)
        target->restore_screen_state(id);

    LOG_DEBUG("DockRouter", QString("Moved panel '%1' (uuid=%2) to target frame").arg(id, target_uuid.to_string()));
    return true;
}

bool DockScreenRouter::tear_off_to_new_frame(const QString& id, QString* error) {
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw) {
        LOG_WARN("DockRouter", QString("Panel tear-off failed, no such panel screen=%1").arg(id));
        if (error)
            *error = QString("That panel is no longer open.");
        return false;
    }
    if (!factories_.contains(id) && !screens_.contains(id)) {
        LOG_WARN("DockRouter", QString("Panel tear-off failed, no factory screen=%1").arg(id));
        if (error)
            *error = QString("'%1' cannot be moved to another window.").arg(dw->windowTitle());
        return false;
    }

    // Spawn a new frame on the next monitor. Reuses WindowCycler's smart
    // placement so tear-offs land somewhere sensible (decision 5.6).
    const auto frames_before = WindowRegistry::instance().frames();
    WindowCycler::instance().new_window_on_next_monitor();

    // Find the freshly-created frame. WindowCycler::new_window_on_next_monitor
    // doesn't return a pointer, so diff the registry against the snapshot taken
    // above. The monitor picker can be cancelled, in which case no frame is
    // created — blindly taking the last registered frame would then move the
    // panel into some other, unrelated window.
    WindowFrame* new_frame = nullptr;
    for (WindowFrame* f : WindowRegistry::instance().frames()) {
        if (f && !frames_before.contains(f))
            new_frame = f; // frames() is sorted by window_id; keep the highest new one
    }
    if (!new_frame) {
        LOG_DEBUG("DockRouter", QString("tear_off_to_new_frame('%1'): no new window was created (cancelled?)").arg(id));
        return false;
    }
    if (!new_frame->dock_router()) {
        LOG_ERROR("DockRouter", QString("Panel tear-off failed, the new window has no router screen=%1").arg(id));
        if (error)
            *error = QString("Could not set up the new window.");
        return false;
    }

    return move_panel_to_frame(id, new_frame->dock_router(), error);
}
} // namespace fincept
