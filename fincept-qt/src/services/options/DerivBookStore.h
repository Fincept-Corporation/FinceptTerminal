#pragma once
// DerivBookStore — the Derivatives position book and saved strategies on disk, for the
// GUI. Every statement runs on the store's own single worker thread (QtConcurrent on a
// one-thread pool, so writes never race and the GUI thread never waits on SQLite)
// through storage/repositories/DerivBookRepository on that thread's connection;
// callbacks run on the GUI thread and are dropped when `ctx` was destroyed meanwhile.
// Errors arrive as plain sentences (the driver text goes to the log only).
//
//   auto& store = fincept::services::options::DerivBookStore::instance();
//   store.load_positions({}, this, [this](const QVector<BookPosition>& rows, const QString& error) { ... });
//   store.save_position(p, this, [this](const BookPosition& saved, const QString& error) { ... });

#include "services/options/BookTypes.h"

#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QVector>

#include <functional>

namespace fincept::services::options {

class DerivBookStore : public QObject {
    Q_OBJECT
  public:
    static DerivBookStore& instance();

    using PositionsDone = std::function<void(const QVector<book::BookPosition>& rows, const QString& error)>;
    using PositionDone = std::function<void(const book::BookPosition& saved, const QString& error)>;
    using StrategiesDone = std::function<void(const QVector<book::SavedStrategy>& rows, const QString& error)>;
    using StrategyDone = std::function<void(const book::SavedStrategy& saved, const QString& error)>;
    using BooksDone = std::function<void(const QStringList& books, const QString& error)>;
    using Done = std::function<void(const QString& error)>;

    /// Positions of `book` (every book when empty), oldest first.
    void load_positions(const QString& book, QObject* ctx, PositionsDone done);
    void load_books(QObject* ctx, BooksDone done);
    void save_position(const book::BookPosition& p, QObject* ctx, PositionDone done);
    void remove_position(const QString& id, QObject* ctx, Done done);

    void load_strategies(QObject* ctx, StrategiesDone done);
    /// Saving under an existing name replaces that strategy.
    void save_strategy(const book::SavedStrategy& s, QObject* ctx, StrategyDone done);
    void remove_strategy(const QString& id, QObject* ctx, Done done);

  signals:
    /// After any successful position write.
    void positions_changed();
    /// After any successful strategy write.
    void strategies_changed();

  private:
    explicit DerivBookStore(QObject* parent = nullptr);
    Q_DISABLE_COPY(DerivBookStore)

    QThreadPool pool_; ///< one thread: statements run in order
};

} // namespace fincept::services::options
