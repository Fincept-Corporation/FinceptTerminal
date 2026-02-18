/**
 * StatusBar - 24px footer matching portfolio StatusBar pattern
 * HEADER_BG, border-top, pipe-separated status items, live indicator
 */

import React, { useMemo } from 'react';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';
import { PROVIDER_COLORS } from '../constants';
import { PROVIDER_CONFIGS } from '../backtestingProviderConfigs';
import { useTimezone } from '@/contexts/TimezoneContext';
import type { BacktestingState } from '../types';

interface StatusBarProps {
  state: BacktestingState;
}

export const StatusBar: React.FC<StatusBarProps> = ({ state }) => {
  const { selectedProvider, activeCommand, currentStrategy, isRunning, symbols } = state;
  const providerColor = PROVIDER_COLORS[selectedProvider];
  const providerConfig = PROVIDER_CONFIGS[selectedProvider];

  const { timezone, formatTimeShort } = useTimezone();
  const shortLabel = timezone.shortLabel;

  const formattedTime = useMemo(() => {
    return formatTimeShort(new Date());
  }, [isRunning, formatTimeShort]); // updates when running state changes

  return (
    <div style={{
      height: '24px',
      flexShrink: 0,
      backgroundColor: FINCEPT.HEADER_BG,
      borderTop: `1px solid ${FINCEPT.BORDER}`,
      padding: '0 12px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      fontSize: '9px',
    }}>
      {/* Left */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 800 }}>FINCEPT</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span style={{ color: FINCEPT.GRAY }}>BACKTESTING TERMINAL</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>

        <span style={{ color: providerColor, fontWeight: 700 }}>
          {providerConfig.displayName}
        </span>

        <span style={{ color: FINCEPT.BORDER }}>|</span>

        <span style={{ color: FINCEPT.GRAY }}>
          CMD: <span style={{ color: FINCEPT.WHITE }}>{activeCommand.toUpperCase().replace('_', ' ')}</span>
        </span>

        {currentStrategy && (
          <>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.GRAY }}>
              STRAT: <span style={{ color: FINCEPT.CYAN }}>{currentStrategy.name.toUpperCase()}</span>
            </span>
          </>
        )}

        {symbols && (
          <>
            <span style={{ color: FINCEPT.BORDER }}>|</span>
            <span style={{ color: FINCEPT.GRAY }}>
              SYM: <span style={{ color: FINCEPT.YELLOW }}>{symbols.toUpperCase()}</span>
            </span>
          </>
        )}
      </div>

      {/* Right */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '3px' }}>
          {isRunning ? (
            <div style={{
              width: '5px', height: '5px', borderRadius: '50%',
              backgroundColor: FINCEPT.ORANGE,
              boxShadow: `0 0 4px ${FINCEPT.ORANGE}`,
              animation: 'pulse 1s infinite',
            }} />
          ) : (
            <div style={{
              width: '5px', height: '5px', borderRadius: '50%',
              backgroundColor: FINCEPT.GREEN,
              boxShadow: `0 0 4px ${FINCEPT.GREEN}`,
              animation: 'pulse 2s infinite',
            }} />
          )}
          <span style={{
            color: isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN,
            fontWeight: 700,
            letterSpacing: '0.5px',
          }}>
            {isRunning ? 'RUNNING' : 'READY'}
          </span>
        </div>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>{formattedTime}</span>
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>{shortLabel}</span>
      </div>
    </div>
  );
};
