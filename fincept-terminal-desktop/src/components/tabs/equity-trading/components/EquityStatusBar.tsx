/**
 * EquityStatusBar - Professional Terminal-Style Status Bar (matches CryptoStatusBar)
 */
import React from 'react';
import { Wifi, WifiOff } from 'lucide-react';
import { APP_VERSION } from '@/constants/version';
import { FINCEPT } from '../constants';
import type { EquityStatusBarProps } from '../types';

export const EquityStatusBar: React.FC<EquityStatusBarProps> = ({
  activeBrokerMetadata,
  tradingMode,
  isConnected,
  isPaper,
}) => {
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
      flexShrink: 0,
      fontFamily: '"IBM Plex Mono", monospace',
    }}>
      {/* Left - Branding */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>FINCEPT</span>
        <span>v{APP_VERSION}</span>
        <span style={{ color: FINCEPT.BORDER }}>|</span>
        <span>EQUITY TERMINAL</span>
      </div>

      {/* Right - Status */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        {/* Broker */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ color: FINCEPT.GRAY }}>BROKER:</span>
          <span style={{ color: FINCEPT.ORANGE, fontWeight: 600 }}>
            {activeBrokerMetadata?.displayName?.toUpperCase() || 'NONE'}
          </span>
        </div>

        <span style={{ color: FINCEPT.BORDER }}>|</span>

        {/* Trading Mode */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ color: FINCEPT.GRAY }}>MODE:</span>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            padding: '1px 6px',
            backgroundColor: isPaper ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
            border: `1px solid ${isPaper ? FINCEPT.GREEN : FINCEPT.RED}40`,
          }}>
            <div style={{
              width: '5px',
              height: '5px',
              backgroundColor: isPaper ? FINCEPT.GREEN : FINCEPT.RED,
              animation: !isPaper ? 'pulse 1s infinite' : 'none',
            }} />
            <span style={{
              color: isPaper ? FINCEPT.GREEN : FINCEPT.RED,
              fontWeight: 700,
              fontSize: '8px',
            }}>
              {tradingMode.toUpperCase()}
            </span>
          </div>
        </div>

        <span style={{ color: FINCEPT.BORDER }}>|</span>

        {/* Connection Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          {isConnected ? (
            <>
              <Wifi size={10} color={FINCEPT.GREEN} />
              <span style={{ color: FINCEPT.GREEN, fontWeight: 600 }}>CONNECTED</span>
            </>
          ) : (
            <>
              <WifiOff size={10} color={FINCEPT.RED} />
              <span style={{ color: FINCEPT.RED, fontWeight: 600 }}>OFFLINE</span>
            </>
          )}
        </div>

        <span style={{ color: FINCEPT.BORDER }}>|</span>

        {/* Time */}
        <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>
          {new Date().toLocaleTimeString('en-US', { hour12: false })}
        </span>
      </div>
    </div>
  );
};
