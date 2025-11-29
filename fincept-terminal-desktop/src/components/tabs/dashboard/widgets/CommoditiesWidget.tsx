import React, { useState, useEffect } from 'react';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/marketDataService';

const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GREEN = '#00C800';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_YELLOW = '#FFFF00';

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
  const [quotes, setQuotes] = useState<QuoteData[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadQuotes = async () => {
    setLoading(true);
    setError(null);
    try {
      const data = await marketDataService.getEnhancedQuotesWithCache(TOP_COMMODITIES, 'Commodities', 10);
      setQuotes(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load commodities data');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadQuotes();
    const interval = setInterval(loadQuotes, 10 * 60 * 1000); // Refresh every 10 minutes
    return () => clearInterval(interval);
  }, []);

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  return (
    <BaseWidget
      id={id}
      title="COMMODITIES"
      onRemove={onRemove}
      onRefresh={loadQuotes}
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
          <div>COMMODITY</div>
          <div style={{ textAlign: 'right' }}>PRICE</div>
          <div style={{ textAlign: 'right' }}>CHG</div>
          <div style={{ textAlign: 'right' }}>%CHG</div>
        </div>
        {quotes.map((quote, index) => (
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
            <div style={{ color: BLOOMBERG_YELLOW }}>{COMMODITY_NAMES[quote.symbol] || quote.symbol}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>
              ${quote.price.toFixed(2)}
            </div>
            <div style={{
              color: quote.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>
              {formatChange(quote.change)}
            </div>
            <div style={{
              color: quote.change_percent >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              textAlign: 'right'
            }}>
              {formatPercent(quote.change_percent)}
            </div>
          </div>
        ))}
        {quotes.length === 0 && !loading && !error && (
          <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', textAlign: 'center', padding: '12px' }}>
            No commodities data available
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
