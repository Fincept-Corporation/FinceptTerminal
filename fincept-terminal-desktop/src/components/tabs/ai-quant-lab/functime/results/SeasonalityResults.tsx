/**
 * SeasonalityResults Component
 * Displays seasonality analysis results
 */

import React from 'react';
import { Waves } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { PortfolioSeasonalityResult } from '../types';

interface SeasonalityResultsProps {
  seasonalityResult: PortfolioSeasonalityResult;
}

export const SeasonalityResults: React.FC<SeasonalityResultsProps> = ({ seasonalityResult }) => {
  if (!seasonalityResult.success || !seasonalityResult.seasonality) {
    return null;
  }

  return (
    <div className="space-y-4">
      <div
        className="rounded overflow-hidden"
        style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}` }}
      >
        <div
          className="px-4 py-3 flex items-center gap-2"
          style={{ backgroundColor: FINCEPT.HEADER_BG }}
        >
          <Waves size={16} color={FINCEPT.CYAN} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Seasonality Analysis Results
          </span>
        </div>
        <div className="p-4">
          <div className="grid grid-cols-2 gap-4">
            {Object.entries(seasonalityResult.seasonality).map(([symbol, data]) => (
              <div
                key={symbol}
                className="p-3 rounded"
                style={{ backgroundColor: FINCEPT.DARK_BG }}
              >
                <div className="flex justify-between items-center mb-2">
                  <span className="text-sm font-bold" style={{ color: FINCEPT.CYAN }}>
                    {symbol}
                  </span>
                  <span
                    className="text-xs px-2 py-0.5 rounded"
                    style={{
                      backgroundColor: data.is_seasonal ? FINCEPT.GREEN : FINCEPT.MUTED,
                      color: FINCEPT.WHITE
                    }}
                  >
                    {data.is_seasonal ? 'Seasonal' : 'Non-Seasonal'}
                  </span>
                </div>
                <div className="text-xs font-mono space-y-1" style={{ color: FINCEPT.GRAY }}>
                  <div className="flex justify-between">
                    <span>Period:</span>
                    <span style={{ color: FINCEPT.WHITE }}>{data.period} days</span>
                  </div>
                  <div className="flex justify-between">
                    <span>Seasonal Strength:</span>
                    <span style={{ color: FINCEPT.ORANGE }}>{(data.strength * 100).toFixed(1)}%</span>
                  </div>
                  {data.trend_strength !== undefined && (
                    <div className="flex justify-between">
                      <span>Trend Strength:</span>
                      <span style={{ color: FINCEPT.GREEN }}>{(data.trend_strength * 100).toFixed(1)}%</span>
                    </div>
                  )}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
