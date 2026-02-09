// CryptoOrderEntry.tsx - Professional Order Entry Panel
import React, { useState, useCallback, useMemo, useRef, useEffect } from 'react';
import { RefreshCw, TrendingUp, TrendingDown, AlertTriangle, Check, Loader2, ChevronDown, ChevronUp } from 'lucide-react';
import { FINCEPT } from '../constants';

interface CryptoOrderEntryProps {
  selectedSymbol: string;
  currentPrice: number;
  balance: number;
  tradingMode: 'paper' | 'live';
  activeBroker: string | null;
  isLoading: boolean;
  isConnecting: boolean;
  paperAdapter: any;
  realAdapter: any;
  onTradingModeChange: (mode: 'paper' | 'live') => void;
  onRetryConnection?: () => void;
  onOrderPlaced?: () => void;
}

type OrderType = 'market' | 'limit' | 'stop-limit';
type OrderSide = 'buy' | 'sell';

export function CryptoOrderEntry({
  selectedSymbol,
  currentPrice,
  balance,
  tradingMode,
  activeBroker,
  isLoading,
  isConnecting,
  paperAdapter,
  realAdapter,
  onTradingModeChange,
  onRetryConnection,
  onOrderPlaced,
}: CryptoOrderEntryProps) {
  // Order state
  const [side, setSide] = useState<OrderSide>('buy');
  const [orderType, setOrderType] = useState<OrderType>('limit');
  const [quantity, setQuantity] = useState('');
  const [price, setPrice] = useState('');
  const [stopPrice, setStopPrice] = useState('');
  const [isPlacing, setIsPlacing] = useState(false);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [stopLoss, setStopLoss] = useState('');
  const [takeProfit, setTakeProfit] = useState('');
  const [orderMessage, setOrderMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  // Refs for input focus
  const quantityInputRef = useRef<HTMLInputElement>(null);
  const priceInputRef = useRef<HTMLInputElement>(null);

  // Track if user has manually interacted with price field
  const [priceInitialized, setPriceInitialized] = useState(false);

  // Set initial price when switching to limit or stop-limit order type
  useEffect(() => {
    if ((orderType === 'limit' || orderType === 'stop-limit') && !priceInitialized && currentPrice > 0) {
      setPrice(currentPrice.toFixed(2));
      setPriceInitialized(true);
    }
  }, [orderType, priceInitialized, currentPrice]);

  // Reset fields when order type changes
  useEffect(() => {
    if (orderType === 'market') {
      setPriceInitialized(false);
      setPrice('');
      setStopPrice('');
    } else {
      // Re-initialize price for limit/stop-limit if needed
      setPriceInitialized(false);
    }
  }, [orderType]);

  // Get active adapter
  const adapter = tradingMode === 'paper' ? paperAdapter : realAdapter;
  const isConnected = adapter?.isConnected?.() || false;

  // Parse values
  const quantityNum = parseFloat(quantity) || 0;
  const parsedPrice = parseFloat(price) || 0;
  const priceNum = orderType === 'market' ? currentPrice : parsedPrice;
  const total = quantityNum * priceNum;

  // Calculate max quantity — use priceNum if set, else currentPrice for display
  const baseAsset = selectedSymbol.split('/')[0];
  const effectivePriceForMax = priceNum > 0 ? priceNum : currentPrice;
  const maxBuyQuantity = balance > 0 && effectivePriceForMax > 0 ? balance / effectivePriceForMax : 0;

  // Format helpers
  const formatNum = (num: number, decimals = 2) => {
    if (num >= 1000000) return `${(num / 1000000).toFixed(2)}M`;
    if (num >= 1000) return `${(num / 1000).toFixed(2)}K`;
    return num.toFixed(decimals);
  };

  const formatPrice = (num: number) => {
    if (num >= 1000) return num.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    if (num >= 1) return num.toFixed(2);
    return num.toFixed(6);
  };

  // Validation
  const validation = useMemo(() => {
    const errors: string[] = [];
    if (quantityNum <= 0) errors.push('Enter quantity');
    if ((orderType === 'limit' || orderType === 'stop-limit') && priceNum <= 0) errors.push('Enter price');
    if (orderType === 'stop-limit' && (!stopPrice || parseFloat(stopPrice) <= 0)) errors.push('Enter stop price');
    // Include estimated fee (0.1%) in balance check
    const totalWithFees = total + (total * 0.001);
    if (side === 'buy' && totalWithFees > balance && total > 0) errors.push(`Insufficient balance ($${balance.toFixed(2)} available, $${totalWithFees.toFixed(2)} needed)`);
    return { valid: errors.length === 0, errors };
  }, [quantityNum, priceNum, orderType, stopPrice, side, total, balance]);

  // Handle percentage buttons
  const handlePercentage = useCallback((percent: number) => {
    const effectivePrice = priceNum > 0 ? priceNum : currentPrice;
    if (effectivePrice <= 0) return;

    if (side === 'buy') {
      // Buy: how much base asset can we afford with balance
      const maxQty = balance / effectivePrice;
      const qty = maxQty * percent;
      setQuantity(qty > 0 ? qty.toFixed(6) : '');
    } else {
      // Sell: percentage of balance (balance here is USD, not holdings)
      // For sell side the percentage is of available holdings, not USD balance
      const qty = balance * percent / effectivePrice;
      setQuantity(qty > 0 ? qty.toFixed(6) : '');
    }
  }, [balance, currentPrice, priceNum, side]);

  // Handle order submission
  const handleSubmit = useCallback(async () => {
    if (!validation.valid || !adapter || isPlacing) return;

    setIsPlacing(true);
    setOrderMessage(null);
    try {
      const parsedStopLoss = parseFloat(stopLoss) || undefined;
      const parsedTakeProfit = parseFloat(takeProfit) || undefined;
      const parsedStopPrice = parseFloat(stopPrice) || undefined;

      // Build params object for advanced options
      const params: Record<string, any> = {};
      if (parsedStopLoss) params.stopLoss = parsedStopLoss;
      if (parsedTakeProfit) params.takeProfit = parsedTakeProfit;
      if (orderType === 'stop-limit' && parsedStopPrice) params.stopPrice = parsedStopPrice;

      if (typeof adapter.createOrder === 'function') {
        await adapter.createOrder(
          selectedSymbol,
          orderType,
          side,
          quantityNum,
          orderType !== 'market' ? priceNum : undefined,
          Object.keys(params).length > 0 ? params : undefined
        );
      }

      // Reset form on success
      setQuantity('');
      setStopLoss('');
      setTakeProfit('');
      setStopPrice('');
      if (orderType === 'limit' || orderType === 'stop-limit') {
        setPrice(currentPrice.toFixed(2));
        setPriceInitialized(true);
      }

      setOrderMessage({ type: 'success', text: `${side.toUpperCase()} ${quantityNum} ${selectedSymbol} ${orderType} order placed` });

      // Trigger data refresh in parent
      onOrderPlaced?.();

      // Auto-clear message after 4 seconds
      setTimeout(() => setOrderMessage(null), 4000);

    } catch (err: any) {
      console.error('[OrderEntry] Order failed:', err);
      setOrderMessage({ type: 'error', text: err?.message || 'Order failed' });
      setTimeout(() => setOrderMessage(null), 6000);
    } finally {
      setIsPlacing(false);
    }
  }, [validation.valid, adapter, isPlacing, selectedSymbol, orderType, side, quantityNum, priceNum, currentPrice, stopLoss, takeProfit, stopPrice, onOrderPlaced]);

  // Handle retry connection
  const handleRetryConnection = useCallback(() => {
    if (onRetryConnection) {
      onRetryConnection();
    }
    onTradingModeChange('paper');
  }, [onRetryConnection, onTradingModeChange]);

  // Loading state
  if (isLoading) {
    return (
      <div style={{
        height: '45%',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        backgroundColor: FINCEPT.PANEL_BG,
      }}>
        <OrderHeader tradingMode={tradingMode} />
        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '20px' }}>
          <div style={{ textAlign: 'center' }}>
            <Loader2 size={24} color={FINCEPT.CYAN} style={{ animation: 'spin 1s linear infinite' }} />
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginTop: '8px' }}>Loading...</div>
          </div>
        </div>
        <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
      </div>
    );
  }

  // Not connected state
  if (!isConnected) {
    return (
      <div style={{
        height: '45%',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        backgroundColor: FINCEPT.PANEL_BG,
      }}>
        <OrderHeader tradingMode={tradingMode} />
        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '20px' }}>
          <div style={{ textAlign: 'center' }}>
            <AlertTriangle size={24} color={FINCEPT.YELLOW} />
            <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 600, marginTop: '8px' }}>
              {isConnecting ? 'Connecting...' : 'Not Connected'}
            </div>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '4px' }}>
              {isConnecting ? `Connecting to ${activeBroker}...` : 'Select a broker to start trading'}
            </div>
            {!isConnecting && (
              <button
                onClick={handleRetryConnection}
                style={{
                  marginTop: '12px',
                  padding: '6px 16px',
                  backgroundColor: FINCEPT.ORANGE,
                  border: 'none',
                  color: FINCEPT.DARK_BG,
                  cursor: 'pointer',
                  fontSize: '10px',
                  fontWeight: 700,
                  borderRadius: '4px',
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '6px',
                }}
              >
                <RefreshCw size={12} />
                Retry
              </button>
            )}
          </div>
        </div>
      </div>
    );
  }

  return (
    <div style={{
      height: '45%',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      backgroundColor: FINCEPT.PANEL_BG,
    }}>
      <style>{`
        .oe-input {
          width: 100%;
          padding: 10px 12px;
          background: ${FINCEPT.HEADER_BG};
          border: 1px solid ${FINCEPT.BORDER};
          color: ${FINCEPT.WHITE};
          font-size: 13px;
          font-family: "IBM Plex Mono", monospace;
          outline: none;
          transition: border-color 0.15s ease;
        }
        .oe-input:focus {
          border-color: ${FINCEPT.ORANGE};
        }
        .oe-input::placeholder {
          color: ${FINCEPT.GRAY};
        }
        .oe-label {
          display: flex;
          justify-content: space-between;
          align-items: center;
          font-size: 9px;
          font-weight: 600;
          color: ${FINCEPT.GRAY};
          margin-bottom: 4px;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }
        .oe-pct-btn {
          padding: 6px 0;
          background: ${FINCEPT.HEADER_BG};
          border: 1px solid ${FINCEPT.BORDER};
          color: ${FINCEPT.GRAY};
          font-size: 10px;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.15s ease;
          flex: 1;
        }
        .oe-pct-btn:hover {
          background: ${FINCEPT.HOVER};
          border-color: ${FINCEPT.ORANGE};
          color: ${FINCEPT.ORANGE};
        }
        .oe-type-btn {
          flex: 1;
          padding: 8px 4px;
          background: transparent;
          border: none;
          border-bottom: 2px solid transparent;
          color: ${FINCEPT.GRAY};
          font-size: 9px;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.15s ease;
          text-transform: uppercase;
        }
        .oe-type-btn:hover {
          color: ${FINCEPT.WHITE};
        }
        .oe-type-btn.active {
          color: ${FINCEPT.ORANGE};
          border-bottom-color: ${FINCEPT.ORANGE};
        }
      `}</style>

      {/* Header */}
      <OrderHeader tradingMode={tradingMode} />

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {/* Buy/Sell Toggle */}
        <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
          <button
            onClick={() => setSide('buy')}
            style={{
              flex: 1,
              padding: '12px',
              background: side === 'buy' ? FINCEPT.GREEN : 'transparent',
              border: `2px solid ${side === 'buy' ? FINCEPT.GREEN : FINCEPT.BORDER}`,
              color: side === 'buy' ? FINCEPT.DARK_BG : FINCEPT.GREEN,
              fontSize: '12px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
            }}
          >
            <TrendingUp size={14} />
            BUY
          </button>
          <button
            onClick={() => setSide('sell')}
            style={{
              flex: 1,
              padding: '12px',
              background: side === 'sell' ? FINCEPT.RED : 'transparent',
              border: `2px solid ${side === 'sell' ? FINCEPT.RED : FINCEPT.BORDER}`,
              color: side === 'sell' ? FINCEPT.WHITE : FINCEPT.RED,
              fontSize: '12px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.15s ease',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
            }}
          >
            <TrendingDown size={14} />
            SELL
          </button>
        </div>

        {/* Order Type Tabs */}
        <div style={{ display: 'flex', borderBottom: `1px solid ${FINCEPT.BORDER}`, marginBottom: '16px' }}>
          {(['limit', 'market', 'stop-limit'] as OrderType[]).map((type) => (
            <button
              key={type}
              className={`oe-type-btn ${orderType === type ? 'active' : ''}`}
              onClick={() => setOrderType(type)}
            >
              {type.replace('-', ' ')}
            </button>
          ))}
        </div>

        {/* Price Input (for limit/stop-limit) */}
        {orderType !== 'market' && (
          <div style={{ marginBottom: '12px' }}>
            <div className="oe-label">
              <span>Price</span>
              <span style={{ color: FINCEPT.ORANGE, cursor: 'pointer' }} onClick={() => { setPrice(currentPrice.toFixed(2)); setPriceInitialized(true); }}>
                Market: ${formatPrice(currentPrice)}
              </span>
            </div>
            <div style={{ position: 'relative' }}>
              <input
                ref={priceInputRef}
                type="text"
                className="oe-input"
                value={price}
                onChange={(e) => /^\d*\.?\d*$/.test(e.target.value) && setPrice(e.target.value)}
                placeholder="0.00"
              />
              <span style={{ position: 'absolute', right: '12px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.GRAY, fontSize: '11px' }}>
                USD
              </span>
            </div>
          </div>
        )}

        {/* Stop Price (for stop-limit) */}
        {orderType === 'stop-limit' && (
          <div style={{ marginBottom: '12px' }}>
            <div className="oe-label">
              <span>Stop Price</span>
            </div>
            <input
              type="text"
              className="oe-input"
              value={stopPrice}
              onChange={(e) => /^\d*\.?\d*$/.test(e.target.value) && setStopPrice(e.target.value)}
              placeholder="0.00"
            />
          </div>
        )}

        {/* Market Price Display */}
        {orderType === 'market' && (
          <div style={{
            marginBottom: '12px',
            padding: '12px',
            background: `${FINCEPT.ORANGE}10`,
            border: `1px solid ${FINCEPT.ORANGE}30`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}>
            <span style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>Market Price</span>
            <span style={{ color: FINCEPT.ORANGE, fontSize: '14px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace' }}>
              ${formatPrice(currentPrice)}
            </span>
          </div>
        )}

        {/* Quantity Input */}
        <div style={{ marginBottom: '12px' }}>
          <div className="oe-label">
            <span>Amount</span>
            <span>Max: {formatNum(maxBuyQuantity, 6)} {baseAsset}</span>
          </div>
          <div style={{ position: 'relative' }}>
            <input
              ref={quantityInputRef}
              type="text"
              className="oe-input"
              value={quantity}
              onChange={(e) => /^\d*\.?\d*$/.test(e.target.value) && setQuantity(e.target.value)}
              placeholder="0.00"
            />
            <span style={{ position: 'absolute', right: '12px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.GRAY, fontSize: '11px' }}>
              {baseAsset}
            </span>
          </div>
        </div>

        {/* Percentage Buttons */}
        <div style={{ display: 'flex', gap: '4px', marginBottom: '16px' }}>
          {[25, 50, 75, 100].map((pct) => (
            <button
              key={pct}
              className="oe-pct-btn"
              onClick={() => handlePercentage(pct / 100)}
            >
              {pct}%
            </button>
          ))}
        </div>

        {/* Order Summary */}
        <div style={{
          padding: '12px',
          background: FINCEPT.HEADER_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          marginBottom: '12px',
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>Order Value</span>
            <span style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600 }}>
              ${formatNum(total)}
            </span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>Est. Fee (0.1%)</span>
            <span style={{ color: FINCEPT.GRAY, fontSize: '11px' }}>
              ${formatNum(total * 0.001)}
            </span>
          </div>
          <div style={{ height: '1px', background: FINCEPT.BORDER, margin: '8px 0' }} />
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>Total</span>
            <span style={{ color: FINCEPT.YELLOW, fontSize: '14px', fontWeight: 700, fontFamily: '"IBM Plex Mono", monospace' }}>
              ${formatNum(total + (total * 0.001))}
            </span>
          </div>
        </div>

        {/* Advanced Options Toggle */}
        <button
          onClick={() => setShowAdvanced(!showAdvanced)}
          style={{
            width: '100%',
            padding: '8px',
            background: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            fontSize: '10px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '6px',
            marginBottom: '12px',
          }}
        >
          {showAdvanced ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
          {showAdvanced ? 'Hide' : 'Show'} Advanced Options
        </button>

        {/* Advanced Options */}
        {showAdvanced && (
          <div style={{ marginBottom: '12px', padding: '12px', background: 'rgba(0,0,0,0.2)', border: `1px solid ${FINCEPT.BORDER}` }}>
            <div style={{ marginBottom: '12px' }}>
              <div className="oe-label">
                <span>Stop Loss</span>
              </div>
              <input
                type="text"
                className="oe-input"
                value={stopLoss}
                onChange={(e) => /^\d*\.?\d*$/.test(e.target.value) && setStopLoss(e.target.value)}
                placeholder="Optional"
                style={{ fontSize: '11px', padding: '8px 10px' }}
              />
            </div>
            <div>
              <div className="oe-label">
                <span>Take Profit</span>
              </div>
              <input
                type="text"
                className="oe-input"
                value={takeProfit}
                onChange={(e) => /^\d*\.?\d*$/.test(e.target.value) && setTakeProfit(e.target.value)}
                placeholder="Optional"
                style={{ fontSize: '11px', padding: '8px 10px' }}
              />
            </div>
          </div>
        )}

        {/* Validation Errors */}
        {!validation.valid && quantityNum > 0 && (
          <div style={{
            padding: '8px 12px',
            marginBottom: '12px',
            background: `${FINCEPT.RED}10`,
            borderLeft: `3px solid ${FINCEPT.RED}`,
            color: FINCEPT.RED,
            fontSize: '10px',
          }}>
            {validation.errors.join(' • ')}
          </div>
        )}

        {/* Order Result Message */}
        {orderMessage && (
          <div style={{
            padding: '8px 12px',
            marginBottom: '12px',
            background: orderMessage.type === 'success' ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
            borderLeft: `3px solid ${orderMessage.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED}`,
            color: orderMessage.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '10px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}>
            {orderMessage.type === 'success' ? <Check size={12} /> : <AlertTriangle size={12} />}
            {orderMessage.text}
          </div>
        )}

        {/* Submit Button */}
        <button
          onClick={handleSubmit}
          disabled={!validation.valid || isPlacing}
          style={{
            width: '100%',
            padding: '14px',
            background: validation.valid
              ? (side === 'buy' ? `linear-gradient(180deg, ${FINCEPT.GREEN}, #00A857)` : `linear-gradient(180deg, ${FINCEPT.RED}, #CC2F2F)`)
              : FINCEPT.HEADER_BG,
            border: 'none',
            color: validation.valid ? FINCEPT.WHITE : FINCEPT.GRAY,
            fontSize: '13px',
            fontWeight: 700,
            cursor: validation.valid ? 'pointer' : 'not-allowed',
            transition: 'all 0.15s ease',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px',
            textTransform: 'uppercase',
            letterSpacing: '1px',
            opacity: isPlacing ? 0.7 : 1,
          }}
        >
          {isPlacing ? (
            <>
              <Loader2 size={16} style={{ animation: 'spin 1s linear infinite' }} />
              Placing Order...
            </>
          ) : (
            <>
              {side === 'buy' ? <TrendingUp size={16} /> : <TrendingDown size={16} />}
              {side.toUpperCase()} {baseAsset}
            </>
          )}
        </button>

        {/* Balance Info */}
        <div style={{
          marginTop: '12px',
          padding: '8px 12px',
          background: FINCEPT.HEADER_BG,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}>
          <span style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>Available Balance</span>
          <span style={{ color: FINCEPT.CYAN, fontSize: '11px', fontWeight: 600, fontFamily: '"IBM Plex Mono", monospace' }}>
            ${formatNum(balance)}
          </span>
        </div>
      </div>
    </div>
  );
}

// Header Component
function OrderHeader({ tradingMode }: { tradingMode: 'paper' | 'live' }) {
  return (
    <div style={{
      padding: '10px 12px',
      background: tradingMode === 'live'
        ? `linear-gradient(90deg, ${FINCEPT.RED}30, ${FINCEPT.HEADER_BG})`
        : FINCEPT.HEADER_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
    }}>
      <span style={{
        fontSize: '11px',
        fontWeight: 700,
        color: FINCEPT.WHITE,
        letterSpacing: '0.5px',
      }}>
        ORDER ENTRY
      </span>
      <span style={{
        padding: '3px 10px',
        background: tradingMode === 'live' ? FINCEPT.RED : FINCEPT.GREEN,
        color: tradingMode === 'live' ? FINCEPT.WHITE : FINCEPT.DARK_BG,
        fontSize: '9px',
        fontWeight: 700,
        borderRadius: '3px',
        display: 'flex',
        alignItems: 'center',
        gap: '4px',
      }}>
        <div style={{
          width: '5px',
          height: '5px',
          borderRadius: '50%',
          background: tradingMode === 'live' ? FINCEPT.WHITE : FINCEPT.DARK_BG,
        }} />
        {tradingMode === 'live' ? 'LIVE' : 'PAPER'}
      </span>
    </div>
  );
}

export default CryptoOrderEntry;
