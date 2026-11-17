#pragma once
// WatchlistAlertService — evaluates watchlist alerts on live quotes.
//
// Alerts live in `watchlist_alerts` (WatchlistRepository, migration v054) and
// follow the symbol. The service subscribes `market:quote:<SYM>` for every
// symbol with an enabled alert — owned by the service, so evaluation goes on
// whichever screen is open — and checks each quote as it arrives:
//
//   price_above / price_below   last >= / <= threshold
//   change_up / change_down     day change % >= threshold / <= -threshold
//   volume_spike                volume / 3-month average volume >= threshold
//   high_52w / low_52w          today's high / low reaches the 52-week high / low
//   ma_cross_above / _below     last crosses the 50- or 200-day moving average
//
// Level alerts fire when their condition becomes true (also on the first quote
// after start, unless a repeating alert already fired today); a cross fires
// only on an observed change of side. A one-shot alert disables itself after
// firing; a repeating one re-arms once the condition clears (5-minute
// cool-down). Firing notifies through ToastService (desktop notification) and
// NotificationService (bell history + the user's enabled channels, subject to
// the "price alerts" filter).
//
// GUI thread only. start() is idempotent and cheap (the load is deferred to
// the event loop); the Watchlist screen calls it, and the app may call it at
// startup so alerts run before the screen is first opened.

#include "core/result/Result.h"
#include "services/markets/MarketDataTypes.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::services {

class WatchlistAlertService : public QObject {
    Q_OBJECT
  public:
    static WatchlistAlertService& instance();

    /// Load the alerts and start watching quotes. Idempotent.
    void start();
    bool started() const { return started_; }
    /// False when the database lacks the alerts table (migration not applied).
    bool available() const;

    QVector<WatchlistAlert> alerts() const { return alerts_; }
    QVector<WatchlistAlert> alerts_for(const QString& symbol) const;

    /// Insert (id == 0) or update; a saved alert is re-armed. Returns the id.
    Result<qint64> save(const WatchlistAlert& alert);
    Result<void> remove(qint64 id);
    Result<void> set_enabled(qint64 id, bool enabled);

    /// Kind keys in menu order, and their labels.
    static QStringList kinds();
    static QString kind_label(const QString& kind);
    /// Whether the kind takes a threshold, and what it means ("Price", "Percent", "x average").
    static bool kind_has_threshold(const QString& kind);
    static QString threshold_label(const QString& kind);
    static bool kind_is_ma_cross(const QString& kind);
    /// One-line description: "Price above 182.50", "Crosses above the 50-day average".
    static QString describe(const WatchlistAlert& alert);
    /// last_triggered_at falls on today's (local) date.
    static bool fired_today(const WatchlistAlert& alert);
    /// The alert's condition on `q`; nullopt when the quote lacks the data.
    static std::optional<bool> condition(const WatchlistAlert& alert, const QuoteData& q);

  signals:
    /// The alert set or a state changed (save, remove, fire).
    void alerts_changed();
    void alert_fired(qint64 id, const QString& symbol, const QString& message);

  private:
    WatchlistAlertService();
    Q_DISABLE_COPY(WatchlistAlertService)

    void reload();
    void resubscribe();
    void on_quote(const QString& symbol, const QuoteData& q);
    void fire(WatchlistAlert& alert, const QuoteData& q);

    QVector<WatchlistAlert> alerts_;
    QHash<qint64, int> last_state_; ///< -1 unknown, 0 false, 1 true
    QSet<QString> subscribed_;
    bool started_ = false;
};

} // namespace fincept::services
