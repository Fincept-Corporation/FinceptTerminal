#include "screens/crypto_trading/CryptoTickerBar.h"
#include <QHBoxLayout>

namespace fincept::screens::crypto {

CryptoTickerBar::CryptoTickerBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(16);

    symbol_label_ = new QLabel("BTC/USDT");
    symbol_label_->setStyleSheet("font-weight: bold; font-size: 13px; color: #00aaff;");
    layout->addWidget(symbol_label_);

    price_label_ = new QLabel("--");
    price_label_->setStyleSheet("font-size: 18px; font-weight: bold; color: #00ff88;");
    layout->addWidget(price_label_);

    change_label_ = new QLabel("--");
    change_label_->setStyleSheet("font-size: 13px;");
    layout->addWidget(change_label_);

    layout->addSpacing(12);

    high_label_ = new QLabel("H: --");
    high_label_->setStyleSheet("color: #aaa; font-size: 12px;");
    layout->addWidget(high_label_);

    low_label_ = new QLabel("L: --");
    low_label_->setStyleSheet("color: #aaa; font-size: 12px;");
    layout->addWidget(low_label_);

    volume_label_ = new QLabel("Vol: --");
    volume_label_->setStyleSheet("color: #aaa; font-size: 12px;");
    layout->addWidget(volume_label_);

    layout->addStretch();

    ws_indicator_ = new QLabel("[REST]");
    ws_indicator_->setStyleSheet("color: #ff8800; font-size: 11px; font-weight: bold;");
    layout->addWidget(ws_indicator_);

    setStyleSheet("background: #0d1117; border-bottom: 1px solid #222;");
    setFixedHeight(36);
}

void CryptoTickerBar::set_symbol(const QString& symbol) {
    symbol_label_->setText(symbol);
}

void CryptoTickerBar::update_data(double price, double change_pct, double high,
                                    double low, double volume, bool ws_connected) {
    if (price <= 0) return;

    price_label_->setText(QString("$%1").arg(price, 0, 'f', 2));

    bool positive = change_pct >= 0;
    change_label_->setText(QString("%1%2%").arg(positive ? "+" : "").arg(change_pct, 0, 'f', 2));
    change_label_->setStyleSheet(
        QString("font-size: 13px; color: %1;").arg(positive ? "#00ff88" : "#ff4444"));

    high_label_->setText(QString("H: %1").arg(high, 0, 'f', 2));
    low_label_->setText(QString("L: %1").arg(low, 0, 'f', 2));

    if (volume >= 1e9) {
        volume_label_->setText(QString("Vol: %1B").arg(volume / 1e9, 0, 'f', 2));
    } else if (volume >= 1e6) {
        volume_label_->setText(QString("Vol: %1M").arg(volume / 1e6, 0, 'f', 2));
    } else {
        volume_label_->setText(QString("Vol: %1").arg(volume, 0, 'f', 0));
    }

    ws_indicator_->setText(ws_connected ? "[WS LIVE]" : "[REST]");
    ws_indicator_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;")
        .arg(ws_connected ? "#00ff88" : "#ff8800"));
}

} // namespace fincept::screens::crypto
