#pragma once
// BrokerHoldingsWidget — long-term holdings table for one broker account.
// Subscribes to `broker:<id>:<account>:holdings` (TTL 30s). Displays symbol,
// quantity, avg cost, LTP, P&L and total-return %.
//
// Config (persisted): { "account_id": "<uuid>" }

#include "screens/dashboard/widgets/BaseWidget.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QTableWidget>
#include <QVector>

class QPushButton;

namespace fincept::screens::widgets {

class BrokerHoldingsWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit BrokerHoldingsWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

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
    void populate(const QVector<trading::BrokerHolding>& rows);
    void ensure_stream_running();
    void hub_resubscribe();
    void hub_unsubscribe_all();
    QString resolve_account_id() const;

    // Square-off: places MARKET SELL / CNC orders for holdings of THIS account
    // only — never touches positions. `square_off_all` exits every holding;
    // `square_off_holding` exits one row.
    void square_off_all();
    void square_off_holding(const trading::BrokerHolding& h);

    QString account_id_;
    QString broker_id_;

    QTableWidget* table_ = nullptr;
    QLabel* header_hint_ = nullptr;
    QPushButton* square_off_all_btn_ = nullptr;

    // Last holdings rendered — source of truth for square-off actions.
    QVector<trading::BrokerHolding> holdings_;

    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
