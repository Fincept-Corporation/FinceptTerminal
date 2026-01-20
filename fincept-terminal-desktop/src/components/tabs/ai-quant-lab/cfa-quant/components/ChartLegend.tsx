/**
 * Chart Legend Component
 * Displays legend for different analysis types
 */

import React from 'react';
import { BB, analysisConfigs } from '../constants';

interface ChartLegendProps {
  selectedAnalysis: string;
}

export function ChartLegend({ selectedAnalysis }: ChartLegendProps) {
  const config = analysisConfigs.find(a => a.id === selectedAnalysis);

  if (selectedAnalysis === 'forecasting') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TRAIN DATA</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TEST ACTUAL</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.blueLight }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ETS LEVEL</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t border-dashed" style={{ borderColor: BB.greenDim }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST CI</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'resampling_methods') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-3 h-3" style={{ backgroundColor: BB.green, opacity: 0.7 }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>DISTRIBUTION</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>MEAN</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 h-3" style={{ backgroundColor: BB.blueLight, opacity: 0.2 }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'unsupervised_learning') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PRICE</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>BOUNDARY</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>CLUSTER 0</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>CLUSTER 1</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'supervised_learning') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TRAIN DATA</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TEST ACTUAL</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.blueLight }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PREDICTION</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'validate_data') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>NORMAL DATA</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>SUSPICIOUS (2-3σ)</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.red }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>OUTLIER (&gt;3σ)</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.blueLight }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>MEAN</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'stationarity_test') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PRICE</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ROLLING MEAN</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>±2σ BANDS</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.blueLight }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>OVERALL MEAN</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'trend_analysis') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ACTUAL</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: config?.color || BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>TREND</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>PROJECTION</span>
        </div>
      </>
    );
  }

  if (selectedAnalysis === 'arima_model') {
    return (
      <>
        <div className="flex items-center gap-2">
          <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ACTUAL</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t-2 border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FITTED</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t border-dashed" style={{ borderColor: BB.amber }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>95% CI</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full" style={{ backgroundColor: BB.green }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST</span>
        </div>
        <div className="flex items-center gap-2">
          <div className="w-4 border-t border-dashed" style={{ borderColor: BB.greenDim }} />
          <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FORECAST CI</span>
        </div>
      </>
    );
  }

  // Default legend
  return (
    <>
      <div className="flex items-center gap-2">
        <div className="w-4 h-0.5" style={{ backgroundColor: BB.textSecondary }} />
        <span className="text-xs font-mono" style={{ color: BB.textMuted }}>ACTUAL</span>
      </div>
      <div className="flex items-center gap-2">
        <div className="w-4 h-0.5" style={{ backgroundColor: config?.color, borderStyle: 'dashed', borderWidth: 1, borderColor: config?.color }} />
        <span className="text-xs font-mono" style={{ color: BB.textMuted }}>FITTED</span>
      </div>
    </>
  );
}
