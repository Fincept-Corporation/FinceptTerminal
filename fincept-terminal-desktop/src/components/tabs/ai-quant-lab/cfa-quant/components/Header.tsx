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
    data: 'DATA',
    analysis: 'ANALYSIS',
    results: 'RESULTS'
  };

  return (
    <div style={{
      padding: '12px 16px',
      borderBottom: `1px solid ${BB.borderDark}`,
      backgroundColor: BB.panelBg,
      display: 'flex',
      alignItems: 'center',
      gap: '12px'
    }}>
      <Activity size={16} color={BB.amber} />
      <span style={{
        color: BB.amber,
        fontSize: '12px',
        fontWeight: 700,
        letterSpacing: '0.5px',
        fontFamily: 'monospace'
      }}>
        CFA QUANT ANALYTICS
      </span>
      <div style={{
        height: '14px',
        width: '1px',
        backgroundColor: BB.borderDark,
        marginLeft: '4px',
        marginRight: '4px'
      }} />

      {/* Step Navigation - Joined Square Design */}
      <div style={{ display: 'flex', gap: '0' }}>
        {steps.map((step, idx) => {
          const isActive = currentStep === step;
          const isPast = steps.indexOf(currentStep) > idx;
          const isDisabled = step === 'results' && !analysisResult;

          return (
            <button
              key={step}
              onClick={() => {
                if (!isDisabled) setCurrentStep(step);
              }}
              style={{
                padding: '6px 12px',
                backgroundColor: isActive ? BB.amber : BB.darkBg,
                border: `1px solid ${BB.borderDark}`,
                borderLeft: idx === 0 ? `1px solid ${BB.borderDark}` : '0',
                marginLeft: idx === 0 ? '0' : '-1px',
                color: isActive ? '#000000' : isPast ? BB.amber : BB.textMuted,
                fontSize: '10px',
                fontWeight: 700,
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                cursor: isDisabled ? 'not-allowed' : 'pointer',
                opacity: isDisabled ? 0.5 : 1,
                transition: 'all 0.15s'
              }}
              onMouseEnter={(e) => {
                if (!isActive && !isDisabled) {
                  e.currentTarget.style.backgroundColor = BB.hover;
                }
              }}
              onMouseLeave={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = BB.darkBg;
                }
              }}
            >
              {labels[step]}
            </button>
          );
        })}
      </div>

      <div style={{ flex: 1 }} />

      {/* Symbol Info */}
      {dataSourceType === 'symbol' && symbolInput && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{
            fontSize: '11px',
            fontFamily: 'monospace',
            fontWeight: 700,
            color: BB.textPrimary
          }}>
            {symbolInput.toUpperCase()}
          </span>
          {dataStats && (
            <>
              <span style={{
                fontSize: '11px',
                fontFamily: 'monospace',
                color: BB.textSecondary
              }}>
                {formatPrice(dataStats.mean)}
              </span>
              <div style={{
                fontSize: '10px',
                fontFamily: 'monospace',
                padding: '3px 8px',
                backgroundColor: dataStats.changePercent >= 0 ? BB.green + '20' : BB.red + '20',
                border: `1px solid ${dataStats.changePercent >= 0 ? BB.green : BB.red}`,
                color: dataStats.changePercent >= 0 ? BB.green : BB.red
              }}>
                {dataStats.changePercent >= 0 ? '▲' : '▼'} {Math.abs(dataStats.changePercent).toFixed(2)}%
              </div>
            </>
          )}
        </div>
      )}
    </div>
  );
}
