#pragma once

#include "services/wallet/WalletTypes.h"

#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QHideEvent;
class QLabel;
class QPushButton;
class QShowEvent;
class QTableWidget;

namespace fincept::screens {

/// ACTIVITY tab — last 50 parsed wallet operations (Phase 2 §2B).
///
/// Subscribes to `wallet:activity:<pubkey>` (visibility-driven, P3 + D3).
/// Renders a table with TIMESTAMP / EVENT / ASSET / AMOUNT / STATUS
/// columns. Filter chips (ALL / SWAP / SEND / RECEIVE / OTHER) narrow the
/// view client-side.
///
/// Whole-row click opens the transaction's signature in Solscan via the
/// system browser. No in-app explorer.
///
/// When no Helius API key is configured, the producer falls back to raw
/// signatures from the standard RPC and rows show only TIMESTAMP +
/// SIGNATURE + STATUS. The footer hints "Add a Helius key in Settings for
/// parsed activity".
class ActivityTab : public QWidget {
    Q_OBJECT
  public:
    explicit ActivityTab(QWidget* parent = nullptr);
    ~ActivityTab() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    using Activity = fincept::wallet::ParsedActivity;
    using Kind = Activity::Kind;

    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_activity_update(const QVariant& v);
    void on_topic_error(const QString& topic, const QString& error);
    void on_filter_changed();
    void on_row_clicked(int row, int column);

    void refresh_subscription();
    void rebuild_table();
    bool helius_key_present() const;

    QTableWidget* table_ = nullptr;
    QLabel* footer_ = nullptr;
    QPushButton* filter_all_ = nullptr;
    QPushButton* filter_swap_ = nullptr;
    QPushButton* filter_send_ = nullptr;
    QPushButton* filter_receive_ = nullptr;
    QPushButton* filter_other_ = nullptr;

    QString current_pubkey_;
    QString current_topic_;
    QVector<Activity> latest_;
    Kind active_filter_kind_ = Kind::Other; // sentinel — `filter_all_` checked = no filter
    bool show_all_ = true;
};

} // namespace fincept::screens
