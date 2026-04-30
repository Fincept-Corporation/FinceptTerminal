// ToolFilterPolicy.cpp — Screen→tool-category routing.

#include "mcp/ToolFilterPolicy.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>

namespace fincept::mcp {

// Parse a JSON string array into a QStringList (silently drops non-strings).
static QStringList json_to_string_list(const QJsonValue& v) {
    QStringList out;
    if (!v.isArray()) return out;
    for (const auto& item : v.toArray())
        if (item.isString()) out.append(item.toString());
    return out;
}

QStringList ToolFilterPolicy::tier_1_categories() {
    // Stable, ordered. Don't reorder casually — Anthropic prompt-cache
    // rebuilds a session prefix when the tool block bytes shift.
    return {
        "meta",       // tool.list / tool.describe / mcp.health — always-on for Tier-3 discovery
        "navigation", // route_to_screen, current_screen — needed by every UI flow
        "system",     // window controls, app state
        "settings",   // theme, preferences
        "datahub",    // hub introspection — tiny but useful for self-diagnosis
    };
}

bool ToolFilterPolicy::is_full_catalogue_screen(const QString& s) {
    // These surfaces are explicit "kitchen sink" contexts where the user is
    // either authoring/configuring tools or invoking the assistant for any
    // task. Don't pre-filter — let the LLM see everything.
    static const QHash<QString, bool> kFullCatalogue = {
        {"ai_chat", true},
        {"chat_mode", true},
        {"agent_config", true},
        {"mcp_servers", true},
        {"node_editor", true}, // workflow author tool — needs all categories
    };
    return kFullCatalogue.value(s, false);
}

QStringList ToolFilterPolicy::categories_for_screen(const QString& s) {
    // Screen → Tier-2 categories. Order doesn't matter (composed into a set
    // before filtering), but kept readable. Unknown screens return empty →
    // caller composes Tier-1 only, which is intentional (safe minimum).
    static const QHash<QString, QStringList> kMap = {
        // Core surfaces ─────────────────────────────────────────────────
        {"dashboard", {"markets", "watchlist", "news", "portfolio"}},
        {"markets", {"markets", "watchlist", "news"}},
        {"watchlist", {"watchlist", "markets", "news"}},
        {"news", {"news", "markets"}},

        // Trading ──────────────────────────────────────────────────────
        {"crypto_trading", {"crypto-trading", "markets", "watchlist", "paper-trading"}},
        {"equity_trading", {"markets", "watchlist", "paper-trading"}},
        {"paper_trading", {"paper-trading", "markets", "watchlist"}},
        {"algo_trading", {"paper-trading", "markets", "analytics"}},
        {"backtesting", {"analytics", "markets", "paper-trading"}},
        {"trade_viz", {"markets", "analytics", "paper-trading"}},

        // Research ─────────────────────────────────────────────────────
        {"equity_research", {"markets", "sec-edgar", "news", "analytics"}},
        {"portfolio", {"portfolio", "watchlist", "markets", "paper-trading", "analytics"}},
        {"surface_analytics", {"analytics", "markets"}},
        {"ma_analytics", {"ma-analytics", "sec-edgar", "markets", "news"}},
        {"derivatives", {"markets", "analytics"}},
        {"alt_investments", {"alt-investments", "markets"}},

        // Data / economics / geopolitics ───────────────────────────────
        {"data_sources", {"data-sources", "markets", "news"}},
        {"data_mapping", {"data-sources"}},
        {"dbnomics", {"data-sources", "news"}},
        {"akshare", {"data-sources", "markets", "news"}},
        {"asia_markets", {"markets", "news", "data-sources"}},
        {"economics", {"data-sources", "news"}},
        {"gov_data", {"data-sources", "news"}},
        {"geopolitics", {"news", "data-sources"}},
        {"maritime", {"news", "data-sources"}},
        {"relationship_map", {"data-sources", "news"}},
        {"polymarket", {"markets", "news"}},

        // Tools / authoring ────────────────────────────────────────────
        {"notes", {"notes", "file_manager"}},
        {"report_builder", {"report-builder", "file_manager", "markets", "news", "portfolio"}},
        {"file_manager", {"file_manager", "notes"}},
        {"code_editor", {"file_manager"}},
        {"excel", {"file_manager"}},

        // Community / account ──────────────────────────────────────────
        {"forum", {"forum", "profile"}},
        {"profile", {"profile"}},
        {"settings", {}}, // settings already in Tier-1
        {"about", {}},
        {"docs", {}},
        {"support", {}},

        // QuantLib / advanced research — analytics + markets is enough
        {"quantlib", {"analytics", "markets"}},
        {"ai_quant_lab", {"analytics", "markets"}},
        {"alpha_arena", {"analytics", "markets"}},
    };
    return kMap.value(s, {});
}

std::optional<ToolFilter> ToolFilterPolicy::filter_for_screen(const QString& screen_id) {
    if (is_full_catalogue_screen(screen_id))
        return std::nullopt;

    ToolFilter f;
    f.categories = tier_1_categories();
    for (const auto& cat : categories_for_screen(screen_id)) {
        if (!f.categories.contains(cat))
            f.categories.append(cat);
    }
    f.max_tools = kMaxToolsDefault;
    return f;
}

std::optional<ToolFilter> ToolFilterPolicy::filter_from_config_json(const QJsonObject& config) {
    if (!config.contains("tool_filter"))
        return std::nullopt;
    const QJsonObject tf = config.value("tool_filter").toObject();
    ToolFilter f;
    f.categories = json_to_string_list(tf.value("categories"));
    f.exclude_categories = json_to_string_list(tf.value("exclude_categories"));
    f.name_patterns = json_to_string_list(tf.value("name_patterns"));
    f.exclude_name_patterns = json_to_string_list(tf.value("exclude_name_patterns"));
    f.max_tools = tf.value("max_tools").toInt(0);
    return f;
}

QJsonObject ToolFilterPolicy::filter_to_config_value(const ToolFilter& f) {
    QJsonObject tf;
    QJsonArray cats;
    for (const auto& c : f.categories) cats.append(c);
    tf["categories"] = cats;

    QJsonArray xcats;
    for (const auto& c : f.exclude_categories) xcats.append(c);
    tf["exclude_categories"] = xcats;

    QJsonArray names;
    for (const auto& n : f.name_patterns) names.append(n);
    tf["name_patterns"] = names;

    QJsonArray xnames;
    for (const auto& n : f.exclude_name_patterns) xnames.append(n);
    tf["exclude_name_patterns"] = xnames;

    tf["max_tools"] = f.max_tools;
    return tf;
}

QStringList ToolFilterPolicy::infer_extra_categories(const QString& user_message) {
    // Keyword → category. Conservative: only adds when the keyword is
    // unambiguous. Case-insensitive substring match. The "extras" returned
    // here are added on top of the screen's Tier-2 set, so over-firing
    // costs at most a few extra tools — far cheaper than missing the
    // category and forcing the LLM to fall back to tool.list.
    struct Rule { QString keyword; QString category; };
    static const QVector<Rule> kRules = {
        // Portfolio / positions
        {"portfolio", "portfolio"},
        {"holdings", "portfolio"},
        {"position", "portfolio"},
        {"rebalance", "portfolio"},
        {"allocation", "portfolio"},
        {"pnl", "portfolio"},
        {"p&l", "portfolio"},

        // News
        {"news", "news"},
        {"headline", "news"},
        {"announcement", "news"},
        {"press release", "news"},

        // SEC / filings
        {"10-k", "sec-edgar"},
        {"10-q", "sec-edgar"},
        {"8-k", "sec-edgar"},
        {"edgar", "sec-edgar"},
        {"sec filing", "sec-edgar"},
        {"proxy", "sec-edgar"},

        // Reports / research
        {"report", "report-builder"},
        {"research note", "report-builder"},
        {"write up", "report-builder"},
        {"draft", "report-builder"},

        // M&A
        {"merger", "ma-analytics"},
        {"acquisition", "ma-analytics"},
        {"m&a", "ma-analytics"},
        {"buyout", "ma-analytics"},
        {"takeover", "ma-analytics"},

        // Trading
        {"buy", "paper-trading"},
        {"sell", "paper-trading"},
        {"order", "paper-trading"},
        {"paper trade", "paper-trading"},
        {"backtest", "paper-trading"},

        // Watchlist
        {"watchlist", "watchlist"},
        {"add to list", "watchlist"},
        {"track", "watchlist"},

        // Markets / quotes
        {"price", "markets"},
        {"quote", "markets"},
        {"chart", "markets"},
        {"candle", "markets"},

        // Notes
        {"note", "notes"},
        {"memo", "notes"},

        // File manager
        {"file", "file_manager"},
        {"export", "file_manager"},
        {"save to", "file_manager"},
    };

    const QString lower = user_message.toLower();
    QStringList extras;
    for (const auto& r : kRules) {
        if (lower.contains(r.keyword) && !extras.contains(r.category))
            extras.append(r.category);
    }
    return extras;
}

ToolFilter ToolFilterPolicy::compose_for_agent_and_screen(const QJsonObject& agent_config_json,
                                                          const QString& screen_id) {
    // Per-agent filter wins when present — these are explicit operator
    // intent ("this agent only ever needs portfolio + sec-edgar tools").
    if (auto agent_filter = filter_from_config_json(agent_config_json))
        return *agent_filter;

    // Else fall back to screen-driven filter, or Tier-1-only when the
    // screen wants the "full catalogue" but we still need a sane cap.
    if (auto screen_filter = filter_for_screen(screen_id))
        return *screen_filter;

    ToolFilter f;
    f.categories = tier_1_categories();
    f.max_tools = kMaxToolsDefault;
    return f;
}

} // namespace fincept::mcp
