#pragma once
// Depth Chart — custom-painted bid/ask cumulative area chart

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

    void set_data(const QVector<QPair<double, double>>& bids,
                  const QVector<QPair<double, double>>& asks,
                  double spread, double spread_pct);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void rebuild_cache();

    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    double spread_ = 0;
    QPixmap cache_;
    QMutex mutex_;
    bool cache_dirty_ = true;
    QTimer* repaint_timer_ = nullptr;
};

} // namespace fincept::screens::crypto
