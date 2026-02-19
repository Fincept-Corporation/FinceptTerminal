/**
 * StatsRibbon - Horizontal metrics row matching VBMetricsCards style.
 * Label on top, big value, sublabel below. Dark card bg, no borders.
 */

import React, { useMemo } from 'react';
import { FINCEPT, TYPOGRAPHY, getValueColor } from '../../portfolio-tab/finceptStyles';
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
      { label: 'MAX DD', value: fmtPct(perf?.max_drawdown), sub: 'PEAK-TROUGH', color: FINCEPT.RED },
      { label: 'WIN RATE', value: perf?.win_rate != null ? `${fmt(perf.win_rate)}%` : '--', sub: 'TRADES', color: FINCEPT.GREEN },
      { label: 'P. FACTOR', value: fmt(perf?.profit_factor), sub: 'GROSS P/L', color: perf?.profit_factor > 1 ? FINCEPT.GREEN : FINCEPT.RED },
      { label: 'TRADES', value: perf?.total_trades != null ? String(perf.total_trades) : '--', sub: 'EXECUTED', color: FINCEPT.WHITE },
      { label: 'ANN. RETURN', value: fmtPct(perf?.annual_return), sub: 'CAGR', color: perf?.annual_return != null ? getValueColor(perf.annual_return) : FINCEPT.GRAY },
      { label: 'VOLATILITY', value: perf?.annual_volatility != null ? `${fmt(perf.annual_volatility)}%` : '--', sub: 'ANN.', color: FINCEPT.ORANGE },
      { label: 'SORTINO', value: fmt(perf?.sortino_ratio), sub: 'DOWNSIDE', color: FINCEPT.CYAN },
      { label: 'CALMAR', value: fmt(perf?.calmar_ratio), sub: 'RATIO', color: FINCEPT.YELLOW },
      { label: 'PROVIDER', value: selectedProvider.toUpperCase(), sub: `${state.providerCommands.length} CMDS`, color: providerColor },
    ];
  }, [result, symbols, activeCommand, selectedProvider, providerColor, state.providerCommands.length]);

  return (
    <div style={{
      flexShrink: 0,
      padding: '6px 8px',
      display: 'flex',
      gap: '4px',
      borderBottom: `1px solid #2a2a2a`,
      backgroundColor: '#000000',
      overflowX: 'auto',
      fontFamily: '"IBM Plex Mono", Consolas, monospace',
    }}>
      {cells.map((cell, i) => (
        <div
          key={i}
          style={{
            flex: '1 1 0',
            minWidth: '72px',
            padding: '8px 10px',
            backgroundColor: '#0f0f0f',
            borderRadius: '2px',
            display: 'flex',
            flexDirection: 'column',
            justifyContent: 'center',
            gap: '2px',
          }}
        >
          <div style={{
            fontSize: '8px', fontWeight: 700, color: '#787878',
            letterSpacing: '0.5px', lineHeight: 1,
          }}>
            {cell.label}
          </div>
          <div style={{
            fontSize: '13px', fontWeight: 800,
            color: isRunning && i > 0 && i < 11 ? FINCEPT.ORANGE : cell.color,
            lineHeight: 1, letterSpacing: '-0.3px',
            fontFamily: '"IBM Plex Mono", Consolas, monospace',
          }}>
            {isRunning && i > 0 && i < 11 ? '...' : cell.value}
          </div>
          <div style={{ fontSize: '8px', color: '#4a4a4a', lineHeight: 1 }}>
            {cell.sub}
          </div>
        </div>
      ))}
    </div>
  );
};
