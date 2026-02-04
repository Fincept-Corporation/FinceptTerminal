/**
 * Stock Order Form Component
 *
 * Order entry form for stock trading with support for various order types
 */

import React, { useState, useEffect } from 'react';
import {
  TrendingUp, TrendingDown, Loader2, AlertCircle, DollarSign,
  Clock, Target, ShieldAlert, Calculator, Info
} from 'lucide-react';
import { useStockBrokerContext, useStockBrokerCapabilities } from '@/contexts/StockBrokerContext';
import type {
  OrderParams,
  OrderSide,
  OrderType,
  ProductType,
  OrderValidity,
  StockExchange,
  MarginRequired,
} from '@/brokers/stocks/types';

interface StockOrderFormProps {
  symbol: string;
  exchange: StockExchange;
  lastPrice?: number;
  onOrderPlaced?: (orderId: string) => void;
}

const ORDER_TYPES: { value: OrderType; label: string; description: string }[] = [
  { value: 'MARKET', label: 'Market', description: 'Execute at current market price' },
  { value: 'LIMIT', label: 'Limit', description: 'Execute at specified price or better' },
  { value: 'STOP', label: 'Stop Loss', description: 'Trigger when price reaches stop price' },
  { value: 'STOP_LIMIT', label: 'Stop Limit', description: 'Limit order triggered at stop price' },
];

const PRODUCT_TYPES: { value: ProductType; label: string; description: string }[] = [
  { value: 'CASH', label: 'Delivery', description: 'Hold position overnight (CNC)' },
  { value: 'INTRADAY', label: 'Intraday', description: 'Square off same day (MIS)' },
  { value: 'MARGIN', label: 'Margin', description: 'Leverage with margin (NRML)' },
];

const VALIDITY_OPTIONS: { value: OrderValidity; label: string }[] = [
  { value: 'DAY', label: 'Day' },
  { value: 'IOC', label: 'IOC' },
  { value: 'GTC', label: 'GTC' },
];

