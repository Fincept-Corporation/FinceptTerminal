#pragma once
// ToolFilterPolicy.h — Production policy for scoping the MCP tool catalogue
// per LLM turn. Implements the 3-tier disclosure model:
//
//   Tier 1 — always exposed (meta, navigation, system, settings, datahub).
//            Small stable prefix → Anthropic prompt-cache friendly.
//   Tier 2 — screen-scoped categories. Driven by the active screen id.
//   Tier 3 — discovered lazily by the LLM via tool.list / tool.describe.
//
// Public surface:
//   - tier_1_categories()     → fixed set, never empty.
//   - categories_for_screen() → Tier-2 add-ons; empty when screen unknown.
//   - filter_for_screen()     → composed ToolFilter ready for LlmService.
//                               Returns std::nullopt for "expose everything"
//                               screens (e.g. AI Chat, agent shells).
//
// Keep this file pure data + composition — no Qt UI deps, no service deps,
// so it can be unit-tested without spinning up the app.

#include "mcp/McpTypes.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace fincept::mcp {

class ToolFilterPolicy {
  public:
    // Tier-1 categories — exposed on every LLM turn regardless of screen.
    // Order is intentional: most-stable first (helps prompt-cache hits).
    static QStringList tier_1_categories();

    // Tier-2 categories for a given screen. Returns empty when the screen
    // has no special scoping (caller falls back to Tier-1 only or to the
    // full catalogue, depending on context).
    static QStringList categories_for_screen(const QString& screen_id);

    // Returns a fully composed ToolFilter (Tier-1 ∪ Tier-2) for a screen.
    // Returns std::nullopt when the screen wants the full unfiltered
    // catalogue (AI chat, agent shells, the MCP servers manager itself).
    // Caller (LlmService::set_tool_filter / clear_tool_filter) translates
    // nullopt into a cleared filter.
    //
    // Default cap: kMaxToolsDefault — applied even on full-catalogue screens
    // unless they explicitly opt out, so we never dump all 237+ schemas.
    static std::optional<ToolFilter> filter_for_screen(const QString& screen_id);

    // Hard ceiling for any composed filter. Tool-pick accuracy degrades
    // sharply past ~40 tools — this is the production backstop.
    static constexpr int kMaxToolsDefault = 50;

    // Screens that intentionally want the full catalogue (no scoping).
    // Examples: ai_chat, chat_mode, agent_config, mcp_servers.
    static bool is_full_catalogue_screen(const QString& screen_id);

    // ── Per-agent persistence ───────────────────────────────────────────
    // Agents store an optional ToolFilter inside AgentConfig.config_json
    // under the key "tool_filter". The fields mirror ToolFilter members.
    // Schema:
    //   "tool_filter": {
    //     "categories":            ["markets", "watchlist"],
    //     "exclude_categories":    ["forum"],
    //     "name_patterns":         ["^get_.*"],
    //     "exclude_name_patterns": [],
    //     "max_tools":             40
    //   }
    // Missing keys → empty defaults. Returns std::nullopt when the JSON
    // has no "tool_filter" key (caller treats as "no per-agent override").
    static std::optional<ToolFilter> filter_from_config_json(const QJsonObject& config);
    static QJsonObject filter_to_config_value(const ToolFilter& filter);

    // Compose: per-agent filter overrides → screen filter → Tier-1 only.
    // Use this when the LLM is dispatching for a specific agent on a
    // specific screen (covers the AI-chat-with-persona scenario).
    static ToolFilter compose_for_agent_and_screen(const QJsonObject& agent_config_json,
                                                   const QString& screen_id);

    // Lightweight intent classifier. Scans the recent user message for
    // keywords that suggest categories beyond what the active screen
    // already exposes. Pure local — no LLM, no embeddings, no network.
    // Returns the categories that should be ADDED to an existing filter.
    // Empty when no extra categories are warranted.
    //
    // Examples:
    //   "rebalance my portfolio"      → adds "portfolio"
    //   "draft a research note on…"   → adds "report-builder", "sec-edgar"
    //   "any news on Apple?"          → adds "news"
    //   "edgar 10-K for MSFT"         → adds "sec-edgar"
    //
    // Caller pattern (in LlmService::chat or wherever per-turn filter is
    // assembled):
    //   auto extras = ToolFilterPolicy::infer_extra_categories(user_msg);
    //   for (auto& c : extras)
    //     if (!filter.categories.contains(c)) filter.categories.append(c);
    //   llm.set_tool_filter(filter);
    static QStringList infer_extra_categories(const QString& user_message);
};

} // namespace fincept::mcp
