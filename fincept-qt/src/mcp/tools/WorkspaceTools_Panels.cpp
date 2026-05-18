// src/mcp/tools/WorkspaceTools_Panels.cpp
//
// Panel-level tools: list_panels, list_available_screen_ids, navigate_panel,
// tab_panel, add_panel_alongside, replace_panel, remove_panel, duplicate_panel,
// tear_off_panel, move_panel_to_window, tile_panels_2x2, cycle_panel_focus,
// set_panel_title, refresh_focused_panel.
//
// Part of the topic-based split of WorkspaceTools.cpp.

#include "mcp/tools/WorkspaceTools.h"
#include "mcp/tools/WorkspaceTools_internal.h"

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/actions/ActionDef.h"
#include "core/actions/ActionRegistry.h"
#include "core/identity/Uuid.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/LayoutTemplates.h"
#include "core/layout/LayoutTypes.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelHandle.h"
#include "core/panel/PanelRegistry.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "core/symbol/SymbolRef.h"
#include "core/window/WindowRegistry.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRect>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>

namespace fincept::mcp::tools {

using namespace fincept::mcp::tools::workspace_internal;

void workspace_internal::register_panel_tools(std::vector<ToolDef>& tools) {
    // ═══════════════════════════════════════════════════════════════════
    //  PANELS (14)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_panels";
        t.description = "List every PanelHandle, optionally filtered by type_id / link_group / window_id.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("type_id", "Filter by panel type id").default_str("").length(0, 128)
            .string("link_group", "Filter by link group letter (A..J)").default_str("").length(0, 4)
            .integer("window_id", "Filter by window int id (-1 = no filter)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const QString type = args["type_id"].toString();
                const QString grp = args["link_group"].toString();
                const int wid = args["window_id"].toInt(-1);

                QList<PanelHandle*> panels = PanelRegistry::instance().all_panels();
                if (!type.isEmpty())
                    panels = PanelRegistry::instance().find_by_type(type);
                if (!grp.isEmpty())
                    panels = PanelRegistry::instance().find_by_group(grp);
                if (wid >= 0) {
                    auto* w = frame_by_id(wid);
                    if (w) panels = PanelRegistry::instance().find_by_frame(w->frame_uuid());
                }
                QJsonArray arr;
                for (auto* p : panels) arr.append(panel_to_json_v2(p));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "list_available_screen_ids";
        t.description = "List every registered screen id available to a window's dock router (target for navigate/add/replace).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window id (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                QJsonArray arr;
                for (const auto& id : w->dock_router()->all_screen_ids())
                    arr.append(id);
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "navigate_panel";
        t.description = "Open a screen in a window's dock area. Equivalent to typing the screen id into the command bar.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id (e.g. 'dashboard', 'news', 'watchlist')").required().length(1, 64)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .boolean("exclusive", "Close all other panels first").default_bool(false)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                const QString id = args["screen_id"].toString();
                const bool excl = args["exclusive"].toBool(false);
                w->dock_router()->navigate(id, excl);
                resolve(ToolResult::ok("Navigated", QJsonObject{{"screen_id", id}, {"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "tab_panel";
        t.description = "Add a screen as a tab inside the currently-focused dock area of a window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id").required().length(1, 64)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                const QString id = args["screen_id"].toString();
                w->dock_router()->tab_into(id);
                resolve(ToolResult::ok("Tabbed", QJsonObject{{"screen_id", id}, {"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "add_panel_alongside";
        t.description = "Split-pane: open primary alone, then add secondary next to it. Mirrors the 'X add Y' command-bar syntax (e.g. 'dash add news').";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("primary", "Primary screen id (e.g. 'dashboard')").required().length(1, 64)
            .string("secondary", "Secondary screen id to add (e.g. 'news')").required().length(1, 64)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                w->dock_router()->add_alongside(args["primary"].toString(), args["secondary"].toString());
                resolve(ToolResult::ok("Panel added alongside",
                                        QJsonObject{{"primary", args["primary"].toString()},
                                                    {"secondary", args["secondary"].toString()},
                                                    {"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "replace_panel";
        t.description = "Replace one panel with another (full area). Mirrors the 'X replace Y' command-bar syntax.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("primary", "Existing screen id to close").required().length(1, 64)
            .string("secondary", "New screen id to open in its place").required().length(1, 64)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                w->dock_router()->replace_screen(args["primary"].toString(), args["secondary"].toString());
                resolve(ToolResult::ok("Panel replaced",
                                        QJsonObject{{"old", args["primary"].toString()},
                                                    {"new", args["secondary"].toString()},
                                                    {"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "remove_panel";
        t.description = "Close all panels except the given one (single-panel mode). Mirrors the 'X remove' command-bar syntax.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id to keep").required().length(1, 64)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                w->dock_router()->remove_screen(args["screen_id"].toString());
                resolve(ToolResult::ok("Panels removed", QJsonObject{{"kept", args["screen_id"].toString()}, {"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "duplicate_panel";
        t.description = "Duplicate an open panel into a new tab with its state preserved.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id to duplicate").required().length(1, 64)
            .integer("window_id", "Target window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                auto* dw = w->dock_router()->duplicate_panel(args["screen_id"].toString());
                if (!dw) { resolve(ToolResult::fail("Cannot duplicate (eager-registered or missing)")); return; }
                resolve(ToolResult::ok("Panel duplicated", QJsonObject{{"screen_id", args["screen_id"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "tear_off_panel";
        t.description = "Tear off a panel into a brand-new WindowFrame on the next available monitor.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id to tear off").required().length(1, 64)
            .integer("window_id", "Source window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                const bool ok = w->dock_router()->tear_off_to_new_frame(args["screen_id"].toString());
                if (!ok) { resolve(ToolResult::fail("Tear-off failed (unknown id or no factory)")); return; }
                resolve(ToolResult::ok("Panel torn off", QJsonObject{{"screen_id", args["screen_id"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "move_panel_to_window";
        t.description = "Move a panel from its current window to another window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id to move").required().length(1, 64)
            .integer("source_window_id", "Source window id").required().min(0)
            .integer("target_window_id", "Target window id").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* src = frame_by_id(args["source_window_id"].toInt());
                auto* dst = frame_by_id(args["target_window_id"].toInt());
                if (!src || !src->dock_router()) { resolve(ToolResult::fail("Source window not found")); return; }
                if (!dst || !dst->dock_router()) { resolve(ToolResult::fail("Target window not found")); return; }
                const bool ok = src->dock_router()->move_panel_to_frame(args["screen_id"].toString(),
                                                                          dst->dock_router());
                if (!ok) { resolve(ToolResult::fail("Move failed")); return; }
                resolve(ToolResult::ok("Panel moved", QJsonObject{
                    {"screen_id", args["screen_id"].toString()},
                    {"from", src->window_id()},
                    {"to", dst->window_id()},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "tile_panels_2x2";
        t.description = "Rearrange the open panels in a window into a 2x2 grid.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                w->dock_router()->tile_2x2();
                resolve(ToolResult::ok("Tiled 2x2", QJsonObject{{"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "cycle_panel_focus";
        t.description = "Cycle keyboard focus between the open panels in a window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window (-1 = focused)").default_int(-1).min(-1)
            .boolean("forward", "true to cycle forward").default_bool(true)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                w->dock_router()->cycle_focused_panel(args["forward"].toBool(true));
                resolve(ToolResult::ok("Panel cycled",
                                        QJsonObject{{"window_id", w->window_id()},
                                                    {"current_screen_id", w->dock_router()->current_screen_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_panel_title";
        t.description = "Rename a panel's tab title (separate from the underlying screen state).";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .string("screen_id", "Screen id").required().length(1, 64)
            .string("title", "New tab title").required().length(1, 256)
            .integer("window_id", "Window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w || !w->dock_router()) { resolve(ToolResult::fail("Window not found")); return; }
                auto* dw = w->dock_router()->find_dock_widget(args["screen_id"].toString());
                if (!dw) { resolve(ToolResult::fail("Panel not open")); return; }
                dw->setWindowTitle(args["title"].toString());
                resolve(ToolResult::ok("Title updated",
                                        QJsonObject{{"screen_id", args["screen_id"].toString()},
                                                    {"title", args["title"].toString()}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "refresh_focused_panel";
        t.description = "Trigger the 'refresh' action on the currently-focused panel inside a window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window (-1 = focused)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = resolve_window(args);
                if (!w) { resolve(ToolResult::fail("Window not found")); return; }
                w->refresh_focused_panel();
                resolve(ToolResult::ok("Refresh dispatched", QJsonObject{{"window_id", w->window_id()}}));
            });
        };
        tools.push_back(std::move(t));
    }

}
} // namespace fincept::mcp::tools
