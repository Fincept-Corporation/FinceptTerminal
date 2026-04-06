#pragma once
// Crypto Price Ribbon — dense single-line: price, change, bid/ask, spread, high/low/volume, WS status

#include <QLabel>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoTickerBar : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoTickerBar(QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);
    void update_data(double price, double change_pct, double high, double low, double volume, bool ws_connected);
    void update_bid_ask(double bid, double ask, double spread);
    void update_mark_price(double mark_price, double index_price);

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
    QLabel* mark_price_label_ = nullptr;
    QLabel* index_price_label_ = nullptr;

    // State cache — avoid redundant style updates
    bool last_positive_ = true;
};

} // namespace fincept::screens::crypto
