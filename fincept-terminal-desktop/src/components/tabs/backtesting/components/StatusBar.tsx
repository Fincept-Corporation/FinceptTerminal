/**
 * StatusBar - Bottom status bar
 * Design system: HEADER_BG background, border-top, 9px gray text
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY, PROVIDER_COLORS } from '../constants';
import { PROVIDER_CONFIGS } from '../backtestingProviderConfigs';
import type { BacktestingState } from '../types';

interface StatusBarProps {
  state: BacktestingState;
}

export const StatusBar: React.FC<StatusBarProps> = ({ state }) => {
  const { selectedProvider, activeCommand, currentStrategy, isRunning } = state;
  const providerColor = PROVIDER_COLORS[selectedProvider];
  const providerConfig = PROVIDER_CONFIGS[selectedProvider];

  return (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderTop: `1px solid ${FINCEPT.BORDER}`,
      padding: '4px 16px',
      fontSize: '9px',
      color: FINCEPT.GRAY,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      fontFamily: FONT_FAMILY,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <span style={{ color: providerColor, fontWeight: 600 }}>
          {providerConfig.displayName}
        </span>
        <span style={{ color: FINCEPT.MUTED }}>|</span>
        <span>
          <span style={{ color: FINCEPT.MUTED }}>CMD: </span>
          <span style={{ color: FINCEPT.WHITE }}>
            {activeCommand.toUpperCase().replace('_', ' ')}
          </span>
        </span>
        {currentStrategy && (
          <>
            <span style={{ color: FINCEPT.MUTED }}>|</span>
            <span>
              <span style={{ color: FINCEPT.MUTED }}>STRATEGY: </span>
              <span style={{ color: FINCEPT.CYAN }}>
                {currentStrategy.name.toUpperCase()}
              </span>
            </span>
          </>
        )}
      </div>

      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
      }}>
        <div style={{
          width: '6px',
          height: '6px',
          borderRadius: '2px',
          backgroundColor: isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN,
        }} />
        <span style={{
          color: isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN,
          fontWeight: 700,
          letterSpacing: '0.5px',
        }}>
          {isRunning ? 'RUNNING' : 'READY'}
        </span>
      </div>
    </div>
  );
};
