/**
 * PortfolioRiskMode Component
 * Portfolio risk analysis configuration and results display
 */

import React from 'react';
import {
  Settings,
  ChevronUp,
  ChevronDown,
  Play,
  RefreshCw,
  BarChart2
} from 'lucide-react';
import { FINCEPT } from '../constants';
import { formatPercent, formatRatio } from '../utils';
import { MetricRow } from '../components';
import type { PortfolioRiskModeProps, ExpandedSections } from '../types';

export const PortfolioRiskMode: React.FC<PortfolioRiskModeProps> = ({
  weights,
  setWeights,
  alpha,
  setAlpha,
  halfLife,
  setHalfLife,
  isLoading,
  analysisResult,
  expandedSections,
  toggleSection,
  runPortfolioAnalysis
}) => {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Configuration Panel */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderLeft: `3px solid ${FINCEPT.BLUE}`
      }}>
        <button
          onClick={() => toggleSection('config')}
          style={{
            width: '100%',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '12px 16px',
            backgroundColor: 'transparent',
            border: 'none',
            cursor: 'pointer',
            transition: 'background-color 0.15s'
          }}
          onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
          onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Settings size={14} color={FINCEPT.BLUE} />
            <span style={{
              color: FINCEPT.WHITE,
              fontSize: '11px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              fontFamily: 'monospace'
            }}>
              CONFIGURATION
            </span>
          </div>
          {expandedSections.config ? (
            <ChevronUp size={14} color={FINCEPT.GRAY} />
          ) : (
            <ChevronDown size={14} color={FINCEPT.GRAY} />
          )}
        </button>

        {expandedSections.config && (
          <div style={{
            padding: '16px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '10px',
                  marginBottom: '6px',
                  color: FINCEPT.GRAY,
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  PORTFOLIO WEIGHTS
                </label>
                <input
                  type="text"
                  value={weights}
                  onChange={(e) => setWeights(e.target.value)}
                  placeholder="0.3,0.4,0.3"
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace'
                  }}
                />
              </div>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '10px',
                  marginBottom: '6px',
                  color: FINCEPT.GRAY,
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  ALPHA (VaR/CVaR)
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={String(alpha)}
                  onChange={(e) => {
                    const v = e.target.value;
                    if (v === '' || /^\d*\.?\d*$/.test(v)) {
                      setAlpha(parseFloat(v) || 0);
                    }
                  }}
                  placeholder="0.05"
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace'
                  }}
                />
              </div>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '10px',
                  marginBottom: '6px',
                  color: FINCEPT.GRAY,
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  HALF-LIFE (DAYS)
                </label>
                <input
                  type="text"
                  inputMode="numeric"
                  value={String(halfLife)}
                  onChange={(e) => {
                    const v = e.target.value;
                    if (v === '' || /^\d+$/.test(v)) {
                      setHalfLife(parseInt(v) || 0);
                    }
                  }}
                  placeholder="252"
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    fontFamily: 'monospace'
                  }}
                />
              </div>
            </div>

            <button
              onClick={runPortfolioAnalysis}
              disabled={isLoading}
              style={{
                width: '100%',
                padding: '10px 16px',
                backgroundColor: FINCEPT.BLUE,
                border: 'none',
                color: FINCEPT.DARK_BG,
                fontSize: '11px',
                fontWeight: 700,
                cursor: isLoading ? 'not-allowed' : 'pointer',
                opacity: isLoading ? 0.6 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                fontFamily: 'monospace',
                letterSpacing: '0.5px',
                transition: 'all 0.15s'
              }}
            >
              {isLoading ? (
                <>
                  <RefreshCw size={14} className="animate-spin" />
                  ANALYZING...
                </>
              ) : (
                <>
                  <Play size={14} />
                  RUN PORTFOLIO ANALYSIS
                </>
              )}
            </button>
          </div>
        )}
      </div>

      {/* Results */}
      {analysisResult && (
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderLeft: `3px solid ${FINCEPT.GREEN}`
        }}>
          <button
            onClick={() => toggleSection('metrics')}
            style={{
              width: '100%',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '12px 16px',
              backgroundColor: 'transparent',
              border: 'none',
              cursor: 'pointer',
              transition: 'background-color 0.15s'
            }}
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = FINCEPT.HOVER}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <BarChart2 size={14} color={FINCEPT.GREEN} />
              <span style={{
                color: FINCEPT.WHITE,
                fontSize: '11px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                fontFamily: 'monospace'
              }}>
                RISK METRICS
              </span>
            </div>
            {expandedSections.metrics ? (
              <ChevronUp size={14} color={FINCEPT.GRAY} />
            ) : (
              <ChevronDown size={14} color={FINCEPT.GRAY} />
            )}
          </button>

          {expandedSections.metrics && (
            <div style={{
              padding: '16px',
              borderTop: `1px solid ${FINCEPT.BORDER}`
            }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                {/* Equal Weight Metrics */}
                <div>
                  <h3 style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    marginBottom: '12px',
                    color: FINCEPT.ORANGE,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px'
                  }}>
                    EQUAL WEIGHT SCENARIOS
                  </h3>
                  {analysisResult.metrics_equal_weight && (
                    <div className="space-y-2">
                      <MetricRow
                        label="Expected Return"
                        value={formatPercent(
                          analysisResult.metrics_equal_weight.expected_return
                        )}
                        positive={
                          analysisResult.metrics_equal_weight.expected_return > 0
                        }
                      />
                      <MetricRow
                        label="Volatility"
                        value={formatPercent(
                          analysisResult.metrics_equal_weight.volatility
                        )}
                      />
                      <MetricRow
                        label={`VaR (${(1 - alpha) * 100}%)`}
                        value={formatPercent(analysisResult.metrics_equal_weight.var)}
                        negative
                      />
                      <MetricRow
                        label={`CVaR (${(1 - alpha) * 100}%)`}
                        value={formatPercent(analysisResult.metrics_equal_weight.cvar)}
                        negative
                      />
                      <MetricRow
                        label="Sharpe Ratio"
                        value={formatRatio(
                          analysisResult.metrics_equal_weight.sharpe_ratio
                        )}
                        positive={
                          analysisResult.metrics_equal_weight.sharpe_ratio > 0
                        }
                      />
                    </div>
                  )}
                </div>

                {/* Exp Decay Metrics */}
                <div>
                  <h3 style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    marginBottom: '12px',
                    color: FINCEPT.CYAN,
                    fontFamily: 'monospace',
                    letterSpacing: '0.5px'
                  }}>
                    EXP DECAY WEIGHTED ({halfLife}d)
                  </h3>
                  {analysisResult.metrics_exp_decay && (
                    <div className="space-y-2">
                      <MetricRow
                        label="Expected Return"
                        value={formatPercent(
                          analysisResult.metrics_exp_decay.expected_return
                        )}
                        positive={
                          analysisResult.metrics_exp_decay.expected_return > 0
                        }
                      />
                      <MetricRow
                        label="Volatility"
                        value={formatPercent(
                          analysisResult.metrics_exp_decay.volatility
                        )}
                      />
                      <MetricRow
                        label={`VaR (${(1 - alpha) * 100}%)`}
                        value={formatPercent(analysisResult.metrics_exp_decay.var)}
                        negative
                      />
                      <MetricRow
                        label={`CVaR (${(1 - alpha) * 100}%)`}
                        value={formatPercent(analysisResult.metrics_exp_decay.cvar)}
                        negative
                      />
                      <MetricRow
                        label="Sharpe Ratio"
                        value={formatRatio(
                          analysisResult.metrics_exp_decay.sharpe_ratio
                        )}
                        positive={analysisResult.metrics_exp_decay.sharpe_ratio > 0}
                      />
                    </div>
                  )}
                </div>
              </div>

              {/* Summary */}
              <div style={{
                marginTop: '16px',
                padding: '12px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '24px',
                  fontSize: '10px',
                  fontFamily: 'monospace'
                }}>
                  <span style={{ color: FINCEPT.GRAY }}>
                    Scenarios:{' '}
                    <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
                      {analysisResult.n_scenarios}
                    </span>
                  </span>
                  <span style={{ color: FINCEPT.GRAY }}>
                    Assets:{' '}
                    <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
                      {analysisResult.n_assets}
                    </span>
                  </span>
                  <span style={{ color: FINCEPT.GRAY }}>
                    Alpha:{' '}
                    <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{alpha}</span>
                  </span>
                </div>
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
};
