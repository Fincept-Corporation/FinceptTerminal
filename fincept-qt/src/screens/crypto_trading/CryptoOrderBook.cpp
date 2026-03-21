#include "screens/crypto_trading/CryptoOrderBook.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMutexLocker>
#include <QDateTime>
#include <cmath>

namespace fincept::screens::crypto {

CryptoOrderBook::CryptoOrderBook(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(2);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel("ORDER BOOK");
    title->setStyleSheet("font-weight: bold; color: #00aaff; font-size: 12px;");
    header->addWidget(title);

    mode_combo_ = new QComboBox;
    mode_combo_->addItems({"Book", "Volume", "Imbalance", "Signals"});
    mode_combo_->setFixedWidth(90);
    connect(mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        view_mode_ = static_cast<ObViewMode>(idx);
        update_display();
    });
    header->addWidget(mode_combo_);
    layout->addLayout(header);

    table_ = new QTableWidget;
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Price", "Amount", "Total"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->setShowGrid(false);
    table_->setStyleSheet(
        "QTableWidget { background: #0d1117; border: none; color: #ccc; font-size: 11px; }"
        "QTableWidget::item { padding: 1px 3px; }");
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto* item = table_->item(row, 0);
        if (item) emit price_clicked(item->text().toDouble());
    });
    layout->addWidget(table_, 1);

    spread_label_ = new QLabel("Spread: --");
    spread_label_->setAlignment(Qt::AlignCenter);
    spread_label_->setStyleSheet("background: #1a1a2e; padding: 3px; font-weight: bold; "
                                  "font-size: 11px; color: #aaa;");
    layout->addWidget(spread_label_);
}

void CryptoOrderBook::set_data(const QVector<QPair<double, double>>& bids,
                                const QVector<QPair<double, double>>& asks,
                                double spread, double spread_pct) {
    QMutexLocker lock(&mutex_);
    bids_ = bids;
    asks_ = asks;
    spread_ = spread;
    spread_pct_ = spread_pct;
    lock.unlock();
    update_display();
}

void CryptoOrderBook::add_tick_snapshot(const TickSnapshot& snap) {
    QMutexLocker lock(&mutex_);
    tick_history_.append(snap);
    if (tick_history_.size() > OB_MAX_TICK_HISTORY) {
        tick_history_.remove(0, tick_history_.size() - OB_MAX_TICK_HISTORY);
    }
}

void CryptoOrderBook::update_display() {
    switch (view_mode_) {
        case ObViewMode::Book:       render_book_mode(); break;
        case ObViewMode::Volume:     render_volume_mode(); break;
        case ObViewMode::Imbalance:  render_imbalance_mode(); break;
        case ObViewMode::Signals:    render_signals_mode(); break;
    }
}

void CryptoOrderBook::render_book_mode() {
    QMutexLocker lock(&mutex_);
    int ask_count = std::min(static_cast<int>(asks_.size()), OB_MAX_DISPLAY_LEVELS);
    int bid_count = std::min(static_cast<int>(bids_.size()), OB_MAX_DISPLAY_LEVELS);
    int total = ask_count + bid_count;

    table_->setRowCount(total);
    table_->setHorizontalHeaderLabels({"Price", "Amount", "Total"});

    // Asks (reversed — lowest at bottom near spread)
    double ask_cum = 0;
    for (int i = 0; i < ask_count; ++i) {
        int src = ask_count - 1 - i;
        ask_cum += asks_[src].second;
        auto* price = new QTableWidgetItem(QString::number(asks_[src].first, 'f', 2));
        price->setForeground(QColor("#ff4444"));
        table_->setItem(i, 0, price);
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(asks_[src].second, 'f', 4)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::number(ask_cum, 'f', 4)));
    }

    // Bids
    double bid_cum = 0;
    for (int i = 0; i < bid_count; ++i) {
        bid_cum += bids_[i].second;
        int row = ask_count + i;
        auto* price = new QTableWidgetItem(QString::number(bids_[i].first, 'f', 2));
        price->setForeground(QColor("#00ff88"));
        table_->setItem(row, 0, price);
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(bids_[i].second, 'f', 4)));
        table_->setItem(row, 2, new QTableWidgetItem(QString::number(bid_cum, 'f', 4)));
    }

    spread_label_->setText(QString("Spread: %1 (%2%)")
        .arg(spread_, 0, 'f', 2).arg(spread_pct_, 0, 'f', 4));
}

