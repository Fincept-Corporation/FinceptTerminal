#pragma once
// AltFundStore — the Alternative Investments fund book on disk: funds (alt_funds) and their dated
// cash flows (alt_fund_flows), tables of the shared M&A / Alt migration.
//
//   auto& store = fincept::services::alt::AltFundStore::instance();
//   store.list_funds(this, [this](const QVector<AltFund>& funds, const QString& error) { ... });
//   store.save_flows(fund_id, grid->to_json(), this, [](const QString& error) { ... });
//
// Every statement runs on the store's own single worker thread (QtConcurrent on a one-thread pool, so
// writes never race each other and the GUI thread never waits on SQLite); callbacks run on the GUI
// thread and are dropped when `ctx` was destroyed meanwhile. The flows of a fund are deleted
// explicitly with it (no reliance on a foreign-key cascade).
//
// Fund cash flows are private data: they live only in this local database (never in settings,
// screen state, input memory or cloud sync) and are never written to the log.

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QVector>

#include <functional>

namespace fincept::services::alt {

struct AltFund {
    QString id;
    QString name;
    QString strategy; ///< buyout | venture | growth | credit | real_estate | infrastructure | secondaries | fund_of_funds | other
    int vintage_year = 0;
    double commitment = 0.0;
    QString currency = QStringLiteral("USD");
    QString benchmark; ///< PME index (Yahoo symbol); empty = not chosen
    QString notes;
    qint64 created_at = 0;
    qint64 updated_at = 0;

    bool is_valid() const { return !id.isEmpty(); }
};

class AltFundStore : public QObject {
    Q_OBJECT
  public:
    static AltFundStore& instance();

    using FundsDone = std::function<void(const QVector<AltFund>& funds, const QString& error)>;
    using FlowsDone = std::function<void(const QString& fund_id, const QJsonArray& rows, const QString& error)>;
    using FundDone = std::function<void(const AltFund& fund, const QString& error)>;
    using BookDone = std::function<void(const QVector<AltFund>& funds, const QHash<QString, QJsonArray>& flows,
                                        const QString& error)>;
    using Done = std::function<void(const QString& error)>;

    /// Every fund, newest first.
    void list_funds(QObject* ctx, FundsDone done);
    /// One fund's flows [{date, kind, amount, note}] by date.
    void load_flows(const QString& fund_id, QObject* ctx, FlowsDone done);
    /// Every fund with its flows (the Book view).
    void load_book(QObject* ctx, BookDone done);
    /// Insert (empty id: a new id is assigned) or update a fund.
    void save_fund(const AltFund& fund, QObject* ctx, FundDone done);
    /// Replace a fund's flows with `rows` ({date, kind, amount, note}; invalid rows are skipped).
    void save_flows(const QString& fund_id, const QJsonArray& rows, QObject* ctx, Done done);
    /// Delete a fund and its flows.
    void delete_fund(const QString& fund_id, QObject* ctx, Done done);

  signals:
    /// After any successful write (the Book view refreshes).
    void funds_changed();

  private:
    explicit AltFundStore(QObject* parent = nullptr);
    Q_DISABLE_COPY(AltFundStore)

    QThreadPool pool_; ///< one thread: statements run in order
};

} // namespace fincept::services::alt
