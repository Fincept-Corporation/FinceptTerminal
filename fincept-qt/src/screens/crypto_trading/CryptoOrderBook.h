#pragma once
// Crypto Order Book — displays bids/asks with multiple view modes

#include "screens/crypto_trading/CryptoTypes.h"
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QVector>
#include <QPair>
#include <QMutex>

namespace fincept::screens::crypto {

class CryptoOrderBook : public QWidget {
    Q_OBJECT
public:
    explicit CryptoOrderBook(QWidget* parent = nullptr);

    void set_data(const QVector<QPair<double, double>>& bids,
                  const QVector<QPair<double, double>>& asks,
                  double spread, double spread_pct);

    void add_tick_snapshot(const TickSnapshot& snap);

signals:
    void price_clicked(double price);

private:
    void render_book_mode();
    void render_volume_mode();
    void render_imbalance_mode();
    void render_signals_mode();
    void update_display();

    QComboBox* mode_combo_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* spread_label_ = nullptr;

    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    double spread_ = 0;
    double spread_pct_ = 0;

    QVector<TickSnapshot> tick_history_;
    ObViewMode view_mode_ = ObViewMode::Book;
    QMutex mutex_;
};

} // namespace fincept::screens::crypto
