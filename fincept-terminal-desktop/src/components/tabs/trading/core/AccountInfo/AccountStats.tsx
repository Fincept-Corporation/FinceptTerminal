/**
 * AccountStats - Trading statistics dashboard
 * Shows P&L, win rate, trade count, etc.
 */

import React from 'react';
import { TrendingUp, TrendingDown, Target, DollarSign } from 'lucide-react';
import { useTradingStats } from '../../hooks/useAccountInfo';

export function AccountStats() {
  const { stats, isLoading } = useTradingStats();

  if (isLoading || !stats) {
    return (
      <div className="flex items-center justify-center p-8 bg-black border border-gray-800 rounded">
        <div className="text-gray-500 text-sm">
          {isLoading ? (
            <div className="animate-spin rounded-full h-6 w-6 border-b-2 border-blue-500"></div>
          ) : (
            'No statistics available'
          )}
        </div>
      </div>
    );
  }

  const statCards = [
    {
      label: 'Total P&L',
      value: `${stats.totalPnL >= 0 ? '+' : ''}$${stats.totalPnL.toFixed(2)}`,
      color: stats.totalPnL >= 0 ? 'text-green-500' : 'text-red-500',
      icon: stats.totalPnL >= 0 ? TrendingUp : TrendingDown,
      bgColor: stats.totalPnL >= 0 ? 'bg-green-500/10' : 'bg-red-500/10',
    },
    {
      label: 'Return',
      value: `${stats.returnPercent >= 0 ? '+' : ''}${stats.returnPercent.toFixed(2)}%`,
      color: stats.returnPercent >= 0 ? 'text-green-500' : 'text-red-500',
      icon: Target,
      bgColor: stats.returnPercent >= 0 ? 'bg-green-500/10' : 'bg-red-500/10',
    },
    {
      label: 'Win Rate',
      value: `${stats.winRate.toFixed(1)}%`,
      color: stats.winRate >= 50 ? 'text-green-500' : 'text-yellow-500',
      icon: Target,
      bgColor: stats.winRate >= 50 ? 'bg-green-500/10' : 'bg-yellow-500/10',
    },
    {
      label: 'Total Trades',
      value: stats.totalTrades.toString(),
      color: 'text-blue-500',
      icon: DollarSign,
      bgColor: 'bg-blue-500/10',
    },
    {
      label: 'Winning Trades',
      value: stats.winningTrades.toString(),
      color: 'text-green-500',
      icon: TrendingUp,
      bgColor: 'bg-green-500/10',
    },
    {
      label: 'Losing Trades',
      value: stats.losingTrades.toString(),
      color: 'text-red-500',
      icon: TrendingDown,
      bgColor: 'bg-red-500/10',
    },
    {
      label: 'Realized P&L',
      value: `${stats.realizedPnL >= 0 ? '+' : ''}$${stats.realizedPnL.toFixed(2)}`,
      color: stats.realizedPnL >= 0 ? 'text-green-500' : 'text-red-500',
      icon: DollarSign,
      bgColor: 'bg-gray-800',
    },
    {
      label: 'Unrealized P&L',
      value: `${stats.unrealizedPnL >= 0 ? '+' : ''}$${stats.unrealizedPnL.toFixed(2)}`,
      color: stats.unrealizedPnL >= 0 ? 'text-green-500' : 'text-red-500',
      icon: DollarSign,
      bgColor: 'bg-gray-800',
    },
  ];

  return (
    <div className="space-y-4">
      {/* Main Stats Grid */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
        {statCards.map((stat, idx) => {
          const Icon = stat.icon;
          return (
            <div
              key={idx}
              className={`${stat.bgColor} border border-gray-800 rounded p-3 transition-all hover:border-gray-700`}
            >
              <div className="flex items-center justify-between mb-2">
                <span className="text-xs text-gray-400 font-medium">{stat.label}</span>
                <Icon className={`w-4 h-4 ${stat.color}`} />
              </div>
              <div className={`text-lg font-bold font-mono ${stat.color}`}>{stat.value}</div>
            </div>
          );
        })}
      </div>

      {/* Advanced Stats */}
      {stats.averageWin !== undefined && (
        <div className="grid grid-cols-2 md:grid-cols-3 gap-3">
          {stats.averageWin !== undefined && (
            <div className="bg-gray-900 border border-gray-800 rounded p-3">
              <div className="text-xs text-gray-400 mb-1">Avg Win</div>
              <div className="text-sm font-mono text-green-500">${stats.averageWin.toFixed(2)}</div>
            </div>
          )}
          {stats.averageLoss !== undefined && (
            <div className="bg-gray-900 border border-gray-800 rounded p-3">
              <div className="text-xs text-gray-400 mb-1">Avg Loss</div>
              <div className="text-sm font-mono text-red-500">${stats.averageLoss.toFixed(2)}</div>
            </div>
          )}
          {stats.profitFactor !== undefined && (
            <div className="bg-gray-900 border border-gray-800 rounded p-3">
              <div className="text-xs text-gray-400 mb-1">Profit Factor</div>
              <div className="text-sm font-mono text-blue-500">{stats.profitFactor.toFixed(2)}</div>
            </div>
          )}
          {stats.largestWin !== undefined && (
            <div className="bg-gray-900 border border-gray-800 rounded p-3">
              <div className="text-xs text-gray-400 mb-1">Largest Win</div>
              <div className="text-sm font-mono text-green-500">${stats.largestWin.toFixed(2)}</div>
            </div>
          )}
          {stats.largestLoss !== undefined && (
            <div className="bg-gray-900 border border-gray-800 rounded p-3">
              <div className="text-xs text-gray-400 mb-1">Largest Loss</div>
              <div className="text-sm font-mono text-red-500">${stats.largestLoss.toFixed(2)}</div>
            </div>
          )}
          {stats.sharpeRatio !== undefined && (
            <div className="bg-gray-900 border border-gray-800 rounded p-3">
              <div className="text-xs text-gray-400 mb-1">Sharpe Ratio</div>
              <div className="text-sm font-mono text-purple-500">{stats.sharpeRatio.toFixed(2)}</div>
            </div>
          )}
        </div>
      )}

      {/* Total Fees */}
      <div className="bg-gray-900 border border-gray-800 rounded p-3">
        <div className="flex items-center justify-between">
          <span className="text-xs text-gray-400">Total Fees Paid</span>
          <span className="text-sm font-mono text-orange-500">${stats.totalFees.toFixed(2)}</span>
        </div>
      </div>
    </div>
  );
}
