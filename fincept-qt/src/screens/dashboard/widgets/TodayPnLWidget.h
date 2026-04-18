#pragma once
// TodayPnLWidget — aggregated P&L tile for one broker account. Subscribes to
// `broker:<id>:<account>:positions` and sums total P&L + day P&L across all
// positions. Displays large P&L figure with signed colour, day vs total
// breakdown, and open-position count.
//
// Config (persisted): { "account_id": "<uuid>" }

#include "screens/dashboard/widgets/BaseWidget.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QVector>

class QLabel;

namespace fincept::screens::widgets {

class TodayPnLWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit TodayPnLWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void populate(const QVector<trading::BrokerPosition>& rows);
    void ensure_stream_running();
    void hub_resubscribe();
    void hub_unsubscribe_all();
    QString resolve_account_id() const;

    QString account_id_;
    QString broker_id_;

    QLabel* header_hint_ = nullptr;
    QLabel* total_pnl_label_ = nullptr;
    QLabel* day_pnl_label_ = nullptr;
    QLabel* day_pnl_value_ = nullptr;
    QLabel* realized_pnl_label_ = nullptr;
    QLabel* realized_pnl_value_ = nullptr;
    QLabel* positions_label_ = nullptr;
    QLabel* positions_value_ = nullptr;

    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
