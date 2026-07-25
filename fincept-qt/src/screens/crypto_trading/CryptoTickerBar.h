#pragma once
// Crypto Price Ribbon — dense single-line: price, change, bid/ask, spread, high/low/volume, WS status

#include <QLabel>
#include <QString>
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

    // State cache — avoid redundant setText calls at ticker rate.
    //
    // Cached as the *rendered strings*, not as values rounded to a fixed 2 dp.
    // The old scheme compared `round(v, 2)`, so on a sub-cent pair every tick
    // rounded to the same 0.00 and the labels were never updated at all: the
    // ribbon looked frozen for exactly the assets whose price moves most.
    // Comparing the final text is both correct at every magnitude and still
    // suppresses repaints when nothing visible changed.
    bool last_positive_ = true;
    QString last_price_text_;
    QString last_change_text_;
    QString last_high_text_;
    QString last_low_text_;
    QString last_volume_text_;
    QString last_bid_text_;
    QString last_ask_text_;
    QString last_spread_text_;
};

} // namespace fincept::screens::crypto
