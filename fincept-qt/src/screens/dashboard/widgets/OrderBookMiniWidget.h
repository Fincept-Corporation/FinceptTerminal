#pragma once
// OrderBookMiniWidget — dashboard tile showing working (pending) orders for
// one broker account. Subscribes to `broker:<id>:<account>:orders`. Clicking
// a row reveals an inline Cancel button that fires UnifiedTrading::cancel_order
// on a worker thread (BrokerHttp blocking trap, P1).
//
// Config (persisted): { "account_id": "<uuid>" }
//
// Only NON-terminal orders are displayed — the terminal-status list mirrors
// the Equity Trading blotter's (complete/filled/traded/executed/cancelled/
// rejected/expired/closed/replaced/done_for_day/stopped/suspended). Widget
// does not filter by symbol; users can open a per-symbol view from Equity
// Trading.

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
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    void apply_styles();
    void populate(const QVector<trading::BrokerOrderInfo>& rows);
    void cancel_order(const QString& order_id);
    /// After the user confirmed: cancel off the UI thread, report a refusal.
    void send_cancel(const QString& acct_id, const QString& order_id);
    /// Account name in the header, or why the broker session is unusable.
    void update_session_state();
    /// No account: "No broker account connected" over the tile (BaseWidget helper).
    void show_no_account_state();
    QString orders_topic() const;
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
    bool showing_empty_ = false; // table currently shows the "No working orders" row
    /// The last order list rendered — re-rendered on a theme change (side colours live in the items).
    QVector<trading::BrokerOrderInfo> orders_;
};

} // namespace fincept::screens::widgets
