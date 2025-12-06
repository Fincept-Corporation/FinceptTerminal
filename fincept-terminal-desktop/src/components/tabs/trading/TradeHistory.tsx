// File: src/components/tabs/trading/TradeHistory.tsx
// Display executed trades log

import React from 'react';
import type { PaperTradingTrade } from '../../../paper-trading/types';

interface TradeHistoryProps {
  trades: PaperTradingTrade[];
}

export function TradeHistory({ trades }: TradeHistoryProps) {
  // Sort by timestamp descending (newest first)
  const sortedTrades = [...trades].sort(
    (a, b) => new Date(b.timestamp).getTime() - new Date(a.timestamp).getTime()
  );

  return (
    <div className="bg-[#0d0d0d] border-t border-gray-800">
      <div className="px-4 py-2 border-b border-gray-800">
        <h3 className="text-sm font-bold text-gray-400">TRADE HISTORY ({trades.length})</h3>
      </div>

      <div className="max-h-64 overflow-y-auto">
        {sortedTrades.length === 0 ? (
          <div className="p-4 text-center text-gray-500 text-xs">No trades executed</div>
        ) : (
          <table className="w-full text-xs font-mono">
            <thead className="bg-[#1a1a1a] text-gray-500 sticky top-0">
              <tr>
                <th className="px-3 py-2 text-left">TIME</th>
                <th className="px-3 py-2 text-left">SYMBOL</th>
                <th className="px-3 py-2 text-left">SIDE</th>
                <th className="px-3 py-2 text-right">QTY</th>
                <th className="px-3 py-2 text-right">PRICE</th>
                <th className="px-3 py-2 text-right">FEE</th>
                <th className="px-3 py-2 text-center">TYPE</th>
              </tr>
            </thead>
            <tbody>
              {sortedTrades.map((trade) => (
                <TradeRow key={trade.id} trade={trade} />
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

interface TradeRowProps {
  trade: PaperTradingTrade;
}

function TradeRow({ trade }: TradeRowProps) {
  const time = new Date(trade.timestamp).toLocaleTimeString();
  const date = new Date(trade.timestamp).toLocaleDateString();

  return (
    <tr className="border-b border-gray-800 hover:bg-gray-800/50">
      <td className="px-3 py-2 text-gray-400">
        <div className="flex flex-col">
          <span>{time}</span>
          <span className="text-[9px] opacity-60">{date}</span>
        </div>
      </td>
      <td className="px-3 py-2 text-white font-semibold">{trade.symbol}</td>
      <td className="px-3 py-2">
        <span
          className={`px-2 py-0.5 rounded text-[10px] font-semibold ${
            trade.side === 'buy'
              ? 'bg-green-900/30 text-green-400'
              : 'bg-red-900/30 text-red-400'
          }`}
        >
          {trade.side.toUpperCase()}
        </span>
      </td>
      <td className="px-3 py-2 text-right text-gray-300">{trade.quantity.toFixed(4)}</td>
      <td className="px-3 py-2 text-right text-white">${trade.price.toFixed(2)}</td>
      <td className="px-3 py-2 text-right text-gray-400">${trade.fee.toFixed(2)}</td>
      <td className="px-3 py-2 text-center">
        <span
          className={`px-2 py-0.5 rounded text-[9px] ${
            trade.isMaker
              ? 'bg-purple-900/30 text-purple-400'
              : 'bg-cyan-900/30 text-cyan-400'
          }`}
        >
          {trade.isMaker ? 'MAKER' : 'TAKER'}
        </span>
      </td>
    </tr>
  );
}
