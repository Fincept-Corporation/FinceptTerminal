#pragma once

#include "services/wallet/WalletTypes.h"

#include <QHash>
#include <QString>
#include <QVariant>
#include <QWidget>

class QHideEvent;
class QLabel;
class QShowEvent;
class QTimer;

namespace fincept::screens {

/// Persistent identity bar at the top of CryptoCenterScreen, visible across
/// every tab. Renders SOL · $FNCPT · TOTAL (USD) · feed status · RPC chip.
///
/// Phase 2 §2A.5: TOTAL is the sum across SOL + every priced verified
/// holding. Tokens whose price hasn't arrived (or which Jupiter doesn't
/// price) are excluded with a tooltip footnote.
///
/// Subscriptions (visibility-driven, P3 + D3):
///   - `wallet:balance:<pubkey>`              owned data feed
///   - `market:price:token:<mint>` (per held mint, dynamically subscribed)
///   - `billing:fncpt_discount:<pubkey>`      (Phase 2 Stage 2C — wired then)
class HoldingsBar : public QWidget {
    Q_OBJECT
  public:
    explicit HoldingsBar(QWidget* parent = nullptr);
    ~HoldingsBar() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    enum class FeedStatus { Idle, Connecting, Live, Stale, Error };

    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_balance_update(const QVariant& v);
    void on_price_update(const QString& mint, const QVariant& v);
    void on_discount_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_mode_changed(bool is_stream);

    void refresh_subscription();
    void resubscribe_prices();
    void recompute_total();
    void set_feed_status(FeedStatus s);
    void update_rpc_indicator();
    void update_staleness();

    QLabel* sol_value_ = nullptr;
    QLabel* fncpt_value_ = nullptr;
    QLabel* total_value_ = nullptr;     ///< sum of all priced holdings
    QLabel* fncpt_price_value_ = nullptr; ///< $FNCPT spot for legacy display
    QLabel* updated_value_ = nullptr;
    QLabel* feed_status_ = nullptr;
    QLabel* rpc_indicator_ = nullptr;
    QLabel* discount_chip_ = nullptr;  // hidden until Stage 2C wires it

    QTimer* staleness_timer_ = nullptr;

    QString current_pubkey_;
    QString current_balance_topic_;
    fincept::wallet::WalletBalance latest_balance_;
    QHash<QString, double> price_usd_;     ///< mint → USD price
    QHash<QString, QString> price_topic_;  ///< mint → topic for unsubscribe
    qint64 last_balance_ts_ = 0;
    FeedStatus feed_status_state_ = FeedStatus::Idle;
    bool first_publish_received_ = false;
};

} // namespace fincept::screens
