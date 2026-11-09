// UiNavigate.cpp — ui_open, ui_workspace and ui_invoke: open screens (on a
// symbol), arrange panels and windows, save / load layouts, and run the
// terminal's own commands (ActionRegistry). See UiEngine.h.

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "auth/InactivityGuard.h"
#include "core/actions/ActionRegistry.h"
#include "core/components/ComponentCatalog.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/LayoutTemplates.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolRef.h"
#include "mcp/core/CoreApproval.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/UiEngine.h"

#include <QApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <algorithm>
#include <memory>

namespace fincept::mcp::core::ui {

namespace ui_nav_detail {

constexpr int kOpenSettleMs = 450;

QString screen_title(const QString& id) {
    const ComponentMeta meta = ComponentCatalog::instance().by_id(id);
    return meta.is_valid() ? meta.title : DockScreenRouter::title_for_id(id);
}

/// Screen ids the active window can open.
QStringList openable_screen_ids(WindowFrame* f) {
    QStringList ids = f && f->dock_router() ? f->dock_router()->all_screen_ids() : QStringList{};
    QStringList out;
    for (const QString& id : ids) {
        if (!id.contains(QLatin1Char('#')) && !out.contains(id))
            out << id;
    }
    out.sort();
    return out;
}

/// Screen id for what the caller typed: an id, a title, a catalogue tag, or a
/// unique partial match. Empty + `*error` (with suggestions) otherwise.
QString resolve_screen_id(const QString& raw, const QStringList& ids, QString* error) {
    const QString s = raw.trimmed().toLower();
    const QString underscored =
        QString(s).replace(QLatin1Char(' '), QLatin1Char('_')).replace(QLatin1Char('-'), QLatin1Char('_'));
    for (const QString& id : ids) {
        if (id == s || id == underscored)
            return id;
    }
    for (const QString& id : ids) {
        if (screen_title(id).toLower() == s || DockScreenRouter::title_for_id(id).toLower() == s)
            return id;
    }
    for (const QString& id : ids) {
        const ComponentMeta meta = ComponentCatalog::instance().by_id(id);
        for (const QString& tag : meta.tags) {
            if (tag.toLower() == s)
                return id;
        }
    }
    QStringList partial;
    for (const QString& id : ids) {
        if (id.contains(underscored) || screen_title(id).toLower().contains(s))
            partial << id;
    }
    if (partial.size() == 1)
        return partial.first();
    QStringList suggestions = partial.mid(0, 6);
    if (suggestions.isEmpty()) {
        // Nearest by shared words.
        const QStringList words = query_words(QString(s).replace(QLatin1Char('_'), QLatin1Char(' ')));
        for (const QString& id : ids) {
            const QString hay = (id + QLatin1Char(' ') + screen_title(id) + QLatin1Char(' ') +
                                 ComponentCatalog::instance().by_id(id).description)
                                    .toLower();
            for (const QString& w : words) {
                if (w.size() >= 3 && hay.contains(w)) {
                    suggestions << id;
                    break;
                }
            }
            if (suggestions.size() >= 6)
                break;
        }
    }
    *error = suggestions.isEmpty()
                 ? QStringLiteral("No screen \"%1\". ui_open() with no screen lists every screen.").arg(raw)
                 : QStringLiteral("\"%1\" is ambiguous or unknown — did you mean: %2?")
                       .arg(raw, suggestions.join(QStringLiteral(", ")));
    return {};
}

const QStringList& symbol_screens() {
    static const QStringList k = {
        QStringLiteral("markets"),
        QStringLiteral("watchlist"),
        QStringLiteral("news"),
        QStringLiteral("equity_research"),
        QStringLiteral("equity_trading"),
        QStringLiteral("crypto_trading"),
        QStringLiteral("fno"),
        QStringLiteral("portfolio"),
        QStringLiteral("backtesting"),
        QStringLiteral("screener"),
        QStringLiteral("ai_chat"),
        QStringLiteral("akshare"),
        QStringLiteral("asia_markets"),
        QStringLiteral("relationship_map"),
        QStringLiteral("surface_analytics"),
    };
    return k;
}

QString list_screens(WindowFrame* f) {
    const QStringList ids = openable_screen_ids(f);
    QMap<QString, QStringList> by_cat;
    for (const QString& id : ids) {
        const ComponentMeta meta = ComponentCatalog::instance().by_id(id);
        const QString cat = meta.is_valid() && !meta.category.isEmpty() ? meta.category : QStringLiteral("Other");
        QString line = QStringLiteral("  %1 — %2").arg(id, screen_title(id));
        if (meta.is_valid() && !meta.description.isEmpty())
            line += QStringLiteral(": ") + common::clip(meta.description, 70);
        by_cat[cat] << line;
    }
    QStringList lines;
    lines << QStringLiteral("%1 screens. ui_open(screen=<id>, symbol=…, placement=…) opens one.").arg(ids.size());
    for (auto it = by_cat.constBegin(); it != by_cat.constEnd(); ++it) {
        lines << it.key() + QLatin1Char(':');
        lines += it.value();
    }
    QStringList open;
    for (const PanelInfo& p : panels_of(f))
        open << p.screen_id;
    if (!open.isEmpty())
        lines << QStringLiteral("Open now: ") + open.join(QStringLiteral(", "));
    lines << QStringLiteral("Take a symbol: ") + symbol_screens().join(QStringLiteral(", ")) +
                 QStringLiteral(" (crypto_trading: crypto only).");
    return lines.join(QLatin1Char('\n'));
}

ads::DockWidgetArea area_for(const QString& where) {
    if (where == QLatin1String("right"))
        return ads::RightDockWidgetArea;
    if (where == QLatin1String("left"))
        return ads::LeftDockWidgetArea;
    if (where == QLatin1String("top"))
        return ads::TopDockWidgetArea;
    if (where == QLatin1String("bottom"))
        return ads::BottomDockWidgetArea;
    if (where == QLatin1String("tab") || where == QLatin1String("center"))
        return ads::CenterDockWidgetArea;
    return ads::NoDockWidgetArea;
}

/// Brief panel listing after something opened / moved.
QString settled_summary(ads::CDockWidget* dock, const QString& headline) {
    QStringList lines{headline};
    if (dock && dock->isVisible()) {
        WalkOptions opt;
        opt.detail = Detail::Brief;
        opt.max_nodes = 80;
        opt.base_depth = 1;
        bool truncated = false;
        for (const Node& n : collect_nodes(dock, opt, &truncated))
            lines += format_node(n, Detail::Brief);
        if (truncated)
            lines << QStringLiteral("  … more — ui_snapshot(target=\"%1\")")
                         .arg(Refs::instance().ref_for(dock, RefKind::Panel));
    }
    const BusyState busy = busy_state(dock);
    if (busy.busy())
        lines << QStringLiteral("loading: %1 — ui_wait(until=\"idle\") before reading").arg(busy.describe());
    return lines.join(QLatin1Char('\n'));
}

WindowFrame* frame_by_id(int id) {
    for (WindowFrame* f : frames()) {
        if (f->window_id() == id)
            return f;
    }
    return nullptr;
}

/// A window from `window` (id number or w-ref) or the active one.
WindowFrame* window_arg(const QJsonValue& v, QString* error) {
    if (v.isUndefined() || v.isNull() || (v.isString() && v.toString().trimmed().isEmpty())) {
        WindowFrame* f = active_frame();
        if (!f)
            *error = QStringLiteral("No terminal window is open.");
        return f;
    }
    if (v.isDouble()) {
        WindowFrame* f = frame_by_id(v.toInt());
        if (!f)
            *error = QStringLiteral("No window with id %1 (ui_overview lists them).").arg(v.toInt());
        return f;
    }
    const QString s = v.toString().trimmed();
    bool ok = false;
    const int id = s.toInt(&ok);
    if (ok) {
        WindowFrame* f = frame_by_id(id);
        if (!f)
            *error = QStringLiteral("No window with id %1.").arg(id);
        return f;
    }
    if (auto r = Refs::instance().resolve(s)) {
        if (WindowFrame* f = frame_of(r->widget ? r->widget.data() : r->dock.data()))
            return f;
    }
    *error = QStringLiteral("\"%1\" is not a window (pass its id or w-ref from ui_overview).").arg(s);
    return nullptr;
}

ads::CDockWidget* panel_arg(const QString& panel, QString* error) {
    if (panel.trimmed().isEmpty()) {
        if (WindowFrame* f = active_frame()) {
            if (ads::CDockWidget* d = focused_dock(f))
                return d;
        }
        *error = QStringLiteral("Which panel? Pass `panel` (its p-ref or screen id).");
        return nullptr;
    }
    if (auto r = Refs::instance().resolve(panel)) {
        if (r->dock)
            return r->dock;
        if (r->widget) {
            if (ads::CDockWidget* d = dock_of(r->widget))
                return d;
        }
    }
    if (ads::CDockWidget* d = find_open_panel(panel))
        return d;
    *error = QStringLiteral("Panel \"%1\" is not open (ui_overview lists open panels).").arg(panel);
    return nullptr;
}

QString panel_label(ads::CDockWidget* d) {
    return location_label(d);
}

void after_settle(int ms, std::function<void()> fn) {
    QTimer::singleShot(ms, QCoreApplication::instance(), std::move(fn));
}

} // namespace ui_nav_detail

using namespace ui_nav_detail;

// ═════════════════════════════════════════════════════════════════════════════
// ui_open
// ═════════════════════════════════════════════════════════════════════════════

void ui_open(const QJsonObject& args, Reply reply) {
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        reply(ToolResult::fail(QStringLiteral("The terminal is locked — the user must unlock it first.")));
        return;
    }
    WindowFrame* f = active_frame();
    if (!f || !f->dock_router()) {
        reply(ToolResult::fail(QStringLiteral("No terminal window is open.")));
        return;
    }
    if (QWidget* modal = blocking_dialog()) {
        reply(ToolResult::fail(
            QStringLiteral("%1 is open and blocks the terminal — answer it first.").arg(location_label(modal))));
        return;
    }
    const QString screen = args.value(QStringLiteral("screen")).toString().trimmed();
    if (screen.isEmpty()) {
        reply(ToolResult::ok(list_screens(f)));
        return;
    }
    QString error;
    const QString id = resolve_screen_id(screen, openable_screen_ids(f), &error);
    if (id.isEmpty()) {
        reply(ToolResult::fail(error));
        return;
    }
    QString placement = args.value(QStringLiteral("placement")).toString().trimmed().toLower();
    if (placement.isEmpty())
        placement = QStringLiteral("full");
    static const QStringList kPlacements = {
        QStringLiteral("full"),   QStringLiteral("only"),   QStringLiteral("replace"), QStringLiteral("tab"),
        QStringLiteral("split"),  QStringLiteral("right"),  QStringLiteral("left"),    QStringLiteral("top"),
        QStringLiteral("bottom"), QStringLiteral("window"),
    };
    if (!kPlacements.contains(placement)) {
        reply(ToolResult::fail(
            QStringLiteral("placement must be one of: %1.").arg(kPlacements.join(QStringLiteral(", ")))));
        return;
    }
    const QString symbol = args.value(QStringLiteral("symbol")).toString().trimmed();
    const QString type = args.value(QStringLiteral("type")).toString().trimmed().toLower();
    const QString asset_class = type == QLatin1String("crypto") ? QStringLiteral("crypto") : QStringLiteral("equity");
    if (!symbol.isEmpty() && id == QLatin1String("crypto_trading") && asset_class != QLatin1String("crypto")) {
        reply(ToolResult::fail(
            QStringLiteral("crypto_trading takes crypto symbols — pass type=\"crypto\" (e.g. BTC/USDT).")));
        return;
    }

