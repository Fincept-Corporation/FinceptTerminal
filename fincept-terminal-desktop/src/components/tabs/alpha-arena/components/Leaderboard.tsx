/**
 * Leaderboard Component
 *
 * Displays the competition leaderboard with rankings and performance metrics.
 */

import React from 'react';
import { Trophy, TrendingUp, TrendingDown, Minus } from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import type { LeaderboardEntry } from '../types';
import { formatCurrency, formatPercent, getModelColor } from '../types';

interface LeaderboardProps {
  entries: LeaderboardEntry[];
  cycleNumber?: number;
  isLoading?: boolean;
}

const Leaderboard: React.FC<LeaderboardProps> = ({
  entries,
  cycleNumber = 0,
  isLoading = false,
}) => {
  // Defensive guard: ensure entries is an array
  if (!Array.isArray(entries)) {
    return (
      <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
        <div className="flex items-center gap-2 mb-4">
          <Trophy className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Leaderboard</h3>
        </div>
        <p className="text-[#787878] text-center py-8">No data available</p>
      </div>
    );
  }

  if (isLoading) {
    return (
      <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
        <div className="flex items-center gap-2 mb-4">
          <Trophy className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Leaderboard</h3>
        </div>
        <div className="space-y-2">
          {[1, 2, 3].map((i) => (
            <div key={i} className="h-12 bg-[#1A1A1A] rounded animate-pulse" />
          ))}
        </div>
      </div>
    );
  }

  if (entries.length === 0) {
    return (
      <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
        <div className="flex items-center gap-2 mb-4">
          <Trophy className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Leaderboard</h3>
        </div>
        <p className="text-[#787878] text-center py-8">
          No competition data yet. Create and run a competition to see results.
        </p>
      </div>
    );
  }

  return (
    <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <Trophy className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Leaderboard</h3>
        </div>
        <span className="text-sm text-[#787878]">Cycle {cycleNumber}</span>
      </div>

      <div className="space-y-2">
        {entries.map((entry) => (
          <LeaderboardRow key={entry.model_name} entry={entry} />
        ))}
      </div>
    </div>
  );
};

interface LeaderboardRowProps {
  entry: LeaderboardEntry;
}

const LeaderboardRow: React.FC<LeaderboardRowProps> = ({ entry }) => {
  const totalPnl = Number.isFinite(entry.total_pnl) ? entry.total_pnl : 0;
  const returnPct = Number.isFinite(entry.return_pct) ? entry.return_pct : 0;
  const portfolioValue = Number.isFinite(entry.portfolio_value) ? entry.portfolio_value : 0;
  const winRate = entry.win_rate !== undefined && Number.isFinite(entry.win_rate) ? entry.win_rate : undefined;
  const isPositive = totalPnl >= 0;
  const modelColor = getModelColor(entry.model_name);

  // Rank badge styling
  const getRankBadge = (rank: number) => {
    if (rank === 1) return 'bg-yellow-500/20 text-yellow-400 border-yellow-500/30';
    if (rank === 2) return 'bg-gray-400/20 text-gray-300 border-gray-400/30';
    if (rank === 3) return 'bg-orange-600/20 text-orange-400 border-orange-500/30';
    return 'bg-[#1A1A1A] text-[#787878] border-[#2A2A2A]';
  };

  return (
    <div className="flex items-center gap-3 p-3 bg-[#1A1A1A] rounded-lg hover:bg-[#222222] transition-colors">
      {/* Rank */}
      <div
        className={`w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold border ${getRankBadge(entry.rank)}`}
      >
        {entry.rank}
      </div>

      {/* Model indicator */}
      <div
        className="w-1 h-10 rounded-full"
        style={{ backgroundColor: modelColor }}
      />

      {/* Model name and stats */}
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2">
          <span className="text-white font-medium truncate">
            {entry.model_name}
          </span>
          {entry.rank === 1 && <Trophy className="w-4 h-4 text-yellow-400" />}
        </div>
        <div className="flex items-center gap-4 text-xs text-[#787878]">
          <span>{entry.trades_count} trades</span>
          <span>{entry.positions_count} positions</span>
          {winRate !== undefined && (
            <span>{(winRate * 100).toFixed(0)}% win rate</span>
          )}
        </div>
      </div>

      {/* P&L */}
      <div className="text-right">
        <div className="flex items-center gap-1">
          {isPositive ? (
            <TrendingUp className="w-4 h-4 text-[#00D66F]" />
          ) : totalPnl === 0 ? (
            <Minus className="w-4 h-4 text-[#787878]" />
          ) : (
            <TrendingDown className="w-4 h-4 text-[#FF3B3B]" />
          )}
          <span className={isPositive ? 'text-[#00D66F]' : totalPnl === 0 ? 'text-[#787878]' : 'text-[#FF3B3B]'}>
            {formatPercent(returnPct)}
          </span>
        </div>
        <span className="text-xs text-[#787878]">
          {formatCurrency(portfolioValue)}
        </span>
      </div>
    </div>
  );
};

export default withErrorBoundary(Leaderboard, { name: 'AlphaArena.Leaderboard' });
