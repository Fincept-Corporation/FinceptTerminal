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

// Resolve a caller-supplied screen name against the live registry: exact id, then
// case-insensitive id, then a UNIQUE substring match (lets the LLM say "news" and get
// "news" or "ai_chat" and get "ai_chat" without remembering exact ids). Stricter than
// the previous TAB_MAP partial-match because we go off the live registry, no
// display-name aliases. Returns the id, or an empty string with `error` filled.
//
// Several substring matches used to resolve to the first one silently, so "trading"
// opened whichever *_trading id sorted first. One match is unambiguous; several is a
// question for the caller, not a coin toss with side effects on the user's layout.
QString nav_resolve_screen_id(const QString& tab, const QStringList& ids, QString& error) {
    for (const auto& id : ids) {
        if (id == tab)
            return id;
    }
    for (const auto& id : ids) {
        if (id.compare(tab, Qt::CaseInsensitive) == 0)
            return id;
    }
    const QString tab_lower = tab.toLower();
    QStringList candidates;
    for (const auto& id : ids) {
        if (id.toLower().contains(tab_lower))
            candidates.append(id);
    }
    if (candidates.size() == 1)
        return candidates.first();
    if (candidates.size() > 1) {
        error = "Ambiguous tab '" + tab + "' — matches: " + candidates.join(", ") + ". Use one of these ids exactly.";
        return {};
    }
    error = "Unknown tab '" + tab + "'. Valid ids: " + ids.join(", ");
    return {};
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
                        "DockScreenRouter, not hardcoded. To open a screen ON a specific ticker use "
                        "open_symbol_on_screen instead.";
        t.category = "navigation";
        // This tool is Tier-0 (its schema is sent on every round of every conversation), so it
        // stays minimal — the symbol variant is a separate, discoverable tool.
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

            QString resolve_error;
            const QString resolved = nav_resolve_screen_id(tab, snap.ids, resolve_error);
            if (resolved.isEmpty())
                return ToolResult::fail(resolve_error);

            // Hand off to WindowFrame's existing nav.switch_screen subscriber.
            // The subscriber marshals onto dock_router_'s thread, so we don't
            // duplicate that logic here.
            EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", resolved}});

            LOG_INFO(TAG, "Navigate to tab: " + resolved);
            return ToolResult::ok("Navigated to " + resolved, QJsonObject{{"tab", resolved}});
        };
        tools.push_back(std::move(t));
    }

    // ── open_symbol_on_screen ───────────────────────────────────────────
    // Navigate to a screen AND hand it a ticker, through the nav.open_symbol event. The
    // router queues the symbol until the screen exists (screens are built lazily), which
    // navigate_to_tab + a screen-specific "load symbol" call could not: that pair dropped
    // the symbol whenever the target tab had never been opened. Kept as its own tool, not
    // extra parameters on navigate_to_tab, because navigate_to_tab is Tier-0 and its schema
    // is paid on every round.
    {
        ToolDef t;
        t.name = "open_symbol_on_screen";
        t.description = "Open a terminal screen on a specific ticker (\"show AAPL in equity research\", \"put BTC/USDT "
                        "on the crypto trading screen\"). Screens that accept a symbol: equity_research, "
                        "equity_trading, watchlist, news, portfolio (only if held), fno, surface_analytics, "
                        "backtesting, ai_chat, and crypto_trading (crypto only). On any other screen it just "
                        "navigates there. Use list_tabs for screen ids.";
        t.category = "navigation";
        t.input_schema = ToolSchemaBuilder()
                             .string("tab", "Screen id (use list_tabs to discover live ids)")
                             .required()
                             .length(1, 64)
                             .string("symbol", "Ticker to open (e.g. AAPL, RELIANCE.NS, BTC/USDT)")
                             .required()
                             .length(1, 32)
                             .string("asset_class", "equity (default) or crypto — crypto_trading only accepts crypto")
                             .default_str("equity")
                             .enums({"equity", "crypto"})
                             .string("exchange", "Optional exchange code (e.g. NSE, NASDAQ)")
                             .default_str("")
                             .length(0, 16)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString tab = args["tab"].toString().trimmed();
            const QString symbol = args["symbol"].toString().trimmed();
            if (tab.isEmpty() || symbol.isEmpty())
                return ToolResult::fail("Both 'tab' and 'symbol' are required");

            auto snap = snapshot_screens();
            if (snap.ids.isEmpty())
                return ToolResult::fail("No active terminal window — DockScreenRouter unavailable");

            QString resolve_error;
            const QString resolved = nav_resolve_screen_id(tab, snap.ids, resolve_error);
            if (resolved.isEmpty())
                return ToolResult::fail(resolve_error);

            const QString asset_class = args["asset_class"].toString("equity");
            // crypto_trading only takes crypto and equity screens only take non-crypto; the
            // subscriber ignores a mismatch silently, so say so up front.
            if (resolved == QLatin1String("crypto_trading") && asset_class != QLatin1String("crypto"))
                return ToolResult::fail("crypto_trading only accepts asset_class='crypto'");

            QVariantMap payload{{"screen_id", resolved}, {"symbol", symbol}, {"asset_class", asset_class}};
            const QString exchange = args["exchange"].toString().trimmed();
            if (!exchange.isEmpty())
                payload.insert("exchange", exchange);
            EventBus::instance().publish("nav.open_symbol", payload);

            LOG_INFO(TAG, "Open symbol on screen: " + resolved + " " + symbol);
            return ToolResult::ok("Opened " + resolved + " and handed it " + symbol +
                                      " (a screen without symbol support just opens normally)",
                                  QJsonObject{{"tab", resolved}, {"symbol", symbol}, {"asset_class", asset_class}});
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
                tabs.append(QJsonObject{{"id", id}, {"title", fincept::DockScreenRouter::title_for_id(id)}});
            }
            return ToolResult::ok_data(
                QJsonObject{{"count", tabs.size()}, {"current", snap.current_id}, {"tabs", tabs}});
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
                {"id", snap.current_id}, {"title", fincept::DockScreenRouter::title_for_id(snap.current_id)}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
