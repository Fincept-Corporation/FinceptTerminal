// File: src/components/tabs/trading/PaperTradingStats.tsx
// Display paper trading performance statistics

import React from 'react';
import { TrendingUp, TrendingDown, Activity, DollarSign } from 'lucide-react';

interface PaperTradingStatsProps {
  stats: {
    totalTrades: number;
    winningTrades: number;
    losingTrades: number;
    winRate: number;
    totalPnL: number;
    realizedPnL: number;
    unrealizedPnL: number;
    totalFees: number;
    averageWin: number;
    averageLoss: number;
    largestWin: number;
    largestLoss: number;
    profitFactor: number;
  };
  balance: number;
  initialBalance: number;
}

export function PaperTradingStats({ stats, balance, initialBalance }: PaperTradingStatsProps) {
  const totalReturn = initialBalance > 0 ? ((balance - initialBalance) / initialBalance) * 100 : 0;
  const totalReturnColor = totalReturn >= 0 ? 'text-green-400' : 'text-red-400';

  return (
    <div className="bg-[#0d0d0d] border-t border-gray-800 p-4">
      <div className="flex items-center justify-between mb-3">
        <h3 className="text-sm font-bold text-gray-400">PERFORMANCE STATISTICS</h3>
        <Activity size={14} className="text-orange-600" />
      </div>

      <div className="grid grid-cols-2 gap-3">
        {/* Total Return */}
        <div className="bg-[#1a1a1a] rounded p-3 border border-gray-800">
          <div className="flex items-center gap-2 mb-1">
            {totalReturn >= 0 ? (
              <TrendingUp size={12} className="text-green-400" />
            ) : (
              <TrendingDown size={12} className="text-red-400" />
            )}
            <span className="text-[10px] text-gray-500 uppercase">Total Return</span>
          </div>
          <div className={`text-lg font-bold font-mono ${totalReturnColor}`}>
            {totalReturn >= 0 ? '+' : ''}
            {totalReturn.toFixed(2)}%
          </div>
          <div className="text-[9px] text-gray-500 mt-1">
            ${balance.toFixed(2)} / ${initialBalance.toFixed(2)}
          </div>
        </div>

        {/* Total P&L */}
        <div className="bg-[#1a1a1a] rounded p-3 border border-gray-800">
          <div className="flex items-center gap-2 mb-1">
            <DollarSign size={12} className="text-orange-600" />
            <span className="text-[10px] text-gray-500 uppercase">Total P&L</span>
          </div>
          <div
            className={`text-lg font-bold font-mono ${
              stats.totalPnL >= 0 ? 'text-green-400' : 'text-red-400'
            }`}
          >
            {stats.totalPnL >= 0 ? '+' : ''}${stats.totalPnL.toFixed(2)}
          </div>
          <div className="text-[9px] text-gray-500 mt-1">
            Realized: ${stats.realizedPnL.toFixed(2)}
          </div>
        </div>

        {/* Win Rate */}
        <div className="bg-[#1a1a1a] rounded p-3 border border-gray-800">
          <div className="text-[10px] text-gray-500 uppercase mb-1">Win Rate</div>
          <div className="text-lg font-bold font-mono text-white">
            {stats.winRate.toFixed(1)}%
          </div>
          <div className="text-[9px] text-gray-500 mt-1">
            {stats.winningTrades}W / {stats.losingTrades}L
          </div>
        </div>

        {/* Total Trades */}
        <div className="bg-[#1a1a1a] rounded p-3 border border-gray-800">
          <div className="text-[10px] text-gray-500 uppercase mb-1">Total Trades</div>
          <div className="text-lg font-bold font-mono text-white">{stats.totalTrades}</div>
          <div className="text-[9px] text-gray-500 mt-1">Fees: ${stats.totalFees.toFixed(2)}</div>
        </div>

        {/* Profit Factor */}
        <div className="bg-[#1a1a1a] rounded p-3 border border-gray-800">
          <div className="text-[10px] text-gray-500 uppercase mb-1">Profit Factor</div>
          <div
            className={`text-lg font-bold font-mono ${
              stats.profitFactor >= 1 ? 'text-green-400' : 'text-red-400'
            }`}
          >
            {stats.profitFactor.toFixed(2)}x
          </div>
          <div className="text-[9px] text-gray-500 mt-1">
            {stats.profitFactor >= 1.5
              ? 'Excellent'
              : stats.profitFactor >= 1
              ? 'Good'
              : 'Poor'}
          </div>
        </div>

        {/* Average Win/Loss */}
        <div className="bg-[#1a1a1a] rounded p-3 border border-gray-800">
          <div className="text-[10px] text-gray-500 uppercase mb-1">Avg Win/Loss</div>
          <div className="text-xs font-mono">
            <div className="text-green-400">+${stats.averageWin.toFixed(2)}</div>
            <div className="text-red-400">-${Math.abs(stats.averageLoss).toFixed(2)}</div>
          </div>
        </div>
      </div>

      {/* Best/Worst Trades */}
      <div className="mt-3 grid grid-cols-2 gap-3">
        <div className="bg-green-900/10 border border-green-900/30 rounded p-2">
          <div className="text-[9px] text-green-600 uppercase mb-1">Best Trade</div>
          <div className="text-sm font-bold font-mono text-green-400">
            +${stats.largestWin.toFixed(2)}
          </div>
        </div>
        <div className="bg-red-900/10 border border-red-900/30 rounded p-2">
          <div className="text-[9px] text-red-600 uppercase mb-1">Worst Trade</div>
          <div className="text-sm font-bold font-mono text-red-400">
            -${Math.abs(stats.largestLoss).toFixed(2)}
          </div>
        </div>
      </div>
    </div>
  );
}
