// Fyers Trading Panel - Order Placement and Margin Calculator
import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Calculator, DollarSign, Activity } from 'lucide-react';
import { fyersService } from '../../../services/fyersService';

const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_DARK_BG = '#000000';
const BLOOMBERG_PANEL_BG = '#0a0a0a';
const BLOOMBERG_GREEN = '#00C800';
const BLOOMBERG_RED = '#FF0000';

interface Order {
  symbol: string;
  qty: number;
  side: 'BUY' | 'SELL';
  type: 'MARKET' | 'LIMIT' | 'STOP';
  productType: 'CNC' | 'INTRADAY' | 'MARGIN';
  limitPrice?: number;
  stopLoss?: number;
  stopPrice?: number;
  takeProfit?: number;
}

const FyersTradingPanel: React.FC = () => {
  const [activeSection, setActiveSection] = useState<'order' | 'gtt' | 'margin'>('order');

  // Order state
  const [orderSymbol, setOrderSymbol] = useState('');
  const [orderQty, setOrderQty] = useState<number>(1);
  const [orderSide, setOrderSide] = useState<'BUY' | 'SELL'>('BUY');
  const [orderType, setOrderType] = useState<'MARKET' | 'LIMIT' | 'STOP'>('MARKET');
  const [orderProduct, setOrderProduct] = useState<'CNC' | 'INTRADAY' | 'MARGIN'>('INTRADAY');
  const [orderLimitPrice, setOrderLimitPrice] = useState<number>(0);
  const [orderStopPrice, setOrderStopPrice] = useState<number>(0);

  // GTT state
  const [gttSymbol, setGttSymbol] = useState('');
  const [gttQty, setGttQty] = useState<number>(1);
  const [gttSide, setGttSide] = useState<'BUY' | 'SELL'>('BUY');
  const [gttProduct, setGttProduct] = useState<'CNC' | 'INTRADAY' | 'MARGIN'>('INTRADAY');
  const [gttTriggerPrice, setGttTriggerPrice] = useState<number>(0);
  const [gttLimitPrice, setGttLimitPrice] = useState<number>(0);

  // Margin calculator state
  const [marginOrders, setMarginOrders] = useState<Order[]>([]);
  const [marginResult, setMarginResult] = useState<any>(null);
  const [marginLoading, setMarginLoading] = useState(false);

  // New order for margin calculator
  const [newMarginOrder, setNewMarginOrder] = useState<Order>({
    symbol: '',
    qty: 1,
    side: 'BUY',
    type: 'LIMIT',
    productType: 'INTRADAY',
    limitPrice: 0
  });

  const handlePlaceOrder = async () => {
    if (!orderSymbol.trim()) {
      alert('Please enter a symbol');
      return;
    }

    try {
      const orderPayload: any = {
        symbol: orderSymbol.toUpperCase(),
        qty: orderQty,
        type: orderType === 'MARKET' ? 2 : orderType === 'LIMIT' ? 1 : 3,
        side: orderSide === 'BUY' ? 1 : -1,
        productType: orderProduct,
        validity: 'DAY',
        disclosedQty: 0,
        offlineOrder: false
      };

      if (orderType === 'LIMIT') {
        orderPayload.limitPrice = orderLimitPrice;
      }

      if (orderType === 'STOP') {
        orderPayload.stopPrice = orderStopPrice;
        orderPayload.limitPrice = orderLimitPrice;
      }

      const response = await fyersService.placeOrder(orderPayload);

      if (response.orderId || (response as any).s === 'ok') {
        alert(`Order placed successfully! Order ID: ${response.orderId || (response as any).id || 'N/A'}`);
        // Reset form
        setOrderSymbol('');
        setOrderQty(1);
        setOrderLimitPrice(0);
        setOrderStopPrice(0);
      } else {
        alert(`Order failed: ${response.message || 'Unknown error'}`);
      }
    } catch (error: any) {
      console.error('Order placement error:', error);
      alert(`Order failed: ${error.message || 'Unknown error'}`);
    }
  };

  const handlePlaceGTT = async () => {
    if (!gttSymbol.trim()) {
      alert('Please enter a symbol');
      return;
    }

    try {
      const gttPayload = {
        side: gttSide === 'BUY' ? 1 : -1,
        symbol: gttSymbol.toUpperCase(),
        productType: gttProduct,
        orderInfo: {
          leg1: {
            price: gttLimitPrice,
            triggerPrice: gttTriggerPrice,
            qty: gttQty
          }
        }
      };

      const response = await fyersService.placeGTTOrder(gttPayload);

      if (response.s === 'ok') {
        alert(`GTT Order placed successfully! Order ID: ${response.id || 'N/A'}`);
        // Reset form
        setGttSymbol('');
        setGttQty(1);
        setGttTriggerPrice(0);
        setGttLimitPrice(0);
      } else {
        alert(`GTT Order failed: ${response.message || 'Unknown error'}`);
      }
    } catch (error: any) {
      console.error('GTT Order placement error:', error);
      alert(`GTT Order failed: ${error.message || 'Unknown error'}`);
    }
  };

  const addMarginOrder = () => {
    if (!newMarginOrder.symbol.trim()) {
      alert('Please enter a symbol');
      return;
    }

    setMarginOrders([...marginOrders, { ...newMarginOrder, symbol: newMarginOrder.symbol.toUpperCase() }]);

    // Reset new order form
    setNewMarginOrder({
      symbol: '',
      qty: 1,
      side: 'BUY',
      type: 'LIMIT',
      productType: 'INTRADAY',
      limitPrice: 0
    });
  };

  const removeMarginOrder = (index: number) => {
    setMarginOrders(marginOrders.filter((_, i) => i !== index));
  };

  const calculateMargin = async () => {
    if (marginOrders.length === 0) {
      alert('Please add at least one order to calculate margin');
      return;
    }

    setMarginLoading(true);
    try {
      const response = await fyersService.calculateMultiOrderMargin(marginOrders);
      setMarginResult(response);
    } catch (error: any) {
      console.error('Margin calculation error:', error);
      alert(`Margin calculation failed: ${error.message || 'Unknown error'}`);
    } finally {
      setMarginLoading(false);
    }
  };

  return (
    <div style={{
      padding: '12px',
      fontFamily: 'Consolas, monospace',
      fontSize: '11px',
      color: BLOOMBERG_WHITE
    }}>
      {/* Section Tabs */}
      <div style={{
        display: 'flex',
        gap: '2px',
        marginBottom: '16px',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        {[
          { id: 'order', label: 'PLACE ORDER', icon: TrendingUp },
          { id: 'gtt', label: 'GTT ORDER', icon: Activity },
          { id: 'margin', label: 'MARGIN CALCULATOR', icon: Calculator }
        ].map(({ id, label, icon: Icon }) => (
          <button
            key={id}
            onClick={() => setActiveSection(id as any)}
            style={{
              backgroundColor: activeSection === id ? BLOOMBERG_DARK_BG : 'transparent',
              border: 'none',
              borderBottom: activeSection === id ? `2px solid ${BLOOMBERG_ORANGE}` : '2px solid transparent',
              color: activeSection === id ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
              padding: '8px 16px',
              fontSize: '9px',
              cursor: 'pointer',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontFamily: 'Consolas, monospace'
            }}
          >
            <Icon size={12} />
            {label}
          </button>
        ))}
      </div>

      {/* Place Order Section */}
      {activeSection === 'order' && (
        <div style={{
          backgroundColor: BLOOMBERG_PANEL_BG,
          padding: '16px',
          border: `1px solid ${BLOOMBERG_GRAY}`
        }}>
          <div style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', marginBottom: '16px', fontSize: '12px' }}>
            PLACE ORDER
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {/* Symbol */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                SYMBOL
              </label>
              <input
                type="text"
                value={orderSymbol}
                onChange={(e) => setOrderSymbol(e.target.value)}
                placeholder="e.g., NSE:SBIN-EQ"
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            {/* Quantity */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                QUANTITY
              </label>
              <input
                type="number"
                value={orderQty}
                onChange={(e) => setOrderQty(Number(e.target.value))}
                min="1"
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            {/* Side */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                SIDE
              </label>
              <div style={{ display: 'flex', gap: '4px' }}>
                <button
                  onClick={() => setOrderSide('BUY')}
                  style={{
                    flex: 1,
                    backgroundColor: orderSide === 'BUY' ? BLOOMBERG_GREEN : BLOOMBERG_DARK_BG,
                    color: orderSide === 'BUY' ? 'black' : BLOOMBERG_GREEN,
                    border: `1px solid ${BLOOMBERG_GREEN}`,
                    padding: '6px',
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  BUY
                </button>
                <button
                  onClick={() => setOrderSide('SELL')}
                  style={{
                    flex: 1,
                    backgroundColor: orderSide === 'SELL' ? BLOOMBERG_RED : BLOOMBERG_DARK_BG,
                    color: orderSide === 'SELL' ? BLOOMBERG_WHITE : BLOOMBERG_RED,
                    border: `1px solid ${BLOOMBERG_RED}`,
                    padding: '6px',
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  SELL
                </button>
              </div>
            </div>

            {/* Type */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                ORDER TYPE
              </label>
              <select
                value={orderType}
                onChange={(e) => setOrderType(e.target.value as any)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value="MARKET">MARKET</option>
                <option value="LIMIT">LIMIT</option>
                <option value="STOP">STOP</option>
              </select>
            </div>

            {/* Product Type */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                PRODUCT TYPE
              </label>
              <select
                value={orderProduct}
                onChange={(e) => setOrderProduct(e.target.value as any)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value="CNC">CNC</option>
                <option value="INTRADAY">INTRADAY</option>
                <option value="MARGIN">MARGIN</option>
              </select>
            </div>

            {/* Limit Price (show if LIMIT or STOP) */}
            {(orderType === 'LIMIT' || orderType === 'STOP') && (
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  LIMIT PRICE
                </label>
                <input
                  type="number"
                  value={orderLimitPrice}
                  onChange={(e) => setOrderLimitPrice(Number(e.target.value))}
                  step="0.05"
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            )}

            {/* Stop Price (show if STOP) */}
            {orderType === 'STOP' && (
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  STOP PRICE
                </label>
                <input
                  type="number"
                  value={orderStopPrice}
                  onChange={(e) => setOrderStopPrice(Number(e.target.value))}
                  step="0.05"
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            )}
          </div>

          <button
            onClick={handlePlaceOrder}
            style={{
              marginTop: '16px',
              backgroundColor: BLOOMBERG_ORANGE,
              color: 'black',
              border: 'none',
              padding: '10px 20px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              width: '100%',
              fontFamily: 'Consolas, monospace'
            }}
          >
            PLACE ORDER
          </button>
        </div>
      )}

      {/* GTT Order Section */}
      {activeSection === 'gtt' && (
        <div style={{
          backgroundColor: BLOOMBERG_PANEL_BG,
          padding: '16px',
          border: `1px solid ${BLOOMBERG_GRAY}`
        }}>
          <div style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', marginBottom: '16px', fontSize: '12px' }}>
            GTT ORDER (GOOD TILL TRIGGERED)
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {/* Symbol */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                SYMBOL
              </label>
              <input
                type="text"
                value={gttSymbol}
                onChange={(e) => setGttSymbol(e.target.value)}
                placeholder="e.g., NSE:SBIN-EQ"
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            {/* Quantity */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                QUANTITY
              </label>
              <input
                type="number"
                value={gttQty}
                onChange={(e) => setGttQty(Number(e.target.value))}
                min="1"
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            {/* Side */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                SIDE
              </label>
              <div style={{ display: 'flex', gap: '4px' }}>
                <button
                  onClick={() => setGttSide('BUY')}
                  style={{
                    flex: 1,
                    backgroundColor: gttSide === 'BUY' ? BLOOMBERG_GREEN : BLOOMBERG_DARK_BG,
                    color: gttSide === 'BUY' ? 'black' : BLOOMBERG_GREEN,
                    border: `1px solid ${BLOOMBERG_GREEN}`,
                    padding: '6px',
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  BUY
                </button>
                <button
                  onClick={() => setGttSide('SELL')}
                  style={{
                    flex: 1,
                    backgroundColor: gttSide === 'SELL' ? BLOOMBERG_RED : BLOOMBERG_DARK_BG,
                    color: gttSide === 'SELL' ? BLOOMBERG_WHITE : BLOOMBERG_RED,
                    border: `1px solid ${BLOOMBERG_RED}`,
                    padding: '6px',
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  SELL
                </button>
              </div>
            </div>

            {/* Product Type */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                PRODUCT TYPE
              </label>
              <select
                value={gttProduct}
                onChange={(e) => setGttProduct(e.target.value as any)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              >
                <option value="CNC">CNC</option>
                <option value="INTRADAY">INTRADAY</option>
                <option value="MARGIN">MARGIN</option>
              </select>
            </div>

            {/* Trigger Price */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                TRIGGER PRICE
              </label>
              <input
                type="number"
                value={gttTriggerPrice}
                onChange={(e) => setGttTriggerPrice(Number(e.target.value))}
                step="0.05"
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>

            {/* Limit Price */}
            <div>
              <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                LIMIT PRICE
              </label>
              <input
                type="number"
                value={gttLimitPrice}
                onChange={(e) => setGttLimitPrice(Number(e.target.value))}
                step="0.05"
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
            </div>
          </div>

          <button
            onClick={handlePlaceGTT}
            style={{
              marginTop: '16px',
              backgroundColor: BLOOMBERG_ORANGE,
              color: 'black',
              border: 'none',
              padding: '10px 20px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              width: '100%',
              fontFamily: 'Consolas, monospace'
            }}
          >
            PLACE GTT ORDER
          </button>
        </div>
      )}

      {/* Margin Calculator Section */}
      {activeSection === 'margin' && (
        <div>
          {/* Add Order Form */}
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            padding: '16px',
            border: `1px solid ${BLOOMBERG_GRAY}`,
            marginBottom: '12px'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', marginBottom: '16px', fontSize: '12px' }}>
              ADD ORDER TO CALCULATE MARGIN
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
              {/* Symbol */}
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  SYMBOL
                </label>
                <input
                  type="text"
                  value={newMarginOrder.symbol}
                  onChange={(e) => setNewMarginOrder({ ...newMarginOrder, symbol: e.target.value })}
                  placeholder="e.g., NSE:SBIN-EQ"
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>

              {/* Quantity */}
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  QTY
                </label>
                <input
                  type="number"
                  value={newMarginOrder.qty}
                  onChange={(e) => setNewMarginOrder({ ...newMarginOrder, qty: Number(e.target.value) })}
                  min="1"
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>

              {/* Side */}
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  SIDE
                </label>
                <select
                  value={newMarginOrder.side}
                  onChange={(e) => setNewMarginOrder({ ...newMarginOrder, side: e.target.value as any })}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                >
                  <option value="BUY">BUY</option>
                  <option value="SELL">SELL</option>
                </select>
              </div>

              {/* Type */}
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  TYPE
                </label>
                <select
                  value={newMarginOrder.type}
                  onChange={(e) => setNewMarginOrder({ ...newMarginOrder, type: e.target.value as any })}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                >
                  <option value="MARKET">MARKET</option>
                  <option value="LIMIT">LIMIT</option>
                  <option value="STOP">STOP</option>
                </select>
              </div>

              {/* Product Type */}
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  PRODUCT
                </label>
                <select
                  value={newMarginOrder.productType}
                  onChange={(e) => setNewMarginOrder({ ...newMarginOrder, productType: e.target.value as any })}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                >
                  <option value="CNC">CNC</option>
                  <option value="INTRADAY">INTRADAY</option>
                  <option value="MARGIN">MARGIN</option>
                </select>
              </div>

              {/* Limit Price */}
              <div>
                <label style={{ display: 'block', color: BLOOMBERG_GRAY, marginBottom: '4px', fontSize: '9px' }}>
                  LIMIT PRICE
                </label>
                <input
                  type="number"
                  value={newMarginOrder.limitPrice || 0}
                  onChange={(e) => setNewMarginOrder({ ...newMarginOrder, limitPrice: Number(e.target.value) })}
                  step="0.05"
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace'
                  }}
                />
              </div>
            </div>

            <button
              onClick={addMarginOrder}
              style={{
                marginTop: '12px',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_ORANGE}`,
                color: BLOOMBERG_ORANGE,
                padding: '8px 16px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              + ADD ORDER
            </button>
          </div>

          {/* Orders List */}
          {marginOrders.length > 0 && (
            <div style={{
              backgroundColor: BLOOMBERG_PANEL_BG,
              padding: '16px',
              border: `1px solid ${BLOOMBERG_GRAY}`,
              marginBottom: '12px'
            }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', marginBottom: '12px', fontSize: '12px' }}>
                ORDERS ({marginOrders.length})
              </div>

              <div style={{ maxHeight: '200px', overflow: 'auto' }}>
                {marginOrders.map((order, index) => (
                  <div
                    key={index}
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      padding: '8px',
                      backgroundColor: BLOOMBERG_DARK_BG,
                      marginBottom: '4px',
                      fontSize: '10px'
                    }}
                  >
                    <div style={{ flex: 1 }}>
                      <span style={{ color: BLOOMBERG_WHITE }}>{order.symbol}</span>
                      <span style={{ color: BLOOMBERG_GRAY, marginLeft: '8px' }}>
                        {order.side} {order.qty} @ {order.limitPrice || 'MKT'}
                      </span>
                      <span style={{ color: BLOOMBERG_GRAY, marginLeft: '8px' }}>
                        {order.productType}
                      </span>
                    </div>
                    <button
                      onClick={() => removeMarginOrder(index)}
                      style={{
                        backgroundColor: 'transparent',
                        border: `1px solid ${BLOOMBERG_RED}`,
                        color: BLOOMBERG_RED,
                        padding: '4px 8px',
                        fontSize: '9px',
                        cursor: 'pointer'
                      }}
                    >
                      REMOVE
                    </button>
                  </div>
                ))}
              </div>

              <button
                onClick={calculateMargin}
                disabled={marginLoading}
                style={{
                  marginTop: '12px',
                  backgroundColor: BLOOMBERG_ORANGE,
                  color: 'black',
                  border: 'none',
                  padding: '10px 20px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: marginLoading ? 'not-allowed' : 'pointer',
                  width: '100%',
                  fontFamily: 'Consolas, monospace',
                  opacity: marginLoading ? 0.5 : 1
                }}
              >
                {marginLoading ? 'CALCULATING...' : 'CALCULATE MARGIN'}
              </button>
            </div>
          )}

          {/* Margin Result */}
          {marginResult && (
            <div style={{
              backgroundColor: BLOOMBERG_PANEL_BG,
              padding: '16px',
              border: `1px solid ${BLOOMBERG_GRAY}`
            }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', marginBottom: '12px', fontSize: '12px' }}>
                MARGIN CALCULATION RESULT
              </div>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '4px' }}>
                    AVAILABLE MARGIN
                  </div>
                  <div style={{ color: BLOOMBERG_GREEN, fontSize: '14px', fontWeight: 'bold' }}>
                    ₹{marginResult.margin_avail?.toFixed(2) || '0.00'}
                  </div>
                </div>

                <div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '4px' }}>
                    REQUIRED MARGIN
                  </div>
                  <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
                    ₹{marginResult.total?.toFixed(2) || '0.00'}
                  </div>
                </div>
              </div>

              {marginResult.data && marginResult.data.length > 0 && (
                <div style={{ marginTop: '12px' }}>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginBottom: '8px' }}>
                    ORDER-WISE BREAKDOWN
                  </div>
                  {marginResult.data.map((item: any, index: number) => (
                    <div
                      key={index}
                      style={{
                        backgroundColor: BLOOMBERG_DARK_BG,
                        padding: '8px',
                        marginBottom: '4px',
                        fontSize: '10px'
                      }}
                    >
                      <div style={{ color: BLOOMBERG_WHITE }}>{item.symbol || marginOrders[index]?.symbol}</div>
                      <div style={{ color: BLOOMBERG_GRAY, marginTop: '4px' }}>
                        Margin: ₹{item.margin?.toFixed(2) || '0.00'}
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default FyersTradingPanel;
