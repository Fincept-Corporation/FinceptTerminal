import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/markets/marketDataService';
import { useCache } from '../../../../hooks/useCache';

const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GREEN = '#00C800';
const FINCEPT_RED = '#FF0000';
const FINCEPT_GRAY = '#787878';
const FINCEPT_YELLOW = '#FFFF00';

interface CommoditiesWidgetProps {
  id: string;
  onRemove?: () => void;
}

const TOP_COMMODITIES = ['GC=F', 'SI=F', 'CL=F', 'NG=F', 'HG=F', 'ZC=F', 'ZS=F', 'ZW=F'];

const COMMODITY_NAMES: { [key: string]: string } = {
  'GC=F': 'Gold',
  'SI=F': 'Silver',
  'CL=F': 'Crude Oil',
  'NG=F': 'Nat Gas',
  'HG=F': 'Copper',
  'ZC=F': 'Corn',
  'ZS=F': 'Soybeans',
  'ZW=F': 'Wheat'
};

export const CommoditiesWidget: React.FC<CommoditiesWidgetProps> = ({ id, onRemove }) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for commodities data
  const {
    data: quotes,
    isLoading: loading,
    error,
    refresh
  } = useCache<QuoteData[]>({
    key: 'market-widget:commodities',
    category: 'market-quotes',
    fetcher: () => marketDataService.getEnhancedQuotes(TOP_COMMODITIES),
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
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
      title={t('widgets.commodities')}
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
          fontSize: '9px',
          fontWeight: 'bold',
          color: FINCEPT_WHITE,
          borderBottom: `1px solid ${FINCEPT_GRAY}`,
          padding: '4px 0',
          marginBottom: '4px'
        }}>
          <div>{t('widgets.commodity')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.price')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.change')}</div>
          <div style={{ textAlign: 'right' }}>{t('widgets.percentChange')}</div>
        </div>
        {loading && (!quotes || quotes.length === 0) ? (
          <>
            {TOP_COMMODITIES.map((_, idx) => <SkeletonRow key={idx} />)}
          </>
        ) : (quotes || []).map((quote, index) => (
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
            <div style={{ color: FINCEPT_YELLOW }}>{COMMODITY_NAMES[quote.symbol] || quote.symbol}</div>
            <div style={{ color: FINCEPT_WHITE, textAlign: 'right' }}>
              ${quote.price.toFixed(2)}
            </div>
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
        {(!quotes || quotes.length === 0) && !loading && !error && (
          <div style={{ color: FINCEPT_GRAY, fontSize: '10px', textAlign: 'center', padding: '12px' }}>
            {t('widgets.noCommoditiesData')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
