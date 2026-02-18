/**
 * TopNavBar - 40px command bar matching portfolio CommandBar pattern
 * Gradient bg, 2px orange bottom border, provider tab-style buttons, status indicator
 */

import React from 'react';
import { Activity, Settings, Loader, Play } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, EFFECTS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { PROVIDER_COLORS } from '../constants';
import { PROVIDER_OPTIONS } from '../backtestingProviderConfigs';
import type { BacktestingState } from '../types';

interface TopNavBarProps {
  state: BacktestingState;
}

export const TopNavBar: React.FC<TopNavBarProps> = ({ state }) => {
  const { selectedProvider, setSelectedProvider, showAdvanced, setShowAdvanced, isRunning, handleRunCommand } = state;
  const providerColor = PROVIDER_COLORS[selectedProvider];

  return (
    <div style={{
      height: '40px',
      flexShrink: 0,
      background: 'linear-gradient(180deg, #141414 0%, #0A0A0A 100%)',
      borderBottom: `2px solid ${FINCEPT.ORANGE}`,
      display: 'flex',
      alignItems: 'center',
      padding: '0 10px',
      gap: '8px',
    }}>
      {/* Title */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
        <Activity size={11} color={FINCEPT.ORANGE} style={{ filter: EFFECTS.ICON_GLOW_ORANGE }} />
        <span style={{
          fontSize: '12px',
          fontWeight: 800,
          color: FINCEPT.ORANGE,
          letterSpacing: '1px',
        }}>
          BACKTESTING
        </span>
      </div>

      <span style={{ color: FINCEPT.BORDER }}>|</span>

      {/* Provider tabs */}
      <div style={{ display: 'flex', gap: '2px', flex: 1 }}>
        {PROVIDER_OPTIONS.map(p => {
          const isActive = selectedProvider === p.slug;
          return (
            <button
              key={p.slug}
              onClick={() => setSelectedProvider(p.slug)}
              style={{
                padding: '6px 12px',
                backgroundColor: isActive ? PROVIDER_COLORS[p.slug] : 'transparent',
                color: isActive ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                borderRadius: '2px',
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                cursor: 'pointer',
                letterSpacing: '0.5px',
                transition: EFFECTS.TRANSITION_FAST,
                textTransform: 'uppercase',
              }}
              onMouseEnter={e => {
                if (!isActive) {
                  e.currentTarget.style.color = FINCEPT.WHITE;
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={e => {
                if (!isActive) {
                  e.currentTarget.style.color = FINCEPT.GRAY;
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              {p.displayName}
            </button>
          );
        })}
      </div>

      {/* Right: Run + Advanced + Status */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
        {/* Run Button */}
        <button
          onClick={handleRunCommand}
          disabled={isRunning}
          style={{
            ...COMMON_STYLES.buttonPrimary,
            padding: '5px 14px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            opacity: isRunning ? 0.6 : 1,
            cursor: isRunning ? 'wait' : 'pointer',
          }}
        >
          {isRunning ? <Loader size={9} className="animate-spin" /> : <Play size={9} />}
          {isRunning ? 'RUNNING' : 'RUN'}
        </button>

        {/* Advanced toggle */}
        <button
          onClick={() => setShowAdvanced(!showAdvanced)}
          style={{
            ...COMMON_STYLES.buttonSecondary,
            padding: '5px 8px',
            backgroundColor: showAdvanced ? `${FINCEPT.ORANGE}15` : 'transparent',
            borderColor: showAdvanced ? FINCEPT.ORANGE : FINCEPT.BORDER,
            color: showAdvanced ? FINCEPT.ORANGE : FINCEPT.GRAY,
            display: 'flex',
            alignItems: 'center',
            gap: '3px',
          }}
        >
          <Settings size={9} />
          ADV
        </button>

        {/* Status indicator */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          padding: '4px 8px',
          backgroundColor: isRunning ? `${FINCEPT.ORANGE}15` : `${FINCEPT.GREEN}15`,
          borderRadius: '2px',
          border: `1px solid ${isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN}30`,
        }}>
          {isRunning ? (
            <Loader size={8} color={FINCEPT.ORANGE} className="animate-spin" />
          ) : (
            <div style={{
              width: '5px', height: '5px', borderRadius: '50%',
              backgroundColor: FINCEPT.GREEN,
              boxShadow: `0 0 4px ${FINCEPT.GREEN}`,
              animation: 'pulse 2s infinite',
            }} />
          )}
          <span style={{
            fontSize: '8px', fontWeight: 700,
            color: isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN,
            letterSpacing: '0.5px',
          }}>
            {isRunning ? 'EXEC' : 'READY'}
          </span>
        </div>
      </div>
    </div>
  );
};
