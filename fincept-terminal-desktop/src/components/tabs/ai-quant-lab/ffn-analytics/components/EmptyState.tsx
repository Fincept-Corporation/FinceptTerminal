/**
 * EmptyState Component
 * Displays when no analysis results are available
 */

import React from 'react';
import { BarChart2 } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { AnalysisMode } from '../types';

interface EmptyStateProps {
  analysisMode: AnalysisMode;
}

const EMPTY_STATE_MESSAGES: Record<AnalysisMode, string> = {
  performance: 'Enter price data and click "Run Analysis" to see performance metrics',
  monthly: 'Click "Run Analysis" to see monthly returns heatmap',
  rolling: 'Click "Run Analysis" to see rolling performance metrics',
  comparison: 'Enter multiple symbols and click "Run Analysis" to compare assets',
  optimize: 'Enter multiple symbols and click "Run Analysis" to optimize portfolio weights',
  benchmark: 'Enter portfolio data and benchmark symbol, then click "Run Analysis" to compare against benchmark'
};

export function EmptyState({ analysisMode }: EmptyStateProps) {
  return (
    <div className="flex items-center justify-center h-full">
      <div className="text-center">
        <BarChart2 size={64} color={FINCEPT.MUTED} className="mx-auto mb-4" />
        <p className="text-sm font-mono" style={{ color: FINCEPT.GRAY }}>
          {EMPTY_STATE_MESSAGES[analysisMode]}
        </p>
      </div>
    </div>
  );
}
