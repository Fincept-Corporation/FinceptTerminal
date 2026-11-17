// src/storage/repositories/NoteBoardRepository.cpp
#include "storage/repositories/NoteBoardRepository.h"

#include <QDateTime>

#include <algorithm>

namespace fincept {

using notes_board::NoteBoardEvent;
using notes_board::NoteBoardMeta;
using notes_board::NoteBoardRecord;
using notes_board::NoteWallGroup;
using notes_board::NoteWallItem;

namespace {

NoteBoardMeta nbr_map_meta(QSqlQuery& q) {
    NoteBoardMeta m;
    m.note_id = q.value(0).toInt();
    m.status = notes_board::normalize_status(q.value(1).toString());
    m.rank = q.value(2).toDouble();
    m.review_on = q.value(3).toString();
    m.origin = q.value(4).toString();
    m.origin_ref = q.value(5).toString();
    m.status_changed_ms = q.value(6).toLongLong();
    m.reviewed_ms = q.value(7).toLongLong();
    m.stored = true;
    return m;
}

QVariantList nbr_meta_params(const NoteBoardMeta& m) {
    return {m.note_id,    notes_board::normalize_status(m.status), m.rank,
            m.review_on,  m.origin,
            m.origin_ref, m.status_changed_ms,
            m.reviewed_ms};
}

qint64 nbr_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

NoteBoardRepository& NoteBoardRepository::instance() {
    static NoteBoardRepository s;
    return s;
}

Result<void> NoteBoardRepository::batch(const QString& sql, const QVector<QVariantList>& rows) {
    if (rows.isEmpty())
        return Result<void>::ok();
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return t;
    auto r = d.execute_many(sql, rows);
    if (r.is_err()) {
        d.rollback();
        LOG_ERROR("NotesBoard", QString("Batch write failed rows=%1 error=\"%2\" %3")
                                    .arg(rows.size())
                                    .arg(QString::fromStdString(r.error()), sql_context(sql, rows.first())));
        return r;
    }
    return d.commit();
}

// ── Per-note rows ────────────────────────────────────────────────────────────

Result<QVector<NoteBoardMeta>> NoteBoardRepository::load_meta() {
    return query_list(QStringLiteral("SELECT note_id, status, sort_rank, review_on, origin, origin_ref, "
                                     "status_changed_at, reviewed_at FROM note_board_meta"),
                      {}, nbr_map_meta);
}

Result<void> NoteBoardRepository::upsert_meta(const QVector<NoteBoardMeta>& rows) {
    QVector<QVariantList> params;
    params.reserve(rows.size());
    for (const auto& m : rows)
        if (m.note_id > 0)
            params << nbr_meta_params(m);
    return batch(QStringLiteral("INSERT INTO note_board_meta (note_id, status, sort_rank, review_on, origin, origin_ref, "
                                "status_changed_at, reviewed_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                                "ON CONFLICT(note_id) DO UPDATE SET status = excluded.status, "
                                "sort_rank = excluded.sort_rank, review_on = excluded.review_on, "
                                "origin = excluded.origin, origin_ref = excluded.origin_ref, "
                                "status_changed_at = excluded.status_changed_at, reviewed_at = excluded.reviewed_at"),
                 params);
}

Result<void> NoteBoardRepository::insert_missing_meta(const QVector<NoteBoardMeta>& rows) {
    QVector<QVariantList> params;
    params.reserve(rows.size());
    for (const auto& m : rows)
        if (m.note_id > 0)
            params << nbr_meta_params(m);
    // Only for notes that still exist (a note deleted meanwhile must not get a row).
    return batch(QStringLiteral("INSERT OR IGNORE INTO note_board_meta (note_id, status, sort_rank, review_on, origin, "
                                "origin_ref, status_changed_at, reviewed_at) "
                                "SELECT ?, ?, ?, ?, ?, ?, ?, ? WHERE EXISTS (SELECT 1 FROM financial_notes WHERE id = ?)"),
                 [&params]() {
                     QVector<QVariantList> p = params;
                     for (auto& row : p)
                         row << row.first();
                     return p;
                 }());
}

// ── History ──────────────────────────────────────────────────────────────────

Result<void> NoteBoardRepository::add_events(const QVector<NoteBoardEvent>& events) {
    QVector<QVariantList> params;
    params.reserve(events.size());
    for (const auto& e : events)
        if (e.note_id > 0)
            params << QVariantList{e.note_id, e.at_ms > 0 ? e.at_ms : nbr_now_ms(), e.kind, e.from_value, e.to_value};
    return batch(QStringLiteral("INSERT INTO note_board_history (note_id, at, kind, from_value, to_value) "
                                "SELECT ?, ?, ?, ?, ? WHERE EXISTS (SELECT 1 FROM financial_notes WHERE id = ?)"),
                 [&params]() {
                     QVector<QVariantList> p = params;
                     for (auto& row : p)
                         row << row.first();
                     return p;
                 }());
}

Result<QVector<NoteBoardEvent>> NoteBoardRepository::events_for(int note_id, int limit) {
    return query_list_as<NoteBoardEvent>(
        QStringLiteral("SELECT id, note_id, at, kind, from_value, to_value FROM note_board_history "
                       "WHERE note_id = ? ORDER BY at DESC, id DESC LIMIT ?"),
        {note_id, std::clamp(limit, 1, 500)}, [](QSqlQuery& q) {
            NoteBoardEvent e;
            e.id = q.value(0).toLongLong();
            e.note_id = q.value(1).toInt();
            e.at_ms = q.value(2).toLongLong();
            e.kind = q.value(3).toString();
            e.from_value = q.value(4).toString();
            e.to_value = q.value(5).toString();
            return e;
        });
}

// ── Boards ───────────────────────────────────────────────────────────────────

Result<QVector<NoteBoardRecord>> NoteBoardRepository::list_boards() {
    return query_list_as<NoteBoardRecord>(
        QStringLiteral("SELECT id, name, position, definition, created_at, updated_at FROM note_boards "
                       "ORDER BY position, id"),
        {}, [](QSqlQuery& q) {
            NoteBoardRecord b;
            b.id = q.value(0).toLongLong();
            b.name = q.value(1).toString();
            b.position = q.value(2).toInt();
            b.definition_json = q.value(3).toString();
            b.created_ms = q.value(4).toLongLong();
            b.updated_ms = q.value(5).toLongLong();
            return b;
        });
}

Result<qint64> NoteBoardRepository::save_board(const NoteBoardRecord& b) {
    const qint64 now = nbr_now_ms();
    if (b.id > 0) {
        auto r = exec_write(QStringLiteral("UPDATE note_boards SET name = ?, position = ?, definition = ?, updated_at = ? "
                                           "WHERE id = ?"),
                            {b.name, b.position, b.definition_json, now, b.id});
        if (r.is_err())
            return Result<qint64>::err(r.error());
        return Result<qint64>::ok(b.id);
    }
    return exec_insert(QStringLiteral("INSERT INTO note_boards (name, position, definition, created_at, updated_at) "
                                      "VALUES (?, ?, ?, ?, ?)"),
                       {b.name, b.position, b.definition_json, now, now});
}

Result<void> NoteBoardRepository::remove_board(qint64 board_id) {
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return t;
    for (const char* sql : {"DELETE FROM note_wall_items WHERE board_id = ?", "DELETE FROM note_wall_groups WHERE board_id = ?",
                            "DELETE FROM note_boards WHERE id = ?"}) {
        auto r = d.execute(QString::fromLatin1(sql), {board_id});
        if (r.is_err()) {
            d.rollback();
            LOG_ERROR("NotesBoard", QString("Could not delete board board_id=%1 error=\"%2\"")
                                        .arg(board_id)
                                        .arg(QString::fromStdString(r.error())));
            return Result<void>::err(r.error());
        }
    }
    return d.commit();
}

// ── Wall ─────────────────────────────────────────────────────────────────────

Result<QVector<NoteWallItem>> NoteBoardRepository::wall_items(qint64 board_id) {
    return query_list_as<NoteWallItem>(QStringLiteral("SELECT note_id, x, y FROM note_wall_items WHERE board_id = ?"),
                                       {board_id}, [](QSqlQuery& q) {
                                           NoteWallItem w;
                                           w.note_id = q.value(0).toInt();
                                           w.x = q.value(1).toDouble();
                                           w.y = q.value(2).toDouble();
                                           return w;
                                       });
}

Result<void> NoteBoardRepository::upsert_wall_items(qint64 board_id, const QVector<NoteWallItem>& items) {
    QVector<QVariantList> params;
    params.reserve(items.size());
    for (const auto& w : items)
        if (w.note_id > 0)
            params << QVariantList{board_id, w.note_id, w.x, w.y};
    return batch(QStringLiteral("INSERT INTO note_wall_items (board_id, note_id, x, y) VALUES (?, ?, ?, ?) "
                                "ON CONFLICT(board_id, note_id) DO UPDATE SET x = excluded.x, y = excluded.y"),
                 params);
}

Result<QVector<NoteWallGroup>> NoteBoardRepository::wall_groups(qint64 board_id) {
    return query_list_as<NoteWallGroup>(
        QStringLiteral("SELECT id, board_id, title, x, y, w, h, tone FROM note_wall_groups WHERE board_id = ? ORDER BY id"),
        {board_id}, [](QSqlQuery& q) {
            NoteWallGroup g;
            g.id = q.value(0).toLongLong();
            g.board_id = q.value(1).toLongLong();
            g.title = q.value(2).toString();
            g.x = q.value(3).toDouble();
            g.y = q.value(4).toDouble();
            g.w = q.value(5).toDouble();
            g.h = q.value(6).toDouble();
            g.tone = q.value(7).toInt();
            return g;
        });
}

Result<qint64> NoteBoardRepository::save_wall_group(const NoteWallGroup& g) {
    if (g.id > 0) {
        auto r = exec_write(QStringLiteral("UPDATE note_wall_groups SET title = ?, x = ?, y = ?, w = ?, h = ?, tone = ? "
                                           "WHERE id = ?"),
                            {g.title, g.x, g.y, g.w, g.h, g.tone, g.id});
        if (r.is_err())
            return Result<qint64>::err(r.error());
        return Result<qint64>::ok(g.id);
    }
    return exec_insert(QStringLiteral("INSERT INTO note_wall_groups (board_id, title, x, y, w, h, tone) "
                                      "VALUES (?, ?, ?, ?, ?, ?, ?)"),
                       {g.board_id, g.title, g.x, g.y, g.w, g.h, g.tone});
}

Result<void> NoteBoardRepository::remove_wall_group(qint64 group_id) {
    return exec_write(QStringLiteral("DELETE FROM note_wall_groups WHERE id = ?"), {group_id});
}

Result<void> NoteBoardRepository::prune_orphans() {
    auto& d = db();
    auto t = d.begin_transaction();
    if (t.is_err())
        return t;
    for (const char* sql : {"DELETE FROM note_board_meta WHERE note_id NOT IN (SELECT id FROM financial_notes)",
                            "DELETE FROM note_board_history WHERE note_id NOT IN (SELECT id FROM financial_notes)",
                            "DELETE FROM note_wall_items WHERE note_id NOT IN (SELECT id FROM financial_notes) "
                            "OR board_id NOT IN (SELECT id FROM note_boards)"}) {
        auto r = d.execute(QString::fromLatin1(sql));
        if (r.is_err()) {
            d.rollback();
            return Result<void>::err(r.error());
        }
    }
    return d.commit();
}

} // namespace fincept
