#pragma once

#include <QString>
#include <QStringList>

namespace fincept::datahub {

/// A Producer owns refresh for a set of topic patterns. Services
/// (MarketDataService, NewsService, ExchangeService, broker adapters, …)
/// implement this interface to plug into DataHub.
class Producer {
  public:
    virtual ~Producer() = default;

    /// Patterns this producer owns. Example: `{"market:quote:*",
    /// "market:history:*", "market:sparkline:*"}`. Patterns match
    /// topics via simple `*`-suffix globbing.
    virtual QStringList topic_patterns() const = 0;

    /// Hub calls this when ≥1 subscriber exists and the cached value is
    /// stale. Producer fetches asynchronously and calls
    /// `DataHub::publish()` when results are ready, or
    /// `DataHub::publish_error()` if the fetch failed.
    virtual void refresh(const QStringList& topics) = 0;

    /// Optional cap on outbound requests per second for this producer.
    /// 0 = unlimited. The hub scheduler paces `refresh()` calls to
    /// respect this limit. Typical values: Zerodha REST 3, Angel One
    /// REST 1, Polymarket 10.
    virtual int max_requests_per_sec() const { return 0; }
};

} // namespace fincept::datahub
