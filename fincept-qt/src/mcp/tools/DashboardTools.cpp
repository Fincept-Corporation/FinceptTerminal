// DashboardTools.cpp — Tools that drive the Dashboard tab.
//
// 24 tools in category "dashboard", grouped:
//   • Discovery       (4)  — WidgetRegistry catalog + DashboardTemplates
//   • Layout state    (3)  — read / load / save GridLayout via DashboardLayoutRepository
//   • Layout mutation (6)  — apply_template / clear / add / remove / move / resize
//   • Widget ops      (4)  — per-instance config get/set, refresh, row height
//   • View / toolbar  (4)  — refresh all, toggle pulse, toggle compact, stats
//   • Ticker bar      (3)  — get / set symbols, force refresh
//
// All targets (DashboardCanvas, BaseWidget, repository) are UI-thread only.
// Every tool dispatches its body onto qApp's thread via
// AsyncDispatch::callback_to_promise.

#include "mcp/tools/DashboardTools.h"

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "core/logging/Logger.h"
#include "core/window/WindowRegistry.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "screens/dashboard/DashboardScreen.h"
#include "screens/dashboard/TickerBar.h"
#include "screens/dashboard/canvas/DashboardCanvas.h"
#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "screens/dashboard/canvas/GridLayout.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "screens/dashboard/canvas/WidgetTile.h"
#include "screens/dashboard/widgets/BaseWidget.h"
#include "storage/repositories/DashboardLayoutRepository.h"
#include "storage/repositories/SettingsRepository.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace fincept::mcp::tools {

namespace {

static constexpr const char* TAG = "DashboardTools";

// Dashboard ops are fast (no I/O); 15s default covers worst-case template apply.
static constexpr int kDefaultTimeoutMs = 15000;

static constexpr const char* kTickerSymbolsKey = "ticker_bar_symbols";

// ── Helpers: locate the live DashboardScreen / canvas / ticker ─────────

screens::DashboardScreen* find_dashboard_screen() {
    // Walk every WindowFrame's DockScreenRouter looking for the "dashboard"
    // dock widget. Whichever frame has it gives us the live screen.
    for (auto* w : WindowRegistry::instance().frames()) {
        if (!w || !w->dock_router()) continue;
        auto* dw = w->dock_router()->find_dock_widget("dashboard");
        if (!dw) continue;
        auto* widget = dw->widget();
        if (!widget) continue;
        if (auto* ds = qobject_cast<screens::DashboardScreen*>(widget))
            return ds;
        // The dashboard may be wrapped in a group-badge container — descend.
        if (auto* ds = widget->findChild<screens::DashboardScreen*>())
            return ds;
    }
    return nullptr;
}

screens::DashboardCanvas* find_dashboard_canvas() {
    auto* ds = find_dashboard_screen();
    return ds ? ds->findChild<screens::DashboardCanvas*>() : nullptr;
}

screens::TickerBar* find_ticker_bar() {
    auto* ds = find_dashboard_screen();
    return ds ? ds->findChild<screens::TickerBar*>() : nullptr;
}

screens::WidgetTile* find_tile_by_instance_id(screens::DashboardCanvas* canvas,
                                                const QString& instance_id) {
    if (!canvas) return nullptr;
    for (auto* tile : canvas->findChildren<screens::WidgetTile*>()) {
        if (tile && tile->instance_id() == instance_id)
            return tile;
    }
    return nullptr;
}

// ── JSON serialisers ───────────────────────────────────────────────────

QJsonObject cell_to_json(const screens::GridCell& c) {
    return QJsonObject{{"x", c.x}, {"y", c.y}, {"w", c.w}, {"h", c.h},
                        {"min_w", c.min_w}, {"min_h", c.min_h}};
}

screens::GridCell json_to_cell(const QJsonObject& j, const screens::GridCell& fallback = {}) {
    screens::GridCell c = fallback;
    if (j.contains("x"))     c.x = j["x"].toInt();
    if (j.contains("y"))     c.y = j["y"].toInt();
    if (j.contains("w"))     c.w = j["w"].toInt();
    if (j.contains("h"))     c.h = j["h"].toInt();
    if (j.contains("min_w")) c.min_w = j["min_w"].toInt();
    if (j.contains("min_h")) c.min_h = j["min_h"].toInt();
    return c;
}

QJsonObject grid_item_to_json(const screens::GridItem& it) {
    return QJsonObject{
        {"id", it.id},
        {"instance_id", it.instance_id},
        {"cell", cell_to_json(it.cell)},
        {"is_static", it.is_static},
        {"config", it.config},
    };
}

QJsonObject grid_layout_to_json(const screens::GridLayout& L) {
    QJsonArray items;
    for (const auto& it : L.items) items.append(grid_item_to_json(it));
    return QJsonObject{
        {"items", items},
        {"cols", L.cols},
        {"row_h", L.row_h},
        {"margin", L.margin},
    };
}

QJsonObject widget_meta_to_json(const screens::WidgetMeta& m) {
    return QJsonObject{
        {"type_id", m.type_id},
        {"display_name", m.display_name},
        {"category", m.category},
        {"description", m.description},
        {"default_w", m.default_w},
        {"default_h", m.default_h},
        {"min_w", m.min_w},
        {"min_h", m.min_h},
    };
}

QJsonObject template_to_json(const screens::DashboardTemplate& t) {
    QJsonArray items;
    for (const auto& it : t.items) items.append(grid_item_to_json(it));
    return QJsonObject{
        {"id", t.id},
        {"display_name", t.display_name},
        {"description", t.description},
        {"items", items},
    };
}

// Marshal a synchronous body onto the UI thread.
template <typename BodyFn>
void run_on_ui(ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise, BodyFn&& body) {
    AsyncDispatch::callback_to_promise(qApp, std::move(ctx), promise, std::forward<BodyFn>(body));
}

} // namespace

