#pragma once
// TradeTapeWidget — live trade tape for a single exchange/pair. Subscribes to
// `ws:<exchange>:trades:<pair>` (a QVector<TradeData> batch per ~100 ms window;
// a single TradeData is also accepted). Shows last N trades with side colour
// and time.
//
// Config (persisted):
//   { "exchange": "kraken"|"hyperliquid", "pair": "BTC/USD", "max_rows": <int> }

#include "screens/dashboard/widgets/BaseWidget.h"

#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QString>
#include <QVector>

class QTableWidget;

namespace fincept::trading {
struct TradeData;
}

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
    void retranslateUi() override;
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
    void prepend_trade(const fincept::trading::TradeData& td);
    /// Coalesces render() calls and caps them at 10 Hz.
    void request_render();
    /// Incremental: inserts the trades that arrived since the last render at
    /// the top and drops the overflow off the bottom — existing items are kept.
    void render();
    /// Full resync of every row, reusing items (config / theme / language).
    void render_all();
    void fill_row(int row, const Trade& t, const QColor& buy, const QColor& sell);
    void hub_resubscribe();
    void hub_unsubscribe_all();

    QString exchange_ = "kraken";
    QString pair_ = "BTC/USD";
    int max_rows_ = 20;

    QVector<Trade> trades_;
    int unrendered_ = 0; // trades prepended since the last render (capped at max_rows_)
    QTableWidget* table_ = nullptr;
    QElapsedTimer last_render_;
    bool render_armed_ = false;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
