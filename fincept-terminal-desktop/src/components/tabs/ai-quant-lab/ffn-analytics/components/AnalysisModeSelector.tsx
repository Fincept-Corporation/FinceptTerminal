/**
 * AnalysisModeSelector Component
 * Joined square design analysis mode buttons
 * GREEN THEME
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
  Icon: any;
}

const MODE_OPTIONS: ModeOption[] = [
  { mode: 'performance', label: 'Performance', Icon: Activity },
  { mode: 'monthly', label: 'Monthly', Icon: Calendar },
  { mode: 'rolling', label: 'Rolling', Icon: LineChart },
  { mode: 'comparison', label: 'Compare', Icon: Layers },
  { mode: 'optimize', label: 'Optimize', Icon: Scale },
  { mode: 'benchmark', label: 'Benchmark', Icon: GitCompare }
];

export function AnalysisModeSelector({
  analysisMode,
  setAnalysisMode
}: AnalysisModeSelectorProps) {
  return (
    <div style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
      <div style={{
        padding: '10px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: FINCEPT.GREEN,
        fontFamily: 'monospace',
        letterSpacing: '0.5px'
      }}>
        ANALYSIS MODE
      </div>
      <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
        {MODE_OPTIONS.map(({ mode, label, Icon }, idx) => {
          const isActive = analysisMode === mode;
          return (
            <button
              key={mode}
              onClick={() => setAnalysisMode(mode)}
              style={{
                padding: '10px 12px',
                backgroundColor: isActive ? FINCEPT.HOVER : 'transparent',
                border: `1px solid ${isActive ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                borderTop: idx === 0 ? `1px solid ${isActive ? FINCEPT.GREEN : FINCEPT.BORDER}` : '0',
                cursor: 'pointer',
                transition: 'all 0.15s',
                marginTop: idx === 0 ? '0' : '-1px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px'
              }}
              onMouseEnter={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                  e.currentTarget.style.borderColor = FINCEPT.GREEN;
                }
              }}
              onMouseLeave={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                }
              }}
            >
              <Icon size={14} color={FINCEPT.GREEN} />
              <span style={{
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                color: FINCEPT.WHITE,
                opacity: isActive ? 1 : 0.7,
                letterSpacing: '0.5px'
              }}>
                {label.toUpperCase()}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
}
