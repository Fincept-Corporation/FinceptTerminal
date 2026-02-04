/**
 * ModeSelector Component
 * Terminal-style vertical sidebar for switching between analysis modes
 */

import React from 'react';
import { Shield, DollarSign, Target, Layers } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { AnalysisMode } from '../types';

const MODE_CONFIG = [
  { id: 'portfolio' as const, label: 'Portfolio Risk', shortLabel: 'PORTFOLIO', Icon: Shield, color: FINCEPT.BLUE },
  { id: 'options' as const, label: 'Option Pricing', shortLabel: 'OPTIONS', Icon: DollarSign, color: FINCEPT.GREEN },
  { id: 'optimization' as const, label: 'Optimization', shortLabel: 'OPTIMIZE', Icon: Target, color: FINCEPT.ORANGE },
  { id: 'entropy' as const, label: 'Entropy Pooling', shortLabel: 'ENTROPY', Icon: Layers, color: FINCEPT.PURPLE }
];

interface ModeSelectorProps {
  activeMode: AnalysisMode;
  setActiveMode: (mode: AnalysisMode) => void;
}

export const ModeSelector: React.FC<ModeSelectorProps> = ({
  activeMode,
  setActiveMode
}) => {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{
        padding: '10px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        fontSize: '10px',
        fontWeight: 700,
        color: FINCEPT.BLUE,
        fontFamily: 'monospace',
        letterSpacing: '0.5px'
      }}>
        ANALYSIS MODE
      </div>
      <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '0' }}>
        {MODE_CONFIG.map(({ id, label, shortLabel, Icon, color }, idx) => {
          const isActive = activeMode === id;
          return (
            <button
              key={id}
              onClick={() => setActiveMode(id)}
              style={{
                padding: '10px',
                backgroundColor: isActive ? FINCEPT.HOVER : 'transparent',
                border: `1px solid ${isActive ? FINCEPT.BLUE : FINCEPT.BORDER}`,
                borderTop: idx === 0 ? `1px solid ${isActive ? FINCEPT.BLUE : FINCEPT.BORDER}` : '0',
                cursor: 'pointer',
                transition: 'all 0.15s',
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'flex-start',
                gap: '6px',
                textAlign: 'left',
                marginTop: idx === 0 ? '0' : '-1px'
              }}
              onMouseEnter={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                  e.currentTarget.style.borderColor = FINCEPT.BLUE;
                }
              }}
              onMouseLeave={(e) => {
                if (!isActive) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                }
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <Icon size={14} style={{ color: FINCEPT.BLUE }} />
                <span style={{
                  color: FINCEPT.WHITE,
                  fontSize: '11px',
                  fontWeight: 600,
                  fontFamily: 'monospace'
                }}>
                  {label}
                </span>
              </div>
              <span style={{
                color: FINCEPT.WHITE,
                opacity: 0.5,
                fontSize: '9px',
                fontFamily: 'monospace',
                letterSpacing: '0.5px'
              }}>
                {shortLabel}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
};
