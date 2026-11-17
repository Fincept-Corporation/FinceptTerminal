// src/storage/repositories/NoteBoardRepository.h
//
// The Notes board tables (migration v066; row types in NoteBoardTypes.h). Kept
// apart from NotesRepository on purpose: financial_notes and NotesRepository's
// functions are a contract other readers rely on (the dashboard Notes tile, Chat
// Mode's "Send to Notes", MCP tools, cloud sync), so the board's own attributes
// live in side tables keyed by note id. A delete trigger on financial_notes
// removes a note's board rows with it; prune_orphans() sweeps anything left
// behind by an older build.
//
// Every function is called on the Notes screen's database thread (NotesStore) —
// except insert_missing_meta(), which Chat Mode's "Send to Notes" also calls on
// ArtefactService's worker (each thread has its own connection, see Database.h);
// batch writes run in one transaction.
#pragma once

#include "storage/repositories/BaseRepository.h"
#include "storage/repositories/NoteBoardTypes.h"

#include <QPair>
#include <QVector>

namespace fincept {

class NoteBoardRepository : public BaseRepository<notes_board::NoteBoardMeta> {
  public:
    static NoteBoardRepository& instance();

    // ── Per-note board rows ─────────────────────────────────────────────────
    Result<QVector<notes_board::NoteBoardMeta>> load_meta();
    /// Insert or replace each row (one transaction).
    Result<void> upsert_meta(const QVector<notes_board::NoteBoardMeta>& rows);
    /// Insert the rows that do not exist yet; existing rows are left alone.
    Result<void> insert_missing_meta(const QVector<notes_board::NoteBoardMeta>& rows);

    // ── History ─────────────────────────────────────────────────────────────
    Result<void> add_events(const QVector<notes_board::NoteBoardEvent>& events);
    /// Newest first.
    Result<QVector<notes_board::NoteBoardEvent>> events_for(int note_id, int limit = 60);

    // ── Boards ──────────────────────────────────────────────────────────────
    Result<QVector<notes_board::NoteBoardRecord>> list_boards();
    /// Insert (id 0) or update; returns the id.
    Result<qint64> save_board(const notes_board::NoteBoardRecord& board);
    /// The board with its wall positions and groups.
    Result<void> remove_board(qint64 board_id);

    // ── Wall ────────────────────────────────────────────────────────────────
    Result<QVector<notes_board::NoteWallItem>> wall_items(qint64 board_id);
    Result<void> upsert_wall_items(qint64 board_id, const QVector<notes_board::NoteWallItem>& items);
    Result<QVector<notes_board::NoteWallGroup>> wall_groups(qint64 board_id);
    /// Insert (id 0) or update; returns the id.
    Result<qint64> save_wall_group(const notes_board::NoteWallGroup& group);
    Result<void> remove_wall_group(qint64 group_id);

    /// Delete board rows, history and wall positions of notes that no longer exist.
    Result<void> prune_orphans();

  private:
    NoteBoardRepository() = default;
    /// Run `rows` through `sql` in one transaction (rolled back on failure).
    Result<void> batch(const QString& sql, const QVector<QVariantList>& rows);
};

} // namespace fincept
