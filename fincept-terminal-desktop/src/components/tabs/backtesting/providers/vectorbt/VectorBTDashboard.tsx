/**
 * VectorBTDashboard - Quant Research Studio layout
 *
 * NEW 2-column design:
 *   CommandBar (52px) → [StrategyPanel (300px) | MetricsCards + Chart + BottomTabs] → StatusBar
 *
 * Removed: VBStatsRibbon (replaced by VBMetricsCards), VBMetricsPanel (metrics moved to cards row)
 *
 * Non-backtest commands route to VBNonBacktestView in the right area.
 */

import React from 'react';
import { FINCEPT, COMMON_STYLES } from '../../../portfolio-tab/finceptStyles';
import { StatusBar } from '../../components/StatusBar';
import { VBCommandBar } from './VBCommandBar';
import { VBStrategyPanel } from './VBStrategyPanel';
import { VBMetricsCards } from './VBMetricsCards';
import { VBChartPanel } from './VBChartPanel';
import { VBBottomTabs } from './VBBottomTabs';
import { VBNonBacktestView } from './VBNonBacktestView';
import type { BacktestingState } from '../../types';

interface VectorBTDashboardProps {
  state: BacktestingState;
}

const BACKTEST_COMMANDS = new Set(['backtest', 'optimize', 'walk_forward']);

export const VectorBTDashboard: React.FC<VectorBTDashboardProps> = ({ state }) => {
  const isBacktestCommand = BACKTEST_COMMANDS.has(state.activeCommand);

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      overflow: 'hidden',
    }}>
      <style>{`
        ${COMMON_STYLES.scrollbarCSS}
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: translateY(0); } }
        .vb-row:hover { background-color: ${FINCEPT.HOVER} !important; }
        .vb-pill:hover { border-color: ${FINCEPT.ORANGE} !important; color: ${FINCEPT.WHITE} !important; }
        .vb-strat:hover { background-color: ${FINCEPT.HOVER} !important; }
      `}</style>

      {/* ═══ COMMAND BAR (52px) ═══ */}
      <VBCommandBar state={state} />

      {/* ═══ MAIN CONTENT (2-column) ═══ */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* LEFT: Strategy Panel (300px) */}
        <VBStrategyPanel state={state} />

        {/* RIGHT: Everything else */}
        {isBacktestCommand ? (
          <div style={{
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
          }}>
            {/* Metrics Cards Row (~90px) */}
            <VBMetricsCards state={state} />

            {/* Chart (flex: 1, min 280px) */}
            <VBChartPanel state={state} />

            {/* Bottom Tabs (220px) */}
            <VBBottomTabs state={state} />
          </div>
        ) : (
          /* Non-backtest: full-width results */
          <VBNonBacktestView state={state} />
        )}
      </div>

      {/* ═══ STATUS BAR (24px) ═══ */}
      <StatusBar state={state} />
    </div>
  );
};