void CryptoOrderBook::render_volume_mode() {
    QMutexLocker lock(&mutex_);
    int ask_count = std::min(static_cast<int>(asks_.size()), OB_MAX_DISPLAY_LEVELS);
    int bid_count = std::min(static_cast<int>(bids_.size()), OB_MAX_DISPLAY_LEVELS);

    table_->setRowCount(ask_count + bid_count);
    table_->setHorizontalHeaderLabels({"Price", "Volume", "% of Max"});

    // Find max volume for bar scaling
    double max_vol = 0;
    for (int i = 0; i < ask_count; ++i) max_vol = std::max(max_vol, asks_[i].second);
    for (int i = 0; i < bid_count; ++i) max_vol = std::max(max_vol, bids_[i].second);
    if (max_vol <= 0) max_vol = 1;

    for (int i = 0; i < ask_count; ++i) {
        int src = ask_count - 1 - i;
        auto* price = new QTableWidgetItem(QString::number(asks_[src].first, 'f', 2));
        price->setForeground(QColor("#ff4444"));
        table_->setItem(i, 0, price);
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(asks_[src].second, 'f', 4)));
        double pct = (asks_[src].second / max_vol) * 100.0;
        table_->setItem(i, 2, new QTableWidgetItem(QString("%1%").arg(pct, 0, 'f', 1)));
    }

    for (int i = 0; i < bid_count; ++i) {
        int row = ask_count + i;
        auto* price = new QTableWidgetItem(QString::number(bids_[i].first, 'f', 2));
        price->setForeground(QColor("#00ff88"));
        table_->setItem(row, 0, price);
        table_->setItem(row, 1, new QTableWidgetItem(QString::number(bids_[i].second, 'f', 4)));
        double pct = (bids_[i].second / max_vol) * 100.0;
        table_->setItem(row, 2, new QTableWidgetItem(QString("%1%").arg(pct, 0, 'f', 1)));
    }
}

void CryptoOrderBook::render_imbalance_mode() {
    QMutexLocker lock(&mutex_);
    table_->setHorizontalHeaderLabels({"Time", "Imbalance", "Signal"});

    int count = std::min(static_cast<int>(tick_history_.size()), 30);
    table_->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        int idx = tick_history_.size() - 1 - i;
        const auto& snap = tick_history_[idx];

        QDateTime dt = QDateTime::fromMSecsSinceEpoch(snap.timestamp);
        table_->setItem(i, 0, new QTableWidgetItem(dt.toString("HH:mm:ss")));

        auto* imb = new QTableWidgetItem(QString("%1").arg(snap.imbalance, 0, 'f', 3));
        imb->setForeground(snap.imbalance > 0 ? QColor("#00ff88") : QColor("#ff4444"));
        table_->setItem(i, 1, imb);

        QString signal;
        if (snap.imbalance > OB_IMBALANCE_BUY_THRESHOLD) signal = "BUY PRESSURE";
        else if (snap.imbalance < OB_IMBALANCE_SELL_THRESHOLD) signal = "SELL PRESSURE";
        else signal = "NEUTRAL";

        auto* sig = new QTableWidgetItem(signal);
        sig->setForeground(signal == "BUY PRESSURE" ? QColor("#00ff88")
                           : signal == "SELL PRESSURE" ? QColor("#ff4444")
                           : QColor("#888"));
        table_->setItem(i, 2, sig);
    }
}

void CryptoOrderBook::render_signals_mode() {
    QMutexLocker lock(&mutex_);
    table_->setHorizontalHeaderLabels({"Time", "Rise%", "Action"});

    int count = std::min(static_cast<int>(tick_history_.size()), 30);
    table_->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        int idx = tick_history_.size() - 1 - i;
        const auto& snap = tick_history_[idx];

        QDateTime dt = QDateTime::fromMSecsSinceEpoch(snap.timestamp);
        table_->setItem(i, 0, new QTableWidgetItem(dt.toString("HH:mm:ss")));

        auto* rise = new QTableWidgetItem(QString("%1%").arg(snap.rise_ratio_60 * 100.0, 0, 'f', 2));
        rise->setForeground(snap.rise_ratio_60 >= 0 ? QColor("#00ff88") : QColor("#ff4444"));
        table_->setItem(i, 1, rise);

        // Combined signal from imbalance + momentum
        QString action = "HOLD";
        if (snap.imbalance > OB_IMBALANCE_BUY_THRESHOLD && snap.rise_ratio_60 > 0.001) action = "STRONG BUY";
        else if (snap.imbalance < OB_IMBALANCE_SELL_THRESHOLD && snap.rise_ratio_60 < -0.001) action = "STRONG SELL";
        else if (snap.imbalance > OB_IMBALANCE_BUY_THRESHOLD) action = "BUY SIGNAL";
        else if (snap.imbalance < OB_IMBALANCE_SELL_THRESHOLD) action = "SELL SIGNAL";

        auto* act = new QTableWidgetItem(action);
        act->setForeground(action.contains("BUY") ? QColor("#00ff88")
                           : action.contains("SELL") ? QColor("#ff4444")
                           : QColor("#888"));
        table_->setItem(i, 2, act);
    }
}

} // namespace fincept::screens::crypto
