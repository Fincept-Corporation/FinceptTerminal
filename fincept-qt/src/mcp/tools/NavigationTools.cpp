// NavigationTools.cpp — Tab switching MCP tools (Qt port)

#include "mcp/tools/NavigationTools.h"

#include "app/DockScreenRouter.h"
#include "app/WindowFrame.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/tools/ThreadHelper.h"

#include <QApplication>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

#include <algorithm>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "NavTools";

// ── Live router lookup ──────────────────────────────────────────────────────
// MCP tool handlers run on a worker thread. QApplication's widget tree must
// be queried from the main thread. We marshal via run_on_target_thread_sync
// (qApp lives on the main thread) and pick the active or primary WindowFrame.
namespace {

// Must be called on the main thread.
fincept::DockScreenRouter* find_active_router_main_thread() {
    // Prefer the focused window — that's where the user just typed.
    if (auto* active = QApplication::activeWindow()) {
        if (auto* mw = qobject_cast<fincept::WindowFrame*>(active))
            return mw->dock_router();
    }
    // Fall back to the primary window (window_id == 0). Multi-window mode is
    // possible; we deliberately don't pick "any" WindowFrame because that
    // would surprise users running two terminals.
    const auto top_widgets = QApplication::topLevelWidgets();
    for (QWidget* w : top_widgets) {
        if (auto* mw = qobject_cast<fincept::WindowFrame*>(w)) {
            if (mw->window_id() == 0)
                return mw->dock_router();
        }
    }
    return nullptr;
}

struct ScreenSnapshot {
    QStringList ids;
    QString current_id;
};

ScreenSnapshot snapshot_screens() {
    ScreenSnapshot snap;
    detail::run_on_target_thread_sync(qApp, [&]() {
        if (auto* router = find_active_router_main_thread()) {
            snap.ids = router->all_screen_ids();
            snap.current_id = router->current_screen_id();
        }
    });
    std::sort(snap.ids.begin(), snap.ids.end());
    return snap;
}

} // anonymous namespace

std::vector<ToolDef> get_navigation_tools() {
    std::vector<ToolDef> tools;

    // ── navigate_to_tab ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "navigate_to_tab";
        t.description = "Navigate to a specific terminal screen by id. "
                        "Use list_tabs to see the live registry of available "
                        "screen ids — the set is determined at runtime by "
                        "DockScreenRouter, not hardcoded.";
        t.category = "navigation";
        t.input_schema = ToolSchemaBuilder()
            .string("tab", "Screen id to navigate to (use list_tabs to discover live ids)")
                .required()
                .length(1, 64)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString tab = args["tab"].toString().trimmed();
            if (tab.isEmpty())
                return ToolResult::fail("Missing 'tab' parameter");

            auto snap = snapshot_screens();
            if (snap.ids.isEmpty())
                return ToolResult::fail("No active terminal window — DockScreenRouter unavailable");

            // Resolve: exact id, then case-insensitive id, then first id that
            // contains the query as substring (lets the LLM say "news" and
            // get "news" or "ai_chat" and get "ai_chat" without remembering
            // exact ids). Stricter than the previous TAB_MAP partial-match
            // because we go off the live registry, no display-name aliases.
            QString resolved;
            for (const auto& id : snap.ids) {
                if (id == tab) { resolved = id; break; }
            }
            if (resolved.isEmpty()) {
                for (const auto& id : snap.ids) {
                    if (id.compare(tab, Qt::CaseInsensitive) == 0) { resolved = id; break; }
                }
            }
            if (resolved.isEmpty()) {
                const QString tab_lower = tab.toLower();
                for (const auto& id : snap.ids) {
                    if (id.toLower().contains(tab_lower)) { resolved = id; break; }
                }
            }

            if (resolved.isEmpty()) {
                return ToolResult::fail("Unknown tab '" + tab + "'. Valid ids: " + snap.ids.join(", "));
            }

            // Hand off to WindowFrame's existing nav.switch_screen subscriber.
            // The subscriber marshals onto dock_router_'s thread, so we don't
            // duplicate that logic here.
            EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", resolved}});

            LOG_INFO(TAG, "Navigate to tab: " + resolved);
            return ToolResult::ok("Navigated to " + resolved, QJsonObject{{"tab", resolved}});
        };
        tools.push_back(std::move(t));
    }

    // ── list_tabs ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_tabs";
        t.description = "List all available terminal screens currently registered with the active "
                        "window's DockScreenRouter. Returns each id plus its human-readable title.";
        t.category = "navigation";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto snap = snapshot_screens();
            if (snap.ids.isEmpty())
                return ToolResult::fail("No active terminal window — DockScreenRouter unavailable");

            QJsonArray tabs;
            // title_for_id is a thread-safe static lookup (string→string map).
            for (const auto& id : snap.ids) {
                tabs.append(QJsonObject{{"id", id},
                                        {"title", fincept::DockScreenRouter::title_for_id(id)}});
            }
            return ToolResult::ok_data(QJsonObject{{"count", tabs.size()},
                                                   {"current", snap.current_id},
                                                   {"tabs", tabs}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_current_tab ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_current_tab";
        t.description = "Return the currently focused screen id (and its human-readable title) "
                        "from the active terminal window.";
        t.category = "navigation";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto snap = snapshot_screens();
            if (snap.current_id.isEmpty())
                return ToolResult::fail("No active screen — terminal may not be ready yet");

            return ToolResult::ok_data(QJsonObject{
                {"id", snap.current_id},
                {"title", fincept::DockScreenRouter::title_for_id(snap.current_id)}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
