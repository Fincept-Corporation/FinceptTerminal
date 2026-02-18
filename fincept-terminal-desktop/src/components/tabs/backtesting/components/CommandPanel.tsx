/**
 * CommandPanel - Left sidebar (220px) with command list + strategy categories
 * Matches portfolio HoldingsHeatmap sidebar pattern: dense, watchlist-style items
 */

import React from 'react';
import { ChevronRight } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, EFFECTS } from '../../portfolio-tab/finceptStyles';
import { PROVIDER_COLORS } from '../constants';
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
      width: '220px',
      flexShrink: 0,
      backgroundColor: FINCEPT.PANEL_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Commands Header */}
      <div style={{
        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <span style={{
          fontSize: TYPOGRAPHY.TINY,
          fontWeight: TYPOGRAPHY.BOLD,
          color: FINCEPT.ORANGE,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
        }}>
          COMMANDS
        </span>
        <span style={{
          fontSize: '8px',
          fontWeight: TYPOGRAPHY.BOLD,
          color: providerColor,
          padding: '1px 5px',
          backgroundColor: `${providerColor}15`,
          borderRadius: '2px',
        }}>
          {providerCommands.length}
        </span>
      </div>

      {/* Command List */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {providerCommands.map(cmd => {
          const Icon = cmd.icon;
          const isActive = activeCommand === cmd.id;

          return (
            <div
              key={cmd.id}
              onClick={() => setActiveCommand(cmd.id)}
              className="bt-row"
              style={{
                padding: '8px 10px',
                backgroundColor: isActive ? `${FINCEPT.ORANGE}08` : 'transparent',
                borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                transition: EFFECTS.TRANSITION_FAST,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              <Icon size={10} color={isActive ? FINCEPT.ORANGE : FINCEPT.MUTED} style={isActive ? { filter: EFFECTS.ICON_GLOW_ORANGE } : undefined} />
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: TYPOGRAPHY.BOLD,
                  color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                  letterSpacing: '0.3px',
                }}>
                  {cmd.label}
                </div>
                <div style={{
                  fontSize: '8px',
                  color: FINCEPT.MUTED,
                  marginTop: '1px',
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}>
                  {cmd.description}
                </div>
              </div>
              <ChevronRight size={9} color={isActive ? FINCEPT.ORANGE : FINCEPT.MUTED} style={{ flexShrink: 0 }} />
            </div>
          );
        })}
      </div>

      {/* Strategy Categories */}
      {['backtest', 'optimize', 'walk_forward'].includes(activeCommand) && (
        <>
          <div style={{
            padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
            backgroundColor: FINCEPT.HEADER_BG,
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}>
            <span style={{
              fontSize: TYPOGRAPHY.TINY,
              fontWeight: TYPOGRAPHY.BOLD,
              color: FINCEPT.ORANGE,
              letterSpacing: '0.5px',
            }}>
              STRATEGIES
            </span>
          </div>
          <div style={{ overflowY: 'auto', maxHeight: '260px' }}>
            {Object.entries(providerCategoryInfo).map(([key, info]: [string, any]) => {
              const CatIcon = info.icon;
              const isActive = selectedCategory === key;
              const count = providerStrategies[key]?.length || 0;

              return (
                <div
                  key={key}
                  onClick={() => setSelectedCategory(key)}
                  className="bt-row"
                  style={{
                    padding: '6px 10px',
                    backgroundColor: isActive ? `${info.color}08` : 'transparent',
                    borderLeft: isActive ? `2px solid ${info.color}` : '2px solid transparent',
                    cursor: 'pointer',
                    transition: EFFECTS.TRANSITION_FAST,
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                  }}
                >
                  <CatIcon size={9} color={isActive ? info.color : FINCEPT.MUTED} />
                  <span style={{
                    fontSize: TYPOGRAPHY.TINY,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                    flex: 1,
                    letterSpacing: '0.3px',
                    textTransform: 'uppercase',
                  }}>
                    {info.label}
                  </span>
                  <span style={{
                    fontSize: '8px',
                    fontWeight: TYPOGRAPHY.BOLD,
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