export function StockOrderForm({ symbol, exchange, lastPrice, onOrderPlaced }: StockOrderFormProps) {
  const { adapter, isAuthenticated, refreshOrders, refreshPositions, refreshFunds } = useStockBrokerContext();
  const { supportsFeature, supportsTradingFeature, hasAMO, hasBracketOrder } = useStockBrokerCapabilities();

  // Order state
  const [side, setSide] = useState<OrderSide>('BUY');
  const [orderType, setOrderType] = useState<OrderType>('LIMIT');
  const [productType, setProductType] = useState<ProductType>('CASH');
  const [quantity, setQuantity] = useState<string>('');
  const [price, setPrice] = useState<string>(lastPrice?.toString() || '');
  const [triggerPrice, setTriggerPrice] = useState<string>('');
  const [validity, setValidity] = useState<OrderValidity>('DAY');
  const [isAMO, setIsAMO] = useState(false);

  // Bracket order
  const [showBracket, setShowBracket] = useState(false);
  const [squareOffTarget, setSquareOffTarget] = useState<string>('');
  const [stopLoss, setStopLoss] = useState<string>('');

  // UI state
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [marginRequired, setMarginRequired] = useState<MarginRequired | null>(null);
  const [isCalculatingMargin, setIsCalculatingMargin] = useState(false);
  const [priceInitialized, setPriceInitialized] = useState(false);

  // Set initial price once when switching to a non-market order type
  useEffect(() => {
    if (orderType !== 'MARKET' && !priceInitialized && lastPrice && lastPrice > 0) {
      setPrice(lastPrice.toFixed(2));
      setPriceInitialized(true);
    }
  }, [orderType, priceInitialized, lastPrice]);

  // Reset initialized flag when switching to market order
  useEffect(() => {
    if (orderType === 'MARKET') {
      setPriceInitialized(false);
      setPrice('');
    } else {
      setPriceInitialized(false);
    }
  }, [orderType]);

  // Calculate margin when order params change
  useEffect(() => {
    const calculateMargin = async () => {
      if (!adapter || !isAuthenticated || !quantity || Number(quantity) <= 0) {
        setMarginRequired(null);
        return;
      }

      // Don't calculate margin if price is required but missing
      if (orderType !== 'MARKET' && (!price || Number(price) <= 0)) {
        setMarginRequired(null);
        return;
      }

      if (['STOP', 'STOP_LIMIT'].includes(orderType) && (!triggerPrice || Number(triggerPrice) <= 0)) {
        setMarginRequired(null);
        return;
      }

      setIsCalculatingMargin(true);
      try {
        const orderParams: OrderParams = {
          symbol,
          exchange,
          side,
          quantity: Number(quantity),
          orderType,
          productType,
          price: orderType !== 'MARKET' ? Number(price) : undefined,
          triggerPrice: ['STOP', 'STOP_LIMIT'].includes(orderType) ? Number(triggerPrice) : undefined,
        };

        const margin = await adapter.getMarginRequired(orderParams);
        setMarginRequired(margin);
      } catch (err) {
        console.error('Failed to calculate margin:', err);
      } finally {
        setIsCalculatingMargin(false);
      }
    };

    const debounce = setTimeout(calculateMargin, 500);
    return () => clearTimeout(debounce);
  }, [adapter, isAuthenticated, symbol, exchange, side, quantity, orderType, productType, price, triggerPrice]);

  // Submit order
  const handleSubmit = async () => {
    if (!adapter || !isAuthenticated) {
      setError('Please connect to your broker first');
      return;
    }

    if (!quantity || Number(quantity) <= 0) {
      setError('Please enter a valid quantity');
      return;
    }

    if (orderType !== 'MARKET' && (!price || Number(price) <= 0)) {
      setError('Please enter a valid price');
      return;
    }

    if (['STOP', 'STOP_LIMIT'].includes(orderType) && (!triggerPrice || Number(triggerPrice) <= 0)) {
      setError('Please enter a valid trigger price');
      return;
    }

    setError(null);
    setIsSubmitting(true);

    try {
      const orderParams: OrderParams = {
        symbol,
        exchange,
        side,
        quantity: Number(quantity),
        orderType,
        productType,
        validity,
        price: orderType !== 'MARKET' ? Number(price) : undefined,
        triggerPrice: ['STOP', 'STOP_LIMIT'].includes(orderType) ? Number(triggerPrice) : undefined,
        amo: isAMO,
        squareOff: showBracket && squareOffTarget ? Number(squareOffTarget) : undefined,
        stopLoss: showBracket && stopLoss ? Number(stopLoss) : undefined,
      };

      const response = await adapter.placeOrder(orderParams);

      if (response.success && response.orderId) {
        // Reset form
        setQuantity('');
        setTriggerPrice('');
        setSquareOffTarget('');
        setStopLoss('');
        if (orderType !== 'MARKET' && lastPrice) {
          setPrice(lastPrice.toFixed(2));
          setPriceInitialized(true);
        }

        // Refresh data
        await Promise.all([refreshOrders(), refreshPositions(), refreshFunds()]);

        // Callback
        if (onOrderPlaced) {
          onOrderPlaced(response.orderId);
        }
      } else {
        setError(response.message || 'Order placement failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Order placement failed');
    } finally {
      setIsSubmitting(false);
    }
  };

  // Estimated order value — don't fallback to lastPrice for limit orders when price is empty
  const effectivePrice = orderType === 'MARKET' ? (lastPrice || 0) : (Number(price) || 0);
  const estimatedValue = Number(quantity) * effectivePrice;

  // Fincept color constants
  const COLORS = {
    ORANGE: '#FF8800',
    GREEN: '#00D66F',
    RED: '#FF3B3B',
    CYAN: '#00E5FF',
    DARK_BG: '#000000',
    PANEL_BG: '#0F0F0F',
    HEADER_BG: '#1A1A1A',
    BORDER: '#2A2A2A',
    GRAY: '#787878',
    WHITE: '#FFFFFF',
  };

  return (
    <div style={{
      backgroundColor: COLORS.PANEL_BG,
      border: `1px solid ${COLORS.BORDER}`,
      overflow: 'hidden',
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace'
    }}>
      {/* Header */}
      <div style={{
        padding: '8px 12px',
        borderBottom: `1px solid ${COLORS.BORDER}`,
        backgroundColor: COLORS.HEADER_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: COLORS.WHITE, fontWeight: 600, fontSize: '12px' }}>{symbol}</span>
          <span style={{ color: COLORS.GRAY, fontSize: '10px' }}>{exchange}</span>
        </div>
        {lastPrice && (
          <span style={{ color: COLORS.ORANGE, fontFamily: 'monospace', fontSize: '12px', fontWeight: 600 }}>
            ₹{lastPrice.toFixed(2)}
          </span>
        )}
      </div>

      {/* Buy/Sell Toggle */}
      <div style={{ padding: '8px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            onClick={() => setSide('BUY')}
            style={{
              flex: 1,
              padding: '10px',
              fontWeight: 700,
              fontSize: '11px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              border: side === 'BUY' ? `2px solid ${COLORS.GREEN}` : `1px solid ${COLORS.BORDER}`,
              backgroundColor: side === 'BUY' ? COLORS.GREEN : COLORS.DARK_BG,
              color: side === 'BUY' ? COLORS.DARK_BG : COLORS.GRAY,
              transition: 'all 0.15s'
            }}
          >
            <TrendingUp size={14} />
            BUY
          </button>
          <button
            onClick={() => setSide('SELL')}
            style={{
              flex: 1,
              padding: '10px',
              fontWeight: 700,
              fontSize: '11px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              border: side === 'SELL' ? `2px solid ${COLORS.RED}` : `1px solid ${COLORS.BORDER}`,
              backgroundColor: side === 'SELL' ? COLORS.RED : COLORS.DARK_BG,
              color: side === 'SELL' ? COLORS.WHITE : COLORS.GRAY,
              transition: 'all 0.15s'
            }}
          >
            <TrendingDown size={14} />
            SELL
          </button>
        </div>
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
        {/* Order Type */}
        <div style={{ marginBottom: '10px' }}>
          <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>
            ORDER TYPE
          </label>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
            {ORDER_TYPES.map((type) => (
              <button
                key={type.value}
                onClick={() => setOrderType(type.value)}
                disabled={type.value === 'TRAILING_STOP' && !supportsTradingFeature('trailingStopOrders')}
                style={{
                  padding: '6px 8px',
                  fontSize: '10px',
                  cursor: 'pointer',
                  border: orderType === type.value ? `1px solid ${COLORS.ORANGE}` : `1px solid ${COLORS.BORDER}`,
                  backgroundColor: orderType === type.value ? `${COLORS.ORANGE}20` : COLORS.DARK_BG,
                  color: orderType === type.value ? COLORS.ORANGE : COLORS.GRAY,
                  transition: 'all 0.15s',
                  opacity: type.value === 'TRAILING_STOP' && !supportsTradingFeature('trailingStopOrders') ? 0.5 : 1
                }}
              >
                {type.label}
              </button>
            ))}
          </div>
        </div>

        {/* Product Type */}
        <div style={{ marginBottom: '10px' }}>
          <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>
            PRODUCT
          </label>
          <div style={{ display: 'flex', gap: '4px' }}>
            {PRODUCT_TYPES.map((type) => (
              <button
                key={type.value}
                onClick={() => setProductType(type.value)}
                style={{
                  flex: 1,
                  padding: '6px',
                  fontSize: '9px',
                  cursor: 'pointer',
                  border: productType === type.value ? `1px solid ${COLORS.CYAN}` : `1px solid ${COLORS.BORDER}`,
                  backgroundColor: productType === type.value ? `${COLORS.CYAN}15` : COLORS.DARK_BG,
                  color: productType === type.value ? COLORS.CYAN : COLORS.GRAY,
                  transition: 'all 0.15s'
                }}
                title={type.description}
              >
                {type.label}
              </button>
            ))}
          </div>
        </div>

        {/* Quantity */}
        <div style={{ marginBottom: '10px' }}>
          <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>
            QUANTITY
          </label>
          <input
            type="text"
            inputMode="numeric"
            value={quantity}
            onChange={(e) => {
              const val = e.target.value;
              if (val === '' || /^\d+$/.test(val)) setQuantity(val);
            }}
            placeholder="0"
            style={{
              width: '100%',
              padding: '8px 10px',
              backgroundColor: COLORS.DARK_BG,
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.WHITE,
              fontSize: '12px',
              fontFamily: 'monospace',
              outline: 'none'
            }}
            onFocus={(e) => e.target.style.borderColor = COLORS.ORANGE}
            onBlur={(e) => e.target.style.borderColor = COLORS.BORDER}
          />
        </div>

        {/* Price (for limit orders) */}
        {orderType !== 'MARKET' && (
          <div style={{ marginBottom: '10px' }}>
            <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>
              PRICE
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={price}
              onChange={(e) => {
                const val = e.target.value;
                if (val === '' || /^\d*\.?\d*$/.test(val)) setPrice(val);
              }}
              placeholder="0.00"
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '12px',
                fontFamily: 'monospace',
                outline: 'none'
              }}
              onFocus={(e) => e.target.style.borderColor = COLORS.ORANGE}
              onBlur={(e) => e.target.style.borderColor = COLORS.BORDER}
            />
          </div>
        )}

        {/* Trigger Price (for stop orders) */}
        {['STOP', 'STOP_LIMIT'].includes(orderType) && (
          <div style={{ marginBottom: '10px' }}>
            <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>
              TRIGGER PRICE
            </label>
            <input
              type="text"
              inputMode="decimal"
              value={triggerPrice}
              onChange={(e) => {
                const val = e.target.value;
                if (val === '' || /^\d*\.?\d*$/.test(val)) setTriggerPrice(val);
              }}
              placeholder="0.00"
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '12px',
                fontFamily: 'monospace',
                outline: 'none'
              }}
              onFocus={(e) => e.target.style.borderColor = COLORS.RED}
              onBlur={(e) => e.target.style.borderColor = COLORS.BORDER}
            />
          </div>
        )}

        {/* Validity & AMO */}
        <div style={{ display: 'flex', alignItems: 'flex-end', gap: '8px', marginBottom: '10px' }}>
          <div style={{ flex: 1 }}>
            <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px', fontWeight: 600, letterSpacing: '0.5px' }}>
              VALIDITY
            </label>
            <select
              value={validity}
              onChange={(e) => setValidity(e.target.value as OrderValidity)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '11px',
                outline: 'none',
                cursor: 'pointer'
              }}
            >
              {VALIDITY_OPTIONS.map((opt) => (
                <option key={opt.value} value={opt.value}>{opt.label}</option>
              ))}
            </select>
          </div>

          {/* AMO Toggle */}
          {hasAMO && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', paddingBottom: '2px' }}>
              <input
                type="checkbox"
                id="amo"
                checked={isAMO}
                onChange={(e) => setIsAMO(e.target.checked)}
                style={{ width: '14px', height: '14px', cursor: 'pointer' }}
              />
              <label htmlFor="amo" style={{ fontSize: '10px', color: COLORS.GRAY, cursor: 'pointer' }}>AMO</label>
            </div>
          )}
        </div>

        {/* Bracket Order */}
        {hasBracketOrder && productType === 'INTRADAY' && (
          <div style={{ marginBottom: '10px' }}>
            <button
              onClick={() => setShowBracket(!showBracket)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                fontSize: '10px',
                color: COLORS.ORANGE,
                background: 'none',
                border: 'none',
                cursor: 'pointer',
                padding: 0
              }}
            >
              <Target size={12} />
              {showBracket ? 'Hide' : 'Add'} Bracket Order
            </button>

            {showBracket && (
              <div style={{ marginTop: '8px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px' }}>
                    TARGET
                  </label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={squareOffTarget}
                    onChange={(e) => {
                      const val = e.target.value;
                      if (val === '' || /^\d*\.?\d*$/.test(val)) setSquareOffTarget(val);
                    }}
                    placeholder="0.00"
                    style={{
                      width: '100%',
                      padding: '6px 8px',
                      backgroundColor: COLORS.DARK_BG,
                      border: `1px solid ${COLORS.GREEN}40`,
                      color: COLORS.WHITE,
                      fontSize: '11px',
                      fontFamily: 'monospace',
                      outline: 'none'
                    }}
                  />
                </div>
                <div>
                  <label style={{ display: 'block', fontSize: '9px', color: COLORS.GRAY, marginBottom: '4px' }}>
                    STOP LOSS
                  </label>
                  <input
                    type="text"
                    inputMode="decimal"
                    value={stopLoss}
                    onChange={(e) => {
                      const val = e.target.value;
                      if (val === '' || /^\d*\.?\d*$/.test(val)) setStopLoss(val);
                    }}
                    placeholder="0.00"
                    style={{
                      width: '100%',
                      padding: '6px 8px',
                      backgroundColor: COLORS.DARK_BG,
                      border: `1px solid ${COLORS.RED}40`,
                      color: COLORS.WHITE,
                      fontSize: '11px',
                      fontFamily: 'monospace',
                      outline: 'none'
                    }}
                  />
                </div>
              </div>
            )}
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div style={{
            padding: '8px',
            backgroundColor: `${COLORS.RED}15`,
            border: `1px solid ${COLORS.RED}40`,
            display: 'flex',
            alignItems: 'flex-start',
            gap: '8px',
            marginBottom: '10px'
          }}>
            <AlertCircle size={14} color={COLORS.RED} style={{ flexShrink: 0, marginTop: '1px' }} />
            <p style={{ fontSize: '10px', color: COLORS.RED, margin: 0 }}>{error}</p>
          </div>
        )}

        {/* Order Summary */}
        <div style={{
          padding: '8px',
          backgroundColor: COLORS.DARK_BG,
          border: `1px solid ${COLORS.BORDER}`,
          marginBottom: '10px'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: '4px' }}>
            <span style={{ color: COLORS.GRAY }}>Est. Value</span>
            <span style={{ color: COLORS.WHITE, fontFamily: 'monospace' }}>
              ₹{estimatedValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </span>
          </div>
          {marginRequired && (
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
              <span style={{ color: COLORS.GRAY }}>Margin Req.</span>
              <span style={{ color: COLORS.ORANGE, fontFamily: 'monospace' }}>
                {isCalculatingMargin ? (
                  <Loader2 size={12} className="animate-spin" />
                ) : (
                  `₹${marginRequired.totalMargin.toLocaleString('en-IN', { minimumFractionDigits: 2 })}`
                )}
              </span>
            </div>
          )}
        </div>
      </div>

      {/* Submit Button - Fixed at bottom */}
      <div style={{ padding: '8px', borderTop: `1px solid ${COLORS.BORDER}` }}>
        <button
          onClick={handleSubmit}
          disabled={
            isSubmitting ||
            !isAuthenticated ||
            !quantity ||
            Number(quantity) <= 0 ||
            (orderType !== 'MARKET' && (!price || Number(price) <= 0)) ||
            (['STOP', 'STOP_LIMIT'].includes(orderType) && (!triggerPrice || Number(triggerPrice) <= 0))
          }
          style={{
            width: '100%',
            padding: '12px',
            fontWeight: 700,
            fontSize: '12px',
            cursor: (
              isSubmitting || !isAuthenticated || !quantity || Number(quantity) <= 0 ||
              (orderType !== 'MARKET' && (!price || Number(price) <= 0)) ||
              (['STOP', 'STOP_LIMIT'].includes(orderType) && (!triggerPrice || Number(triggerPrice) <= 0))
            ) ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
            border: 'none',
            backgroundColor: side === 'BUY' ? COLORS.GREEN : COLORS.RED,
            color: side === 'BUY' ? COLORS.DARK_BG : COLORS.WHITE,
            opacity: (
              isSubmitting || !isAuthenticated || !quantity || Number(quantity) <= 0 ||
              (orderType !== 'MARKET' && (!price || Number(price) <= 0)) ||
              (['STOP', 'STOP_LIMIT'].includes(orderType) && (!triggerPrice || Number(triggerPrice) <= 0))
            ) ? 0.5 : 1,
            transition: 'all 0.15s'
          }}
        >
          {isSubmitting ? (
            <Loader2 size={16} className="animate-spin" />
          ) : (
            <>
              {side === 'BUY' ? <TrendingUp size={16} /> : <TrendingDown size={16} />}
              {side} {symbol}
            </>
          )}
        </button>
      </div>
    </div>
  );
}
