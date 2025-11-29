import React, { useState, useEffect } from 'react';
import { BaseWidget } from './BaseWidget';
import { marketDataService, QuoteData } from '../../../../services/marketDataService';

const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GREEN = '#00C800';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_GRAY = '#787878';

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
  const [quotes, setQuotes] = useState<QuoteData[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadQuotes = async () => {
    setLoading(true);
    setError(null);
    try {
      // Use cached quotes with 10 minute cache age (matches refresh interval)
      const data = await marketDataService.getEnhancedQuotesWithCache(
        tickers,
        category,
        10 // 10 minutes cache
      );
      setQuotes(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load market data');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadQuotes();
    const interval = setInterval(loadQuotes, 10 * 60 * 1000); // Refresh every 10 minutes
    return () => clearInterval(interval);
  }, [tickers.join(',')]);

  const formatChange = (value: number) => value >= 0 ? `+${value.toFixed(2)}` : value.toFixed(2);
  const formatPercent = (value: number) => value >= 0 ? `+${value.toFixed(2)}%` : `${value.toFixed(2)}%`;

  return (
    <BaseWidget
      id={id}
      title={`MARKETS - ${category}`}
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
          <div>SYMBOL</div>
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
            <div style={{ color: BLOOMBERG_WHITE }}>{quote.symbol}</div>
            <div style={{ color: BLOOMBERG_WHITE, textAlign: 'right' }}>{quote.price.toFixed(2)}</div>
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
      </div>
    </BaseWidget>
  );
};
