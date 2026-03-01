import React from 'react';
import { Layers } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../../../finceptStyles';

interface StrategyResults {
  weights: Record<string, number>;
  performance: { expected_return: number; volatility: number; sharpe_ratio: number; [key: string]: any };
  [key: string]: any;
}

interface StrategiesTabProps {
  assetSymbols: string;
  setAssetSymbols: (s: string) => void;
  selectedStrategy: string;
  setSelectedStrategy: (s: string) => void;
  strategyResults: StrategyResults | null;
  marketNeutralLong: number;
  setMarketNeutralLong: (v: number) => void;
  marketNeutralShort: number;
  setMarketNeutralShort: (v: number) => void;
  benchmarkWeightsStr: string;
  setBenchmarkWeightsStr: (s: string) => void;
  loadingAction: string | null;
  handleStrategyOptimize: () => void;
}

export const StrategiesTab: React.FC<StrategiesTabProps> = ({
  assetSymbols,
  setAssetSymbols,
  selectedStrategy,
  setSelectedStrategy,
  strategyResults,
  marketNeutralLong,
  setMarketNeutralLong,
  marketNeutralShort,
  setMarketNeutralShort,
  benchmarkWeightsStr,
  setBenchmarkWeightsStr,
  loadingAction,
  handleStrategyOptimize,
}) => {
  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
      {/* Strategy Configuration */}
      <div>
        <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM }}>
          ADDITIONAL STRATEGIES (PyPortfolioOpt)
        </div>

        {/* Strategy Selector */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>STRATEGY</label>
          <select
            value={selectedStrategy}
            onChange={(e) => setSelectedStrategy(e.target.value)}
            style={{ ...COMMON_STYLES.inputField, transition: EFFECTS.TRANSITION_STANDARD }}
          >
            <option value="risk_parity">Risk Parity - Equal risk contribution</option>
            <option value="equal_weight">Equal Weight - 1/N portfolio</option>
            <option value="inverse_volatility">Inverse Volatility - Weight by inverse vol</option>
            <option value="market_neutral">Market Neutral - Long/short zero net exposure</option>
            <option value="min_tracking_error">Min Tracking Error - Stay close to benchmark</option>
            <option value="max_diversification">Max Diversification - Maximize diversification ratio</option>
          </select>
        </div>

        {/* Asset Symbols */}
        <div style={{ marginBottom: SPACING.LARGE }}>
          <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>ASSET SYMBOLS</label>
          <input
            type="text"
            value={assetSymbols}
            onChange={(e) => setAssetSymbols(e.target.value)}
            style={{ ...COMMON_STYLES.inputField }}
            placeholder="AAPL,GOOGL,MSFT,AMZN,TSLA"
          />
        </div>

        {/* Market Neutral Config */}
        {selectedStrategy === 'market_neutral' && (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
            <div>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>LONG EXPOSURE</label>
              <input
                type="text" inputMode="decimal"
                value={marketNeutralLong}
                onChange={(e) => setMarketNeutralLong(parseFloat(e.target.value) || 1.0)}
                style={{ ...COMMON_STYLES.inputField }}
              />
            </div>
            <div>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>SHORT EXPOSURE</label>
              <input
                type="text" inputMode="decimal"
                value={marketNeutralShort}
                onChange={(e) => setMarketNeutralShort(parseFloat(e.target.value) || -1.0)}
                style={{ ...COMMON_STYLES.inputField }}
              />
            </div>
          </div>
        )}

        {/* Min Tracking Error Config */}
        {selectedStrategy === 'min_tracking_error' && (
          <div style={{ marginBottom: SPACING.LARGE }}>
            <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
              BENCHMARK WEIGHTS (JSON)
            </label>
            <input
              type="text"
              value={benchmarkWeightsStr}
              onChange={(e) => setBenchmarkWeightsStr(e.target.value)}
              style={{ ...COMMON_STYLES.inputField }}
              placeholder='{"AAPL": 0.3, "MSFT": 0.3, "GOOGL": 0.4}'
            />
          </div>
        )}

        {/* Run Button */}
        <button
          onClick={handleStrategyOptimize}
          disabled={loadingAction === 'strategy'}
          style={{
            width: '100%',
            padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
            backgroundColor: loadingAction === 'strategy' ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: loadingAction === 'strategy' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
            border: 'none', borderRadius: '2px',
            fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL,
            fontWeight: TYPOGRAPHY.BOLD, letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase' as const,
            cursor: loadingAction === 'strategy' ? 'not-allowed' : 'pointer',
            opacity: loadingAction === 'strategy' ? 0.6 : 1,
            transition: EFFECTS.TRANSITION_STANDARD,
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM
          }}
        >
          <Layers size={16} />
          {loadingAction === 'strategy' ? 'RUNNING...' : 'RUN STRATEGY'}
        </button>
      </div>

      {/* Strategy Results */}
      <div>
        <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM }}>
          STRATEGY RESULTS
        </div>

        {strategyResults ? (
          <>
            {/* Performance Metrics */}
            {strategyResults.performance && (
              <div style={{ marginBottom: SPACING.XLARGE }}>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                  PERFORMANCE METRICS
                </div>
                <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.DEFAULT, border: BORDERS.STANDARD, borderRadius: '2px' }}>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT, fontSize: TYPOGRAPHY.SMALL }}>
                    <div>
                      <div style={{ color: FINCEPT.GRAY }}>Expected Return</div>
                      <div style={{ color: FINCEPT.GREEN, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                        {(strategyResults.performance.expected_return * 100).toFixed(2)}%
                      </div>
                    </div>
                    <div>
                      <div style={{ color: FINCEPT.GRAY }}>Volatility</div>
                      <div style={{ color: FINCEPT.YELLOW, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                        {(strategyResults.performance.volatility * 100).toFixed(2)}%
                      </div>
                    </div>
                    <div>
                      <div style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</div>
                      <div style={{ color: FINCEPT.ORANGE, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                        {strategyResults.performance.sharpe_ratio.toFixed(3)}
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Weights */}
            {strategyResults.weights && (
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                  OPTIMAL WEIGHTS
                </div>
                <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', maxHeight: '300px', overflow: 'auto' }}>
                  {Object.entries(strategyResults.weights)
                    .sort(([, a], [, b]) => Math.abs(b) - Math.abs(a))
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
                        }}
                      >
                        <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{symbol}</span>
                        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                          <div style={{ width: '100px', height: '6px', backgroundColor: FINCEPT.HEADER_BG, borderRadius: '2px', position: 'relative', overflow: 'hidden' }}>
                            <div style={{
                              position: 'absolute',
                              left: weight < 0 ? `${50 + weight * 50}%` : '50%',
                              top: 0, height: '100%',
                              width: `${Math.abs(weight) * 50}%`,
                              backgroundColor: weight >= 0 ? FINCEPT.ORANGE : FINCEPT.RED,
                            }} />
                          </div>
                          <span style={{ color: weight >= 0 ? FINCEPT.WHITE : FINCEPT.RED, width: '60px', textAlign: 'right' }}>
                            {(weight * 100).toFixed(2)}%
                          </span>
                        </div>
                      </div>
                    ))}
                </div>
              </div>
            )}
          </>
        ) : (
          <div style={{ padding: '48px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px' }}>
            Select a strategy and click "RUN STRATEGY" to see results
          </div>
        )}
      </div>
    </div>
  );
};
