/**
 * Status Bar Component
 * Shows system status and info
 */

import React from 'react';
import { CheckCircle, XCircle } from 'lucide-react';
import { APP_VERSION } from '@/constants/version';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface StatusBarProps {
  qlibStatus: { available: boolean };
  rdAgentStatus: { available: boolean };
}

export function StatusBar({ qlibStatus, rdAgentStatus }: StatusBarProps) {
  const { colors } = useTerminalTheme();

  return (
    <div
      className="flex items-center justify-between px-6 py-2 border-t text-xs uppercase"
      style={{ backgroundColor: colors.panel, borderColor: 'var(--ft-border-color, #2A2A2A)' }}
    >
      <div className="flex items-center gap-6">
        <div className="flex items-center gap-2">
          {qlibStatus.available ? (
            <CheckCircle size={14} color={colors.success} />
          ) : (
            <XCircle size={14} color={colors.alert} />
          )}
          <span style={{ color: colors.textMuted }}>
            Qlib: {qlibStatus.available ? 'Installed' : 'Not Installed'}
          </span>
        </div>
        <div className="flex items-center gap-2">
          {rdAgentStatus.available ? (
            <CheckCircle size={14} color={colors.success} />
          ) : (
            <XCircle size={14} color={colors.alert} />
          )}
          <span style={{ color: colors.textMuted }}>
            RD-Agent: {rdAgentStatus.available ? 'Installed' : 'Not Installed'}
          </span>
        </div>
      </div>
      <div style={{ color: colors.textMuted }}>
        AI Quant Lab v{APP_VERSION} | Microsoft Qlib + RD-Agent Integration
      </div>
    </div>
  );
}
