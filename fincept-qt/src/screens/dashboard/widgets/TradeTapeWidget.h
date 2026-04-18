#pragma once
// TradeTapeWidget — live trade tape for a single exchange/pair. Subscribes to
// `ws:<exchange>:trades:<pair>` (TradeData). Shows last N trades with side
// colour and time.
//
// Config (persisted):
//   { "exchange": "kraken"|"hyperliquid", "pair": "BTC/USD", "max_rows": <int> }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QDateTime>
#include <QString>
#include <QVector>

class QTableWidget;

namespace fincept::screens::widgets {

class TradeTapeWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit TradeTapeWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    QDialog* make_config_dialog(QWidget* parent) override;

  private:
    struct Trade {
        QDateTime when;
        QString side;
        double price = 0.0;
        double amount = 0.0;
    };

    void apply_styles();
    void on_trade(const QVariant& v);
    void render();
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QString exchange_ = "kraken";
    QString pair_ = "BTC/USD";
    int max_rows_ = 20;

    QVector<Trade> trades_;
    QTableWidget* table_ = nullptr;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
