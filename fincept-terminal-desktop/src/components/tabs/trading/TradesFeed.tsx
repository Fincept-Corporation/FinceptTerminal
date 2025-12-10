// File: src/components/tabs/trading/TradesFeed.tsx
// Provider-agnostic trades feed component

import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';
import { isProviderAvailable } from '../../../services/websocket';

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

  // Only subscribe if WebSocket adapter exists for this provider
  const hasWebSocket = isProviderAvailable(provider);

  const { message, isLoading, error } = useWebSocket(
    hasWebSocket ? `${provider}.trades.${symbol}` : null,
    null,
    { autoSubscribe: hasWebSocket }
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
        {isLoading && trades.length === 0 ? (
          <div className="p-4 text-center text-gray-500">
            <div className="animate-spin rounded-full h-6 w-6 border-b-2 border-orange-500 mx-auto mb-2"></div>
            Connecting...
          </div>
        ) : error && trades.length === 0 ? (
          <div className="p-4 text-center text-red-500 text-xs">
            Connection error
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
