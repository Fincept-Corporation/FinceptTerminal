import React from 'react';
import { Cpu, ExternalLink, ArrowUpCircle, ArrowDownCircle, MinusCircle } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';
import { useCache } from '@/hooks/useCache';

interface LiveSignalsWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface Signal {
  symbol: string;
  action: 'BUY' | 'SELL' | 'HOLD';
  confidence: number;
  model: string;
  timestamp: string;
  price?: number;
}

interface SignalsData {
  signals: Signal[];
  lastUpdate: string | null;
}

const ACTION_COLORS = {
  BUY: 'var(--ft-color-success)',
  SELL: 'var(--ft-color-alert)',
  HOLD: 'var(--ft-color-text-muted)',
};

const ActionIcon = ({ action }: { action: string }) => {
  if (action === 'BUY') return <ArrowUpCircle size={12} color="var(--ft-color-success)" />;
  if (action === 'SELL') return <ArrowDownCircle size={12} color="var(--ft-color-alert)" />;
  return <MinusCircle size={12} color="var(--ft-color-text-muted)" />;
};

export const LiveSignalsWidget: React.FC<LiveSignalsWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<SignalsData>({
    key: 'widget:live-signals',
    category: 'trading',
    ttl: '1m',
    refetchInterval: 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        const raw = await invoke<string>('get_live_signals');
        const result = JSON.parse(raw);
        const signals: Signal[] = (result?.data ?? []).slice(0, 8);
        return { signals, lastUpdate: signals[0]?.timestamp ?? null };
      } catch {
        return { signals: [], lastUpdate: null };
      }
    }
  });

  const hasSignals = (data?.signals.length ?? 0) > 0;
  const buys = data?.signals.filter(s => s.action === 'BUY').length ?? 0;
  const sells = data?.signals.filter(s => s.action === 'SELL').length ?? 0;

  const formatTime = (ts: string) => {
    try {
      return new Date(ts).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
    } catch { return ts; }
  };

  return (
    <BaseWidget
      id={id}
      title="LIVE SIGNALS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-primary)"
    >
      {!hasSignals ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          <Cpu size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
          <div>No live signals</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>Configure AI Quant Lab to generate signals</div>
        </div>
      ) : (
        <div style={{ padding: '4px' }}>
          {/* Stats */}
          <div style={{ display: 'flex', gap: '4px', margin: '4px', marginBottom: '8px' }}>
            <div style={{ flex: 1, backgroundColor: 'var(--ft-color-panel)', padding: '6px', borderRadius: '2px', textAlign: 'center' }}>
              <div style={{ fontSize: '16px', fontWeight: 'bold', color: 'var(--ft-color-success)' }}>{buys}</div>
              <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>BUY</div>
            </div>
            <div style={{ flex: 1, backgroundColor: 'var(--ft-color-panel)', padding: '6px', borderRadius: '2px', textAlign: 'center' }}>
              <div style={{ fontSize: '16px', fontWeight: 'bold', color: 'var(--ft-color-alert)' }}>{sells}</div>
              <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>SELL</div>
            </div>
            <div style={{ flex: 1, backgroundColor: 'var(--ft-color-panel)', padding: '6px', borderRadius: '2px', textAlign: 'center' }}>
              <div style={{ fontSize: '16px', fontWeight: 'bold', color: 'var(--ft-color-text-muted)' }}>{data!.signals.length - buys - sells}</div>
              <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>HOLD</div>
            </div>
          </div>

          {/* Header */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 50px 55px 40px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
            <span>SYMBOL</span>
            <span style={{ textAlign: 'center' }}>ACTION</span>
            <span style={{ textAlign: 'right' }}>CONF.</span>
            <span style={{ textAlign: 'right' }}>TIME</span>
          </div>

          {data!.signals.map((sig, i) => (
            <div key={`${sig.symbol}-${i}`} style={{ display: 'grid', gridTemplateColumns: '1fr 50px 55px 40px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: 'var(--ft-font-size-tiny)', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{sig.symbol}</div>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{sig.model.substring(0, 12)}</div>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px' }}>
                <ActionIcon action={sig.action} />
                <span style={{ fontSize: '9px', fontWeight: 'bold', color: ACTION_COLORS[sig.action] }}>{sig.action}</span>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div style={{ width: '100%', height: '4px', backgroundColor: 'var(--ft-border-color)', borderRadius: '2px', overflow: 'hidden' }}>
                  <div style={{ width: `${sig.confidence * 100}%`, height: '100%', backgroundColor: ACTION_COLORS[sig.action], borderRadius: '2px' }} />
                </div>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{(sig.confidence * 100).toFixed(0)}%</div>
              </div>
              <span style={{ textAlign: 'right', fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
                {sig.timestamp ? formatTime(sig.timestamp) : '—'}
              </span>
            </div>
          ))}

          {onNavigate && (
            <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
              <span>OPEN AI QUANT LAB</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
