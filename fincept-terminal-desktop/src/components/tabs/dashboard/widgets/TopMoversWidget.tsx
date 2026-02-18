import React, { useState } from 'react';
import { TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { useCache } from '@/hooks/useCache';

interface TopMoversWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface MoversData {
  gainers: QuoteData[];
  losers: QuoteData[];
}

// Standard liquid tickers to scan for movers
const SCAN_TICKERS = [
  'AAPL', 'MSFT', 'GOOGL', 'AMZN', 'NVDA', 'META', 'TSLA', 'NFLX',
  'AMD', 'INTC', 'BABA', 'JPM', 'GS', 'BAC', 'WMT', 'DIS',
  'BA', 'XOM', 'CVX', 'COIN', 'PLTR', 'SOFI', 'RIVN', 'LCID'
];

export const TopMoversWidget: React.FC<TopMoversWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const [tab, setTab] = useState<'gainers' | 'losers'>('gainers');

  const { data, isLoading, isFetching, error, refresh } = useCache<MoversData>({
    key: 'widget:top-movers',
    category: 'market',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      const results = await Promise.allSettled(
        SCAN_TICKERS.map(sym => marketDataService.getQuote(sym))
      );
      const quotes: QuoteData[] = [];
      results.forEach(r => {
        if (r.status === 'fulfilled' && r.value) quotes.push(r.value);
      });
      const sorted = [...quotes].sort((a, b) => b.change_percent - a.change_percent);
      return {
        gainers: sorted.filter(q => q.change_percent > 0).slice(0, 6),
        losers: sorted.filter(q => q.change_percent < 0).reverse().slice(0, 6)
      };
    }
  });

  const list = tab === 'gainers' ? (data?.gainers ?? []) : (data?.losers ?? []);

  return (
    <BaseWidget
      id={id}
      title="TOP MOVERS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-primary)"
    >
      <div style={{ padding: '4px' }}>
        {/* Tab Toggle */}
        <div style={{ display: 'flex', margin: '4px', borderRadius: '2px', overflow: 'hidden', border: '1px solid var(--ft-border-color)' }}>
          {(['gainers', 'losers'] as const).map(t => (
            <button
              key={t}
              onClick={() => setTab(t)}
              style={{
                flex: 1,
                padding: '5px',
                fontSize: 'var(--ft-font-size-tiny)',
                fontWeight: 'bold',
                border: 'none',
                cursor: 'pointer',
                backgroundColor: tab === t ? (t === 'gainers' ? 'var(--ft-color-success)' : 'var(--ft-color-alert)') : 'var(--ft-color-panel)',
                color: tab === t ? '#000' : 'var(--ft-color-text-muted)',
                textTransform: 'uppercase'
              }}
            >
              {t === 'gainers' ? '▲ GAINERS' : '▼ LOSERS'}
            </button>
          ))}
        </div>

        {/* Header */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 80px 70px', gap: '4px', padding: '4px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
          <span>SYMBOL</span>
          <span style={{ textAlign: 'right' }}>PRICE</span>
          <span style={{ textAlign: 'right' }}>CHANGE%</span>
        </div>

        {list.length === 0 ? (
          <div style={{ padding: '16px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)' }}>
            No data available
          </div>
        ) : list.map(q => {
          const pos = q.change_percent >= 0;
          const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
          return (
            <div key={q.symbol} style={{ display: 'grid', gridTemplateColumns: '1fr 80px 70px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                {pos ? <TrendingUp size={10} color={clr} /> : <TrendingDown size={10} color={clr} />}
                <span style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{q.symbol}</span>
              </div>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                ${q.price.toFixed(2)}
              </span>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: clr }}>
                {pos ? '+' : ''}{q.change_percent.toFixed(2)}%
              </span>
            </div>
          );
        })}

        {onNavigate && (
          <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
            <span>OPEN MARKETS</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
