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
  return (
    <div className="flex-1 p-2 overflow-auto">
      <label className="block text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
        PRICE DATA (JSON)
      </label>
      <textarea
        value={priceInput}
        onChange={(e) => setPriceInput(e.target.value)}
        className="w-full h-48 p-3 rounded text-xs font-mono resize-none"
        style={{
          backgroundColor: FINCEPT.DARK_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`
        }}
        placeholder='{"2023-01-01": 100, "2023-01-02": 101, ...}'
      />
    </div>
  );
}
