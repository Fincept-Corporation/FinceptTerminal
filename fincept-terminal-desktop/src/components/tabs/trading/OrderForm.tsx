// File: src/components/tabs/trading/OrderForm.tsx
// Fincept Terminal-styled order placement form

import React, { useState } from 'react';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import type { OrderRequest, OrderType, OrderSide } from '../../../types/trading';

interface OrderFormProps {
  symbol: string;
  currentPrice?: number;
  onPlaceOrder: (order: OrderRequest) => Promise<void>;
  isLoading?: boolean;
}

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

export function OrderForm({ symbol, currentPrice, onPlaceOrder, isLoading }: OrderFormProps) {
  const { activeBrokerMetadata, tradingMode } = useBrokerContext();

  const [side, setSide] = useState<OrderSide>('buy');
  const [orderType, setOrderType] = useState<OrderType>('market');
  const [quantity, setQuantity] = useState<string>('');
  const [price, setPrice] = useState<string>('');
  const [stopPrice, setStopPrice] = useState<string>('');
  const [takeProfitPrice, setTakeProfitPrice] = useState<string>('');
  const [stopLossPrice, setStopLossPrice] = useState<string>('');
  const [enableAdvanced, setEnableAdvanced] = useState(false);
  const [error, setError] = useState<string>('');

  // Get supported order types from broker
  const supportsMarket = activeBrokerMetadata?.tradingFeatures.marketOrders ?? true;
  const supportsLimit = activeBrokerMetadata?.tradingFeatures.limitOrders ?? true;
  const supportsStop = activeBrokerMetadata?.tradingFeatures.stopOrders ?? false;
  const supportsStopLimit = activeBrokerMetadata?.tradingFeatures.stopLimitOrders ?? false;

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');

    // Validation
    if (!quantity || parseFloat(quantity) <= 0) {
      setError('Please enter a valid quantity');
      return;
    }

    if ((orderType === 'limit' || orderType === 'stop_limit') && (!price || parseFloat(price) <= 0)) {
      setError('Please enter a valid limit price');
      return;
    }

    if ((orderType === 'stop_market' || orderType === 'stop_limit') && (!stopPrice || parseFloat(stopPrice) <= 0)) {
      setError('Please enter a valid stop price');
      return;
    }

    const order: OrderRequest = {
      symbol,
      side,
      type: orderType,
      quantity: parseFloat(quantity),
      ...(price && { price: parseFloat(price) }),
      ...(stopPrice && { stopPrice: parseFloat(stopPrice) })
    };

    try {
      await onPlaceOrder(order);

      // Reset form on success
      setQuantity('');
      setPrice('');
      setStopPrice('');
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to place order');
    }
  };

  const estimatedCost = currentPrice && quantity
    ? (parseFloat(quantity) * (parseFloat(price) || currentPrice)).toFixed(2)
    : '0.00';

  return (
    <div style={{
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      fontSize: '11px'
    }}>
      <form onSubmit={handleSubmit}>
        {/* Buy/Sell Toggle */}
        <div style={{ display: 'flex', gap: '6px', marginBottom: '12px' }}>
          <button
            type="button"
            onClick={() => setSide('buy')}
            style={{
              flex: 1,
              padding: '8px',
              backgroundColor: side === 'buy' ? FINCEPT.GREEN : FINCEPT.HEADER_BG,
              border: `1px solid ${side === 'buy' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
              color: side === 'buy' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              cursor: 'pointer',
              fontWeight: 700,
              fontSize: '10px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (side !== 'buy') {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              }
            }}
            onMouseLeave={(e) => {
              if (side !== 'buy') {
                e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
              }
            }}
          >
            BUY
          </button>
          <button
            type="button"
            onClick={() => setSide('sell')}
            style={{
              flex: 1,
              padding: '8px',
              backgroundColor: side === 'sell' ? FINCEPT.RED : FINCEPT.HEADER_BG,
              border: `1px solid ${side === 'sell' ? FINCEPT.RED : FINCEPT.BORDER}`,
              color: side === 'sell' ? FINCEPT.WHITE : FINCEPT.GRAY,
              cursor: 'pointer',
              fontWeight: 700,
              fontSize: '10px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (side !== 'sell') {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              }
            }}
            onMouseLeave={(e) => {
              if (side !== 'sell') {
                e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
              }
            }}
          >
            SELL
          </button>
        </div>

        {/* Order Type */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{
            display: 'block',
            color: FINCEPT.GRAY,
            fontSize: '9px',
            marginBottom: '4px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            ORDER TYPE
          </label>
          <select
            value={orderType}
            onChange={(e) => setOrderType(e.target.value as OrderType)}
            style={{
              width: '100%',
              padding: '6px 8px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 600,
              outline: 'none',
              cursor: 'pointer'
            }}
          >
            {supportsMarket && <option value="market">MARKET</option>}
            {supportsLimit && <option value="limit">LIMIT</option>}
            {supportsStop && <option value="stop_market">STOP MARKET</option>}
            {supportsStopLimit && <option value="stop_limit">STOP LIMIT</option>}
          </select>
        </div>

        {/* Quantity */}
        <div style={{ marginBottom: '12px' }}>
          <label style={{
            display: 'block',
            color: FINCEPT.GRAY,
            fontSize: '9px',
            marginBottom: '4px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            QUANTITY
          </label>
          <input
            type="text"
            inputMode="decimal"
            value={quantity}
            onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setQuantity(v); }}
            placeholder="0.0000"
            style={{
              width: '100%',
              padding: '6px 8px',
              backgroundColor: FINCEPT.HEADER_BG,
              color: FINCEPT.YELLOW,
              border: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '11px',
              fontWeight: 600,
              outline: 'none',
              fontFamily: 'monospace'
            }}
            onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
            onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
          />
        </div>

        {/* Limit Price (for limit and stop_limit) */}
        {(orderType === 'limit' || orderType === 'stop_limit') && (
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: FINCEPT.GRAY,
              fontSize: '9px',
              marginBottom: '4px',
              fontWeight: 600,
              letterSpacing: '0.5px'
            }}>
              LIMIT PRICE
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={price}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setPrice(v); }}
              placeholder={currentPrice?.toFixed(2) || '0.00'}
              style={{
                width: '100%',
                padding: '6px 8px',
                backgroundColor: FINCEPT.HEADER_BG,
                color: FINCEPT.CYAN,
                border: `1px solid ${FINCEPT.BORDER}`,
                fontSize: '11px',
                fontWeight: 600,
                outline: 'none',
                fontFamily: 'monospace'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
            />
          </div>
        )}

        {/* Stop Price (for stop_market and stop_limit) */}
        {(orderType === 'stop_market' || orderType === 'stop_limit') && (
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: FINCEPT.GRAY,
              fontSize: '9px',
              marginBottom: '4px',
              fontWeight: 600,
              letterSpacing: '0.5px'
            }}>
              STOP PRICE
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={stopPrice}
              onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setStopPrice(v); }}
              placeholder={currentPrice?.toFixed(2) || '0.00'}
              style={{
                width: '100%',
                padding: '6px 8px',
                backgroundColor: FINCEPT.HEADER_BG,
                color: FINCEPT.CYAN,
                border: `1px solid ${FINCEPT.BORDER}`,
                fontSize: '11px',
                fontWeight: 600,
                outline: 'none',
                fontFamily: 'monospace'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
            />
          </div>
        )}

        {/* Advanced Settings Toggle */}
        <button
          type="button"
          onClick={() => setEnableAdvanced(!enableAdvanced)}
          style={{
            marginBottom: '12px',
            color: FINCEPT.ORANGE,
            backgroundColor: 'transparent',
            border: 'none',
            cursor: 'pointer',
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px',
            transition: 'all 0.2s'
          }}
          onMouseEnter={(e) => e.currentTarget.style.color = FINCEPT.YELLOW}
          onMouseLeave={(e) => e.currentTarget.style.color = FINCEPT.ORANGE}
        >
          {enableAdvanced ? '▼' : '▶'} ADVANCED (SL/TP)
        </button>

        {/* Advanced Settings: Stop Loss & Take Profit */}
        {enableAdvanced && (
          <div style={{
            marginBottom: '12px',
            padding: '10px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`
          }}>
            <div style={{ marginBottom: '10px' }}>
              <label style={{
                display: 'block',
                color: FINCEPT.GREEN,
                fontSize: '9px',
                marginBottom: '4px',
                fontWeight: 600,
                letterSpacing: '0.5px'
              }}>
                TAKE PROFIT
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={takeProfitPrice}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setTakeProfitPrice(v); }}
                placeholder="Optional"
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  color: FINCEPT.GREEN,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontWeight: 600,
                  outline: 'none',
                  fontFamily: 'monospace'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.GREEN}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            <div>
              <label style={{
                display: 'block',
                color: FINCEPT.RED,
                fontSize: '9px',
                marginBottom: '4px',
                fontWeight: 600,
                letterSpacing: '0.5px'
              }}>
                STOP LOSS
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={stopLossPrice}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setStopLossPrice(v); }}
                placeholder="Optional"
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  color: FINCEPT.RED,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  fontSize: '10px',
                  fontWeight: 600,
                  outline: 'none',
                  fontFamily: 'monospace'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.RED}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {currentPrice && (takeProfitPrice || stopLossPrice) && (
              <div style={{
                marginTop: '8px',
                paddingTop: '8px',
                borderTop: `1px solid ${FINCEPT.BORDER}`,
                fontSize: '9px'
              }}>
                {takeProfitPrice && (
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    color: FINCEPT.GREEN,
                    marginBottom: '4px'
                  }}>
                    <span>TP R/R:</span>
                    <span style={{ fontWeight: 700 }}>
                      {((Math.abs(parseFloat(takeProfitPrice) - currentPrice) / currentPrice) * 100).toFixed(2)}%
                    </span>
                  </div>
                )}
                {stopLossPrice && (
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    color: FINCEPT.RED
                  }}>
                    <span>SL RISK:</span>
                    <span style={{ fontWeight: 700 }}>
                      {((Math.abs(currentPrice - parseFloat(stopLossPrice)) / currentPrice) * 100).toFixed(2)}%
                    </span>
                  </div>
                )}
              </div>
            )}
          </div>
        )}

        {/* Estimated Cost */}
        {currentPrice && (
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            marginBottom: '12px',
            padding: '8px',
            backgroundColor: FINCEPT.HEADER_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '9px'
          }}>
            <span style={{ color: FINCEPT.GRAY, fontWeight: 600 }}>
              EST. {side === 'buy' ? 'COST' : 'PROCEEDS'}:
            </span>
            <span style={{
              color: FINCEPT.YELLOW,
              fontWeight: 700,
              fontFamily: 'monospace',
              fontSize: '10px'
            }}>
              ${estimatedCost}
            </span>
          </div>
        )}

        {/* Error Message */}
        {error && (
          <div style={{
            marginBottom: '12px',
            padding: '8px',
            backgroundColor: `${FINCEPT.RED}20`,
            border: `1px solid ${FINCEPT.RED}`,
            color: FINCEPT.RED,
            fontSize: '9px',
            fontWeight: 600
          }}>
            [WARN] {error}
          </div>
        )}

        {/* Trading Mode Warning */}
        {tradingMode === 'live' && (
          <div style={{
            marginBottom: '12px',
            padding: '8px',
            backgroundColor: `${FINCEPT.RED}20`,
            border: `2px solid ${FINCEPT.RED}`,
            color: FINCEPT.RED,
            fontSize: '9px',
            fontWeight: 700,
            textAlign: 'center',
            letterSpacing: '0.5px'
          }}>
            [WARN]️ LIVE TRADING MODE - REAL MONEY!
          </div>
        )}

        {/* Submit Button */}
        <button
          type="submit"
          disabled={isLoading}
          style={{
            width: '100%',
            padding: '10px',
            backgroundColor: tradingMode === 'live'
              ? FINCEPT.RED
              : side === 'buy'
              ? FINCEPT.GREEN
              : FINCEPT.RED,
            border: tradingMode === 'live'
              ? `2px solid ${FINCEPT.YELLOW}`
              : `1px solid ${side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED}`,
            color: tradingMode === 'live' || side === 'sell' ? FINCEPT.WHITE : FINCEPT.DARK_BG,
            cursor: isLoading ? 'not-allowed' : 'pointer',
            fontWeight: 700,
            fontSize: '11px',
            letterSpacing: '1px',
            transition: 'all 0.2s',
            opacity: isLoading ? 0.6 : 1,
            boxShadow: tradingMode === 'live' ? `0 0 10px ${FINCEPT.RED}40` : 'none'
          }}
          onMouseEnter={(e) => {
            if (!isLoading) {
              if (tradingMode === 'live') {
                e.currentTarget.style.backgroundColor = '#FF1111';
              } else if (side === 'buy') {
                e.currentTarget.style.backgroundColor = '#00FF88';
              } else {
                e.currentTarget.style.backgroundColor = '#FF5555';
              }
            }
          }}
          onMouseLeave={(e) => {
            if (!isLoading) {
              if (tradingMode === 'live') {
                e.currentTarget.style.backgroundColor = FINCEPT.RED;
              } else if (side === 'buy') {
                e.currentTarget.style.backgroundColor = FINCEPT.GREEN;
              } else {
                e.currentTarget.style.backgroundColor = FINCEPT.RED;
              }
            }
          }}
        >
          {isLoading
            ? 'PLACING ORDER...'
            : tradingMode === 'live'
            ? `🔴 LIVE ${side.toUpperCase()} ${symbol}`
            : `${side.toUpperCase()} ${symbol}`}
        </button>

        {/* Paper Trading Info */}
        {tradingMode === 'paper' && (
          <div style={{
            marginTop: '8px',
            textAlign: 'center',
            color: FINCEPT.GRAY,
            fontSize: '8px',
            fontStyle: 'italic'
          }}>
            Paper trading - No real money used
          </div>
        )}
      </form>
    </div>
  );
}
