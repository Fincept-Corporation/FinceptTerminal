import React from 'react';
import { Ship, ExternalLink, TrendingUp, TrendingDown } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { useCache } from '@/hooks/useCache';

interface MaritimeWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface ShippingData {
  indices: { symbol: string; name: string; quote: QuoteData | null }[];
}

// Publicly traded shipping ETFs and sector stocks as maritime proxies
const MARITIME_TICKERS = [
  { symbol: 'BDRY', name: 'Dry Bulk ETF' },
  { symbol: 'SBLK', name: 'Star Bulk' },
  { symbol: 'ZIM', name: 'ZIM Integrated' },
  { symbol: 'GOGL', name: 'Golden Ocean' },
  { symbol: 'MATX', name: 'Matson Inc' },
  { symbol: 'DAC', name: 'Danaos Corp' },
];

export const MaritimeWidget: React.FC<MaritimeWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<ShippingData>({
    key: 'widget:maritime',
    category: 'markets',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      const results = await Promise.allSettled(
        MARITIME_TICKERS.map(t => marketDataService.getQuote(t.symbol))
      );
      const indices = MARITIME_TICKERS.map((t, i) => ({
        symbol: t.symbol,
        name: t.name,
        quote: results[i].status === 'fulfilled' ? results[i].value : null
      }));
      return { indices };
    }
  });

  return (
    <BaseWidget
      id={id}
      title="MARITIME"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-primary)"
    >
      <div style={{ padding: '4px' }}>
        {/* Header */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 65px 60px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
          <span>INSTRUMENT</span>
          <span style={{ textAlign: 'right' }}>PRICE</span>
          <span style={{ textAlign: 'right' }}>CHG%</span>
        </div>

        {(!data || data.indices.length === 0) ? (
          <div style={{ padding: '20px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)' }}>
            <Ship size={24} style={{ margin: '0 auto 8px', opacity: 0.3 }} />
            <div>No data available</div>
          </div>
        ) : data.indices.map(row => {
          const q = row.quote;
          const pos = (q?.change_percent ?? 0) >= 0;
          const clr = q ? (pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)') : 'var(--ft-color-text-muted)';
          return (
            <div key={row.symbol} style={{ display: 'grid', gridTemplateColumns: '1fr 65px 60px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{row.symbol}</div>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{row.name}</div>
              </div>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {q ? `$${q.price.toFixed(2)}` : '—'}
              </span>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '3px' }}>
                {q && (pos ? <TrendingUp size={10} color={clr} /> : <TrendingDown size={10} color={clr} />)}
                <span style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: clr }}>
                  {q ? `${pos ? '+' : ''}${q.change_percent.toFixed(2)}%` : '—'}
                </span>
              </div>
            </div>
          );
        })}

        <div style={{ padding: '4px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
          Shipping sector ETFs & stocks
        </div>

        {onNavigate && (
          <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
            <span>OPEN MARITIME</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
