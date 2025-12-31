/**
 * Basic Order Form - Rewritten for performance
 * No useEffect hooks, direct state management
 */

import React, { useState, useCallback } from 'react';
import type { OrderType, OrderSide } from '../../../../../brokers/crypto/types';
import type { UnifiedOrderRequest } from '../../types';
import { formatPrice, formatCurrency } from '../../utils/formatters';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  YELLOW: '#FFD700',
};

interface BasicOrderFormProps {
  symbol: string;
  currentPrice: number;
  orderType: OrderType;
  side: OrderSide;
  onOrderChange: (order: Partial<UnifiedOrderRequest>) => void;
  balance?: number;
}

export function BasicOrderForm({
  symbol,
  currentPrice,
  orderType,
  side,
  onOrderChange,
  balance = 0,
}: BasicOrderFormProps) {
  const [quantity, setQuantity] = useState('');
  const [price, setPrice] = useState(currentPrice.toFixed(2));

  // Calculate total
  const total = (parseFloat(quantity) || 0) * (parseFloat(price) || currentPrice);
  const maxQuantity = balance > 0 && currentPrice > 0 ? balance / currentPrice : 0;

  // Update parent
  const notifyParent = useCallback((qty: number, prc: number) => {
    onOrderChange({
      symbol,
      type: orderType,
      side,
      quantity: qty,
      price: orderType === 'limit' && prc > 0 ? prc : undefined,
    });
  }, [symbol, orderType, side, onOrderChange]);

  // Handle quantity input
  const handleQuantityChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const value = e.target.value;
    if (value === '' || /^\d*\.?\d*$/.test(value)) {
      setQuantity(value);
      const qty = parseFloat(value) || 0;
      const prc = parseFloat(price) || currentPrice;
      notifyParent(qty, prc);
    }
  }, [price, currentPrice, notifyParent]);

  // Handle price input
  const handlePriceChange = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const value = e.target.value;
    if (value === '' || /^\d*\.?\d*$/.test(value)) {
      setPrice(value);
      const qty = parseFloat(quantity) || 0;
      const prc = parseFloat(value) || 0;
      notifyParent(qty, prc);
    }
  }, [quantity, notifyParent]);

  // Handle percentage buttons
  const handlePercentage = useCallback((percent: number) => {
    if (balance === 0 || currentPrice === 0) return;
    const prc = parseFloat(price) || currentPrice;
    const qty = (balance * percent) / prc;
    const qtyStr = qty.toFixed(4);
    setQuantity(qtyStr);
    notifyParent(qty, prc);
  }, [balance, currentPrice, price, notifyParent]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Price Input (limit orders only) */}
      {orderType === 'limit' && (
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
            PRICE
          </label>
          <input
            type="text"
            value={price}
            onChange={handlePriceChange}
            placeholder="0.00"
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              fontSize: '12px',
              fontFamily: 'inherit',
              outline: 'none',
            }}
            onFocus={(e) => (e.target.style.borderColor = BLOOMBERG.ORANGE)}
            onBlur={(e) => (e.target.style.borderColor = BLOOMBERG.BORDER)}
          />
          <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginTop: '4px' }}>
            Market: {formatCurrency(currentPrice)}
          </div>
        </div>
      )}

      {orderType === 'market' && (
        <div
          style={{
            padding: '8px',
            backgroundColor: `${BLOOMBERG.ORANGE}10`,
            border: `1px solid ${BLOOMBERG.ORANGE}40`,
            fontSize: '10px',
            color: BLOOMBERG.GRAY,
          }}
        >
          Market Price: <span style={{ color: BLOOMBERG.ORANGE, fontWeight: 700 }}>{formatCurrency(currentPrice)}</span>
        </div>
      )}

      {/* Quantity Input */}
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
          QUANTITY
        </label>
        <input
          type="text"
          value={quantity}
          onChange={handleQuantityChange}
          placeholder="0.0000"
          style={{
            width: '100%',
            padding: '8px 10px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            color: BLOOMBERG.WHITE,
            fontSize: '12px',
            fontFamily: 'inherit',
            outline: 'none',
          }}
          onFocus={(e) => (e.target.style.borderColor = BLOOMBERG.ORANGE)}
          onBlur={(e) => (e.target.style.borderColor = BLOOMBERG.BORDER)}
        />
        <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginTop: '4px' }}>
          Max: {maxQuantity.toFixed(4)} {symbol.split('/')[0]}
        </div>
      </div>

      {/* Percentage Buttons */}
      <div style={{ display: 'flex', gap: '4px' }}>
        {[0.25, 0.5, 0.75, 1.0].map((percent) => (
          <button
            key={percent}
            onClick={() => handlePercentage(percent)}
            style={{
              flex: 1,
              padding: '6px',
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
              e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
              e.currentTarget.style.color = BLOOMBERG.ORANGE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = BLOOMBERG.PANEL_BG;
              e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
              e.currentTarget.style.color = BLOOMBERG.GRAY;
            }}
          >
            {percent * 100}%
          </button>
        ))}
      </div>

      {/* Total */}
      <div
        style={{
          padding: '10px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}
      >
        <span style={{ fontSize: '9px', color: BLOOMBERG.GRAY, fontWeight: 700 }}>TOTAL</span>
        <span style={{ fontSize: '13px', color: BLOOMBERG.YELLOW, fontWeight: 700 }}>
          {formatCurrency(total)}
        </span>
      </div>

      {/* Balance Warning */}
      {side === 'buy' && total > balance && (
        <div
          style={{
            padding: '8px',
            backgroundColor: `${BLOOMBERG.RED}10`,
            border: `1px solid ${BLOOMBERG.RED}40`,
            fontSize: '10px',
            color: BLOOMBERG.RED,
          }}
        >
          [WARN] Insufficient balance (Available: {formatCurrency(balance)})
        </div>
      )}
    </div>
  );
}
