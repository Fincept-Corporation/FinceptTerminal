// CryptoStatusBar.tsx - Bottom Status Bar for Crypto Trading
import React from 'react';
import { FINCEPT } from '../constants';
import { APP_VERSION } from '@/constants/version';

interface CryptoStatusBarProps {
  activeBroker: string | null;
  tradingMode: 'paper' | 'live';
  isConnected: boolean;
}

export function CryptoStatusBar({
  activeBroker,
  tradingMode,
  isConnected,
}: CryptoStatusBarProps) {
  return (
    <div style={{
      borderTop: `1px solid ${FINCEPT.BORDER}`,
      backgroundColor: FINCEPT.HEADER_BG,
      padding: '4px 12px',
      fontSize: '9px',
      color: FINCEPT.GRAY,
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      flexShrink: 0
    }}>
      <span>Fincept Terminal v{APP_VERSION} | Professional Crypto Trading Platform</span>
      <span>
        Broker: <span style={{ color: FINCEPT.ORANGE }}>{activeBroker?.toUpperCase() || 'NONE'}</span> |
        Mode: <span style={{ color: tradingMode === 'paper' ? FINCEPT.GREEN : FINCEPT.RED }}>
          {tradingMode.toUpperCase()}
        </span> |
        Status: <span style={{ color: isConnected ? FINCEPT.GREEN : FINCEPT.RED }}>
          {isConnected ? 'CONNECTED' : 'DISCONNECTED'}
        </span>
      </span>
    </div>
  );
}

export default CryptoStatusBar;
