#pragma once
// McpServerTypes.h — value types of the terminal's external MCP server shared
// by the engine (McpServerEngine), the public facade (McpHttpServer) and the
// settings UI. QtCore only.

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace fincept::mcp {

/// One external tool call, for the Connect tab's activity log. Never holds
/// arguments or results verbatim (they may contain account data) — only a short
/// summary.
struct ExternalCallRecord {
    QDateTime at;
    QString client;          // clientInfo.name from `initialize` ("claude-code", "codex-mcp-client", …) or "unknown"
    QString tool;            // bare tool name as the client called it
    bool success = false;
    bool denied = false;     // refused by policy or by the user
    qint64 duration_ms = 0;
    QString summary;         // short status / error text, never secrets
};

/// One client that said hello since start(), for the settings page.
struct ExternalClientInfo {
    QString name;       // clientInfo.name ("claude-code", "codex-mcp-client", …)
    QString user_agent; // HTTP User-Agent, trimmed
    QDateTime first_seen;
    QDateTime last_seen;
    int calls = 0;
};

/// Counters since start(), for the settings page.
struct ExternalServerStats {
    QDateTime started_at;
    quint64 calls = 0;
    quint64 ok = 0;
    quint64 failed = 0;
    quint64 denied = 0;
};

} // namespace fincept::mcp

Q_DECLARE_METATYPE(fincept::mcp::ExternalCallRecord)
Q_DECLARE_METATYPE(fincept::mcp::ExternalClientInfo)
