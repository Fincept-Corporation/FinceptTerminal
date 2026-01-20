/**
 * Analysis Step Component
 * Handles model selection and execution
 */

import React from 'react';
import { ChevronLeft, Zap, RotateCcw } from 'lucide-react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  ResponsiveContainer,
} from 'recharts';
import { BB, analysisConfigs } from '../constants';
import type { AnalysisStepProps } from '../types';

export function AnalysisStep({
  selectedAnalysis,
  setSelectedAnalysis,
  priceData,
  isLoading,
  error,
  runAnalysis,
  setCurrentStep,
  zoomedChartData,
  dataChartZoom,
  setDataChartZoom,
}: AnalysisStepProps) {
  const selectedConfig = analysisConfigs.find(a => a.id === selectedAnalysis);
  const canRun = priceData.length >= (selectedConfig?.minDataPoints || 0);

  return (
    <div className="flex h-full">
      {/* Left Panel - Analysis Selection */}
      <div
        className="w-80 flex-shrink-0 p-4 overflow-y-auto"
        style={{ backgroundColor: BB.panelBg, borderRight: `1px solid ${BB.borderDark}` }}
      >
        <div className="text-xs font-mono mb-3" style={{ color: BB.textAmber }}>SELECT MODEL</div>

        <div className="space-y-1">
          {analysisConfigs.map((config) => {
            const isSelected = selectedAnalysis === config.id;
            const hasEnoughData = priceData.length >= config.minDataPoints;
            const Icon = config.icon;

            return (
              <button
                key={config.id}
                onClick={() => hasEnoughData && setSelectedAnalysis(config.id)}
                className="w-full px-3 py-2 text-left flex items-center gap-3 transition-colors"
                style={{
                  backgroundColor: isSelected ? BB.cardBg : 'transparent',
                  borderLeft: `3px solid ${isSelected ? config.color : 'transparent'}`,
                  opacity: hasEnoughData ? 1 : 0.4,
                  cursor: hasEnoughData ? 'pointer' : 'not-allowed',
                }}
              >
                <Icon size={16} style={{ color: config.color }} />
                <div className="flex-1 min-w-0">
                  <div className="text-xs font-mono font-bold" style={{ color: BB.textPrimary }}>{config.shortName}</div>
                  <div className="text-xs font-mono truncate" style={{ color: BB.textMuted }}>{config.name}</div>
                </div>
                {!hasEnoughData && (
                  <span className="text-xs font-mono" style={{ color: BB.red }}>
                    {config.minDataPoints}+
                  </span>
                )}
              </button>
            );
          })}
        </div>

        <div className="mt-4 flex gap-2">
          <button
            onClick={() => setCurrentStep('data')}
            className="flex-1 py-2 text-xs font-mono"
            style={{ backgroundColor: BB.cardBg, color: BB.textSecondary, border: `1px solid ${BB.borderDark}` }}
          >
            <ChevronLeft size={12} className="inline" /> BACK
          </button>
          <button
            onClick={runAnalysis}
            disabled={isLoading || !canRun}
            className="flex-1 py-2 text-xs font-mono font-bold"
            style={{
              backgroundColor: canRun ? BB.green : BB.cardBg,
              color: canRun ? BB.black : BB.textMuted,
              cursor: canRun ? 'pointer' : 'not-allowed',
            }}
          >
            {isLoading ? 'RUNNING...' : 'RUN'} <Zap size={12} className="inline ml-1" />
          </button>
        </div>

        {error && (
          <div
            className="mt-4 p-3 text-xs font-mono"
            style={{ backgroundColor: `${BB.red}20`, border: `1px solid ${BB.red}`, color: BB.red }}
          >
            {error}
          </div>
        )}
      </div>

      {/* Right Panel - Model Details */}
      <div className="flex-1 p-4" style={{ backgroundColor: BB.darkBg }}>
        {selectedConfig && (
          <div className="h-full flex flex-col">
            <div className="mb-4 p-4" style={{ backgroundColor: BB.cardBg, border: `1px solid ${BB.borderDark}` }}>
              <div className="flex items-center gap-3 mb-3">
                <div className="p-2" style={{ backgroundColor: `${selectedConfig.color}20` }}>
                  <selectedConfig.icon size={24} style={{ color: selectedConfig.color }} />
                </div>
                <div>
                  <div className="text-sm font-mono font-bold" style={{ color: BB.textPrimary }}>{selectedConfig.name}</div>
                  <div className="text-xs font-mono" style={{ color: BB.textMuted }}>{selectedConfig.category.replace('_', ' ').toUpperCase()}</div>
                </div>
              </div>
              <div className="text-xs font-mono" style={{ color: BB.textSecondary }}>{selectedConfig.helpText}</div>

              <div className="mt-3 flex items-center gap-4 text-xs font-mono">
                <div>
                  <span style={{ color: BB.textMuted }}>MIN POINTS: </span>
                  <span style={{ color: priceData.length >= selectedConfig.minDataPoints ? BB.green : BB.red }}>
                    {selectedConfig.minDataPoints}
                  </span>
                </div>
                <div>
                  <span style={{ color: BB.textMuted }}>YOUR DATA: </span>
                  <span style={{ color: BB.textPrimary }}>{priceData.length}</span>
                </div>
              </div>
            </div>

            {/* Mini Chart Preview */}
            <div className="flex-1" style={{ minHeight: 200 }}>
              <div className="flex items-center justify-between mb-2">
                <div className="text-xs font-mono" style={{ color: BB.textAmber }}>DATA PREVIEW</div>
                {dataChartZoom && (
                  <button
                    onClick={() => setDataChartZoom(null)}
                    className="text-xs font-mono px-2 py-0.5 flex items-center gap-1"
                    style={{ backgroundColor: BB.cardBg, color: BB.textMuted, border: `1px solid ${BB.borderDark}` }}
                  >
                    <RotateCcw size={10} /> RESET ZOOM
                  </button>
                )}
              </div>
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={zoomedChartData} margin={{ top: 5, right: 5, left: 0, bottom: 5 }}>
                  <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                  <XAxis dataKey="index" hide />
                  <YAxis hide domain={['auto', 'auto']} />
                  <Line
                    type="monotone"
                    dataKey="value"
                    stroke={selectedConfig.color}
                    dot={false}
                    strokeWidth={1.5}
                  />
                </LineChart>
              </ResponsiveContainer>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
