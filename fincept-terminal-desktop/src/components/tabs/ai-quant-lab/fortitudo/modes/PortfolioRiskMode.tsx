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
    <>
      {/* Configuration */}
      <div
        className="rounded border"
        style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
      >
        <button
          onClick={() => toggleSection('config')}
          className="w-full flex items-center justify-between p-3"
        >
          <div className="flex items-center gap-2">
            <Settings size={16} color={FINCEPT.ORANGE} />
            <span
              className="text-sm font-bold uppercase"
              style={{ color: FINCEPT.WHITE }}
            >
              CONFIGURATION
            </span>
          </div>
          {expandedSections.config ? (
            <ChevronUp size={16} color={FINCEPT.GRAY} />
          ) : (
            <ChevronDown size={16} color={FINCEPT.GRAY} />
          )}
        </button>

        {expandedSections.config && (
          <div
            className="p-4 border-t space-y-4"
            style={{ borderColor: FINCEPT.BORDER }}
          >
            <div className="grid grid-cols-3 gap-4">
              <div>
                <label
                  className="block text-xs font-mono mb-1"
                  style={{ color: FINCEPT.GRAY }}
                >
                  PORTFOLIO WEIGHTS
                </label>
                <input
                  type="text"
                  value={weights}
                  onChange={(e) => setWeights(e.target.value)}
                  className="w-full px-3 py-2 rounded text-sm font-mono border"
                  style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    borderColor: FINCEPT.BORDER,
                    color: FINCEPT.WHITE
                  }}
                />
              </div>
              <div>
                <label
                  className="block text-xs font-mono mb-1"
                  style={{ color: FINCEPT.GRAY }}
                >
                  ALPHA (VaR/CVaR)
                </label>
                <input
                  type="number"
                  value={alpha}
                  onChange={(e) => setAlpha(parseFloat(e.target.value))}
                  step="0.01"
                  min="0.01"
                  max="0.50"
                  className="w-full px-3 py-2 rounded text-sm font-mono border"
                  style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    borderColor: FINCEPT.BORDER,
                    color: FINCEPT.WHITE
                  }}
                />
              </div>
              <div>
                <label
                  className="block text-xs font-mono mb-1"
                  style={{ color: FINCEPT.GRAY }}
                >
                  HALF-LIFE (DAYS)
                </label>
                <input
                  type="number"
                  value={halfLife}
                  onChange={(e) => setHalfLife(parseInt(e.target.value))}
                  min="20"
                  max="504"
                  className="w-full px-3 py-2 rounded text-sm font-mono border"
                  style={{
                    backgroundColor: FINCEPT.DARK_BG,
                    borderColor: FINCEPT.BORDER,
                    color: FINCEPT.WHITE
                  }}
                />
              </div>
            </div>

            <button
              onClick={runPortfolioAnalysis}
              disabled={isLoading}
              className="w-full py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
              style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
            >
              {isLoading ? (
                <RefreshCw size={16} className="animate-spin" />
              ) : (
                <Play size={16} />
              )}
              RUN PORTFOLIO ANALYSIS
            </button>
          </div>
        )}
      </div>

      {/* Results */}
      {analysisResult && (
        <div
          className="rounded border"
          style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
        >
          <button
            onClick={() => toggleSection('metrics')}
            className="w-full flex items-center justify-between p-3"
          >
            <div className="flex items-center gap-2">
              <BarChart2 size={16} color={FINCEPT.CYAN} />
              <span
                className="text-sm font-bold uppercase"
                style={{ color: FINCEPT.WHITE }}
              >
                RISK METRICS
              </span>
            </div>
            {expandedSections.metrics ? (
              <ChevronUp size={16} color={FINCEPT.GRAY} />
            ) : (
              <ChevronDown size={16} color={FINCEPT.GRAY} />
            )}
          </button>

          {expandedSections.metrics && (
            <div className="p-4 border-t" style={{ borderColor: FINCEPT.BORDER }}>
              <div className="grid grid-cols-2 gap-6">
                {/* Equal Weight Metrics */}
                <div>
                  <h3
                    className="text-xs font-bold uppercase mb-3"
                    style={{ color: FINCEPT.ORANGE }}
                  >
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
                  <h3
                    className="text-xs font-bold uppercase mb-3"
                    style={{ color: FINCEPT.CYAN }}
                  >
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
              <div
                className="mt-4 p-3 rounded"
                style={{ backgroundColor: FINCEPT.DARK_BG }}
              >
                <div className="flex items-center gap-6 text-xs font-mono">
                  <span style={{ color: FINCEPT.GRAY }}>
                    Scenarios:{' '}
                    <span style={{ color: FINCEPT.WHITE }}>
                      {analysisResult.n_scenarios}
                    </span>
                  </span>
                  <span style={{ color: FINCEPT.GRAY }}>
                    Assets:{' '}
                    <span style={{ color: FINCEPT.WHITE }}>
                      {analysisResult.n_assets}
                    </span>
                  </span>
                  <span style={{ color: FINCEPT.GRAY }}>
                    Alpha:{' '}
                    <span style={{ color: FINCEPT.WHITE }}>{alpha}</span>
                  </span>
                </div>
              </div>
            </div>
          )}
        </div>
      )}
    </>
  );
};
