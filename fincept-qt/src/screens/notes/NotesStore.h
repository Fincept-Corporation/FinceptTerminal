// src/screens/notes/NotesStore.h
//
// The Notes screen's access to NotesRepository and NoteBoardRepository, off the
// GUI thread.
//
// Every database call runs on one private worker thread, so writes keep their
// order (an autosave queued before a delete is written before it) and a slow
// SQLite lock (busy_timeout is 5 s) never freezes the window. Results come back
// on the GUI thread. Building the board (filter, group, sort, statistics) over a
// large library runs on a second worker so it is never queued behind writes.
//
// The *_now() variants run on the calling (GUI) thread after the queue has
// drained — only for quit, destruction, lock and an explicit layout flush, when
// the write must be on disk before the call returns.
#pragma once

#include "screens/notes/NotesBoardLogic.h"
#include "storage/repositories/NoteBoardTypes.h"
#include "storage/repositories/NotesRepository.h"

#include <QObject>
#include <QThreadPool>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace fincept::screens::notes {

using NoteEntry = BasicEntry<fincept::FinancialNote>;
using BoardLane = BasicLane<NoteEntry>;
using BoardLayout = BasicBoardLayout<NoteEntry>;

/// The database id of the note one editor session writes. Every save of the
/// session goes through the same slot: the first save of a new note creates the
/// row and stores its id, later saves (even ones queued before that id reached
/// the GUI thread) update that row instead of creating another.
using IdSlot = std::shared_ptr<std::atomic<int>>;

inline IdSlot make_id_slot(int id) {
    return std::make_shared<std::atomic<int>>(id);
}

class NotesStore : public QObject {
    Q_OBJECT
  public:
    explicit NotesStore(QObject* parent = nullptr);
    ~NotesStore() override;

    struct LoadResult {
        bool ok = false;
        QString error;
        QVector<NoteEntry> entries; ///< every note (archived included) with its board row
        QVector<fincept::NoteTemplate> templates;
        QVector<fincept::notes_board::NoteBoardRecord> boards; ///< seeded with the default boards on first use
        bool board_tables = true; ///< false: the board tables are missing (older schema); nothing board-related is stored
    };
    /// Every note with its board row (rows written for notes first seen), the saved
    /// templates and the boards.
    void load(std::function<void(LoadResult)> done);

    struct SaveOutcome {
        int id = 0; ///< > 0 on success
        QString error;
        bool ok() const { return id > 0; }
    };
    /// Create the note (slot <= 0) or update row `*slot`. `meta_on_create` is
    /// written in the same job when this save creates the row (the lane, order and
    /// origin of a note made on the board). `done(id, ok)`.
    void save(const fincept::FinancialNote& note, const IdSlot& slot, std::function<void(int, bool)> done,
              std::optional<fincept::notes_board::NoteBoardMeta> meta_on_create = std::nullopt);
    SaveOutcome save_now(fincept::FinancialNote note, const IdSlot& slot,
                         std::optional<fincept::notes_board::NoteBoardMeta> meta_on_create = std::nullopt);

    /// Write each note as it is (pin, archive, tags; bulk edits; board moves).
    void update_many(const QVector<fincept::FinancialNote>& notes, std::function<void(bool)> done);
    /// Insert each note (duplicate, import), with its board row when `metas` has
    /// one at the same index. `done(ids)` — 0 for a failed row.
    void create_many(const QVector<fincept::FinancialNote>& notes, std::function<void(QVector<int>)> done,
                     const QVector<fincept::notes_board::NoteBoardMeta>& metas = {});
    void remove(const QVector<int>& ids, std::function<void(bool)> done);
    bool remove_now(const QVector<int>& ids);
    void save_template(const fincept::NoteTemplate& t, std::function<void(bool)> done);

    // ── Board rows (coalesced by the screen; one transaction per call) ─────
    void save_meta(const QVector<fincept::notes_board::NoteBoardMeta>& rows,
                   const QVector<fincept::notes_board::NoteBoardEvent>& events, std::function<void(bool)> done);
    bool save_meta_now(const QVector<fincept::notes_board::NoteBoardMeta>& rows,
                       const QVector<fincept::notes_board::NoteBoardEvent>& events);
    /// History of one note, newest first. `done(request_id, events)`.
    void load_history(int note_id, quint64 request_id,
                      std::function<void(quint64, QVector<fincept::notes_board::NoteBoardEvent>)> done);

    // ── Boards ──────────────────────────────────────────────────────────────
    void save_board(const fincept::notes_board::NoteBoardRecord& board, std::function<void(qint64)> done);
    void save_board_now(const fincept::notes_board::NoteBoardRecord& board);
    void remove_board(qint64 board_id, std::function<void(bool)> done);

    // ── Wall ────────────────────────────────────────────────────────────────
    struct WallData {
        qint64 board_id = 0;
        quint64 request_id = 0;
        QVector<fincept::notes_board::NoteWallItem> items;
        QVector<fincept::notes_board::NoteWallGroup> groups;
    };
    void load_wall(qint64 board_id, quint64 request_id, std::function<void(WallData)> done);
    void save_wall_items(qint64 board_id, const QVector<fincept::notes_board::NoteWallItem>& items,
                         std::function<void(bool)> done);
    bool save_wall_items_now(qint64 board_id, const QVector<fincept::notes_board::NoteWallItem>& items);
    void save_wall_group(const fincept::notes_board::NoteWallGroup& group, std::function<void(qint64)> done);
    void remove_wall_group(qint64 group_id, std::function<void(bool)> done);

    /// Any other work for the database thread (file import / export); `done(result)` on the GUI thread.
    template <class R>
    void run(std::function<R()> work, std::function<void(R)> done) {
        db_pool_.start([this, work = std::move(work), done = std::move(done)]() mutable {
            R result = work();
            post(&done, std::move(result));
        });
    }

    /// Filter, group and sort the board (and its statistics) on the board worker.
    /// `done(request_id, layout)`; a request still waiting to start is dropped
    /// when a newer one arrives.
    void build(const QVector<NoteEntry>& all, const BoardDef& def, const SearchQuery& query, quint64 request_id,
               std::function<void(quint64, BoardLayout)> done);

    /// Block until queued database work has finished (GUI thread; quit paths only).
    void wait_idle();

  private:
    template <class R>
    void post(std::function<void(R)>* done, R result) {
        // `this` outlives every job: the destructor waits for both pools first.
        QMetaObject::invokeMethod(
            this, [d = std::move(*done), r = std::move(result)]() mutable { if (d) d(std::move(r)); },
            Qt::QueuedConnection);
    }

    QThreadPool db_pool_;
    QThreadPool build_pool_;
    std::atomic<bool> board_tables_{true};
    bool pruned_ = false; ///< orphan sweep done this session (db thread only)
};

} // namespace fincept::screens::notes
