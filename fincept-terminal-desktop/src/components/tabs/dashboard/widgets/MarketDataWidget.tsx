import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/markets/marketDataService';
import { useCache } from '../../../../hooks/useCache';

const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GREEN = '#00C800';
const FINCEPT_RED = '#FF0000';
const FINCEPT_GRAY = '#787878';

interface MarketDataWidgetProps {
  id: string;
  category?: string;
  tickers?: string[];
  onRemove?: () => void;
}

export const MarketDataWidget: React.FC<MarketDataWidgetProps> = ({
  id,
  category = 'Indices',
  tickers = ['^GSPC', '^IXIC', '^DJI', '^RUT'],
  onRemove
}) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for market data
  const {
    data: quotes,
    isLoading: loading,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: `market-widget:${category}:${tickers.sort().join(',')}`,
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(tickers),
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000, // Auto-refresh every 10 minutes
    staleWhileRevalidate: true
  });

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  return (
    <BaseWidget
      id={id}
      title={`MARKETS - ${category}`}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={loading}
      error={error?.message}
    >
      <div style={{ padding: '4px' }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '4px',
          fontSize: '9px',
          fontWeight: 'bold',
          color: FINCEPT_WHITE,
          borderBottom: `1px solid ${FINCEPT_GRAY}`,
          padding: '4px 0',
          marginBottom: '4px'
        }}>
          <div>{t('widgets.symbol')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.price')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.change')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.percentChange')}</div>
        </div>
        {(quotes || []).map((quote, index) => (
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
            <div style={{ color: FINCEPT_WHITE }}>{quote.symbol}</div>
            <div style={{ color: FINCEPT_WHITE, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
            <div style={{
              color: quote.change >= 0 ? FINCEPT_GREEN : FINCEPT_RED,
              textAlign: 'right'
            }}>
              {formatChange(quote.change)}
            </div>
            <div style={{
              color: quote.change_percent >= 0 ? FINCEPT_GREEN : FINCEPT_RED,
              textAlign: 'right'
            }}>
              {formatPercent(quote.change_percent)}
            </div>
          </div>
        ))}
      </div>
    </BaseWidget>
  );
};
