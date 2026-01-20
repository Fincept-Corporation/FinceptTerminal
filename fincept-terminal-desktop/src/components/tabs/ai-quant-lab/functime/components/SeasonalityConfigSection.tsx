/**
 * SeasonalityConfigSection Component
 * Configuration for seasonality analysis (informational)
 */

import React from 'react';
import { Waves } from 'lucide-react';
import { FINCEPT } from '../constants';

export const SeasonalityConfigSection: React.FC = () => {
  return (
    <div
      className="rounded overflow-hidden"
      style={{ border: `1px solid ${FINCEPT.BORDER}` }}
    >
      <div
        className="px-3 py-2 flex items-center gap-2"
        style={{ backgroundColor: FINCEPT.HEADER_BG }}
      >
        <Waves size={14} color={FINCEPT.CYAN} />
        <span className="text-xs font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
          Seasonality Analysis
        </span>
      </div>
      <div className="p-3">
        <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
          Analyzes time series for seasonal patterns, trend strength, and periodicity.
          Uses FFT and ACF for automatic period detection.
        </p>
      </div>
    </div>
  );
};
