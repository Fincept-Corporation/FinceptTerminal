/**
 * Status Bar Component
 * Shows system status and info
 */

import React from 'react';
import { CheckCircle, XCircle } from 'lucide-react';
import { APP_VERSION } from '@/constants/version';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface StatusBarProps {
  qlibStatus: { available: boolean; initialized: boolean };
  rdAgentStatus: { available: boolean; initialized: boolean };
}

export function StatusBar({ qlibStatus, rdAgentStatus }: StatusBarProps) {
  return (
    <div
      className="flex items-center justify-between px-6 py-2 border-t text-xs uppercase"
      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
    >
      <div className="flex items-center gap-6">
        <div className="flex items-center gap-2">
          {qlibStatus.available ? (
            <CheckCircle size={14} color={FINCEPT.GREEN} />
          ) : (
            <XCircle size={14} color={FINCEPT.RED} />
          )}
          <span style={{ color: FINCEPT.GRAY }}>
            Qlib: {qlibStatus.initialized ? 'Ready' : 'Not Initialized'}
          </span>
        </div>
        <div className="flex items-center gap-2">
          {rdAgentStatus.available ? (
            <CheckCircle size={14} color={FINCEPT.GREEN} />
          ) : (
            <XCircle size={14} color={FINCEPT.RED} />
          )}
          <span style={{ color: FINCEPT.GRAY }}>
            RD-Agent: {rdAgentStatus.initialized ? 'Ready' : 'Not Initialized'}
          </span>
        </div>
      </div>
      <div style={{ color: FINCEPT.GRAY }}>
        AI Quant Lab v{APP_VERSION} | Microsoft Qlib + RD-Agent Integration
      </div>
    </div>
  );
}
