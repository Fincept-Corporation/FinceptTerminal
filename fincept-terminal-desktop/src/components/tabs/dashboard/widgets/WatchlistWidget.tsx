import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { watchlistService, WatchlistStockWithQuote } from '../../../../services/core/watchlistService';
import { useCache } from '@/hooks/useCache';

interface WatchlistWidgetProps {
  id: string;
  watchlistId?: string;
  watchlistName?: string;
  onRemove?: () => void;
}

export const WatchlistWidget: React.FC<WatchlistWidgetProps> = ({
  id,
  watchlistId,
  watchlistName = 'Watchlist',
  onRemove
}) => {
  const { t } = useTranslation('dashboard');

  const {
    data: stocks,
    isLoading: loading,
    error,
    refresh
  } = useCache<WatchlistStockWithQuote[]>({
    key: `widget:watchlist:${watchlistId || 'none'}`,
    category: 'market-quotes',
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
    staleWhileRevalidate: true,
    enabled: !!watchlistId,
    fetcher: async () => {
      await watchlistService.initialize();
      return watchlistService.getWatchlistStocksWithQuotes(watchlistId!);
    }
  });

  const displayStocks = stocks || [];

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  return (
    <BaseWidget
      id={id}
      title={`${t('widgets.watchlist')} - ${watchlistName}`}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={loading && !stocks}
      error={!watchlistId ? t('widgets.noWatchlistSelected') : (error?.message || null)}
    >
      <div style={{ padding: '4px' }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '4px',
          fontSize: 'var(--ft-font-size-tiny)',
          fontWeight: 'bold',
          color: 'var(--ft-color-text)',
          borderBottom: '1px solid var(--ft-border-color)',
          padding: '4px 0',
          marginBottom: '4px'
        }}>
          <div>{t('widgets.symbol')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.price')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.change')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.percentChange')}</div>
        </div>
        {displayStocks.map((stock, index) => (
          <div
            key={index}
            style={{
              display: 'grid',
              gridTemplateColumns: '2fr 1fr 1fr 1fr',
              gap: '4px',
              fontSize: 'var(--ft-font-size-tiny)',
              padding: '2px 0',
              borderBottom: '1px solid var(--ft-border-color)'
            }}
          >
            <div style={{ color: 'var(--ft-color-accent)' }}>{stock.symbol}</div>
            <div style={{ color: 'var(--ft-color-text)', textAlign: 'right' }}>
              {stock.quote?.price?.toFixed(2) || '-'}
            </div>
            <div style={{
              color: (stock.quote?.change || 0) >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
              textAlign: 'right'
            }}>
              {stock.quote ? formatChange(stock.quote.change) : '-'}
            </div>
            <div style={{
              color: (stock.quote?.change_percent || 0) >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
              textAlign: 'right'
            }}>
              {stock.quote ? formatPercent(stock.quote.change_percent) : '-'}
            </div>
          </div>
        ))}
        {displayStocks.length === 0 && !loading && !error && watchlistId && (
          <div style={{ color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)', textAlign: 'center', padding: '12px' }}>
            {t('widgets.noStocksInWatchlist')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
