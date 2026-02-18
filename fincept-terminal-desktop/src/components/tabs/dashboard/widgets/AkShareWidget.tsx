import React from 'react';
import { TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';
import { useCache } from '@/hooks/useCache';

interface AkShareWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface ChineseIndex {
  symbol: string;
  name: string;
  price: number;
  change: number;
  change_pct: number;
}

interface AkShareData {
  indices: ChineseIndex[];
  lastUpdate: string;
}

const CN_INDICES = [
  { symbol: 'sh000001', name: 'Shanghai Composite' },
  { symbol: 'sz399001', name: 'Shenzhen Component' },
  { symbol: 'sh000300', name: 'CSI 300' },
  { symbol: 'sh000016', name: 'SSE 50' },
  { symbol: 'sz399006', name: 'ChiNext' },
];

export const AkShareWidget: React.FC<AkShareWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<AkShareData>({
    key: 'widget:akshare-indices',
    category: 'markets',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        const raw = await invoke<string>('fetch_akshare_index_spot');
        const result = JSON.parse(raw);
        const rawData: any[] = result?.data ?? [];

        const indices: ChineseIndex[] = CN_INDICES.map(idx => {
          const row = rawData.find((r: any) =>
            (r.code ?? r.symbol ?? '').toLowerCase() === idx.symbol
          );
          return {
            symbol: idx.symbol,
            name: idx.name,
            price: parseFloat(row?.latest ?? row?.close ?? row?.price ?? 0),
            change: parseFloat(row?.change ?? 0),
            change_pct: parseFloat(row?.pct_change ?? row?.change_pct ?? 0),
          };
        }).filter(i => i.price > 0);

        return { indices, lastUpdate: new Date().toLocaleTimeString() };
      } catch {
        return { indices: [], lastUpdate: '' };
      }
    }
  });

  return (
    <BaseWidget
      id={id}
      title="CHINA MARKETS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-alert)"
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 70px 65px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
          <span>INDEX</span>
          <span style={{ textAlign: 'right' }}>PRICE</span>
          <span style={{ textAlign: 'right' }}>CHANGE%</span>
        </div>

        {(!data || data.indices.length === 0) ? (
          <div style={{ padding: '20px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)' }}>
            No data available
          </div>
        ) : data.indices.map(idx => {
          const pos = idx.change_pct >= 0;
          const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
          return (
            <div key={idx.symbol} style={{ display: 'grid', gridTemplateColumns: '1fr 70px 65px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{idx.name}</div>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{idx.symbol}</div>
              </div>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {idx.price.toLocaleString('en-US', { maximumFractionDigits: 2 })}
              </span>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '3px' }}>
                {pos ? <TrendingUp size={10} color={clr} /> : <TrendingDown size={10} color={clr} />}
                <span style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: clr }}>
                  {pos ? '+' : ''}{idx.change_pct.toFixed(2)}%
                </span>
              </div>
            </div>
          );
        })}

        {data?.lastUpdate && (
          <div style={{ padding: '4px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', textAlign: 'right' }}>
            Updated: {data.lastUpdate}
          </div>
        )}

        {onNavigate && (
          <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-alert)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
            <span>OPEN AKSHARE DATA</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
