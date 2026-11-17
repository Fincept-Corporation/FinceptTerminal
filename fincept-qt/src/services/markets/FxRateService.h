#pragma once
// FxRateService — spot and daily historical FX rates for converting
// valuations between currencies (never prices: a price keeps its own currency).
//
// Source: Yahoo quotes through the DataHub market-data topics owned by
// MarketDataService ("market:quote:EURUSD=X", "market:history:USDJPY=X:1y:1d"),
// so legs are refreshed by the hub's batched quote fetch like any other symbol
// (no fetch of its own). Every currency is held against USD (EURUSD=X,
// GBPUSD=X, AUDUSD=X, NZDUSD=X; USDxxx=X for the rest; "BTC-USD" / "USDT-USD"
// for crypto and stablecoins); a cross is taken from a direct Yahoo pair
// already in the hub when there is one, else triangulated through USD. The
// leg / combination maths is in FxRateMath.h.
//
// Never made up: an unknown currency or a pair Yahoo has not answered yet
// gives std::nullopt (convert_for_display(): amount NaN) — never 1.0. The one
// exception is a stablecoin with no rate yet, taken 1:1 to USD and flagged
// approximate. A rate older than stale_after() (the user's "FX stale after"
// setting, default 30 minutes) is still returned, flagged stale; older than 7
// days it is dropped. "as of" is the rate's own market time.
//
// rate()/quote()/convert()/as_of()/convert_for_display() are thread-safe and
// never block. ensure() and historical_rates() may be called from any thread;
// they run on the service's (GUI) thread.

#include "services/markets/FxRateMath.h"
#include "ui/formatting/Money.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace fincept::services {

class FxRateService : public QObject {
    Q_OBJECT
  public:
    using CurrencyPair = QPair<QString, QString>; ///< (from, to)
    /// `rates`: calendar date -> units of `to` per one `from`, forward-filled over
    /// weekends / holidays (at most 5 days past a real observation). Dates before
    /// the first observation are absent, never back-filled.
    using HistoryCallback = std::function<void(bool ok, const QMap<QDate, double>& rates, const QString& error)>;

    static FxRateService& instance();

    /// Spot rate with its time and state. nullopt: unknown code, no data yet,
    /// or older than 7 days. A rate older than stale_after() is returned with
    /// stale = true. A missing leg is subscribed in the background;
    /// rates_changed() follows when it lands.
    std::optional<FxQuote> quote(const QString& from_ccy, const QString& to_ccy) const;
    /// quote()->rate (stale rates included).
    std::optional<double> rate(const QString& from_ccy, const QString& to_ccy) const;
    /// `amount` in `from_ccy` expressed in `to_ccy`; nullopt when the rate is unknown.
    std::optional<double> convert(double amount, const QString& from_ccy, const QString& to_ccy) const;
    /// quote()->as_of, or an invalid QDateTime when there is no usable rate.
    QDateTime as_of(const QString& from_ccy, const QString& to_ccy) const;

    /// One call for a display conversion: amount + rate + the rate's time +
    /// stale / triangulated / approximate, consistent with each other. `to`
    /// empty = the user's display currency. Same currency -> amount scaled for
    /// minor units, converted = false. No rate -> amount NaN (ok() false) and
    /// the missing legs are requested.
    ui::formatting::ConvertedMoney convert_for_display(double amount, const QString& from_ccy,
                                                       const QString& to_ccy = {}) const;

    /// Subscribe the USD legs these pairs need (kept refreshed by the hub from
    /// then on). rates_changed() fires as they arrive.
    void ensure(const QList<CurrencyPair>& pairs);
    /// ensure() for every (currency, to_ccy) pair.
    void ensure_currencies(const QStringList& currencies, const QString& to_ccy);

    /// Daily close series for [start, end] (end clamped to today, UTC). The
    /// callback runs once, on the service's thread, never synchronously.
    void historical_rates(const QString& from_ccy, const QString& to_ccy, QDate start, QDate end,
                          HistoryCallback cb);

    /// Yahoo symbol of a currency's USD leg ("EURUSD=X", "USDJPY=X", "BTC-USD");
    /// empty for USD / invalid codes.
    static QString usd_leg_symbol(const QString& ccy);
    /// A 3-letter fiat code or a known crypto / stablecoin code (minor units accepted).
    static bool is_convertible(const QString& ccy);

    /// Age after which a rate is flagged stale. Follows the user's setting
    /// (FormatSettings fx_stale_minutes) automatically; setting it here
    /// overrides until the setting next changes.
    void set_stale_after(std::chrono::seconds age);
    std::chrono::seconds stale_after() const;
    /// Old names of set_stale_after() / stale_after(). A rate older than this is
    /// no longer dropped: it is returned flagged stale (dropped after 7 days).
    void set_max_age(std::chrono::seconds age);
    std::chrono::seconds max_age() const;

  signals:
    /// One or more spot legs were updated (coalesced).
    void rates_changed();

  private:
    FxRateService();
    Q_DISABLE_COPY(FxRateService)

    struct HistoryJob;
    void subscribe_leg(const QString& ccy);
    void request_missing_legs(const QStringList& ccys) const;
    void on_history_leg(const std::shared_ptr<HistoryJob>& job, const QString& ccy, const QVariant& value);
    void on_history_error(const std::shared_ptr<HistoryJob>& job, const QString& ccy, const QString& topic,
                          const QString& error);
    void finish_history(const std::shared_ptr<HistoryJob>& job, bool ok, const QString& error);

    mutable QMutex subs_mutex_;
    QSet<QString> subscribed_; ///< currencies whose USD leg is subscribed (guarded by subs_mutex_)
    std::atomic<qint64> stale_after_sec_{30 * 60};
    QTimer* changed_timer_ = nullptr; ///< coalesces rates_changed()

    /// Per-pair quotes, cleared whenever a subscribed leg publishes; `stale` is
    /// recomputed on read (guarded by cache_mutex_). Keeps data() calls of big
    /// tables from repeating 2-4 hub lookups per cell.
    mutable QMutex cache_mutex_;
    mutable QHash<QString, FxQuote> cache_;
};

/// Short names for call sites: `fx::convert_for_display(v, "GBp")`.
namespace fx {
inline ui::formatting::ConvertedMoney convert_for_display(double amount, const QString& from_ccy,
                                                          const QString& to_ccy = {}) {
    return FxRateService::instance().convert_for_display(amount, from_ccy, to_ccy);
}
inline std::optional<double> convert(double amount, const QString& from_ccy, const QString& to_ccy) {
    return FxRateService::instance().convert(amount, from_ccy, to_ccy);
}
} // namespace fx

} // namespace fincept::services
