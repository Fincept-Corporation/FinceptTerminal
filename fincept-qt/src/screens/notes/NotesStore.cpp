// src/screens/notes/NotesStore.cpp
#include "screens/notes/NotesStore.h"

#include "core/logging/Logger.h"
#include "storage/repositories/NoteBoardRepository.h"

#include <QDateTime>

namespace fincept::screens::notes {

namespace nb = fincept::notes_board;

namespace {

/// Create or update, resolving the id through the session's slot at run time.
/// A note this call creates gets `meta_on_create` as its board row.
NotesStore::SaveOutcome nst_write_note(fincept::FinancialNote note, const IdSlot& slot,
                                       const std::optional<nb::NoteBoardMeta>& meta_on_create) {
    NotesStore::SaveOutcome out;
    auto& repo = fincept::NotesRepository::instance();
    const int id = slot ? slot->load() : note.id;
    if (id > 0) {
        note.id = id;
        auto r = repo.update(note);
        if (r.is_err()) {
            out.error = QString::fromStdString(r.error());
            LOG_ERROR("Notes", QString("Could not update note note_id=%1 error=\"%2\"").arg(id).arg(out.error));
            return out;
        }
        out.id = id;
        return out;
    }
    auto r = repo.create(note);
    if (r.is_err()) {
        out.error = QString::fromStdString(r.error());
        LOG_ERROR("Notes", QString("Could not create note title=\"%1\" error=\"%2\"").arg(note.title.left(60), out.error));
        return out;
    }
    out.id = static_cast<int>(r.value());
    if (slot)
        slot->store(out.id);
    if (meta_on_create) {
        nb::NoteBoardMeta m = *meta_on_create;
        m.note_id = out.id;
        auto mr = fincept::NoteBoardRepository::instance().upsert_meta({m});
        if (mr.is_err())
            LOG_WARN("Notes", QString("Could not store the board lane of a new note note_id=%1 error=\"%2\"")
                                  .arg(out.id)
                                  .arg(QString::fromStdString(mr.error())));
    }
    return out;
}

} // namespace

NotesStore::NotesStore(QObject* parent) : QObject(parent) {
    // One thread each: database writes stay in the order they were queued, and a
    // board build never waits behind a write. Idle threads exit after 30 s (and
    // take their cloned database connection with them).
    db_pool_.setMaxThreadCount(1);
    build_pool_.setMaxThreadCount(1);
}

NotesStore::~NotesStore() {
    db_pool_.waitForDone();
    build_pool_.clear();
    build_pool_.waitForDone();
}

void NotesStore::wait_idle() {
    db_pool_.waitForDone();
}

void NotesStore::load(std::function<void(LoadResult)> done) {
    run<LoadResult>(
        [this]() {
            LoadResult out;
            auto& repo = fincept::NotesRepository::instance();
            auto r = repo.list_all(/*include_archived=*/true);
            if (r.is_err()) {
                out.error = QString::fromStdString(r.error());
                return out;
            }
            const QVector<fincept::FinancialNote>& notes = r.value();
            out.entries.reserve(notes.size());
            for (const auto& n : notes)
                out.entries.append(make_entry(n));
            auto t = repo.list_templates();
            if (t.is_ok())
                out.templates = t.value();

            // Board rows: stored ones applied; notes seen for the first time get
            // their defaults written, so their place on the board stays put.
            auto& board = fincept::NoteBoardRepository::instance();
            auto meta = board.load_meta();
            if (meta.is_err()) {
                out.board_tables = false;
                board_tables_ = false;
                LOG_WARN("Notes", QString("Board data unavailable, showing defaults error=\"%1\"")
                                      .arg(QString::fromStdString(meta.error())));
            } else {
                board_tables_ = true;
                QHash<int, nb::NoteBoardMeta> by_id;
                by_id.reserve(meta.value().size());
                for (const auto& m : meta.value())
                    by_id.insert(m.note_id, m);
                QVector<nb::NoteBoardMeta> missing;
                for (auto& e : out.entries) {
                    auto it = by_id.constFind(e.note.id);
                    if (it != by_id.constEnd()) {
                        apply_meta(e, it.value());
                        // Chat Mode's tag marks a note as linked even when its row predates it.
                        if (e.linked && e.meta.origin.isEmpty())
                            e.meta.origin = nb::kOriginAgentRuns;
                    } else {
                        nb::NoteBoardMeta m = e.meta;
                        m.stored = true;
                        missing.append(m);
                        e.meta.stored = true;
                    }
                }
                if (!missing.isEmpty()) {
                    auto w = board.insert_missing_meta(missing);
                    if (w.is_err())
                        LOG_WARN("Notes", QString("Could not store default board rows count=%1 error=\"%2\"")
                                              .arg(missing.size())
                                              .arg(QString::fromStdString(w.error())));
                }
                if (!pruned_) {
                    pruned_ = true;
                    auto p = board.prune_orphans();
                    if (p.is_err())
                        LOG_WARN("Notes", QString("Could not sweep board rows of deleted notes error=\"%1\"")
                                              .arg(QString::fromStdString(p.error())));
                }
                auto boards = board.list_boards();
                if (boards.is_ok() && boards.value().isEmpty()) {
                    for (const BoardDef& d : default_boards())
                        board.save_board(d.to_record());
                    boards = board.list_boards();
                }
                if (boards.is_ok())
                    out.boards = boards.value();
            }
            out.ok = true;
            return out;
        },
        std::move(done));
}

void NotesStore::save(const fincept::FinancialNote& note, const IdSlot& slot, std::function<void(int, bool)> done,
                      std::optional<nb::NoteBoardMeta> meta_on_create) {
    if (!board_tables_)
        meta_on_create.reset();
    run<SaveOutcome>([note, slot, meta_on_create]() { return nst_write_note(note, slot, meta_on_create); },
                     [done = std::move(done)](SaveOutcome o) {
                         if (done)
                             done(o.id, o.ok());
                     });
}

NotesStore::SaveOutcome NotesStore::save_now(fincept::FinancialNote note, const IdSlot& slot,
                                             std::optional<nb::NoteBoardMeta> meta_on_create) {
    wait_idle();
    if (!board_tables_)
        meta_on_create.reset();
    return nst_write_note(std::move(note), slot, meta_on_create);
}

void NotesStore::update_many(const QVector<fincept::FinancialNote>& notes, std::function<void(bool)> done) {
    run<bool>(
        [notes]() {
            bool ok = true;
            auto& repo = fincept::NotesRepository::instance();
            for (const auto& n : notes)
                if (n.id > 0 && repo.update(n).is_err())
                    ok = false;
            return ok;
        },
        std::move(done));
}

void NotesStore::create_many(const QVector<fincept::FinancialNote>& notes, std::function<void(QVector<int>)> done,
                             const QVector<nb::NoteBoardMeta>& metas) {
    const bool with_meta = board_tables_ && !metas.isEmpty();
    run<QVector<int>>(
        [notes, metas, with_meta]() {
            QVector<int> ids;
            ids.reserve(notes.size());
            for (int i = 0; i < notes.size(); ++i) {
                fincept::FinancialNote copy = notes.at(i);
                copy.id = 0;
                std::optional<nb::NoteBoardMeta> m;
                if (with_meta && i < metas.size())
                    m = metas.at(i);
                ids.append(std::max(0, nst_write_note(copy, nullptr, m).id));
            }
            return ids;
        },
        std::move(done));
}

void NotesStore::remove(const QVector<int>& ids, std::function<void(bool)> done) {
    run<bool>(
        [ids]() {
            bool ok = true;
            for (int id : ids)
                if (fincept::NotesRepository::instance().remove(id).is_err())
                    ok = false;
            return ok;
        },
        std::move(done));
}

bool NotesStore::remove_now(const QVector<int>& ids) {
    wait_idle();
    bool ok = true;
    for (int id : ids)
        if (fincept::NotesRepository::instance().remove(id).is_err())
            ok = false;
    return ok;
}

void NotesStore::save_template(const fincept::NoteTemplate& t, std::function<void(bool)> done) {
    run<bool>([t]() { return fincept::NotesRepository::instance().create_template(t).is_ok(); }, std::move(done));
}

// ── Board rows ───────────────────────────────────────────────────────────────

namespace {

bool nst_write_meta(const QVector<nb::NoteBoardMeta>& rows, const QVector<nb::NoteBoardEvent>& events) {
    auto& board = fincept::NoteBoardRepository::instance();
    bool ok = true;
    if (!rows.isEmpty()) {
        auto r = board.upsert_meta(rows);
        if (r.is_err()) {
            ok = false;
            LOG_ERROR("Notes", QString("Could not store board changes rows=%1 error=\"%2\"")
                                   .arg(rows.size())
                                   .arg(QString::fromStdString(r.error())));
        }
    }
    if (!events.isEmpty()) {
        auto r = board.add_events(events);
        if (r.is_err())
            LOG_WARN("Notes", QString("Could not store note history events=%1 error=\"%2\"")
                                  .arg(events.size())
                                  .arg(QString::fromStdString(r.error())));
    }
    return ok;
}

} // namespace

void NotesStore::save_meta(const QVector<nb::NoteBoardMeta>& rows, const QVector<nb::NoteBoardEvent>& events,
                           std::function<void(bool)> done) {
    if (!board_tables_) {
        if (done)
            QMetaObject::invokeMethod(this, [done]() { done(false); }, Qt::QueuedConnection);
        return;
    }
    run<bool>([rows, events]() { return nst_write_meta(rows, events); }, std::move(done));
}

bool NotesStore::save_meta_now(const QVector<nb::NoteBoardMeta>& rows, const QVector<nb::NoteBoardEvent>& events) {
    wait_idle();
    if (!board_tables_)
        return false;
    return nst_write_meta(rows, events);
}

void NotesStore::load_history(int note_id, quint64 request_id,
                              std::function<void(quint64, QVector<nb::NoteBoardEvent>)> done) {
    using Out = QPair<quint64, QVector<nb::NoteBoardEvent>>;
    const bool tables = board_tables_;
    run<Out>(
        [note_id, request_id, tables]() {
            Out out{request_id, {}};
            if (!tables)
                return out;
            auto r = fincept::NoteBoardRepository::instance().events_for(note_id);
            if (r.is_ok())
                out.second = r.value();
            return out;
        },
        [done = std::move(done)](Out o) {
            if (done)
                done(o.first, std::move(o.second));
        });
}

// ── Boards ───────────────────────────────────────────────────────────────────

void NotesStore::save_board(const nb::NoteBoardRecord& board, std::function<void(qint64)> done) {
    if (!board_tables_) {
        if (done)
            QMetaObject::invokeMethod(this, [done]() { done(0); }, Qt::QueuedConnection);
        return;
    }
    run<qint64>(
        [board]() {
            auto r = fincept::NoteBoardRepository::instance().save_board(board);
            if (r.is_err()) {
                LOG_ERROR("Notes", QString("Could not save board name=\"%1\" error=\"%2\"")
                                       .arg(board.name, QString::fromStdString(r.error())));
                return qint64(0);
            }
            return r.value();
        },
        std::move(done));
}

void NotesStore::save_board_now(const nb::NoteBoardRecord& board) {
    wait_idle();
    if (!board_tables_ || board.id <= 0)
        return;
    auto r = fincept::NoteBoardRepository::instance().save_board(board);
    if (r.is_err())
        LOG_ERROR("Notes", QString("Could not save board name=\"%1\" error=\"%2\"")
                               .arg(board.name, QString::fromStdString(r.error())));
}

void NotesStore::remove_board(qint64 board_id, std::function<void(bool)> done) {
    run<bool>([board_id]() { return fincept::NoteBoardRepository::instance().remove_board(board_id).is_ok(); },
              std::move(done));
}

// ── Wall ─────────────────────────────────────────────────────────────────────

void NotesStore::load_wall(qint64 board_id, quint64 request_id, std::function<void(WallData)> done) {
    const bool tables = board_tables_;
    run<WallData>(
        [board_id, request_id, tables]() {
            WallData w;
            w.board_id = board_id;
            w.request_id = request_id;
            if (!tables || board_id <= 0)
                return w;
            auto& repo = fincept::NoteBoardRepository::instance();
            auto items = repo.wall_items(board_id);
            if (items.is_ok())
                w.items = items.value();
            auto groups = repo.wall_groups(board_id);
            if (groups.is_ok())
                w.groups = groups.value();
            return w;
        },
        std::move(done));
}

void NotesStore::save_wall_items(qint64 board_id, const QVector<nb::NoteWallItem>& items,
                                 std::function<void(bool)> done) {
    if (!board_tables_ || board_id <= 0) {
        if (done)
            QMetaObject::invokeMethod(this, [done]() { done(false); }, Qt::QueuedConnection);
        return;
    }
    run<bool>(
        [board_id, items]() {
            auto r = fincept::NoteBoardRepository::instance().upsert_wall_items(board_id, items);
            if (r.is_err())
                LOG_ERROR("Notes", QString("Could not store wall positions board_id=%1 items=%2 error=\"%3\"")
                                       .arg(board_id)
                                       .arg(items.size())
                                       .arg(QString::fromStdString(r.error())));
            return r.is_ok();
        },
        std::move(done));
}

bool NotesStore::save_wall_items_now(qint64 board_id, const QVector<nb::NoteWallItem>& items) {
    wait_idle();
    if (!board_tables_ || board_id <= 0)
        return false;
    return fincept::NoteBoardRepository::instance().upsert_wall_items(board_id, items).is_ok();
}

void NotesStore::save_wall_group(const nb::NoteWallGroup& group, std::function<void(qint64)> done) {
    if (!board_tables_ || group.board_id <= 0) {
        if (done)
            QMetaObject::invokeMethod(this, [done]() { done(0); }, Qt::QueuedConnection);
        return;
    }
    run<qint64>(
        [group]() {
            auto r = fincept::NoteBoardRepository::instance().save_wall_group(group);
            return r.is_ok() ? r.value() : qint64(0);
        },
        std::move(done));
}

void NotesStore::remove_wall_group(qint64 group_id, std::function<void(bool)> done) {
    run<bool>([group_id]() { return fincept::NoteBoardRepository::instance().remove_wall_group(group_id).is_ok(); },
              std::move(done));
}

// ── Board build ──────────────────────────────────────────────────────────────

void NotesStore::build(const QVector<NoteEntry>& all, const BoardDef& def, const SearchQuery& query,
                       quint64 request_id, std::function<void(quint64, BoardLayout)> done) {
    // Only the newest request matters: drop one still waiting to start.
    build_pool_.clear();
    build_pool_.start([this, all, def, query, request_id, done = std::move(done)]() mutable {
        BoardLayout layout = build_board(all, def, query, QDateTime::currentDateTime());
        QMetaObject::invokeMethod(
            this,
            [d = std::move(done), request_id, l = std::move(layout)]() mutable {
                if (d)
                    d(request_id, std::move(l));
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::screens::notes
