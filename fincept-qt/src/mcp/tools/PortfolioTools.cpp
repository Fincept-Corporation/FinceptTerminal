// PortfolioTools.cpp — Notes and chat session MCP tools (Qt port)

#include "mcp/tools/PortfolioTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/ChatRepository.h"
#include "storage/repositories/NotesRepository.h"

#include <QVariantMap>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_portfolio_tools() {
    std::vector<ToolDef> tools;

    // ── create_note ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_note";
        t.description = "Create a research note with optional category, priority, tickers, and sentiment.";
        t.category = "portfolio";
        t.input_schema.properties = QJsonObject{
            {"title", QJsonObject{{"type", "string"}, {"description", "Note title"}}},
            {"content", QJsonObject{{"type", "string"}, {"description", "Note content (markdown)"}}},
            {"category",
             QJsonObject{
                 {"type", "string"},
                 {"description", "RESEARCH, TRADE_IDEA, MARKET_ANALYSIS, EARNINGS, ECONOMIC, PORTFOLIO, GENERAL"}}},
            {"priority", QJsonObject{{"type", "string"}, {"description", "HIGH, MEDIUM, LOW"}}},
            {"tickers", QJsonObject{{"type", "string"}, {"description", "Comma-separated ticker symbols"}}},
            {"sentiment", QJsonObject{{"type", "string"}, {"description", "BULLISH, BEARISH, NEUTRAL"}}},
            {"tags", QJsonObject{{"type", "string"}, {"description", "Comma-separated tags"}}}};
        t.input_schema.required = {"title", "content"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString title = args["title"].toString().trimmed();
            if (title.isEmpty())
                return ToolResult::fail("Missing 'title'");

            FinancialNote note;
            note.title = title;
            note.content = args["content"].toString();
            note.category = args["category"].toString("GENERAL");
            note.priority = args["priority"].toString("MEDIUM");
            note.tickers = args["tickers"].toString();
            note.sentiment = args["sentiment"].toString("NEUTRAL");
            note.tags = args["tags"].toString();

            auto result = NotesRepository::instance().create(note);
            if (result.is_err())
                return ToolResult::fail("Failed to create note: " + QString::fromStdString(result.error()));

            EventBus::instance().publish("notes.created", QVariantMap{{"id", static_cast<qlonglong>(result.value())}});

            return ToolResult::ok("Note created: " + title,
                                  QJsonObject{{"id", static_cast<int>(result.value())}, {"title", title}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_notes ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_notes";
        t.description = "Get all research notes, optionally filtered by search query.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"query", QJsonObject{{"type", "string"}, {"description", "Search query (optional)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();

            auto result =
                query.isEmpty() ? NotesRepository::instance().list_all() : NotesRepository::instance().search(query);

            if (result.is_err())
                return ToolResult::fail("Failed to load notes: " + QString::fromStdString(result.error()));

            QJsonArray arr;
            for (const auto& n : result.value()) {
                arr.append(QJsonObject{{"id", n.id},
                                       {"title", n.title},
                                       {"category", n.category},
                                       {"priority", n.priority},
                                       {"tickers", n.tickers},
                                       {"sentiment", n.sentiment},
                                       {"is_favorite", n.is_favorite},
                                       {"created_at", n.created_at},
                                       {"updated_at", n.updated_at}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── delete_note ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_note";
        t.description = "Delete a research note by ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "integer"}, {"description", "Note ID to delete"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args["id"].toInt(-1);
            if (id < 0)
                return ToolResult::fail("Missing or invalid 'id'");

            auto r = NotesRepository::instance().remove(id);
            if (r.is_err())
                return ToolResult::fail("Failed to delete note: " + QString::fromStdString(r.error()));

            EventBus::instance().publish("notes.deleted", QVariantMap{{"id", id}});
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
        t.input_schema.properties =
            QJsonObject{{"title", QJsonObject{{"type", "string"}, {"description", "Chat session title"}}}};
        t.input_schema.required = {"title"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString title = args["title"].toString("New Chat");

            auto r = ChatRepository::instance().create_session(title);
            if (r.is_err())
                return ToolResult::fail("Failed to create session: " + QString::fromStdString(r.error()));

            const auto& session = r.value();
            return ToolResult::ok("Chat session created", QJsonObject{{"id", session.id}, {"title", session.title}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_chat_sessions ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_chat_sessions";
        t.description = "Get recent AI chat sessions.";
        t.category = "portfolio";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto sessions = ChatRepository::instance().list_sessions();
            if (sessions.is_err())
                return ToolResult::fail("Failed to load sessions: " + QString::fromStdString(sessions.error()));

            QJsonArray arr;
            for (const auto& s : sessions.value()) {
                arr.append(QJsonObject{{"id", s.id},
                                       {"title", s.title},
                                       {"message_count", s.message_count},
                                       {"created_at", s.created_at},
                                       {"updated_at", s.updated_at}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
