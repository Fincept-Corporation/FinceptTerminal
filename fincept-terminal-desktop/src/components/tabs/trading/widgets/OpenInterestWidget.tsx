// File: src/components/tabs/trading/widgets/OpenInterestWidget.tsx
// Display open interest for derivatives

import React, { useState, useEffect } from 'react';
import { BarChart3 } from 'lucide-react';

const COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  GRAY: '#525252',
  PANEL_BG: '#1a1a1a',
};

interface OpenInterestWidgetProps {
  adapter: any;
  symbol: string;
}

export function OpenInterestWidget({ adapter, symbol }: OpenInterestWidgetProps) {
  const [openInterest, setOpenInterest] = useState<number | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  const { ORANGE, WHITE, GRAY, PANEL_BG } = COLORS;

  useEffect(() => {
    const fetchOpenInterest = async () => {
      if (!adapter || !symbol) return;

      try {
        setIsLoading(true);

        if ('fetchOpenInterest' in adapter) {
          const data = await adapter.fetchOpenInterest(symbol);
          setOpenInterest(data.openInterestValue || data.openInterestAmount || null);
        }
      } catch (error) {
        console.error('Failed to fetch open interest:', error);
      } finally {
        setIsLoading(false);
      }
    };

    fetchOpenInterest();
    const interval = setInterval(fetchOpenInterest, 30000); // Update every 30s

    return () => clearInterval(interval);
  }, [adapter, symbol]);

  if (isLoading && openInterest === null) return null;
  if (openInterest === null) return null;

  const formattedOI = openInterest >= 1000000
    ? `$${(openInterest / 1000000).toFixed(2)}M`
    : openInterest >= 1000
    ? `$${(openInterest / 1000).toFixed(2)}K`
    : `$${openInterest.toFixed(2)}`;

  return (
    <div style={{
      padding: '8px 12px',
      backgroundColor: PANEL_BG,
      borderBottom: `1px solid ${GRAY}`,
      display: 'flex',
      alignItems: 'center',
      gap: '8px',
    }}>
      <BarChart3 size={12} style={{ color: ORANGE }} />
      <span style={{ fontSize: '10px', color: GRAY, fontWeight: 'bold', textTransform: 'uppercase' }}>
        Open Interest:
      </span>
      <span style={{ fontSize: '11px', fontWeight: 'bold', color: WHITE }}>
        {formattedOI}
      </span>
    </div>
  );
}
