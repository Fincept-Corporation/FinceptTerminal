#pragma once
// OrderBookMiniWidget — dashboard tile showing working (pending) orders for
// one broker account. Subscribes to `broker:<id>:<account>:orders`. Clicking
// a row reveals an inline Cancel button that fires UnifiedTrading::cancel_order
// on a worker thread (BrokerHttp blocking trap, P1).
//
// Config (persisted): { "account_id": "<uuid>" }
//
// By default only NON-terminal orders are displayed (status not in
// {COMPLETE, CANCELLED, REJECTED, FILLED}). Widget does not filter by
// symbol; users can open a per-symbol view from Equity Trading.

#include "screens/dashboard/widgets/BaseWidget.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QTableWidget>
#include <QVector>

namespace fincept::screens::widgets {

class OrderBookMiniWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit OrderBookMiniWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void populate(const QVector<trading::BrokerOrderInfo>& rows);
    void cancel_order(const QString& order_id);
    void ensure_stream_running();
    void hub_resubscribe();
    void hub_unsubscribe_all();
    QString resolve_account_id() const;
    static bool is_working(const QString& status);

    QString account_id_;
    QString broker_id_;

    QTableWidget* table_ = nullptr;
    QLabel* header_hint_ = nullptr;

    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
