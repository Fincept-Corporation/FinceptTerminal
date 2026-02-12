// IndicatorSelector Component - Select indicator for data fetching

import React from 'react';
import { Search } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { Indicator } from '../types';

interface IndicatorSelectorProps {
  indicators: Indicator[];
  selectedIndicator: string;
  setSelectedIndicator: (id: string) => void;
  searchIndicator: string;
  setSearchIndicator: (search: string) => void;
  sourceName: string;
  sourceColor: string;
}

export function IndicatorSelector({
  indicators,
  selectedIndicator,
  setSelectedIndicator,
  searchIndicator,
  setSearchIndicator,
  sourceName,
  sourceColor,
}: IndicatorSelectorProps) {
  const filteredIndicators = indicators.filter(
    (i) =>
      i.name.toLowerCase().includes(searchIndicator.toLowerCase()) ||
      i.id.toLowerCase().includes(searchIndicator.toLowerCase()) ||
      i.category.toLowerCase().includes(searchIndicator.toLowerCase())
  );

  return (
    <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      <div
        style={{
          padding: '10px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          letterSpacing: '0.5px',
          display: 'flex',
          justifyContent: 'space-between',
        }}
      >
        <span>INDICATORS ({sourceName})</span>
        <span style={{ color: FINCEPT.MUTED }}>{indicators.length}</span>
      </div>
      <div style={{ padding: '8px', flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        <div style={{ position: 'relative', marginBottom: '8px' }}>
          <Search size={12} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.MUTED }} />
          <input
            type="text"
            placeholder="Search indicators..."
            value={searchIndicator}
            onChange={(e) => setSearchIndicator(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px 8px 30px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              fontFamily: 'inherit',
              outline: 'none',
            }}
          />
        </div>
        <div style={{ flex: 1, overflowY: 'auto' }}>
          {filteredIndicators.map((indicator) => (
            <div
              key={indicator.id}
              onClick={() => setSelectedIndicator(indicator.id)}
              style={{
                padding: '6px 10px',
                backgroundColor: selectedIndicator === indicator.id ? `${sourceColor}15` : 'transparent',
                borderLeft: selectedIndicator === indicator.id ? `2px solid ${sourceColor}` : '2px solid transparent',
                cursor: 'pointer',
              }}
            >
              <div style={{ fontSize: '10px', color: selectedIndicator === indicator.id ? FINCEPT.WHITE : FINCEPT.GRAY, marginBottom: '2px' }}>
                {indicator.name}
              </div>
              <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
                {indicator.category} • {indicator.id}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

export default IndicatorSelector;
