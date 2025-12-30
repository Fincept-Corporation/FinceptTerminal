// File: src/components/tabs/trading/TradesFeed.tsx
// Provider-agnostic trades feed component

import React, { useState, useEffect } from 'react';
import { useRustTrades } from '../../../hooks/useRustWebSocket';


interface Trade {
  price: number;
  size: number;
  side: 'buy' | 'sell';
  time: number;
}

interface TradesFeedProps {
  symbol: string;
  provider: string;
  maxTrades?: number;
}

export function TradesFeed({ symbol, provider, maxTrades = 50 }: TradesFeedProps) {
  const [trades, setTrades] = useState<Trade[]>([]);

  // Use Rust WebSocket backend
  const { trades: rustTrades, error: wsError } = useRustTrades(
    provider,
    symbol,
    maxTrades
  );

  useEffect(() => {
    if (!rustTrades || rustTrades.length === 0) return;

    // Map Rust trade data to component format
    const newTrades: Trade[] = rustTrades.map((trade) => ({
      price: trade.price,
      size: trade.quantity,
      side: trade.side === 'buy' || trade.side === 'sell' ? trade.side : 'buy',
      time: trade.timestamp
    }));

    setTrades(newTrades.slice(0, maxTrades));
  }, [rustTrades, maxTrades]);

  return (
    <div className="flex flex-col h-full bg-[#0d0d0d] text-white font-mono text-[11px]">
      {/* Header */}
      <div className="flex items-center justify-between px-2 py-1.5 border-b border-gray-800">
        <h3 className="text-[10px] font-bold text-gray-400 tracking-wider">RECENT TRADES</h3>
        <div className="flex items-center gap-1.5">
          <div className="w-1.5 h-1.5 bg-green-500 rounded-full animate-pulse"></div>
          <span className="text-[10px] text-gray-500">Live</span>
        </div>
      </div>

      {/* Column Headers */}
      <div className="flex justify-between px-2 py-0.5 bg-[#1a1a1a] text-gray-500 text-[9px] font-bold uppercase tracking-wide">
        <div className="w-1/3">Time</div>
        <div className="w-1/3 text-right">Price</div>
        <div className="w-1/3 text-right">Size</div>
      </div>

      {/* Trades List */}
      <div className="flex-1 overflow-y-auto">
        {wsError && trades.length === 0 ? (
          <div className="p-4 text-center text-red-500 text-xs">
            Connection error: {wsError}
          </div>
        ) : trades.length === 0 ? (
          <div className="p-4 text-center text-gray-500">Waiting for trades...</div>
        ) : (
          trades.map((trade, idx) => (
            <TradeRow key={`${trade.time}-${idx}`} trade={trade} />
          ))
        )}
      </div>
    </div>
  );
}

interface TradeRowProps {
  trade: Trade;
}

function TradeRow({ trade }: TradeRowProps) {
  const timeStr = new Date(trade.time).toLocaleTimeString('en-US', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });

  return (
    <div className="flex justify-between px-2 py-[2px] hover:bg-gray-800/70 cursor-pointer transition-colors">
      <div className="w-1/3 text-gray-500 tabular-nums">{timeStr}</div>
      <div className={`w-1/3 text-right font-bold tabular-nums ${trade.side === 'buy' ? 'text-green-400' : 'text-red-400'}`}>
        {trade.price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
      </div>
      <div className="w-1/3 text-right text-gray-200 tabular-nums">
        {trade.size.toLocaleString('en-US', { minimumFractionDigits: 4, maximumFractionDigits: 4 })}
      </div>
    </div>
  );
}
