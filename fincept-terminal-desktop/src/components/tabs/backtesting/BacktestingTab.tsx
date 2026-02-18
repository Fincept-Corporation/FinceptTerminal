/**
 * BacktestingTab - Bloomberg-style multi-provider backtesting terminal
 *
 * Layout: CommandBar (40px) → StatsRibbon (52px) → 3-panel content → StatusBar (24px)
 * Uses shared finceptStyles.ts design system for consistency across terminal.
 * All state lives in useBacktestingState hook.
 */

import React from 'react';
import { FINCEPT, COMMON_STYLES } from '../portfolio-tab/finceptStyles';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useBacktestingState } from './hooks';
import { TopNavBar, CommandPanel, ConfigPanel, ResultsPanel, StatusBar, StatsRibbon } from './components';
import { VectorBTDashboard } from './providers/vectorbt';

const BacktestingTab: React.FC = () => {
  const state = useBacktestingState();
  const { fontFamily } = useTerminalTheme();

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily,
      overflow: 'hidden',
    }}>
      <style>{`
        ${COMMON_STYLES.scrollbarCSS}
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.4; } }
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
        .bt-row:hover { background-color: ${FINCEPT.HOVER} !important; }
      `}</style>

      {/* ═══ PROVIDER TABS (always visible for switching) ═══ */}
      <TopNavBar state={state} />

      {state.selectedProvider === 'vectorbt' ? (
        /* ═══ VECTORBT: Full Dashboard UI ═══ */
        <VectorBTDashboard state={state} />
      ) : (
        /* ═══ GENERIC LAYOUT (other providers) ═══ */
        <>
          {/* ═══ STATS RIBBON (52px) ═══ */}
          <StatsRibbon state={state} />

          {/* ═══ MAIN CONTENT ═══ */}
          <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
            {/* LEFT: Commands + Categories (220px) */}
            <CommandPanel state={state} />

            {/* CENTER: Configuration (flex:1) */}
            <ConfigPanel state={state} />

            {/* RIGHT: Results (320px) */}
            <ResultsPanel state={state} />
          </div>

          {/* ═══ STATUS BAR (24px) ═══ */}
          <StatusBar state={state} />
        </>
      )}
    </div>
  );
};

export default BacktestingTab;
