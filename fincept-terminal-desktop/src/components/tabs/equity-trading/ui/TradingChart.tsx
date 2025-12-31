/**
 * Trading Chart
 * Real-time candlestick chart with indicators
 */

import React, { useState, useEffect } from 'react';
import { BrokerType, Candle, TimeInterval } from '../core/types';
import { historicalDataService } from '../services/HistoricalDataService';

interface TradingChartProps {
  brokerId: BrokerType;
  symbol: string;
  exchange: string;
  interval?: TimeInterval;
}

const TradingChart: React.FC<TradingChartProps> = ({
  brokerId,
  symbol,
  exchange,
  interval = '5m',
}) => {
  const [candles, setCandles] = useState<Candle[]>([]);
  const [loading, setLoading] = useState(false);
  const [selectedInterval, setSelectedInterval] = useState<TimeInterval>(interval);

  useEffect(() => {
    if (!symbol || !brokerId) return;

    fetchCandles();
  }, [brokerId, symbol, exchange, selectedInterval]);

  const fetchCandles = async () => {
    setLoading(true);
    try {
      const to = new Date();
      const from = new Date(to.getTime() - 24 * 60 * 60 * 1000); // Last 24 hours

      const data = await historicalDataService.fetchHistoricalData(
        brokerId,
        symbol,
        exchange,
        selectedInterval,
        from,
        to
      );

      setCandles(data);
    } catch (error) {
      console.error('[TradingChart] Failed to fetch candles:', error);
    } finally {
      setLoading(false);
    }
  };

  if (loading) {
    return (
      <div className="h-full flex items-center justify-center">
        <div className="text-muted-foreground">Loading chart...</div>
      </div>
    );
  }

  if (candles.length === 0) {
    return (
      <div className="h-full flex items-center justify-center">
        <div className="text-center text-muted-foreground">
          <div className="text-2xl mb-2">📈</div>
          <div className="text-sm">No chart data available</div>
        </div>
      </div>
    );
  }

  // Calculate price range
  const prices = candles.flatMap(c => [c.high, c.low]);
  const maxPrice = Math.max(...prices);
  const minPrice = Math.min(...prices);
  const priceRange = maxPrice - minPrice;
  const padding = priceRange * 0.1;

  const normalizePrice = (price: number): number => {
    return ((maxPrice + padding - price) / (priceRange + 2 * padding)) * 100;
  };

  const currentPrice = candles[candles.length - 1]?.close || 0;
  const firstPrice = candles[0]?.open || 0;
  const priceChange = currentPrice - firstPrice;
  const priceChangePercent = (priceChange / firstPrice) * 100;

  return (
    <div className="h-full flex flex-col">
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-2 border-b border-border">
        <div>
          <div className="flex items-center gap-2">
            <h3 className="font-semibold">{symbol}</h3>
            <span className="text-xs text-muted-foreground uppercase">
              {brokerId} - {exchange}
            </span>
          </div>
          <div className="flex items-center gap-2 text-sm mt-1">
            <span className={priceChange >= 0 ? 'text-green-500' : 'text-red-500'}>
              ₹{currentPrice.toFixed(2)}
            </span>
            <span className={`text-xs ${priceChange >= 0 ? 'text-green-500' : 'text-red-500'}`}>
              {priceChange >= 0 ? '+' : ''}
              {priceChange.toFixed(2)} ({priceChangePercent.toFixed(2)}%)
            </span>
          </div>
        </div>

        {/* Interval Selector */}
        <div className="flex items-center gap-1">
          {(['1m', '5m', '15m', '1h', '1d'] as TimeInterval[]).map((int) => (
            <button
              key={int}
              onClick={() => setSelectedInterval(int)}
              className={`px-2 py-1 text-xs rounded ${
                selectedInterval === int
                  ? 'bg-primary text-primary-foreground'
                  : 'bg-muted text-muted-foreground hover:bg-muted/80'
              }`}
            >
              {int}
            </button>
          ))}
        </div>
      </div>

      {/* Chart Area */}
      <div className="flex-1 p-4">
        <div className="relative h-full">
          {/* Price Axis (Right) */}
          <div className="absolute right-0 top-0 bottom-0 w-16 flex flex-col justify-between text-xs text-muted-foreground">
            <div>₹{maxPrice.toFixed(2)}</div>
            <div>₹{((maxPrice + minPrice) / 2).toFixed(2)}</div>
            <div>₹{minPrice.toFixed(2)}</div>
          </div>

          {/* Candlestick Chart */}
          <div className="h-full pr-16 flex items-end gap-px">
            {candles.map((candle, idx) => {
              const isGreen = candle.close >= candle.open;
              const bodyTop = normalizePrice(Math.max(candle.open, candle.close));
              const bodyBottom = normalizePrice(Math.min(candle.open, candle.close));
              const bodyHeight = Math.abs(bodyBottom - bodyTop);
              const wickTop = normalizePrice(candle.high);
              const wickBottom = normalizePrice(candle.low);

              return (
                <div
                  key={idx}
                  className="relative flex-1 h-full"
                  style={{ minWidth: '2px', maxWidth: '20px' }}
                  title={`O: ${candle.open} H: ${candle.high} L: ${candle.low} C: ${candle.close}`}
                >
                  {/* Wick */}
                  <div
                    className={`absolute left-1/2 transform -translate-x-1/2 w-px ${
                      isGreen ? 'bg-green-500' : 'bg-red-500'
                    }`}
                    style={{
                      top: `${wickTop}%`,
                      height: `${wickBottom - wickTop}%`,
                    }}
                  />
                  {/* Body */}
                  <div
                    className={`absolute left-0 right-0 ${
                      isGreen ? 'bg-green-500' : 'bg-red-500'
                    }`}
                    style={{
                      top: `${bodyTop}%`,
                      height: `${Math.max(bodyHeight, 0.5)}%`,
                    }}
                  />
                </div>
              );
            })}
          </div>
        </div>
      </div>

      {/* Footer */}
      <div className="px-4 py-2 border-t border-border">
        <div className="flex items-center justify-between text-xs text-muted-foreground">
          <div>
            <span>O: ₹{candles[candles.length - 1]?.open.toFixed(2)}</span>
            <span className="ml-3">H: ₹{candles[candles.length - 1]?.high.toFixed(2)}</span>
            <span className="ml-3">L: ₹{candles[candles.length - 1]?.low.toFixed(2)}</span>
            <span className="ml-3">C: ₹{candles[candles.length - 1]?.close.toFixed(2)}</span>
          </div>
          <div>Vol: {candles[candles.length - 1]?.volume.toLocaleString()}</div>
        </div>
      </div>
    </div>
  );
};

export default TradingChart;
