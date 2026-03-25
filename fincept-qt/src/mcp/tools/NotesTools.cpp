// NotesTools.cpp — Notes tab MCP tools (research notes)

#include "mcp/tools/NotesTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/NotesRepository.h"

#include <QVariantMap>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_notes_tools() {
    std::vector<ToolDef> tools;

    // ── create_note ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_note";
        t.description = "Create a research note with optional category, priority, tickers, and sentiment.";
        t.category = "notes";
        t.input_schema.properties = QJsonObject{
            {"title", QJsonObject{{"type", "string"}, {"description", "Note title"}}},
            {"content", QJsonObject{{"type", "string"}, {"description", "Note content (markdown)"}}},
            {"category",
             QJsonObject{{"type", "string"},
                         {"description",
                          "RESEARCH, TRADE_IDEA, MARKET_ANALYSIS, EARNINGS, ECONOMIC, PORTFOLIO, GENERAL"}}},
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

            EventBus::instance().publish("notes.created",
                                         QVariantMap{{"id", static_cast<qlonglong>(result.value())}});
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
        t.category = "notes";
        t.input_schema.properties =
            QJsonObject{{"query", QJsonObject{{"type", "string"}, {"description", "Search query (optional)"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            auto result = query.isEmpty() ? NotesRepository::instance().list_all()
                                          : NotesRepository::instance().search(query);

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
        t.category = "notes";
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

    return tools;
}

} // namespace fincept::mcp::tools
