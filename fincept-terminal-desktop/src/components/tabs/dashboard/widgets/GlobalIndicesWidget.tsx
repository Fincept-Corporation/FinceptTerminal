import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/markets/marketDataService';
import { useCache } from '../../../../hooks/useCache';


interface GlobalIndicesWidgetProps {
  id: string;
  onRemove?: () => void;
}

// 12 Most Traded Global Indices
const TOP_12_INDICES = [
  '^GSPC',   // S&P 500
  '^DJI',    // Dow Jones
  '^IXIC',   // NASDAQ
  '^RUT',    // Russell 2000
  '^FTSE',   // FTSE 100
  '^GDAXI',  // DAX
  '^FCHI',   // CAC 40
  '^N225',   // Nikkei 225
  '^HSI',    // Hang Seng
  '000001.SS', // Shanghai Composite
  '^BSESN',  // BSE Sensex
  '^NSEI'    // Nifty 50
];

const INDEX_NAME_KEYS: { [key: string]: string } = {
  '^GSPC': 'widgets.indexSP500',
  '^DJI': 'widgets.indexDowJones',
  '^IXIC': 'widgets.indexNasdaq',
  '^RUT': 'widgets.indexRussell2000',
  '^FTSE': 'widgets.indexFTSE100',
  '^GDAXI': 'widgets.indexDAX',
  '^FCHI': 'widgets.indexCAC40',
  '^N225': 'widgets.indexNikkei225',
  '^HSI': 'widgets.indexHangSeng',
  '000001.SS': 'widgets.indexShanghai',
  '^BSESN': 'widgets.indexSensex',
  '^NSEI': 'widgets.indexNifty50'
};

export const GlobalIndicesWidget: React.FC<GlobalIndicesWidgetProps> = ({ id, onRemove }) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for indices data
  const {
    data: quotes,
    isLoading: loading,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: 'market-widget:global-indices',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(TOP_12_INDICES),
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000, // Auto-refresh every 10 minutes
    staleWhileRevalidate: true
  });

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
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
      title={t('widgets.globalIndices')}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={false}
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
          <div>{t('widgets.index')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.price')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.change')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.percentChange')}</div>
        </div>
        {loading && (!quotes || quotes.length === 0) ? (
          <>
            {TOP_12_INDICES.map((_, idx) => <SkeletonRow key={idx} />)}
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
            <div style={{ color: 'var(--ft-color-accent)' }}>{INDEX_NAME_KEYS[quote.symbol] ? t(INDEX_NAME_KEYS[quote.symbol]) : quote.symbol}</div>
            <div style={{ color: 'var(--ft-color-text)', textAlign: 'right' }}>
              {quote.price.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
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
            {t('widgets.noIndexData')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
