// CryptoSettingsPanel.tsx - Terminal-style settings modal for crypto trading
import React from 'react';
import { X, Sliders, DollarSign, Percent, RotateCcw, Trash2 } from 'lucide-react';
import { FINCEPT } from '../constants';

interface CryptoSettingsPanelProps {
  isOpen: boolean;
  onClose: () => void;
  // Trading settings
  slippage: number;
  makerFee: number;
  takerFee: number;
  onSlippageChange: (value: number) => void;
  onMakerFeeChange: (value: number) => void;
  onTakerFeeChange: (value: number) => void;
  // Portfolio actions
  onResetPortfolio?: () => void;
  tradingMode: 'paper' | 'live';
}

export function CryptoSettingsPanel({
  isOpen,
  onClose,
  slippage,
  makerFee,
  takerFee,
  onSlippageChange,
  onMakerFeeChange,
  onTakerFeeChange,
  onResetPortfolio,
  tradingMode,
}: CryptoSettingsPanelProps) {
  if (!isOpen) return null;

  const handleSlippageChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const value = parseFloat(e.target.value);
    if (!isNaN(value) && value >= 0 && value <= 10) {
      onSlippageChange(value);
    }
  };

  const handleResetPortfolio = () => {
    if (onResetPortfolio && window.confirm('Are you sure you want to reset your paper trading portfolio? This will clear all positions, orders, and trade history.')) {
      onResetPortfolio();
      onClose();
    }
  };

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.85)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 10000,
      }}
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${FINCEPT.ORANGE}`,
          width: '400px',
          maxWidth: '90vw',
          maxHeight: '80vh',
          overflow: 'auto',
          boxShadow: `0 0 30px ${FINCEPT.ORANGE}30`,
        }}
      >
        {/* Header */}
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '12px 16px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: FINCEPT.HEADER_BG,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <Sliders size={16} color={FINCEPT.ORANGE} />
            <span
              style={{
                color: FINCEPT.WHITE,
                fontSize: '12px',
                fontWeight: 700,
                letterSpacing: '0.5px',
              }}
            >
              TRADING SETTINGS
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              background: 'transparent',
              border: 'none',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              padding: '4px',
              display: 'flex',
              alignItems: 'center',
            }}
            onMouseEnter={(e) => (e.currentTarget.style.color = FINCEPT.WHITE)}
            onMouseLeave={(e) => (e.currentTarget.style.color = FINCEPT.GRAY)}
          >
            <X size={16} />
          </button>
        </div>

        {/* Content */}
        <div style={{ padding: '16px' }}>
          {/* Paper Trading Settings */}
          {tradingMode === 'paper' && (
            <>
              <div
                style={{
                  marginBottom: '20px',
                  padding: '12px',
                  backgroundColor: `${FINCEPT.ORANGE}10`,
                  border: `1px solid ${FINCEPT.ORANGE}30`,
                }}
              >
                <div
                  style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: FINCEPT.ORANGE,
                    marginBottom: '8px',
                    letterSpacing: '0.5px',
                  }}
                >
                  PAPER TRADING MODE
                </div>
                <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: 1.5 }}>
                  Configure simulation settings. These only apply to paper trading and do not affect live orders.
                </div>
              </div>

              {/* Slippage Setting */}
              <div style={{ marginBottom: '16px' }}>
                <label
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: FINCEPT.GRAY,
                    marginBottom: '8px',
                    letterSpacing: '0.5px',
                  }}
                >
                  <Percent size={12} color={FINCEPT.CYAN} />
                  SLIPPAGE SIMULATION (%)
                </label>
                <input
                  type="number"
                  value={slippage}
                  onChange={handleSlippageChange}
                  min="0"
                  max="10"
                  step="0.01"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '12px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                />
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                  Simulates price slippage on market orders (0-10%)
                </div>
              </div>

              {/* Maker Fee */}
              <div style={{ marginBottom: '16px' }}>
                <label
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: FINCEPT.GRAY,
                    marginBottom: '8px',
                    letterSpacing: '0.5px',
                  }}
                >
                  <DollarSign size={12} color={FINCEPT.GREEN} />
                  MAKER FEE (%)
                </label>
                <input
                  type="number"
                  value={(makerFee * 100).toFixed(4)}
                  onChange={(e) => {
                    const value = parseFloat(e.target.value) / 100;
                    if (!isNaN(value) && value >= 0 && value <= 1) {
                      onMakerFeeChange(value);
                    }
                  }}
                  min="0"
                  max="100"
                  step="0.001"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '12px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                />
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                  Fee for limit orders that add liquidity
                </div>
              </div>

              {/* Taker Fee */}
              <div style={{ marginBottom: '20px' }}>
                <label
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: FINCEPT.GRAY,
                    marginBottom: '8px',
                    letterSpacing: '0.5px',
                  }}
                >
                  <DollarSign size={12} color={FINCEPT.RED} />
                  TAKER FEE (%)
                </label>
                <input
                  type="number"
                  value={(takerFee * 100).toFixed(4)}
                  onChange={(e) => {
                    const value = parseFloat(e.target.value) / 100;
                    if (!isNaN(value) && value >= 0 && value <= 1) {
                      onTakerFeeChange(value);
                    }
                  }}
                  min="0"
                  max="100"
                  step="0.001"
                  style={{
                    width: '100%',
                    padding: '10px 12px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '12px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                />
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                  Fee for market orders that take liquidity
                </div>
              </div>

              {/* Divider */}
              <div
                style={{
                  height: '1px',
                  backgroundColor: FINCEPT.BORDER,
                  margin: '20px 0',
                }}
              />

              {/* Reset Portfolio */}
              <div>
                <div
                  style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: FINCEPT.RED,
                    marginBottom: '12px',
                    letterSpacing: '0.5px',
                  }}
                >
                  DANGER ZONE
                </div>
                <button
                  onClick={handleResetPortfolio}
                  style={{
                    width: '100%',
                    padding: '12px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.RED}`,
                    color: FINCEPT.RED,
                    fontSize: '10px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                    transition: 'all 0.15s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.RED;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = FINCEPT.RED;
                  }}
                >
                  <RotateCcw size={12} />
                  RESET PAPER PORTFOLIO
                </button>
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '8px', textAlign: 'center' }}>
                  Clears all positions, orders, and resets balance to initial amount
                </div>
              </div>
            </>
          )}

          {/* Live Trading Mode Info */}
          {tradingMode === 'live' && (
            <div
              style={{
                padding: '20px',
                backgroundColor: `${FINCEPT.RED}10`,
                border: `1px solid ${FINCEPT.RED}30`,
                textAlign: 'center',
              }}
            >
              <div
                style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: FINCEPT.RED,
                  marginBottom: '8px',
                }}
              >
                LIVE TRADING MODE
              </div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: 1.5 }}>
                Settings are managed by your exchange. Fees and slippage are determined by market conditions.
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div
          style={{
            padding: '12px 16px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: FINCEPT.HEADER_BG,
            display: 'flex',
            justifyContent: 'flex-end',
          }}
        >
          <button
            onClick={onClose}
            style={{
              padding: '8px 20px',
              backgroundColor: FINCEPT.ORANGE,
              border: 'none',
              color: FINCEPT.DARK_BG,
              fontSize: '10px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.15s',
            }}
            onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = '#FFA030')}
            onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = FINCEPT.ORANGE)}
          >
            CLOSE
          </button>
        </div>
      </div>
    </div>
  );
}

export default CryptoSettingsPanel;
