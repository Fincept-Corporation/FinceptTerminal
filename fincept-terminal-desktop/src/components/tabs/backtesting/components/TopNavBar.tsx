/**
 * TopNavBar - Provider selector, advanced toggle, status indicator
 * Design system: flat HEADER_BG, 2px orange bottom border, tab-style buttons
 */

import React from 'react';
import { Activity, Settings, Loader } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, PROVIDER_COLORS } from '../constants';
import { PROVIDER_OPTIONS } from '../backtestingProviderConfigs';
import type { BacktestingState } from '../types';

interface TopNavBarProps {
  state: BacktestingState;
}

export const TopNavBar: React.FC<TopNavBarProps> = ({ state }) => {
  const { selectedProvider, setSelectedProvider, showAdvanced, setShowAdvanced, isRunning } = state;
  const providerColor = PROVIDER_COLORS[selectedProvider];

  return (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `2px solid ${FINCEPT.ORANGE}`,
      padding: '8px 16px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
    }}>
      {/* Left: Title + Provider selector */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity size={14} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            fontFamily: FONT_FAMILY,
            letterSpacing: '1px',
          }}>
            BACKTESTING
          </span>
        </div>

        {/* Provider tabs */}
        <div style={{ display: 'flex', gap: '2px' }}>
          {PROVIDER_OPTIONS.map(p => (
            <button
              key={p.slug}
              onClick={() => setSelectedProvider(p.slug)}
              style={{
                padding: '6px 12px',
                backgroundColor: selectedProvider === p.slug ? PROVIDER_COLORS[p.slug] : 'transparent',
                color: selectedProvider === p.slug ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={e => {
                if (selectedProvider !== p.slug) {
                  e.currentTarget.style.color = FINCEPT.WHITE;
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={e => {
                if (selectedProvider !== p.slug) {
                  e.currentTarget.style.color = FINCEPT.GRAY;
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              {p.displayName}
            </button>
          ))}
        </div>
      </div>

      {/* Right: Advanced toggle + Status */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <button
          onClick={() => setShowAdvanced(!showAdvanced)}
          style={{
            padding: '6px 10px',
            backgroundColor: showAdvanced ? `${FINCEPT.ORANGE}15` : 'transparent',
            border: `1px solid ${showAdvanced ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
            color: showAdvanced ? FINCEPT.ORANGE : FINCEPT.GRAY,
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            fontFamily: FONT_FAMILY,
            transition: 'all 0.2s',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
          }}
        >
          <Settings size={10} />
          ADVANCED
        </button>

        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
          padding: '4px 8px',
          backgroundColor: isRunning ? `${FINCEPT.ORANGE}15` : `${FINCEPT.GREEN}15`,
          borderRadius: '2px',
          border: `1px solid ${isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN}30`,
        }}>
          {isRunning ? (
            <Loader size={10} color={FINCEPT.ORANGE} className="animate-spin" />
          ) : (
            <div style={{
              width: '6px',
              height: '6px',
              borderRadius: '2px',
              backgroundColor: FINCEPT.GREEN,
            }} />
          )}
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: isRunning ? FINCEPT.ORANGE : FINCEPT.GREEN,
            letterSpacing: '0.5px',
          }}>
            {isRunning ? 'RUNNING' : 'READY'}
          </span>
        </div>
      </div>
    </div>
  );
};
