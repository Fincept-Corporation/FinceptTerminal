#pragma once
// Depth Chart — custom-painted bid/ask cumulative area chart

#include <QEvent>
#include <QMutex>
#include <QPair>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoDepthChart : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoDepthChart(QWidget* parent = nullptr);

    void set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks, double spread,
                  double spread_pct);
    /// The market the book comes from: its quote asset is the price axis's unit.
    void set_price_market(const QString& market);
    /// What the empty chart says instead of "Waiting for order book data…" (e.g.
    /// why the live stream is down); empty restores the waiting text.
    void set_status_text(const QString& text);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_cache();

    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    double spread_ = 0;
    QString price_market_; // empty = unit not known yet
    QString status_text_;  // empty-chart message override ("" = waiting)
    QPixmap cache_;
    QMutex mutex_;
    bool cache_dirty_ = true;
    QTimer* repaint_timer_ = nullptr;
};

} // namespace fincept::screens::crypto
