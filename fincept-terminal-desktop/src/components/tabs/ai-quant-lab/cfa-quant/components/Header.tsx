/**
 * CFA Quant Header Component
 * Step navigation and symbol info display
 */

import React from 'react';
import { Activity } from 'lucide-react';
import { BB } from '../constants';
import { formatPrice } from '../utils';
import type { HeaderProps, Step } from '../types';

export function Header({
  currentStep,
  setCurrentStep,
  analysisResult,
  dataSourceType,
  symbolInput,
  dataStats,
}: HeaderProps) {
  const steps: Step[] = ['data', 'analysis', 'results'];
  const labels: Record<Step, string> = {
    data: '1 DATA',
    analysis: '2 MODEL',
    results: '3 OUTPUT'
  };

  return (
    <div
      className="flex items-center justify-between px-4 py-2"
      style={{ backgroundColor: BB.black, borderBottom: `1px solid ${BB.borderDark}` }}
    >
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          <Activity size={18} style={{ color: BB.amber }} />
          <span className="text-sm font-bold tracking-wider" style={{ color: BB.textAmber }}>
            CFA QUANT ANALYTICS
          </span>
        </div>
        <div className="h-4 w-px" style={{ backgroundColor: BB.borderLight }} />
        <div className="flex items-center gap-2">
          {steps.map((step, idx) => {
            const isActive = currentStep === step;
            const isPast = steps.indexOf(currentStep) > idx;
            return (
              <React.Fragment key={step}>
                <button
                  onClick={() => {
                    if (step === 'results' && !analysisResult) return;
                    setCurrentStep(step);
                  }}
                  className="px-3 py-1 text-xs font-mono transition-colors"
                  style={{
                    backgroundColor: isActive ? BB.amber : 'transparent',
                    color: isActive ? BB.black : isPast ? BB.textAmber : BB.textMuted,
                    cursor: step === 'results' && !analysisResult ? 'not-allowed' : 'pointer',
                  }}
                >
                  {labels[step]}
                </button>
                {idx < 2 && <span style={{ color: BB.textMuted }}>›</span>}
              </React.Fragment>
            );
          })}
        </div>
      </div>
      {dataSourceType === 'symbol' && symbolInput && (
        <div className="flex items-center gap-3">
          <span className="text-sm font-mono font-bold" style={{ color: BB.textPrimary }}>
            {symbolInput.toUpperCase()}
          </span>
          {dataStats && (
            <>
              <span className="text-sm font-mono" style={{ color: BB.textSecondary }}>
                {formatPrice(dataStats.mean)}
              </span>
              <span
                className="text-sm font-mono"
                style={{ color: dataStats.changePercent >= 0 ? BB.green : BB.red }}
              >
                {dataStats.changePercent >= 0 ? '▲' : '▼'} {Math.abs(dataStats.changePercent).toFixed(2)}%
              </span>
            </>
          )}
        </div>
      )}
    </div>
  );
}
