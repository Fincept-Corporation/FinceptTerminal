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
    <div style={{ display: 'flex', height: '100%' }}>
      {/* Left Panel - Analysis Selection */}
      <div style={{
        width: '320px',
        borderRight: `1px solid ${BB.borderDark}`,
        backgroundColor: BB.panelBg,
        display: 'flex',
        flexDirection: 'column'
      }}>
        <div style={{
          padding: '10px 12px',
          borderBottom: `1px solid ${BB.borderDark}`,
          fontSize: '10px',
          fontWeight: 700,
          color: BB.amber,
          fontFamily: 'monospace',
          letterSpacing: '0.5px'
        }}>
          SELECT MODEL
        </div>

        {/* Model List - Joined Square Design */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
          {analysisConfigs.map((config, idx) => {
            const isSelected = selectedAnalysis === config.id;
            const hasEnoughData = priceData.length >= config.minDataPoints;
            const Icon = config.icon;

            return (
              <button
                key={config.id}
                onClick={() => hasEnoughData && setSelectedAnalysis(config.id)}
                style={{
                  padding: '12px',
                  backgroundColor: isSelected ? BB.hover : 'transparent',
                  border: `1px solid ${isSelected ? BB.amber : BB.borderDark}`,
                  borderTop: idx === 0 ? `1px solid ${isSelected ? BB.amber : BB.borderDark}` : '0',
                  marginTop: idx === 0 ? '0' : '-1px',
                  cursor: hasEnoughData ? 'pointer' : 'not-allowed',
                  opacity: hasEnoughData ? 1 : 0.4,
                  transition: 'all 0.15s',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px'
                }}
                onMouseEnter={(e) => {
                  if (!isSelected && hasEnoughData) {
                    e.currentTarget.style.backgroundColor = BB.darkBg;
                    e.currentTarget.style.borderColor = BB.amber;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.borderColor = BB.borderDark;
                  }
                }}
              >
                <Icon size={16} color={BB.amber} />
                <div style={{ flex: 1, minWidth: 0, textAlign: 'left' }}>
                  <div style={{
                    fontSize: '11px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    color: BB.textPrimary,
                    marginBottom: '2px'
                  }}>
                    {config.shortName}
                  </div>
                  <div style={{
                    fontSize: '10px',
                    fontFamily: 'monospace',
                    color: BB.textPrimary,
                    opacity: 0.6,
                    whiteSpace: 'nowrap',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis'
                  }}>
                    {config.name}
                  </div>
                </div>
                {!hasEnoughData && (
                  <span style={{
                    fontSize: '9px',
                    fontFamily: 'monospace',
                    color: BB.red,
                    padding: '2px 6px',
                    backgroundColor: BB.red + '20',
                    border: `1px solid ${BB.red}`
                  }}>
                    {config.minDataPoints}+
                  </span>
                )}
              </button>
            );
          })}
        </div>

        {/* Action Buttons */}
        <div style={{ borderTop: `1px solid ${BB.borderDark}`, padding: '12px', display: 'flex', gap: '8px' }}>
          <button
            onClick={() => setCurrentStep('data')}
            style={{
              flex: 1,
              padding: '10px 14px',
              backgroundColor: 'transparent',
              border: `1px solid ${BB.amber}`,
              color: BB.amber,
              fontSize: '10px',
              fontWeight: 700,
              fontFamily: 'monospace',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              letterSpacing: '0.5px',
              transition: 'all 0.15s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BB.amber + '20';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
            }}
          >
            <ChevronLeft size={12} /> BACK
          </button>
          <button
            onClick={runAnalysis}
            disabled={isLoading || !canRun}
            style={{
              flex: 1,
              padding: '10px 14px',
              backgroundColor: canRun ? BB.amber : BB.cardBg,
              border: 'none',
              color: canRun ? '#000000' : BB.textMuted,
              fontSize: '10px',
              fontWeight: 700,
              fontFamily: 'monospace',
              cursor: canRun ? 'pointer' : 'not-allowed',
              opacity: canRun ? 1 : 0.5,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              letterSpacing: '0.5px',
              transition: 'all 0.15s'
            }}
          >
            {isLoading ? 'RUNNING...' : 'RUN'} <Zap size={12} />
          </button>
        </div>

        {/* Error Display */}
        {error && (
          <div style={{
            margin: '0 12px 12px 12px',
            padding: '10px 12px',
            backgroundColor: BB.red + '15',
            border: `1px solid ${BB.red}`,
            borderLeft: `3px solid ${BB.red}`,
            color: BB.red,
            fontSize: '10px',
            fontFamily: 'monospace',
            lineHeight: '1.5'
          }}>
            {error}
          </div>
        )}
      </div>

      {/* Right Panel - Model Details */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', backgroundColor: BB.black }}>
        {selectedConfig && (
          <>
            <div style={{
              padding: '10px 16px',
              borderBottom: `1px solid ${BB.borderDark}`,
              fontSize: '10px',
              fontWeight: 700,
              color: BB.amber,
              fontFamily: 'monospace',
              letterSpacing: '0.5px',
              backgroundColor: BB.panelBg
            }}>
              MODEL DETAILS
            </div>

            <div style={{ padding: '16px', overflowY: 'auto' }}>
              {/* Model Info Card */}
              <div style={{
                padding: '14px',
                backgroundColor: BB.panelBg,
                border: `1px solid ${BB.borderDark}`,
                borderLeft: `3px solid ${BB.amber}`,
                marginBottom: '16px'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '10px' }}>
                  <div style={{
                    padding: '8px',
                    backgroundColor: BB.amber + '20',
                    border: `1px solid ${BB.amber}`
                  }}>
                    <selectedConfig.icon size={20} color={BB.amber} />
                  </div>
                  <div>
                    <div style={{
                      fontSize: '12px',
                      fontWeight: 700,
                      fontFamily: 'monospace',
                      color: BB.textPrimary,
                      marginBottom: '2px'
                    }}>
                      {selectedConfig.name}
                    </div>
                    <div style={{
                      fontSize: '9px',
                      fontFamily: 'monospace',
                      color: BB.textPrimary,
                      opacity: 0.6,
                      letterSpacing: '0.5px'
                    }}>
                      {selectedConfig.category.replace('_', ' ').toUpperCase()}
                    </div>
                  </div>
                </div>

                <div style={{
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  color: BB.textPrimary,
                  opacity: 0.8,
                  lineHeight: '1.6',
                  marginBottom: '10px'
                }}>
                  {selectedConfig.helpText}
                </div>

                <div style={{ display: 'flex', gap: '16px' }}>
                  <div>
                    <span style={{
                      fontSize: '9px',
                      fontFamily: 'monospace',
                      color: BB.textPrimary,
                      opacity: 0.5,
                      letterSpacing: '0.5px'
                    }}>
                      MIN POINTS:{' '}
                    </span>
                    <span style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      fontWeight: 700,
                      color: priceData.length >= selectedConfig.minDataPoints ? BB.green : BB.red
                    }}>
                      {selectedConfig.minDataPoints}
                    </span>
                  </div>
                  <div>
                    <span style={{
                      fontSize: '9px',
                      fontFamily: 'monospace',
                      color: BB.textPrimary,
                      opacity: 0.5,
                      letterSpacing: '0.5px'
                    }}>
                      YOUR DATA:{' '}
                    </span>
                    <span style={{
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      fontWeight: 700,
                      color: BB.textPrimary
                    }}>
                      {priceData.length}
                    </span>
                  </div>
                </div>
              </div>

              {/* Chart Preview */}
              <div style={{ flex: 1, minHeight: '300px' }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  marginBottom: '12px'
                }}>
                  <div style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    fontFamily: 'monospace',
                    color: BB.amber,
                    letterSpacing: '0.5px'
                  }}>
                    DATA PREVIEW
                  </div>
                  {dataChartZoom && (
                    <button
                      onClick={() => setDataChartZoom(null)}
                      style={{
                        padding: '6px 10px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${BB.amber}`,
                        color: BB.amber,
                        fontSize: '9px',
                        fontWeight: 700,
                        fontFamily: 'monospace',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        letterSpacing: '0.5px',
                        transition: 'all 0.15s'
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.backgroundColor = BB.amber + '20';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }}
                    >
                      <RotateCcw size={10} /> RESET
                    </button>
                  )}
                </div>
                <ResponsiveContainer width="100%" height={300}>
                  <LineChart data={zoomedChartData} margin={{ top: 5, right: 5, left: 0, bottom: 5 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke={BB.chartGrid} />
                    <XAxis dataKey="index" hide />
                    <YAxis hide domain={['auto', 'auto']} />
                    <Line
                      type="monotone"
                      dataKey="value"
                      stroke={BB.amber}
                      dot={false}
                      strokeWidth={1.5}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
          </>
        )}
      </div>
    </div>
  );
}
