import React from 'react';
import { Target, Download } from 'lucide-react';
import { PortfolioSummary } from '../../../../../../services/portfolio/portfolioService';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../../../finceptStyles';
import {
  OptimizationConfig,
  OptimizationResults,
  LIBRARIES,
  PYPORTFOLIOOPT_METHODS,
  SKFOLIO_METHODS,
  PYPORTFOLIOOPT_OBJECTIVES,
  SKFOLIO_OBJECTIVES,
  SKFOLIO_RISK_MEASURES,
  EXPECTED_RETURNS_METHODS,
  RISK_MODELS,
} from '../types';

interface OptimizeTabProps {
  library: 'pyportfolioopt' | 'skfolio';
  setLibrary: (lib: 'pyportfolioopt' | 'skfolio') => void;
  config: OptimizationConfig;
  setConfig: React.Dispatch<React.SetStateAction<OptimizationConfig>>;
  skfolioConfig: {
    risk_measure: string;
    covariance_estimator: string;
    train_test_split: number;
    l1_coef: number;
    l2_coef: number;
  };
  setSkfolioConfig: React.Dispatch<React.SetStateAction<{
    risk_measure: string;
    covariance_estimator: string;
    train_test_split: number;
    l1_coef: number;
    l2_coef: number;
  }>>;
  assetSymbols: string;
  setAssetSymbols: (s: string) => void;
  results: OptimizationResults | null;
  loading: boolean;
  loadingAction: string | null;
  portfolioSummary: PortfolioSummary | null;
  handleOptimize: () => void;
  handleExport: (format: 'json' | 'csv') => void;
}

