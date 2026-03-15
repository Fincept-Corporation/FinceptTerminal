// Portfolio MCP Tools — notes, chat sessions, data management

#include "portfolio_tools.h"
#include "../../storage/database.h"
#include "../../core/event_bus.h"

namespace fincept::mcp::tools {

std::vector<ToolDef> get_portfolio_tools() {
    std::vector<ToolDef> tools;

    // ── create_note ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_note";
        t.description = "Create a research note with optional category, priority, tickers, and sentiment.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"title", {{"type", "string"}, {"description", "Note title"}}},
            {"content", {{"type", "string"}, {"description", "Note content (markdown)"}}},
            {"category", {{"type", "string"}, {"description", "RESEARCH, TRADE_IDEA, MARKET_ANALYSIS, EARNINGS, ECONOMIC, PORTFOLIO, GENERAL"}}},
            {"priority", {{"type", "string"}, {"description", "HIGH, MEDIUM, LOW"}}},
            {"tickers", {{"type", "string"}, {"description", "Comma-separated ticker symbols"}}},
            {"sentiment", {{"type", "string"}, {"description", "BULLISH, BEARISH, NEUTRAL"}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}}
        };
        t.input_schema.required = {"title", "content"};
        t.handler = [](const json& args) -> ToolResult {
            db::Note note;
            note.title = args.value("title", "");
            note.content = args.value("content", "");
            note.category = args.value("category", "GENERAL");
            note.priority = args.value("priority", "MEDIUM");
            note.tickers = args.value("tickers", "");
            note.sentiment = args.value("sentiment", "NEUTRAL");
            note.tags = args.value("tags", "");

            if (note.title.empty()) return ToolResult::fail("Missing 'title'");

            auto created = db::ops::create_note(note);

            core::EventBus::instance().publish("notes.created", {{"id", created.id}});

            return ToolResult::ok("Note created: " + note.title, {
                {"id", created.id}, {"title", created.title}
            });
        };
        tools.push_back(std::move(t));
    }

    // ── get_notes ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_notes";
        t.description = "Get all research notes, optionally filtered by search query.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"query", {{"type", "string"}, {"description", "Search query (optional)"}}}
        };
        t.handler = [](const json& args) -> ToolResult {
            std::string query = args.value("query", "");

            std::vector<db::Note> notes;
            if (query.empty()) {
                notes = db::ops::get_all_notes();
            } else {
                notes = db::ops::search_notes(query);
            }

            json result = json::array();
            for (const auto& n : notes) {
                result.push_back({
                    {"id", n.id},
                    {"title", n.title},
                    {"category", n.category},
                    {"priority", n.priority},
                    {"tickers", n.tickers},
                    {"sentiment", n.sentiment},
                    {"is_favorite", n.is_favorite},
                    {"created_at", n.created_at},
                    {"updated_at", n.updated_at}
                });
            }

            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── delete_note ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_note";
        t.description = "Delete a research note by ID.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"id", {{"type", "integer"}, {"description", "Note ID to delete"}}}
        };
        t.input_schema.required = {"id"};
        t.handler = [](const json& args) -> ToolResult {
            if (!args.contains("id")) return ToolResult::fail("Missing 'id'");
            int64_t id = args["id"].get<int64_t>();
            db::ops::delete_note(id);
            core::EventBus::instance().publish("notes.deleted", {{"id", id}});
            return ToolResult::ok("Note deleted");
        };
        tools.push_back(std::move(t));
    }

    // ── create_chat_session ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_chat_session";
        t.description = "Create a new AI chat session.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"title", {{"type", "string"}, {"description", "Chat session title"}}}
        };
        t.input_schema.required = {"title"};
        t.handler = [](const json& args) -> ToolResult {
            std::string title = args.value("title", "New Chat");
            auto session = db::ops::create_chat_session(title);
            return ToolResult::ok("Chat session created", {
                {"session_uuid", session.session_uuid},
                {"title", session.title}
            });
        };
        tools.push_back(std::move(t));
    }

    // ── get_chat_sessions ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_chat_sessions";
        t.description = "Get recent AI chat sessions.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"limit", {{"type", "integer"}, {"description", "Max sessions to return (default: 20)"}}}
        };
        t.handler = [](const json& args) -> ToolResult {
            int limit = args.value("limit", 20);
            auto sessions = db::ops::get_chat_sessions(limit);
            json result = json::array();
            for (const auto& s : sessions) {
                result.push_back({
                    {"session_uuid", s.session_uuid},
                    {"title", s.title},
                    {"message_count", s.message_count},
                    {"created_at", s.created_at},
                    {"updated_at", s.updated_at}
                });
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
