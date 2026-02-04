/**
 * PriceInputSection Component
 * Manual price data input textarea
 */

import React from 'react';
import { FINCEPT } from '../constants';
import type { PriceInputSectionProps } from '../types';

export function PriceInputSection({
  priceInput,
  setPriceInput
}: PriceInputSectionProps) {
  const dataPointCount = React.useMemo(() => {
    try {
      const data = JSON.parse(priceInput);
      if (typeof data === 'object' && data !== null) {
        if (Array.isArray(data)) return data.length;
        const firstValue = Object.values(data)[0];
        if (typeof firstValue === 'object') {
          // Multi-asset format
          const symbols = Object.keys(data);
          const dates = Object.keys(Object.values(data)[0] as Record<string, number>);
          return `${symbols.length} assets × ${dates.length} dates`;
        }
        // Single asset format
        return Object.keys(data).length;
      }
    } catch {
      return 0;
    }
    return 0;
  }, [priceInput]);

  return (
    <div className="flex-1 p-2 overflow-auto">
      <div className="flex items-center justify-between mb-2">
        <label className="block text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
          PRICE DATA (JSON)
        </label>
        {dataPointCount !== 0 && (
          <span className="text-xs font-mono" style={{ color: FINCEPT.CYAN }}>
            {dataPointCount} {typeof dataPointCount === 'string' ? '' : 'data points'}
          </span>
        )}
      </div>
      <textarea
        value={priceInput}
        onChange={(e) => setPriceInput(e.target.value)}
        className="w-full h-48 p-3 rounded text-xs font-mono resize-none"
        style={{
          backgroundColor: FINCEPT.DARK_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`
        }}
        placeholder='Click "Fetch Data" to load live market data from yfinance, or enter custom JSON: {"2023-01-01": 100, "2023-01-02": 101, ...}'
      />
    </div>
  );
}
