/**
 * Stock Trading Chart
 *
 * Chart component for Indian stock brokers using ProChartWithToolkit
 * Fetches historical OHLCV data from stock broker adapters (Fyers, Zerodha, etc.)
 */

import React, { useEffect, useState, useCallback } from 'react';
import { Loader2, AlertCircle, RefreshCw, Clock } from 'lucide-react';

import type { StockExchange, TimeFrame, OHLCV } from '@/brokers/stocks/types';
import { useStockBrokerContext } from '@/contexts/StockBrokerContext';
import { ProChartWithToolkit } from '../../trading/charts/ProChartWithToolkit';

interface LiveQuote {
  lastPrice: number;
  change: number;
  changePercent: number;
  previousClose?: number;
  open?: number;
  high?: number;
  low?: number;
  volume?: number;
}

interface StockTradingChartProps {
  symbol: string;
  exchange: StockExchange;
  interval?: TimeFrame;
  height?: number;
  timezone?: string; // e.g., 'Asia/Kolkata' for IST
  liveQuote?: LiveQuote; // Live quote data from parent for consistent display
}

// Available timeframes
const TIMEFRAMES: { value: TimeFrame; label: string }[] = [
  { value: '1m', label: '1m' },
  { value: '3m', label: '3m' },
  { value: '5m', label: '5m' },
  { value: '15m', label: '15m' },
  { value: '30m', label: '30m' },
  { value: '1h', label: '1h' },
  { value: '2h', label: '2h' },
  { value: '4h', label: '4h' },
  { value: '1d', label: '1D' },
  { value: '1w', label: '1W' },
];

// Helper to convert TimeFrame to days for historical data range
const timeframeToDays = (tf: TimeFrame): number => {
  switch (tf) {
    case '1m': return 7;      // 1 min -> 7 days
    case '3m': return 7;      // 3 min -> 7 days
    case '5m': return 14;     // 5 min -> 14 days
    case '10m': return 14;    // 10 min -> 14 days
    case '15m': return 30;    // 15 min -> 30 days
    case '30m': return 30;    // 30 min -> 30 days
    case '1h': return 60;     // 1 hour -> 60 days
    case '2h': return 90;     // 2 hours -> 90 days (3 months)
    case '4h': return 90;     // 4 hours -> 90 days (3 months)
    case '1d': return 90;     // 1 day -> 90 days (3 months) - default for paper trading
    case '1w': return 365;    // 1 week -> 1 year
    case '1M': return 1825;   // 1 month -> 5 years
    default: return 90;       // Default to 3 months
  }
};

// Map exchange to timezone
const EXCHANGE_TIMEZONES: Record<string, string> = {
  // Indian exchanges - IST (UTC+5:30)
  NSE: 'Asia/Kolkata',
  BSE: 'Asia/Kolkata',
  MCX: 'Asia/Kolkata',
  NFO: 'Asia/Kolkata',
  BFO: 'Asia/Kolkata',
  CDS: 'Asia/Kolkata',
  // US exchanges - Eastern Time
  NYSE: 'America/New_York',
  NASDAQ: 'America/New_York',
  AMEX: 'America/New_York',
  // European exchanges
  LSE: 'Europe/London',
  XETRA: 'Europe/Berlin',
  EURONEXT: 'Europe/Paris',
  // Default
  DEFAULT: 'UTC',
};

