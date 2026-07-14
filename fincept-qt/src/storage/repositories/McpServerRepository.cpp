#include "storage/repositories/McpServerRepository.h"

#include "storage/secure/SecureStorage.h"

namespace fincept {

namespace {
// The `env` blob holds an MCP server's environment variables, which routinely
// include API tokens. Encrypt it at rest in SecureStorage; reads fall back to
// the plaintext column so legacy rows keep working and nothing is lost.
QString mcp_env_secret_key(const QString& id) {
    return QStringLiteral("mcp:env:") + id;
}
} // namespace

McpServerRepository& McpServerRepository::instance() {
    static McpServerRepository s;
    return s;
}

McpServer McpServerRepository::map_row(QSqlQuery& q) {
    McpServer s{q.value(0).toString(), q.value(1).toString(), q.value(2).toString(),  q.value(3).toString(),
                q.value(4).toString(), q.value(5).toString(), q.value(6).toString(),  q.value(7).toString(),
                q.value(8).toBool(),   q.value(9).toBool(),   q.value(10).toString(), q.value(11).toString(),
                q.value(12).toString()};
    if (s.env.isEmpty() && !s.id.isEmpty()) {
        auto sr = SecureStorage::instance().retrieve(mcp_env_secret_key(s.id));
        if (sr.is_ok())
            s.env = sr.value();
    }
    return s;
}

static const char* kCols = "id, name, description, command, args, env, category, icon, enabled, "
                           "auto_start, status, created_at, updated_at";

Result<QVector<McpServer>> McpServerRepository::list_all() {
    return query_list(QString("SELECT %1 FROM mcp_servers ORDER BY name").arg(kCols), {}, map_row);
}

Result<McpServer> McpServerRepository::get(const QString& id) {
    return query_one(QString("SELECT %1 FROM mcp_servers WHERE id = ?").arg(kCols), {id}, map_row);
}

Result<void> McpServerRepository::save(const McpServer& s) {
    // Encrypt the env blob; blank the column only if the encrypted store worked.
    QString column_env = s.env;
    if (!s.env.isEmpty() && SecureStorage::instance().store(mcp_env_secret_key(s.id), s.env).is_ok())
        column_env.clear();
    return exec_write(
        "INSERT OR REPLACE INTO mcp_servers "
        "(id, name, description, command, args, env, category, icon, enabled, auto_start, status, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))",
        {s.id, s.name, s.description, s.command, s.args, column_env, s.category, s.icon, s.enabled ? 1 : 0,
         s.auto_start ? 1 : 0, s.status});
}

Result<void> McpServerRepository::remove(const QString& id) {
    SecureStorage::instance().remove(mcp_env_secret_key(id)); // drop the encrypted env too
    return exec_write("DELETE FROM mcp_servers WHERE id = ?", {id});
}

Result<void> McpServerRepository::set_status(const QString& id, const QString& status) {
    return exec_write("UPDATE mcp_servers SET status = ?, updated_at = datetime('now') WHERE id = ?", {status, id});
}

Result<void> McpServerRepository::set_enabled(const QString& id, bool enabled) {
    return exec_write("UPDATE mcp_servers SET enabled = ?, updated_at = datetime('now') WHERE id = ?",
                      {enabled ? 1 : 0, id});
}

} // namespace fincept
