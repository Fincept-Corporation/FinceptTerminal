/**
 * AnomalyResults Component
 * Displays anomaly detection results
 */

import React from 'react';
import { AlertTriangle } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { PortfolioAnomalyResult } from '../types';

interface AnomalyResultsProps {
  anomalyResult: PortfolioAnomalyResult;
}

export const AnomalyResults: React.FC<AnomalyResultsProps> = ({ anomalyResult }) => {
  if (!anomalyResult.success || !anomalyResult.anomalies) {
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
          <AlertTriangle size={16} color={FINCEPT.YELLOW} />
          <span className="text-xs font-bold uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
            Anomaly Detection Results
          </span>
        </div>
        <div className="p-4">
          <div className="grid grid-cols-2 gap-4">
            {Object.entries(anomalyResult.anomalies).map(([symbol, data]) => (
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
                      backgroundColor: data.count > 0 ? FINCEPT.RED : FINCEPT.GREEN,
                      color: FINCEPT.WHITE
                    }}
                  >
                    {data.count} anomalies
                  </span>
                </div>
                <div className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                  <div>Rate: {(data.rate * 100).toFixed(2)}%</div>
                  {data.dates?.length > 0 && (
                    <div className="mt-1">
                      Latest: {data.dates[data.dates.length - 1]}
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
