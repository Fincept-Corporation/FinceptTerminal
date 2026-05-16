// AgenticMemoryTools.cpp — MCP tools for Letta-tier-3 archival memory.
//
// Two tools exposed to agents:
//   archival_memory_save(content, type?, metadata?)
//   archival_memory_search(query, k?, user_id?)
//
// Storage: agent_tasks.db sibling to the Python module. FTS5 search with a
// LIKE fallback for builds without FTS5. We open a fresh per-handler
// connection (tagged by thread id) so MCP's worker-thread invocations don't
// clash with the Qt main-thread.

#include "mcp/tools/AgenticMemoryTools.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QUuid>
#include <QVariant>

namespace fincept::mcp::tools {

namespace {
constexpr const char* TAG = "AgenticMemoryTools";

QString db_path() {
    // Mirrors archival_memory.py:_DEFAULT_DB → <scripts>/agents/agent_tasks.db
    const QString scripts = python::PythonRunner::instance().scripts_dir();
    return QDir(scripts).filePath("agents/agent_tasks.db");
}

QSqlDatabase open_conn() {
    // Per-thread connection. Each MCP handler invocation gets its own — Qt
    // requires QSqlDatabase access from the thread that opened it.
    const QString conn_name = QStringLiteral("agentic_mem_%1").arg(
        reinterpret_cast<qulonglong>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(conn_name)) {
        return QSqlDatabase::database(conn_name);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
    db.setDatabaseName(db_path());
    if (!db.open()) {
        LOG_ERROR(TAG, QString("Failed to open agent_tasks.db: ") + db.lastError().text());
    }
    return db;
}

// FTS5 needs sanitisation — strip everything that isn't alnum/space/-/_ and
// OR-join the terms. Mirrors the Python sanitiser.
QString fts_sanitize(const QString& raw) {
    QString cleaned;
    for (QChar c : raw) {
        if (c.isLetterOrNumber() || c == ' ' || c == '-' || c == '_')
            cleaned.append(c);
    }
    QStringList terms;
    for (const auto& t : cleaned.split(' ', Qt::SkipEmptyParts)) {
        if (t.length() >= 2) terms << t;
    }
    return terms.join(" OR ");
}
} // namespace

std::vector<ToolDef> get_agentic_memory_tools() {
    std::vector<ToolDef> tools;

    // ── archival_memory_save ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "archival_memory_save";
        t.description =
            "Persist a fact in the agent's long-term archival memory. Use for "
            "user preferences, prior conclusions, portfolio facts, or anything "
            "that should surface in future similar tasks. Searchable via "
            "archival_memory_search.";
        t.category = "agentic_memory";
        t.input_schema.properties = QJsonObject{
            {"content", QJsonObject{{"type", "string"}, {"description", "The fact to remember (one paragraph)"}}},
            {"type", QJsonObject{{"type", "string"},
                                  {"description", "Memory type tag, e.g. 'preference', 'conclusion', 'fact'"}}},
            {"user_id", QJsonObject{{"type", "string"}, {"description", "Owner identifier (optional)"}}},
        };
        t.input_schema.required = {"content"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString content = args["content"].toString().trimmed();
            if (content.isEmpty()) return ToolResult::fail("Missing 'content'");
            const QString type = args.value("type").toString("fact");
            const QString user_id = args.value("user_id").toString();

            QSqlDatabase db = open_conn();
            if (!db.isOpen()) return ToolResult::fail("Could not open agent_tasks.db");
            QSqlQuery q(db);
            q.prepare("INSERT INTO agent_memory_archival "
                      "(id, user_id, type, content, metadata_json, created_at) "
                      "VALUES (?, ?, ?, ?, '{}', ?)");
            const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            q.addBindValue(id);
            q.addBindValue(user_id.isEmpty() ? QVariant() : user_id);
            q.addBindValue(type);
            q.addBindValue(content);
            q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            if (!q.exec())
                return ToolResult::fail("Insert failed: " + q.lastError().text());
            return ToolResult::ok_data(QJsonObject{{"id", id}, {"type", type}});
        };
        tools.push_back(t);
    }

    // ── archival_memory_search ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "archival_memory_search";
        t.description =
            "Search the agent's long-term archival memory by content. Returns "
            "the top-k most relevant entries. Use BEFORE making any claim that "
            "relies on a remembered fact.";
        t.category = "agentic_memory";
        t.input_schema.properties = QJsonObject{
            {"query", QJsonObject{{"type", "string"}, {"description", "What to look for"}}},
            {"k", QJsonObject{{"type", "integer"}, {"description", "Max results (default 5)"}}},
            {"user_id", QJsonObject{{"type", "string"},
                                     {"description", "Filter to a specific owner (optional)"}}},
        };
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString query = args["query"].toString().trimmed();
            if (query.isEmpty()) return ToolResult::fail("Missing 'query'");
            const int k = args.value("k").toInt(5);
            const QString user_id = args.value("user_id").toString();

            QSqlDatabase db = open_conn();
            if (!db.isOpen()) return ToolResult::fail("Could not open agent_tasks.db");

            // Try FTS5 first; fall back to LIKE on any error (build without FTS).
            QJsonArray results;
            auto run_fts = [&]() -> bool {
                const QString cleaned = fts_sanitize(query);
                if (cleaned.isEmpty()) return false;
                QSqlQuery q(db);
                QString sql =
                    "SELECT m.id, m.type, m.content, m.user_id, m.created_at "
                    "FROM agent_memory_archival_fts f "
                    "JOIN agent_memory_archival m ON m.rowid = f.rowid "
                    "WHERE agent_memory_archival_fts MATCH ?";
                if (!user_id.isEmpty()) sql += " AND m.user_id = ?";
                sql += " ORDER BY bm25(agent_memory_archival_fts) ASC LIMIT ?";
                q.prepare(sql);
                q.addBindValue(cleaned);
                if (!user_id.isEmpty()) q.addBindValue(user_id);
                q.addBindValue(k);
                if (!q.exec()) return false;
                while (q.next()) {
                    results.append(QJsonObject{
                        {"id", q.value(0).toString()},
                        {"type", q.value(1).toString()},
                        {"content", q.value(2).toString()},
                        {"user_id", q.value(3).toString()},
                        {"created_at", q.value(4).toString()},
                    });
                }
                return true;
            };
            if (!run_fts()) {
                // Fallback: LIKE
                QSqlQuery q(db);
                QString sql = "SELECT id, type, content, user_id, created_at FROM agent_memory_archival "
                              "WHERE content LIKE ?";
                if (!user_id.isEmpty()) sql += " AND user_id = ?";
                sql += " ORDER BY created_at DESC LIMIT ?";
                q.prepare(sql);
                q.addBindValue("%" + query + "%");
                if (!user_id.isEmpty()) q.addBindValue(user_id);
                q.addBindValue(k);
                if (!q.exec())
                    return ToolResult::fail("Search failed: " + q.lastError().text());
                while (q.next()) {
                    results.append(QJsonObject{
                        {"id", q.value(0).toString()},
                        {"type", q.value(1).toString()},
                        {"content", q.value(2).toString()},
                        {"user_id", q.value(3).toString()},
                        {"created_at", q.value(4).toString()},
                    });
                }
            }
            return ToolResult::ok_data(QJsonObject{{"results", results}, {"count", results.size()}});
        };
        tools.push_back(t);
    }

    return tools;
}

} // namespace fincept::mcp::tools
