// AiChatTools.cpp — AI Chat tab MCP tools (session management)

#include "mcp/tools/AiChatTools.h"

#include "core/logging/Logger.h"
#include "storage/repositories/ChatRepository.h"

namespace fincept::mcp::tools {

std::vector<ToolDef> get_ai_chat_tools() {
    std::vector<ToolDef> tools;

    // ── create_chat_session ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_chat_session";
        t.description = "Create a new AI chat session.";
        t.category = "ai-chat";
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
        t.category = "ai-chat";
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
