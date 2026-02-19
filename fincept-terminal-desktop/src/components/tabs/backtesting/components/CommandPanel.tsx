/**
 * CommandPanel - Left sidebar: NodePalette-identical structure.
 * Commands as expandable category + Strategy categories as items with dot+name.
 */

import React, { useState } from 'react';
import { Layers, Search, ChevronDown } from 'lucide-react';
import { FINCEPT, EFFECTS } from '../../portfolio-tab/finceptStyles';
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
  const totalCommands = providerCommands.length;

  const [searchQuery, setSearchQuery] = useState('');
  const [cmdExpanded, setCmdExpanded] = useState(true);
  const [stratExpanded, setStratExpanded] = useState(true);

  const filteredCommands = providerCommands.filter(cmd =>
    cmd.label.toLowerCase().includes(searchQuery.toLowerCase()) ||
    (cmd.description || '').toLowerCase().includes(searchQuery.toLowerCase())
  );

  const showStrategies = ['backtest', 'optimize', 'walk_forward'].includes(activeCommand);

  return (
    <div style={{
      width: '240px',
      flexShrink: 0,
      borderRight: '1px solid #2a2a2a',
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: '#000000',
      fontFamily: '"IBM Plex Mono", Consolas, monospace',
    }}>

      {/* ── Header ── */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: '#0f0f0f',
        borderBottom: '1px solid #2a2a2a',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
          <Layers size={13} style={{ color: '#FF8800' }} />
          <span style={{ color: '#FF8800', fontSize: '10px', fontWeight: 700, letterSpacing: '0.8px' }}>
            COMMANDS
          </span>
        </div>
        <span style={{
          fontSize: '8px', fontWeight: 700,
          color: providerColor,
          padding: '1px 5px',
          backgroundColor: `${providerColor}20`,
          border: `1px solid ${providerColor}40`,
          borderRadius: '2px',
          letterSpacing: '0.3px',
        }}>
          {selectedProvider.toUpperCase()}
        </span>
      </div>

      {/* ── Search ── */}
      <div style={{
        padding: '8px 10px',
        borderBottom: '1px solid #2a2a2a',
        backgroundColor: '#000000',
        flexShrink: 0,
      }}>
        <div style={{ position: 'relative' }}>
          <Search size={11} style={{
            position: 'absolute', left: '8px', top: '50%',
            transform: 'translateY(-50%)', color: '#787878', pointerEvents: 'none',
          }} />
          <input
            type="text"
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
            placeholder="Search commands..."
            style={{
              width: '100%', backgroundColor: '#0f0f0f',
              border: '1px solid #2a2a2a', borderRadius: '2px',
              padding: '6px 8px 6px 26px', color: '#ffffff',
              fontSize: '10px', fontFamily: '"IBM Plex Mono", Consolas, monospace',
              outline: 'none', boxSizing: 'border-box' as const,
              transition: 'border-color 0.15s',
            }}
            onFocus={e => (e.currentTarget.style.borderColor = '#FF8800')}
            onBlur={e => (e.currentTarget.style.borderColor = '#2a2a2a')}
          />
        </div>
        <div style={{ color: '#787878', fontSize: '9px', marginTop: '5px', letterSpacing: '0.3px' }}>
          {totalCommands} COMMANDS AVAILABLE
        </div>
      </div>

      {/* ── Scrollable body ── */}
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>

        {/* Commands category */}
        <div>
          <button
            onClick={() => setCmdExpanded(v => !v)}
            style={{
              width: '100%', padding: '7px 10px', backgroundColor: '#0f0f0f',
              border: 'none', borderBottom: '1px solid #2a2a2a',
              display: 'flex', alignItems: 'center', justifyContent: 'space-between',
              cursor: 'pointer', transition: 'background-color 0.15s',
            }}
            onMouseEnter={e => (e.currentTarget.style.backgroundColor = '#1a1a1a')}
            onMouseLeave={e => (e.currentTarget.style.backgroundColor = '#0f0f0f')}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
              <span style={{ fontSize: '9px', fontWeight: 700, color: '#d4d4d4', letterSpacing: '0.5px' }}>
                OPERATIONS
              </span>
              <span style={{
                fontSize: '8px', color: '#787878', backgroundColor: '#1a1a1a',
                border: '1px solid #2a2a2a', borderRadius: '2px', padding: '1px 4px',
              }}>
                {filteredCommands.length}
              </span>
            </div>
            <ChevronDown size={11} style={{
              color: '#787878',
              transform: cmdExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
              transition: 'transform 0.15s',
            }} />
          </button>

          {cmdExpanded && (
            <div style={{ padding: '4px 6px 6px', backgroundColor: '#000000', borderBottom: '1px solid #1a1a1a' }}>
              {filteredCommands.map(cmd => {
                const Icon = cmd.icon;
                const isActive = activeCommand === cmd.id;
                const cmdColor = (cmd as any).color || '#FF8800';
                return (
                  <div
                    key={cmd.id}
                    onClick={() => setActiveCommand(cmd.id)}
                    style={{
                      backgroundColor: isActive ? `${cmdColor}15` : '#0f0f0f',
                      border: `1px solid ${isActive ? cmdColor + '50' : '#2a2a2a'}`,
                      borderLeft: `2px solid ${isActive ? cmdColor : '#2a2a2a'}`,
                      borderRadius: '2px', padding: '6px 8px', marginBottom: '3px',
                      cursor: 'pointer', transition: 'all 0.15s',
                      display: 'flex', alignItems: 'center', gap: '8px',
                    }}
                    onMouseEnter={e => {
                      if (!isActive) {
                        e.currentTarget.style.backgroundColor = `${cmdColor}10`;
                        e.currentTarget.style.borderColor = `${cmdColor}50`;
                        e.currentTarget.style.borderLeftColor = cmdColor;
                        e.currentTarget.style.borderLeftWidth = '2px';
                      }
                    }}
                    onMouseLeave={e => {
                      if (!isActive) {
                        e.currentTarget.style.backgroundColor = '#0f0f0f';
                        e.currentTarget.style.borderColor = '#2a2a2a';
                        e.currentTarget.style.borderLeftWidth = '1px';
                      }
                    }}
                  >
                    <div style={{
                      width: '6px', height: '6px', borderRadius: '50%',
                      backgroundColor: isActive ? cmdColor : '#787878', flexShrink: 0,
                      boxShadow: isActive ? `0 0 4px ${cmdColor}60` : 'none',
                    }} />
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{
                        color: isActive ? '#ffffff' : '#d4d4d4', fontSize: '10px',
                        fontWeight: 700, letterSpacing: '0.3px',
                        whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
                      }}>
                        {cmd.label}
                      </div>
                      {cmd.description && (
                        <div style={{
                          color: '#787878', fontSize: '8px', marginTop: '2px',
                          whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
                        }}>
                          {cmd.description}
                        </div>
                      )}
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Strategy categories (shown only for backtest/optimize/walk_forward) */}
        {showStrategies && (
          <div>
            <button
              onClick={() => setStratExpanded(v => !v)}
              style={{
                width: '100%', padding: '7px 10px', backgroundColor: '#0f0f0f',
                border: 'none', borderBottom: '1px solid #2a2a2a',
                display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                cursor: 'pointer', transition: 'background-color 0.15s',
              }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = '#1a1a1a')}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = '#0f0f0f')}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
                <span style={{ fontSize: '9px', fontWeight: 700, color: '#d4d4d4', letterSpacing: '0.5px' }}>
                  STRATEGIES
                </span>
                <span style={{
                  fontSize: '8px', color: '#787878', backgroundColor: '#1a1a1a',
                  border: '1px solid #2a2a2a', borderRadius: '2px', padding: '1px 4px',
                }}>
                  {Object.keys(providerCategoryInfo).length}
                </span>
              </div>
              <ChevronDown size={11} style={{
                color: '#787878',
                transform: stratExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
                transition: 'transform 0.15s',
              }} />
            </button>

            {stratExpanded && (
              <div style={{ padding: '4px 6px 6px', backgroundColor: '#000000', borderBottom: '1px solid #1a1a1a' }}>
                {Object.entries(providerCategoryInfo).map(([key, info]: [string, any]) => {
                  const CatIcon = info.icon;
                  const isActive = selectedCategory === key;
                  const count = providerStrategies[key]?.length || 0;
                  return (
                    <div
                      key={key}
                      onClick={() => setSelectedCategory(key)}
                      style={{
                        backgroundColor: isActive ? `${info.color}15` : '#0f0f0f',
                        border: `1px solid ${isActive ? info.color + '50' : '#2a2a2a'}`,
                        borderLeft: `2px solid ${isActive ? info.color : '#2a2a2a'}`,
                        borderRadius: '2px', padding: '6px 8px', marginBottom: '3px',
                        cursor: 'pointer', transition: 'all 0.15s',
                        display: 'flex', alignItems: 'center', gap: '8px',
                      }}
                      onMouseEnter={e => {
                        if (!isActive) {
                          e.currentTarget.style.backgroundColor = `${info.color}10`;
                          e.currentTarget.style.borderColor = `${info.color}50`;
                          e.currentTarget.style.borderLeftColor = info.color;
                          e.currentTarget.style.borderLeftWidth = '2px';
                        }
                      }}
                      onMouseLeave={e => {
                        if (!isActive) {
                          e.currentTarget.style.backgroundColor = '#0f0f0f';
                          e.currentTarget.style.borderColor = '#2a2a2a';
                          e.currentTarget.style.borderLeftWidth = '1px';
                        }
                      }}
                    >
                      <div style={{
                        width: '6px', height: '6px', borderRadius: '50%',
                        backgroundColor: isActive ? info.color : '#787878', flexShrink: 0,
                        boxShadow: isActive ? `0 0 4px ${info.color}60` : 'none',
                      }} />
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{
                          color: isActive ? '#ffffff' : '#d4d4d4', fontSize: '10px',
                          fontWeight: 700, letterSpacing: '0.3px',
                          whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis',
                        }}>
                          {info.label}
                        </div>
                      </div>
                      <span style={{
                        fontSize: '8px', color: isActive ? info.color : '#787878',
                        backgroundColor: '#1a1a1a', border: '1px solid #2a2a2a',
                        borderRadius: '2px', padding: '1px 4px', flexShrink: 0,
                      }}>
                        {count}
                      </span>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
        )}
      </div>

      {/* ── Footer ── */}
      <div style={{
        padding: '6px 10px', borderTop: '1px solid #2a2a2a', backgroundColor: '#0f0f0f',
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        gap: '6px', flexShrink: 0,
      }}>
        <div style={{ width: '4px', height: '4px', borderRadius: '50%', backgroundColor: '#FF8800' }} />
        <span style={{ color: '#4a4a4a', fontSize: '8px', letterSpacing: '0.5px', fontWeight: 700 }}>
          CLICK TO SELECT COMMAND
        </span>
      </div>
    </div>
  );
};
