#include "database.h"
#include "core/logger.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace fincept::db {
namespace ops {

// Operations — Chat
// ============================================================================
ChatSession create_chat_session(const std::string& title) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string uuid = generate_uuid();
    auto stmt = db.prepare("INSERT INTO chat_sessions (session_uuid, title) VALUES (?1, ?2)");
    stmt.bind_text(1, uuid);
    stmt.bind_text(2, title);
    stmt.execute();

    auto q = db.prepare(
        "SELECT session_uuid, title, message_count, created_at, updated_at "
        "FROM chat_sessions WHERE session_uuid = ?1");
    q.bind_text(1, uuid);
    q.step();
    return {q.col_text(0), q.col_text(1), q.col_int(2), q.col_text(3), q.col_text(4)};
}

std::vector<ChatSession> get_chat_sessions(int limit) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    std::string sql = "SELECT session_uuid, title, message_count, created_at, updated_at "
                      "FROM chat_sessions ORDER BY updated_at DESC";
    if (limit > 0) sql += " LIMIT " + std::to_string(limit);
    auto stmt = db.prepare(sql.c_str());
    std::vector<ChatSession> result;
    while (stmt.step()) {
        result.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_int(2), stmt.col_text(3), stmt.col_text(4)});
    }
    return result;
}

ChatMessage add_chat_message(const ChatMessage& msg) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "INSERT INTO chat_messages (id, session_uuid, role, content, provider, model, tokens_used) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)");
    stmt.bind_text(1, msg.id);
    stmt.bind_text(2, msg.session_uuid);
    stmt.bind_text(3, msg.role);
    stmt.bind_text(4, msg.content);
    msg.provider    ? stmt.bind_text(5, *msg.provider)      : stmt.bind_null(5);
    msg.model       ? stmt.bind_text(6, *msg.model)         : stmt.bind_null(6);
    msg.tokens_used ? stmt.bind_int(7, *msg.tokens_used)    : stmt.bind_null(7);
    stmt.execute();

    auto upd = db.prepare(
        "UPDATE chat_sessions SET message_count = message_count + 1, updated_at = CURRENT_TIMESTAMP "
        "WHERE session_uuid = ?1");
    upd.bind_text(1, msg.session_uuid);
    upd.execute();

    auto q = db.prepare(
        "SELECT id, session_uuid, role, content, timestamp, provider, model, tokens_used "
        "FROM chat_messages WHERE id = ?1");
    q.bind_text(1, msg.id);
    q.step();
    std::optional<int64_t> tokens;
    if (sqlite3_column_type(q.raw(), 7) != SQLITE_NULL) tokens = q.col_int(7);
    return {q.col_text(0), q.col_text(1), q.col_text(2), q.col_text(3),
            q.col_text(4), q.col_text_opt(5), q.col_text_opt(6), tokens};
}

std::vector<ChatMessage> get_chat_messages(const std::string& session_uuid) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "SELECT id, session_uuid, role, content, timestamp, provider, model, tokens_used "
        "FROM chat_messages WHERE session_uuid = ?1 ORDER BY timestamp ASC");
    stmt.bind_text(1, session_uuid);
    std::vector<ChatMessage> result;
    while (stmt.step()) {
        std::optional<int64_t> tokens;
        if (sqlite3_column_type(stmt.raw(), 7) != SQLITE_NULL) tokens = stmt.col_int(7);
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
            stmt.col_text(4), stmt.col_text_opt(5), stmt.col_text_opt(6), tokens
        });
    }
    return result;
}

void delete_chat_session(const std::string& session_uuid) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    // Delete messages first, then session
    auto stmt1 = db.prepare("DELETE FROM chat_messages WHERE session_uuid = ?1");
    stmt1.bind_text(1, session_uuid);
    stmt1.execute();
    auto stmt2 = db.prepare("DELETE FROM chat_sessions WHERE session_uuid = ?1");
    stmt2.bind_text(1, session_uuid);
    stmt2.execute();
}

void update_chat_session_title(const std::string& session_uuid, const std::string& title) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare(
        "UPDATE chat_sessions SET title = ?1, updated_at = datetime('now') WHERE session_uuid = ?2");
    stmt.bind_text(1, title);
    stmt.bind_text(2, session_uuid);
    stmt.execute();
}

void clear_chat_session_messages(const std::string& session_uuid) {
    auto& db = Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());
    auto stmt = db.prepare("DELETE FROM chat_messages WHERE session_uuid = ?1");
    stmt.bind_text(1, session_uuid);
    stmt.execute();
    // Reset message count
    auto stmt2 = db.prepare(
        "UPDATE chat_sessions SET message_count = 0, updated_at = datetime('now') WHERE session_uuid = ?1");
    stmt2.bind_text(1, session_uuid);
    stmt2.execute();
}

// ============================================================================

} // namespace ops
} // namespace fincept::db
