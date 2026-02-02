// CryptoTopNav.tsx - Professional Terminal-Style Top Navigation Bar
import React, { useState, useCallback } from 'react';
import { Globe, Wifi, WifiOff, ChevronDown, Settings as SettingsIcon, AlertTriangle, Zap } from 'lucide-react';
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

// Live Mode Confirmation Modal - Terminal style
function LiveModeConfirmModal({ isOpen, onConfirm, onCancel }: { isOpen: boolean; onConfirm: () => void; onCancel: () => void }) {
  if (!isOpen) return null;

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.9)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 10000,
    }}>
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `2px solid ${FINCEPT.RED}`,
        padding: '20px',
        maxWidth: '400px',
        width: '90%',
        boxShadow: `0 0 20px ${FINCEPT.RED}40`,
      }}>
        {/* Header */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
          marginBottom: '16px',
          paddingBottom: '12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <AlertTriangle size={20} color={FINCEPT.RED} />
          <div>
            <div style={{ color: FINCEPT.WHITE, fontSize: '13px', fontWeight: 700, letterSpacing: '0.5px' }}>
              [WARN] ENABLE LIVE TRADING
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
              This action requires confirmation
            </div>
          </div>
        </div>

        {/* Warning Content */}
        <div style={{
          padding: '12px',
          backgroundColor: `${FINCEPT.RED}10`,
          border: `1px solid ${FINCEPT.RED}30`,
          marginBottom: '16px',
          fontSize: '10px',
          color: FINCEPT.WHITE,
          lineHeight: 1.6,
        }}>
          <div style={{ marginBottom: '8px', fontWeight: 600, color: FINCEPT.RED }}>
            WARNING: SWITCHING TO LIVE MODE
          </div>
          <div style={{ marginBottom: '4px' }}>- Orders will execute with REAL funds</div>
          <div style={{ marginBottom: '4px' }}>- Trades are IRREVERSIBLE</div>
          <div style={{ color: FINCEPT.YELLOW }}>- You may LOSE money</div>
        </div>

        {/* Buttons */}
        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={onCancel}
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              fontSize: '10px',
              fontWeight: 600,
              transition: 'all 0.15s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onConfirm}
            style={{
              flex: 1,
              padding: '10px',
              backgroundColor: FINCEPT.RED,
              border: `1px solid ${FINCEPT.RED}`,
              color: FINCEPT.WHITE,
              cursor: 'pointer',
              fontSize: '10px',
              fontWeight: 700,
              transition: 'all 0.15s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#FF5555';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.RED;
            }}
          >
            ENABLE LIVE MODE
          </button>
        </div>
      </div>
    </div>
  );
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
  const [showLiveModeModal, setShowLiveModeModal] = useState(false);

  const handleLiveModeClick = useCallback(() => {
    if (tradingMode !== 'live') {
      setShowLiveModeModal(true);
    }
  }, [tradingMode]);

  const handleConfirmLiveMode = useCallback(() => {
    setShowLiveModeModal(false);
    onTradingModeChange('live');
  }, [onTradingModeChange]);

  return (
    <>
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>

      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}>
        {/* Left Section */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          {/* Terminal Branding */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Zap size={18} color={FINCEPT.ORANGE} style={{ filter: `drop-shadow(0 0 4px ${FINCEPT.ORANGE})` }} />
            <span style={{
              color: FINCEPT.ORANGE,
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: `0 0 10px ${FINCEPT.ORANGE}40`,
            }}>
              CRYPTO TERMINAL
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
                gap: '8px',
                padding: '6px 12px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
                cursor: 'pointer',
                fontSize: '11px',
                fontWeight: 600,
                transition: 'all 0.15s',
                minWidth: '130px',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                e.currentTarget.style.borderColor = FINCEPT.GRAY;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
              }}
            >
              <Globe size={12} color={FINCEPT.CYAN} />
              <span>{activeBroker?.toUpperCase() || 'SELECT'}</span>
              <ChevronDown size={10} color={FINCEPT.GRAY} style={{ marginLeft: 'auto' }} />
            </button>

            {showBrokerDropdown && (
              <div style={{
                position: 'absolute',
                top: 'calc(100% + 2px)',
                left: 0,
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                minWidth: '200px',
                zIndex: 1000,
                boxShadow: '0 4px 16px rgba(0,0,0,0.5)',
              }}>
                <div style={{
                  padding: '6px 10px',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '9px',
                  color: FINCEPT.GRAY,
                  fontWeight: 600,
                  letterSpacing: '0.5px',
                }}>
                  SELECT EXCHANGE
                </div>
                {availableBrokers.map((broker) => (
                  <div
                    key={broker.id}
                    onClick={() => onBrokerChange(broker.id)}
                    style={{
                      padding: '10px 12px',
                      cursor: 'pointer',
                      fontSize: '11px',
                      backgroundColor: activeBroker === broker.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                      borderLeft: activeBroker === broker.id ? `3px solid ${FINCEPT.ORANGE}` : '3px solid transparent',
                      transition: 'all 0.15s',
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
                    <div style={{
                      color: activeBroker === broker.id ? FINCEPT.ORANGE : FINCEPT.WHITE,
                      fontWeight: 600,
                    }}>
                      {broker.name}
                    </div>
                    <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                      {broker.type.toUpperCase()}
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

          {/* Trading Mode Toggle - Boxy terminal style */}
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
                transition: 'all 0.15s',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              {tradingMode === 'paper' && (
                <div style={{
                  width: '6px',
                  height: '6px',
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
                transition: 'all 0.15s',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              {tradingMode === 'live' && (
                <div style={{
                  width: '6px',
                  height: '6px',
                  backgroundColor: FINCEPT.WHITE,
                  boxShadow: `0 0 6px ${FINCEPT.WHITE}`,
                  animation: 'pulse 1s infinite',
                }} />
              )}
              LIVE
            </button>
          </div>
        </div>

        {/* Right Section */}
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
              padding: '6px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              transition: 'all 0.15s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <SettingsIcon size={14} />
          </button>
        </div>
      </div>

      <LiveModeConfirmModal
        isOpen={showLiveModeModal}
        onConfirm={handleConfirmLiveMode}
        onCancel={() => setShowLiveModeModal(false)}
      />
    </>
  );
}

export default CryptoTopNav;
