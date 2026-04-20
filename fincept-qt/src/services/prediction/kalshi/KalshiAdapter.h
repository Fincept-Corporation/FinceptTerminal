#pragma once

#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/kalshi/KalshiCredentials.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace fincept::services::prediction::kalshi_ns {

class KalshiRestClient;
class KalshiWsClient;

/// Kalshi exchange adapter.
///
/// Owns its own REST client + WS client (unlike Polymarket which wraps
/// existing global singletons). Kalshi has no global singletons today —
/// this class IS the singleton-of-concerns for the Kalshi integration.
///
/// Phase 4 implements public read endpoints. Phase 7 wires RSA-PSS signed
/// trading methods via a Python bridge (py `cryptography` package).
class KalshiAdapter : public fincept::services::prediction::PredictionExchangeAdapter {
    Q_OBJECT
  public:
    explicit KalshiAdapter(QObject* parent = nullptr);
    ~KalshiAdapter() override;

    // Identity
    QString id() const override;
    QString display_name() const override;
    fincept::services::prediction::ExchangeCapabilities capabilities() const override;

    // Read
    void list_markets(const QString& category, const QString& sort_by, int limit, int offset) override;
    void list_events(const QString& category, const QString& sort_by, int limit, int offset) override;
    void search(const QString& query, int limit) override;
    void list_tags() override;
    void fetch_market(const fincept::services::prediction::MarketKey& key) override;
    void fetch_event(const fincept::services::prediction::MarketKey& key) override;
    void fetch_order_book(const QString& asset_id) override;
    void fetch_price_history(const QString& asset_id, const QString& interval, int fidelity) override;
    void fetch_recent_trades(const fincept::services::prediction::MarketKey& key, int limit) override;

    // WebSocket
    void subscribe_market(const QStringList& asset_ids) override;
    void unsubscribe_market(const QStringList& asset_ids) override;
    bool is_ws_connected() const override;

    // Auth / trading (Phase 7 — stubs here)
    bool has_credentials() const override;
    QString account_label() const override;
    void fetch_balance() override;
    void fetch_positions() override;
    void fetch_open_orders() override;
    void fetch_user_activity(int limit) override;
    void place_order(const fincept::services::prediction::OrderRequest& req) override;
    void cancel_order(const QString& order_id) override;
    void cancel_all_for_market(const fincept::services::prediction::MarketKey& key,
                               const QString& asset_id) override;

    void ensure_registered_with_hub() override;

    // Credential injection (Phase 5 will wire this via the account dialog).
    void set_credentials(const KalshiCredentials& creds);

  private:
    void wire();
    void stub_unsupported(const QString& ctx);

    /// Run a command against prediction_kalshi.py with the stored creds
    /// merged into `extra`. Invokes `on_ok` with the parsed JSON on
    /// success; emits error_occurred() on failure.
    void run_py(const QString& command, const QJsonObject& extra,
                std::function<void(const QJsonObject&)> on_ok, const QString& ctx);
    QJsonObject creds_to_json() const;

    /// Kalshi asset IDs look like "TICKER:yes" / "TICKER:no". Split into
    /// (ticker, side). Returns ("", "") if malformed.
    static std::pair<QString, QString> split_asset_id(const QString& asset_id);

    std::unique_ptr<KalshiRestClient> rest_;
    std::unique_ptr<KalshiWsClient> ws_;
    KalshiCredentials creds_;
    bool hub_registered_ = false;

    // Tracks the last-requested price-history asset id so REST callbacks
    // can attach it to the emitted PriceHistory (Kalshi REST only returns
    // YES-side candles).
    QString last_history_asset_id_;
};

} // namespace fincept::services::prediction::kalshi_ns
