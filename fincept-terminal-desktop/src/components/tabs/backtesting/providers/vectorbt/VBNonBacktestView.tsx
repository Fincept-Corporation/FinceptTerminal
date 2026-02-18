/**
 * VBNonBacktestView - Full-width results for non-backtest commands
 * (indicator, indicator_signals, labels, splits, returns, signals)
 */

import React from 'react';
import { FINCEPT, COMMON_STYLES, BORDERS } from '../../../portfolio-tab/finceptStyles';
import {
  IndicatorResult,
  SignalsResult,
  LabelsResult,
  SplittersResult,
  ReturnsResult,
  SignalsGeneratorResult,
  ResultRaw,
} from '../../results';
import type { BacktestingState } from '../../types';

interface VBNonBacktestViewProps {
  state: BacktestingState;
}

const COMMAND_LABELS: Record<string, string> = {
  indicator: 'INDICATOR ANALYSIS',
  indicator_signals: 'INDICATOR SIGNALS',
  labels: 'ML LABEL GENERATION',
  splits: 'CROSS-VALIDATION SPLITS',
  returns: 'RETURNS ANALYSIS',
  signals: 'SIGNAL GENERATION',
};

export const VBNonBacktestView: React.FC<VBNonBacktestViewProps> = ({ state }) => {
  const { activeCommand, result, error, isRunning } = state;

  const renderResult = () => {
    if (!result) return null;

    switch (activeCommand) {
      case 'indicator':
        return <IndicatorResult result={result} />;
      case 'indicator_signals':
        return <SignalsResult result={result} />;
      case 'labels':
        return <LabelsResult result={result} />;
      case 'splits':
        return <SplittersResult result={result} />;
      case 'returns':
        return <ReturnsResult result={result} />;
      case 'signals':
        return <SignalsGeneratorResult result={result} />;
      default:
        return <ResultRaw result={result} />;
    }
  };

  return (
    <div style={{
      flex: 1,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: '8px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        flexShrink: 0,
      }}>
        <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>
          {COMMAND_LABELS[activeCommand] || activeCommand.toUpperCase()}
        </span>

        {result && (
          <span style={{
            ...COMMON_STYLES.badgeSuccess,
            fontSize: '9px',
          }}>
            COMPLETED
          </span>
        )}

        {isRunning && (
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${FINCEPT.ORANGE}20`,
            color: FINCEPT.ORANGE,
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
            animation: 'pulse 1s infinite',
          }}>
            RUNNING...
          </span>
        )}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {error && (
          <div style={{
            padding: '12px',
            backgroundColor: `${FINCEPT.RED}10`,
            border: BORDERS.RED,
            borderRadius: '2px',
            marginBottom: '12px',
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '4px' }}>ERROR</div>
            <div style={{ fontSize: '10px', color: FINCEPT.WHITE, lineHeight: 1.5 }}>{error}</div>
          </div>
        )}

        {!result && !error && !isRunning && (
          <div style={{
            ...COMMON_STYLES.emptyState,
            height: '300px',
          }}>
            <div style={{ fontSize: '12px', color: FINCEPT.MUTED, marginBottom: '4px' }}>
              No results yet
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
              Configure parameters and click RUN to execute
            </div>
          </div>
        )}

        {isRunning && !result && (
          <div style={{
            ...COMMON_STYLES.emptyState,
            height: '300px',
          }}>
            <div style={{ fontSize: '12px', color: FINCEPT.ORANGE, fontWeight: 700, animation: 'pulse 1s infinite' }}>
              Executing...
            </div>
          </div>
        )}

        {renderResult()}
      </div>
    </div>
  );
};
