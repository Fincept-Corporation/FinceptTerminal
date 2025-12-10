// File: src/components/tabs/trading/TickerDisplay.tsx
// Provider-agnostic ticker display component

import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';
import { isProviderAvailable } from '../../../services/websocket';

interface TickerDisplayProps {
  symbol: string;
  provider: string;
}

interface TickerData {
  last: number;
  bid: number;
  ask: number;
  volume: number;
  high: number;
  low: number;
  change: number;
  changePct: number;
}

export function TickerDisplay({ symbol, provider }: TickerDisplayProps) {
  const [ticker, setTicker] = useState<TickerData | null>(null);

  // Only subscribe if WebSocket adapter exists for this provider
  const hasWebSocket = isProviderAvailable(provider);

  const { message } = useWebSocket(
    hasWebSocket ? `${provider}.ticker.${symbol}` : null,
    null,
    { autoSubscribe: hasWebSocket }
  );

  useEffect(() => {
    if (!message || message.type !== 'ticker') return;

    const data = message.data;

    // Parse nested data if needed
    const tickerData = data.ticker || data;

    // Extract values with multiple fallbacks for different provider formats
    const last = parseFloat(tickerData.last || tickerData.close || tickerData.price || tickerData.mid || 0);
    const bid = parseFloat(tickerData.bid || tickerData.bid_price || tickerData.b || 0);
    const ask = parseFloat(tickerData.ask || tickerData.ask_price || tickerData.a || 0);
    const volume = parseFloat(tickerData.volume || tickerData.vol || tickerData.v || 0);
    const high = parseFloat(tickerData.high || tickerData.h || 0);
    const low = parseFloat(tickerData.low || tickerData.l || 0);

    // Calculate change if not provided
    let change = parseFloat(tickerData.change || 0);
    let changePct = parseFloat(tickerData.change_pct || tickerData.changePct || 0);

    if (change === 0 && tickerData.open) {
      change = last - parseFloat(tickerData.open);
      changePct = (change / parseFloat(tickerData.open)) * 100;
    }

    setTicker({
      last,
      bid,
      ask,
      volume,
      high,
      low,
      change,
      changePct
    });
  }, [message]);

  if (!ticker) {
    return (
      <div className="bg-[#0d0d0d] p-3 text-center text-gray-500">
        <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-orange-500 mx-auto mb-1.5"></div>
        <div className="text-[10px]">Loading ticker...</div>
      </div>
    );
  }

  const isPositive = ticker.change >= 0;

  return (
    <div className="bg-[#0d0d0d] border-b border-gray-800">
      {/* Main Price */}
      <div className="px-3 py-2 border-b border-gray-800">
        <div className="text-xl font-bold font-mono text-white mb-0.5 tabular-nums tracking-tight">
          ${ticker.last.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </div>
        <div className={`text-xs font-bold tabular-nums ${isPositive ? 'text-green-400' : 'text-red-400'}`}>
          {isPositive ? '▲' : '▼'} {isPositive ? '+' : ''}{ticker.change.toFixed(2)} ({isPositive ? '+' : ''}{ticker.changePct.toFixed(2)}%)
        </div>
      </div>

      {/* Stats Grid */}
      <div className="grid grid-cols-3 gap-x-2 gap-y-1.5 px-3 py-2 text-[10px] font-mono">
        <StatItem label="BID" value={ticker.bid.toFixed(2)} valueColor="text-green-400" />
        <StatItem label="ASK" value={ticker.ask.toFixed(2)} valueColor="text-red-400" />
        <StatItem
          label="SPREAD"
          value={(ticker.ask - ticker.bid).toFixed(2)}
          valueColor="text-orange-400"
        />
        <StatItem label="HIGH" value={ticker.high.toFixed(2)} valueColor="text-gray-300" />
        <StatItem label="LOW" value={ticker.low.toFixed(2)} valueColor="text-gray-300" />
        <StatItem label="VOLUME" value={ticker.volume.toLocaleString(undefined, { maximumFractionDigits: 0 })} valueColor="text-cyan-400" />
      </div>
    </div>
  );
}

interface StatItemProps {
  label: string;
  value: string;
  valueColor: string;
}

function StatItem({ label, value, valueColor }: StatItemProps) {
  return (
    <div className="flex flex-col">
      <div className="text-gray-600 text-[9px] font-bold uppercase tracking-wide mb-0.5">{label}</div>
      <div className={`${valueColor} font-bold text-[11px] tabular-nums`}>{value}</div>
    </div>
  );
}
