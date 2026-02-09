/**
 * Stock Trading Chart
 *
 * Chart component for Indian stock brokers using ProChartWithToolkit
 * Fetches historical OHLCV data from stock broker adapters (Fyers, Zerodha, etc.)
 */

import React, { useEffect, useState, useCallback, useRef } from 'react';
import { Loader2, AlertCircle, RefreshCw, Clock } from 'lucide-react';

import type { StockExchange, TimeFrame, OHLCV } from '@/brokers/stocks/types';
import { useStockBrokerContext } from '@/contexts/StockBrokerContext';
import { ProChartWithToolkit } from '../../trading/charts/ProChartWithToolkit';
import { withTimeout } from '@/services/core/apiUtils';

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
  onIntervalChange?: (interval: TimeFrame) => void; // Callback when user changes timeframe
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
// Optimized for AngelOne API rate limits and chart performance
const timeframeToDays = (tf: TimeFrame): number => {
  switch (tf) {
    case '1m': return 2;      // 1 min -> 2 days (avoid rate limits)
    case '3m': return 3;      // 3 min -> 3 days
    case '5m': return 5;      // 5 min -> 5 days (reasonable for 5m chart)
    case '10m': return 7;     // 10 min -> 7 days
    case '15m': return 10;    // 15 min -> 10 days (reduced from 30 to avoid rate limits)
    case '30m': return 15;    // 30 min -> 15 days (reduced from 30)
    case '1h': return 30;     // 1 hour -> 30 days (reduced from 60)
    case '2h': return 60;     // 2 hours -> 60 days (reduced from 90)
    case '4h': return 90;     // 4 hours -> 90 days (3 months)
    case '1d': return 180;    // 1 day -> 180 days (6 months)
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
  interval: initialInterval = '5m',
  height,
  timezone: propTimezone,
  liveQuote,
  onIntervalChange
}: StockTradingChartProps) {
  // Auto-detect timezone based on exchange if not provided
  const timezone = propTimezone || EXCHANGE_TIMEZONES[exchange] || EXCHANGE_TIMEZONES.DEFAULT;
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedInterval, setSelectedInterval] = useState<TimeFrame>(initialInterval);
  const mountedRef = useRef(true);
  const fetchIdRef = useRef(0);
  const [chartData, setChartData] = useState<Array<{
    time: number;
    open: number;
    high: number;
    low: number;
    close: number;
    volume: number;
  }>>([]);

  const { adapter, isAuthenticated, isLoading: contextLoading, isConnecting, masterContractReady } = useStockBrokerContext();

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Sync selectedInterval when initialInterval prop changes (e.g., broker switch)
  useEffect(() => {
    console.log(`[StockTradingChart] Syncing interval from prop: ${initialInterval}`);
    setSelectedInterval(initialInterval);
  }, [initialInterval]);

  // Fetch historical data with retry for race condition handling
  const fetchHistoricalData = useCallback(async (retryCount = 0) => {
    const INIT_MAX_RETRIES = 5;   // retries for adapter/auth readiness
    const INIT_RETRY_DELAY = 800; // ms
    const FETCH_MAX_RETRIES = 2;  // retries for actual API call failures
    const FETCH_TIMEOUT = 45000;  // 45s per attempt (Rust timeout is 15s, plus overhead)

    if (!symbol || !exchange) {
      console.log('[StockTradingChart] No symbol or exchange selected');
      setError('Please select a symbol');
      setIsLoading(false);
      return;
    }

    if (!adapter || !isAuthenticated) {
      // Don't retry with setTimeout — the stale closure captures the same null adapter.
      // Instead, just return. The useEffect will re-call us when adapter/isAuthenticated change.
      console.log(`[StockTradingChart] DEBUG: Waiting for auth — adapter=${!!adapter}, isAuthenticated=${isAuthenticated}`);
      // Keep isLoading true so the spinner shows while we wait
      return;
    }

    // Check if adapter is actually connected (internal state)
    if (!adapter.isConnected) {
      if (retryCount < INIT_MAX_RETRIES) {
        console.log(`[StockTradingChart] DEBUG: adapter.isConnected=${adapter.isConnected}, brokerId=${adapter.brokerId}, retry ${retryCount + 1}/${INIT_MAX_RETRIES} in ${INIT_RETRY_DELAY}ms...`);
        setTimeout(() => fetchHistoricalData(retryCount + 1), INIT_RETRY_DELAY);
        return;
      }
      console.warn('[StockTradingChart] Adapter still not connected after retries');
      setError('Broker connection not ready. Please try again.');
      setIsLoading(false);
      return;
    }

    // Wait for master contract to be ready before checking instrument
    if (!masterContractReady) {
      if (retryCount < INIT_MAX_RETRIES) {
        console.log(`[StockTradingChart] Master contract not ready, retry ${retryCount + 1}/${INIT_MAX_RETRIES} in ${INIT_RETRY_DELAY}ms...`);
        setTimeout(() => fetchHistoricalData(retryCount + 1), INIT_RETRY_DELAY);
        return;
      }
      console.warn('[StockTradingChart] Master contract still not ready after retries');
      setError('Symbol data loading. Please wait...');
      setIsLoading(false);
      return;
    }

    // Check if master contract/instrument lookup is ready (for brokers that need it)
    if (typeof adapter.getInstrument === 'function') {
      try {
        const instrument = await adapter.getInstrument(symbol, exchange);
        if (!instrument || !instrument.token || instrument.token === '0') {
          if (retryCount < INIT_MAX_RETRIES) {
            console.log(`[StockTradingChart] Instrument token not ready for ${symbol}, retry ${retryCount + 1}/${INIT_MAX_RETRIES} in ${INIT_RETRY_DELAY}ms...`);
            setTimeout(() => fetchHistoricalData(retryCount + 1), INIT_RETRY_DELAY);
            return;
          }
          console.warn('[StockTradingChart] Instrument token still not available after retries');
          setError('Symbol not found. Try searching for a different symbol.');
          setIsLoading(false);
          return;
        }
        console.log(`[StockTradingChart] Instrument token ready: ${symbol} -> ${instrument.token}`);
      } catch (err) {
        console.warn('[StockTradingChart] getInstrument check failed:', err);
      }
    }

    try {
      setIsLoading(true);
      setError(null);

      console.log(`[StockTradingChart] Fetching historical data for ${exchange}:${symbol} (${selectedInterval})...`);

      // Calculate date range based on timeframe
      const to = new Date();
      const from = new Date();
      from.setDate(from.getDate() - timeframeToDays(selectedInterval));

      console.log(`[StockTradingChart] DEBUG: Date range: ${from.toISOString()} to ${to.toISOString()}, days=${timeframeToDays(selectedInterval)}, timeout=${FETCH_TIMEOUT}ms`);
      console.log(`[StockTradingChart] DEBUG: adapter.brokerId=${adapter.brokerId}, adapter.isConnected=${adapter.isConnected}`);

      // Fetch OHLCV data with timeout + retry for transient network failures
      let ohlcvData: OHLCV[] = [];
      let lastFetchError: unknown = null;

      for (let attempt = 0; attempt <= FETCH_MAX_RETRIES; attempt++) {
        const attemptStart = Date.now();
        try {
          if (attempt > 0) {
            console.log(`[StockTradingChart] Retry attempt ${attempt}/${FETCH_MAX_RETRIES}...`);
            await new Promise(r => setTimeout(r, 1000 * attempt));
          }
          console.log(`[StockTradingChart] DEBUG: Starting getOHLCV attempt ${attempt + 1}/${FETCH_MAX_RETRIES + 1} at ${new Date().toISOString()}`);
          ohlcvData = await withTimeout(
            adapter.getOHLCV(symbol, exchange, selectedInterval, from, to),
            FETCH_TIMEOUT
          );
          const elapsed = Date.now() - attemptStart;
          console.log(`[StockTradingChart] DEBUG: getOHLCV succeeded in ${elapsed}ms, got ${ohlcvData.length} candles`);
          lastFetchError = null;
          break; // success
        } catch (err) {
          const elapsed = Date.now() - attemptStart;
          lastFetchError = err;
          const msg = err instanceof Error ? err.message : String(err);
          const stack = err instanceof Error ? err.stack : '';
          console.error(`[StockTradingChart] DEBUG: Fetch attempt ${attempt + 1} failed after ${elapsed}ms:`, {
            message: msg,
            stack,
            errorType: err?.constructor?.name,
            error: err,
          });
          if (attempt === FETCH_MAX_RETRIES) break;
        }
      }

      if (lastFetchError) {
        throw lastFetchError;
      }

      if (!mountedRef.current) return;

      console.log(`[StockTradingChart] Fetched ${ohlcvData.length} candles from ${adapter.brokerId}`);

      if (ohlcvData.length === 0) {
        setError('No historical data available. Try a different timeframe or check your connection.');
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
      const errMsg = err instanceof Error ? err.message : String(err);
      const errStack = err instanceof Error ? err.stack : '';
      console.error('[StockTradingChart] FINAL ERROR - Failed to fetch historical data:', {
        message: errMsg,
        stack: errStack,
        errorType: err?.constructor?.name,
        symbol,
        exchange,
        interval: selectedInterval,
        brokerId: adapter?.brokerId,
        adapterConnected: adapter?.isConnected,
        isAuthenticated,
        error: err,
      });

      // If we have existing data, keep showing it with a warning
      if (chartData.length > 0) {
        console.warn('[StockTradingChart] Keeping existing chart data visible despite error');
        setError('Failed to update chart. Showing previous data.');
      } else {
        setError(errMsg || 'Failed to load chart data. Try again or select a different timeframe.');
      }

      setIsLoading(false);
    }
  }, [adapter, isAuthenticated, symbol, exchange, selectedInterval, masterContractReady]);

  // Auto-fetch on mount and when dependencies change
  useEffect(() => {
    fetchHistoricalData();
  }, [fetchHistoricalData]);

  // Handle manual refresh
  const handleRefresh = () => {
    fetchHistoricalData();
  };

  // Loading state — show spinner while context is initializing or data is being fetched
  if ((isLoading || contextLoading) && chartData.length === 0) {
    return (
      <div className="flex-1 flex items-center justify-center bg-[#0A0A0A]" style={{ height: height || '100%', minHeight: height || 300 }}>
        <div className="text-center">
          <Loader2 className="w-12 h-12 mx-auto mb-4 text-[#FF8800] animate-spin" />
          <p className="text-sm text-gray-400">Loading chart data...</p>
        </div>
      </div>
    );
  }

  // Not authenticated state — only show after context has finished loading
  if (!isAuthenticated && !contextLoading && !isConnecting && chartData.length === 0) {
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
                onClick={() => {
                  setSelectedInterval(tf.value);
                  onIntervalChange?.(tf.value);
                }}
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
              interval={selectedInterval}
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

        {/* Error Notification - non-blocking, appears at bottom */}
        {error && chartData.length > 0 && (
          <div className="absolute bottom-2 left-1/2 -translate-x-1/2 bg-red-900/90 backdrop-blur-sm px-4 py-2 rounded border border-red-700 z-10 max-w-md">
            <div className="flex items-center gap-2">
              <AlertCircle className="w-4 h-4 text-red-400 flex-shrink-0" />
              <span className="text-xs text-red-200">{error}</span>
              <button
                onClick={() => setError(null)}
                className="ml-2 text-red-400 hover:text-red-200 transition-colors"
                title="Dismiss"
              >
                ×
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
