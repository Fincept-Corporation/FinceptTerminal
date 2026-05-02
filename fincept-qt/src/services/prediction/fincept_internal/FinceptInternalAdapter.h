#pragma once

#include "services/prediction/PredictionExchangeAdapter.h"

#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace fincept::services::prediction::fincept_internal {

/// Internal Fincept prediction-market exchange adapter (Phase 4).
///
/// Joins `PolymarketAdapter` and `KalshiAdapter` as a third member of
/// `PredictionExchangeRegistry`. The exchange is operated by Fincept and
/// settles in $FNCPT — see `plans/crypto-center-future-phases.md` §4.
///
/// **Endpoint resolution:** the adapter reads `fincept.markets_endpoint`
/// from `SecureStorage`. Examples: `https://markets.fincept.in/api/v1`,
/// `wss://markets.fincept.in/ws`. When unset, the adapter operates in
/// **demo mode** — every read method emits a curated mock dataset (the
/// three example markets from plan §4.2: "Fed cuts in May 2026",
/// "NYC max 80F+ on May 1", "$FNCPT > $0.0001 by month end") and every
/// authenticated method emits an `error_occurred("not deployed", …)`
/// signal so screens can surface a "service unavailable" banner.
///
/// **On-chain dependency.** Real settlement requires the `fincept_market`
/// Anchor program (`solana/programs/fincept_market/`) which lives in a
/// separate repo and isn't deployed yet. Per CLAUDE.md, no Rust source
/// in this tree. The adapter is forward-compatible: when the program ID
/// is configured (`fincept.market_program_id`), `place_order` and the
/// resolution flows can be wired up without touching the screen layer.
///
/// **Hub topics:** when the endpoint is configured, the adapter publishes
/// to `prediction:fincept:*` topics following the same family pattern
/// Polymarket / Kalshi already use. In demo mode, no topics are published
/// — screens read from the adapter's Qt signals directly.
class FinceptInternalAdapter : public PredictionExchangeAdapter {
    Q_OBJECT
  public:
    explicit FinceptInternalAdapter(QObject* parent = nullptr);
    ~FinceptInternalAdapter() override;

    // ── Identity ─────────────────────────────────────────────────────────
    QString id() const override;
    QString display_name() const override;
    ExchangeCapabilities capabilities() const override;

    // ── Read ─────────────────────────────────────────────────────────────
    void list_markets(const QString& category, const QString& sort_by, int limit, int offset) override;
    void list_events(const QString& category, const QString& sort_by, int limit, int offset) override;
    void search(const QString& query, int limit) override;
    void list_tags() override;
    void fetch_market(const MarketKey& key) override;
    void fetch_event(const MarketKey& key) override;
    void fetch_order_book(const QString& asset_id) override;
    void fetch_price_history(const QString& asset_id, const QString& interval, int fidelity) override;
    void fetch_recent_trades(const MarketKey& key, int limit) override;

    // ── Real-time ────────────────────────────────────────────────────────
    void subscribe_market(const QStringList& asset_ids) override;
    void unsubscribe_market(const QStringList& asset_ids) override;
    bool is_ws_connected() const override;

    // ── Trading (auth-gated) ─────────────────────────────────────────────
    bool has_credentials() const override;
    QString account_label() const override;
    void fetch_balance() override;
    void fetch_positions() override;
    void fetch_open_orders() override;
    void fetch_user_activity(int limit) override;
    void place_order(const OrderRequest& req) override;
    void cancel_order(const QString& order_id) override;
    void cancel_all_for_market(const MarketKey& key, const QString& asset_id) override;

    // ── Hub ──────────────────────────────────────────────────────────────
    void ensure_registered_with_hub() override;

    /// True iff `fincept.markets_endpoint` is configured. Screens can use
    /// this to surface a "service in demo mode" banner when the endpoint
    /// is missing — distinct from `has_credentials()` which gates trading.
    bool is_demo_mode() const;

  private:
    /// Resolve the endpoint from SecureStorage. Empty string → demo mode.
    QString resolve_endpoint() const;

    /// Mock-mode emitters — emit the curated dataset described in plan §4.2.
    /// All three return synchronously via QTimer::singleShot(0) so signal
    /// handlers see the same async semantics as the real path.
    void emit_mock_markets();
    void emit_mock_events();
    void emit_mock_tags();
    void emit_demo_unavailable(const QString& context);

    QNetworkAccessManager* nam_ = nullptr;
    bool hub_registered_ = false;
};

} // namespace fincept::services::prediction::fincept_internal
