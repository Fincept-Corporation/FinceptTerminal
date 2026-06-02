#pragma once
// Crypto Order Book — custom-painted dual-column with depth bars

#include "screens/crypto_trading/CryptoTypes.h"

#include <QEvent>
#include <QLabel>
#include <QMutex>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoOrderBook : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoOrderBook(QWidget* parent = nullptr);

    void set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks, double spread,
                  double spread_pct);

    void add_tick_snapshot(const TickSnapshot& snap);

  signals:
    void price_clicked(double price);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_cache();
    void set_active_mode(int idx);
    void retranslateUi();

    // Mode buttons
    QPushButton* mode_btns_[4] = {};
    QLabel* title_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    bool has_spread_data_ = false; // gates whether spread_label_ shows live data vs placeholder
    QWidget* canvas_ = nullptr;

    // Data (mutex-protected)
    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    double spread_ = 0;
    double spread_pct_ = 0;
    QVector<TickSnapshot> tick_history_;

    ObViewMode view_mode_ = ObViewMode::Book;
    QPixmap cache_;
    bool cache_dirty_ = true;
    QMutex mutex_;
    QTimer* repaint_timer_ = nullptr; // 50ms coalesce — max 20fps

    // Layout constants
    static constexpr int ROW_H = 20;
    static constexpr int HEADER_H = 30;
    static constexpr int SPREAD_H = 22;
};

} // namespace fincept::screens::crypto
