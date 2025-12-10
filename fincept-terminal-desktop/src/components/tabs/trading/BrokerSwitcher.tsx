// File: src/components/tabs/trading/BrokerSwitcher.tsx
// Enhanced broker selection with metadata and capabilities display

import React from 'react';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import { Activity, Zap, Shield, TrendingUp } from 'lucide-react';

const BLOOMBERG_COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  GREEN: '#10b981',
  GRAY: '#525252',
  DARK_BG: '#0d0d0d',
  PANEL_BG: '#1a1a1a',
};

export function BrokerSwitcher() {
  const {
    activeBroker,
    activeBrokerMetadata,
    availableBrokers,
    setActiveBroker,
    tradingMode,
    setTradingMode,
    isLoading,
    isConnecting,
  } = useBrokerContext();

  const { ORANGE, WHITE, GREEN, GRAY, DARK_BG, PANEL_BG } = BLOOMBERG_COLORS;

  if (isLoading) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        backgroundColor: DARK_BG,
        padding: '8px 12px',
        borderBottom: `1px solid ${GRAY}`,
      }}>
        <span style={{ fontSize: '10px', color: GRAY, fontWeight: 'bold' }}>BROKER:</span>
        <div style={{ fontSize: '10px', color: GRAY }}>Loading...</div>
      </div>
    );
  }

  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      backgroundColor: PANEL_BG,
      padding: '8px 12px',
      borderBottom: `1px solid ${GRAY}`,
    }}>
      {/* Left: Broker Selection */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <span style={{ fontSize: '10px', color: GRAY, fontWeight: 'bold', textTransform: 'uppercase' }}>
          Broker:
        </span>
        <div style={{ display: 'flex', gap: '6px' }}>
          {availableBrokers.map(broker => {
            const isActive = activeBroker === broker.id;
            return (
              <button
                key={broker.id}
                onClick={() => !isLoading && setActiveBroker(broker.id)}
                disabled={isLoading || isConnecting}
                style={{
                  padding: '4px 12px',
                  borderRadius: '2px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  textTransform: 'uppercase',
                  backgroundColor: isActive ? ORANGE : '#2d2d2d',
                  color: isActive ? '#000' : GRAY,
                  border: isActive ? `1px solid ${ORANGE}` : '1px solid #404040',
                  cursor: (isLoading || isConnecting) ? 'not-allowed' : 'pointer',
                  opacity: (isLoading || isConnecting) ? 0.5 : 1,
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  if (!isActive && !isLoading && !isConnecting) {
                    e.currentTarget.style.backgroundColor = '#404040';
                    e.currentTarget.style.borderColor = GRAY;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = '#2d2d2d';
                    e.currentTarget.style.borderColor = '#404040';
                  }
                }}
              >
                {broker.displayName}
              </button>
            );
          })}
        </div>

        {/* Broker Type Badge */}
        {activeBrokerMetadata && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <span style={{
              fontSize: '9px',
              color: '#666',
              padding: '2px 6px',
              backgroundColor: '#1a1a1a',
              border: '1px solid #333',
              borderRadius: '2px',
              textTransform: 'uppercase',
            }}>
              {activeBrokerMetadata.category === 'centralized' ? (
                <>
                  <Shield size={10} style={{ display: 'inline', marginRight: '4px' }} />
                  CEX
                </>
              ) : (
                <>
                  <Zap size={10} style={{ display: 'inline', marginRight: '4px' }} />
                  DEX
                </>
              )}
            </span>

            {/* Max Leverage Badge */}
            {activeBrokerMetadata.advancedFeatures.leverage && (
              <span style={{
                fontSize: '9px',
                color: ORANGE,
                padding: '2px 6px',
                backgroundColor: '#1a1a1a',
                border: `1px solid ${ORANGE}40`,
                borderRadius: '2px',
                fontWeight: 'bold',
              }}>
                <TrendingUp size={10} style={{ display: 'inline', marginRight: '4px' }} />
                {activeBrokerMetadata.advancedFeatures.maxLeverage}x
              </span>
            )}
          </div>
        )}
      </div>

      {/* Right: Trading Mode & Status */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        {/* Trading Mode Toggle */}
        <div style={{ display: 'flex', gap: '4px', backgroundColor: DARK_BG, padding: '2px', borderRadius: '2px' }}>
          <button
            onClick={() => setTradingMode('paper')}
            disabled={isLoading}
            style={{
              padding: '4px 10px',
              fontSize: '10px',
              fontWeight: 'bold',
              backgroundColor: tradingMode === 'paper' ? ORANGE : 'transparent',
              color: tradingMode === 'paper' ? '#000' : GRAY,
              border: 'none',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              transition: 'all 0.2s',
            }}
          >
            PAPER
          </button>
          <button
            onClick={() => setTradingMode('live')}
            disabled={isLoading}
            style={{
              padding: '4px 10px',
              fontSize: '10px',
              fontWeight: 'bold',
              backgroundColor: tradingMode === 'live' ? '#dc2626' : 'transparent',
              color: tradingMode === 'live' ? WHITE : GRAY,
              border: 'none',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              transition: 'all 0.2s',
            }}
          >
            LIVE
          </button>
        </div>

        {/* Connection Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Activity
            size={12}
            style={{
              color: isConnecting ? ORANGE : GREEN,
              animation: isConnecting ? 'pulse 1.5s ease-in-out infinite' : 'none',
            }}
          />
          <span style={{
            fontSize: '10px',
            color: isConnecting ? ORANGE : GREEN,
            fontWeight: 'bold',
            textTransform: 'uppercase',
          }}>
            {isConnecting ? 'Connecting...' : 'Connected'}
          </span>
        </div>
      </div>

      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>
    </div>
  );
}

// Re-export as ProviderSwitcher for backward compatibility
export { BrokerSwitcher as ProviderSwitcher };
