// File: src/components/tabs/trading/OrderForm.tsx
// Feature-aware order placement form - adapts to broker capabilities

import React, { useState } from 'react';
import { useBrokerContext } from '../../../contexts/BrokerContext';
import type { OrderRequest, OrderType, OrderSide } from '../../../types/trading';

interface OrderFormProps {
  symbol: string;
  currentPrice?: number;
  onPlaceOrder: (order: OrderRequest) => Promise<void>;
  isLoading?: boolean;
}

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
    <div className="bg-[#0d0d0d] border-t border-gray-800 p-4">
      <h3 className="text-sm font-bold text-gray-400 mb-3">PLACE ORDER</h3>

      <form onSubmit={handleSubmit} className="space-y-3">
        {/* Buy/Sell Toggle */}
        <div className="flex gap-2">
          <button
            type="button"
            onClick={() => setSide('buy')}
            className={`flex-1 py-2 rounded font-semibold text-sm transition-colors ${
              side === 'buy'
                ? 'bg-green-600 text-white'
                : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
            }`}
          >
            BUY
          </button>
          <button
            type="button"
            onClick={() => setSide('sell')}
            className={`flex-1 py-2 rounded font-semibold text-sm transition-colors ${
              side === 'sell'
                ? 'bg-red-600 text-white'
                : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
            }`}
          >
            SELL
          </button>
        </div>

        {/* Order Type - Only show supported types */}
        <div>
          <label className="block text-xs text-gray-500 mb-1">Order Type</label>
          <select
            value={orderType}
            onChange={(e) => setOrderType(e.target.value as OrderType)}
            className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-orange-500"
          >
            {supportsMarket && <option value="market">Market</option>}
            {supportsLimit && <option value="limit">Limit</option>}
            {supportsStop && <option value="stop_market">Stop Market</option>}
            {supportsStopLimit && <option value="stop_limit">Stop Limit</option>}
          </select>
        </div>

        {/* Quantity */}
        <div>
          <label className="block text-xs text-gray-500 mb-1">Quantity</label>
          <input
            type="number"
            step="0.0001"
            value={quantity}
            onChange={(e) => setQuantity(e.target.value)}
            placeholder="0.00"
            className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-orange-500"
          />
        </div>

        {/* Limit Price (for limit and stop_limit) */}
        {(orderType === 'limit' || orderType === 'stop_limit') && (
          <div>
            <label className="block text-xs text-gray-500 mb-1">Limit Price</label>
            <input
              type="number"
              step="0.01"
              value={price}
              onChange={(e) => setPrice(e.target.value)}
              placeholder={currentPrice?.toFixed(2) || '0.00'}
              className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-orange-500"
            />
          </div>
        )}

        {/* Stop Price (for stop_market and stop_limit) */}
        {(orderType === 'stop_market' || orderType === 'stop_limit') && (
          <div>
            <label className="block text-xs text-gray-500 mb-1">Stop Price</label>
            <input
              type="number"
              step="0.01"
              value={stopPrice}
              onChange={(e) => setStopPrice(e.target.value)}
              placeholder={currentPrice?.toFixed(2) || '0.00'}
              className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-orange-500"
            />
          </div>
        )}

        {/* Advanced Settings Toggle */}
        <button
          type="button"
          onClick={() => setEnableAdvanced(!enableAdvanced)}
          className="text-xs text-orange-500 hover:text-orange-400 transition-colors"
        >
          {enableAdvanced ? '− ' : '+ '}Advanced (SL/TP)
        </button>

        {/* Advanced Settings: Stop Loss & Take Profit */}
        {enableAdvanced && (
          <div className="space-y-3 border border-gray-700 rounded p-3 bg-gray-900/50">
            <div>
              <label className="block text-xs text-gray-500 mb-1">Take Profit Price</label>
              <input
                type="number"
                step="0.01"
                value={takeProfitPrice}
                onChange={(e) => setTakeProfitPrice(e.target.value)}
                placeholder="Optional"
                className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-green-500"
              />
            </div>

            <div>
              <label className="block text-xs text-gray-500 mb-1">Stop Loss Price</label>
              <input
                type="number"
                step="0.01"
                value={stopLossPrice}
                onChange={(e) => setStopLossPrice(e.target.value)}
                placeholder="Optional"
                className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-red-500"
              />
            </div>

            {currentPrice && (takeProfitPrice || stopLossPrice) && (
              <div className="text-xs space-y-1">
                {takeProfitPrice && (
                  <div className="flex justify-between text-green-400">
                    <span>TP R/R:</span>
                    <span>
                      {((Math.abs(parseFloat(takeProfitPrice) - currentPrice) / currentPrice) * 100).toFixed(2)}%
                    </span>
                  </div>
                )}
                {stopLossPrice && (
                  <div className="flex justify-between text-red-400">
                    <span>SL Risk:</span>
                    <span>
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
          <div className="flex justify-between text-xs">
            <span className="text-gray-500">Estimated {side === 'buy' ? 'Cost' : 'Proceeds'}:</span>
            <span className="text-white font-mono">${estimatedCost}</span>
          </div>
        )}

        {/* Error Message */}
        {error && (
          <div className="bg-red-900/20 border border-red-600 rounded p-2 text-xs text-red-400">
            {error}
          </div>
        )}

        {/* Trading Mode Warning */}
        {tradingMode === 'live' && (
          <div className="bg-red-900/20 border border-red-600 rounded p-2 text-xs text-red-400 font-bold">
            ⚠️ LIVE TRADING MODE - Real money will be used!
          </div>
        )}

        {/* Submit Button */}
        <button
          type="submit"
          disabled={isLoading}
          className={`w-full py-2 rounded font-semibold text-sm transition-colors ${
            tradingMode === 'live'
              ? 'bg-red-600 hover:bg-red-700 text-white border-2 border-red-400'
              : side === 'buy'
              ? 'bg-green-600 hover:bg-green-700 text-white'
              : 'bg-red-600 hover:bg-red-700 text-white'
          } ${isLoading ? 'opacity-50 cursor-not-allowed' : ''}`}
        >
          {isLoading
            ? 'Placing Order...'
            : tradingMode === 'live'
            ? `🔴 LIVE ${side.toUpperCase()} ${symbol}`
            : `${side.toUpperCase()} ${symbol}`}
        </button>

        {/* Paper Trading Info */}
        {tradingMode === 'paper' && (
          <div className="text-xs text-gray-500 text-center italic">
            Paper trading - No real money used
          </div>
        )}
      </form>
    </div>
  );
}
