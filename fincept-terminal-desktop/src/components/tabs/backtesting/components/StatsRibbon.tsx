/**
 * StatsRibbon - 52px metric cells for last backtest results
 * Matches portfolio StatsRibbon pattern: 3-line cells (label, value, sub)
 */

import React, { useMemo } from 'react';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';
import { getValueColor } from '../../portfolio-tab/finceptStyles';
import { PROVIDER_COLORS } from '../constants';
import type { BacktestingState } from '../types';

interface StatsRibbonProps {
  state: BacktestingState;
}

export const StatsRibbon: React.FC<StatsRibbonProps> = ({ state }) => {
  const { result, isRunning, selectedProvider, symbols, activeCommand } = state;
  const providerColor = PROVIDER_COLORS[selectedProvider];

  const cells = useMemo(() => {
    const perf = result?.data?.performance;
    const fmt = (v: any, dec = 2) => v != null ? Number(v).toFixed(dec) : '--';
    const fmtPct = (v: any) => v != null ? `${Number(v) >= 0 ? '+' : ''}${Number(v).toFixed(2)}%` : '--';

    return [
      { label: 'SYMBOL', value: symbols.toUpperCase() || '--', sub: activeCommand.toUpperCase().replace('_', ' '), color: FINCEPT.CYAN },
      { label: 'TOTAL RETURN', value: fmtPct(perf?.total_return), sub: 'CUMULATIVE', color: perf?.total_return != null ? getValueColor(perf.total_return) : FINCEPT.GRAY },
      { label: 'SHARPE', value: fmt(perf?.sharpe_ratio), sub: 'ANNUALIZED', color: FINCEPT.CYAN },
      { label: 'MAX DRAWDOWN', value: fmtPct(perf?.max_drawdown), sub: 'PEAK-TROUGH', color: FINCEPT.RED },
      { label: 'WIN RATE', value: perf?.win_rate != null ? `${fmt(perf.win_rate)}%` : '--', sub: 'TRADES', color: FINCEPT.GREEN },
      { label: 'PROFIT FACTOR', value: fmt(perf?.profit_factor), sub: 'GROSS P/L', color: perf?.profit_factor > 1 ? FINCEPT.GREEN : FINCEPT.RED },
      { label: 'TOTAL TRADES', value: perf?.total_trades != null ? String(perf.total_trades) : '--', sub: 'EXECUTED', color: FINCEPT.WHITE },
      { label: 'ANNUAL RETURN', value: fmtPct(perf?.annual_return), sub: 'CAGR', color: perf?.annual_return != null ? getValueColor(perf.annual_return) : FINCEPT.GRAY },
      { label: 'VOLATILITY', value: perf?.annual_volatility != null ? `${fmt(perf.annual_volatility)}%` : '--', sub: 'ANN.', color: FINCEPT.ORANGE },
      { label: 'SORTINO', value: fmt(perf?.sortino_ratio), sub: 'DOWNSIDE', color: FINCEPT.CYAN },
      { label: 'CALMAR', value: fmt(perf?.calmar_ratio), sub: 'RATIO', color: FINCEPT.YELLOW },
      { label: 'PROVIDER', value: selectedProvider.toUpperCase(), sub: `${state.providerCommands.length} CMDS`, color: providerColor },
    ];
  }, [result, symbols, activeCommand, selectedProvider, providerColor, state.providerCommands.length]);

  return (
    <div style={{
      height: '52px',
      flexShrink: 0,
      backgroundColor: '#0A0A0A',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      alignItems: 'stretch',
      overflow: 'hidden',
    }}>
      {cells.map((m, i) => (
        <div key={i} style={{
          flex: 1,
          minWidth: '80px',
          padding: '6px 8px',
          borderRight: '1px solid #141414',
          display: 'flex',
          flexDirection: 'column',
          justifyContent: 'center',
        }}>
          <div style={{
            fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700,
            letterSpacing: '0.5px', lineHeight: 1, marginBottom: '3px',
          }}>
            {m.label}
          </div>
          <div style={{
            fontSize: '14px', fontWeight: 800, color: m.color, lineHeight: 1,
          }}>
            {isRunning && i > 0 && i < 11 ? '...' : m.value}
          </div>
          <div style={{
            fontSize: '8px', color: FINCEPT.GRAY, marginTop: '2px', lineHeight: 1,
          }}>
            {m.sub}
          </div>
        </div>
      ))}
    </div>
  );
};
