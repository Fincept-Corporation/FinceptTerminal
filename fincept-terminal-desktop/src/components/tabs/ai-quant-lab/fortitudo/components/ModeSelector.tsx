/**
 * ModeSelector Component
 * Tab-style selector for switching between analysis modes
 */

import React from 'react';
import { Shield, DollarSign, Target, Layers } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { AnalysisMode } from '../types';

const MODE_CONFIG = [
  { id: 'portfolio' as const, label: 'PORTFOLIO RISK', Icon: Shield },
  { id: 'options' as const, label: 'OPTION PRICING', Icon: DollarSign },
  { id: 'optimization' as const, label: 'OPTIMIZATION', Icon: Target },
  { id: 'entropy' as const, label: 'ENTROPY', Icon: Layers }
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
    <div
      className="flex rounded border overflow-hidden"
      style={{ borderColor: FINCEPT.BORDER }}
    >
      {MODE_CONFIG.map(({ id, label, Icon }) => (
        <button
          key={id}
          onClick={() => setActiveMode(id)}
          className="flex-1 flex items-center justify-center gap-2 px-4 py-2.5 text-xs font-semibold uppercase tracking-wide transition-all"
          style={{
            backgroundColor: activeMode === id ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
            color: activeMode === id ? FINCEPT.DARK_BG : FINCEPT.GRAY
          }}
        >
          <Icon size={14} />
          {label}
        </button>
      ))}
    </div>
  );
};
