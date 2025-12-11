/**
 * DepthChart - Market depth visualization
 * Shows orderbook bid/ask distribution
 */

import React, { useMemo } from 'react';
import { AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer } from 'recharts';
import { useOrderBook } from '../hooks/useMarketData';

interface DepthChartProps {
  symbol: string;
  height?: number;
  limit?: number;
}

interface DepthDataPoint {
  price: number;
  bidTotal: number;
  askTotal: number;
}

export function DepthChart({ symbol, height = 300, limit = 50 }: DepthChartProps) {
  const { orderBook, isLoading, error } = useOrderBook(symbol, limit, true);

  const depthData = useMemo(() => {
    if (!orderBook) return [];

    const data: DepthDataPoint[] = [];

    // Calculate cumulative bid depth
    let bidTotal = 0;
    const bids = [...orderBook.bids]
      .sort((a, b) => b.price - a.price) // Highest to lowest
      .slice(0, limit);

    bids.forEach((bid) => {
      bidTotal += bid.size;
      data.push({
        price: bid.price,
        bidTotal,
        askTotal: 0,
      });
    });

    // Calculate cumulative ask depth
    let askTotal = 0;
    const asks = [...orderBook.asks]
      .sort((a, b) => a.price - b.price) // Lowest to highest
      .slice(0, limit);

    asks.forEach((ask) => {
      askTotal += ask.size;
      data.push({
        price: ask.price,
        bidTotal: 0,
        askTotal,
      });
    });

    // Sort by price
    return data.sort((a, b) => a.price - b.price);
  }, [orderBook, limit]);

  const midPrice = useMemo(() => {
    if (!orderBook || !orderBook.bids.length || !orderBook.asks.length) return null;
    return (orderBook.bids[0].price + orderBook.asks[0].price) / 2;
  }, [orderBook]);

  if (isLoading) {
    return (
      <div className="flex items-center justify-center bg-black border border-gray-800 rounded" style={{ height }}>
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-blue-500"></div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="flex items-center justify-center bg-black border border-gray-800 rounded text-red-500 text-sm" style={{ height }}>
        Failed to load depth chart: {error.message}
      </div>
    );
  }

  if (!depthData.length) {
    return (
      <div className="flex items-center justify-center bg-black border border-gray-800 rounded text-gray-500 text-sm" style={{ height }}>
        No orderbook data available
      </div>
    );
  }

  return (
    <div className="bg-black border border-gray-800 rounded p-2">
      <div className="flex items-center justify-between mb-2 px-2">
        <span className="text-xs text-gray-400">MARKET DEPTH</span>
        {midPrice && (
          <span className="text-xs font-mono text-gray-300">
            MID: ${midPrice.toFixed(2)}
          </span>
        )}
      </div>

      <ResponsiveContainer width="100%" height={height}>
        <AreaChart data={depthData} margin={{ top: 5, right: 5, left: 5, bottom: 5 }}>
          <defs>
            <linearGradient id="bidGradient" x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor="#10b981" stopOpacity={0.4} />
              <stop offset="95%" stopColor="#10b981" stopOpacity={0} />
            </linearGradient>
            <linearGradient id="askGradient" x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor="#ef4444" stopOpacity={0.4} />
              <stop offset="95%" stopColor="#ef4444" stopOpacity={0} />
            </linearGradient>
          </defs>

          <XAxis
            dataKey="price"
            stroke="#4b5563"
            tick={{ fill: '#9ca3af', fontSize: 11 }}
            tickFormatter={(value) => `$${value.toFixed(0)}`}
          />

          <YAxis
            stroke="#4b5563"
            tick={{ fill: '#9ca3af', fontSize: 11 }}
            tickFormatter={(value) => value.toFixed(2)}
          />

          <Tooltip
            contentStyle={{
              backgroundColor: '#1f2937',
              border: '1px solid #374151',
              borderRadius: '4px',
              fontSize: '12px',
            }}
            labelStyle={{ color: '#d1d5db' }}
            itemStyle={{ color: '#d1d5db' }}
            formatter={(value: number) => value.toFixed(4)}
            labelFormatter={(label) => `Price: $${label}`}
          />

          <Area
            type="stepAfter"
            dataKey="bidTotal"
            stroke="#10b981"
            fill="url(#bidGradient)"
            strokeWidth={2}
            name="Bid Depth"
          />

          <Area
            type="stepBefore"
            dataKey="askTotal"
            stroke="#ef4444"
            fill="url(#askGradient)"
            strokeWidth={2}
            name="Ask Depth"
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
