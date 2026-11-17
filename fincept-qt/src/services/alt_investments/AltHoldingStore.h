#pragma once
// AltHoldingStore — what the ALLOCATION & LIQUIDITY view reads and keeps (migration v064):
//   * alternative holdings entered by hand (alt_holdings), replaced as a whole from the editing grid;
//   * a read-only position summary of each private-markets fund (alt_funds + alt_fund_flows).
// Every query runs on a worker thread (per-thread Database connection); callbacks arrive on the GUI
// thread and are dropped when their context object is gone. Local only (not in cloud sync).

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services::alt {

struct AltHolding {
    QString id;
    QString name;
    QString asset_class = QStringLiteral("other");
    double value = 0;
    QString currency = QStringLiteral("USD");
    QString liquidity = QStringLiteral("locked"); ///< daily | monthly | quarterly | annual | locked
    double unfunded = 0;
    QString portfolio_id;
};

struct AltFundPosition {
    QString id;
    QString name;
    QString strategy;
    QString currency;
    double commitment = 0;
    double called = 0;
    double distributed = 0;
    /// Latest NAV statement rolled forward with the calls and distributions dated after it
    /// (NAV + calls since - distributions since); 0 when the fund has no NAV statement.
    double nav = 0;
    QString nav_date; ///< date of the latest NAV statement ("" = none)
    double unfunded() const { return commitment > called ? commitment - called : 0.0; }
};

class AltHoldingStore {
  public:
    using HoldingsDone = std::function<void(const QVector<AltHolding>& holdings, const QString& error)>;
    using FundsDone = std::function<void(const QVector<AltFundPosition>& funds, const QString& error)>;
    using Done = std::function<void(bool ok, const QString& error)>;

    /// Holdings of `portfolio_id` ("" = those not tied to a portfolio).
    static void list_holdings(const QString& portfolio_id, QObject* ctx, HoldingsDone done);
    /// Replace the holdings of `portfolio_id` with `holdings` (one transaction).
    static void replace_holdings(const QString& portfolio_id, const QVector<AltHolding>& holdings, QObject* ctx,
                                 Done done = {});
    static void list_fund_positions(QObject* ctx, FundsDone done);
};

} // namespace fincept::services::alt
