import React from 'react';
import { Trophy, ExternalLink, Zap } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { alphaArenaService, LeaderboardEntry } from '@/services/trading/alphaArenaService';
import { useCache } from '@/hooks/useCache';
import { invoke } from '@tauri-apps/api/core';

interface AlphaArenaWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface ArenaData {
  leaderboard: LeaderboardEntry[];
  isRunning: boolean;
  totalAgents: number;
}

export const AlphaArenaWidget: React.FC<AlphaArenaWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<ArenaData>({
    key: 'widget:alpha-arena',
    category: 'trading',
    ttl: '1m',
    refetchInterval: 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        const raw = await invoke<string>('get_competition_state');
        const state = JSON.parse(raw);
        const leaderboard: LeaderboardEntry[] = state?.data?.leaderboard ?? [];
        const isRunning: boolean = state?.data?.is_running ?? false;
        return {
          leaderboard: leaderboard.slice(0, 6),
          isRunning,
          totalAgents: leaderboard.length,
        };
      } catch {
        return { leaderboard: [], isRunning: false, totalAgents: 0 };
      }
    }
  });

  const topAgent = data?.leaderboard[0];

  return (
    <BaseWidget
      id={id}
      title="ALPHA ARENA"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-warning)"
    >
      {(!data || data.leaderboard.length === 0) ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          <Trophy size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
          <div>No active competition</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>Start a competition in Alpha Arena</div>
        </div>
      ) : (
        <div style={{ padding: '4px' }}>
          {/* Status + Leader */}
          <div style={{ padding: '8px', backgroundColor: 'var(--ft-color-panel)', margin: '4px', borderRadius: '2px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <div style={{ width: '8px', height: '8px', borderRadius: '50%', backgroundColor: data.isRunning ? 'var(--ft-color-success)' : 'var(--ft-color-text-muted)', boxShadow: data.isRunning ? '0 0 6px var(--ft-color-success)' : 'none' }} />
              <span style={{ fontSize: 'var(--ft-font-size-tiny)', color: data.isRunning ? 'var(--ft-color-success)' : 'var(--ft-color-text-muted)', fontWeight: 'bold' }}>
                {data.isRunning ? 'LIVE' : 'IDLE'}
              </span>
              <span style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{data.totalAgents} agents</span>
            </div>
            {topAgent && (
              <div style={{ textAlign: 'right' }}>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>LEADER</div>
                <div style={{ fontSize: 'var(--ft-font-size-tiny)', fontWeight: 'bold', color: 'var(--ft-color-warning)' }}>
                  {topAgent.model.length > 14 ? topAgent.model.substring(0, 14) + '...' : topAgent.model}
                </div>
              </div>
            )}
          </div>

          {/* Header */}
          <div style={{ display: 'grid', gridTemplateColumns: '20px 1fr 65px 55px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
            <span>#</span>
            <span>MODEL</span>
            <span style={{ textAlign: 'right' }}>P&L</span>
            <span style={{ textAlign: 'right' }}>RETURN</span>
          </div>

          {data.leaderboard.map((entry, i) => {
            const pos = entry.pnl >= 0;
            const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
            const rankColor = i === 0 ? 'var(--ft-color-warning)' : i === 1 ? '#C0C0C0' : i === 2 ? '#CD7F32' : 'var(--ft-color-text-muted)';
            return (
              <div key={entry.model} style={{ display: 'grid', gridTemplateColumns: '20px 1fr 65px 55px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
                <span style={{ fontSize: 'var(--ft-font-size-tiny)', fontWeight: 'bold', color: rankColor }}>#{i + 1}</span>
                <span style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {entry.model}
                </span>
                <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-tiny)', fontWeight: 'bold', color: clr }}>
                  {pos ? '+' : ''}${Math.abs(entry.pnl).toFixed(0)}
                </span>
                <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-tiny)', color: clr }}>
                  {pos ? '+' : ''}{entry.return_pct.toFixed(1)}%
                </span>
              </div>
            );
          })}

          {onNavigate && (
            <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-warning)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
              <span>OPEN ALPHA ARENA</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
