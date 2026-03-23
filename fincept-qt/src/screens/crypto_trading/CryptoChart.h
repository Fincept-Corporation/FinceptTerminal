#pragma once
// Crypto Chart — Candlestick chart with timeframe toggle buttons

#include "trading/TradingTypes.h"

#include <QPushButton>
#include <QVector>
#include <QWidget>

class QChartView;
class QChart;
class QCandlestickSeries;
class QDateTimeAxis;
class QValueAxis;

namespace fincept::screens::crypto {

class CryptoChart : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoChart(QWidget* parent = nullptr);

    void set_candles(const QVector<trading::Candle>& candles);
    void append_candle(const trading::Candle& candle);
    void clear();

    QString current_timeframe() const;

  signals:
    void timeframe_changed(const QString& tf);

  private:
    void rebuild_chart();
    void update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time);
    void set_active_tf(int idx);

    QChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QCandlestickSeries* series_ = nullptr;
    QDateTimeAxis* time_axis_ = nullptr;
    QValueAxis* price_axis_ = nullptr;

    // Timeframe toggle buttons
    QPushButton* tf_buttons_[6] = {};
    int active_tf_ = 3; // default "1h"
    static constexpr const char* TF_LABELS[] = {"1m", "5m", "15m", "1h", "4h", "1d"};

    QVector<trading::Candle> candles_;
    static constexpr int MAX_VISIBLE = 120;

    // Axis range cache
    double last_min_price_ = -1;
    double last_max_price_ = -1;
    qint64 last_min_time_  = -1;
    qint64 last_max_time_  = -1;
};

} // namespace fincept::screens::crypto
