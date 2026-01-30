// CryptoTopNav.tsx - Top Navigation Bar for Crypto Trading
import React from 'react';
import {
  Activity, Globe, Wifi, WifiOff, ChevronDown,
  Settings as SettingsIcon
} from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { TimezoneSelector } from '@/components/common/TimezoneSelector';
import { FINCEPT } from '../constants';

interface BrokerInfo {
  id: string;
  name: string;
  type: string;
}

interface CryptoTopNavProps {
  activeBroker: string | null;
  availableBrokers: BrokerInfo[];
  tradingMode: 'paper' | 'live';
  isConnected: boolean;
  showBrokerDropdown: boolean;
  showSettings: boolean;
  onBrokerDropdownToggle: () => void;
  onBrokerChange: (brokerId: string) => void;
  onTradingModeChange: (mode: 'paper' | 'live') => void;
  onSettingsToggle: () => void;
}

export function CryptoTopNav({
  activeBroker,
  availableBrokers,
  tradingMode,
  isConnected,
  showBrokerDropdown,
  showSettings,
  onBrokerDropdownToggle,
  onBrokerChange,
  onTradingModeChange,
  onSettingsToggle,
}: CryptoTopNavProps) {
  const { t } = useTranslation('trading');

  const handleLiveModeClick = () => {
    if (tradingMode !== 'live') {
      const confirmed = window.confirm(
        '⚠️ WARNING: SWITCHING TO LIVE TRADING ⚠️\n\n' +
        'You are about to enable LIVE trading mode.\n\n' +
        '• All orders will be placed with REAL funds\n' +
        '• Trades will be executed on the actual exchange\n' +
        '• You may lose money\n\n' +
        'Are you sure you want to continue?'
      );
      if (confirmed) {
        onTradingModeChange('live');
      }
    }
  };

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
          <Activity size={18} color={FINCEPT.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + FINCEPT.ORANGE + ')' }} />
          <span style={{
            color: FINCEPT.ORANGE,
            fontWeight: 700,
            fontSize: '14px',
            letterSpacing: '0.5px',
            textShadow: `0 0 10px ${FINCEPT.ORANGE}40`
          }}>
            {t('header.terminal')}
          </span>
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {/* Broker Selector */}
        <div style={{ position: 'relative' }}>
          <button
            onClick={onBrokerDropdownToggle}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '4px 10px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.CYAN,
              cursor: 'pointer',
              fontSize: '11px',
              fontWeight: 600,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => e.currentTarget.style.borderColor = FINCEPT.CYAN}
            onMouseLeave={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
          >
            <Globe size={12} />
            {activeBroker?.toUpperCase() || t('header.selectBroker')}
            <ChevronDown size={10} />
          </button>

          {showBrokerDropdown && (
            <div style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              marginTop: '4px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              minWidth: '200px',
              zIndex: 1000,
              boxShadow: `0 4px 16px ${FINCEPT.DARK_BG}80`
            }}>
              {availableBrokers.map((broker) => (
                <div
                  key={broker.id}
                  onClick={() => onBrokerChange(broker.id)}
                  style={{
                    padding: '10px 12px',
                    cursor: 'pointer',
                    fontSize: '11px',
                    backgroundColor: activeBroker === broker.id ? `${FINCEPT.ORANGE}20` : 'transparent',
                    borderLeft: activeBroker === broker.id ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    if (activeBroker !== broker.id) {
                      e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (activeBroker !== broker.id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <div>
                    <div style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{broker.name}</div>
                    <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>{broker.type.toUpperCase()}</div>
                  </div>
                  {activeBroker === broker.id && (
                    <div style={{
                      width: '6px',
                      height: '6px',
                      borderRadius: '50%',
                      backgroundColor: FINCEPT.GREEN,
                      boxShadow: `0 0 6px ${FINCEPT.GREEN}`
                    }} />
                  )}
                </div>
              ))}
            </div>
          )}
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {/* Trading Mode Toggle - Matches Equity Tab Style */}
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
              backgroundColor: tradingMode === 'paper' ? FINCEPT.GREEN : 'transparent',
              border: 'none',
              color: tradingMode === 'paper' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              transition: 'all 0.2s',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            {tradingMode === 'paper' && (
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
            onClick={handleLiveModeClick}
            style={{
              padding: '6px 14px',
              backgroundColor: tradingMode === 'live' ? FINCEPT.RED : 'transparent',
              border: 'none',
              color: tradingMode === 'live' ? FINCEPT.WHITE : FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              transition: 'all 0.2s',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            {tradingMode === 'live' && (
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

        <TimezoneSelector compact />

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
            transition: 'all 0.2s'
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
}

export default CryptoTopNav;
