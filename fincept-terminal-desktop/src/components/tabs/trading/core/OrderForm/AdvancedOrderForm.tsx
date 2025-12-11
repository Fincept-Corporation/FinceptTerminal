/**
 * Advanced Order Form
 * Stop-loss, take-profit, trailing stop (capability-based)
 */

import React, { useState } from 'react';
import type { UnifiedOrderRequest } from '../../types';
import { formatCurrency } from '../../utils/formatters';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  CYAN: '#00E5FF',
};

interface AdvancedOrderFormProps {
  currentPrice: number;
  supportsStopLoss: boolean;
  supportsTakeProfit: boolean;
  supportsTrailing: boolean;
  onOrderChange: (order: Partial<UnifiedOrderRequest>) => void;
}

export function AdvancedOrderForm({
  currentPrice,
  supportsStopLoss,
  supportsTakeProfit,
  supportsTrailing,
  onOrderChange,
}: AdvancedOrderFormProps) {
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [stopLossPrice, setStopLossPrice] = useState<string>('');
  const [takeProfitPrice, setTakeProfitPrice] = useState<string>('');
  const [trailingPercent, setTrailingPercent] = useState<string>('');

  // Show advanced panel only if at least one feature is supported
  const hasAdvancedFeatures = supportsStopLoss || supportsTakeProfit || supportsTrailing;

  if (!hasAdvancedFeatures) {
    return null;
  }

  // Handle input changes
  const handleStopLossChange = (value: string) => {
    if (/^\d*\.?\d*$/.test(value) || value === '') {
      setStopLossPrice(value);
      onOrderChange({ stopLossPrice: value ? parseFloat(value) : undefined });
    }
  };

  const handleTakeProfitChange = (value: string) => {
    if (/^\d*\.?\d*$/.test(value) || value === '') {
      setTakeProfitPrice(value);
      onOrderChange({ takeProfitPrice: value ? parseFloat(value) : undefined });
    }
  };

  const handleTrailingChange = (value: string) => {
    if (/^\d*\.?\d*$/.test(value) || value === '') {
      setTrailingPercent(value);
      onOrderChange({ trailingPercent: value ? parseFloat(value) : undefined });
    }
  };

  return (
    <div style={{ marginTop: '8px' }}>
      {/* Toggle Button */}
      <button
        onClick={() => setShowAdvanced(!showAdvanced)}
        style={{
          width: '100%',
          padding: '8px',
          backgroundColor: showAdvanced ? `${BLOOMBERG.CYAN}15` : BLOOMBERG.PANEL_BG,
          border: `1px solid ${showAdvanced ? BLOOMBERG.CYAN : BLOOMBERG.BORDER}`,
          color: showAdvanced ? BLOOMBERG.CYAN : BLOOMBERG.GRAY,
          fontSize: '10px',
          fontWeight: 700,
          cursor: 'pointer',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          transition: 'all 0.2s',
        }}
        onMouseEnter={(e) => {
          if (!showAdvanced) {
            e.currentTarget.style.borderColor = BLOOMBERG.CYAN;
            e.currentTarget.style.color = BLOOMBERG.CYAN;
          }
        }}
        onMouseLeave={(e) => {
          if (!showAdvanced) {
            e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
            e.currentTarget.style.color = BLOOMBERG.GRAY;
          }
        }}
      >
        <span>⚡ ADVANCED OPTIONS</span>
        <span>{showAdvanced ? '▼' : '▶'}</span>
      </button>

      {/* Advanced Fields */}
      {showAdvanced && (
        <div
          style={{
            marginTop: '8px',
            padding: '12px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px',
          }}
        >
          {/* Stop Loss */}
          {supportsStopLoss && (
            <div>
              <label
                style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  marginBottom: '4px',
                  letterSpacing: '0.5px',
                }}
              >
                STOP LOSS
              </label>
              <input
                type="text"
                value={stopLossPrice}
                onChange={(e) => handleStopLossChange(e.target.value)}
                placeholder="Optional"
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  fontSize: '11px',
                  fontFamily: 'inherit',
                  outline: 'none',
                }}
                onFocus={(e) => (e.target.style.borderColor = BLOOMBERG.RED)}
                onBlur={(e) => (e.target.style.borderColor = BLOOMBERG.BORDER)}
              />
              {stopLossPrice && (
                <div style={{ fontSize: '8px', color: BLOOMBERG.RED, marginTop: '4px' }}>
                  {parseFloat(stopLossPrice) < currentPrice ? '↓' : '↑'}{' '}
                  {Math.abs(((parseFloat(stopLossPrice) - currentPrice) / currentPrice) * 100).toFixed(2)}% from current
                </div>
              )}
            </div>
          )}

          {/* Take Profit */}
          {supportsTakeProfit && (
            <div>
              <label
                style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  marginBottom: '4px',
                  letterSpacing: '0.5px',
                }}
              >
                TAKE PROFIT
              </label>
              <input
                type="text"
                value={takeProfitPrice}
                onChange={(e) => handleTakeProfitChange(e.target.value)}
                placeholder="Optional"
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  fontSize: '11px',
                  fontFamily: 'inherit',
                  outline: 'none',
                }}
                onFocus={(e) => (e.target.style.borderColor = BLOOMBERG.GREEN)}
                onBlur={(e) => (e.target.style.borderColor = BLOOMBERG.BORDER)}
              />
              {takeProfitPrice && (
                <div style={{ fontSize: '8px', color: BLOOMBERG.GREEN, marginTop: '4px' }}>
                  {parseFloat(takeProfitPrice) > currentPrice ? '↑' : '↓'}{' '}
                  {Math.abs(((parseFloat(takeProfitPrice) - currentPrice) / currentPrice) * 100).toFixed(2)}% from current
                </div>
              )}
            </div>
          )}

          {/* Trailing Stop */}
          {supportsTrailing && (
            <div>
              <label
                style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  marginBottom: '4px',
                  letterSpacing: '0.5px',
                }}
              >
                TRAILING STOP (%)
              </label>
              <input
                type="text"
                value={trailingPercent}
                onChange={(e) => handleTrailingChange(e.target.value)}
                placeholder="e.g. 2.5"
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  fontSize: '11px',
                  fontFamily: 'inherit',
                  outline: 'none',
                }}
                onFocus={(e) => (e.target.style.borderColor = BLOOMBERG.ORANGE)}
                onBlur={(e) => (e.target.style.borderColor = BLOOMBERG.BORDER)}
              />
              {trailingPercent && (
                <div style={{ fontSize: '8px', color: BLOOMBERG.ORANGE, marginTop: '4px' }}>
                  Trailing {trailingPercent}% ({formatCurrency((currentPrice * parseFloat(trailingPercent)) / 100)})
                </div>
              )}
            </div>
          )}

          {/* Info Message */}
          <div
            style={{
              padding: '8px',
              backgroundColor: `${BLOOMBERG.CYAN}10`,
              border: `1px solid ${BLOOMBERG.CYAN}40`,
              fontSize: '9px',
              color: BLOOMBERG.GRAY,
              lineHeight: '1.4',
            }}
          >
            💡 Advanced orders help manage risk automatically. Stop-loss limits losses, take-profit locks in gains, and
            trailing stop follows price movements.
          </div>
        </div>
      )}
    </div>
  );
}
