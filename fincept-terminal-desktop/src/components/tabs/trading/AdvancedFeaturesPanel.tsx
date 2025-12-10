// File: src/components/tabs/trading/AdvancedFeaturesPanel.tsx
// Advanced trading features panel with leverage, margin mode, and broker-specific controls

import React, { useState } from 'react';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import { Sliders, ArrowUpDown, Settings, Zap } from 'lucide-react';

const COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  GREEN: '#10b981',
  RED: '#dc2626',
  GRAY: '#525252',
  DARK_BG: '#0d0d0d',
  PANEL_BG: '#1a1a1a',
};

export function AdvancedFeaturesPanel() {
  const {
    activeBroker,
    activeBrokerMetadata,
    activeAdapter,
    supports,
    tradingMode,
  } = useBrokerContext();

  const [leverage, setLeverage] = useState(1);
  const [marginMode, setMarginMode] = useState<'cross' | 'isolated'>('cross');
  const [isLoading, setIsLoading] = useState(false);

  const { ORANGE, WHITE, GREEN, RED, GRAY, PANEL_BG, DARK_BG } = COLORS;

  const canSetLeverage = supports?.canSetLeverage() || false;
  const canTransfer = supports?.canTransfer() || false;
  const hasVaults = supports?.hasVaults() || false;
  const hasSubaccounts = supports?.hasSubaccounts() || false;
  const maxLeverage = activeBrokerMetadata?.advancedFeatures.maxLeverage || 1;

  // Handle leverage change
  const handleLeverageChange = async (newLeverage: number) => {
    if (!activeAdapter || !canSetLeverage) return;

    setLeverage(newLeverage);

    if (tradingMode === 'live') {
      try {
        setIsLoading(true);
        if ('setLeverage' in activeAdapter) {
          await (activeAdapter as any).setLeverage('', newLeverage);
        }
      } catch (error) {
        console.error('Failed to set leverage:', error);
      } finally {
        setIsLoading(false);
      }
    }
  };

  // Handle margin mode change
  const handleMarginModeChange = async (mode: 'cross' | 'isolated') => {
    if (!activeAdapter) return;

    setMarginMode(mode);

    if (tradingMode === 'live') {
      try {
        setIsLoading(true);
        if ('setMarginMode' in activeAdapter) {
          await (activeAdapter as any).setMarginMode('', mode);
        }
      } catch (error) {
        console.error('Failed to set margin mode:', error);
      } finally {
        setIsLoading(false);
      }
    }
  };

  // Don't render if no advanced features
  if (!canSetLeverage && !canTransfer && !hasVaults && !hasSubaccounts) {
    return null;
  }

  return (
    <div style={{
      backgroundColor: PANEL_BG,
      borderBottom: `1px solid ${GRAY}`,
      padding: '8px 12px',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px', flexWrap: 'wrap' }}>
        {/* Header */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Settings size={12} style={{ color: ORANGE }} />
          <span style={{ fontSize: '10px', color: GRAY, fontWeight: 'bold', textTransform: 'uppercase' }}>
            Advanced:
          </span>
        </div>

        {/* Leverage Control */}
        {canSetLeverage && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Sliders size={12} style={{ color: GRAY }} />
            <span style={{ fontSize: '10px', color: GRAY }}>Leverage:</span>
            <input
              type="range"
              min="1"
              max={maxLeverage}
              value={leverage}
              onChange={(e) => handleLeverageChange(Number(e.target.value))}
              disabled={isLoading}
              style={{
                width: '100px',
                height: '4px',
                accentColor: ORANGE,
                cursor: isLoading ? 'not-allowed' : 'pointer',
              }}
            />
            <span style={{
              fontSize: '11px',
              fontWeight: 'bold',
              color: leverage > 1 ? ORANGE : WHITE,
              minWidth: '35px',
              textAlign: 'right',
            }}>
              {leverage}x
            </span>
          </div>
        )}

        {/* Margin Mode */}
        {canSetLeverage && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <ArrowUpDown size={12} style={{ color: GRAY }} />
            <span style={{ fontSize: '10px', color: GRAY }}>Margin:</span>
            <div style={{ display: 'flex', gap: '4px', backgroundColor: DARK_BG, padding: '2px', borderRadius: '2px' }}>
              <button
                onClick={() => handleMarginModeChange('cross')}
                disabled={isLoading}
                style={{
                  padding: '3px 8px',
                  fontSize: '9px',
                  fontWeight: 'bold',
                  backgroundColor: marginMode === 'cross' ? ORANGE : 'transparent',
                  color: marginMode === 'cross' ? '#000' : GRAY,
                  border: 'none',
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  borderRadius: '2px',
                  textTransform: 'uppercase',
                }}
              >
                Cross
              </button>
              <button
                onClick={() => handleMarginModeChange('isolated')}
                disabled={isLoading}
                style={{
                  padding: '3px 8px',
                  fontSize: '9px',
                  fontWeight: 'bold',
                  backgroundColor: marginMode === 'isolated' ? ORANGE : 'transparent',
                  color: marginMode === 'isolated' ? '#000' : GRAY,
                  border: 'none',
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  borderRadius: '2px',
                  textTransform: 'uppercase',
                }}
              >
                Isolated
              </button>
            </div>
          </div>
        )}

        {/* HyperLiquid Specific Features */}
        {activeBroker === 'hyperliquid' && (
          <>
            {hasVaults && (
              <div style={{
                padding: '3px 8px',
                fontSize: '9px',
                fontWeight: 'bold',
                backgroundColor: '#1a1a1a',
                color: ORANGE,
                border: `1px solid ${ORANGE}40`,
                borderRadius: '2px',
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}>
                <Zap size={10} />
                Vaults Available
              </div>
            )}
            {hasSubaccounts && (
              <div style={{
                padding: '3px 8px',
                fontSize: '9px',
                fontWeight: 'bold',
                backgroundColor: '#1a1a1a',
                color: GREEN,
                border: `1px solid ${GREEN}40`,
                borderRadius: '2px',
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}>
                Subaccounts
              </div>
            )}
          </>
        )}

        {/* Trading Mode Warning */}
        {tradingMode === 'paper' && canSetLeverage && (
          <span style={{
            fontSize: '9px',
            color: '#666',
            fontStyle: 'italic',
            marginLeft: 'auto',
          }}>
            Paper mode - changes apply to virtual trading only
          </span>
        )}
      </div>
    </div>
  );
}
