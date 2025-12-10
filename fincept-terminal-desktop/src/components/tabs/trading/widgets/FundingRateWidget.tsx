// File: src/components/tabs/trading/widgets/FundingRateWidget.tsx
// Display funding rates for perpetual futures

import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown } from 'lucide-react';

const COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  GREEN: '#10b981',
  RED: '#dc2626',
  GRAY: '#525252',
  PANEL_BG: '#1a1a1a',
};

interface FundingRateWidgetProps {
  adapter: any;
  symbol: string;
}

export function FundingRateWidget({ adapter, symbol }: FundingRateWidgetProps) {
  const [fundingRate, setFundingRate] = useState<number | null>(null);
  const [nextFundingTime, setNextFundingTime] = useState<number | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  const { ORANGE, WHITE, GREEN, RED, GRAY, PANEL_BG } = COLORS;

  useEffect(() => {
    const fetchFundingRate = async () => {
      if (!adapter || !symbol) return;

      try {
        setIsLoading(true);

        if ('fetchFundingRate' in adapter) {
          const data = await adapter.fetchFundingRate(symbol);
          setFundingRate(data.fundingRate || null);
          setNextFundingTime(data.fundingTimestamp || null);
        }
      } catch (error) {
        console.error('Failed to fetch funding rate:', error);
      } finally {
        setIsLoading(false);
      }
    };

    fetchFundingRate();
    const interval = setInterval(fetchFundingRate, 60000); // Update every minute

    return () => clearInterval(interval);
  }, [adapter, symbol]);

  if (isLoading && fundingRate === null) {
    return (
      <div style={{
        padding: '8px 12px',
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
      }}>
        <span style={{ fontSize: '10px', color: GRAY }}>Loading funding rate...</span>
      </div>
    );
  }

  if (fundingRate === null) return null;

  const fundingRatePercent = (fundingRate * 100).toFixed(4);
  const isPositive = fundingRate > 0;
  const color = isPositive ? RED : GREEN;

  return (
    <div style={{
      padding: '8px 12px',
      backgroundColor: PANEL_BG,
      borderBottom: `1px solid ${GRAY}`,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ fontSize: '10px', color: GRAY, fontWeight: 'bold', textTransform: 'uppercase' }}>
          Funding Rate:
        </span>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          {isPositive ? <TrendingUp size={12} style={{ color }} /> : <TrendingDown size={12} style={{ color }} />}
          <span style={{ fontSize: '11px', fontWeight: 'bold', color }}>
            {isPositive ? '+' : ''}{fundingRatePercent}%
          </span>
        </div>
      </div>

      {nextFundingTime && (
        <span style={{ fontSize: '9px', color: '#666' }}>
          Next: {new Date(nextFundingTime).toLocaleTimeString()}
        </span>
      )}
    </div>
  );
}
