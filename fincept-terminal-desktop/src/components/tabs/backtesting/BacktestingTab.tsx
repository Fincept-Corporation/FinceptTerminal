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
import { TopNavBar } from './components';
import { VectorBTDashboard } from './providers/vectorbt';
import { FinceptDashboard } from './providers/fincept/FinceptDashboard';

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
        <VectorBTDashboard state={state} />
      ) : (
        <FinceptDashboard state={state} />
      )}
    </div>
  );
};

export default BacktestingTab;