    if (placement == QLatin1String("window")) {
        auto* nf = new WindowFrame(WindowFrame::next_window_id(), nullptr);
        nf->setAttribute(Qt::WA_DeleteOnClose); // keeps WindowRegistry honest (see WindowFrame)
        nf->show();
        f = nf;
        placement = QStringLiteral("full");
    }
    DockScreenRouter* router = f->dock_router();
    ads::CDockWidget* anchor = focused_dock(f);

    if (placement == QLatin1String("full") || placement == QLatin1String("only") ||
        placement == QLatin1String("replace"))
        router->navigate(id, true);
    else if (placement == QLatin1String("tab"))
        router->tab_into(id);
    else
        router->navigate(id, false);

    ads::CDockWidget* dock = router->find_dock_widget(id);
    if (!dock) {
        reply(ToolResult::fail(QStringLiteral("%1 could not be opened.").arg(id)));
        return;
    }
    const ads::DockWidgetArea area = area_for(placement);
    if (area != ads::NoDockWidgetArea && area != ads::CenterDockWidgetArea && anchor && anchor != dock &&
        anchor->dockAreaWidget() && f->dock_manager()) {
        f->dock_manager()->addDockWidget(area, dock, anchor->dockAreaWidget());
        dock->toggleView(true);
        dock->raise();
        dock->setAsCurrentTab();
    }