export const OptimizeTab: React.FC<OptimizeTabProps> = ({
  library,
  setLibrary,
  config,
  setConfig,
  skfolioConfig,
  setSkfolioConfig,
  assetSymbols,
  setAssetSymbols,
  results,
  loading,
  loadingAction,
  portfolioSummary,
  handleOptimize,
  handleExport,
}) => {
  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
      {/* Configuration Panel */}
      <div>
        <div style={{
          ...COMMON_STYLES.sectionHeader,
          borderBottom: `1px solid ${FINCEPT.ORANGE}`,
          paddingBottom: SPACING.MEDIUM
        }}>
          OPTIMIZATION CONFIGURATION
        </div>

        {/* Library Selection */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            OPTIMIZATION LIBRARY
          </label>
          <select
            value={library}
            onChange={(e) => {
              setLibrary(e.target.value as 'pyportfolioopt' | 'skfolio');
              // Reset config based on library
              if (e.target.value === 'skfolio') {
                setConfig(prev => ({ ...prev, optimization_method: 'mean_risk', objective: 'maximize_ratio' }));
              } else {
                setConfig(prev => ({ ...prev, optimization_method: 'efficient_frontier', objective: 'max_sharpe' }));
              }
            }}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          >
            {LIBRARIES.map(lib => (
              <option key={lib.value} value={lib.value}>{lib.label} - {lib.description}</option>
            ))}
          </select>
        </div>

        {/* Portfolio Info */}
        {portfolioSummary && portfolioSummary.portfolio && (
          <div style={{
            marginBottom: SPACING.LARGE,
            padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD,
            borderLeft: `3px solid ${FINCEPT.CYAN}`,
            borderRadius: '2px'
          }}>
            <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
              OPTIMIZING PORTFOLIO
            </div>
            <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>
              {portfolioSummary.portfolio.name}
            </div>
            <div style={{ ...COMMON_STYLES.dataLabel, marginTop: SPACING.SMALL }}>
              {portfolioSummary.holdings?.length || 0} holdings • Total Value: {new Intl.NumberFormat('en-US', {
                style: 'currency',
                currency: portfolioSummary.portfolio.currency || 'USD'
              }).format(portfolioSummary.total_market_value || 0)}
            </div>
          </div>
        )}

        {/* Asset Symbols */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            ASSET SYMBOLS (comma-separated)
            {portfolioSummary?.holdings && portfolioSummary.holdings.length > 0 && (
              <span style={{ color: FINCEPT.CYAN, marginLeft: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
                • From portfolio
              </span>
            )}
          </label>
          <input
            type="text"
            value={assetSymbols}
            onChange={(e) => setAssetSymbols(e.target.value)}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
            placeholder="AAPL,GOOGL,MSFT,AMZN,TSLA"
          />
          {(!portfolioSummary || portfolioSummary.holdings.length === 0) && (
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.ORANGE, marginTop: SPACING.SMALL }}>
              No portfolio selected. Using default symbols.
            </div>
          )}
        </div>

        {/* Optimization Method */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            OPTIMIZATION METHOD
          </label>
          <select
            value={config.optimization_method}
            onChange={(e) => setConfig({ ...config, optimization_method: e.target.value })}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          >
            {(library === 'skfolio' ? SKFOLIO_METHODS : PYPORTFOLIOOPT_METHODS).map(m => (
              <option key={m.value} value={m.value}>{m.label} - {m.description}</option>
            ))}
          </select>
        </div>

        {/* Objective */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            OBJECTIVE FUNCTION
          </label>
          <select
            value={config.objective}
            onChange={(e) => setConfig({ ...config, objective: e.target.value })}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          >
            {(library === 'skfolio' ? SKFOLIO_OBJECTIVES : PYPORTFOLIOOPT_OBJECTIVES).map(o => (
              <option key={o.value} value={o.value}>{o.label}</option>
            ))}
          </select>
        </div>

        {/* skfolio-specific: Risk Measure */}
        {library === 'skfolio' && (
          <div style={{ marginBottom: SPACING.LARGE }}>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
              RISK MEASURE
            </label>
            <select
              value={skfolioConfig.risk_measure}
              onChange={(e) => setSkfolioConfig({ ...skfolioConfig, risk_measure: e.target.value })}
              style={{
                ...COMMON_STYLES.inputField,
                transition: EFFECTS.TRANSITION_STANDARD,
              }}
            >
              {SKFOLIO_RISK_MEASURES.map(r => (
                <option key={r.value} value={r.value}>{r.label}</option>
              ))}
            </select>
          </div>
        )}

        {/* Expected Returns Method */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            EXPECTED RETURNS METHOD
          </label>
          <select
            value={config.expected_returns_method}
            onChange={(e) => setConfig({ ...config, expected_returns_method: e.target.value })}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          >
            {EXPECTED_RETURNS_METHODS.map(m => (
              <option key={m.value} value={m.value}>{m.label}</option>
            ))}
          </select>
        </div>

        {/* Risk Model */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            RISK MODEL
          </label>
          <select
            value={config.risk_model_method}
            onChange={(e) => setConfig({ ...config, risk_model_method: e.target.value })}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          >
            {RISK_MODELS.map(m => (
              <option key={m.value} value={m.value}>{m.label}</option>
            ))}
          </select>
        </div>

        {/* Risk-Free Rate */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            RISK-FREE RATE (%)
          </label>
          <input
            type="text"
            inputMode="decimal"
            value={config.risk_free_rate * 100}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, risk_free_rate: (parseFloat(v) || 0) / 100 }); }}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          />
        </div>

        {/* Weight Bounds */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
              MIN WEIGHT (%)
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={config.weight_bounds_min * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, weight_bounds_min: (parseFloat(v) || 0) / 100 }); }}
              style={{
                ...COMMON_STYLES.inputField,
                transition: EFFECTS.TRANSITION_STANDARD,
              }}
            />
          </div>
          <div>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
              MAX WEIGHT (%)
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={config.weight_bounds_max * 100}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, weight_bounds_max: (parseFloat(v) || 0) / 100 }); }}
              style={{
                ...COMMON_STYLES.inputField,
                transition: EFFECTS.TRANSITION_STANDARD,
              }}
            />
          </div>
        </div>

        {/* L2 Regularization */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            L2 REGULARIZATION (Gamma)
          </label>
          <input
            type="text"
            inputMode="decimal"
            value={config.gamma}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, gamma: parseFloat(v) || 0 }); }}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          />
        </div>

        {/* Portfolio Value */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
            TOTAL PORTFOLIO VALUE ($)
          </label>
          <input
            type="text"
            inputMode="numeric"
            value={config.total_portfolio_value}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setConfig({ ...config, total_portfolio_value: parseInt(v) || 0 }); }}
            style={{
              ...COMMON_STYLES.inputField,
              transition: EFFECTS.TRANSITION_STANDARD,
            }}
          />
        </div>

        {/* Optimize Button */}
        <button
          onClick={handleOptimize}
          disabled={loadingAction === 'optimize'}
          style={{
            width: '100%',
            padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
            backgroundColor: loadingAction === 'optimize' ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: loadingAction === 'optimize' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            border: loadingAction === 'optimize' ? `1px solid ${FINCEPT.GRAY}` : 'none',
            borderRadius: '2px',
            fontFamily: TYPOGRAPHY.MONO,
            fontSize: TYPOGRAPHY.SMALL,
            fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase' as const,
            cursor: loadingAction === 'optimize' ? 'not-allowed' : 'pointer',
            opacity: loadingAction === 'optimize' ? 0.6 : 1,
            transition: EFFECTS.TRANSITION_STANDARD,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: SPACING.MEDIUM
          }}
        >
          <Target size={16} />
          {loadingAction === 'optimize' ? 'OPTIMIZING...' : 'RUN OPTIMIZATION'}
        </button>
        {loadingAction === 'optimize' && (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM, marginTop: SPACING.DEFAULT }}>
            <div style={{
              width: '16px',
              height: '16px',
              border: `2px solid ${FINCEPT.BORDER}`,
              borderTop: `2px solid ${FINCEPT.CYAN}`,
              borderRadius: '50%',
              animation: 'spin 0.8s linear infinite'
            }} />
            <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Running optimization...</span>
          </div>
        )}
      </div>

      {/* Results Panel */}
      <div>
        <div style={{
          ...COMMON_STYLES.sectionHeader,
          borderBottom: `1px solid ${FINCEPT.ORANGE}`,
          paddingBottom: SPACING.MEDIUM
        }}>
          OPTIMIZATION RESULTS
        </div>

        {results?.performance_metrics && (
          <>
            {/* Performance Metrics */}
            <div style={{ marginBottom: SPACING.XLARGE }}>
              <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                PERFORMANCE METRICS
              </div>
              <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.DEFAULT, border: BORDERS.STANDARD, borderRadius: '2px' }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT, fontSize: TYPOGRAPHY.SMALL }}>
                  <div>
                    <div style={{ color: FINCEPT.GRAY }}>Expected Annual Return</div>
                    <div style={{ color: FINCEPT.GREEN, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                      {(results.performance_metrics.expected_annual_return * 100).toFixed(2)}%
                    </div>
                  </div>
                  <div>
                    <div style={{ color: FINCEPT.GRAY }}>Annual Volatility</div>
                    <div style={{ color: FINCEPT.YELLOW, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                      {results.performance_metrics.annual_volatility > 0
                        ? `${(results.performance_metrics.annual_volatility * 100).toFixed(2)}%`
                        : 'N/A'}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</div>
                    <div style={{ color: FINCEPT.ORANGE, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                      {results.performance_metrics.sharpe_ratio.toFixed(3)}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: FINCEPT.GRAY }}>Sortino Ratio</div>
                    <div style={{ color: FINCEPT.CYAN, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                      {results.performance_metrics.sortino_ratio?.toFixed(3) || 'N/A'}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: FINCEPT.GRAY }}>Calmar Ratio</div>
                    <div style={{ color: FINCEPT.PURPLE, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                      {results.performance_metrics.calmar_ratio?.toFixed(3) || 'N/A'}
                    </div>
                  </div>
                  <div>
                    <div style={{ color: FINCEPT.GRAY }}>Max Drawdown</div>
                    <div style={{ color: FINCEPT.RED, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                      {results.performance_metrics.max_drawdown
                        ? `${(results.performance_metrics.max_drawdown * 100).toFixed(2)}%`
                        : 'N/A'}
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* Portfolio Weights */}
            {results.weights && (
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                  OPTIMAL WEIGHTS
                </div>
                <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', maxHeight: '300px', overflow: 'auto' }}>
                  {Object.entries(results.weights)
                    .sort(([, a], [, b]) => b - a)
                    .map(([symbol, weight]) => (
                      <div
                        key={symbol}
                        style={{
                          padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                          borderBottom: `1px solid ${FINCEPT.HEADER_BG}`,
                          display: 'flex',
                          justifyContent: 'space-between',
                          alignItems: 'center',
                          fontSize: TYPOGRAPHY.SMALL,
                          transition: EFFECTS.TRANSITION_STANDARD,
                        }}
                      >
                        <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{symbol}</span>
                        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                          <div
                            style={{
                              width: '100px',
                              height: '6px',
                              backgroundColor: FINCEPT.HEADER_BG,
                              borderRadius: '2px',
                              position: 'relative',
                              overflow: 'hidden'
                            }}
                          >
                            <div
                              style={{
                                position: 'absolute',
                                left: 0,
                                top: 0,
                                height: '100%',
                                width: `${weight * 100}%`,
                                backgroundColor: FINCEPT.ORANGE
                              }}
                            />
                          </div>
                          <span style={{ color: FINCEPT.WHITE, width: '60px', textAlign: 'right' }}>
                            {(weight * 100).toFixed(2)}%
                          </span>
                        </div>
                      </div>
                    ))}
                </div>
              </div>
            )}

            {/* Export Buttons */}
            <div style={{ display: 'flex', gap: SPACING.MEDIUM }}>
              <button
                onClick={() => handleExport('json')}
                style={{
                  flex: 1,
                  padding: `${SPACING.MEDIUM} ${SPACING.LARGE}`,
                  backgroundColor: FINCEPT.GREEN,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontFamily: TYPOGRAPHY.MONO,
                  fontSize: TYPOGRAPHY.TINY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  textTransform: 'uppercase' as const,
                  cursor: 'pointer',
                  transition: EFFECTS.TRANSITION_STANDARD,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: SPACING.SMALL
                }}
              >
                <Download size={12} />
                EXPORT JSON
              </button>
              <button
                onClick={() => handleExport('csv')}
                style={{
                  flex: 1,
                  padding: `${SPACING.MEDIUM} ${SPACING.LARGE}`,
                  backgroundColor: FINCEPT.CYAN,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontFamily: TYPOGRAPHY.MONO,
                  fontSize: TYPOGRAPHY.TINY,
                  fontWeight: TYPOGRAPHY.BOLD,
                  letterSpacing: TYPOGRAPHY.WIDE,
                  textTransform: 'uppercase' as const,
                  cursor: 'pointer',
                  transition: EFFECTS.TRANSITION_STANDARD,
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: SPACING.SMALL
                }}
              >
                <Download size={12} />
                EXPORT CSV
              </button>
            </div>
          </>
        )}

        {!results && !loading && (
          <div style={{
            padding: '48px',
            textAlign: 'center',
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.BODY,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD,
            borderRadius: '2px'
          }}>
            Configure parameters and click "RUN OPTIMIZATION" to see results
          </div>
        )}
      </div>
    </div>
  );
};
