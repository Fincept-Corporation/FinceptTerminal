import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/markets/marketDataService';
import { useCache } from '../../../../hooks/useCache';


interface ForexWidgetProps {
  id: string;
  onRemove?: () => void;
}

const MAJOR_FOREX_PAIRS = ['EURUSD=X', 'GBPUSD=X', 'USDJPY=X', 'AUDUSD=X', 'USDCAD=X', 'USDCHF=X', 'NZDUSD=X', 'EURCHF=X'];

const FOREX_NAME_KEYS: { [key: string]: string } = {
  'EURUSD=X': 'widgets.forexEURUSD',
  'GBPUSD=X': 'widgets.forexGBPUSD',
  'USDJPY=X': 'widgets.forexUSDJPY',
  'AUDUSD=X': 'widgets.forexAUDUSD',
  'USDCAD=X': 'widgets.forexUSDCAD',
  'USDCHF=X': 'widgets.forexUSDCHF',
  'NZDUSD=X': 'widgets.forexNZDUSD',
  'EURCHF=X': 'widgets.forexEURCHF'
};

export const ForexWidget: React.FC<ForexWidgetProps> = ({ id, onRemove }) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for forex data
  const {
    data: quotes,
    isLoading: loading,
    isFetching,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: 'market-widget:forex',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(MAJOR_FOREX_PAIRS),
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
    staleWhileRevalidate: true
  });

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(4)}` : value.toFixed(4);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  const SkeletonRow = () => (
    <div style={{
      display: 'grid',
      gridTemplateColumns: '2fr 1fr 1fr 1fr',
      gap: '4px',
      fontSize: '9px',
      padding: '2px 0',
      borderBottom: `1px solid rgba(120,120,120,0.3)`
    }}>
      <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '12px', borderRadius: '2px', animation: 'pulse 1.5s infinite' }}></div>
      <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '12px', borderRadius: '2px', animation: 'pulse 1.5s infinite', animationDelay: '0.1s' }}></div>
      <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '12px', borderRadius: '2px', animation: 'pulse 1.5s infinite', animationDelay: '0.2s' }}></div>
      <div style={{ backgroundColor: 'rgba(120,120,120,0.3)', height: '12px', borderRadius: '2px', animation: 'pulse 1.5s infinite', animationDelay: '0.3s' }}></div>
    </div>
  );

  return (
    <BaseWidget
      id={id}
      title={t('widgets.forex')}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(loading && !quotes) || isFetching}
      error={error?.message}
    >
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 0.4; }
          50% { opacity: 0.8; }
        }
      `}</style>
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
          <div>{t('widgets.pair')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.rate')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.change')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.percentChange')}</div>
        </div>
        {loading && (!quotes || quotes.length === 0) ? (
          <>
            {MAJOR_FOREX_PAIRS.map((_, idx) => <SkeletonRow key={idx} />)}
          </>
        ) : (quotes || []).map((quote, index) => (
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
            <div style={{ color: 'var(--ft-color-purple)' }}>{FOREX_NAME_KEYS[quote.symbol] ? t(FOREX_NAME_KEYS[quote.symbol]) : quote.symbol}</div>
            <div style={{ color: 'var(--ft-color-text)', textAlign: 'right' }}>
              {quote.price.toFixed(4)}
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
            {t('widgets.noForexData')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
