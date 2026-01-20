/**
 * AnalysisModeSelector Component
 * Grid of analysis mode buttons
 */

import React from 'react';
import {
  Activity,
  Calendar,
  LineChart,
  Layers,
  Scale,
  GitCompare
} from 'lucide-react';
import { FINCEPT } from '../constants';
import type { AnalysisModeSelectorProps, AnalysisMode } from '../types';

interface ModeOption {
  mode: AnalysisMode;
  label: string;
  icon: React.ReactNode;
}

const MODE_OPTIONS: ModeOption[] = [
  { mode: 'performance', label: 'Performance', icon: <Activity size={12} /> },
  { mode: 'monthly', label: 'Monthly', icon: <Calendar size={12} /> },
  { mode: 'rolling', label: 'Rolling', icon: <LineChart size={12} /> },
  { mode: 'comparison', label: 'Compare', icon: <Layers size={12} /> },
  { mode: 'optimize', label: 'Optimize', icon: <Scale size={12} /> },
  { mode: 'benchmark', label: 'Benchmark', icon: <GitCompare size={12} /> }
];

export function AnalysisModeSelector({
  analysisMode,
  setAnalysisMode
}: AnalysisModeSelectorProps) {
  return (
    <div className="px-2 py-2">
      <label className="block text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>
        ANALYSIS MODE
      </label>
      <div className="grid grid-cols-2 gap-1">
        {MODE_OPTIONS.map(({ mode, label, icon }) => (
          <button
            key={mode}
            onClick={() => setAnalysisMode(mode)}
            className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
            style={{
              backgroundColor: analysisMode === mode ? FINCEPT.ORANGE : 'transparent',
              color: analysisMode === mode ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: `1px solid ${FINCEPT.BORDER}`
            }}
          >
            {icon}
            {label}
          </button>
        ))}
      </div>
    </div>
  );
}
