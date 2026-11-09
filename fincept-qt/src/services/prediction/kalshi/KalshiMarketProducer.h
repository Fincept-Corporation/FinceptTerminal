#pragma once

#include "datahub/Producer.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::services::prediction::kalshi_ns {

class KalshiRestClient;

/// Keyless Kalshi market snapshots on the hub.
///
///   prediction:kalshi:market:<ticker>   payload: prediction::PredictionMarket
///
/// REST-backed (public GET /markets, no credentials). Every refresh() turns the
/// batch of due topics into ONE `GET /markets?tickers=A,B,…` (chunked at 100
/// tickers) through its own KalshiRestClient's callback-style
/// fetch_markets_by_tickers(), so none of this traffic reaches the
/// KalshiAdapter signals the Prediction Markets screen listens to.
///
/// Payload extras on top of the REST client's parse_market(): last_price_dollars,
/// yes_ask_dollars, previous_price_dollars / previous_yes_bid_dollars /
/// previous_yes_ask_dollars (24 h ago), status, result.
///
/// Errors (publish_error, "[CODE] message"):
///   [NOT_FOUND]      Kalshi returned no market for that ticker (retrying cannot help)
///   [INVALID]        the ticker cannot be sent (empty / contains ',')
///   [REQUEST_FAILED] network / HTTP / parse failure for the whole batch
///
/// Standalone singleton (not owned by KalshiAdapter) so a consumer can register
/// it on demand before main.cpp's deferred adapter setup has run;
/// KalshiAdapter::ensure_registered_with_hub() registers it at startup too.
class KalshiMarketProducer : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static KalshiMarketProducer& instance();

    /// Register with the hub + install the `prediction:kalshi:market:*` policy
    /// (TTL 30 s, min interval 10 s, pull). Idempotent; GUI thread.
    void ensure_registered_with_hub();

    /// `prediction:kalshi:market:<ticker>`.
    static QString topic_for(const QString& ticker);

    // ── fincept::datahub::Producer ────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 2; }

  private:
    explicit KalshiMarketProducer(QObject* parent = nullptr);
    Q_DISABLE_COPY(KalshiMarketProducer)

    KalshiRestClient* rest_ = nullptr; // child; production endpoint (public data needs no demo switch)
    bool hub_registered_ = false;
};

} // namespace fincept::services::prediction::kalshi_ns
