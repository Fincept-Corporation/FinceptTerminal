/**
 * PortfolioAggregator - Unified view of balances across all exchanges
 */

import React from 'react';
import { Wallet, TrendingUp, RefreshCw, Loader, PieChart } from 'lucide-react';
import { useCrossExchangePortfolio } from '../hooks/useCrossExchange';

export function PortfolioAggregator() {
  const { aggregatedBalances, totalPortfolioValue, isLoading, error, refresh } =
    useCrossExchangePortfolio();

  const formatCurrency = (value: number): string => {
    return value.toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
  };

  const formatCrypto = (value: number, decimals: number = 4): string => {
    return value.toLocaleString('en-US', {
      minimumFractionDigits: decimals,
      maximumFractionDigits: decimals,
    });
  };

  if (isLoading) {
    return (
      <div className="bg-gray-900 border border-gray-800 rounded p-6">
        <div className="flex items-center justify-center py-8">
          <Loader className="w-6 h-6 text-blue-500 animate-spin" />
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="bg-gray-900 border border-gray-800 rounded p-6">
        <div className="text-center py-8 text-red-500">{error}</div>
      </div>
    );
  }

  return (
    <div className="bg-gray-900 border border-gray-800 rounded p-6">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div className="flex items-center gap-2">
          <PieChart className="w-5 h-5 text-blue-500" />
          <span className="text-lg font-semibold text-white">Cross-Exchange Portfolio</span>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={refresh}
            className="px-3 py-1 text-xs text-blue-400 hover:text-blue-300 border border-blue-500/30 hover:border-blue-400/50 rounded transition flex items-center gap-1"
          >
            <RefreshCw className="w-3 h-3" />
            Refresh
          </button>
        </div>
      </div>

      {/* Total Portfolio Value */}
      <div className="mb-6 p-4 bg-gradient-to-r from-blue-500/10 to-purple-500/10 border border-blue-500/20 rounded">
        <div className="text-xs text-gray-400 mb-1">Total Portfolio Value</div>
        <div className="flex items-center gap-3">
          <div className="text-3xl font-bold text-white">${formatCurrency(totalPortfolioValue)}</div>
          <div className="flex items-center gap-1 text-green-500 text-sm">
            <TrendingUp className="w-4 h-4" />
            <span>+2.45%</span>
          </div>
        </div>
        <div className="text-xs text-gray-500 mt-1">Across {aggregatedBalances.length} assets</div>
      </div>

      {/* Aggregated Balances */}
      <div className="space-y-3">
        <div className="text-sm font-semibold text-gray-400 mb-3">Asset Breakdown</div>

        {aggregatedBalances.length === 0 ? (
          <div className="text-center py-8 text-gray-500">
            <Wallet className="w-12 h-12 mx-auto mb-3 text-gray-600" />
            <div className="text-sm">No balances found</div>
          </div>
        ) : (
          aggregatedBalances
            .sort((a, b) => b.totalUsdValue - a.totalUsdValue)
            .map((balance) => (
              <div
                key={balance.currency}
                className="p-4 bg-gray-800/50 border border-gray-700 rounded hover:border-gray-600 transition"
              >
                {/* Currency Header */}
                <div className="flex items-center justify-between mb-3">
                  <div className="flex items-center gap-3">
                    <div className="w-10 h-10 bg-gradient-to-br from-blue-500 to-purple-500 rounded-full flex items-center justify-center text-white font-bold text-sm">
                      {balance.currency.substring(0, 2)}
                    </div>
                    <div>
                      <div className="text-white font-semibold">{balance.currency}</div>
                      <div className="text-xs text-gray-500">
                        {formatCrypto(balance.totalAmount)} {balance.currency}
                      </div>
                    </div>
                  </div>
                  <div className="text-right">
                    <div className="text-white font-semibold">
                      ${formatCurrency(balance.totalUsdValue)}
                    </div>
                    <div className="text-xs text-gray-500">
                      {((balance.totalUsdValue / totalPortfolioValue) * 100).toFixed(2)}%
                    </div>
                  </div>
                </div>

                {/* Exchange Breakdown */}
                <div className="space-y-2 pl-13">
                  {balance.exchanges.map((exch, idx) => (
                    <div
                      key={idx}
                      className="flex items-center justify-between text-xs p-2 bg-black/20 rounded"
                    >
                      <div className="flex items-center gap-2">
                        <span className="px-2 py-0.5 bg-gray-700 rounded text-gray-300 capitalize">
                          {exch.exchange}
                        </span>
                        <span className="text-gray-400">
                          {formatCrypto(exch.amount)} {balance.currency}
                        </span>
                      </div>
                      <span className="text-gray-300">${formatCurrency(exch.usdValue)}</span>
                    </div>
                  ))}
                </div>

                {/* Progress Bar */}
                <div className="mt-3 h-2 bg-gray-700 rounded-full overflow-hidden">
                  <div
                    className="h-full bg-gradient-to-r from-blue-500 to-purple-500 transition-all"
                    style={{
                      width: `${(balance.totalUsdValue / totalPortfolioValue) * 100}%`,
                    }}
                  ></div>
                </div>
              </div>
            ))
        )}
      </div>

      {/* Info Note */}
      <div className="mt-6 p-3 bg-blue-500/10 border border-blue-500/20 rounded">
        <div className="text-xs text-blue-400">
          <strong>Note:</strong> This view aggregates balances from all connected exchanges. Values
          are updated every 30 seconds. Exchange-specific details are shown for each asset.
        </div>
      </div>
    </div>
  );
}
