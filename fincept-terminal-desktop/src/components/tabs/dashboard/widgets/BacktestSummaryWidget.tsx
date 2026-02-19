import React from 'react';
import { BarChart2, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';
import { useCache } from '@/hooks/useCache';

interface BacktestSummaryWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface BacktestRun {
  id: string;
  strategy_name: string;
  symbol?: string;
  provider: string;
  total_return?: number;
  sharpe_ratio?: number;
  max_drawdown?: number;
  win_rate?: number;
  total_trades?: number;
  created_at: string;
}

interface BacktestData {
  runs: BacktestRun[];
}

export const BacktestSummaryWidget: React.FC<BacktestSummaryWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<BacktestData>({
    key: 'widget:backtest-summary',
    category: 'backtesting',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        // db_get_backtest_runs returns Vec<BacktestRun> directly (not wrapped)
        const runs = await invoke<BacktestRun[]>('db_get_backtest_runs', { limit: 5 });
        return { runs: Array.isArray(runs) ? runs.slice(0, 5) : [] };
      } catch {
        return { runs: [] };
      }
    }
  });

  const formatDate = (d: string) => {
    const date = new Date(d);
    return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
  };

  return (
    <BaseWidget
      id={id}
      title="BACKTEST RESULTS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-primary)"
    >
      {(!data || data.runs.length === 0) ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          <BarChart2 size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
          <div>No backtest runs found</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>Run a backtest to see results</div>
        </div>
      ) : (
        <div style={{ padding: '4px' }}>
          {/* Header */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 55px 50px 45px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
            <span>STRATEGY</span>
            <span style={{ textAlign: 'right' }}>RETURN</span>
            <span style={{ textAlign: 'right' }}>SHARPE</span>
            <span style={{ textAlign: 'right' }}>DATE</span>
          </div>

          {data.runs.map((run) => {
            const pos = (run.total_return ?? 0) >= 0;
            const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
            return (
              <div key={run.id} style={{ display: 'grid', gridTemplateColumns: '1fr 55px 50px 45px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
                <div>
                  <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {run.strategy_name}
                  </div>
                  {run.symbol && (
                    <div style={{ fontSize: '9px', color: 'var(--ft-color-primary)' }}>{run.symbol} · {run.provider}</div>
                  )}
                </div>
                <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-tiny)', fontWeight: 'bold', color: run.total_return != null ? clr : 'var(--ft-color-text-muted)' }}>
                  {run.total_return != null ? `${pos ? '+' : ''}${run.total_return.toFixed(1)}%` : '—'}
                </span>
                <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text)' }}>
                  {run.sharpe_ratio != null ? run.sharpe_ratio.toFixed(2) : '—'}
                </span>
                <span style={{ textAlign: 'right', fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
                  {formatDate(run.created_at)}
                </span>
              </div>
            );
          })}

          {onNavigate && (
            <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
              <span>OPEN BACKTESTING</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
