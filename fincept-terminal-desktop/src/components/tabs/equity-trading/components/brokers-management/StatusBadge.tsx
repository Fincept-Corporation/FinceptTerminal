import React from 'react';
import { CheckCircle2, XCircle, AlertTriangle, Key } from 'lucide-react';
import { COLORS, ConnectionStatus } from './types';

interface StatusBadgeProps {
  status: ConnectionStatus;
}

export function StatusBadge({ status }: StatusBadgeProps) {
  switch (status) {
    case 'connected':
      return (
        <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.GREEN, fontSize: '10px' }}>
          <CheckCircle2 size={12} />
          CONNECTED
        </span>
      );
    case 'expired':
      return (
        <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.YELLOW, fontSize: '10px' }}>
          <AlertTriangle size={12} />
          EXPIRED
        </span>
      );
    case 'configured':
      return (
        <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.CYAN, fontSize: '10px' }}>
          <Key size={12} />
          CONFIGURED
        </span>
      );
    default:
      return (
        <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.GRAY, fontSize: '10px' }}>
          <XCircle size={12} />
          NOT CONFIGURED
        </span>
      );
  }
}
