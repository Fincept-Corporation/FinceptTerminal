// QuickOrderButtons.tsx - One-click trading with preset amounts
import React, { useState } from 'react';

interface QuickOrderButtonsProps {
  symbol: string;
  currentPrice?: number;
  onPlaceOrder: (order: {
    symbol: string;
    side: 'buy' | 'sell';
    type: string;
    quantity: number;
    price?: number;
  }) => Promise<void>;
}

const PRESET_AMOUNTS = [0.01, 0.05, 0.1, 0.25, 0.5, 1.0];

export function QuickOrderButtons({ symbol, currentPrice, onPlaceOrder }: QuickOrderButtonsProps) {
  const [isProcessing, setIsProcessing] = useState<string>('');

  const handleQuickOrder = async (side: 'buy' | 'sell', quantity: number) => {
    const key = `${side}-${quantity}`;
    setIsProcessing(key);

    try {
      await onPlaceOrder({
        symbol,
        side,
        type: 'market',
        quantity,
      });
    } catch (error) {
      console.error('[QuickOrderButtons] Failed:', error);
    } finally {
      setIsProcessing('');
    }
  };

  return (
    <div className="bg-[#0d0d0d] border-t border-gray-800 p-4">
      <h3 className="text-sm font-bold text-gray-400 mb-3">QUICK ORDERS</h3>

      {/* Buy Buttons */}
      <div className="mb-3">
        <div className="text-xs text-gray-500 mb-1">Quick Buy</div>
        <div className="grid grid-cols-3 gap-1">
          {PRESET_AMOUNTS.map((amount) => {
            const key = `buy-${amount}`;
            const isLoading = isProcessing === key;
            const cost = currentPrice ? (amount * currentPrice).toFixed(2) : '0.00';

            return (
              <button
                key={key}
                onClick={() => handleQuickOrder('buy', amount)}
                disabled={isLoading || !currentPrice}
                className="bg-green-900/30 hover:bg-green-700/50 border border-green-600 text-green-400 rounded px-2 py-1.5 text-xs font-semibold transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              >
                {isLoading ? (
                  <span className="animate-pulse">...</span>
                ) : (
                  <>
                    {amount}
                    <div className="text-[9px] text-green-500/70">${cost}</div>
                  </>
                )}
              </button>
            );
          })}
        </div>
      </div>

      {/* Sell Buttons */}
      <div>
        <div className="text-xs text-gray-500 mb-1">Quick Sell</div>
        <div className="grid grid-cols-3 gap-1">
          {PRESET_AMOUNTS.map((amount) => {
            const key = `sell-${amount}`;
            const isLoading = isProcessing === key;
            const proceeds = currentPrice ? (amount * currentPrice).toFixed(2) : '0.00';

            return (
              <button
                key={key}
                onClick={() => handleQuickOrder('sell', amount)}
                disabled={isLoading || !currentPrice}
                className="bg-red-900/30 hover:bg-red-700/50 border border-red-600 text-red-400 rounded px-2 py-1.5 text-xs font-semibold transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              >
                {isLoading ? (
                  <span className="animate-pulse">...</span>
                ) : (
                  <>
                    {amount}
                    <div className="text-[9px] text-red-500/70">${proceeds}</div>
                  </>
                )}
              </button>
            );
          })}
        </div>
      </div>

      <div className="mt-3 text-[10px] text-gray-600">
        ⚠️ Market orders execute instantly
      </div>
    </div>
  );
}
