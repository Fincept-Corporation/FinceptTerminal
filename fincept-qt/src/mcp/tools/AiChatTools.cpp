// AiChatTools.cpp — AI Chat tab MCP tools (session management)

#include "mcp/tools/AiChatTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "storage/repositories/ChatRepository.h"

#include <QVariantMap>

#include <algorithm>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_ai_chat_tools() {
    std::vector<ToolDef> tools;

    // ── create_chat_session ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_chat_session";
        t.description = "Create a new AI chat session.";
        t.category = "ai-chat";
        t.is_destructive = true; // persistent write that also refreshes the AI Chat session list
        // Title was previously declared `required` but the handler defaulted
        // to "New Chat" when missing — contradictory. Match the actual
        // handler behaviour: optional, default "New Chat".
        t.input_schema =
            ToolSchemaBuilder().string("title", "Chat session title").default_str("New Chat").length(1, 200).build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString title = args["title"].toString("New Chat");
            auto r = ChatRepository::instance().create_session(title);
            if (r.is_err())
                return ToolResult::fail("Failed to create session: " + QString::fromStdString(r.error()));

            const auto& session = r.value();
            // Phase 2.10: publish so AiChatScreen reloads its session list
            // immediately. ChatRepository has no Qt signals (it's a plain
            // BaseRepository<T>), so EventBus is the only path.
            EventBus::instance().publish("ai_chat.session_created",
                                         QVariantMap{{"id", session.id}, {"title", session.title}});
            return ToolResult::ok("Chat session created", QJsonObject{{"id", session.id}, {"title", session.title}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_chat_sessions ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_chat_sessions";
        t.description = "Get recent AI chat sessions (capped by `limit`).";
        t.category = "ai-chat";
        t.input_schema = ToolSchemaBuilder()
                             .integer("limit", "Max sessions to return")
                             .default_int(30)
                             .between(1, 200)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const int limit = std::clamp(args["limit"].toInt(30), 1, 200);
            auto sessions = ChatRepository::instance().list_sessions();
            if (sessions.is_err())
                return ToolResult::fail("Failed to load sessions: " + QString::fromStdString(sessions.error()));

            const auto total = sessions.value().size();
            QJsonArray arr;
            for (const auto& s : sessions.value()) {
                if (arr.size() >= limit)
                    break;
                arr.append(QJsonObject{{"id", s.id},
                                       {"title", s.title},
                                       {"message_count", s.message_count},
                                       {"created_at", s.created_at},
                                       {"updated_at", s.updated_at}});
            }
            // The tool is described as "recent" but used to return every session ever
            // created; cap it and say when the list was clipped (§M3/M4).
            if (total > arr.size())
                return ToolResult::ok(QStringLiteral("Showing %1 of %2 chat sessions — raise `limit` (max 200) to "
                                                     "see more.")
                                          .arg(arr.size())
                                          .arg(total),
                                      arr);
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
