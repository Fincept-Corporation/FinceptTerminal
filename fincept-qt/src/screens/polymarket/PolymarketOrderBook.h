#pragma once

#include "services/prediction/PredictionTypes.h"

#include <QEvent>
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

    void set_data(const fincept::services::prediction::PredictionOrderBook& book);
    void clear();
    /// A book was requested for a newly selected market: placeholder rows and
    /// "Loading order book…" until set_data() / set_error().
    void set_loading();
    /// The request failed: the reason replaces the (empty) book.
    void set_error(const QString& message);

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
    // Number of ask/bid levels actually painted, derived from the current widget
    // height. Both the painter and the click hit-test MUST use this so a click
    // maps to the same level that was drawn. Caller must hold mutex_.
    void visible_row_counts(int& ask_rows, int& bid_rows) const;

    QVector<fincept::services::prediction::OrderLevel> bids_;
    QVector<fincept::services::prediction::OrderLevel> asks_;
    double spread_ = 0.0;
    int price_decimals_ = 2; // follows the book's tick size (0.001-tick markets need 3 dp)
    bool loading_ = false;   // a request is out and no book has arrived yet
    QString error_;          // the last request failed (shown while the book is empty)
    QMutex mutex_;
    QPixmap cache_;
    bool cache_dirty_ = true;
    QTimer* repaint_timer_ = nullptr;

    // Row / header heights follow the user's font size (20 / 28 px at the default).
    int row_height() const;
    int header_height() const;
};

} // namespace fincept::screens::polymarket
