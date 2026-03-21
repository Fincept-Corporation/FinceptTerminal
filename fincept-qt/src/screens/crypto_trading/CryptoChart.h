#pragma once
// Crypto Chart — Candlestick chart using QChart

#include "trading/TradingTypes.h"
#include <QWidget>
#include <QVector>
#include <QComboBox>

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

    QChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QCandlestickSeries* series_ = nullptr;
    QDateTimeAxis* time_axis_ = nullptr;
    QValueAxis* price_axis_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;

    QVector<trading::Candle> candles_;
    static constexpr int MAX_VISIBLE = 120;
};

} // namespace fincept::screens::crypto
