/**
 * HyperLiquid Leverage Control
 * Set leverage (1x-50x) and margin mode
 */

import React, { useState } from 'react';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
};

interface LeverageControlProps {
  symbol: string;
  currentLeverage?: number;
  onLeverageChange?: (leverage: number) => void;
}

export function HyperLiquidLeverageControl({ symbol, currentLeverage = 1, onLeverageChange }: LeverageControlProps) {
  const { activeBroker, activeAdapter } = useBrokerContext();
  const [leverage, setLeverage] = useState(currentLeverage);
  const [marginMode, setMarginMode] = useState<'cross' | 'isolated'>('cross');
  const [isUpdating, setIsUpdating] = useState(false);

  if (activeBroker !== 'hyperliquid') {
    return null;
  }

  const handleLeverageChange = async (newLeverage: number) => {
    if (!activeAdapter) return;

    setLeverage(newLeverage);

    // Update on exchange
    setIsUpdating(true);
    try {
      await (activeAdapter as any).setLeverage(symbol, newLeverage);
      if (onLeverageChange) {
        onLeverageChange(newLeverage);
      }
    } catch (error) {
      console.error('Failed to set leverage:', error);
    } finally {
      setIsUpdating(false);
    }
  };

  const handleMarginModeChange = async (mode: 'cross' | 'isolated') => {
    if (!activeAdapter) return;

    setMarginMode(mode);

    setIsUpdating(true);
    try {
      await (activeAdapter as any).setMarginMode(symbol, mode);
    } catch (error) {
      console.error('Failed to set margin mode:', error);
    } finally {
      setIsUpdating(false);
    }
  };

  const quickLeverages = [1, 2, 5, 10, 20, 50];

  return (
    <div style={{ marginTop: '12px' }}>
      {/* Leverage Slider */}
      <div style={{ marginBottom: '12px' }}>
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            marginBottom: '6px',
          }}
        >
          <label
            style={{
              fontSize: '9px',
              fontWeight: 700,
              color: BLOOMBERG.GRAY,
              letterSpacing: '0.5px',
            }}
          >
            LEVERAGE
          </label>
          <span
            style={{
              fontSize: '12px',
              fontWeight: 700,
              color: leverage > 10 ? BLOOMBERG.RED : BLOOMBERG.ORANGE,
            }}
          >
            {leverage}x
          </span>
        </div>

        <input
          type="range"
          min="1"
          max="50"
          value={leverage}
          onChange={(e) => handleLeverageChange(parseInt(e.target.value))}
          disabled={isUpdating}
          style={{
            width: '100%',
            height: '4px',
            backgroundColor: BLOOMBERG.BORDER,
            outline: 'none',
            opacity: isUpdating ? 0.5 : 1,
          }}
        />

        {/* Quick Leverage Buttons */}
        <div style={{ display: 'flex', gap: '4px', marginTop: '6px' }}>
          {quickLeverages.map((lev) => (
            <button
              key={lev}
              onClick={() => handleLeverageChange(lev)}
              disabled={isUpdating}
              style={{
                flex: 1,
                padding: '4px',
                backgroundColor: leverage === lev ? BLOOMBERG.ORANGE : BLOOMBERG.PANEL_BG,
                border: `1px solid ${leverage === lev ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                color: leverage === lev ? BLOOMBERG.PANEL_BG : BLOOMBERG.GRAY,
                fontSize: '8px',
                fontWeight: 700,
                cursor: isUpdating ? 'not-allowed' : 'pointer',
                opacity: isUpdating ? 0.5 : 1,
              }}
            >
              {lev}x
            </button>
          ))}
        </div>
      </div>

      {/* Margin Mode */}
      <div>
        <label
          style={{
            display: 'block',
            fontSize: '9px',
            fontWeight: 700,
            color: BLOOMBERG.GRAY,
            marginBottom: '6px',
            letterSpacing: '0.5px',
          }}
        >
          MARGIN MODE
        </label>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => handleMarginModeChange('cross')}
            disabled={isUpdating}
            style={{
              flex: 1,
              padding: '6px',
              backgroundColor: marginMode === 'cross' ? BLOOMBERG.GREEN : BLOOMBERG.PANEL_BG,
              border: `1px solid ${marginMode === 'cross' ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}`,
              color: marginMode === 'cross' ? BLOOMBERG.PANEL_BG : BLOOMBERG.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: isUpdating ? 'not-allowed' : 'pointer',
              opacity: isUpdating ? 0.5 : 1,
            }}
          >
            CROSS
          </button>
          <button
            onClick={() => handleMarginModeChange('isolated')}
            disabled={isUpdating}
            style={{
              flex: 1,
              padding: '6px',
              backgroundColor: marginMode === 'isolated' ? BLOOMBERG.ORANGE : BLOOMBERG.PANEL_BG,
              border: `1px solid ${marginMode === 'isolated' ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
              color: marginMode === 'isolated' ? BLOOMBERG.PANEL_BG : BLOOMBERG.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: isUpdating ? 'not-allowed' : 'pointer',
              opacity: isUpdating ? 0.5 : 1,
            }}
          >
            ISOLATED
          </button>
        </div>
      </div>

      {/* Warning for high leverage */}
      {leverage > 20 && (
        <div
          style={{
            marginTop: '8px',
            padding: '6px 8px',
            backgroundColor: `${BLOOMBERG.RED}15`,
            border: `1px solid ${BLOOMBERG.RED}40`,
            fontSize: '8px',
            color: BLOOMBERG.RED,
          }}
        >
          ⚠️ High leverage ({leverage}x) increases liquidation risk
        </div>
      )}
    </div>
  );
}
