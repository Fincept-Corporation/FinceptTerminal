// File: src/components/tabs/trading/TickerDisplay.tsx
// Provider-agnostic ticker display component

import React, { useState, useEffect } from 'react';
import { useWebSocket } from '../../../hooks/useWebSocket';

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

  const { message } = useWebSocket(
    `${provider}.ticker.${symbol}`,
    null,
    { autoSubscribe: true }
  );

  useEffect(() => {
    if (!message || message.type !== 'ticker') return;

    const data = message.data;

    setTicker({
      last: data.last || data.close || data.mid || 0,
      bid: data.bid || 0,
      ask: data.ask || 0,
      volume: data.volume || data.v || 0,
      high: data.high || data.h || 0,
      low: data.low || data.l || 0,
      change: data.change || 0,
      changePct: data.change_pct || data.changePct || 0
    });
  }, [message]);

  if (!ticker) {
    return (
      <div className="bg-[#0d0d0d] p-4 text-center text-gray-500">
        Loading ticker data...
      </div>
    );
  }

  const isPositive = ticker.change >= 0;

  return (
    <div className="bg-[#0d0d0d] border-b border-gray-800">
      {/* Main Price */}
      <div className="px-4 py-3 border-b border-gray-800">
        <div className="text-2xl font-bold font-mono text-white mb-1">
          ${ticker.last.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
        </div>
        <div className={`text-sm font-semibold ${isPositive ? 'text-green-400' : 'text-red-400'}`}>
          {isPositive ? '+' : ''}{ticker.change.toFixed(2)} ({isPositive ? '+' : ''}{ticker.changePct.toFixed(2)}%)
        </div>
      </div>

      {/* Stats Grid */}
      <div className="grid grid-cols-2 gap-2 px-4 py-3 text-xs font-mono">
        <StatItem label="BID" value={ticker.bid.toFixed(2)} valueColor="text-green-400" />
        <StatItem label="ASK" value={ticker.ask.toFixed(2)} valueColor="text-red-400" />
        <StatItem label="HIGH" value={ticker.high.toFixed(2)} valueColor="text-gray-300" />
        <StatItem label="LOW" value={ticker.low.toFixed(2)} valueColor="text-gray-300" />
        <StatItem label="VOLUME" value={ticker.volume.toFixed(2)} valueColor="text-blue-400" />
        <StatItem
          label="SPREAD"
          value={(ticker.ask - ticker.bid).toFixed(2)}
          valueColor="text-orange-400"
        />
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
    <div>
      <div className="text-gray-500 text-[10px] mb-0.5">{label}</div>
      <div className={`${valueColor} font-semibold`}>{value}</div>
    </div>
  );
}
