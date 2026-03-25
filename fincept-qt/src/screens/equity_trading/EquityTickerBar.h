#pragma once
// Equity Price Ribbon — dense single-line: symbol, price, change, bid/ask, spread, high/low/volume

#include <QLabel>
#include <QWidget>

namespace fincept::screens::equity {

class EquityTickerBar : public QWidget {
    Q_OBJECT
  public:
    explicit EquityTickerBar(QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);
    void set_currency(const QString& currency_sym) { currency_sym_ = currency_sym; }
    void update_quote(double ltp, double change, double change_pct, double high, double low, double volume, double bid,
                      double ask);

  private:
    QLabel* symbol_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* bid_label_ = nullptr;
    QLabel* ask_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    QLabel* high_label_ = nullptr;
    QLabel* low_label_ = nullptr;
    QLabel* volume_label_ = nullptr;

    bool last_positive_ = true;
    QString currency_sym_ = "$";
};

} // namespace fincept::screens::equity
