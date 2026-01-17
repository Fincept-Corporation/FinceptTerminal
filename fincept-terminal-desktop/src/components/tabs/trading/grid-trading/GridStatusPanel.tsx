/**
 * Grid Status Panel
 *
 * Shows grid status, statistics, and controls for managing running grids.
 */

import React from 'react';
import {
  Grid, Play, Pause, Square, Trash2, TrendingUp, TrendingDown,
  DollarSign, Activity, BarChart3, Clock, AlertCircle, RefreshCw,
} from 'lucide-react';
import type { GridState } from '../../../../services/gridTrading/types';

interface GridStatusPanelProps {
  grid: GridState;
  onStart: () => void;
  onPause: () => void;
  onStop: () => void;
  onDelete: () => void;
}

export function GridStatusPanel({
  grid,
  onStart,
  onPause,
  onStop,
  onDelete,
}: GridStatusPanelProps) {
  const { config, status, currentPrice, levels } = grid;

  // Calculate statistics
  const openOrders = levels.filter(l => l.status === 'open').length;
  const filledOrders = levels.filter(l => l.status === 'filled').length;
  const pendingOrders = levels.filter(l => l.status === 'pending').length;
  const buyOrders = levels.filter(l => l.side === 'buy' && l.status === 'open').length;
  const sellOrders = levels.filter(l => l.side === 'sell' && l.status === 'open').length;

  // Price position in grid
  const pricePosition = ((currentPrice - config.lowerPrice) / (config.upperPrice - config.lowerPrice)) * 100;

  // Status colors
  const getStatusColor = () => {
    switch (status) {
      case 'running': return '#00D66F';
      case 'paused': return '#FFD700';
      case 'stopped': return '#787878';
      case 'error': return '#FF3B3B';
      default: return '#787878';
    }
  };

  const formatDuration = (ms: number): string => {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) return `${days}d ${hours % 24}h`;
    if (hours > 0) return `${hours}h ${minutes % 60}m`;
    if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
    return `${seconds}s`;
  };

  const runningTime = grid.startedAt ? Date.now() - grid.startedAt : 0;

  return (
    <div className="bg-[#0F0F0F] border border-[#2A2A2A] rounded-lg p-4">
      {/* Header */}
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <Grid className="w-5 h-5 text-[#FF8800]" />
          <span className="text-white font-semibold">{config.symbol}</span>
          <div
            className="px-2 py-0.5 rounded text-xs font-medium"
            style={{
              backgroundColor: `${getStatusColor()}20`,
              color: getStatusColor(),
            }}
          >
            {status.toUpperCase()}
          </div>
        </div>
        <div className="text-xs text-[#787878]">
          {config.brokerId}
        </div>
      </div>

      {/* Controls */}
      <div className="flex gap-2 mb-4">
        {(status === 'draft' || status === 'paused') && (
          <button
            onClick={onStart}
            className="flex-1 flex items-center justify-center gap-1 py-2 bg-[#00D66F] hover:bg-[#00E57F] text-black font-semibold rounded transition-colors"
          >
            <Play className="w-4 h-4" />
            {status === 'draft' ? 'Start' : 'Resume'}
          </button>
        )}
        {status === 'running' && (
          <button
            onClick={onPause}
            className="flex-1 flex items-center justify-center gap-1 py-2 bg-[#FFD700] hover:bg-[#FFE030] text-black font-semibold rounded transition-colors"
          >
            <Pause className="w-4 h-4" />
            Pause
          </button>
        )}
        {(status === 'running' || status === 'paused') && (
          <button
            onClick={onStop}
            className="flex-1 flex items-center justify-center gap-1 py-2 bg-[#1A1A1A] hover:bg-[#2A2A2A] text-[#FF3B3B] border border-[#FF3B3B]/30 rounded transition-colors"
          >
            <Square className="w-4 h-4" />
            Stop
          </button>
        )}
        <button
          onClick={onDelete}
          className="p-2 bg-[#1A1A1A] hover:bg-[#2A2A2A] text-[#FF3B3B] border border-[#2A2A2A] hover:border-[#FF3B3B]/30 rounded transition-colors"
        >
          <Trash2 className="w-4 h-4" />
        </button>
      </div>

      {/* Price Info */}
      <div className="bg-[#1A1A1A] rounded p-3 mb-4">
        <div className="flex items-center justify-between mb-2">
          <span className="text-xs text-[#787878]">Current Price</span>
          <span className="text-lg text-white font-mono">${currentPrice.toFixed(2)}</span>
        </div>
        {/* Price position bar */}
        <div className="relative h-2 bg-[#2A2A2A] rounded-full overflow-hidden">
          <div
            className="absolute top-0 left-0 h-full bg-gradient-to-r from-[#FF3B3B] via-[#FFD700] to-[#00D66F]"
            style={{ width: '100%' }}
          />
          <div
            className="absolute top-0 h-full w-1 bg-white shadow-lg"
            style={{ left: `${Math.max(0, Math.min(100, pricePosition))}%`, transform: 'translateX(-50%)' }}
          />
        </div>
        <div className="flex justify-between mt-1 text-xs text-[#787878]">
          <span>${config.lowerPrice.toFixed(2)}</span>
          <span>${config.upperPrice.toFixed(2)}</span>
        </div>
      </div>

      {/* Statistics Grid */}
      <div className="grid grid-cols-2 gap-3 mb-4">
        {/* P&L */}
        <div className="bg-[#1A1A1A] rounded p-3">
          <div className="flex items-center gap-1 text-xs text-[#787878] mb-1">
            <DollarSign className="w-3 h-3" />
            Total P&L
          </div>
          <div className={`text-lg font-mono ${grid.totalPnl >= 0 ? 'text-[#00D66F]' : 'text-[#FF3B3B]'}`}>
            {grid.totalPnl >= 0 ? '+' : ''}{grid.totalPnl.toFixed(2)}
          </div>
          <div className="text-xs text-[#787878]">
            Realized: {grid.realizedPnl >= 0 ? '+' : ''}{grid.realizedPnl.toFixed(2)}
          </div>
        </div>

        {/* Grid Profit */}
        <div className="bg-[#1A1A1A] rounded p-3">
          <div className="flex items-center gap-1 text-xs text-[#787878] mb-1">
            <BarChart3 className="w-3 h-3" />
            Grid Profit
          </div>
          <div className="text-lg font-mono text-[#00D66F]">
            +{grid.gridProfit.toFixed(2)}
          </div>
          <div className="text-xs text-[#787878]">
            {grid.totalBuys + grid.totalSells} trades
          </div>
        </div>

        {/* Position */}
        <div className="bg-[#1A1A1A] rounded p-3">
          <div className="flex items-center gap-1 text-xs text-[#787878] mb-1">
            <Activity className="w-3 h-3" />
            Position
          </div>
          <div className={`text-lg font-mono ${
            grid.currentPosition > 0 ? 'text-[#00D66F]' :
            grid.currentPosition < 0 ? 'text-[#FF3B3B]' : 'text-white'
          }`}>
            {grid.currentPosition > 0 ? '+' : ''}{grid.currentPosition.toFixed(4)}
          </div>
          <div className="text-xs text-[#787878]">
            Avg: ${grid.averageEntryPrice.toFixed(2)}
          </div>
        </div>

        {/* Running Time */}
        <div className="bg-[#1A1A1A] rounded p-3">
          <div className="flex items-center gap-1 text-xs text-[#787878] mb-1">
            <Clock className="w-3 h-3" />
            Running Time
          </div>
          <div className="text-lg font-mono text-white">
            {formatDuration(runningTime)}
          </div>
          <div className="text-xs text-[#787878]">
            Started: {grid.startedAt ? new Date(grid.startedAt).toLocaleDateString() : 'N/A'}
          </div>
        </div>
      </div>

      {/* Order Status */}
      <div className="bg-[#1A1A1A] rounded p-3 mb-4">
        <div className="text-xs text-[#787878] mb-2">Order Status</div>
        <div className="grid grid-cols-4 gap-2 text-center">
          <div>
            <div className="text-lg font-mono text-[#00E5FF]">{openOrders}</div>
            <div className="text-xs text-[#787878]">Open</div>
          </div>
          <div>
            <div className="text-lg font-mono text-[#00D66F]">{filledOrders}</div>
            <div className="text-xs text-[#787878]">Filled</div>
          </div>
          <div>
            <div className="text-lg font-mono text-[#787878]">{pendingOrders}</div>
            <div className="text-xs text-[#787878]">Pending</div>
          </div>
          <div>
            <div className="text-lg font-mono text-white">{config.gridLevels}</div>
            <div className="text-xs text-[#787878]">Total</div>
          </div>
        </div>
        <div className="flex justify-center gap-4 mt-2 text-xs">
          <div className="flex items-center gap-1">
            <TrendingUp className="w-3 h-3 text-[#00D66F]" />
            <span className="text-[#787878]">{buyOrders} buys open</span>
          </div>
          <div className="flex items-center gap-1">
            <TrendingDown className="w-3 h-3 text-[#FF3B3B]" />
            <span className="text-[#787878]">{sellOrders} sells open</span>
          </div>
        </div>
      </div>

      {/* Error Display */}
      {grid.lastError && (
        <div className="bg-[#FF3B3B]/10 border border-[#FF3B3B]/30 rounded p-3">
          <div className="flex items-center gap-2 text-[#FF3B3B] text-sm">
            <AlertCircle className="w-4 h-4" />
            Last Error
          </div>
          <div className="text-xs text-[#FF3B3B] mt-1">{grid.lastError}</div>
          <div className="text-xs text-[#787878] mt-1">
            Total errors: {grid.errorCount}
          </div>
        </div>
      )}

      {/* Grid Config Summary */}
      <div className="mt-4 pt-4 border-t border-[#2A2A2A]">
        <div className="text-xs text-[#787878] mb-2">Grid Configuration</div>
        <div className="grid grid-cols-2 gap-1 text-xs">
          <div className="flex justify-between">
            <span className="text-[#787878]">Type:</span>
            <span className="text-white capitalize">{config.gridType}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[#787878]">Direction:</span>
            <span className="text-white capitalize">{config.direction}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[#787878]">Investment:</span>
            <span className="text-white">${config.totalInvestment}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-[#787878]">Qty/Grid:</span>
            <span className="text-white">{config.quantityPerGrid}</span>
          </div>
          {config.stopLoss && (
            <div className="flex justify-between">
              <span className="text-[#787878]">Stop Loss:</span>
              <span className="text-[#FF3B3B]">${config.stopLoss}</span>
            </div>
          )}
          {config.takeProfit && (
            <div className="flex justify-between">
              <span className="text-[#787878]">Take Profit:</span>
              <span className="text-[#00D66F]">${config.takeProfit}</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

export default GridStatusPanel;
