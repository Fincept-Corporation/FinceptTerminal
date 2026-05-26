// src/mcp/tools/WorkspaceTools_MonitorsWindows.cpp
//
// Monitor / window introspection + control tools: list_monitors,
// get_focused_monitor, get_monitor_topology, list_windows, get_focused_window,
// new_window, close_window, move_window_to_monitor, set_window_geometry,
// set_window_always_on_top, set_window_focus_mode, cycle_focused_window.
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

void workspace_internal::register_monitor_window_tools(std::vector<ToolDef>& tools) {
    // ═══════════════════════════════════════════════════════════════════
    //  MONITORS (3)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_monitors";
        t.description = "List all connected monitors with geometry, DPI, refresh rate, and primary flag.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                const auto screens = QGuiApplication::screens();
                QJsonArray arr;
                for (int i = 0; i < screens.size(); ++i)
                    arr.append(screen_to_json(screens.at(i), i));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_focused_monitor";
        t.description = "Get the monitor hosting the currently-focused window.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* aw = qApp->activeWindow();
                QScreen* s = aw ? aw->screen() : QGuiApplication::primaryScreen();
                resolve(ToolResult::ok_data(screen_to_json(s, screen_index_for(s))));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_monitor_topology";
        t.description = "Get a stable key identifying the current monitor topology (used for layout-variant matching).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                // Build the topology key from sorted screen serial+name+geometry
                // — matches the spirit of layout::MonitorTopologyKey::current_key
                // without depending on the (possibly internal) producer impl.
                const auto screens = QGuiApplication::screens();
                QStringList parts;
                for (auto* s : screens) {
                    parts.append(QString("%1:%2x%3@%4")
                                     .arg(s->name().isEmpty() ? s->serialNumber() : s->name())
                                     .arg(s->geometry().width())
                                     .arg(s->geometry().height())
                                     .arg(static_cast<int>(s->refreshRate())));
                }
                std::sort(parts.begin(), parts.end());
                resolve(ToolResult::ok_data(QJsonObject{
                    {"key", parts.join(",")},
                    {"monitor_count", static_cast<int>(screens.size())},
                }));
            });
        };
        tools.push_back(std::move(t));
    }

    // ═══════════════════════════════════════════════════════════════════
    //  WINDOWS (9)
    // ═══════════════════════════════════════════════════════════════════

    {
        ToolDef t;
        t.name = "list_windows";
        t.description = "List every live WindowFrame (id, uuid, title, monitor index, geometry, flags).";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                QJsonArray arr;
                for (auto* w : WindowRegistry::instance().frames())
                    arr.append(window_to_json(w));
                resolve(ToolResult::ok_data(arr));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "get_focused_window";
        t.description = "Get the currently focused (active) WindowFrame, or the first frame if none has focus.";
        t.category = "workspace";
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.async_handler = [](const QJsonObject&, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [](auto resolve) {
                auto* aw = qApp->activeWindow();
                auto* wf = qobject_cast<WindowFrame*>(aw);
                if (!wf) {
                    auto frames = WindowRegistry::instance().frames();
                    if (!frames.isEmpty()) wf = frames.first();
                }
                if (!wf) {
                    resolve(ToolResult::fail("No window available"));
                    return;
                }
                resolve(ToolResult::ok_data(window_to_json(wf)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "new_window";
        t.description = "Spawn a new WindowFrame, optionally on a specific monitor.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("monitor_index", "Target monitor index (-1 = next available)").default_int(-1).min(-1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const int monitor_idx = args["monitor_index"].toInt(-1);
                auto* frame = new WindowFrame(WindowFrame::next_window_id(), nullptr);
                if (monitor_idx >= 0) {
                    if (auto* s = screen_by_index(monitor_idx)) {
                        frame->move_to_screen(s);
                    }
                }
                frame->show();
                resolve(ToolResult::ok("Window created", window_to_json(frame)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "close_window";
        t.description = "Close a WindowFrame by window_id. Cannot close the last remaining window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.auth_required = AuthLevel::ExplicitConfirm;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window id").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const int id = args["window_id"].toInt();
                if (WindowRegistry::instance().frame_count() <= 1) {
                    resolve(ToolResult::fail("Cannot close the last window"));
                    return;
                }
                auto* w = frame_by_id(id);
                if (!w) { resolve(ToolResult::fail("Window not found")); return; }
                w->close();
                resolve(ToolResult::ok("Window closed", QJsonObject{{"window_id", id}}));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "move_window_to_monitor";
        t.description = "Relocate a window to a given monitor by index.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window id").required().min(0)
            .integer("monitor_index", "Target monitor index").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = frame_by_id(args["window_id"].toInt());
                if (!w) { resolve(ToolResult::fail("Window not found")); return; }
                auto* s = screen_by_index(args["monitor_index"].toInt());
                if (!s) { resolve(ToolResult::fail("Monitor not found")); return; }
                w->move_to_screen(s);
                resolve(ToolResult::ok("Window moved", window_to_json(w)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_window_geometry";
        t.description = "Set explicit window geometry (x, y, width, height).";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window id").required().min(0)
            .integer("x", "X coordinate").required()
            .integer("y", "Y coordinate").required()
            .integer("width", "Width in pixels").required().min(1)
            .integer("height", "Height in pixels").required().min(1)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = frame_by_id(args["window_id"].toInt());
                if (!w) { resolve(ToolResult::fail("Window not found")); return; }
                w->setGeometry(args["x"].toInt(), args["y"].toInt(),
                                args["width"].toInt(), args["height"].toInt());
                resolve(ToolResult::ok("Geometry updated", window_to_json(w)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_window_always_on_top";
        t.description = "Toggle Qt::WindowStaysOnTopHint on a window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window id").required().min(0)
            .boolean("on", "true to enable always-on-top, false to disable").required()
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = frame_by_id(args["window_id"].toInt());
                if (!w) { resolve(ToolResult::fail("Window not found")); return; }
                const bool on = args["on"].toBool();
                w->set_always_on_top(on);
                resolve(ToolResult::ok(on ? "Always-on-top enabled" : "Always-on-top disabled",
                                        window_to_json(w)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "set_window_focus_mode";
        t.description = "Toggle focus mode (hide chrome / toolbar) on a window.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .integer("window_id", "Window id").required().min(0)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                auto* w = frame_by_id(args["window_id"].toInt());
                if (!w) { resolve(ToolResult::fail("Window not found")); return; }
                w->toggle_focus_mode();
                resolve(ToolResult::ok("Focus mode toggled", window_to_json(w)));
            });
        };
        tools.push_back(std::move(t));
    }

    {
        ToolDef t;
        t.name = "cycle_focused_window";
        t.description = "Cycle keyboard focus to the next/previous WindowFrame.";
        t.category = "workspace";
        t.is_destructive = true;
        t.default_timeout_ms = kDefaultTimeoutMs;
        t.input_schema = ToolSchemaBuilder()
            .boolean("forward", "true to cycle forward, false to cycle backward").default_bool(true)
            .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            run_on_ui(std::move(ctx), promise, [args](auto resolve) {
                const bool forward = args["forward"].toBool(true);
                auto frames = WindowRegistry::instance().frames();
                if (frames.size() < 2) {
                    resolve(ToolResult::fail("Need at least 2 windows to cycle"));
                    return;
                }
                auto* aw = qobject_cast<WindowFrame*>(qApp->activeWindow());
                int idx = frames.indexOf(aw);
                if (idx < 0) idx = 0;
                idx = forward ? (idx + 1) % frames.size()
                              : (idx - 1 + frames.size()) % frames.size();
                auto* target = frames.at(idx);
                target->raise();
                target->activateWindow();
                resolve(ToolResult::ok("Window cycled", window_to_json(target)));
            });
        };
        tools.push_back(std::move(t));
    }

}
} // namespace fincept::mcp::tools
