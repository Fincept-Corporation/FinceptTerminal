/**
 * CommandPanel - Left panel with command list and strategy categories
 * Design system: watchlist-style items with orange left-border, flat panels
 */

import React from 'react';
import { ChevronRight } from 'lucide-react';
import { FINCEPT, FONT_FAMILY, PROVIDER_COLORS } from '../constants';
import type { BacktestingState } from '../types';

interface CommandPanelProps {
  state: BacktestingState;
}

export const CommandPanel: React.FC<CommandPanelProps> = ({ state }) => {
  const {
    selectedProvider, activeCommand, setActiveCommand,
    providerCommands, providerCategoryInfo, providerStrategies,
    selectedCategory, setSelectedCategory,
  } = state;
  const providerColor = PROVIDER_COLORS[selectedProvider];

  return (
    <div style={{
      width: '280px',
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Commands Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <span style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GRAY,
          letterSpacing: '0.5px',
          fontFamily: FONT_FAMILY,
        }}>
          COMMANDS
        </span>
        <span style={{
          fontSize: '9px',
          fontWeight: 700,
          color: providerColor,
        }}>
          {providerCommands.length}
        </span>
      </div>

      {/* Command List - Watchlist style */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {providerCommands.map(cmd => {
          const Icon = cmd.icon;
          const isActive = activeCommand === cmd.id;

          return (
            <div
              key={cmd.id}
              onClick={() => setActiveCommand(cmd.id)}
              style={{
                padding: '10px 12px',
                backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}
              onMouseEnter={e => {
                if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              }}
              onMouseLeave={e => {
                if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
              }}
            >
              <Icon size={12} color={isActive ? FINCEPT.ORANGE : FINCEPT.MUTED} />
              <div style={{ flex: 1 }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                  fontFamily: FONT_FAMILY,
                }}>
                  {cmd.label}
                </div>
                <div style={{
                  fontSize: '8px',
                  color: FINCEPT.MUTED,
                  marginTop: '2px',
                }}>
                  {cmd.description}
                </div>
              </div>
              <ChevronRight size={10} color={isActive ? FINCEPT.ORANGE : FINCEPT.MUTED} />
            </div>
          );
        })}
      </div>

      {/* Strategy Categories */}
      {['backtest', 'optimize', 'walk_forward'].includes(activeCommand) && (
        <>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY,
            }}>
              STRATEGIES
            </span>
          </div>
          <div style={{ overflowY: 'auto', maxHeight: '280px' }}>
            {Object.entries(providerCategoryInfo).map(([key, info]: [string, any]) => {
              const CatIcon = info.icon;
              const isActive = selectedCategory === key;
              const count = providerStrategies[key]?.length || 0;

              return (
                <div
                  key={key}
                  onClick={() => setSelectedCategory(key)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: isActive ? `${info.color}15` : 'transparent',
                    borderLeft: isActive ? `2px solid ${info.color}` : '2px solid transparent',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                  }}
                  onMouseEnter={e => {
                    if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={e => {
                    if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                  }}
                >
                  <CatIcon size={11} color={isActive ? info.color : FINCEPT.MUTED} />
                  <span style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                    fontFamily: FONT_FAMILY,
                    flex: 1,
                    letterSpacing: '0.3px',
                  }}>
                    {info.label.toUpperCase()}
                  </span>
                  <span style={{
                    fontSize: '8px',
                    color: isActive ? info.color : FINCEPT.MUTED,
                  }}>
                    {count}
                  </span>
                </div>
              );
            })}
          </div>
        </>
      )}
    </div>
  );
};
