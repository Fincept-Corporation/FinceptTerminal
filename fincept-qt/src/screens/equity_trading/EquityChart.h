#pragma once
// Equity Chart — Custom-painted candlestick chart with timeframe toggle buttons
// Uses QPainter directly — no QCandlestickSeries (which has QDateTimeAxis gaps).

#include "trading/TradingTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QWidget>

namespace fincept::screens::equity {

// ── Canvas: draws candles via QPainter ───────────────────────────────────────
class CandleCanvas : public QWidget {
    Q_OBJECT
  public:
    explicit CandleCanvas(QWidget* parent = nullptr);

    void set_candles(const QVector<trading::BrokerCandle>& candles);
    void clear();

  protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

  private:
    void rebuild_cache();

    QVector<trading::BrokerCandle> candles_;
    QPixmap cache_;
    bool dirty_ = true;

    static constexpr int MAX_VISIBLE = 120;
    static constexpr int PRICE_AXIS_W = 70; // pixels reserved for price labels on right
    static constexpr int TIME_AXIS_H = 20;  // pixels reserved for time labels at bottom
    static constexpr int LABEL_STEP = 20;   // draw a time label every N candles
};

// ── EquityChart: header + canvas ─────────────────────────────────────────────
class EquityChart : public QWidget {
    Q_OBJECT
  public:
    explicit EquityChart(QWidget* parent = nullptr);

    void set_candles(const QVector<trading::BrokerCandle>& candles);
    void clear();

    QString current_timeframe() const;

  signals:
    void timeframe_changed(const QString& tf);

  private:
    void set_active_tf(int idx);

    CandleCanvas* canvas_ = nullptr;
    QPushButton* tf_buttons_[6] = {};
    int active_tf_ = 2; // default "15m"

    static constexpr const char* TF_LABELS[] = {"1m", "5m", "15m", "1h", "1d", "1w"};
};

} // namespace fincept::screens::equity
