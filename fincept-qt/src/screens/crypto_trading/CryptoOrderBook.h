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

    /// Drop the book, the spread readout and the Imb / Sig history. Called on a
    /// symbol / exchange switch so the previous pair's levels can't be shown (or
    /// clicked into the order ticket) while the new pair's first snapshot loads.
    void clear();

    /// The market the levels come from: its quote asset is the price unit named
    /// in the BID / ASK headers, and it decides the price decimals.
    void set_price_market(const QString& market);

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
    void render_spread_label(); // from spread_ / spread_pct_ (format change, market change)

    QString price_market_; // set by the screen (set_price_market); empty = unit not known yet

    // Mode buttons
    QPushButton* mode_btns_[4] = {};
    QLabel* title_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    bool has_spread_data_ = false; // gates whether spread_label_ shows live data vs placeholder
    // (Removed `canvas_`: never assigned or read — the book is painted
    // directly onto this widget via `cache_`.)

    // Data (mutex-protected)
    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    double spread_ = 0;
    double spread_pct_ = 0;
    QVector<TickSnapshot> tick_history_;
    qint64 last_tick_capture_ms_ = 0; // rate-limits the Imb / Sig snapshots to OB_TICK_CAPTURE_MS

    ObViewMode view_mode_ = ObViewMode::Book;
    QPixmap cache_;
    bool cache_dirty_ = true;
    QMutex mutex_;
    QTimer* repaint_timer_ = nullptr; // 50ms coalesce — max 20fps

    // Layout minimums (the sizes at the default font size; rows, header and
    // spread line grow with the user's font)
    static constexpr int ROW_H = 20;
    static constexpr int HEADER_H = 30;
    static constexpr int SPREAD_H = 22;
};

} // namespace fincept::screens::crypto
