// Simple professional candlestick chart with Recharts

import React from 'react';
import {
  ComposedChart,
  Line,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
  Area,
} from 'recharts';
import { CandleData, IndicatorData, MomentumIndicatorResult, IndicatorType } from '../types';

interface SimpleCandlestickChartProps {
  data: CandleData[];
  indicators: MomentumIndicatorResult;
  activeIndicators: Set<IndicatorType>;
  height?: number;
}

const CHART_COLORS = {
  upColor: '#10b981',
  downColor: '#ef4444',
  rsi: '#FFA500',
  macd: '#2962FF',
  macdSignal: '#FF6347',
  macdHistogram: '#787878',
  stochasticK: '#00CED1',
  stochasticD: '#FF69B4',
  cci: '#FFD700',
  roc: '#9370DB',
  williamsR: '#00FA9A',
  ao: '#FF1493',
  tsi: '#4169E1',
  tsiSignal: '#FF8C00',
  uo: '#32CD32',
};

export const SimpleCandlestickChart: React.FC<SimpleCandlestickChartProps> = ({
  data,
  indicators,
  activeIndicators,
  height = 700,
}) => {
  // Merge price data with indicators
  const chartData = data.map((candle, index) => {
    const dataPoint: any = {
      time: new Date(candle.time * 1000).toLocaleDateString(),
      open: candle.open,
      high: candle.high,
      low: candle.low,
      close: candle.close,
      volume: candle.volume,
      wickHigh: candle.high,
      wickLow: candle.low,
      bodyHigh: Math.max(candle.open, candle.close),
      bodyLow: Math.min(candle.open, candle.close),
      color: candle.close >= candle.open ? CHART_COLORS.upColor : CHART_COLORS.downColor,
    };

    // Add indicator values
    if (activeIndicators.has('rsi') && indicators.rsi) {
      const rsiValue = indicators.rsi.values[index]?.value;
      if (rsiValue !== null && rsiValue !== undefined) {
        dataPoint.rsi = rsiValue;
      }
    }

    if (activeIndicators.has('macd') && indicators.macd) {
      const macdValue = indicators.macd.macd_line[index]?.value;
      const signalValue = indicators.macd.signal_line[index]?.value;
      const histValue = indicators.macd.histogram[index]?.value;
      if (macdValue !== null) dataPoint.macd = macdValue;
      if (signalValue !== null) dataPoint.macdSignal = signalValue;
      if (histValue !== null) dataPoint.macdHistogram = histValue;
    }

    if (activeIndicators.has('stochastic') && indicators.stochastic) {
      const kValue = indicators.stochastic.k_values[index]?.value;
      const dValue = indicators.stochastic.d_values[index]?.value;
      if (kValue !== null) dataPoint.stochK = kValue;
      if (dValue !== null) dataPoint.stochD = dValue;
    }

    if (activeIndicators.has('cci') && indicators.cci) {
      const cciValue = indicators.cci.values[index]?.value;
      if (cciValue !== null) dataPoint.cci = cciValue;
    }

    if (activeIndicators.has('roc') && indicators.roc) {
      const rocValue = indicators.roc.values[index]?.value;
      if (rocValue !== null) dataPoint.roc = rocValue;
    }

    if (activeIndicators.has('williams_r') && indicators.williams_r) {
      const wrValue = indicators.williams_r.values[index]?.value;
      if (wrValue !== null) dataPoint.williamsR = wrValue;
    }

    if (activeIndicators.has('awesome_oscillator') && indicators.awesome_oscillator) {
      const aoValue = indicators.awesome_oscillator.values[index]?.value;
      if (aoValue !== null) dataPoint.ao = aoValue;
    }

    if (activeIndicators.has('tsi') && indicators.tsi) {
      const tsiValue = indicators.tsi.values[index]?.value;
      const tsiSignalValue = indicators.tsi.signal_line[index]?.value;
      if (tsiValue !== null) dataPoint.tsi = tsiValue;
      if (tsiSignalValue !== null) dataPoint.tsiSignal = tsiSignalValue;
    }

    if (activeIndicators.has('ultimate_oscillator') && indicators.ultimate_oscillator) {
      const uoValue = indicators.ultimate_oscillator.values[index]?.value;
      if (uoValue !== null) dataPoint.uo = uoValue;
    }

    return dataPoint;
  });

  const mainChartHeight = Math.floor(height * 0.65);
  const indicatorChartHeight = Math.floor(height * 0.35);

  return (
    <div className="space-y-4">
      {/* Main Price Chart */}
      <ResponsiveContainer width="100%" height={mainChartHeight}>
        <ComposedChart
          data={chartData}
          margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke="#1a1a1a" />
          <XAxis
            dataKey="time"
            stroke="#666"
            tick={{ fill: '#999' }}
            tickLine={{ stroke: '#333' }}
          />
          <YAxis
            stroke="#666"
            tick={{ fill: '#999' }}
            tickLine={{ stroke: '#333' }}
            domain={['auto', 'auto']}
          />
          <Tooltip
            contentStyle={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #333',
              borderRadius: '8px',
              color: '#fff',
            }}
          />
          <Legend wrapperStyle={{ color: '#999' }} />

          {/* Candlestick bodies */}
          <Bar
            dataKey={(entry) => [entry.bodyLow, entry.bodyHigh]}
            fill={CHART_COLORS.upColor}
            stroke="none"
            barSize={8}
            name="Price"
          />

          {/* Close price line for easier trending */}
          <Line
            type="monotone"
            dataKey="close"
            stroke="#FFA500"
            strokeWidth={1.5}
            dot={false}
            name="Close"
          />
        </ComposedChart>
      </ResponsiveContainer>

      {/* Indicator Chart */}
      {activeIndicators.size > 0 && (
        <ResponsiveContainer width="100%" height={indicatorChartHeight}>
          <ComposedChart
            data={chartData}
            margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
          >
            <CartesianGrid strokeDasharray="3 3" stroke="#1a1a1a" />
            <XAxis
              dataKey="time"
              stroke="#666"
              tick={{ fill: '#999' }}
              tickLine={{ stroke: '#333' }}
            />
            <YAxis
              stroke="#666"
              tick={{ fill: '#999' }}
              tickLine={{ stroke: '#333' }}
            />
            <Tooltip
              contentStyle={{
                backgroundColor: '#1a1a1a',
                border: '1px solid #333',
                borderRadius: '8px',
                color: '#fff',
              }}
            />
            <Legend wrapperStyle={{ color: '#999' }} />

            {activeIndicators.has('rsi') && (
              <Line
                type="monotone"
                dataKey="rsi"
                stroke={CHART_COLORS.rsi}
                strokeWidth={2}
                dot={false}
                name="RSI"
              />
            )}

            {activeIndicators.has('macd') && (
              <>
                <Line
                  type="monotone"
                  dataKey="macd"
                  stroke={CHART_COLORS.macd}
                  strokeWidth={2}
                  dot={false}
                  name="MACD"
                />
                <Line
                  type="monotone"
                  dataKey="macdSignal"
                  stroke={CHART_COLORS.macdSignal}
                  strokeWidth={2}
                  dot={false}
                  name="Signal"
                />
                <Bar
                  dataKey="macdHistogram"
                  fill={CHART_COLORS.macdHistogram}
                  opacity={0.6}
                  name="Histogram"
                />
              </>
            )}

            {activeIndicators.has('stochastic') && (
              <>
                <Line
                  type="monotone"
                  dataKey="stochK"
                  stroke={CHART_COLORS.stochasticK}
                  strokeWidth={2}
                  dot={false}
                  name="%K"
                />
                <Line
                  type="monotone"
                  dataKey="stochD"
                  stroke={CHART_COLORS.stochasticD}
                  strokeWidth={2}
                  dot={false}
                  name="%D"
                />
              </>
            )}

            {activeIndicators.has('cci') && (
              <Line
                type="monotone"
                dataKey="cci"
                stroke={CHART_COLORS.cci}
                strokeWidth={2}
                dot={false}
                name="CCI"
              />
            )}

            {activeIndicators.has('roc') && (
              <Line
                type="monotone"
                dataKey="roc"
                stroke={CHART_COLORS.roc}
                strokeWidth={2}
                dot={false}
                name="ROC"
              />
            )}

            {activeIndicators.has('williams_r') && (
              <Line
                type="monotone"
                dataKey="williamsR"
                stroke={CHART_COLORS.williamsR}
                strokeWidth={2}
                dot={false}
                name="Williams %R"
              />
            )}

            {activeIndicators.has('awesome_oscillator') && (
              <Bar
                dataKey="ao"
                fill={CHART_COLORS.ao}
                opacity={0.7}
                name="AO"
              />
            )}

            {activeIndicators.has('tsi') && (
              <>
                <Line
                  type="monotone"
                  dataKey="tsi"
                  stroke={CHART_COLORS.tsi}
                  strokeWidth={2}
                  dot={false}
                  name="TSI"
                />
                <Line
                  type="monotone"
                  dataKey="tsiSignal"
                  stroke={CHART_COLORS.tsiSignal}
                  strokeWidth={2}
                  dot={false}
                  name="TSI Signal"
                />
              </>
            )}

            {activeIndicators.has('ultimate_oscillator') && (
              <Line
                type="monotone"
                dataKey="uo"
                stroke={CHART_COLORS.uo}
                strokeWidth={2}
                dot={false}
                name="UO"
              />
            )}
          </ComposedChart>
        </ResponsiveContainer>
      )}
    </div>
  );
};