std::vector<ToolDef> get_dashboard_tools() {
    std::vector<ToolDef> tools;

    // ═══════════════════════════════════════════════════════════════════
    //  DISCOVERY (4)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_dashboard_widgets";
        t.description = "List every registered dashboard widget type with metadata (display name, category, description, default size, min size).";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QJsonArray arr;
                for (const auto& m : screens::WidgetRegistry::instance().all())
                    arr.append(widget_meta_to_json(m));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_dashboard_widgets_by_category";
        t.description = "List dashboard widgets in a single category (Markets, Research, Portfolio, Trading, Tools, Geopolitics).";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("category", "Category name").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                QJsonArray arr;
                for (const auto& m : screens::WidgetRegistry::instance().by_category(args["category"].toString()))
                    arr.append(widget_meta_to_json(m));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_dashboard_widget_meta";
        t.description = "Get a single widget's metadata by type_id.";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("type_id", "Widget type id (use list_dashboard_widgets)").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const auto* m = screens::WidgetRegistry::instance().find(args["type_id"].toString());
                if (!m) { resolve(ToolResult::fail("Unknown widget type_id")); return; }
                resolve(ToolResult::ok_data(widget_meta_to_json(*m)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_dashboard_templates";
        t.description = "List built-in dashboard templates (portfolio_manager, hedge_fund, crypto_trader, equity_trader, macro_economist, geopolitics).";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QJsonArray arr;
                for (const auto& tpl : screens::all_dashboard_templates())
                    arr.append(template_to_json(tpl));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  LAYOUT STATE (3)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "get_current_dashboard_layout";
        t.description = "Get the live dashboard canvas layout (items + cols + row_h + margin).";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                resolve(ToolResult::ok_data(grid_layout_to_json(canvas->current_layout())));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "load_dashboard_layout";
        t.description = "Load a saved layout profile and apply it to the live canvas.";
        t.category = "dashboard";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("profile_name", "Profile name (default = 'default')").default_str("default").length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                auto r = DashboardLayoutRepository::instance().load_layout(args["profile_name"].toString("default"));
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                canvas->load_layout(r.value());
                resolve(ToolResult::ok("Layout loaded", QJsonObject{
                    {"profile_name", args["profile_name"].toString("default")},
                    {"widget_count", static_cast<int>(r.value().items.size())},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "save_dashboard_layout";
        t.description = "Save the current live canvas layout under a profile name (upserts).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("profile_name", "Profile name").default_str("default").length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                auto layout = canvas->current_layout();
                auto r = DashboardLayoutRepository::instance().save_layout(layout, args["profile_name"].toString("default"));
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Layout saved", QJsonObject{
                    {"profile_name", args["profile_name"].toString("default")},
                    {"widget_count", static_cast<int>(layout.items.size())},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  LAYOUT MUTATION (6)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "apply_dashboard_template";
        t.description = "Replace the current dashboard with a built-in template by id (e.g. 'crypto_trader').";
        t.category = "dashboard";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("template_id", "Template id (use list_dashboard_templates)").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                canvas->apply_template(args["template_id"].toString());
                resolve(ToolResult::ok("Template applied", QJsonObject{
                    {"template_id", args["template_id"].toString()},
                    {"widget_count", canvas->tile_count()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "clear_dashboard_layout";
        t.description = "Delete all widget instances for a profile (does NOT touch the live canvas — call load_dashboard_layout after to refresh).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("profile_name", "Profile name").default_str("default").length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto r = DashboardLayoutRepository::instance().clear_layout(args["profile_name"].toString("default"));
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }
                resolve(ToolResult::ok("Layout cleared",
                                        QJsonObject{{"profile_name", args["profile_name"].toString("default")}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "add_dashboard_widget";
        t.description = "Add a widget to the live canvas. Optional cell position and per-instance config (e.g. {symbol: 'AAPL'} for stock_quote). Returns the new instance_id.";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("type_id", "Widget type id (use list_dashboard_widgets)").required().length(1, 64)
            .object("config", "Optional per-instance config object (e.g. {symbol: 'AAPL'})")
            .integer("x", "Grid column (-1 = auto-place)").default_int(-1).min(-1)
            .integer("y", "Grid row (-1 = auto-place)").default_int(-1).min(-1)
            .integer("w", "Width in columns (0 = use widget default)").default_int(0).min(0)
            .integer("h", "Height in rows (0 = use widget default)").default_int(0).min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }

                const QString type_id = args["type_id"].toString();
                if (!screens::WidgetRegistry::instance().find(type_id)) {
                    resolve(ToolResult::fail("Unknown widget type_id: " + type_id));
                    return;
                }

                // Snapshot existing instance_ids so we can identify the new one.
                QSet<QString> before;
                for (const auto& it : canvas->current_layout().items)
                    before.insert(it.instance_id);

                canvas->add_widget(type_id);

                // Diff to find the newly-added instance.
                QString new_id;
                auto after = canvas->current_layout();
                for (const auto& it : after.items) {
                    if (!before.contains(it.instance_id)) { new_id = it.instance_id; break; }
                }
                if (new_id.isEmpty()) {
                    resolve(ToolResult::fail("Widget add failed (no new instance detected)"));
                    return;
                }

                // Apply optional cell overrides + config by mutating layout, then reload.
                const QJsonObject cfg = args["config"].toObject();
                const int x = args["x"].toInt(-1);
                const int y = args["y"].toInt(-1);
                const int w = args["w"].toInt(0);
                const int h = args["h"].toInt(0);
                const bool cell_override = (x >= 0 || y >= 0 || w > 0 || h > 0);

                if (cell_override || !cfg.isEmpty()) {
                    auto layout = canvas->current_layout();
                    for (auto& it : layout.items) {
                        if (it.instance_id != new_id) continue;
                        if (x >= 0) it.cell.x = x;
                        if (y >= 0) it.cell.y = y;
                        if (w > 0)  it.cell.w = std::max(w, it.cell.min_w);
                        if (h > 0)  it.cell.h = std::max(h, it.cell.min_h);
                        if (!cfg.isEmpty()) it.config = cfg;
                        break;
                    }
                    canvas->load_layout(layout);

                    // After load_layout, apply config to the live widget.
                    if (!cfg.isEmpty()) {
                        if (auto* tile = find_tile_by_instance_id(canvas, new_id)) {
                            if (auto* w = tile->content_widget())
                                w->apply_config(cfg);
                        }
                    }
                }

                resolve(ToolResult::ok("Widget added", QJsonObject{
                    {"instance_id", new_id},
                    {"type_id", type_id},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "remove_dashboard_widget";
        t.description = "Remove a widget from the live canvas by instance_id.";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("instance_id", "Widget instance UUID (from get_current_dashboard_layout)").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                const QString id = args["instance_id"].toString();
                canvas->remove_widget(id);
                resolve(ToolResult::ok("Widget removed", QJsonObject{{"instance_id", id}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "move_dashboard_widget";
        t.description = "Move a widget to new grid coordinates (x, y).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("instance_id", "Widget instance UUID").required().length(1, 64)
            .integer("x", "New grid column").required().min(0)
            .integer("y", "New grid row").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                const QString id = args["instance_id"].toString();
                auto layout = canvas->current_layout();
                bool found = false;
                for (auto& it : layout.items) {
                    if (it.instance_id != id) continue;
                    it.cell.x = args["x"].toInt();
                    it.cell.y = args["y"].toInt();
                    found = true;
                    break;
                }
                if (!found) { resolve(ToolResult::fail("Widget instance not found")); return; }
                canvas->load_layout(layout);
                resolve(ToolResult::ok("Widget moved", QJsonObject{
                    {"instance_id", id},
                    {"x", args["x"].toInt()},
                    {"y", args["y"].toInt()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "resize_dashboard_widget";
        t.description = "Resize a widget to new column-span (w) and row-span (h).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("instance_id", "Widget instance UUID").required().length(1, 64)
            .integer("w", "New column span").required().min(1)
            .integer("h", "New row span").required().min(1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                const QString id = args["instance_id"].toString();
                auto layout = canvas->current_layout();
                bool found = false;
                for (auto& it : layout.items) {
                    if (it.instance_id != id) continue;
                    it.cell.w = std::max(args["w"].toInt(), it.cell.min_w);
                    it.cell.h = std::max(args["h"].toInt(), it.cell.min_h);
                    found = true;
                    break;
                }
                if (!found) { resolve(ToolResult::fail("Widget instance not found")); return; }
                canvas->load_layout(layout);
                resolve(ToolResult::ok("Widget resized", QJsonObject{
                    {"instance_id", id},
                    {"w", args["w"].toInt()},
                    {"h", args["h"].toInt()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  WIDGET INSTANCE OPS (4)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "get_dashboard_widget_config";
        t.description = "Read per-instance config of a widget (free-form JSON; shape is widget-type-specific).";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("instance_id", "Widget instance UUID").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                auto* tile = find_tile_by_instance_id(canvas, args["instance_id"].toString());
                if (!tile || !tile->content_widget()) {
                    resolve(ToolResult::fail("Widget instance not found")); return;
                }
                resolve(ToolResult::ok_data(tile->content_widget()->config()));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_dashboard_widget_config";
        t.description = "Write per-instance config to a widget (e.g. {symbol:'AAPL'} for stock_quote, {category:'earnings'} for news_category, {broker:'zerodha'} for holdings).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("instance_id", "Widget instance UUID").required().length(1, 64)
            .object("config", "Widget-type-specific config object").required()
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                const QString id = args["instance_id"].toString();
                auto* tile = find_tile_by_instance_id(canvas, id);
                if (!tile || !tile->content_widget()) {
                    resolve(ToolResult::fail("Widget instance not found")); return;
                }
                const QJsonObject cfg = args["config"].toObject();
                tile->content_widget()->apply_config(cfg);

                // Also persist into the layout's GridItem.config so save/load round-trips.
                auto layout = canvas->current_layout();
                for (auto& it : layout.items) {
                    if (it.instance_id == id) { it.config = cfg; break; }
                }
                canvas->load_layout(layout);

                resolve(ToolResult::ok("Widget config applied", QJsonObject{
                    {"instance_id", id},
                    {"config", cfg},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "refresh_dashboard_widget";
        t.description = "Trigger one widget's refresh action (same as its title-bar refresh button).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("instance_id", "Widget instance UUID").required().length(1, 64)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                auto* tile = find_tile_by_instance_id(canvas, args["instance_id"].toString());
                if (!tile || !tile->content_widget()) {
                    resolve(ToolResult::fail("Widget instance not found")); return;
                }
                tile->content_widget()->request_refresh();
                resolve(ToolResult::ok("Refresh dispatched",
                                        QJsonObject{{"instance_id", args["instance_id"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_dashboard_row_height";
        t.description = "Set the dashboard grid row height in pixels (compact mode uses 40, normal uses 60).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("row_height", "Row height in pixels").required().between(20, 200)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                const int px = args["row_height"].toInt();
                canvas->set_row_height(px);
                resolve(ToolResult::ok("Row height set", QJsonObject{{"row_height", px}}));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  VIEW / TOOLBAR (4)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "refresh_all_dashboard_widgets";
        t.description = "Trigger refresh on every widget in the live dashboard (the REFRESH ALL toolbar button).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) { resolve(ToolResult::fail("Dashboard not open")); return; }
                int n = 0;
                for (auto* w : canvas->findChildren<screens::widgets::BaseWidget*>()) {
                    if (w) { w->request_refresh(); ++n; }
                }
                resolve(ToolResult::ok("Refresh dispatched",
                                        QJsonObject{{"widget_count", n}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "toggle_market_pulse_panel";
        t.description = "Toggle the right-side market pulse panel's visibility.";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* ds = find_dashboard_screen();
                if (!ds) { resolve(ToolResult::fail("Dashboard not open")); return; }
                // Emit the toolbar's signal so the same slot path runs.
                auto* tb = ds->findChild<screens::DashboardToolBar*>();
                if (!tb) { resolve(ToolResult::fail("Toolbar not found")); return; }
                emit tb->toggle_pulse_clicked();
                resolve(ToolResult::ok("Market pulse toggled"));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "toggle_dashboard_compact_mode";
        t.description = "Toggle compact mode (row height 40 vs 60 px).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* ds = find_dashboard_screen();
                if (!ds) { resolve(ToolResult::fail("Dashboard not open")); return; }
                auto* tb = ds->findChild<screens::DashboardToolBar*>();
                if (!tb) { resolve(ToolResult::fail("Toolbar not found")); return; }
                emit tb->toggle_compact_clicked();
                resolve(ToolResult::ok("Compact mode toggled"));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_dashboard_stats";
        t.description = "Get dashboard runtime stats (widget count, grid cols/row_h, screen open).";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* canvas = find_dashboard_canvas();
                if (!canvas) {
                    resolve(ToolResult::ok_data(QJsonObject{{"screen_open", false}}));
                    return;
                }
                const auto L = canvas->current_layout();
                resolve(ToolResult::ok_data(QJsonObject{
                    {"screen_open", true},
                    {"widget_count", canvas->tile_count()},
                    {"cols", L.cols},
                    {"row_h", L.row_h},
                    {"margin", L.margin},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  TICKER BAR (3)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "get_dashboard_ticker_symbols";
        t.description = "Get the current ticker-bar symbol list.";
        t.category = "dashboard";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QStringList symbols;
                if (auto* tb = find_ticker_bar()) {
                    symbols = tb->symbols();
                } else {
                    // Fall back to the persisted setting when dashboard not open.
                    auto r = SettingsRepository::instance().get(kTickerSymbolsKey);
                    if (r.is_ok())
                        symbols = r.value().split(',', Qt::SkipEmptyParts);
                }
                QJsonArray arr;
                for (const auto& s : symbols) arr.append(s);
                resolve(ToolResult::ok_data(QJsonObject{
                    {"symbols", arr},
                    {"count", static_cast<int>(symbols.size())},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_dashboard_ticker_symbols";
        t.description = "Replace the ticker-bar symbol list (comma-separated symbols, persisted across sessions).";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .array("symbols", "Symbol list (e.g. ['SPY','QQQ','AAPL'])",
                   QJsonObject{{"type", "string"}})
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                QStringList symbols;
                for (const auto& v : args["symbols"].toArray()) {
                    const QString s = v.toString().trimmed().toUpper();
                    if (!s.isEmpty()) symbols.append(s);
                }
                // Persist via the same SettingsRepository key the ticker bar uses.
                auto r = SettingsRepository::instance().set(kTickerSymbolsKey, symbols.join(','), "dashboard");
                if (r.is_err()) { resolve(ToolResult::fail(QString::fromStdString(r.error()))); return; }

                // If the dashboard is open, push the change so it re-subscribes.
                if (auto* tb = find_ticker_bar())
                    emit tb->symbols_changed(symbols);

                resolve(ToolResult::ok("Ticker symbols updated", QJsonObject{
                    {"count", static_cast<int>(symbols.size())},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "refresh_dashboard_ticker";
        t.description = "Force the ticker bar to re-fetch quotes for its current symbol list.";
        t.category = "dashboard";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* tb = find_ticker_bar();
                if (!tb) { resolve(ToolResult::fail("Dashboard not open")); return; }
                // Re-emit symbols_changed — DashboardScreen's slot is the refresh path.
                emit tb->symbols_changed(tb->symbols());
                resolve(ToolResult::ok("Ticker refresh dispatched",
                                        QJsonObject{{"symbol_count", static_cast<int>(tb->symbols().size())}}));
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 dashboard tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
