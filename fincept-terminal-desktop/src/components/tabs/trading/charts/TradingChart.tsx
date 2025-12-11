/**
 * TradingChart - TradingView-style candlestick chart
 * Uses lightweight-charts library for high-performance rendering
 */

import React, { useEffect, useRef, useState } from 'react';
import { createChart, ColorType, CandlestickSeries, HistogramSeries } from 'lightweight-charts';
import type { IChartApi, CandlestickData, Time } from 'lightweight-charts';
import { useOHLCV } from '../hooks/useMarketData';
import type { Timeframe, OHLCV } from '../types';

interface TradingChartProps {
  symbol: string;
  timeframe?: Timeframe;
  height?: number;
  showVolume?: boolean;
}

export function TradingChart({
  symbol,
  timeframe = '1h',
  height = 500,
  showVolume = true
}: TradingChartProps) {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candlestickSeriesRef = useRef<any>(null);
  const volumeSeriesRef = useRef<any>(null);

  const { ohlcv, isLoading, error } = useOHLCV(symbol, timeframe, 200);
  const [selectedTimeframe, setSelectedTimeframe] = useState<Timeframe>(timeframe);

  // Initialize chart
  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      width: chartContainerRef.current.clientWidth,
      height,
      layout: {
        background: { type: ColorType.Solid, color: '#0a0a0a' },
        textColor: '#d1d5db',
      },
      grid: {
        vertLines: { color: '#1f1f1f' },
        horzLines: { color: '#1f1f1f' },
      },
      crosshair: {
        mode: 1,
      },
      rightPriceScale: {
        borderColor: '#2a2a2a',
      },
      timeScale: {
        borderColor: '#2a2a2a',
        timeVisible: true,
        secondsVisible: false,
      },
    });

    chartRef.current = chart;

    // Create candlestick series - v5 API: chart.addSeries(SeriesDefinition, options)
    const candlestickSeries = chart.addSeries(CandlestickSeries, {
      upColor: '#10b981',
      downColor: '#ef4444',
      borderUpColor: '#10b981',
      borderDownColor: '#ef4444',
      wickUpColor: '#10b981',
      wickDownColor: '#ef4444',
    });

    candlestickSeriesRef.current = candlestickSeries;

    // Create volume series
    if (showVolume) {
      const volumeSeries = chart.addSeries(HistogramSeries, {
        color: '#6b7280',
        priceFormat: {
          type: 'volume',
        },
        priceScaleId: '',
      });

      volumeSeries.priceScale().applyOptions({
        scaleMargins: {
          top: 0.8,
          bottom: 0,
        },
      });

      volumeSeriesRef.current = volumeSeries;
    }

    // Handle resize
    const handleResize = () => {
      if (chartContainerRef.current && chartRef.current) {
        chartRef.current.resize(
          chartContainerRef.current.clientWidth,
          height,
          true
        );
      }
    };

    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, [height, showVolume]);

  // Handle height changes
  useEffect(() => {
    if (chartRef.current && chartContainerRef.current) {
      chartRef.current.resize(
        chartContainerRef.current.clientWidth,
        height,
        true
      );
    }
  }, [height]);

  // Update chart data
  useEffect(() => {
    if (!ohlcv.length || !candlestickSeriesRef.current) return;

    // Convert OHLCV to candlestick data
    const candleData: CandlestickData[] = ohlcv.map((candle: OHLCV) => ({
      time: Math.floor(candle.timestamp / 1000) as any,
      open: candle.open,
      high: candle.high,
      low: candle.low,
      close: candle.close,
    }));

    candlestickSeriesRef.current.setData(candleData);

    // Update volume data
    if (showVolume && volumeSeriesRef.current) {
      const volumeData = ohlcv.map((candle: OHLCV) => ({
        time: Math.floor(candle.timestamp / 1000) as any,
        value: candle.volume,
        color: candle.close >= candle.open ? '#10b98144' : '#ef444444',
      }));

      volumeSeriesRef.current.setData(volumeData);
    }

    // Fit content
    chartRef.current?.timeScale().fitContent();
  }, [ohlcv, showVolume]);

  const timeframes: Timeframe[] = ['1m', '5m', '15m', '1h', '4h', '1d'];

  return (
    <div className="flex flex-col gap-2">
      {/* Timeframe selector */}
      <div className="flex items-center gap-2 px-4">
        <span className="text-xs text-gray-400">TIMEFRAME:</span>
        <div className="flex gap-1">
          {timeframes.map((tf) => (
            <button
              key={tf}
              onClick={() => setSelectedTimeframe(tf)}
              className={`px-2 py-1 text-xs font-mono rounded transition-colors ${
                selectedTimeframe === tf
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
              }`}
            >
              {tf}
            </button>
          ))}
        </div>
      </div>

      {/* Chart container */}
      <div className="relative">
        {isLoading && (
          <div className="absolute inset-0 flex items-center justify-center bg-black/50 z-10">
            <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
          </div>
        )}

        {error && (
          <div className="absolute inset-0 flex items-center justify-center bg-black/50 z-10">
            <div className="text-red-500 text-sm">
              Failed to load chart data: {error.message}
            </div>
          </div>
        )}

        <div ref={chartContainerRef} className="w-full" />
      </div>
    </div>
  );
}
