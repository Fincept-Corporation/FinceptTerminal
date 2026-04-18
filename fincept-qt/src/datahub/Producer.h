#pragma once

#include <QString>
#include <QStringList>

// Phase 0 stub — see fincept-qt/DATAHUB_ARCHITECTURE.md §3.3.
// Full wiring into DataHub happens in Phase 1.

namespace fincept::datahub {

/// A Producer owns refresh for a set of topic patterns. Existing services
/// (MarketDataService, NewsService, ExchangeService, broker adapters, …)
/// will implement this interface during Phases 2–9. The interface is
/// declared now so Phase 1 can compile against it without churn.
class Producer {
  public:
    virtual ~Producer() = default;

    /// Patterns this producer owns. Example: `{"market:quote:*",
    /// "market:history:*", "market:sparkline:*"}`. Patterns match
    /// topics via simple `*`-suffix globbing.
    virtual QStringList topic_patterns() const = 0;

    /// Hub calls this when ≥1 subscriber exists and the cached value is
    /// stale. Producer fetches asynchronously and calls
    /// `DataHub::publish()` when results are ready.
    virtual void refresh(const QStringList& topics) = 0;

    /// Optional cap on outbound requests per second for this producer.
    /// 0 = unlimited. The hub scheduler paces `refresh()` calls to
    /// respect this limit. Typical values: Zerodha REST 3, Angel One
    /// REST 1, Polymarket 10.
    virtual int max_requests_per_sec() const { return 0; }

    /// Optional: called when the last subscriber leaves a topic owned
    /// by this producer. Producers may release per-topic resources
    /// here (close a WS channel, cancel a stream, etc.).
    virtual void on_topic_idle(const QString& /*topic*/) {}
};

} // namespace fincept::datahub
