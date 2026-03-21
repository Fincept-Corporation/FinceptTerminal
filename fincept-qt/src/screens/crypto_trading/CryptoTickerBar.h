#pragma once
// Crypto Ticker Bar — displays price, change, high/low/volume for selected symbol

#include <QWidget>
#include <QLabel>

namespace fincept::screens::crypto {

class CryptoTickerBar : public QWidget {
    Q_OBJECT
public:
    explicit CryptoTickerBar(QWidget* parent = nullptr);

    void set_symbol(const QString& symbol);
    void update_data(double price, double change_pct, double high, double low,
                     double volume, bool ws_connected);

private:
    QLabel* symbol_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* high_label_ = nullptr;
    QLabel* low_label_ = nullptr;
    QLabel* volume_label_ = nullptr;
    QLabel* ws_indicator_ = nullptr;
};

} // namespace fincept::screens::crypto
