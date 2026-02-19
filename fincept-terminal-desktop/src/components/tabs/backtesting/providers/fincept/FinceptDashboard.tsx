/**
 * FinceptDashboard - Mirrors VectorBTDashboard layout exactly.
 *
 * Layout:
 *   TopNavBar (40px)  ← rendered by BacktestingTab
 *   [CommandPanel (240px) | StatsRibbon + Chart + BottomTabs | ResultsPanel (320px)]
 *   StatusBar (24px)
 */

import React from 'react';
import { FINCEPT, COMMON_STYLES } from '../../../portfolio-tab/finceptStyles';
import { StatusBar } from '../../components/StatusBar';
import { CommandPanel } from '../../components/CommandPanel';
import { StatsRibbon } from '../../components/StatsRibbon';
import { ConfigPanel } from '../../components/ConfigPanel';
import { ResultsPanel } from '../../components/ResultsPanel';
import { VBChartPanel } from '../vectorbt/VBChartPanel';
import { VBBottomTabs } from '../vectorbt/VBBottomTabs';
import type { BacktestingState } from '../../types';

interface FinceptDashboardProps {
  state: BacktestingState;
}

const BACKTEST_COMMANDS = new Set(['backtest', 'optimize', 'walk_forward']);

export const FinceptDashboard: React.FC<FinceptDashboardProps> = ({ state }) => {
  const isBacktestCommand = BACKTEST_COMMANDS.has(state.activeCommand);

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: '#000000',
      overflow: 'hidden',
      fontFamily: '"IBM Plex Mono", Consolas, monospace',
    }}>
      <style>{`
        ${COMMON_STYLES.scrollbarCSS}
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: translateY(0); } }
      `}</style>

      {/* ═══ MAIN CONTENT ═══ */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>

        {/* LEFT: Command Panel (240px, NodePalette style) */}
        <CommandPanel state={state} />

        {/* CENTER+RIGHT: mirrors VectorBTDashboard right area */}
        {isBacktestCommand ? (
          <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>

            {/* CENTER: Metrics + Chart + BottomTabs (same as VBT) */}
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
              <StatsRibbon state={state} />
              <VBChartPanel state={state} />
              <VBBottomTabs state={state} />
            </div>

            {/* RIGHT: Config + Results stacked (320px) */}
            <div style={{
              width: '320px',
              flexShrink: 0,
              display: 'flex',
              flexDirection: 'column',
              overflow: 'hidden',
              borderLeft: '1px solid #2a2a2a',
            }}>
              <ConfigPanel state={state} />
            </div>
          </div>
        ) : (
          /* Non-backtest: metrics + config + results side by side */
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            <StatsRibbon state={state} />
            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
              <ConfigPanel state={state} />
              <ResultsPanel state={state} />
            </div>
          </div>
        )}
      </div>

      {/* ═══ STATUS BAR (24px) ═══ */}
      <StatusBar state={state} />
    </div>
  );
};