    QString note;
    if (!symbol.isEmpty()) {
        if (auto* linked = dynamic_cast<IGroupLinked*>(router->screen_widget(id))) {
            SymbolRef ref;
            ref.symbol = symbol;
            ref.asset_class = asset_class;
            ref.exchange = args.value(QStringLiteral("exchange")).toString().trimmed();
            linked->on_group_symbol_changed(ref);
            note = QStringLiteral(" on %1").arg(symbol);
        } else {
            note = QStringLiteral(" (this screen does not take a symbol — type it into the screen's own search box)");
        }
    }
    LOG_INFO("UiTools", QStringLiteral("ui_open %1 (%2)%3")
                            .arg(id, placement, symbol.isEmpty() ? QString() : QLatin1Char(' ') + symbol));
    QPointer<ads::CDockWidget> dg(dock);
    const QString headline = QStringLiteral("Opened %1%2 (%3).").arg(panel_label(dock), note, placement);
    after_settle(kOpenSettleMs, [dg, headline, reply]() {
        if (!dg) {
            reply(ToolResult::fail(QStringLiteral("The panel closed right after opening.")));
            return;
        }
        reply(ToolResult::ok(settled_summary(dg, headline)));
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// ui_workspace
// ═════════════════════════════════════════════════════════════════════════════

void ui_workspace(const QJsonObject& args, const QString& origin, Reply reply) {
    const QString action = args.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (auth::InactivityGuard::instance().is_terminal_locked() && action != QLatin1String("list")) {
        reply(ToolResult::fail(QStringLiteral("The terminal is locked — the user must unlock it first.")));
        return;
    }
    QString error;

    if (action == QLatin1String("list")) {
        QStringList lines;
        for (WindowFrame* f : frames()) {
            QStringList ps;
            for (const PanelInfo& p : panels_of(f))
                ps << QStringLiteral("%1 %2").arg(Refs::instance().ref_for(p.dock.data(), RefKind::Panel), p.screen_id);
            lines << QStringLiteral("window %1 (id %2)%3: %4")
                         .arg(Refs::instance().ref_for(f, RefKind::Window))
                         .arg(f->window_id())
                         .arg(f == active_frame() ? QStringLiteral(" [active]") : QString())
                         .arg(ps.isEmpty() ? QStringLiteral("(no panels)") : ps.join(QStringLiteral(", ")));
        }
        const QString current = layout::WorkspaceShell::current_name();
        lines << QStringLiteral("current layout: %1")
                     .arg(current.isEmpty() ? QStringLiteral("(unsaved)") : common::quoted(current));
        auto saved = LayoutCatalog::instance().list_layouts();
        if (saved.is_ok()) {
            QStringList names;
            for (const auto& e : saved.value()) {
                if (e.kind == QLatin1String("auto") || e.kind == QLatin1String("crash_recovery"))
                    continue;
                names << QStringLiteral("%1 (%2, %3)")
                             .arg(common::quoted(e.name), e.kind,
                                  QDateTime::fromSecsSinceEpoch(e.updated_at_unix)
                                      .toString(QStringLiteral("yyyy-MM-dd")));
            }
            lines << QStringLiteral("saved layouts: ") +
                         (names.isEmpty() ? QStringLiteral("(none)") : names.join(QStringLiteral(", ")));
        }
        QStringList personas;
        for (const auto& p : layout::LayoutTemplates::personas())
            personas << QStringLiteral("%1 (%2)").arg(p.id, p.display_name);
        lines << QStringLiteral("templates: ") + personas.join(QStringLiteral(", "));
        reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
        return;
    }

    if (QWidget* modal = blocking_dialog()) {
        reply(ToolResult::fail(
            QStringLiteral("%1 is open and blocks the terminal — answer it first.").arg(location_label(modal))));
        return;
    }

    // ── Panels ──
    static const QStringList kPanelActions = {
        QStringLiteral("focus"),     QStringLiteral("close"),  QStringLiteral("only"),          QStringLiteral("place"),
        QStringLiteral("duplicate"), QStringLiteral("rename"), QStringLiteral("move_to_window")};
    if (kPanelActions.contains(action)) {
        ads::CDockWidget* dock = panel_arg(args.value(QStringLiteral("panel")).toString(), &error);
        if (!dock) {
            reply(ToolResult::fail(error));
            return;
        }
        WindowFrame* f = frame_of(dock);
        DockScreenRouter* router = f ? f->dock_router() : nullptr;
        const QString id = dock->objectName();
        QPointer<ads::CDockWidget> dg(dock);
        QString headline;
        if (action == QLatin1String("focus")) {
            dock->raise();
            dock->setAsCurrentTab();
            if (QWidget* c = dock->widget())
                c->setFocus(Qt::OtherFocusReason);
            headline = QStringLiteral("Focused %1.").arg(panel_label(dock));
        } else if (action == QLatin1String("close")) {
            const QString label = panel_label(dock);
            dock->toggleView(false);
            reply(ToolResult::ok(QStringLiteral("Closed %1 (ui_open reopens it).").arg(label)));
            return;
        } else if (action == QLatin1String("only")) {
            if (!router) {
                reply(ToolResult::fail(QStringLiteral("That panel has no window.")));
                return;
            }
            router->remove_screen(id);
            headline = QStringLiteral("%1 is now the only panel in its window.").arg(panel_label(dock));
        } else if (action == QLatin1String("place")) {
            const QString where = args.value(QStringLiteral("where")).toString().trimmed().toLower();
            const ads::DockWidgetArea area = area_for(where);
            if (area == ads::NoDockWidgetArea) {
                reply(ToolResult::fail(QStringLiteral("where must be right, left, top, bottom or tab.")));
                return;
            }
            ads::CDockWidget* anchor = nullptr;
            const QString anchor_arg = args.value(QStringLiteral("anchor")).toString();
            if (!anchor_arg.trimmed().isEmpty()) {
                anchor = panel_arg(anchor_arg, &error);
                if (!anchor) {
                    reply(ToolResult::fail(error));
                    return;
                }
            } else if (f) {
                for (const PanelInfo& p : panels_of(f)) {
                    if (p.dock != dock && p.visible) {
                        anchor = p.dock;
                        if (p.focused)
                            break;
                    }
                }
            }
            if (!anchor || anchor == dock || frame_of(anchor) != f || !anchor->dockAreaWidget() || !f->dock_manager()) {
                reply(ToolResult::fail(
                    QStringLiteral("Need another open panel in the same window to place against (`anchor`).")));
                return;
            }
            f->dock_manager()->addDockWidget(area, dock, anchor->dockAreaWidget());
            dock->toggleView(true);
            dock->raise();
            headline = QStringLiteral("Placed %1 %2 %3.")
                           .arg(panel_label(dock),
                                where == QLatin1String("tab") ? QStringLiteral("as a tab with")
                                                              : where + QStringLiteral(" of"),
                                panel_label(anchor));
        } else if (action == QLatin1String("duplicate")) {
            if (!router) {
                reply(ToolResult::fail(QStringLiteral("That panel has no window.")));
                return;
            }
            ads::CDockWidget* dup = router->duplicate_panel(id);
            if (!dup) {
                reply(ToolResult::fail(QStringLiteral("%1 cannot be duplicated.").arg(panel_label(dock))));
                return;
            }
            dg = dup;
            headline = QStringLiteral("Duplicated into %1.").arg(panel_label(dup));
        } else if (action == QLatin1String("rename")) {
            const QString title = args.value(QStringLiteral("title")).toString().trimmed();
            if (title.isEmpty() || title.size() > 60) {
                reply(ToolResult::fail(QStringLiteral("rename needs `title` (1–60 characters).")));
                return;
            }
            dock->setWindowTitle(title);
            reply(ToolResult::ok(QStringLiteral("Renamed to %1.").arg(panel_label(dock))));
            return;
        } else { // move_to_window
            WindowFrame* to = window_arg(args.value(QStringLiteral("to_window")), &error);
            if (!to) {
                reply(ToolResult::fail(error));
                return;
            }
            if (to == f) {
                reply(ToolResult::fail(QStringLiteral("That panel is already in window %1.").arg(to->window_id())));
                return;
            }
            if (!router || !router->move_panel_to_frame(id, to->dock_router())) {
                reply(ToolResult::fail(
                    QStringLiteral("Could not move %1 (is it already open there?).").arg(panel_label(dock))));
                return;
            }
            dg = to->dock_router()->find_dock_widget(id);
            headline = QStringLiteral("Moved %1 to window %2.").arg(id).arg(to->window_id());
        }
        after_settle(250, [dg, headline, reply]() { reply(ToolResult::ok(settled_summary(dg, headline))); });
        return;
    }

    if (action == QLatin1String("tile")) {
        WindowFrame* f = window_arg(args.value(QStringLiteral("window")), &error);
        if (!f || !f->dock_router()) {
            reply(ToolResult::fail(error.isEmpty() ? QStringLiteral("No window.") : error));
            return;
        }
        f->dock_router()->tile_2x2();
        reply(ToolResult::ok(QStringLiteral("Tiled the open panels of window %1 in a grid.").arg(f->window_id())));
        return;
    }

    // ── Layouts ──
    if (action == QLatin1String("save")) {
        QString name = args.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty())
            name = layout::WorkspaceShell::current_name();
        if (name.isEmpty() || name.size() > 80) {
            reply(ToolResult::fail(QStringLiteral("save needs `name` (1–80 characters).")));
            return;
        }
        layout::Workspace previous;
        const layout::Workspace* prev_ptr = nullptr;
        const LayoutId existing = LayoutCatalog::instance().find_by_name(name);
        if (!existing.is_null()) {
            auto r = LayoutCatalog::instance().load_workspace(existing);
            if (r.is_ok()) {
                previous = r.value();
                prev_ptr = &previous;
            }
        }
        const layout::Workspace ws = layout::WorkspaceShell::capture(name, QStringLiteral("user"), prev_ptr);
        auto saved = LayoutCatalog::instance().save_workspace(ws);
        if (saved.is_err()) {
            reply(ToolResult::fail(
                QStringLiteral("Could not save the layout: %1").arg(QString::fromStdString(saved.error()))));
            return;
        }
        reply(ToolResult::ok(QStringLiteral("Saved layout %1%2.")
                                 .arg(common::quoted(name), prev_ptr ? QStringLiteral(" (updated)") : QString())));
        return;
    }
    if (action == QLatin1String("load") || action == QLatin1String("switch") || action == QLatin1String("delete")) {
        const QString name = args.value(QStringLiteral("name")).toString().trimmed();
        LayoutId id = LayoutCatalog::instance().find_by_name(name);
        if (id.is_null())
            id = LayoutId::from_string(name);
        if (name.isEmpty() || id.is_null()) {
            reply(ToolResult::fail(QStringLiteral("No saved layout %1 (ui_workspace(action=\"list\") lists them).")
                                       .arg(common::quoted(name))));
            return;
        }
        if (action == QLatin1String("delete")) {
            ToolApprovalRequest req;
            req.client = origin;
            req.tool = QStringLiteral("ui_workspace: delete layout");
            req.description =
                QStringLiteral("Delete the saved layout %1 — this cannot be undone.").arg(common::quoted(name));
            req.args = QJsonObject{{QStringLiteral("layout"), name}};
            req.destructive = true;
            ask_user_approval(req, [id, name, reply](ToolApprovalVerdict v) {
                if (v != ToolApprovalVerdict::Approved) {
                    reply(ToolResult::fail(approval_refusal_text(v, QStringLiteral("delete layout %1").arg(name))));
                    return;
                }
                auto r = LayoutCatalog::instance().remove_layout(id);
                reply(r.is_ok() ? ToolResult::ok(QStringLiteral("Deleted layout %1.").arg(common::quoted(name)))
                                : ToolResult::fail(QString::fromStdString(r.error())));
            });
            return;
        }
        auto loaded = LayoutCatalog::instance().load_workspace(id);
        if (loaded.is_err()) {
            reply(ToolResult::fail(QStringLiteral("Could not load %1: %2")
                                       .arg(common::quoted(name), QString::fromStdString(loaded.error()))));
            return;
        }
        const layout::Workspace ws = loaded.value();
        // Applied on the next turn of the loop: it rebuilds windows and panels.
        after_settle(0, [ws, name, reply]() {
            const int applied = layout::WorkspaceShell::apply(ws);
            after_settle(kOpenSettleMs, [applied, name, reply]() {
                reply(ToolResult::ok(QStringLiteral("Loaded layout %1 (%2 window%3). Refs from before are stale — "
                                                    "ui_overview shows the new panels.")
                                         .arg(common::quoted(name))
                                         .arg(applied)
                                         .arg(applied == 1 ? QString() : QStringLiteral("s"))));
            });
        });
        return;
    }
    if (action == QLatin1String("template")) {
        const QString persona = args.value(QStringLiteral("persona")).toString().trimmed();
        bool known = false;
        QStringList ids;
        for (const auto& p : layout::LayoutTemplates::personas()) {
            ids << p.id;
            known = known || p.id == persona;
        }
        if (!known) {
            reply(ToolResult::fail(QStringLiteral("persona must be one of: %1.").arg(ids.join(QStringLiteral(", ")))));
            return;
        }
        after_settle(0, [persona, reply]() {
            const layout::Workspace ws = layout::LayoutTemplates::make(persona);
            const int applied = layout::WorkspaceShell::apply(ws);
            after_settle(kOpenSettleMs, [persona, applied, reply]() {
                reply(ToolResult::ok(QStringLiteral("Applied the %1 template (%2 window%3). ui_overview shows it.")
                                         .arg(persona)
                                         .arg(applied)
                                         .arg(applied == 1 ? QString() : QStringLiteral("s"))));
            });
        });
        return;
    }

    // ── Windows ──
    if (action == QLatin1String("new_window")) {
        auto* nf = new WindowFrame(WindowFrame::next_window_id(), nullptr);
        nf->setAttribute(Qt::WA_DeleteOnClose);
        nf->show();
        reply(ToolResult::ok(QStringLiteral("Opened window %1 (id %2). ui_open(screen=…) fills the active window.")
                                 .arg(Refs::instance().ref_for(nf, RefKind::Window))
                                 .arg(nf->window_id())));
        return;
    }
    if (action == QLatin1String("focus_window") || action == QLatin1String("close_window")) {
        WindowFrame* f = window_arg(args.value(QStringLiteral("window")), &error);
        if (!f) {
            reply(ToolResult::fail(error));
            return;
        }
        if (action == QLatin1String("focus_window")) {
            if (f->isMinimized())
                f->showNormal();
            f->raise();
            f->activateWindow();
            reply(ToolResult::ok(QStringLiteral("Window %1 is in front.").arg(f->window_id())));
            return;
        }
        if (frames().size() <= 1) {
            reply(ToolResult::fail(QStringLiteral("That is the only window — closing it would close the terminal.")));
            return;
        }
        // Answer once the window is really gone (it is WA_DeleteOnClose), so the
        // agent's next ui_overview no longer lists it. A window that refuses to
        // close — its closeEvent asking the user something — is reported as such.
        const int wid = f->window_id();
        const QString wref = Refs::instance().ref_for(f, RefKind::Window);
        QPointer<WindowFrame> fg(f);
        after_settle(0, [fg, wid, wref, reply]() {
            if (fg)
                fg->close();
            common::poll_until(
                50, 3000, [fg]() { return fg.isNull(); },
                [fg, wid, wref, reply](bool deleted) {
                    if (deleted || !fg->isVisible()) {
                        reply(ToolResult::ok(QStringLiteral("Closed window %1 (id %2).").arg(wref).arg(wid)));
                        return;
                    }
                    reply(
                        ToolResult::fail(QStringLiteral("Window %1 is still open — it may be asking the user something "
                                                        "(ui_snapshot(target=\"dialog\")).")
                                             .arg(wref)));
                });
        });
        return;
    }

    reply(ToolResult::fail(
        QStringLiteral("Unknown action \"%1\". Use list, focus, close, only, place, duplicate, rename, "
                       "move_to_window, tile, save, load, delete, template, new_window, focus_window "
                       "or close_window.")
            .arg(action)));
}

// ═════════════════════════════════════════════════════════════════════════════
// ui_invoke
// ═════════════════════════════════════════════════════════════════════════════

void ui_invoke(const QJsonObject& args, const QString& origin, Reply reply) {
    const QString wanted = args.value(QStringLiteral("action")).toString().trimmed();
    auto& registry = ActionRegistry::instance();
    if (wanted.isEmpty() || wanted == QLatin1String("list")) {
        QMap<QString, QStringList> by_cat;
        for (const QString& id : registry.all_ids()) {
            const ActionDef* def = registry.find(id);
            if (!def || id.startsWith(QLatin1String("debug.")))
                continue;
            QString line = QStringLiteral("  %1 — %2").arg(id, def->display);
            QStringList params;
            for (const ParameterSlot& p : def->parameter_slots)
                params << p.name + (p.required ? QString() : QStringLiteral("?"));
            if (!params.isEmpty())
                line += QStringLiteral(" (args: %1)").arg(params.join(QStringLiteral(", ")));
            by_cat[def->category.isEmpty() ? QStringLiteral("General") : def->category] << line;
        }
        QStringList lines{
            QStringLiteral("Terminal commands — ui_invoke(action=<id>, args={…}, target=<panel or window>):")};
        for (auto it = by_cat.constBegin(); it != by_cat.constEnd(); ++it) {
            lines << it.key() + QLatin1Char(':');
            lines += it.value();
        }
        reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
        return;
    }
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        reply(ToolResult::fail(QStringLiteral("The terminal is locked — the user must unlock it first.")));
        return;
    }
    // Like ui_open / ui_workspace: a modal dialog (an order confirmation …) must be
    // answered before anything else changes behind it.
    if (QWidget* modal = blocking_dialog()) {
        reply(ToolResult::fail(
            QStringLiteral("%1 is open and blocks the terminal — answer it first.").arg(location_label(modal))));
        return;
    }
    const ActionDef* def = registry.find(wanted);
    if (!def) {
        for (const QString& id : registry.all_ids()) {
            const ActionDef* d = registry.find(id);
            if (d && (d->display.compare(wanted, Qt::CaseInsensitive) == 0 ||
                      d->aliases.contains(wanted, Qt::CaseInsensitive))) {
                def = d;
                break;
            }
        }
    }
    if (!def) {
        reply(
            ToolResult::fail(QStringLiteral("No command \"%1\". ui_invoke(action=\"list\") lists them.").arg(wanted)));
        return;
    }
    if (def->id.startsWith(QLatin1String("debug."))) {
        reply(ToolResult::fail(QStringLiteral("Debug commands are not available to agents.")));
        return;
    }

    // Target: a panel (focused first, so panel.* commands apply to it) or a window.
    QString error;
    WindowFrame* frame = nullptr;
    const QString target = args.value(QStringLiteral("target")).toString().trimmed();
    if (!target.isEmpty()) {
        if (ads::CDockWidget* d = panel_arg(target, &error)) {
            d->raise();
            d->setAsCurrentTab();
            if (QWidget* c = d->widget())
                c->setFocus(Qt::OtherFocusReason);
            frame = frame_of(d);
        } else {
            error.clear();
            frame = window_arg(target, &error);
            if (!frame) {
                reply(ToolResult::fail(error));
                return;
            }
        }
    } else {
        frame = active_frame();
    }
    CommandContext ctx;
    ctx.shell = &TerminalShell::instance();
    ctx.focused_frame = frame;
    ctx.args = args.value(QStringLiteral("args")).toObject().toVariantMap();
    for (const ParameterSlot& p : def->parameter_slots) {
        if (p.required && !ctx.args.contains(p.name) && !p.default_value.isValid()) {
            reply(ToolResult::fail(QStringLiteral("%1 needs args.%2 (%3).").arg(def->id, p.name, p.display)));
            return;
        }
        if (!ctx.args.contains(p.name) && p.default_value.isValid())
            ctx.args.insert(p.name, p.default_value);
    }
    if (def->predicate && !def->predicate(ctx)) {
        reply(ToolResult::fail(QStringLiteral("%1 is not available right now.").arg(def->id)));
        return;
    }

    const QString id = def->id;
    const QString display = def->display;
    // The frame may close before the posted command runs: rebuild the context
    // there from a guarded pointer rather than carrying a raw one across.
    const QPointer<WindowFrame> frame_guard(frame);
    const QVariantMap cmd_args = ctx.args;
    auto run = [id, display, frame_guard, cmd_args, reply]() {
        const int dialogs_before = static_cast<int>(open_dialogs().size());
        auto result = std::make_shared<QString>();
        auto done = std::make_shared<bool>(false);
        QTimer::singleShot(0, QCoreApplication::instance(), [id, frame_guard, cmd_args, result, done]() {
            CommandContext c;
            c.shell = &TerminalShell::instance();
            c.focused_frame = frame_guard.data();
            c.args = cmd_args;
            auto r = ActionRegistry::instance().invoke(id, c);
            *result = r.is_err() ? QString::fromStdString(r.error()) : QString();
            *done = true;
        });
        common::poll_until(
            50, 600,
            [done, dialogs_before]() { return *done || static_cast<int>(open_dialogs().size()) != dialogs_before; },
            [id, display, result, done, reply](bool) {
                if (*done && !result->isEmpty()) {
                    reply(ToolResult::fail(QStringLiteral("%1 failed: %2").arg(id, *result)));
                    return;
                }
                QStringList lines{QStringLiteral("Ran %1 (%2).").arg(id, display)};
                for (QWidget* d : open_dialogs())
                    lines << QStringLiteral("dialog %1 %2 is open")
                                 .arg(Refs::instance().ref_for(d, RefKind::Dialog), common::quoted(window_title_of(d)));
                reply(ToolResult::ok(lines.join(QLatin1Char('\n'))));
            });
    };

    // Locking the terminal, signing out and anything that deletes data ask first.
    static const QRegularExpression risky(
        QStringLiteral(R"(lock|logout|log_out|sign_out|delete|remove|reset|clear|quit|exit)"),
        QRegularExpression::CaseInsensitiveOption);
    if (risky.match(id + QLatin1Char(' ') + display).hasMatch()) {
        ToolApprovalRequest req;
        req.client = origin;
        req.tool = QStringLiteral("ui_invoke: ") + id;
        req.description = QStringLiteral("Run the terminal command \"%1\" (%2).").arg(display, id);
        req.args = args.value(QStringLiteral("args")).toObject();
        req.destructive = true;
        ask_user_approval(req, [run, id, reply](ToolApprovalVerdict v) {
            if (v != ToolApprovalVerdict::Approved) {
                reply(ToolResult::fail(approval_refusal_text(v, QStringLiteral("run %1").arg(id))));
                return;
            }
            run();
        });
        return;
    }
    run();
}

} // namespace fincept::mcp::core::ui
