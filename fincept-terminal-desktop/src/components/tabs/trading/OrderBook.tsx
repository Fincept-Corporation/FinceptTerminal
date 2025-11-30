// File: src/components/tabs/trading/OrderBook.tsx
// Provider-agnostic order book component

import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';

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

  const { message } = useWebSocket(
    `${provider}.book.${symbol}`,
    null,
    { autoSubscribe: true, params: { depth } }
  );

  useEffect(() => {
    if (!message || message.type !== 'orderbook') return;

    const data = message.data;

    // Process bids (buy orders) - highest first
    const processedBids = (data.bids || [])
      .slice(0, depth)
      .sort((a: any, b: any) => b.price - a.price);

    // Process asks (sell orders) - lowest first
    const processedAsks = (data.asks || [])
      .slice(0, depth)
      .sort((a: any, b: any) => a.price - b.price);

    // Calculate running totals
    let bidTotal = 0;
    const bidsWithTotal = processedBids.map((level: any) => {
      bidTotal += level.size;
      return { price: level.price, size: level.size, total: bidTotal };
    });

    let askTotal = 0;
    const asksWithTotal = processedAsks.map((level: any) => {
      askTotal += level.size;
      return { price: level.price, size: level.size, total: askTotal };
    });

    setBids(bidsWithTotal);
    setAsks(asksWithTotal);

    // Calculate spread
    if (processedBids.length > 0 && processedAsks.length > 0) {
      const bestBid = processedBids[0].price;
      const bestAsk = processedAsks[0].price;
      setSpread(bestAsk - bestBid);
    }
  }, [message, depth]);

  const maxTotal = Math.max(
    bids[bids.length - 1]?.total || 0,
    asks[asks.length - 1]?.total || 0
  );

  return (
    <div className="flex flex-col h-full bg-[#0d0d0d] text-white font-mono text-xs">
      {/* Header */}
      <div className="flex items-center justify-between px-3 py-2 border-b border-gray-800">
        <h3 className="text-sm font-bold text-gray-400">ORDER BOOK</h3>
        <div className="text-xs text-gray-500">
          Spread: <span className="text-orange-500">{spread.toFixed(2)}</span>
        </div>
      </div>

      {/* Column Headers */}
      <div className="flex justify-between px-3 py-1 bg-[#1a1a1a] text-gray-500 text-[10px] font-semibold">
        <div className="w-1/3">PRICE</div>
        <div className="w-1/3 text-right">SIZE</div>
        <div className="w-1/3 text-right">TOTAL</div>
      </div>

      <div className="flex-1 overflow-hidden">
        {/* Asks (Sell Orders) - Reversed to show lowest at bottom */}
        <div className="h-1/2 overflow-y-auto flex flex-col-reverse border-b border-gray-800">
          {asks.slice().reverse().map((level, idx) => (
            <OrderBookRow
              key={`ask-${idx}`}
              price={level.price}
              size={level.size}
              total={level.total || 0}
              maxTotal={maxTotal}
              side="ask"
            />
          ))}
        </div>

        {/* Bids (Buy Orders) */}
        <div className="h-1/2 overflow-y-auto">
          {bids.map((level, idx) => (
            <OrderBookRow
              key={`bid-${idx}`}
              price={level.price}
              size={level.size}
              total={level.total || 0}
              maxTotal={maxTotal}
              side="bid"
            />
          ))}
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
  const percentage = (total / maxTotal) * 100;
  const bgColor = side === 'bid' ? 'bg-green-900/20' : 'bg-red-900/20';

  return (
    <div className="relative px-3 py-0.5 hover:bg-gray-800/50 cursor-pointer">
      {/* Background bar */}
      <div
        className={`absolute right-0 top-0 bottom-0 ${bgColor}`}
        style={{ width: `${percentage}%` }}
      />

      {/* Content */}
      <div className="relative flex justify-between">
        <div className={`w-1/3 ${side === 'bid' ? 'text-green-400' : 'text-red-400'}`}>
          {price.toFixed(2)}
        </div>
        <div className="w-1/3 text-right text-gray-300">{size.toFixed(4)}</div>
        <div className="w-1/3 text-right text-gray-500">{total.toFixed(4)}</div>
      </div>
    </div>
  );
}
