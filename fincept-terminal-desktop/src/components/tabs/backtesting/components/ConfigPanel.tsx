/**
 * ConfigPanel - Center panel: command-specific configuration forms
 * Dense terminal layout with HEADER_BG section headers, 2px radius inputs
 */

import React from 'react';
import { FINCEPT, TYPOGRAPHY, SPACING, EFFECTS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
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
  const { activeCommand, showAdvanced, providerColor } = state;

  return (
    <div style={{
      flex: 1,
      backgroundColor: FINCEPT.DARK_BG,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      minWidth: 0,
    }}>
      {/* Header */}
      <div style={{
        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{
            fontSize: '11px',
            fontWeight: TYPOGRAPHY.BOLD,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
          }}>
            {activeCommand.toUpperCase().replace('_', ' ')}
          </span>
          <span style={{
            fontSize: TYPOGRAPHY.TINY,
            color: FINCEPT.MUTED,
            letterSpacing: '0.3px',
          }}>
            CONFIGURATION
          </span>
        </div>
        <span style={{
          fontSize: '8px',
          fontWeight: TYPOGRAPHY.BOLD,
          color: providerColor,
          padding: '2px 6px',
          backgroundColor: `${providerColor}15`,
          borderRadius: '2px',
          letterSpacing: '0.5px',
        }}>
          {state.selectedProvider.toUpperCase()}
        </span>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto', padding: SPACING.DEFAULT }}>
        {activeCommand === 'backtest' && (
          <>
            <BacktestConfig state={state} />
            {showAdvanced && <div style={{ marginTop: SPACING.DEFAULT }}><AdvancedConfig state={state} /></div>}
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
