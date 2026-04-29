#pragma once

#include "services/wallet/WalletTypes.h"

#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTableWidgetItem;

namespace fincept::screens::panels {

/// Holdings table — replaces HomeTab's three stat boxes (Stage 2A.5).
///
/// Renders one row per asset the wallet holds:
///   TOKEN  ·  BALANCE  ·  PRICE  ·  USD VALUE  ·  % OF PORTFOLIO
///
/// Subscriptions (visibility-driven):
///   - wallet:balance:<pubkey>            for the holdings list
///   - market:price:token:<mint>          one per held mint, refreshed when
///                                        the holdings list changes
///
/// Spam-token policy (Phase 2 §2A.5 D.5.4): unverified tokens are hidden by
/// default. The "Show all" button toggles them in for the current session;
/// the persistent setting lives in SettingsTab.
class HoldingsTable : public QWidget {
    Q_OBJECT
  public:
    explicit HoldingsTable(QWidget* parent = nullptr);
    ~HoldingsTable() override;

  signals:
    /// Emitted when the user clicks a row. Payload is the SPL mint of the
    /// selected asset (or the wrapped-SOL mint for SOL). Wired to
    /// SwapPanel's FROM combo in 2A.5.10.
    void select_token(QString mint);

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_balance_update(const QVariant& v);
    void on_price_update(const QString& mint, const QVariant& v);
    void on_show_all_clicked();
    void on_metadata_changed();

    void resubscribe_prices();
    void refresh_subscription();
    void rebuild_table();

    // UI
    QTableWidget* table_ = nullptr;
    QLabel* footer_ = nullptr;
    QPushButton* show_all_button_ = nullptr;

    // State
    QString current_pubkey_;
    QString current_balance_topic_;
    fincept::wallet::WalletBalance latest_balance_;
    QHash<QString, double> price_usd_;     ///< mint → USD price
    QHash<QString, QString> price_topic_;  ///< mint → topic (so we can unsubscribe)
    bool show_unverified_ = false;
};

} // namespace fincept::screens::panels
