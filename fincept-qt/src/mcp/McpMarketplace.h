#pragma once
// McpMarketplace.h — Shared catalog of preset external MCP servers.
// Consumed by McpServersScreen (UI marketplace tab) and McpServersTools
// (LLM-facing list_mcp_marketplace / install_mcp_server_from_marketplace).

#include <QList>
#include <QString>
#include <QStringList>

namespace fincept::mcp {

struct MarketplaceEntry {
    QString name;
    QString description;
    QString command;
    QStringList args;
    QStringList env_keys;         // env var names the user must fill in
    QStringList env_placeholders; // sample values shown as placeholder text
    QString category;
};

inline const QList<MarketplaceEntry>& marketplace_catalog() {
    static const QList<MarketplaceEntry> kCatalog = {
        {"Fetch",
         "HTTP fetch and web content retrieval — lets the AI read any URL.",
         "uvx",
         {"mcp-server-fetch"},
         {},
         {},
         "utilities"},
        {"Time",
         "Current time, timezone conversion and date arithmetic.",
         "uvx",
         {"mcp-server-time"},
         {},
         {},
         "utilities"},
        {"Git",
         "Git repository operations: log, diff, commit, branch, status.",
         "uvx",
         {"mcp-server-git"},
         {},
         {},
         "developer"},
        {"SQLite",
         "SQLite database query and schema inspection.",
         "uvx",
         {"mcp-server-sqlite"},
         {},
         {},
         "database"},
        {"PostgreSQL",
         "Query and explore a PostgreSQL database schema and data.",
         "uvx",
         {"postgres-mcp", "--access-mode=unrestricted"},
         {"DATABASE_URI"},
         {"postgresql://user:password@localhost:5432/mydb"},
         "database"},
    };
    return kCatalog;
}

} // namespace fincept::mcp
