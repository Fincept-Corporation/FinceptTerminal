/**
 * AnomalyConfigSection Component
 * Configuration for anomaly detection
 */

import React from 'react';
import { AlertTriangle } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { AnomalyConfigProps } from '../types';

export const AnomalyConfigSection: React.FC<AnomalyConfigProps> = ({
  anomalyMethod,
  setAnomalyMethod,
  anomalyThreshold,
  setAnomalyThreshold
}) => {
  return (
    <div
      className="rounded overflow-hidden"
      style={{ border: `1px solid ${FINCEPT.BORDER}` }}
    >
      <div
        className="px-3 py-2 flex items-center gap-2"
        style={{ backgroundColor: FINCEPT.HEADER_BG }}
      >
        <AlertTriangle size={14} color={FINCEPT.YELLOW} />
        <span className="text-xs font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
          Anomaly Detection Settings
        </span>
      </div>
      <div className="p-3 space-y-3">
        <div>
          <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
            DETECTION METHOD
          </label>
          <select
            value={anomalyMethod}
            onChange={(e) => setAnomalyMethod(e.target.value as 'zscore' | 'iqr' | 'isolation_forest')}
            className="w-full p-2 rounded text-xs font-mono"
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`
            }}
          >
            <option value="zscore">Z-Score</option>
            <option value="iqr">IQR (Interquartile Range)</option>
            <option value="isolation_forest">Isolation Forest</option>
          </select>
        </div>
        <div>
          <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
            THRESHOLD
          </label>
          <input
            type="number"
            step="0.5"
            min={1}
            max={5}
            value={anomalyThreshold}
            onChange={(e) => setAnomalyThreshold(parseFloat(e.target.value) || 3.0)}
            className="w-full p-2 rounded text-xs font-mono"
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`
            }}
          />
        </div>
      </div>
    </div>
  );
};
