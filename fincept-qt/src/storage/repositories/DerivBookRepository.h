#pragma once
// DerivBookRepository — the Derivatives position book and saved strategies in SQLite
// (tables deriv_positions / deriv_strategies of migration v065).
//
// Synchronous, on the connection it is given: use it ONLY OFF THE GUI THREAD, with a
// connection valid for the calling thread (services/options/DerivBookStore runs it on
// its own worker with Database::instance().connection(); the unit test passes an
// in-memory database). Every failure is returned as Result::err with the driver text;
// callers turn it into a plain sentence.

#include "core/result/Result.h"
#include "services/options/BookTypes.h"

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept {

class DerivBookRepository {
  public:
    explicit DerivBookRepository(QSqlDatabase db) : db_(std::move(db)) {}

    /// Positions of `book` (every book when empty), oldest first.
    Result<QVector<services::options::book::BookPosition>> positions(const QString& book = {}) const;
    /// Distinct book names, alphabetical.
    Result<QStringList> books() const;
    /// Insert (empty id: a new id is assigned) or update. Returns the stored row (id, timestamps).
    Result<services::options::book::BookPosition> save_position(const services::options::book::BookPosition& p) const;
    Result<void> remove_position(const QString& id) const;
    /// Number of positions (all books).
    Result<int> count_positions() const;

    /// Saved strategies by name.
    Result<QVector<services::options::book::SavedStrategy>> strategies() const;
    /// Save under `s.name` (trimmed, case-insensitive): an existing strategy of that name is
    /// replaced, keeping its id and creation time.
    Result<services::options::book::SavedStrategy> save_strategy(const services::options::book::SavedStrategy& s) const;
    Result<void> remove_strategy(const QString& id) const;

  private:
    QSqlDatabase db_;
};

} // namespace fincept
