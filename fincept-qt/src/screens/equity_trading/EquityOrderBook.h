#pragma once
// Equity Order Book / Market Depth — custom-painted dual-column with depth bars

#include "screens/equity_trading/EquityTypes.h"

#include <QLabel>
#include <QMutex>
#include <QPair>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens::equity {

class EquityOrderBook : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOrderBook(QWidget* parent = nullptr);

    void set_data(const QVector<QPair<double, double>>& bids,
                  const QVector<QPair<double, double>>& asks,
                  double spread, double spread_pct);

  signals:
    void price_clicked(double price);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

  private:
    void rebuild_cache();

    QLabel* spread_label_ = nullptr;
    QWidget* canvas_ = nullptr;

    QVector<QPair<double, double>> bids_;
    QVector<QPair<double, double>> asks_;
    double spread_ = 0;
    double spread_pct_ = 0;

    QPixmap cache_;
    bool cache_dirty_ = true;
    QMutex mutex_;
    QTimer* repaint_timer_ = nullptr;

    static constexpr int ROW_H = 20;
    static constexpr int HEADER_H = 30;
    static constexpr int SPREAD_H = 22;
    static constexpr int MAX_LEVELS = 10;
};

} // namespace fincept::screens::equity
