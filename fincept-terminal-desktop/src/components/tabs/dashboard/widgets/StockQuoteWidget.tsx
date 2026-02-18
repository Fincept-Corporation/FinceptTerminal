import React from 'react';
import { TrendingUp, TrendingDown, ExternalLink, Minus } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { useCache } from '@/hooks/useCache';

interface StockQuoteWidgetProps {
  id: string;
  symbol?: string;
  onRemove?: () => void;
  onNavigate?: () => void;
  onConfigure?: () => void;
}

export const StockQuoteWidget: React.FC<StockQuoteWidgetProps> = ({
  id,
  symbol = 'AAPL',
  onRemove,
  onNavigate,
  onConfigure
}) => {
  const ticker = symbol.toUpperCase();

  const { data: quote, isLoading, isFetching, error, refresh } = useCache<QuoteData | null>({
    key: `widget:stock-quote:${ticker}`,
    category: 'market',
    ttl: '1m',
    refetchInterval: 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        return await marketDataService.getQuote(ticker);
      } catch {
        return null;
      }
    }
  });

  const isPositive = (quote?.change ?? 0) >= 0;
  const color = isPositive ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';

  const formatPrice = (p: number) => p.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  const formatVolume = (v: number) => {
    if (v >= 1e9) return `${(v / 1e9).toFixed(1)}B`;
    if (v >= 1e6) return `${(v / 1e6).toFixed(1)}M`;
    if (v >= 1e3) return `${(v / 1e3).toFixed(1)}K`;
    return v.toString();
  };

  return (
    <BaseWidget
      id={id}
      title={`QUOTE: ${ticker}`}
      onRemove={onRemove}
      onRefresh={refresh}
      onConfigure={onConfigure}
      isLoading={(isLoading && !quote) || isFetching}
      error={error?.message || null}
      headerColor={quote ? color : 'var(--ft-color-primary)'}
    >
      {!quote ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          No data for {ticker}
        </div>
      ) : (
        <div style={{ padding: '12px' }}>
          {/* Price */}
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
            <div>
              <div style={{ fontSize: '28px', fontWeight: 'bold', color: 'var(--ft-color-text)', lineHeight: 1 }}>
                ${formatPrice(quote.price)}
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginTop: '4px' }}>
                {isPositive ? <TrendingUp size={14} color={color} /> : <TrendingDown size={14} color={color} />}
                <span style={{ color, fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold' }}>
                  {isPositive ? '+' : ''}{formatPrice(quote.change)} ({isPositive ? '+' : ''}{quote.change_percent.toFixed(2)}%)
                </span>
              </div>
            </div>
            <div style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
              <div>TODAY</div>
              <div style={{ color: 'var(--ft-color-primary)', fontWeight: 'bold', fontSize: 'var(--ft-font-size-small)' }}>{ticker}</div>
            </div>
          </div>

          {/* Stats Grid */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
            {[
              { label: 'OPEN', value: quote.open != null ? `$${formatPrice(quote.open)}` : '—' },
              { label: 'PREV CLOSE', value: quote.previous_close != null ? `$${formatPrice(quote.previous_close)}` : '—' },
              { label: 'HIGH', value: quote.high != null ? `$${formatPrice(quote.high)}` : '—' },
              { label: 'LOW', value: quote.low != null ? `$${formatPrice(quote.low)}` : '—' },
              { label: 'VOLUME', value: quote.volume != null ? formatVolume(quote.volume) : '—' },
            ].map(({ label, value }) => (
              <div key={label} style={{ backgroundColor: 'var(--ft-color-panel)', padding: '6px 8px', borderRadius: '2px' }}>
                <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '2px' }}>{label}</div>
                <div style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{value}</div>
              </div>
            ))}
          </div>

          {onNavigate && (
            <div onClick={onNavigate} style={{ marginTop: '8px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
              <span>Open in Research</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
