// src/algo_engine/IndicatorEngine.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::algo {

class IndicatorEngine {
public:
    static IndicatorResult compute(const QString& indicator_name,
                                   const QVector<OhlcvCandle>& candles,
                                   const QJsonObject& params,
                                   const QString& field);

private:
    static void extract_arrays(const QVector<OhlcvCandle>& candles,
                               QVector<double>& open, QVector<double>& high,
                               QVector<double>& low, QVector<double>& close,
                               QVector<double>& volume);

    // Moving averages
    static IndicatorResult compute_sma(const QVector<double>& src, int period);
    static IndicatorResult compute_ema(const QVector<double>& src, int period);
    static IndicatorResult compute_wma(const QVector<double>& src, int period);
    static IndicatorResult compute_dema(const QVector<double>& src, int period);
    static IndicatorResult compute_tema(const QVector<double>& src, int period);

    // Momentum
    static IndicatorResult compute_rsi(const QVector<double>& close, int period);
    static IndicatorResult compute_macd(const QVector<double>& close,
                                        int fast, int slow, int signal_period);
    static IndicatorResult compute_stochastic(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close,
                                              int k_period, int d_period);
    static IndicatorResult compute_cci(const QVector<double>& high,
                                       const QVector<double>& low,
                                       const QVector<double>& close, int period);
    static IndicatorResult compute_williams_r(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close, int period);
    static IndicatorResult compute_mfi(const QVector<double>& high,
                                       const QVector<double>& low,
                                       const QVector<double>& close,
                                       const QVector<double>& volume, int period);
    static IndicatorResult compute_roc(const QVector<double>& close, int period);

    // Trend
    static IndicatorResult compute_adx(const QVector<double>& high,
                                       const QVector<double>& low,
                                       const QVector<double>& close, int period);
    static IndicatorResult compute_supertrend(const QVector<double>& high,
                                              const QVector<double>& low,
                                              const QVector<double>& close,
                                              int period, double multiplier);
    static IndicatorResult compute_aroon(const QVector<double>& high,
                                         const QVector<double>& low, int period);
    static IndicatorResult compute_ichimoku(const QVector<double>& high,
                                            const QVector<double>& low,
                                            int tenkan, int kijun, int senkou);

    // Volatility
    static IndicatorResult compute_atr(const QVector<double>& high,
                                       const QVector<double>& low,
                                       const QVector<double>& close, int period);
    static IndicatorResult compute_bollinger(const QVector<double>& close,
                                             int period, double std_dev);
    static IndicatorResult compute_keltner(const QVector<double>& high,
                                           const QVector<double>& low,
                                           const QVector<double>& close,
                                           int period, double multiplier);
    static IndicatorResult compute_donchian(const QVector<double>& high,
                                            const QVector<double>& low, int period);

    // Volume
    static IndicatorResult compute_obv(const QVector<double>& close,
                                       const QVector<double>& volume);
    static IndicatorResult compute_cmf(const QVector<double>& high,
                                       const QVector<double>& low,
                                       const QVector<double>& close,
                                       const QVector<double>& volume, int period);
    static IndicatorResult compute_vol_win_chg(const QVector<double>& volume, int window);

    // Stock attribute pseudo-indicators
    static IndicatorResult compute_stock_attr(const QVector<OhlcvCandle>& candles,
                                              const QString& attr);

    // Helpers
    static QVector<double> ema_series(const QVector<double>& src, int period);
    static QVector<double> sma_series(const QVector<double>& src, int period);
    static QVector<double> true_range_series(const QVector<double>& high,
                                             const QVector<double>& low,
                                             const QVector<double>& close);
};

} // namespace fincept::algo
