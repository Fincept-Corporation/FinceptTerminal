/**
 * VBCommandBar - 52px top bar with large command pills, inline config, RUN button.
 * Redesign: 10px+ fonts, wider inputs, bigger pills with icons.
 */

import React from 'react';
import { Play, Loader, Settings } from 'lucide-react';
import {
  FINCEPT, TYPOGRAPHY, EFFECTS, COMMON_STYLES, BORDERS, SPACING,
  createFocusHandlers,
} from '../../../portfolio-tab/finceptStyles';
import { ALL_COMMANDS } from '../../backtestingProviderConfigs';
import type { BacktestingState } from '../../types';

interface VBCommandBarProps {
  state: BacktestingState;
}

export const VBCommandBar: React.FC<VBCommandBarProps> = ({ state }) => {
  const {
    activeCommand, setActiveCommand,
    providerCommands,
    symbols, setSymbols,
    startDate, setStartDate,
    endDate, setEndDate,
    initialCapital, setInitialCapital,
    isRunning, handleRunCommand,
    showAdvanced, setShowAdvanced,
  } = state;

  const focus = createFocusHandlers();

  return (
    <div style={{
      height: '52px',
      flexShrink: 0,
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `2px solid ${FINCEPT.CYAN}`,
      display: 'flex',
      alignItems: 'center',
      padding: '0 12px',
      gap: '8px',
    }}>
      {/* VectorBT logo */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        flexShrink: 0,
      }}>
        <div style={{
          width: '6px',
          height: '22px',
          backgroundColor: FINCEPT.CYAN,
          borderRadius: '1px',
        }} />
        <span style={{
          fontSize: '12px',
          fontWeight: 800,
          color: FINCEPT.CYAN,
          letterSpacing: '1px',
        }}>
          VBT
        </span>
      </div>

      <div style={{
        width: '1px',
        height: '24px',
        backgroundColor: FINCEPT.BORDER,
        flexShrink: 0,
      }} />

      {/* Command squares */}
      <div style={{
        display: 'flex',
        gap: '3px',
        flexShrink: 0,
      }}>
        {providerCommands.map(cmd => {
          const isActive = activeCommand === cmd.id;
          const Icon = cmd.icon;
          const cmdColor = (cmd as any).color || FINCEPT.ORANGE;
          return (
            <button
              key={cmd.id}
              onClick={() => setActiveCommand(cmd.id)}
              title={cmd.label}
              style={{
                width: '60px',
                height: '40px',
                backgroundColor: isActive ? `${cmdColor}20` : FINCEPT.DARK_BG,
                color: isActive ? cmdColor : FINCEPT.MUTED,
                border: `1px solid ${isActive ? cmdColor : FINCEPT.BORDER}`,
                borderRadius: '3px',
                fontSize: '8px',
                fontWeight: 700,
                cursor: 'pointer',
                letterSpacing: '0.3px',
                textTransform: 'uppercase' as const,
                display: 'flex',
                flexDirection: 'column' as const,
                alignItems: 'center',
                justifyContent: 'center',
                gap: '3px',
                transition: EFFECTS.TRANSITION_FAST,
                fontFamily: TYPOGRAPHY.MONO,
              }}
              onMouseEnter={e => { if (!isActive) { e.currentTarget.style.borderColor = cmdColor; e.currentTarget.style.color = cmdColor; } }}
              onMouseLeave={e => { if (!isActive) { e.currentTarget.style.borderColor = FINCEPT.BORDER; e.currentTarget.style.color = FINCEPT.MUTED; } }}
            >
              <Icon size={13} />
              <span style={{ lineHeight: 1.2, fontSize: '7px', textAlign: 'center' as const }}>{cmd.label}</span>
            </button>
          );
        })}
      </div>

      {/* Spacer */}
      <div style={{ flex: 1 }} />

      {/* Inline config inputs */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        flexShrink: 0,
      }}>
        {/* Symbol */}
        <input
          value={symbols}
          onChange={e => setSymbols(e.target.value)}
          placeholder="SPY"
          style={{
            width: '90px',
            padding: '6px 8px',
            backgroundColor: FINCEPT.DARK_BG,
            color: FINCEPT.YELLOW,
            border: BORDERS.STANDARD,
            borderRadius: '2px',
            fontSize: '10px',
            fontFamily: TYPOGRAPHY.MONO,
            outline: 'none',
            fontWeight: 700,
          }}
          {...focus}
        />

        {/* Date range */}
        <input
          type="date"
          value={startDate}
          onChange={e => setStartDate(e.target.value)}
          style={{
            width: '115px',
            padding: '6px 6px',
            backgroundColor: FINCEPT.DARK_BG,
            color: FINCEPT.GRAY,
            border: BORDERS.STANDARD,
            borderRadius: '2px',
            fontSize: '10px',
            fontFamily: TYPOGRAPHY.MONO,
            outline: 'none',
          }}
          {...focus}
        />
        <span style={{ color: FINCEPT.MUTED, fontSize: '10px' }}>&rarr;</span>
        <input
          type="date"
          value={endDate}
          onChange={e => setEndDate(e.target.value)}
          style={{
            width: '115px',
            padding: '6px 6px',
            backgroundColor: FINCEPT.DARK_BG,
            color: FINCEPT.GRAY,
            border: BORDERS.STANDARD,
            borderRadius: '2px',
            fontSize: '10px',
            fontFamily: TYPOGRAPHY.MONO,
            outline: 'none',
          }}
          {...focus}
        />

        {/* Capital */}
        <div style={{ display: 'flex', alignItems: 'center' }}>
          <span style={{ color: FINCEPT.GREEN, fontSize: '10px', marginRight: '3px', fontWeight: 700 }}>$</span>
          <input
            type="number"
            value={initialCapital}
            onChange={e => setInitialCapital(Number(e.target.value))}
            style={{
              width: '80px',
              padding: '6px 6px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.GREEN,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              fontSize: '10px',
              fontFamily: TYPOGRAPHY.MONO,
              outline: 'none',
              fontWeight: 700,
            }}
            {...focus}
          />
        </div>
      </div>

      <div style={{
        width: '1px',
        height: '24px',
        backgroundColor: FINCEPT.BORDER,
        flexShrink: 0,
      }} />

      {/* Advanced toggle */}
      <button
        onClick={() => setShowAdvanced(!showAdvanced)}
        style={{
          ...COMMON_STYLES.buttonSecondary,
          padding: '6px 10px',
          fontSize: '10px',
          backgroundColor: showAdvanced ? `${FINCEPT.ORANGE}15` : 'transparent',
          borderColor: showAdvanced ? FINCEPT.ORANGE : FINCEPT.BORDER,
          color: showAdvanced ? FINCEPT.ORANGE : FINCEPT.GRAY,
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
        }}
      >
        <Settings size={11} />
        ADV
      </button>

      {/* RUN button */}
      <button
        onClick={handleRunCommand}
        disabled={isRunning}
        style={{
          ...COMMON_STYLES.buttonPrimary,
          padding: '7px 20px',
          fontSize: '10px',
          display: 'flex',
          alignItems: 'center',
          gap: '5px',
          opacity: isRunning ? 0.6 : 1,
          cursor: isRunning ? 'wait' : 'pointer',
          boxShadow: isRunning ? 'none' : `0 0 10px ${FINCEPT.ORANGE}40`,
        }}
      >
        {isRunning ? <Loader size={11} className="animate-spin" /> : <Play size={11} />}
        {isRunning ? 'RUNNING...' : 'RUN'}
      </button>
    </div>
  );
};
