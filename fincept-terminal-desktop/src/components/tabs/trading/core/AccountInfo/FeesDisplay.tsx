/**
 * FeesDisplay - Shows trading fees for current exchange
 */

import React from 'react';
import { DollarSign, Info } from 'lucide-react';
import { useFees } from '../../hooks/useAccountInfo';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

export function FeesDisplay() {
  const { fees, isLoading } = useFees();
  const { activeBroker } = useBrokerContext();

  if (isLoading || !fees) {
    return (
      <div className="bg-gray-900 border border-gray-800 rounded p-4">
        <div className="flex items-center gap-2 mb-3">
          <DollarSign className="w-4 h-4 text-orange-500" />
          <span className="text-sm font-medium text-gray-300">Trading Fees</span>
        </div>
        {isLoading ? (
          <div className="flex items-center justify-center py-4">
            <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-orange-500"></div>
          </div>
        ) : (
          <div className="text-xs text-gray-500">No fee information available</div>
        )}
      </div>
    );
  }

  return (
    <div className="bg-gray-900 border border-gray-800 rounded p-4">
      {/* Header */}
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center gap-2">
          <DollarSign className="w-4 h-4 text-orange-500" />
          <span className="text-sm font-medium text-gray-300">Trading Fees</span>
        </div>
        <span className="text-xs text-gray-500 uppercase">{activeBroker}</span>
      </div>

      {/* Fee Structure */}
      <div className="space-y-2">
        {/* Maker Fee */}
        <div className="flex items-center justify-between p-2 bg-black/30 rounded">
          <div className="flex items-center gap-2">
            <div className="text-xs text-gray-400">Maker Fee</div>
            <div className="group relative">
              <Info className="w-3 h-3 text-gray-600 cursor-help" />
              <div className="hidden group-hover:block absolute bottom-full left-0 mb-2 w-48 p-2 bg-gray-800 border border-gray-700 rounded text-xs text-gray-300 z-10">
                Fee for adding liquidity to the order book (limit orders that don't immediately execute)
              </div>
            </div>
          </div>
          <div className="text-sm font-mono text-green-500">{fees.makerPercent}</div>
        </div>

        {/* Taker Fee */}
        <div className="flex items-center justify-between p-2 bg-black/30 rounded">
          <div className="flex items-center gap-2">
            <div className="text-xs text-gray-400">Taker Fee</div>
            <div className="group relative">
              <Info className="w-3 h-3 text-gray-600 cursor-help" />
              <div className="hidden group-hover:block absolute bottom-full left-0 mb-2 w-48 p-2 bg-gray-800 border border-gray-700 rounded text-xs text-gray-300 z-10">
                Fee for taking liquidity from the order book (market orders and limit orders that execute immediately)
              </div>
            </div>
          </div>
          <div className="text-sm font-mono text-orange-500">{fees.takerPercent}</div>
        </div>
      </div>

      {/* Example Calculation */}
      <div className="mt-3 pt-3 border-t border-gray-800">
        <div className="text-xs text-gray-500 mb-2">Example: $10,000 trade</div>
        <div className="grid grid-cols-2 gap-2 text-xs">
          <div className="flex justify-between">
            <span className="text-gray-400">Maker:</span>
            <span className="text-green-500 font-mono">${(10000 * fees.maker).toFixed(2)}</span>
          </div>
          <div className="flex justify-between">
            <span className="text-gray-400">Taker:</span>
            <span className="text-orange-500 font-mono">${(10000 * fees.taker).toFixed(2)}</span>
          </div>
        </div>
      </div>

      {/* Savings Tip */}
      <div className="mt-3 p-2 bg-blue-500/10 border border-blue-500/20 rounded">
        <div className="text-xs text-blue-400">
          💡 Tip: Use limit orders to pay lower maker fees
        </div>
      </div>
    </div>
  );
}
