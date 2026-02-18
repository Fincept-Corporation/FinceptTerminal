/**
 * ResultsPanel - Right sidebar (320px): result view routing
 * Dense terminal layout with tab-style view switcher, error/warning alerts
 */

import React from 'react';
import { BarChart3, Loader, AlertCircle } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, EFFECTS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import type { BacktestingState } from '../types';
import {
  ResultSummary, ResultMetrics, ResultTrades, ResultRaw, SignalsResult, IndicatorResult,
  ResultCharts, ResultDrawdowns, LabelsResult, SplittersResult, ReturnsResult, SignalsGeneratorResult,
} from '../results';

interface ResultsPanelProps {
  state: BacktestingState;
}

export const ResultsPanel: React.FC<ResultsPanelProps> = ({ state }) => {
  const { activeCommand, isRunning, result, error, resultView, setResultView } = state;

  return (
    <div style={{
      width: '320px',
      minWidth: '320px',
      flexShrink: 0,
      backgroundColor: FINCEPT.DARK_BG,
      borderLeft: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        gap: '6px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <BarChart3 size={10} color={FINCEPT.ORANGE} style={{ filter: EFFECTS.ICON_GLOW_ORANGE }} />
          <span style={{
            fontSize: '11px',
            fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
          }}>
            RESULTS
          </span>
          {result && !isRunning && (
            <span style={{
              ...COMMON_STYLES.badgeSuccess,
              marginLeft: 'auto',
            }}>
              OK
            </span>
          )}
          {error && (
            <span style={{
              ...COMMON_STYLES.badgeError,
              marginLeft: 'auto',
            }}>
              ERR
            </span>
          )}
        </div>

        {/* View Tabs (for backtest command with results) */}
        {activeCommand === 'backtest' && result && !isRunning && (
          <div style={{
            display: 'flex',
            gap: '2px',
            padding: '2px',
            backgroundColor: FINCEPT.DARK_BG,
            borderRadius: '2px',
            flexWrap: 'wrap',
          }}>
            {(['summary', 'metrics', 'charts', 'trades', 'raw'] as const).map(view => (
              <button
                key={view}
                onClick={() => setResultView(view)}
                style={COMMON_STYLES.tabButton(resultView === view)}
              >
                {view.toUpperCase()}
              </button>
            ))}
          </div>
        )}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto', padding: SPACING.DEFAULT }}>
        {/* Error */}
        {error && (
          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: `${FINCEPT.RED}20`,
            border: `1px solid ${FINCEPT.RED}`,
            borderRadius: '2px',
            marginBottom: SPACING.MEDIUM,
          }}>
            <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
              <AlertCircle size={12} color={FINCEPT.RED} style={{ marginTop: '1px', flexShrink: 0 }} />
              <div>
                <div style={{
                  fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD,
                  color: FINCEPT.RED, marginBottom: SPACING.TINY, letterSpacing: '0.5px',
                }}>
                  ERROR
                </div>
                <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.WHITE, lineHeight: 1.4 }}>
                  {error}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Loading */}
        {isRunning && (
          <div style={{
            display: 'flex', flexDirection: 'column',
            alignItems: 'center', justifyContent: 'center',
            padding: '40px 20px', gap: SPACING.DEFAULT,
          }}>
            <Loader size={20} color={FINCEPT.ORANGE} className="animate-spin" style={{ filter: EFFECTS.ICON_GLOW_ORANGE }} />
            <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
              Running {activeCommand.toUpperCase().replace('_', ' ')}...
            </span>
          </div>
        )}

        {/* Results */}
        {result && !isRunning && (
          <div>
            {/* Synthetic data warning */}
            {(result?.data?.usingSyntheticData || result?.data?.syntheticDataWarning) && (
              <div style={{
                padding: '8px 10px',
                marginBottom: SPACING.MEDIUM,
                backgroundColor: `${FINCEPT.ORANGE}15`,
                border: `1px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                display: 'flex', alignItems: 'flex-start', gap: '6px',
              }}>
                <AlertCircle size={12} color={FINCEPT.ORANGE} style={{ marginTop: '1px', flexShrink: 0 }} />
                <div>
                  <div style={{
                    fontSize: '8px', fontWeight: 800, color: FINCEPT.ORANGE,
                    marginBottom: '2px', letterSpacing: '0.5px',
                  }}>
                    SYNTHETIC DATA
                  </div>
                  <div style={{ fontSize: '8px', color: FINCEPT.WHITE, lineHeight: 1.5, opacity: 0.85 }}>
                    {result?.data?.syntheticDataWarning || 'Results based on synthetic data. Install yfinance for real market data.'}
                  </div>
                </div>
              </div>
            )}

            {/* Backtest results */}
            {activeCommand === 'backtest' && (
              <>
                {resultView === 'summary' && <ResultSummary result={result} />}
                {resultView === 'metrics' && <ResultMetrics result={result} />}
                {resultView === 'charts' && <ResultCharts result={result} />}
                {resultView === 'trades' && <ResultTrades result={result} />}
                {resultView === 'raw' && <ResultRaw result={result} />}

                {!result?.data?.performance && (
                  <div style={{
                    padding: SPACING.DEFAULT,
                    backgroundColor: `${FINCEPT.YELLOW}20`,
                    border: `1px solid ${FINCEPT.YELLOW}`,
                    borderRadius: '2px',
                  }}>
                    <div style={{
                      fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD,
                      color: FINCEPT.YELLOW, marginBottom: SPACING.TINY,
                    }}>
                      EMPTY RESULT
                    </div>
                    <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.WHITE, lineHeight: 1.4 }}>
                      Backtest completed but returned no performance data.
                    </div>
                  </div>
                )}
              </>
            )}

            {/* Other command results */}
            {(activeCommand === 'optimize' || activeCommand === 'walk_forward') && (
              <>
                <ResultMetrics result={result} />
                <div style={{ marginTop: SPACING.DEFAULT }}><ResultRaw result={result} /></div>
              </>
            )}
            {activeCommand === 'indicator_signals' && <SignalsResult result={result} />}
            {activeCommand === 'indicator' && <IndicatorResult result={result} />}
            {activeCommand === 'labels' && <LabelsResult result={result} />}
            {activeCommand === 'splits' && <SplittersResult result={result} />}
            {activeCommand === 'returns' && <ReturnsResult result={result} />}
            {activeCommand === 'signals' && <SignalsGeneratorResult result={result} />}

            {!['backtest', 'optimize', 'walk_forward', 'indicator_signals', 'indicator',
              'labels', 'splits', 'returns', 'signals'].includes(activeCommand) && <ResultRaw result={result} />}
          </div>
        )}

        {/* Empty State */}
        {!result && !isRunning && !error && (
          <div style={{
            display: 'flex', flexDirection: 'column',
            alignItems: 'center', justifyContent: 'center',
            height: '100%', color: FINCEPT.MUTED,
            fontSize: TYPOGRAPHY.SMALL, textAlign: 'center', gap: '8px',
          }}>
            <BarChart3 size={20} style={{ opacity: 0.3 }} />
            <span style={{ letterSpacing: '0.5px' }}>NO RESULTS YET</span>
            <span style={{ fontSize: '8px' }}>Configure and run a command</span>
          </div>
        )}
      </div>
    </div>
  );
};
