/**
 * Trading Interface - Bloomberg Style Order Entry
 */

import React, { useState, useEffect } from 'react';
import { BrokerType, OrderSide, OrderType, ProductType } from '../core/types';
import { useOrderExecution } from '../hooks/useOrderExecution';
import { RoutingStrategy } from '../core/OrderRouter';
import { integrationManager } from '../integrations/IntegrationManager';
import SymbolAutocomplete from './SymbolAutocomplete';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
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
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  INPUT_BG: '#0A0A0A',
  MUTED: '#4A4A4A'
};

interface TradingInterfaceProps {
  orderExecution: ReturnType<typeof useOrderExecution>;
  activeBrokers: BrokerType[];
  onSymbolChange?: (symbol: string, exchange: string) => void;
}

const TradingInterface: React.FC<TradingInterfaceProps> = ({ orderExecution, activeBrokers, onSymbolChange }) => {
  const [symbol, setSymbol] = useState('');
  const [exchange, setExchange] = useState('NSE');
  const [isPaperTrading, setIsPaperTrading] = useState(false);
  const [side, setSide] = useState<OrderSide>(OrderSide.BUY);
  const [orderType, setOrderType] = useState<OrderType>(OrderType.LIMIT);
  const [quantity, setQuantity] = useState('');
  const [price, setPrice] = useState('');
  const [triggerPrice, setTriggerPrice] = useState('');
  const [product, setProduct] = useState<ProductType>(ProductType.MIS);
  const [strategy, setStrategy] = useState<RoutingStrategy>(RoutingStrategy.BEST_LATENCY);
  const [selectedBrokers, setSelectedBrokers] = useState<BrokerType[]>([]);
  const [isFetchingPrice, setIsFetchingPrice] = useState(false);
  const [lastFetchedSymbol, setLastFetchedSymbol] = useState('');
  const [livePrice, setLivePrice] = useState<number | null>(null);
  const [priceChange, setPriceChange] = useState<number>(0);
  const [priceChangePercent, setPriceChangePercent] = useState<number>(0);

  // Check paper trading status
  useEffect(() => {
    const checkPaperTradingStatus = () => {
      setIsPaperTrading(integrationManager.isPaperTradingEnabled());
    };

    // Initial check
    checkPaperTradingStatus();

    // Poll every second for status changes
    const interval = setInterval(checkPaperTradingStatus, 1000);

    return () => clearInterval(interval);
  }, []);

  // Auto-fetch price when symbol or exchange changes
  useEffect(() => {
    const fetchPrice = async () => {
      if (!symbol || symbol.length < 2) {
        setPrice('');
        return;
      }

      // Don't fetch if symbol hasn't changed
      const symbolKey = `${exchange}:${symbol}`;
      if (symbolKey === lastFetchedSymbol) {
        return;
      }

      setIsFetchingPrice(true);
      try {
        console.log('[TradingInterface] Fetching price for:', symbol, exchange);

        // Use AuthManager to get authenticated Fyers adapter
        const { authManager } = await import('../services/AuthManager');
        const adapter = authManager.getAdapter('fyers');

        if (!adapter) {
          console.warn('[TradingInterface] ⚠️ Fyers adapter not available');
          setIsFetchingPrice(false);
          return;
        }

        // Format symbol for Fyers API: SBIN-EQ
        const fyersSymbol = `${symbol}-EQ`;
        console.log('[TradingInterface] Requesting quote for:', fyersSymbol, 'on', exchange);

        const quote = await adapter.getQuote(fyersSymbol, exchange);

        if (quote && quote.lastPrice && quote.lastPrice > 0) {
          setPrice(quote.lastPrice.toFixed(2));
          setLastFetchedSymbol(symbolKey);
          console.log('[TradingInterface] ✅ Auto-filled price:', quote.lastPrice);
        } else {
          console.warn('[TradingInterface] ⚠️ No valid quote data for', fyersSymbol);
        }
      } catch (error) {
        console.error('[TradingInterface] ❌ Failed to fetch price:', error);
      } finally {
        setIsFetchingPrice(false);
      }
    };

    // Debounce: Wait 800ms after user stops typing
    const timeoutId = setTimeout(fetchPrice, 800);

    return () => clearTimeout(timeoutId);
  }, [symbol, exchange, lastFetchedSymbol]);

  // Continuous live price updates every 3 seconds for selected symbol
  useEffect(() => {
    if (!symbol || symbol.length < 2) {
      setLivePrice(null);
      return;
    }

    const updateLivePrice = async () => {
      try {
        const { authManager } = await import('../services/AuthManager');
        const adapter = authManager.getAdapter('fyers');

        if (!adapter) return;

        const fyersSymbol = `${symbol}-EQ`;
        const quote = await adapter.getQuote(fyersSymbol, exchange);

        if (quote && quote.lastPrice && quote.lastPrice > 0) {
          const prevPrice = livePrice;
          setLivePrice(quote.lastPrice);

          // Calculate price change
          if (prevPrice && prevPrice > 0) {
            const change = quote.lastPrice - prevPrice;
            const changePercent = (change / prevPrice) * 100;
            setPriceChange(change);
            setPriceChangePercent(changePercent);
          } else if (quote.open && quote.open > 0) {
            const change = quote.lastPrice - quote.open;
            const changePercent = (change / quote.open) * 100;
            setPriceChange(change);
            setPriceChangePercent(changePercent);
          }
        }
      } catch (error) {
        // Silent fail - don't spam console for live updates
      }
    };

    // Initial fetch
    updateLivePrice();

    // Update every 3 seconds
    const interval = setInterval(updateLivePrice, 3000);

    return () => clearInterval(interval);
  }, [symbol, exchange, livePrice]);

  const handleSymbolChange = (newSymbol: string) => {
    setSymbol(newSymbol);
    // Don't call onSymbolChange here - only call it when symbol is explicitly selected
  };

  const handleExchangeChange = (newExchange: string) => {
    setExchange(newExchange);
    setLastFetchedSymbol(''); // Reset to trigger new fetch
    onSymbolChange?.(symbol, newExchange);
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    // Validation
    if (!symbol || !symbol.trim()) {
      alert('Please enter a symbol');
      return;
    }

    if (!quantity || parseInt(quantity) <= 0) {
      alert('Please enter a valid quantity');
      return;
    }

    if (orderType === OrderType.LIMIT && (!price || parseFloat(price) <= 0)) {
      alert('Please enter a valid price for limit order');
      return;
    }

    const order = {
      symbol,
      exchange,
      side,
      type: orderType,
      quantity: parseInt(quantity),
      price: price ? parseFloat(price) : undefined,
      triggerPrice: triggerPrice ? parseFloat(triggerPrice) : undefined,
      product,
    };

    try {
      console.log('[TradingInterface] Placing order:', order);
      await orderExecution.placeOrder(order, strategy, selectedBrokers.length > 0 ? selectedBrokers : undefined);

      // Show success message
      alert(`✅ Order placed successfully!\n${side} ${quantity} ${symbol} @ ${orderType === OrderType.MARKET ? 'Market Price' : '₹' + price}`);

      // Reset form
      setSymbol('');
      setQuantity('');
      setPrice('');
      setTriggerPrice('');
    } catch (error: any) {
      console.error('[TradingInterface] Order failed:', error);
      alert(`❌ Order failed: ${error.message || 'Unknown error'}`);
    }
  };

  // Label style
  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: BLOOMBERG.GRAY,
    marginBottom: '4px',
    letterSpacing: '0.5px',
    textTransform: 'uppercase',
    fontFamily: 'monospace'
  };

  // Input style
  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: BLOOMBERG.INPUT_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '0',
    color: BLOOMBERG.WHITE,
    fontSize: '12px',
    fontWeight: 600,
    fontFamily: 'monospace',
    outline: 'none',
    transition: 'border-color 0.15s ease'
  };

  return (
    <div style={{ height: '100%', overflow: 'auto' }}>
      <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Live Price Ticker */}
        {symbol && livePrice && (
          <div style={{
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `1px solid ${BLOOMBERG.BORDER}`,
            padding: '12px',
            borderRadius: '4px',
            display: 'flex',
            flexDirection: 'column',
            gap: '8px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, fontFamily: 'monospace' }}>
                  {exchange}:{symbol}
                </div>
                <div style={{ fontSize: '20px', fontWeight: 700, color: BLOOMBERG.WHITE, fontFamily: 'monospace', marginTop: '4px' }}>
                  ₹{livePrice.toFixed(2)}
                </div>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  fontSize: '14px',
                  fontWeight: 700,
                  color: priceChange >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  fontFamily: 'monospace'
                }}>
                  {priceChange >= 0 ? '+' : ''}{priceChange.toFixed(2)}
                </div>
                <div style={{
                  fontSize: '11px',
                  fontWeight: 600,
                  color: priceChangePercent >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  fontFamily: 'monospace',
                  marginTop: '2px'
                }}>
                  {priceChangePercent >= 0 ? '+' : ''}{priceChangePercent.toFixed(2)}%
                </div>
              </div>
            </div>
            <div style={{
              fontSize: '8px',
              color: BLOOMBERG.GRAY,
              fontFamily: 'monospace',
              textAlign: 'center'
            }}>
              ● LIVE • Updates every 3s
            </div>
          </div>
        )}

        {/* Paper Trading Indicator */}
        {isPaperTrading && (
          <div style={{
            backgroundColor: '#2A1A0A',
            border: `2px solid ${BLOOMBERG.ORANGE}`,
            padding: '12px',
            borderRadius: '4px',
            textAlign: 'center',
            animation: 'pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px'
            }}>
              <span style={{
                color: BLOOMBERG.ORANGE,
                fontWeight: 'bold',
                fontSize: '11px',
                fontFamily: 'monospace',
                letterSpacing: '1px'
              }}>
                ⚠️ PAPER TRADING MODE ACTIVE
              </span>
            </div>
            <div style={{
              marginTop: '6px',
              fontSize: '9px',
              color: BLOOMBERG.GRAY,
              fontFamily: 'monospace'
            }}>
              All orders will be simulated • No real money at risk
            </div>
          </div>
        )}

        {/* Symbol with Autocomplete */}
        <div>
          <label style={labelStyle}>SYMBOL</label>
          <SymbolAutocomplete
            value={symbol}
            onChange={(value) => handleSymbolChange(value)}
            onSelect={(selectedSymbol) => {
              handleSymbolChange(selectedSymbol);
              // Trigger symbol change event for quote panel
              if (onSymbolChange) {
                onSymbolChange(selectedSymbol, exchange);
              }
            }}
            placeholder="Type to search... (e.g., RELIANCE, TATAMOTORS)"
            style={inputStyle}
          />
        </div>

        {/* Exchange */}
        <div>
          <label style={labelStyle}>EXCHANGE</label>
          <select
            value={exchange}
            onChange={(e) => handleExchangeChange(e.target.value)}
            style={{
              ...inputStyle,
              cursor: 'pointer',
              appearance: 'none',
              backgroundImage: `url("data:image/svg+xml,%3Csvg width='10' height='6' viewBox='0 0 10 6' fill='none' xmlns='http://www.w3.org/2000/svg'%3E%3Cpath d='M1 1L5 5L9 1' stroke='%23787878' stroke-width='1.5'/%3E%3C/svg%3E")`,
              backgroundRepeat: 'no-repeat',
              backgroundPosition: 'right 10px center',
              paddingRight: '30px'
            }}
            onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
            onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
          >
            <option value="NSE">NSE</option>
            <option value="BSE">BSE</option>
            <option value="NFO">NFO</option>
            <option value="MCX">MCX</option>
          </select>
        </div>

        {/* Side - BUY/SELL Buttons */}
        <div>
          <label style={labelStyle}>SIDE</label>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
            <button
              type="button"
              onClick={() => setSide(OrderSide.BUY)}
              style={{
                padding: '10px',
                backgroundColor: side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.INPUT_BG,
                border: `1px solid ${side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}`,
                borderRadius: '0',
                color: side === OrderSide.BUY ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                fontSize: '11px',
                fontWeight: 700,
                letterSpacing: '1px',
                cursor: 'pointer',
                transition: 'all 0.15s ease',
                fontFamily: 'monospace',
                outline: 'none'
              }}
              onMouseEnter={(e) => {
                if (side !== OrderSide.BUY) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                  e.currentTarget.style.borderColor = BLOOMBERG.GREEN;
                  e.currentTarget.style.color = BLOOMBERG.GREEN;
                }
              }}
              onMouseLeave={(e) => {
                if (side !== OrderSide.BUY) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.INPUT_BG;
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                  e.currentTarget.style.color = BLOOMBERG.GRAY;
                }
              }}
            >
              BUY
            </button>
            <button
              type="button"
              onClick={() => setSide(OrderSide.SELL)}
              style={{
                padding: '10px',
                backgroundColor: side === OrderSide.SELL ? BLOOMBERG.RED : BLOOMBERG.INPUT_BG,
                border: `1px solid ${side === OrderSide.SELL ? BLOOMBERG.RED : BLOOMBERG.BORDER}`,
                borderRadius: '0',
                color: side === OrderSide.SELL ? BLOOMBERG.WHITE : BLOOMBERG.GRAY,
                fontSize: '11px',
                fontWeight: 700,
                letterSpacing: '1px',
                cursor: 'pointer',
                transition: 'all 0.15s ease',
                fontFamily: 'monospace',
                outline: 'none'
              }}
              onMouseEnter={(e) => {
                if (side !== OrderSide.SELL) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                  e.currentTarget.style.borderColor = BLOOMBERG.RED;
                  e.currentTarget.style.color = BLOOMBERG.RED;
                }
              }}
              onMouseLeave={(e) => {
                if (side !== OrderSide.SELL) {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.INPUT_BG;
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                  e.currentTarget.style.color = BLOOMBERG.GRAY;
                }
              }}
            >
              SELL
            </button>
          </div>
        </div>

        {/* Order Type */}
        <div>
          <label style={labelStyle}>ORDER TYPE</label>
          <select
            value={orderType}
            onChange={(e) => setOrderType(e.target.value as OrderType)}
            style={{
              ...inputStyle,
              cursor: 'pointer',
              appearance: 'none',
              backgroundImage: `url("data:image/svg+xml,%3Csvg width='10' height='6' viewBox='0 0 10 6' fill='none' xmlns='http://www.w3.org/2000/svg'%3E%3Cpath d='M1 1L5 5L9 1' stroke='%23787878' stroke-width='1.5'/%3E%3C/svg%3E")`,
              backgroundRepeat: 'no-repeat',
              backgroundPosition: 'right 10px center',
              paddingRight: '30px'
            }}
            onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
            onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
          >
            <option value={OrderType.MARKET}>MARKET</option>
            <option value={OrderType.LIMIT}>LIMIT</option>
            <option value={OrderType.STOP_LOSS}>STOP LOSS</option>
            <option value={OrderType.STOP_LOSS_MARKET}>SL-M</option>
          </select>
        </div>

        {/* Quantity */}
        <div>
          <label style={labelStyle}>QUANTITY</label>
          <input
            type="number"
            value={quantity}
            onChange={(e) => setQuantity(e.target.value)}
            style={inputStyle}
            placeholder="0"
            min="1"
            required
            onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
            onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
          />
        </div>

        {/* Price (for limit orders) */}
        {(orderType === OrderType.LIMIT || orderType === OrderType.STOP_LOSS) && (
          <div>
            <label style={{...labelStyle, display: 'flex', alignItems: 'center', justifyContent: 'space-between'}}>
              <span>PRICE (LTP)</span>
              {isFetchingPrice && (
                <span style={{ fontSize: '8px', color: BLOOMBERG.CYAN }}>
                  ⟳ Fetching...
                </span>
              )}
              {price && !isFetchingPrice && (
                <span style={{ fontSize: '8px', color: BLOOMBERG.GREEN }}>
                  ✓ Auto-filled
                </span>
              )}
            </label>
            <input
              type="number"
              value={price}
              onChange={(e) => setPrice(e.target.value)}
              style={{
                ...inputStyle,
                borderColor: isFetchingPrice ? BLOOMBERG.CYAN : (price ? BLOOMBERG.GREEN : BLOOMBERG.BORDER),
                color: price ? BLOOMBERG.GREEN : BLOOMBERG.WHITE
              }}
              placeholder="Auto-fetching..."
              step="0.05"
              required
              onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
              onBlur={(e) => e.target.style.borderColor = price ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}
            />
          </div>
        )}

        {/* Trigger Price (for SL orders) */}
        {(orderType === OrderType.STOP_LOSS || orderType === OrderType.STOP_LOSS_MARKET) && (
          <div>
            <label style={labelStyle}>TRIGGER PRICE</label>
            <input
              type="number"
              value={triggerPrice}
              onChange={(e) => setTriggerPrice(e.target.value)}
              style={inputStyle}
              placeholder="0.00"
              step="0.05"
              required
              onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
              onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
            />
          </div>
        )}

        {/* Product Type */}
        <div>
          <label style={labelStyle}>PRODUCT</label>
          <select
            value={product}
            onChange={(e) => setProduct(e.target.value as ProductType)}
            style={{
              ...inputStyle,
              cursor: 'pointer',
              appearance: 'none',
              backgroundImage: `url("data:image/svg+xml,%3Csvg width='10' height='6' viewBox='0 0 10 6' fill='none' xmlns='http://www.w3.org/2000/svg'%3E%3Cpath d='M1 1L5 5L9 1' stroke='%23787878' stroke-width='1.5'/%3E%3C/svg%3E")`,
              backgroundRepeat: 'no-repeat',
              backgroundPosition: 'right 10px center',
              paddingRight: '30px'
            }}
            onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
            onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
          >
            <option value={ProductType.CNC}>CNC (DELIVERY)</option>
            <option value={ProductType.MIS}>MIS (INTRADAY)</option>
            <option value={ProductType.NRML}>NRML (NORMAL)</option>
          </select>
        </div>

        {/* Routing Strategy */}
        <div>
          <label style={labelStyle}>ROUTING STRATEGY</label>
          <select
            value={strategy}
            onChange={(e) => setStrategy(e.target.value as RoutingStrategy)}
            style={{
              ...inputStyle,
              cursor: 'pointer',
              appearance: 'none',
              backgroundImage: `url("data:image/svg+xml,%3Csvg width='10' height='6' viewBox='0 0 10 6' fill='none' xmlns='http://www.w3.org/2000/svg'%3E%3Cpath d='M1 1L5 5L9 1' stroke='%23787878' stroke-width='1.5'/%3E%3C/svg%3E")`,
              backgroundRepeat: 'no-repeat',
              backgroundPosition: 'right 10px center',
              paddingRight: '30px'
            }}
            onFocus={(e) => e.target.style.borderColor = BLOOMBERG.ORANGE}
            onBlur={(e) => e.target.style.borderColor = BLOOMBERG.BORDER}
          >
            <option value={RoutingStrategy.BEST_LATENCY}>BEST LATENCY</option>
            <option value={RoutingStrategy.BEST_PRICE}>BEST PRICE</option>
            <option value={RoutingStrategy.PARALLEL}>ALL BROKERS</option>
            <option value={RoutingStrategy.ROUND_ROBIN}>ROUND ROBIN</option>
          </select>
        </div>

        {/* Broker Selection (for parallel) */}
        {strategy === RoutingStrategy.PARALLEL && (
          <div>
            <label style={labelStyle}>SELECT BROKERS</label>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', marginTop: '8px' }}>
              {activeBrokers.map(broker => (
                <label
                  key={broker}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    cursor: 'pointer',
                    padding: '6px 8px',
                    backgroundColor: selectedBrokers.includes(broker) ? BLOOMBERG.HOVER : 'transparent',
                    border: `1px solid ${selectedBrokers.includes(broker) ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                    transition: 'all 0.15s ease'
                  }}
                  onMouseEnter={(e) => {
                    if (!selectedBrokers.includes(broker)) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!selectedBrokers.includes(broker)) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <input
                    type="checkbox"
                    checked={selectedBrokers.includes(broker)}
                    onChange={(e) => {
                      if (e.target.checked) {
                        setSelectedBrokers([...selectedBrokers, broker]);
                      } else {
                        setSelectedBrokers(selectedBrokers.filter(b => b !== broker));
                      }
                    }}
                    style={{
                      width: '14px',
                      height: '14px',
                      cursor: 'pointer',
                      accentColor: BLOOMBERG.ORANGE
                    }}
                  />
                  <span style={{
                    fontSize: '10px',
                    fontWeight: 600,
                    color: selectedBrokers.includes(broker) ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY,
                    textTransform: 'uppercase',
                    letterSpacing: '0.5px',
                    fontFamily: 'monospace'
                  }}>
                    {broker}
                  </span>
                </label>
              ))}
            </div>
          </div>
        )}

        {/* Submit Button - Bloomberg Style */}
        <button
          type="submit"
          disabled={orderExecution.executing}
          style={{
            width: '100%',
            padding: '12px',
            marginTop: '8px',
            backgroundColor: side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.RED,
            border: `2px solid ${side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
            borderRadius: '0',
            color: side === OrderSide.BUY ? BLOOMBERG.DARK_BG : BLOOMBERG.WHITE,
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '1.5px',
            cursor: orderExecution.executing ? 'not-allowed' : 'pointer',
            opacity: orderExecution.executing ? 0.5 : 1,
            transition: 'all 0.15s ease',
            fontFamily: 'monospace',
            outline: 'none'
          }}
          onMouseEnter={(e) => {
            if (!orderExecution.executing) {
              e.currentTarget.style.filter = 'brightness(1.2)';
            }
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.filter = 'brightness(1)';
          }}
        >
          {orderExecution.executing ? 'PLACING ORDER...' : `${side} ${symbol || 'SYMBOL'}`}
        </button>

        {/* Error Display */}
        {orderExecution.error && (
          <div style={{
            padding: '10px',
            backgroundColor: 'rgba(255, 59, 59, 0.1)',
            border: `1px solid ${BLOOMBERG.RED}`,
            borderRadius: '0',
            color: BLOOMBERG.RED,
            fontSize: '10px',
            fontWeight: 600,
            letterSpacing: '0.3px',
            fontFamily: 'monospace'
          }}>
            ERROR: {orderExecution.error}
          </div>
        )}
      </form>
    </div>
  );
};

export default TradingInterface;
