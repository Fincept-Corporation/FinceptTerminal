// File: src/components/tabs/trading/TradesFeed.tsx
// Provider-agnostic trades feed component

import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';

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

  const { message } = useWebSocket(
    `${provider}.trades.${symbol}`,
    null,
    { autoSubscribe: true }
  );

  useEffect(() => {
    if (!message || message.type !== 'trade') return;

    const data = message.data;

    // Handle different data formats
    let newTrades: Trade[] = [];

    if (Array.isArray(data.trades)) {
      // Multiple trades (e.g., HyperLiquid)
      newTrades = data.trades.map((trade: any) => ({
        price: trade.price || parseFloat(trade.px),
        size: trade.size || parseFloat(trade.sz),
        side: trade.side === 'sell' || trade.side === 'A' ? 'sell' : 'buy',
        time: trade.time || Date.now()
      }));
    } else {
      // Single trade (e.g., Kraken)
      newTrades = [{
        price: data.price || parseFloat(data.px),
        size: data.size || parseFloat(data.sz),
        side: data.side === 'sell' || data.side === 'A' ? 'sell' : 'buy',
        time: data.time || Date.now()
      }];
    }

    setTrades(prev => {
      const updated = [...newTrades, ...prev];
      return updated.slice(0, maxTrades);
    });
  }, [message, maxTrades]);

  return (
    <div className="flex flex-col h-full bg-[#0d0d0d] text-white font-mono text-xs">
      {/* Header */}
      <div className="px-3 py-2 border-b border-gray-800">
        <h3 className="text-sm font-bold text-gray-400">RECENT TRADES</h3>
      </div>

      {/* Column Headers */}
      <div className="flex justify-between px-3 py-1 bg-[#1a1a1a] text-gray-500 text-[10px] font-semibold">
        <div className="w-1/3">TIME</div>
        <div className="w-1/3 text-right">PRICE</div>
        <div className="w-1/3 text-right">SIZE</div>
      </div>

      {/* Trades List */}
      <div className="flex-1 overflow-y-auto">
        {trades.length === 0 ? (
          <div className="p-4 text-center text-gray-500">No trades yet...</div>
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
    <div className="flex justify-between px-3 py-1 hover:bg-gray-800/50">
      <div className="w-1/3 text-gray-400">{timeStr}</div>
      <div className={`w-1/3 text-right font-semibold ${trade.side === 'buy' ? 'text-green-400' : 'text-red-400'}`}>
        {trade.price.toFixed(2)}
      </div>
      <div className="w-1/3 text-right text-gray-300">{trade.size.toFixed(4)}</div>
    </div>
  );
}
