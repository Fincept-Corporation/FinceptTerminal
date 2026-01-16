import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { watchlistService, WatchlistStockWithQuote } from '../../../../services/core/watchlistService';

const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GREEN = '#00C800';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_CYAN = '#00FFFF';

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
  const [stocks, setStocks] = useState<WatchlistStockWithQuote[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadStocks = async () => {
    if (!watchlistId) {
      setError(t('widgets.noWatchlistSelected'));
      setLoading(false);
      return;
    }

    setLoading(true);
    setError(null);
    try {
      await watchlistService.initialize();
      const data = await watchlistService.getWatchlistStocksWithQuotes(watchlistId);
      setStocks(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load watchlist');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadStocks();
    const interval = setInterval(loadStocks, 10 * 60 * 1000); // Refresh every 10 minutes
    return () => clearInterval(interval);
  }, [watchlistId]);

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  return (
    <BaseWidget
      id={id}
      title={`${t('widgets.watchlist')} - ${watchlistName}`}
      onRemove={onRemove}
      onRefresh={loadStocks}
      isLoading={loading}
      error={error}
    >
      <div style={{ padding: '4px' }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '4px',
          fontSize: '9px',
          fontWeight: 'bold',
          color: BLOOMBERG_WHITE,
          borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
          padding: '4px 0',
          marginBottom: '4px'
        }}>
          <div>{t('widgets.symbol')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.price')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.change')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.percentChange')}</div>
        </div>
        {stocks.map((stock, index) => (
          <div
            key={index}
            style={{
              display: 'grid',
              gridTemplateColumns: '2fr 1fr 1fr 1fr',
              gap: '4px',
              fontSize: '9px',
              padding: '2px 0',
              borderBottom: `1px solid rgba(120,120,120,0.3)`
            }}
          >
            <div style={{ color: BLOOMBERG_CYAN }}>{stock.symbol}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>
              {stock.quote?.price?.toFixed(2) || '-'}
            </div>
            <div style={{
              color: (stock.quote?.change || 0) >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>
              {stock.quote ? formatChange(stock.quote.change) : '-'}
            </div>
            <div style={{
              color: (stock.quote?.change_percent || 0) >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>
              {stock.quote ? formatPercent(stock.quote.change_percent) : '-'}
            </div>
          </div>
        ))}
        {stocks.length === 0 && !loading && !error && (
          <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', textAlign: 'center', padding: '12px' }}>
            {t('widgets.noStocksInWatchlist')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
