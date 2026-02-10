/**
 * ConfigPanel - Center panel shell that routes to command-specific configs
 * Design system: flat HEADER_BG, 2px radius, ORANGE primary button
 */

import React from 'react';
import { Play, Loader } from 'lucide-react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import type { BacktestingState } from '../types';
import {
  BacktestConfig, OptimizeConfig, WalkForwardConfig, IndicatorConfig,
  IndicatorSignalsConfig, AdvancedConfig, LabelsConfig, SplittersConfig,
  ReturnsConfig, SignalsConfig,
} from '../configs';

interface ConfigPanelProps {
  state: BacktestingState;
}

export const ConfigPanel: React.FC<ConfigPanelProps> = ({ state }) => {
  const { activeCommand, isRunning, handleRunCommand, showAdvanced, providerColor } = state;

  return (
    <div style={{
      flex: 1,
      backgroundColor: FINCEPT.DARK_BG,
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
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY,
          }}>
            {activeCommand.toUpperCase().replace('_', ' ')}
          </span>
          <span style={{
            fontSize: '9px',
            color: FINCEPT.MUTED,
            fontFamily: FONT_FAMILY,
          }}>
            CONFIGURATION
          </span>
        </div>

        {/* Run Button */}
        <button
          onClick={handleRunCommand}
          disabled={isRunning}
          style={{
            padding: '8px 16px',
            backgroundColor: isRunning ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: isRunning ? 'wait' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontFamily: FONT_FAMILY,
            letterSpacing: '0.5px',
          }}
        >
          {isRunning ? (
            <>
              <Loader size={10} className="animate-spin" />
              EXECUTING...
            </>
          ) : (
            <>
              <Play size={10} />
              RUN {activeCommand.toUpperCase().replace('_', ' ')}
            </>
          )}
        </button>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {activeCommand === 'backtest' && (
          <>
            <BacktestConfig state={state} />
            {showAdvanced && <div style={{ marginTop: '12px' }}><AdvancedConfig state={state} /></div>}
          </>
        )}
        {activeCommand === 'optimize' && <OptimizeConfig state={state} />}
        {activeCommand === 'walk_forward' && <WalkForwardConfig state={state} />}
        {activeCommand === 'indicator' && <IndicatorConfig state={state} />}
        {activeCommand === 'indicator_signals' && <IndicatorSignalsConfig state={state} />}
        {activeCommand === 'labels' && <LabelsConfig state={state} />}
        {activeCommand === 'splits' && <SplittersConfig state={state} />}
        {activeCommand === 'returns' && <ReturnsConfig state={state} />}
        {activeCommand === 'signals' && <SignalsConfig state={state} />}
      </div>
    </div>
  );
};
