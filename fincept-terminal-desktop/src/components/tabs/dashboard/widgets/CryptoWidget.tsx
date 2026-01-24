import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/markets/marketDataService';
import { useCache } from '../../../../hooks/useCache';


interface CryptoWidgetProps {
  id: string;
  onRemove?: () => void;
}

const TOP_CRYPTOS = ['BTC-USD', 'ETH-USD', 'BNB-USD', 'XRP-USD', 'ADA-USD', 'DOGE-USD', 'SOL-USD', 'DOT-USD', 'MATIC-USD', 'LTC-USD'];

export const CryptoWidget: React.FC<CryptoWidgetProps> = ({ id, onRemove }) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for crypto data
  const {
    data: quotes,
    isLoading: loading,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: 'market-widget:crypto',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(TOP_CRYPTOS),
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
    staleWhileRevalidate: true
  });

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  return (
    <BaseWidget
      id={id}
      title={t('widgets.crypto')}
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
        {(quotes || []).map((quote, index) => (
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
            <div style={{ color: 'var(--ft-color-accent)' }}>{quote.symbol.replace('-USD', '')}</div>
            <div style={{ color: 'var(--ft-color-text)', textAlign: 'right' }}>
              ${quote.price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
            <div style={{
              color: quote.change >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
              textAlign: 'right'
            }}>
              {formatChange(quote.change)}
            </div>
            <div style={{
              color: quote.change_percent >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
              textAlign: 'right'
            }}>
              {formatPercent(quote.change_percent)}
            </div>
          </div>
        ))}
        {(!quotes || quotes.length === 0) && !loading && !error && (
          <div style={{ color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)', textAlign: 'center', padding: '12px' }}>
            {t('widgets.noCryptoData')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
