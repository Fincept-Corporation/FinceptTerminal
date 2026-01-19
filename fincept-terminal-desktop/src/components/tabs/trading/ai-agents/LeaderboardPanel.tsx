/**
 * Leaderboard Panel
 *
 * Model performance tracking and leaderboard (Alpha Arena style)
 * Shows win rate, P&L, trades, and rankings for all competing models
 */

import React, { useState, useEffect } from 'react';
import { Trophy, TrendingUp, TrendingDown, Award, Target, Percent, DollarSign } from 'lucide-react';
import type { ModelPerformance } from '../../../../services/trading/agnoTradingService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface LeaderboardPanelProps {
  refreshInterval?: number;
}

export function LeaderboardPanel({ refreshInterval = 10000 }: LeaderboardPanelProps) {
  const [leaderboard, setLeaderboard] = useState<ModelPerformance[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [sortBy, setSortBy] = useState<'pnl' | 'winrate' | 'trades'>('pnl');

  const fetchLeaderboard = async () => {
    try {
      setIsLoading(true);
      const { invoke } = await import('@tauri-apps/api/core');
      const response = await invoke('agno_get_leaderboard');
      const data = JSON.parse(response as string);

      if (data.success && data.data?.leaderboard) {
        setLeaderboard(data.data.leaderboard);
      }
    } catch (err) {
      console.error('Failed to fetch leaderboard:', err);
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    fetchLeaderboard();
    const interval = setInterval(fetchLeaderboard, refreshInterval);
    return () => clearInterval(interval);
  }, [refreshInterval]);

  const sortedLeaderboard = [...leaderboard].sort((a, b) => {
    switch (sortBy) {
      case 'pnl':
        return b.total_pnl - a.total_pnl;
      case 'winrate':
        return b.win_rate - a.win_rate;
      case 'trades':
        return b.total_trades - a.total_trades;
      default:
        return 0;
    }
  });

  const getRankColor = (rank: number) => {
    switch (rank) {
      case 1: return FINCEPT.YELLOW;
      case 2: return FINCEPT.CYAN;
      case 3: return FINCEPT.ORANGE;
      default: return FINCEPT.GRAY;
    }
  };

  const getModelColor = (model: string) => {
    const colors = [FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.PURPLE, FINCEPT.YELLOW];
    const hash = model.split('').reduce((acc, char) => acc + char.charCodeAt(0), 0);
    return colors[hash % colors.length];
  };

  return (
    <div style={{
      height: '100%',
      background: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        background: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '8px 10px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Trophy size={14} color={FINCEPT.YELLOW} />
          <span style={{
            color: FINCEPT.WHITE,
            fontSize: '11px',
            fontWeight: '600',
            letterSpacing: '0.5px'
          }}>
            LEADERBOARD
          </span>
          {isLoading && (
            <div style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: FINCEPT.ORANGE,
              animation: 'pulse 2s infinite'
            }} />
          )}
        </div>

        {/* Sort Controls */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {[
            { value: 'pnl' as const, label: 'P&L' },
            { value: 'winrate' as const, label: 'WIN%' },
            { value: 'trades' as const, label: 'TRADES' }
          ].map(option => (
            <button
              key={option.value}
              onClick={() => setSortBy(option.value)}
              style={{
                background: sortBy === option.value ? FINCEPT.ORANGE : 'transparent',
                border: `1px solid ${sortBy === option.value ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: sortBy === option.value ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                padding: '2px 6px',
                borderRadius: '2px',
                fontSize: '8px',
                fontWeight: '700',
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
            >
              {option.label}
            </button>
          ))}
        </div>
      </div>

      {/* Leaderboard Table */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '10px'
      }}>
        {sortedLeaderboard.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: FINCEPT.GRAY,
            fontSize: '10px',
            marginTop: '40px'
          }}>
            {isLoading ? 'Loading leaderboard...' : 'No models yet. Create a competition to start tracking.'}
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {sortedLeaderboard.map((model, idx) => (
              <ModelRankCard
                key={model.model}
                model={model}
                rank={idx + 1}
                rankColor={getRankColor(idx + 1)}
                modelColor={getModelColor(model.model)}
              />
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

interface ModelRankCardProps {
  model: ModelPerformance;
  rank: number;
  rankColor: string;
  modelColor: string;
}

function ModelRankCard({ model, rank, rankColor, modelColor }: ModelRankCardProps) {
  const isProfitable = model.total_pnl >= 0;

  return (
    <div style={{
      background: FINCEPT.DARK_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderLeft: `3px solid ${modelColor}`,
      borderRadius: '2px',
      padding: '8px 10px',
      transition: 'all 0.2s'
    }}>
      {/* Header: Rank + Model Name */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '8px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <div style={{
            width: '20px',
            height: '20px',
            borderRadius: '50%',
            background: rank <= 3 ? `${rankColor}20` : 'transparent',
            border: `2px solid ${rankColor}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '10px',
            fontWeight: '700',
            color: rankColor
          }}>
            {rank}
          </div>
          <span style={{
            color: modelColor,
            fontSize: '10px',
            fontWeight: '700',
            fontFamily: '"IBM Plex Mono", monospace',
            textTransform: 'uppercase'
          }}>
            {model.model.split(':')[1] || model.model}
          </span>
        </div>

        {/* Total P&L Badge */}
        <div style={{
          background: isProfitable ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
          border: `1px solid ${isProfitable ? FINCEPT.GREEN : FINCEPT.RED}`,
          borderRadius: '2px',
          padding: '2px 6px',
          display: 'flex',
          alignItems: 'center',
          gap: '3px'
        }}>
          {isProfitable ? (
            <TrendingUp size={10} color={FINCEPT.GREEN} />
          ) : (
            <TrendingDown size={10} color={FINCEPT.RED} />
          )}
          <span style={{
            color: isProfitable ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '9px',
            fontWeight: '700'
          }}>
            {isProfitable ? '+' : ''}{model.total_pnl.toFixed(2)}
          </span>
        </div>
      </div>

      {/* Stats Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 1fr)',
        gap: '8px',
        fontSize: '9px',
        fontFamily: '"IBM Plex Mono", monospace'
      }}>
        {/* Win Rate */}
        <div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '2px' }}>WIN RATE</div>
          <div style={{
            color: model.win_rate >= 50 ? FINCEPT.GREEN : FINCEPT.RED,
            fontWeight: '700',
            fontSize: '10px'
          }}>
            {model.win_rate.toFixed(1)}%
          </div>
        </div>

        {/* Total Trades */}
        <div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '2px' }}>TRADES</div>
          <div style={{ color: FINCEPT.CYAN, fontWeight: '700', fontSize: '10px' }}>
            {model.total_trades}
          </div>
        </div>

        {/* Daily P&L */}
        <div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '7px', marginBottom: '2px' }}>TODAY</div>
          <div style={{
            color: model.daily_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontWeight: '700',
            fontSize: '10px'
          }}>
            {model.daily_pnl >= 0 ? '+' : ''}{model.daily_pnl.toFixed(2)}
          </div>
        </div>
      </div>

      {/* Win/Loss Breakdown */}
      <div style={{
        marginTop: '8px',
        paddingTop: '6px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        fontSize: '8px',
        fontFamily: '"IBM Plex Mono", monospace'
      }}>
        <div style={{ display: 'flex', gap: '8px' }}>
          <span style={{ color: FINCEPT.GRAY }}>W: </span>
          <span style={{ color: FINCEPT.GREEN, fontWeight: '600' }}>{model.winning_trades}</span>
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <span style={{ color: FINCEPT.GRAY }}>L: </span>
          <span style={{ color: FINCEPT.RED, fontWeight: '600' }}>{model.losing_trades}</span>
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <span style={{ color: FINCEPT.GRAY }}>POS: </span>
          <span style={{ color: FINCEPT.YELLOW, fontWeight: '600' }}>{model.positions}</span>
        </div>
      </div>
    </div>
  );
}
