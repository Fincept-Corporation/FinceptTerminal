/**
 * Header Component
 * Terminal-style header bar with title and status
 */

import React from 'react';
import { Shield, CheckCircle2 } from 'lucide-react';
import { FINCEPT } from '../constants';

export const Header: React.FC = () => {
  return (
    <div style={{
      padding: '12px 16px',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      backgroundColor: FINCEPT.PANEL_BG,
      display: 'flex',
      alignItems: 'center',
      gap: '12px'
    }}>
      <Shield size={16} color={FINCEPT.BLUE} />
      <span style={{
        color: FINCEPT.BLUE,
        fontSize: '12px',
        fontWeight: 700,
        letterSpacing: '0.5px',
        fontFamily: 'monospace'
      }}>
        FORTITUDO.TECH RISK ANALYTICS
      </span>
      <div style={{ flex: 1 }} />
      <div style={{
        fontSize: '10px',
        fontFamily: 'monospace',
        padding: '3px 8px',
        backgroundColor: FINCEPT.GREEN + '20',
        border: `1px solid ${FINCEPT.GREEN}`,
        color: FINCEPT.GREEN,
        display: 'flex',
        alignItems: 'center',
        gap: '4px'
      }}>
        <CheckCircle2 size={12} />
        READY
      </div>
    </div>
  );
};
