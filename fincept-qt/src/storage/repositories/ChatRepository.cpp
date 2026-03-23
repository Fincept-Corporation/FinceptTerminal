#include "storage/repositories/ChatRepository.h"

#include <QUuid>

namespace fincept {

ChatRepository& ChatRepository::instance() {
    static ChatRepository s;
    return s;
}

ChatSession ChatRepository::map_session(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
            q.value(4).toInt(),    q.value(5).toString(), q.value(6).toString()};
}

ChatMessage ChatRepository::map_message(QSqlQuery& q) {
    return {q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
            q.value(4).toString(), q.value(5).toString(), q.value(6).toInt(),    q.value(7).toString()};
}

Result<ChatSession> ChatRepository::create_session(const QString& title, const QString& provider,
                                                   const QString& model) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write("INSERT INTO chat_sessions (id, title, provider, model) VALUES (?, ?, ?, ?)",
                        {id, title, provider, model});
    if (r.is_err())
        return Result<ChatSession>::err(r.error());
    return get_session(id);
}

Result<ChatSession> ChatRepository::get_session(const QString& id) {
    return query_one("SELECT id, title, provider, model, message_count, created_at, updated_at "
                     "FROM chat_sessions WHERE id = ?",
                     {id}, map_session);
}

Result<QVector<ChatSession>> ChatRepository::list_sessions() {
    return query_list("SELECT id, title, provider, model, message_count, created_at, updated_at "
                      "FROM chat_sessions ORDER BY updated_at DESC",
                      {}, map_session);
}

Result<void> ChatRepository::update_session_title(const QString& id, const QString& title) {
    return exec_write("UPDATE chat_sessions SET title = ?, updated_at = datetime('now') WHERE id = ?", {title, id});
}

Result<void> ChatRepository::delete_session(const QString& id) {
    return exec_write("DELETE FROM chat_sessions WHERE id = ?", {id});
}

Result<void> ChatRepository::add_message(const QString& session_id, const QString& role, const QString& content,
                                         const QString& provider, const QString& model, int tokens) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write("INSERT INTO chat_messages (id, session_id, role, content, provider, model, tokens_used) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?)",
                        {id, session_id, role, content, provider, model, tokens});
    if (r.is_err())
        return r;

    // Update message count
    return exec_write("UPDATE chat_sessions SET message_count = message_count + 1, "
                      "updated_at = datetime('now') WHERE id = ?",
                      {session_id});
}

Result<QVector<ChatMessage>> ChatRepository::get_messages(const QString& session_id) {
    auto r = db().execute("SELECT id, session_id, role, content, provider, model, tokens_used, timestamp "
                          "FROM chat_messages WHERE session_id = ? ORDER BY timestamp ASC",
                          {session_id});
    if (r.is_err())
        return Result<QVector<ChatMessage>>::err(r.error());
    QVector<ChatMessage> result;
    auto& q = r.value();
    while (q.next())
        result.append(map_message(q));
    return Result<QVector<ChatMessage>>::ok(std::move(result));
}

Result<void> ChatRepository::delete_messages(const QString& session_id) {
    return exec_write("DELETE FROM chat_messages WHERE session_id = ?", {session_id});
}

} // namespace fincept
