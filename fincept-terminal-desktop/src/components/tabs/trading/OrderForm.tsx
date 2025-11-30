// File: src/components/tabs/trading/OrderForm.tsx
// Order placement form with all order types

import React, { useState } from 'react';
import type { OrderRequest, OrderType, OrderSide } from '../../../types/trading';

interface OrderFormProps {
  symbol: string;
  currentPrice?: number;
  onPlaceOrder: (order: OrderRequest) => Promise<void>;
  isLoading?: boolean;
}

export function OrderForm({ symbol, currentPrice, onPlaceOrder, isLoading }: OrderFormProps) {
  const [side, setSide] = useState<OrderSide>('buy');
  const [orderType, setOrderType] = useState<OrderType>('market');
  const [quantity, setQuantity] = useState<string>('');
  const [price, setPrice] = useState<string>('');
  const [stopPrice, setStopPrice] = useState<string>('');
  const [error, setError] = useState<string>('');

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

        {/* Order Type */}
        <div>
          <label className="block text-xs text-gray-500 mb-1">Order Type</label>
          <select
            value={orderType}
            onChange={(e) => setOrderType(e.target.value as OrderType)}
            className="w-full bg-gray-800 text-white rounded px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-orange-500"
          >
            <option value="market">Market</option>
            <option value="limit">Limit</option>
            <option value="stop_market">Stop Market</option>
            <option value="stop_limit">Stop Limit</option>
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

        {/* Submit Button */}
        <button
          type="submit"
          disabled={isLoading}
          className={`w-full py-2 rounded font-semibold text-sm transition-colors ${
            side === 'buy'
              ? 'bg-green-600 hover:bg-green-700 text-white'
              : 'bg-red-600 hover:bg-red-700 text-white'
          } ${isLoading ? 'opacity-50 cursor-not-allowed' : ''}`}
        >
          {isLoading ? 'Placing Order...' : `${side.toUpperCase()} ${symbol}`}
        </button>
      </form>
    </div>
  );
}
