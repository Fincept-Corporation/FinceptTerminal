/**
 * BacktestingTab - Slim orchestrator
 *
 * Multi-provider backtesting terminal with three-panel layout.
 * All state lives in useBacktestingState hook.
 * All UI lives in sub-components under components/, configs/, results/.
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from './constants';
import { useBacktestingState } from './hooks';
import { TopNavBar, CommandPanel, ConfigPanel, ResultsPanel, StatusBar } from './components';

const BacktestingTab: React.FC = () => {
  const state = useBacktestingState();

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: FONT_FAMILY,
    }}>
      {/* Top Navigation */}
      <TopNavBar state={state} />

      {/* Three-Panel Layout: LEFT 280px | CENTER flex:1 | RIGHT 300px */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        <CommandPanel state={state} />
        <ConfigPanel state={state} />
        <ResultsPanel state={state} />
      </div>

      {/* Status Bar */}
      <StatusBar state={state} />
    </div>
  );
};

export default BacktestingTab;
