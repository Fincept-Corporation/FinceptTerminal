import React from 'react';
import { Bot, ExternalLink, Play, Square, AlertCircle } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';
import { useCache } from '@/hooks/useCache';

interface AlgoStatusWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface AlgoDeployment {
  id: string;
  strategy_name: string | null;
  symbol: string;
  provider: string;
  mode: 'paper' | 'live';
  status: 'pending' | 'starting' | 'running' | 'stopped' | 'error';
  timeframe: string;
  quantity: number;
}

interface AlgoData {
  deployments: AlgoDeployment[];
  running: number;
  stopped: number;
  error: number;
}

const STATUS_COLORS: Record<string, string> = {
  running: 'var(--ft-color-success)',
  starting: 'var(--ft-color-warning)',
  pending: 'var(--ft-color-warning)',
  stopped: 'var(--ft-color-text-muted)',
  error: 'var(--ft-color-alert)',
};

const STATUS_ICONS: Record<string, React.ReactNode> = {
  running: <Play size={10} />,
  starting: <Play size={10} />,
  pending: <Play size={10} />,
  stopped: <Square size={10} />,
  error: <AlertCircle size={10} />,
};

export const AlgoStatusWidget: React.FC<AlgoStatusWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<AlgoData>({
    key: 'widget:algo-status',
    category: 'trading',
    ttl: '1m',
    refetchInterval: 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        const raw = await invoke<string>('list_algo_deployments');
        const result = JSON.parse(raw);
        const deployments: AlgoDeployment[] = result?.data ?? [];
        return {
          deployments: deployments.slice(0, 8),
          running: deployments.filter(d => d.status === 'running').length,
          stopped: deployments.filter(d => d.status === 'stopped').length,
          error: deployments.filter(d => d.status === 'error').length,
        };
      } catch {
        return { deployments: [], running: 0, stopped: 0, error: 0 };
      }
    }
  });

  const headerColor = data && data.error > 0 ? 'var(--ft-color-alert)'
    : data && data.running > 0 ? 'var(--ft-color-success)'
    : 'var(--ft-color-text-muted)';

  return (
    <BaseWidget
      id={id}
      title="ALGO STATUS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor={headerColor}
    >
      {(!data || data.deployments.length === 0) ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          <Bot size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
          <div>No algo strategies deployed</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>Deploy a strategy in Algo Trading</div>
        </div>
      ) : (
        <div style={{ padding: '4px' }}>
          {/* Stats */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px', margin: '4px', marginBottom: '8px' }}>
            {[
              { label: 'RUNNING', value: data.running, color: 'var(--ft-color-success)' },
              { label: 'STOPPED', value: data.stopped, color: 'var(--ft-color-text-muted)' },
              { label: 'ERROR', value: data.error, color: 'var(--ft-color-alert)' },
            ].map(s => (
              <div key={s.label} style={{ backgroundColor: 'var(--ft-color-panel)', padding: '6px', borderRadius: '2px', textAlign: 'center' }}>
                <div style={{ fontSize: '18px', fontWeight: 'bold', color: s.color }}>{s.value}</div>
                <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>{s.label}</div>
              </div>
            ))}
          </div>

          {/* Header */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 60px 50px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
            <span>STRATEGY</span>
            <span style={{ textAlign: 'center' }}>SYMBOL</span>
            <span style={{ textAlign: 'right' }}>STATUS</span>
          </div>

          {data.deployments.map(d => {
            const clr = STATUS_COLORS[d.status] || 'var(--ft-color-text-muted)';
            return (
              <div key={d.id} style={{ display: 'grid', gridTemplateColumns: '1fr 60px 50px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
                <div>
                  <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {d.strategy_name || 'Unknown'}
                  </div>
                  <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
                    {d.mode.toUpperCase()} · {d.timeframe}
                  </div>
                </div>
                <span style={{ textAlign: 'center', fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: 'var(--ft-color-primary)' }}>{d.symbol}</span>
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '3px', color: clr, fontSize: '9px', fontWeight: 'bold' }}>
                  {STATUS_ICONS[d.status]}
                  {d.status.toUpperCase()}
                </div>
              </div>
            );
          })}

          {onNavigate && (
            <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-success)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
              <span>OPEN ALGO TRADING</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
