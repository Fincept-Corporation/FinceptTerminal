// TradingChart.tsx - TradingView Lightweight Charts with historical + live data
import React, { useEffect, useRef, useState } from 'react';
import { createChart, IChartApi, ISeriesApi, CandlestickData, Time } from 'lightweight-charts';
import { useWebSocket } from '../../../hooks/useWebSocket';
import { isProviderAvailable } from '../../../services/websocket';
import { useBrokerContext } from '../../../contexts/BrokerContext';

interface TradingChartProps {
  symbol: string;
  provider: string;
  interval?: '1m' | '5m' | '15m' | '30m' | '1h' | '4h' | '1d';
}

export function TradingChart({ symbol, provider, interval = '1m' }: TradingChartProps) {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const candlestickSeriesRef = useRef<ISeriesApi<any> | null>(null);
  const volumeSeriesRef = useRef<ISeriesApi<any> | null>(null);

  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [candleData, setCandleData] = useState<CandlestickData[]>([]);

  const { realAdapter } = useBrokerContext();

  // Only subscribe if WebSocket adapter exists for this provider
  const hasWebSocket = isProviderAvailable(provider);

  // Subscribe to candle data
  const { message, error: wsError } = useWebSocket(
    hasWebSocket ? `${provider}.ohlc.${symbol}` : null,
    null,
    { autoSubscribe: hasWebSocket, params: { interval: parseInterval(interval) } }
  );

  // Initialize chart
  useEffect(() => {
    if (!chartContainerRef.current) return;

    console.log('[TradingChart] Initializing Lightweight Charts...');

    // Create chart
    const chart = createChart(chartContainerRef.current, {
      width: chartContainerRef.current.clientWidth,
      height: chartContainerRef.current.clientHeight,
      layout: {
        background: { color: '#0d0d0d' },
        textColor: '#d1d5db',
      },
      grid: {
        vertLines: { color: '#1a1a1a' },
        horzLines: { color: '#1a1a1a' },
      },
      crosshair: {
        mode: 1, // Normal crosshair
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

    // Create candlestick series
    const candlestickSeries = (chart as any).addCandlestickSeries({
      upColor: '#10b981',
      downColor: '#dc2626',
      borderUpColor: '#10b981',
      borderDownColor: '#dc2626',
      wickUpColor: '#10b981',
      wickDownColor: '#dc2626',
    });

    // Create volume series
    const volumeSeries = (chart as any).addHistogramSeries({
      color: '#26a69a',
      priceFormat: {
        type: 'volume',
      },
      priceScaleId: '', // Use separate price scale for volume
    });

    volumeSeries.priceScale().applyOptions({
      scaleMargins: {
        top: 0.8, // Volume takes up bottom 20%
        bottom: 0,
      },
    });

    chartRef.current = chart;
    candlestickSeriesRef.current = candlestickSeries;
    volumeSeriesRef.current = volumeSeries;

    // Handle resize
    const handleResize = () => {
      if (chartContainerRef.current && chartRef.current) {
        chartRef.current.applyOptions({
          width: chartContainerRef.current.clientWidth,
          height: chartContainerRef.current.clientHeight,
        });
      }
    };

    window.addEventListener('resize', handleResize);

    console.log('[TradingChart] Chart initialized');

    return () => {
      window.removeEventListener('resize', handleResize);
      if (chartRef.current) {
        chartRef.current.remove();
        chartRef.current = null;
      }
    };
  }, []);

  // Fetch historical data on mount or when symbol/interval changes
  useEffect(() => {
    const fetchHistoricalData = async () => {
      if (!realAdapter) {
        console.log('[TradingChart] No adapter available, skipping historical fetch');
        return;
      }

      try {
        setIsLoading(true);
        setError(null);

        console.log(`[TradingChart] Fetching historical data for ${symbol} ${interval}...`);

        // Fetch OHLCV data from exchange
        const timeframe = interval; // CCXT uses same format
        const limit = 100; // Last 100 candles

        const ohlcv = await realAdapter.fetchOHLCV(symbol, timeframe, undefined);

        console.log(`[TradingChart] Fetched ${ohlcv.length} historical candles`);

        // Convert to Lightweight Charts format
        const candles: CandlestickData<Time>[] = ohlcv.map((candle) => ({
          time: Math.floor((candle[0] ?? 0) / 1000) as Time, // Convert ms to seconds
          open: candle[1] ?? 0,
          high: candle[2] ?? 0,
          low: candle[3] ?? 0,
          close: candle[4] ?? 0,
        }));

        const volumes = ohlcv.map((candle) => ({
          time: Math.floor((candle[0] ?? 0) / 1000) as Time,
          value: candle[5] ?? 0,
          color: (candle[4] ?? 0) >= (candle[1] ?? 0) ? '#10b98180' : '#dc262680', // Green if close >= open
        }));

        // Update chart
        if (candlestickSeriesRef.current && volumeSeriesRef.current) {
          candlestickSeriesRef.current.setData(candles as any);
          volumeSeriesRef.current.setData(volumes as any);
          setCandleData(candles);

          // Fit content
          if (chartRef.current) {
            chartRef.current.timeScale().fitContent();
          }

          console.log('[TradingChart] Historical data loaded successfully');
        }

        setIsLoading(false);
      } catch (err) {
        console.error('[TradingChart] Failed to fetch historical data:', err);
        setError(err instanceof Error ? err.message : 'Failed to load chart data');
        setIsLoading(false);
      }
    };

    fetchHistoricalData();
  }, [symbol, interval, realAdapter]);

  // Handle live candle updates from WebSocket
  useEffect(() => {
    if (!message || message.type !== 'candle') return;
    if (!candlestickSeriesRef.current || !volumeSeriesRef.current) return;

    const data = message.data;

    // Parse candle data
    const candle = parseCandleData(data);
    if (!candle) return;

    console.log('[TradingChart] Live candle update:', candle);

    try {
      // Update candlestick
      candlestickSeriesRef.current.update(candle);

      // Update volume
      volumeSeriesRef.current.update({
        time: candle.time,
        value: (data.volume || data.v || 0) as number,
        color: candle.close >= candle.open ? '#10b98180' : '#dc262680',
      });

      // Update local state
      setCandleData(prev => {
        const lastCandle = prev[prev.length - 1];
        if (lastCandle && lastCandle.time === candle.time) {
          // Update existing candle
          return [...prev.slice(0, -1), candle];
        } else {
          // Append new candle
          return [...prev, candle];
        }
      });
    } catch (error) {
      console.error('[TradingChart] Failed to update candle:', error);
    }
  }, [message]);

  return (
    <div className="relative w-full h-full bg-[#0d0d0d]">
      {/* Loading State */}
      {isLoading && (
        <div className="absolute inset-0 flex items-center justify-center bg-[#0d0d0d] z-10">
          <div className="text-center">
            <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-orange-500 mx-auto mb-3"></div>
            <div className="text-gray-400 text-sm">Loading chart data...</div>
            <div className="text-gray-600 text-xs mt-2">{symbol} • {interval}</div>
          </div>
        </div>
      )}

      {/* Error State */}
      {error && !isLoading && (
        <div className="absolute inset-0 flex items-center justify-center bg-[#0d0d0d] z-10">
          <div className="text-center">
            <div className="text-red-500 text-sm mb-2">Failed to Load Chart</div>
            <div className="text-gray-500 text-xs">{error}</div>
            <div className="text-gray-600 text-xs mt-1">{symbol} • {interval}</div>
          </div>
        </div>
      )}

      {/* WebSocket Error */}
      {wsError && !isLoading && !error && (
        <div className="absolute top-2 right-2 bg-yellow-900/50 border border-yellow-600 px-3 py-2 rounded text-xs text-yellow-400 z-10">
          ⚠️ Live updates unavailable
        </div>
      )}

      {/* Chart Container */}
      <div ref={chartContainerRef} className="w-full h-full" />

      {/* Info Overlay */}
      <div className="absolute top-2 left-2 text-xs text-gray-500 bg-black/70 px-3 py-2 rounded backdrop-blur-sm">
        <div className="font-bold text-white mb-1">{symbol}</div>
        <div className="flex items-center gap-2">
          <span>{interval}</span>
          <span>•</span>
          <span>{candleData.length} candles</span>
          {hasWebSocket && (
            <>
              <span>•</span>
              <span className="text-green-400">● Live</span>
            </>
          )}
        </div>
      </div>
    </div>
  );
}

// Helper: Parse interval for WebSocket
function parseInterval(interval: string): number {
  const map: Record<string, number> = {
    '1m': 1,
    '5m': 5,
    '15m': 15,
    '30m': 30,
    '1h': 60,
    '4h': 240,
    '1d': 1440,
  };
  return map[interval] || 1;
}

// Helper: Parse candle data from WebSocket message
function parseCandleData(data: any): CandlestickData | null {
  try {
    const time = Math.floor((data.time || data.timestamp || Date.now()) / 1000) as Time;
    const open = parseFloat(data.open || data.o || 0);
    const high = parseFloat(data.high || data.h || 0);
    const low = parseFloat(data.low || data.l || 0);
    const close = parseFloat(data.close || data.c || 0);

    if (open === 0 || high === 0 || low === 0 || close === 0) {
      return null;
    }

    return { time, open, high, low, close };
  } catch (error) {
    console.error('[TradingChart] Failed to parse candle data:', error);
    return null;
  }
}
