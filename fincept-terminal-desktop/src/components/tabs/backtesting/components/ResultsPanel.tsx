/**
 * ResultsPanel - Right panel shell that routes to result views
 * Design system: flat HEADER_BG, 2px radius, tab-style view switcher
 */

import React from 'react';
import { BarChart3, Loader, AlertCircle } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from '../constants';
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
      width: '300px',
      minWidth: '300px',
      backgroundColor: FINCEPT.DARK_BG,
      borderLeft: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        gap: '8px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart3 size={12} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY,
          }}>
            RESULTS
          </span>
        </div>

        {/* View Tabs (only for backtest command with results) */}
        {activeCommand === 'backtest' && result && !isRunning && (
          <div style={{
            display: 'flex',
            gap: '2px',
            padding: '3px',
            backgroundColor: FINCEPT.DARK_BG,
            borderRadius: '2px',
            flexWrap: 'wrap',
          }}>
            {(['summary', 'metrics', 'charts', 'trades', 'raw'] as const).map(view => (
              <button
                key={view}
                onClick={() => setResultView(view)}
                style={{
                  padding: '5px 8px',
                  backgroundColor: resultView === view ? FINCEPT.ORANGE : 'transparent',
                  color: resultView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '8px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: FONT_FAMILY,
                }}
              >
                {view.toUpperCase()}
              </button>
            ))}
          </div>
        )}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
        {/* Error */}
        {error && (
          <div style={{
            padding: '12px',
            backgroundColor: `${FINCEPT.RED}20`,
            border: `1px solid ${FINCEPT.RED}`,
            borderRadius: '2px',
          }}>
            <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
              <AlertCircle size={14} color={FINCEPT.RED} style={{ marginTop: '2px' }} />
              <div>
                <div style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.RED,
                  marginBottom: '4px',
                  fontFamily: FONT_FAMILY,
                }}>
                  ERROR
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.WHITE, lineHeight: 1.4, fontFamily: FONT_FAMILY }}>
                  {error}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Loading */}
        {isRunning && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            padding: '40px 20px',
            gap: '12px',
          }}>
            <Loader size={24} color={FINCEPT.ORANGE} className="animate-spin" />
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY, fontFamily: FONT_FAMILY }}>
              Running {activeCommand}...
            </span>
          </div>
        )}

        {/* Results */}
        {result && !isRunning && (
          <div>
            {/* Synthetic data warning */}
            {(result?.data?.usingSyntheticData || result?.data?.syntheticDataWarning) && (
              <div style={{
                padding: '10px 12px',
                marginBottom: '10px',
                backgroundColor: `${FINCEPT.ORANGE}15`,
                border: `1px solid ${FINCEPT.ORANGE}`,
                borderRadius: '2px',
                display: 'flex',
                alignItems: 'flex-start',
                gap: '8px',
              }}>
                <AlertCircle size={14} color={FINCEPT.ORANGE} style={{ marginTop: '2px', flexShrink: 0 }} />
                <div>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 800,
                    color: FINCEPT.ORANGE,
                    marginBottom: '3px',
                    letterSpacing: '0.5px',
                    fontFamily: FONT_FAMILY,
                  }}>
                    SYNTHETIC DATA WARNING
                  </div>
                  <div style={{ fontSize: '8px', color: FINCEPT.WHITE, lineHeight: 1.5, opacity: 0.85, fontFamily: FONT_FAMILY }}>
                    {result?.data?.syntheticDataWarning ||
                      'This backtest used SYNTHETIC (fake) data because real market data could not be loaded. ' +
                      'Install yfinance (pip install yfinance) and ensure internet connectivity for real results. ' +
                      'These results have NO financial meaning.'}
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

                {/* Empty result warning */}
                {!result?.data?.performance && (
                  <div style={{
                    padding: '12px',
                    backgroundColor: `${FINCEPT.YELLOW}20`,
                    border: `1px solid ${FINCEPT.YELLOW}`,
                    borderRadius: '2px',
                  }}>
                    <div style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.YELLOW,
                      marginBottom: '4px',
                      fontFamily: FONT_FAMILY,
                    }}>
                      EMPTY RESULT
                    </div>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, lineHeight: 1.4, fontFamily: FONT_FAMILY }}>
                      Backtest completed but returned no performance data. Check console logs for details.
                    </div>
                  </div>
                )}
              </>
            )}

            {/* Optimize / Walk Forward - show metrics + raw */}
            {(activeCommand === 'optimize' || activeCommand === 'walk_forward') && (
              <>
                <ResultMetrics result={result} />
                <div style={{ marginTop: '12px' }}><ResultRaw result={result} /></div>
              </>
            )}

            {/* Indicator Signals */}
            {activeCommand === 'indicator_signals' && <SignalsResult result={result} />}

            {/* Indicator */}
            {activeCommand === 'indicator' && <IndicatorResult result={result} />}

            {/* Labels */}
            {activeCommand === 'labels' && <LabelsResult result={result} />}

            {/* Splitters */}
            {activeCommand === 'splits' && <SplittersResult result={result} />}

            {/* Returns */}
            {activeCommand === 'returns' && <ReturnsResult result={result} />}

            {/* Random Signals */}
            {activeCommand === 'signals' && <SignalsGeneratorResult result={result} />}

            {/* Fallback for any unknown commands */}
            {!['backtest', 'optimize', 'walk_forward', 'indicator_signals', 'indicator',
              'labels', 'splits', 'returns', 'signals'].includes(activeCommand) && <ResultRaw result={result} />}
          </div>
        )}

        {/* Empty State */}
        {!result && !isRunning && !error && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
            gap: '8px',
            fontFamily: FONT_FAMILY,
          }}>
            <BarChart3 size={24} style={{ opacity: 0.5 }} />
            <span>No results yet</span>
            <span style={{ fontSize: '9px' }}>Configure and run</span>
          </div>
        )}
      </div>
    </div>
  );
};