export function StockTradingChart({
  symbol,
  exchange,
  interval: initialInterval = '1d',
  height,
  timezone: propTimezone,
  liveQuote
}: StockTradingChartProps) {
  // Auto-detect timezone based on exchange if not provided
  const timezone = propTimezone || EXCHANGE_TIMEZONES[exchange] || EXCHANGE_TIMEZONES.DEFAULT;
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedInterval, setSelectedInterval] = useState<TimeFrame>(initialInterval);
  const [chartData, setChartData] = useState<Array<{
    time: number;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
  }>>([]);

  const { adapter, isAuthenticated } = useStockBrokerContext();

  // Sync selectedInterval when initialInterval prop changes (e.g., broker switch)
  useEffect(() => {
    setSelectedInterval(initialInterval);
  }, [initialInterval]);

  // Fetch historical data
  const fetchHistoricalData = useCallback(async () => {
    if (!symbol || !exchange) {
      console.log('[StockTradingChart] No symbol or exchange selected');
      setError('Please select a symbol');
      setIsLoading(false);
      return;
    }

    if (!adapter || !isAuthenticated) {
      setError('Please authenticate with your broker to view charts');
      setIsLoading(false);
      return;
    }

    try {
      setIsLoading(true);
      setError(null);

      console.log(`[StockTradingChart] Fetching historical data for ${exchange}:${symbol} (${selectedInterval})...`);

      // Calculate date range based on timeframe
      const to = new Date();
      const from = new Date();
      from.setDate(from.getDate() - timeframeToDays(selectedInterval));

      console.log(`[StockTradingChart] Date range: ${from.toISOString()} to ${to.toISOString()}`);

      // Fetch OHLCV data from stock broker adapter (Fyers, Zerodha, etc.)
      const ohlcvData: OHLCV[] = await adapter.getOHLCV(
        symbol,
        exchange,
        selectedInterval,
        from,
        to
      );

      console.log(`[StockTradingChart] Fetched ${ohlcvData.length} candles from ${adapter.brokerId}`);

      if (ohlcvData.length === 0) {
        setError('No historical data available. Check your API credentials and symbol format.');
        setIsLoading(false);
        return;
      }

      // Convert to ProChartWithToolkit format (timestamps in seconds, not milliseconds)
      const formattedData = ohlcvData.map((candle) => ({
        time: Math.floor(candle.timestamp / 1000), // Convert ms to seconds
        open: candle.open,
        high: candle.high,
        low: candle.low,
        close: candle.close,
        volume: candle.volume,
      }));

      // Sort by time and remove duplicates (Lightweight Charts requires strictly ascending timestamps)
      const sortedData = formattedData.sort((a, b) => a.time - b.time);

      // Remove duplicate timestamps (keep the last one for each timestamp)
      const uniqueData = sortedData.reduce((acc, current) => {
        const lastItem = acc[acc.length - 1];
        if (!lastItem || lastItem.time !== current.time) {
          acc.push(current);
        } else {
          // Replace with current (keep last occurrence)
          acc[acc.length - 1] = current;
        }
        return acc;
      }, [] as typeof sortedData);

      console.log('[StockTradingChart] ✓ Chart data prepared:', {
        originalCount: ohlcvData.length,
        formattedCount: formattedData.length,
        uniqueCount: uniqueData.length,
        duplicatesRemoved: formattedData.length - uniqueData.length,
        firstCandle: uniqueData[0],
        lastCandle: uniqueData[uniqueData.length - 1],
      });

      setChartData(uniqueData);
      setIsLoading(false);
    } catch (err) {
      console.error('[StockTradingChart] Failed to fetch historical data:', err);
      setError(err instanceof Error ? err.message : 'Failed to load chart data');
      setIsLoading(false);
    }
  }, [adapter, isAuthenticated, symbol, exchange, selectedInterval]);

  // Auto-fetch on mount and when dependencies change
  useEffect(() => {
    fetchHistoricalData();
  }, [fetchHistoricalData]);

  // Handle manual refresh
  const handleRefresh = () => {
    fetchHistoricalData();
  };

  // Not authenticated state
  if (!isAuthenticated) {
    return (
      <div className="flex-1 flex items-center justify-center bg-[#0A0A0A]" style={{ height: height || '100%', minHeight: height || 300 }}>
        <div className="text-center">
          <AlertCircle className="w-16 h-16 mx-auto mb-4 text-[#FF8800]" />
          <p className="text-lg text-white mb-2">Connect Your Broker</p>
          <p className="text-sm text-gray-500">Authenticate to view historical charts</p>
        </div>
      </div>
    );
  }

  // Loading state
  if (isLoading && chartData.length === 0) {
    return (
      <div className="flex-1 flex items-center justify-center bg-[#0A0A0A]" style={{ height: height || '100%', minHeight: height || 300 }}>
        <div className="text-center">
          <Loader2 className="w-12 h-12 mx-auto mb-4 text-[#FF8800] animate-spin" />
          <p className="text-sm text-gray-400">Loading chart data...</p>
        </div>
      </div>
    );
  }

  // Error state
  if (error && chartData.length === 0) {
    return (
      <div className="flex-1 flex items-center justify-center bg-[#0A0A0A]" style={{ height: height || '100%', minHeight: height || 300 }}>
        <div className="text-center">
          <AlertCircle className="w-12 h-12 mx-auto mb-4 text-red-500" />
          <p className="text-sm text-red-400 mb-3">{error}</p>
          <button
            onClick={handleRefresh}
            className="px-4 py-2 bg-[#FF8800] text-white rounded hover:bg-[#FF9920] transition-colors"
          >
            Retry
          </button>
        </div>
      </div>
    );
  }

  // Calculate price change for display - prefer live quote data if available
  const latestCandle = chartData[chartData.length - 1];

  // Use live quote for price change (compares to previous close), fallback to chart data
  const displayPrice = liveQuote?.lastPrice ?? latestCandle?.close ?? 0;
  const priceChange = liveQuote?.change ?? (latestCandle && liveQuote?.previousClose
    ? latestCandle.close - liveQuote.previousClose
    : 0);
  const priceChangePercent = liveQuote?.changePercent ?? (liveQuote?.previousClose && liveQuote.previousClose > 0
    ? ((priceChange / liveQuote.previousClose) * 100)
    : 0);

  return (
    <div className="flex-1 flex flex-col bg-[#0A0A0A]" style={{ height: height || '100%', minHeight: 0, overflow: 'hidden' }}>
      {/* Combined Header - Symbol, OHLC, Timeframe, and Controls */}
      <div className="flex items-center justify-between px-3 py-1 bg-[#000000] border-b border-[#2A2A2A]" style={{ flexShrink: 0, height: '28px' }}>
        {/* Left: Symbol, OHLC, Change */}
        <div className="flex items-center gap-4">
          {/* Symbol */}
          <span className="text-xs font-semibold text-[#FF8800] font-mono tracking-wide">
            {exchange}:{symbol}
          </span>

          {/* OHLC Data - Use live quote if available, fallback to chart candle */}
          {(liveQuote || latestCandle) && (
            <div className="flex items-center gap-3 text-[10px] font-mono">
              <div className="flex items-center gap-1">
                <span className="text-gray-500">O</span>
                <span className="text-white">{(liveQuote?.open ?? latestCandle?.open ?? 0).toFixed(2)}</span>
              </div>
              <div className="flex items-center gap-1">
                <span className="text-gray-500">H</span>
                <span className="text-white">{(liveQuote?.high ?? latestCandle?.high ?? 0).toFixed(2)}</span>
              </div>
              <div className="flex items-center gap-1">
                <span className="text-gray-500">L</span>
                <span className="text-white">{(liveQuote?.low ?? latestCandle?.low ?? 0).toFixed(2)}</span>
              </div>
              <div className="flex items-center gap-1">
                <span className="text-gray-500">C</span>
                <span className={`font-semibold ${priceChange >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}`}>
                  {displayPrice.toFixed(2)}
                </span>
              </div>

              {/* Price Change & Percentage - from live quote */}
              <div className={`flex items-center gap-1 ${priceChange >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}`}>
                <span>{priceChange >= 0 ? '+' : ''}{priceChange.toFixed(2)}</span>
                <span>({priceChangePercent >= 0 ? '+' : ''}{priceChangePercent.toFixed(2)}%)</span>
              </div>

              {/* Volume - prefer live quote */}
              <div className="flex items-center gap-1">
                <span className="text-gray-500">Vol</span>
                <span className="text-white">
                  {(() => {
                    const vol = liveQuote?.volume ?? latestCandle?.volume ?? 0;
                    return vol >= 1000000
                      ? `${(vol / 1000000).toFixed(2)}M`
                      : vol >= 1000
                      ? `${(vol / 1000).toFixed(2)}K`
                      : vol.toFixed(0);
                  })()}
                </span>
              </div>
            </div>
          )}
        </div>

        {/* Right: Timeframe Selector & Refresh */}
        <div className="flex items-center gap-3">
          {/* Timeframe Buttons */}
          <div className="flex items-center gap-1">
            {TIMEFRAMES.map((tf) => (
              <button
                key={tf.value}
                onClick={() => setSelectedInterval(tf.value)}
                disabled={isLoading}
                className={`px-2 py-0.5 text-[9px] font-medium rounded transition-colors disabled:opacity-50 ${
                  selectedInterval === tf.value
                    ? 'bg-[#FF8800] text-white'
                    : 'text-gray-400 hover:text-white hover:bg-[#1F1F1F]'
                }`}
              >
                {tf.label}
              </button>
            ))}
          </div>

          {/* Refresh Button */}
          <button
            onClick={handleRefresh}
            disabled={isLoading}
            className="p-1 bg-[#0F0F0F] rounded border border-[#2A2A2A] hover:bg-[#1F1F1F] transition-colors disabled:opacity-50"
            title="Refresh chart"
          >
            <RefreshCw className={`w-3 h-3 text-gray-400 ${isLoading ? 'animate-spin' : ''}`} />
          </button>
        </div>
      </div>

      {/* Chart Container - Full remaining height */}
      <div className="flex-1 relative" style={{ minHeight: 0, overflow: 'hidden' }}>
        {chartData.length > 0 && (
          <div style={{
            position: 'absolute',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            display: 'flex',
            flexDirection: 'column'
          }}>
            <ProChartWithToolkit
              data={chartData}
              symbol={`${exchange}:${symbol}`}
              showVolume={true}
              showToolbar={true}
              showHeader={false}
              timezone={timezone}
            />
          </div>
        )}

        {/* Loading Overlay */}
        {isLoading && chartData.length > 0 && (
          <div className="absolute bottom-2 left-1/2 -translate-x-1/2 bg-[#0F0F0F]/90 backdrop-blur-sm px-4 py-2 rounded border border-[#2A2A2A] z-10">
            <div className="flex items-center gap-2">
              <Loader2 className="w-4 h-4 text-[#FF8800] animate-spin" />
              <span className="text-xs text-gray-400">Updating...</span>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
