/**
 * EquityTopNav - Top navigation bar for equity trading terminal
 */
import React from 'react';
import { Building2, Wifi, WifiOff, Settings as SettingsIcon } from 'lucide-react';
import { showConfirm } from '@/utils/notifications';
import { BrokerSelector } from './BrokerSelector';
import { FINCEPT } from '../constants';
import type { EquityTopNavProps } from '../types';

export const EquityTopNav: React.FC<EquityTopNavProps> = ({
  activeBroker,
  tradingMode,
  isConnected,
  isPaper,
  isLive,
  showSettings,
  currentTime,
  onTradingModeChange,
  onSettingsToggle,
}) => {
  return (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `2px solid ${FINCEPT.ORANGE}`,
      padding: '6px 12px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      flexShrink: 0,
      boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        {/* Terminal Branding */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Building2 size={18} color={FINCEPT.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + FINCEPT.ORANGE + ')' }} />
          <span style={{
            color: FINCEPT.ORANGE,
            fontWeight: 700,
            fontSize: '14px',
            letterSpacing: '0.5px',
            textShadow: `0 0 10px ${FINCEPT.ORANGE}40`
          }}>
            EQUITY TERMINAL
          </span>
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {/* Broker Selector */}
        <BrokerSelector />

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {/* Trading Mode Toggle */}
        {activeBroker === 'yfinance' ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            backgroundColor: FINCEPT.GREEN,
            padding: '6px 14px',
            fontSize: '10px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            color: FINCEPT.DARK_BG,
          }}>
            <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: FINCEPT.DARK_BG }} />
            PAPER ONLY
          </div>
        ) : (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '2px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            padding: '2px',
          }}>
            <button
              onClick={() => onTradingModeChange('paper')}
              style={{
                padding: '6px 14px',
                backgroundColor: isPaper ? FINCEPT.GREEN : 'transparent',
                border: 'none',
                color: isPaper ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.15s',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              {isPaper && (
                <div style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: FINCEPT.DARK_BG,
                }} />
              )}
              PAPER
            </button>
            <button
              onClick={async () => {
                if (!isLive) {
                  const confirmed = await showConfirm(
                    'You are about to enable LIVE trading mode. All orders will be placed with REAL funds and trades will be executed on the actual exchange.\n\nMODE: LIVE TRADING\nRISK: REAL FUNDS AT RISK - YOU MAY LOSE MONEY',
                    {
                      title: 'WARNING: SWITCHING TO LIVE TRADING',
                      type: 'danger'
                    }
                  );
                  if (confirmed) {
                    onTradingModeChange('live');
                  }
                }
              }}
              style={{
                padding: '6px 14px',
                backgroundColor: isLive ? FINCEPT.RED : 'transparent',
                border: 'none',
                color: isLive ? FINCEPT.WHITE : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.15s',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              {isLive && (
                <div style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: FINCEPT.WHITE,
                  boxShadow: `0 0 6px ${FINCEPT.WHITE}`,
                  animation: 'pulse 1s infinite',
                }} />
              )}
              LIVE
            </button>
          </div>
        )}
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        {/* Connection Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '10px' }}>
          {isConnected ? (
            <>
              <Wifi size={12} color={FINCEPT.GREEN} />
              <span style={{ color: FINCEPT.GREEN }}>CONNECTED</span>
            </>
          ) : (
            <>
              <WifiOff size={12} color={FINCEPT.RED} />
              <span style={{ color: FINCEPT.RED }}>DISCONNECTED</span>
            </>
          )}
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {/* Clock */}
        <div style={{
          fontSize: '11px',
          fontWeight: 600,
          color: FINCEPT.CYAN,
          fontFamily: 'monospace'
        }}>
          {currentTime.toLocaleTimeString('en-IN', { hour12: false })}
        </div>

        {/* Settings */}
        <button
          onClick={onSettingsToggle}
          style={{
            padding: '4px 8px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            fontSize: '10px',
            transition: 'all 0.15s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.borderColor = FINCEPT.ORANGE;
            e.currentTarget.style.color = FINCEPT.ORANGE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
            e.currentTarget.style.color = FINCEPT.GRAY;
          }}
        >
          <SettingsIcon size={12} />
        </button>
      </div>
    </div>
  );
};
