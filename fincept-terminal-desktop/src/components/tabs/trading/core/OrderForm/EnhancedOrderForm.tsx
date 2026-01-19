/**
 * Enhanced Order Form
 * Main orchestrator - capability-aware, exchange-agnostic
 */

import React, { useState, useCallback, useMemo } from 'react';
import { BasicOrderForm } from './BasicOrderForm';
import { AdvancedOrderForm } from './AdvancedOrderForm';
import { useExchangeCapabilities } from '../../hooks/useExchangeCapabilities';
import { useOrderExecution } from '../../hooks/useOrderExecution';
import { usePositions } from '../../hooks/usePositions';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';
import { HyperLiquidLeverageControl } from '../../exchange-specific/hyperliquid';
import type { UnifiedOrderRequest } from '../../types';
import type { OrderType, OrderSide } from '../../../../../brokers/crypto/types';
import { validateOrder, formatValidationErrors } from '../../utils/orderValidation';

const FINCEPT = {
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
};

interface EnhancedOrderFormProps {
  symbol: string;
  currentPrice: number;
  balance: number;
  onOrderPlaced?: () => void;
}

export function EnhancedOrderForm({ symbol, currentPrice, balance, onOrderPlaced }: EnhancedOrderFormProps) {
  const capabilities = useExchangeCapabilities();
  const { placeOrder, isPlacing, error } = useOrderExecution();
  const { positions, refresh: refreshPositions } = usePositions(symbol, true, 3000); // Auto-refresh every 3 seconds
  const { activeBroker } = useBrokerContext();

  // Order state
  const [orderType, setOrderType] = useState<OrderType>('limit');
  const [side, setSide] = useState<OrderSide>('buy');
  const [orderData, setOrderData] = useState<Partial<UnifiedOrderRequest>>({});
  const [validationError, setValidationError] = useState<string>('');

  // Calculate position quantity for the current symbol (for sell orders)
  const positionQuantity = useMemo(() => {
    const position = positions.find(p => p.symbol === symbol);
    return position?.quantity || 0;
  }, [positions, symbol]);

  // Merge order data changes
  const handleOrderChange = useCallback((updates: Partial<UnifiedOrderRequest>) => {
    setOrderData((prev) => ({ ...prev, ...updates }));
    setValidationError(''); // Clear validation errors on change
  }, []);

  // Build complete order
  const completeOrder: UnifiedOrderRequest = useMemo(
    () => ({
      symbol,
      type: orderType,
      side,
      quantity: orderData.quantity || 0,
      // For market orders, use currentPrice as the execution price
      // For limit orders, use the user-specified price
      price: orderType === 'market' ? currentPrice : orderData.price,
      stopLossPrice: orderData.stopLossPrice,
      takeProfitPrice: orderData.takeProfitPrice,
      trailingPercent: orderData.trailingPercent,
    }),
    [symbol, orderType, side, orderData, currentPrice]
  );

  // Handle order submission
  const handleSubmit = async () => {
    // Validate order
    const validation = validateOrder(completeOrder, capabilities, balance, currentPrice);

    if (!validation.valid) {
      setValidationError(formatValidationErrors(validation.errors));
      return;
    }

    // Confirm before placing order
    const confirmMsg = `Place ${orderType.toUpperCase()} ${side.toUpperCase()} order?\n\n` +
      `Symbol: ${symbol}\n` +
      `Quantity: ${orderData.quantity}\n` +
      (orderType === 'limit' ? `Price: ${orderData.price}\n` : '') +
      `Total: ${((orderData.quantity || 0) * (orderData.price || currentPrice)).toFixed(2)} USD`;

    const confirmed = confirm(confirmMsg);
    if (!confirmed) {
      // User cancelled - properly handle cancellation without calling placeOrder
      console.log('[EnhancedOrderForm] Order cancelled by user');
      return;
    }

    try {
      await placeOrder(completeOrder);

      // Reset form on success
      setOrderData({});
      setValidationError('');

      // Refresh positions after order is placed
      setTimeout(() => {
        refreshPositions();
      }, 500); // Small delay to let the order settle

      // Notify parent
      if (onOrderPlaced) {
        onOrderPlaced();
      }

      alert(`[OK] Order placed: ${side.toUpperCase()} ${orderData.quantity} ${symbol.split('/')[0]}`);
    } catch (err) {
      const error = err as Error;
      setValidationError(error.message);
    }
  };

  // Calculate if order is valid
  const canSubmit =
    orderData.quantity &&
    orderData.quantity > 0 &&
    (orderType !== 'limit' || (orderData.price && orderData.price > 0)) &&
    !isPlacing;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Order Type Selector */}
      <div>
        <label
          style={{
            display: 'block',
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            marginBottom: '6px',
            letterSpacing: '0.5px',
          }}
        >
          ORDER TYPE
        </label>
        <div style={{ display: 'flex', gap: '4px' }}>
          {capabilities.supportedOrderTypes.map((type) => (
            <button
              key={type}
              onClick={() => setOrderType(type)}
              style={{
                flex: 1,
                padding: '8px 6px',
                backgroundColor: orderType === type ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                border: `1px solid ${orderType === type ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: orderType === type ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                textTransform: 'uppercase',
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => {
                if (orderType !== type) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.color = FINCEPT.ORANGE;
                }
              }}
              onMouseLeave={(e) => {
                if (orderType !== type) {
                  e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              {type}
            </button>
          ))}
        </div>
      </div>

      {/* Side Selector */}
      <div style={{ display: 'flex', gap: '4px' }}>
        <button
          onClick={() => setSide('buy')}
          style={{
            flex: 1,
            padding: '10px',
            backgroundColor: side === 'buy' ? FINCEPT.GREEN : FINCEPT.PANEL_BG,
            border: `2px solid ${side === 'buy' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
            color: side === 'buy' ? FINCEPT.DARK_BG : FINCEPT.GREEN,
            fontSize: '11px',
            fontWeight: 700,
            cursor: 'pointer',
            transition: 'all 0.2s',
          }}
        >
          BUY
        </button>
        <button
          onClick={() => setSide('sell')}
          style={{
            flex: 1,
            padding: '10px',
            backgroundColor: side === 'sell' ? FINCEPT.RED : FINCEPT.PANEL_BG,
            border: `2px solid ${side === 'sell' ? FINCEPT.RED : FINCEPT.BORDER}`,
            color: side === 'sell' ? FINCEPT.WHITE : FINCEPT.RED,
            fontSize: '11px',
            fontWeight: 700,
            cursor: 'pointer',
            transition: 'all 0.2s',
          }}
        >
          SELL
        </button>
      </div>

      {/* Basic Order Form */}
      <BasicOrderForm
        symbol={symbol}
        currentPrice={currentPrice}
        orderType={orderType}
        side={side}
        onOrderChange={handleOrderChange}
        balance={balance}
        positionQuantity={positionQuantity}
      />

      {/* Advanced Order Form (capability-based) */}
      <AdvancedOrderForm
        currentPrice={currentPrice}
        supportsStopLoss={capabilities.supportsStopLoss}
        supportsTakeProfit={capabilities.supportsTakeProfit}
        supportsTrailing={capabilities.supportsTrailingStop}
        onOrderChange={handleOrderChange}
      />

      {/* HyperLiquid Leverage Control (exchange-specific) */}
      {activeBroker === 'hyperliquid' && capabilities.supportsLeverage && (
        <HyperLiquidLeverageControl
          symbol={symbol}
          onLeverageChange={(lev) => handleOrderChange({ leverage: lev })}
        />
      )}

      {/* Validation Error */}
      {validationError && (
        <div
          style={{
            padding: '10px',
            backgroundColor: `${FINCEPT.RED}15`,
            border: `1px solid ${FINCEPT.RED}`,
            color: FINCEPT.RED,
            fontSize: '10px',
            whiteSpace: 'pre-line',
          }}
        >
          [WARN]️ {validationError}
        </div>
      )}

      {/* Execution Error */}
      {error && (
        <div
          style={{
            padding: '10px',
            backgroundColor: `${FINCEPT.RED}15`,
            border: `1px solid ${FINCEPT.RED}`,
            color: FINCEPT.RED,
            fontSize: '10px',
          }}
        >
          [FAIL] {error.message}
        </div>
      )}

      {/* Submit Button */}
      <button
        onClick={handleSubmit}
        disabled={!canSubmit}
        style={{
          width: '100%',
          padding: '12px',
          backgroundColor: canSubmit ? (side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED) : FINCEPT.PANEL_BG,
          border: `2px solid ${canSubmit ? (side === 'buy' ? FINCEPT.GREEN : FINCEPT.RED) : FINCEPT.BORDER}`,
          color: canSubmit ? FINCEPT.WHITE : FINCEPT.GRAY,
          fontSize: '12px',
          fontWeight: 700,
          cursor: canSubmit ? 'pointer' : 'not-allowed',
          textTransform: 'uppercase',
          letterSpacing: '1px',
          opacity: isPlacing ? 0.6 : 1,
          transition: 'all 0.2s',
        }}
      >
        {isPlacing ? '⟳ PLACING ORDER...' : `${side.toUpperCase()} ${symbol.split('/')[0]}`}
      </button>
    </div>
  );
}
