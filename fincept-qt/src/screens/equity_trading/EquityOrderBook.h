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

namespace fincept::ui {
class PanelHeader;
}

namespace fincept::screens::equity {

class EquityOrderBook : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOrderBook(QWidget* parent = nullptr);

    void set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks, double spread,
                  double spread_pct);
    void set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks, double spread,
                  double spread_pct, const QVector<int>& bid_orders, const QVector<int>& ask_orders);

    // Drop the book (shows "No depth data"). The depth feed is not tagged with a symbol, so
    // the owner calls this when the charted symbol/account changes — otherwise the previous
    // symbol's ladder stays on screen, and a click on it would fill the new ticket's limit
    // price with a level that belongs to the old symbol.
    void clear();

    // What the empty ladder says instead of the generic "No depth data" — e.g. that
    // depth needs a broker connection. Empty restores the generic text.
    void set_empty_message(const QString& text);
    // Levels shown per side (1..5): 5 on a wide screen, fewer where the column is
    // short (EquityLayoutPlan). The levels held are kept; only the ladder's height
    // and painting change.
    void set_visible_levels(int levels);
    int visible_levels() const { return levels_; }

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
    // Sizes the header and the painted bands for the active font and pins the
    // widget to the resulting content height.
    void update_metrics();
    // Maps a widget-space y to a price level. Returns true on a hit and fills
    // `is_ask`/`index` (index into asks_/bids_, 0 = best of book).
    bool level_at_y(int widget_y, bool& is_ask, int& index) const;

    fincept::ui::PanelHeader* header_ = nullptr; // "MARKET DEPTH" + level badge
    QWidget* canvas_ = nullptr;      // transparent layout spacer; painting happens on `this`

    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    QVector<int> bid_orders_;
    QVector<int> ask_orders_;
    double spread_ = 0;
    double spread_pct_ = 0;
    bool has_data_ = false;
    QString empty_message_; // see set_empty_message()

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

    // Band heights at the default font size (the minimums)...
    static constexpr int ROW_H = 22;
    static constexpr int HEADER_H = 28;
    static constexpr int COLHDR_H = 18;
    static constexpr int SPREAD_H = 26;
    static constexpr int FOOTER_H = 26;
    static constexpr int MAX_LEVELS = 5; // levels held per side (the most ever shown)
    int levels_ = MAX_LEVELS;            // levels shown per side (set_visible_levels)
    QString levels_badge_;               // "L2 · 5×5" / "L1 · synthetic"

    // ...and for the active font size (see update_metrics()).
    int row_h_ = ROW_H;
    int header_h_ = HEADER_H;
    int colhdr_h_ = COLHDR_H;
    int spread_h_ = SPREAD_H;
    int footer_h_ = FOOTER_H;
};

} // namespace fincept::screens::equity
