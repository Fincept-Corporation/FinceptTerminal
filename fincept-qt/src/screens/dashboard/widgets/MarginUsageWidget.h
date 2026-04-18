#pragma once
// MarginUsageWidget — dashboard tile showing broker account funds: available
// balance, used margin, total, collateral, and a usage % progress bar.
// Subscribes to `broker:<id>:<account>:balance` (TTL 30s).
//
// Config (persisted): { "account_id": "<uuid>" }

#include "screens/dashboard/widgets/BaseWidget.h"
#include "trading/TradingTypes.h"

#include <QString>

class QLabel;
class QProgressBar;

namespace fincept::screens::widgets {

class MarginUsageWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit MarginUsageWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void populate(const trading::BrokerFunds& funds);
    void ensure_stream_running();
    void hub_resubscribe();
    void hub_unsubscribe_all();
    QString resolve_account_id() const;

    QString account_id_;
    QString broker_id_;

    QLabel* header_hint_ = nullptr;
    QLabel* available_val_ = nullptr;
    QLabel* used_val_ = nullptr;
    QLabel* total_val_ = nullptr;
    QLabel* collateral_val_ = nullptr;
    QLabel* usage_pct_label_ = nullptr;
    QProgressBar* usage_bar_ = nullptr;

    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
