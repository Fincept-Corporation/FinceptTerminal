/**
 * AnalysisModeSelector Component
 * Tab-style selector for switching between analysis modes
 */

import React from 'react';
import { TrendingUp, AlertTriangle, Waves } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { AnalysisMode } from '../types';

interface AnalysisModeSelectorProps {
  analysisMode: AnalysisMode;
  setAnalysisMode: (mode: AnalysisMode) => void;
}

export const AnalysisModeSelector: React.FC<AnalysisModeSelectorProps> = ({
  analysisMode,
  setAnalysisMode
}) => {
  return (
    <div className="flex gap-1 p-1 rounded" style={{ backgroundColor: FINCEPT.HEADER_BG }}>
      <button
        onClick={() => setAnalysisMode('forecast')}
        className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
        style={{
          backgroundColor: analysisMode === 'forecast' ? FINCEPT.ORANGE : 'transparent',
          color: analysisMode === 'forecast' ? FINCEPT.DARK_BG : FINCEPT.GRAY
        }}
      >
        <TrendingUp size={12} />
        Forecast
      </button>
      <button
        onClick={() => setAnalysisMode('anomaly')}
        className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
        style={{
          backgroundColor: analysisMode === 'anomaly' ? FINCEPT.ORANGE : 'transparent',
          color: analysisMode === 'anomaly' ? FINCEPT.DARK_BG : FINCEPT.GRAY
        }}
      >
        <AlertTriangle size={12} />
        Anomaly
      </button>
      <button
        onClick={() => setAnalysisMode('seasonality')}
        className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
        style={{
          backgroundColor: analysisMode === 'seasonality' ? FINCEPT.ORANGE : 'transparent',
          color: analysisMode === 'seasonality' ? FINCEPT.DARK_BG : FINCEPT.GRAY
        }}
      >
        <Waves size={12} />
        Season
      </button>
    </div>
  );
};
