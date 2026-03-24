#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QLabel>
#include <QMutex>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Custom-painted order book with depth visualization.
/// Caches to QPixmap, max 20fps repaint via coalescing timer (P9).
class PolymarketOrderBook : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketOrderBook(QWidget* parent = nullptr);

    void set_data(const services::polymarket::OrderBook& book);
    void clear();

  signals:
    void price_clicked(double price);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void rebuild_cache();

    QVector<services::polymarket::OrderLevel> bids_;
    QVector<services::polymarket::OrderLevel> asks_;
    double spread_ = 0.0;
    QMutex mutex_;
    QPixmap cache_;
    bool cache_dirty_ = true;
    QTimer* repaint_timer_ = nullptr;

    static constexpr int ROW_HEIGHT = 20;
    static constexpr int HEADER_HEIGHT = 28;
};

} // namespace fincept::screens::polymarket
