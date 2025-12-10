// File: src/components/tabs/trading/OrderBook.tsx
// Provider-agnostic order book component

import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';
import { isProviderAvailable } from '../../../services/websocket';

interface OrderBookLevel {
  price: number;
  size: number;
  total?: number;
}

interface OrderBookProps {
  symbol: string;
  provider: string;
  depth?: number;
}

export function OrderBook({ symbol, provider, depth = 25 }: OrderBookProps) {
  const [bids, setBids] = useState<OrderBookLevel[]>([]);
  const [asks, setAsks] = useState<OrderBookLevel[]>([]);
  const [spread, setSpread] = useState<number>(0);

  // Only subscribe if WebSocket adapter exists for this provider
  const hasWebSocket = isProviderAvailable(provider);

  const { message } = useWebSocket(
    hasWebSocket ? `${provider}.book.${symbol}` : null,
    null,
    { autoSubscribe: hasWebSocket, params: { depth } }
  );

  useEffect(() => {
    if (!message || message.type !== 'orderbook') return;

    const data = message.data;

    // Validate data structure
    if (!data || typeof data !== 'object') {
      console.warn('[OrderBook] Invalid data structure:', data);
      return;
    }

    // Process bids (buy orders) - highest first
    const rawBids = Array.isArray(data.bids) ? data.bids : [];
    const processedBids = rawBids
      .filter((level: any) =>
        level &&
        typeof level === 'object' &&
        typeof level.price === 'number' &&
        typeof level.size === 'number'
      )
      .slice(0, depth)
      .sort((a: any, b: any) => b.price - a.price);

    // Process asks (sell orders) - lowest first
    const rawAsks = Array.isArray(data.asks) ? data.asks : [];
    const processedAsks = rawAsks
      .filter((level: any) =>
        level &&
        typeof level === 'object' &&
        typeof level.price === 'number' &&
        typeof level.size === 'number'
      )
      .slice(0, depth)
      .sort((a: any, b: any) => a.price - b.price);

    // Calculate running totals
    let bidTotal = 0;
    const bidsWithTotal = processedBids.map((level: any) => {
      bidTotal += level.size || 0;
      return {
        price: level.price || 0,
        size: level.size || 0,
        total: bidTotal
      };
    });

    let askTotal = 0;
    const asksWithTotal = processedAsks.map((level: any) => {
      askTotal += level.size || 0;
      return {
        price: level.price || 0,
        size: level.size || 0,
        total: askTotal
      };
    });

    setBids(bidsWithTotal);
    setAsks(asksWithTotal);

    // Calculate spread
    if (processedBids.length > 0 && processedAsks.length > 0) {
      const bestBid = processedBids[0].price || 0;
      const bestAsk = processedAsks[0].price || 0;
      setSpread(bestAsk - bestBid);
    }
  }, [message, depth]);

  const maxTotal = Math.max(
    bids[bids.length - 1]?.total || 0,
    asks[asks.length - 1]?.total || 0
  );

  return (
    <div className="flex flex-col h-full bg-[#0d0d0d] text-white font-mono text-[11px]">
      {/* Header */}
      <div className="flex items-center justify-between px-2 py-1.5 border-b border-gray-800">
        <h3 className="text-[10px] font-bold text-gray-400 tracking-wider">ORDER BOOK</h3>
        <div className="text-[10px] text-gray-500">
          Spread: <span className="text-orange-500 font-bold">{(spread ?? 0).toFixed(2)}</span>
        </div>
      </div>

      {/* Column Headers */}
      <div className="flex justify-between px-2 py-0.5 bg-[#1a1a1a] text-gray-500 text-[9px] font-bold uppercase tracking-wide">
        <div className="w-1/3">Price</div>
        <div className="w-1/3 text-right">Size</div>
        <div className="w-1/3 text-right">Total</div>
      </div>

      <div className="flex-1 overflow-hidden">
        {/* Asks (Sell Orders) - Reversed to show lowest at bottom */}
        <div className="h-1/2 overflow-y-auto flex flex-col-reverse border-b border-gray-800">
          {asks.length === 0 ? (
            <div className="flex items-center justify-center h-full text-gray-500 text-xs">
              Waiting for data...
            </div>
          ) : (
            asks.slice().reverse().map((level, idx) => (
              <OrderBookRow
                key={`ask-${idx}`}
                price={level.price}
                size={level.size}
                total={level.total || 0}
                maxTotal={maxTotal}
                side="ask"
              />
            ))
          )}
        </div>

        {/* Bids (Buy Orders) */}
        <div className="h-1/2 overflow-y-auto">
          {bids.length === 0 ? (
            <div className="flex items-center justify-center h-full text-gray-500 text-xs">
              Waiting for data...
            </div>
          ) : (
            bids.map((level, idx) => (
              <OrderBookRow
                key={`bid-${idx}`}
                price={level.price}
                size={level.size}
                total={level.total || 0}
                maxTotal={maxTotal}
                side="bid"
              />
            ))
          )}
        </div>
      </div>
    </div>
  );
}

interface OrderBookRowProps {
  price: number;
  size: number;
  total: number;
  maxTotal: number;
  side: 'bid' | 'ask';
}

function OrderBookRow({ price, size, total, maxTotal, side }: OrderBookRowProps) {
  // Safely handle undefined or null values
  const safePrice = price ?? 0;
  const safeSize = size ?? 0;
  const safeTotal = total ?? 0;
  const safeMaxTotal = maxTotal > 0 ? maxTotal : 1; // Prevent division by zero

  const percentage = (safeTotal / safeMaxTotal) * 100;
  const bgColor = side === 'bid' ? 'bg-green-900/30' : 'bg-red-900/30';

  return (
    <div className="relative px-2 py-[2px] hover:bg-gray-800/70 cursor-pointer transition-colors">
      {/* Background bar */}
      <div
        className={`absolute right-0 top-0 bottom-0 ${bgColor} transition-all`}
        style={{ width: `${percentage}%` }}
      />

      {/* Content */}
      <div className="relative flex justify-between items-center">
        <div className={`w-1/3 font-bold ${side === 'bid' ? 'text-green-400' : 'text-red-400'}`}>
          {safePrice.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </div>
        <div className="w-1/3 text-right text-gray-200 tabular-nums">
          {safeSize.toLocaleString('en-US', { minimumFractionDigits: 4, maximumFractionDigits: 4 })}
        </div>
        <div className="w-1/3 text-right text-gray-500 tabular-nums">
          {safeTotal.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </div>
      </div>
    </div>
  );
}
