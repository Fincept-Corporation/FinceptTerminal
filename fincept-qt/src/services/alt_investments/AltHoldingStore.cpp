#include "services/alt_investments/AltHoldingStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

namespace fincept::services::alt {

namespace {

template <typename T>
void alt_hold_async(QObject* ctx, std::function<T()> work, std::function<void(const T&)> done) {
    QObject* target = ctx ? ctx : qApp;
    auto* watcher = new QFutureWatcher<T>(target);
    QObject::connect(watcher, &QFutureWatcher<T>::finished, target, [watcher, done = std::move(done)]() {
        const T result = watcher->result();
        watcher->deleteLater();
        if (done)
            done(result);
    });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

struct AltHoldingsResult {
    QVector<AltHolding> rows;
    QString error;
};
struct AltFundsResult {
    QVector<AltFundPosition> rows;
    QString error;
};
struct AltHoldOpResult {
    bool ok = false;
    QString error;
};

} // namespace

void AltHoldingStore::list_holdings(const QString& portfolio_id, QObject* ctx, HoldingsDone done) {
    alt_hold_async<AltHoldingsResult>(
        ctx,
        [portfolio_id]() {
            AltHoldingsResult out;
            auto r = Database::instance().execute(
                QStringLiteral("SELECT id, name, asset_class, value, currency, liquidity, unfunded, portfolio_id "
                               "FROM alt_holdings WHERE portfolio_id = ? ORDER BY created_at, name"),
                {portfolio_id});
            if (r.is_err()) {
                out.error = QString::fromStdString(r.error());
                return out;
            }
            auto& q = r.value();
            while (q.next()) {
                AltHolding h;
                h.id = q.value(0).toString();
                h.name = q.value(1).toString();
                h.asset_class = q.value(2).toString();
                h.value = q.value(3).toDouble();
                h.currency = q.value(4).toString();
                h.liquidity = q.value(5).toString();
                h.unfunded = q.value(6).toDouble();
                h.portfolio_id = q.value(7).toString();
                out.rows << h;
            }
            return out;
        },
        [done](const AltHoldingsResult& r) {
            if (done)
                done(r.rows, r.error);
        });
}

void AltHoldingStore::replace_holdings(const QString& portfolio_id, const QVector<AltHolding>& holdings, QObject* ctx,
                                       Done done) {
    alt_hold_async<AltHoldOpResult>(
        ctx,
        [portfolio_id, holdings]() {
            AltHoldOpResult out;
            auto& db = Database::instance();
            if (auto b = db.begin_transaction(); b.is_err()) {
                out.error = QString::fromStdString(b.error());
                return out;
            }
            auto fail = [&db, &out](const std::string& e) {
                db.rollback();
                out.error = QString::fromStdString(e);
                return out;
            };
            auto del = db.execute(QStringLiteral("DELETE FROM alt_holdings WHERE portfolio_id = ?"), {portfolio_id});
            if (del.is_err())
                return fail(del.error());
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 order = 0;
            for (const AltHolding& h : holdings) {
                const QString id = h.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : h.id;
                auto ins = db.execute(
                    QStringLiteral("INSERT INTO alt_holdings (id, name, asset_class, value, currency, liquidity, unfunded, "
                                   "portfolio_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"),
                    {id, h.name, h.asset_class, h.value, h.currency, h.liquidity, h.unfunded, portfolio_id, now + order,
                     now});
                ++order; // keeps the grid's row order on reload
                if (ins.is_err())
                    return fail(ins.error());
            }
            if (auto c = db.commit(); c.is_err())
                return fail(c.error());
            out.ok = true;
            return out;
        },
        [done](const AltHoldOpResult& r) {
            if (!r.ok)
                LOG_WARN("AltInvestments", QStringLiteral("Holdings could not be saved reason=%1").arg(r.error.left(160)));
            if (done)
                done(r.ok, r.error);
        });
}

void AltHoldingStore::list_fund_positions(QObject* ctx, FundsDone done) {
    alt_hold_async<AltFundsResult>(
        ctx,
        []() {
            AltFundsResult out;
            auto& db = Database::instance();
            auto funds = db.execute(
                QStringLiteral("SELECT id, name, strategy, currency, commitment FROM alt_funds ORDER BY name"));
            if (funds.is_err()) {
                out.error = QString::fromStdString(funds.error());
                return out;
            }
            QHash<QString, int> at;
            auto& fq = funds.value();
            while (fq.next()) {
                AltFundPosition p;
                p.id = fq.value(0).toString();
                p.name = fq.value(1).toString();
                p.strategy = fq.value(2).toString();
                p.currency = fq.value(3).toString();
                p.commitment = fq.value(4).toDouble();
                at.insert(p.id, int(out.rows.size()));
                out.rows << p;
            }
            // Flows oldest first: the NAV is rolled forward from the latest statement.
            auto flows = db.execute(
                QStringLiteral("SELECT fund_id, flow_date, kind, amount FROM alt_fund_flows ORDER BY fund_id, flow_date, id"));
            if (flows.is_err()) {
                out.error = QString::fromStdString(flows.error());
                return out;
            }
            auto& q = flows.value();
            while (q.next()) {
                const int i = at.value(q.value(0).toString(), -1);
                if (i < 0)
                    continue;
                AltFundPosition& p = out.rows[i];
                const QString kind = q.value(2).toString();
                const double amount = q.value(3).toDouble();
                if (kind == QLatin1String("call")) {
                    p.called += amount;
                    if (!p.nav_date.isEmpty())
                        p.nav += amount;
                } else if (kind == QLatin1String("distribution")) {
                    p.distributed += amount;
                    if (!p.nav_date.isEmpty())
                        p.nav = std::max(0.0, p.nav - amount);
                } else if (kind == QLatin1String("nav")) {
                    p.nav = amount;
                    p.nav_date = q.value(1).toString();
                }
            }
            return out;
        },
        [done](const AltFundsResult& r) {
            if (done)
                done(r.rows, r.error);
        });
}

} // namespace fincept::services::alt
