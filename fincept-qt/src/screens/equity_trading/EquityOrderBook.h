#pragma once
// Equity Order Book / Market Depth — custom-painted stacked DOM with
// cumulative depth bars, best-of-book emphasis, spread band and an
// order-flow imbalance footer. All analytics derive from the bid/ask
// arrays supplied to set_data(); no extra broker plumbing required.

#include "screens/equity_trading/EquityTypes.h"

#include <QEvent>
#include <QLabel>
#include <QMutex>
#include <QPair>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens::equity {

class EquityOrderBook : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOrderBook(QWidget* parent = nullptr);

    void set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks, double spread,
                  double spread_pct);
    void set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks, double spread,
                  double spread_pct, const QVector<int>& bid_orders, const QVector<int>& ask_orders);

  signals:
    void price_clicked(double price);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_cache();
    void retranslateUi();
    // Maps a widget-space y to a price level. Returns true on a hit and fills
    // `is_ask`/`index` (index into asks_/bids_, 0 = best of book).
    bool level_at_y(int widget_y, bool& is_ask, int& index) const;

    QLabel* title_label_ = nullptr;
    QLabel* levels_label_ = nullptr; // right-of-header badge: "L2 · 5×5" / "L1 SYNTH"
    QWidget* canvas_ = nullptr;      // transparent layout spacer; painting happens on `this`

    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    QVector<int> bid_orders_;
    QVector<int> ask_orders_;
    double spread_ = 0;
    double spread_pct_ = 0;
    bool has_data_ = false;

    QPixmap cache_;
    bool cache_dirty_ = true;
    QMutex mutex_;
    QTimer* repaint_timer_ = nullptr;

    // Painted-band geometry in cache/pixmap coordinates (origin at HEADER_H),
    // recomputed each rebuild_cache() and reused for hit-testing.
    int g_ask_top_ = 0;
    int g_bid_top_ = 0;

    // Hover state (drawn as an overlay on top of the cache, not baked in).
    int hover_index_ = -1;
    bool hover_is_ask_ = false;

    static constexpr int ROW_H = 22;
    static constexpr int HEADER_H = 28;
    static constexpr int COLHDR_H = 18;
    static constexpr int SPREAD_H = 26;
    static constexpr int FOOTER_H = 26;
    static constexpr int MAX_LEVELS = 5; // visible levels per side
};

} // namespace fincept::screens::equity
