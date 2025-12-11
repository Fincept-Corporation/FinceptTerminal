/**
 * ArbitrageDetector - Real-time arbitrage opportunity detection across exchanges
 */

import React from 'react';
import { Zap, RefreshCw, Loader, TrendingUp, Clock, AlertTriangle, ExternalLink } from 'lucide-react';
import { useArbitrageDetection } from '../hooks/useCrossExchange';

export function ArbitrageDetector() {
  const { opportunities, isScanning, lastScanTime, scan } = useArbitrageDetection();

  const formatPrice = (price: number): string => {
    return price.toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
  };

  const formatTime = (timestamp: number): string => {
    const seconds = Math.floor((Date.now() - timestamp) / 1000);
    if (seconds < 60) return `${seconds}s ago`;
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `${minutes}m ago`;
    const hours = Math.floor(minutes / 60);
    return `${hours}h ago`;
  };

  return (
    <div className="bg-gray-900 border border-gray-800 rounded p-6">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div className="flex items-center gap-2">
          <Zap className="w-5 h-5 text-yellow-500" />
          <span className="text-lg font-semibold text-white">Arbitrage Opportunities</span>
        </div>
        <div className="flex items-center gap-3">
          {lastScanTime && (
            <div className="flex items-center gap-1 text-xs text-gray-500">
              <Clock className="w-3 h-3" />
              {formatTime(lastScanTime)}
            </div>
          )}
          <button
            onClick={scan}
            disabled={isScanning}
            className="px-3 py-1 text-xs text-yellow-400 hover:text-yellow-300 border border-yellow-500/30 hover:border-yellow-400/50 rounded transition flex items-center gap-1 disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <RefreshCw className={`w-3 h-3 ${isScanning ? 'animate-spin' : ''}`} />
            {isScanning ? 'Scanning...' : 'Scan'}
          </button>
        </div>
      </div>

      {/* Risk Warning */}
      <div className="mb-6 p-3 bg-yellow-500/10 border border-yellow-500/20 rounded flex items-start gap-2">
        <AlertTriangle className="w-4 h-4 text-yellow-500 flex-shrink-0 mt-0.5" />
        <div className="text-xs text-yellow-400">
          <strong>Risk Warning:</strong> Arbitrage opportunities include trading fees, withdrawal
          fees, and execution risk. Always calculate net profit after all costs. Prices can change
          rapidly.
        </div>
      </div>

      {/* Scanning Status */}
      {isScanning && (
        <div className="mb-4 p-3 bg-blue-500/10 border border-blue-500/20 rounded flex items-center gap-2">
          <Loader className="w-4 h-4 text-blue-500 animate-spin" />
          <span className="text-sm text-blue-400">Scanning for arbitrage opportunities...</span>
        </div>
      )}

      {/* Opportunities List */}
      <div className="space-y-3">
        {opportunities.length === 0 ? (
          <div className="text-center py-8 text-gray-500">
            <Zap className="w-12 h-12 mx-auto mb-3 text-gray-600" />
            <div className="text-sm">No arbitrage opportunities found</div>
            <div className="text-xs mt-1">Spread must be &gt; 0.2% to be considered viable</div>
          </div>
        ) : (
          opportunities.map((opp, idx) => (
            <div
              key={idx}
              className="p-4 bg-gradient-to-r from-yellow-500/10 to-orange-500/10 border-2 border-yellow-500/30 rounded hover:border-yellow-400/50 transition"
            >
              {/* Opportunity Header */}
              <div className="flex items-center justify-between mb-3">
                <div className="flex items-center gap-3">
                  <div className="flex items-center gap-1 text-yellow-500">
                    <Zap className="w-5 h-5 fill-current" />
                    <span className="text-lg font-bold text-white">{opp.symbol}</span>
                  </div>
                  <div className="px-2 py-0.5 bg-yellow-500/20 border border-yellow-500/30 rounded text-xs text-yellow-400 font-semibold">
                    {opp.spreadPercent.toFixed(2)}% Spread
                  </div>
                </div>
                <div className="text-right">
                  <div className="text-sm text-gray-400">Potential Profit</div>
                  <div className="text-xl font-bold text-green-500">
                    ${opp.potentialProfit.toFixed(2)}
                  </div>
                </div>
              </div>

              {/* Trade Details */}
              <div className="grid grid-cols-2 gap-4 mb-3">
                {/* Buy Side */}
                <div className="p-3 bg-green-500/10 border border-green-500/20 rounded">
                  <div className="text-xs text-gray-400 mb-1">BUY ON</div>
                  <div className="text-sm font-semibold text-white capitalize mb-2">
                    {opp.buyExchange}
                  </div>
                  <div className="flex items-center gap-2">
                    <TrendingUp className="w-4 h-4 text-green-500" />
                    <span className="text-lg font-bold text-green-500">
                      ${formatPrice(opp.buyPrice)}
                    </span>
                  </div>
                </div>

                {/* Sell Side */}
                <div className="p-3 bg-red-500/10 border border-red-500/20 rounded">
                  <div className="text-xs text-gray-400 mb-1">SELL ON</div>
                  <div className="text-sm font-semibold text-white capitalize mb-2">
                    {opp.sellExchange}
                  </div>
                  <div className="flex items-center gap-2">
                    <TrendingUp className="w-4 h-4 text-red-500 rotate-180" />
                    <span className="text-lg font-bold text-red-500">
                      ${formatPrice(opp.sellPrice)}
                    </span>
                  </div>
                </div>
              </div>

              {/* Spread Info */}
              <div className="p-2 bg-black/30 rounded mb-3">
                <div className="flex items-center justify-between text-xs">
                  <span className="text-gray-400">Price Difference:</span>
                  <span className="text-white font-semibold">${opp.spread.toFixed(2)}</span>
                </div>
              </div>

              {/* Action Buttons */}
              <div className="flex gap-2">
                <button className="flex-1 px-3 py-2 bg-green-600 hover:bg-green-700 text-white text-sm font-semibold rounded transition flex items-center justify-center gap-1">
                  <TrendingUp className="w-4 h-4" />
                  Execute Arbitrage
                </button>
                <button className="px-3 py-2 bg-gray-700 hover:bg-gray-600 text-white text-sm rounded transition">
                  <ExternalLink className="w-4 h-4" />
                </button>
              </div>

              {/* Timestamp */}
              <div className="mt-2 text-xs text-gray-500 text-center">
                Detected {formatTime(opp.timestamp)}
              </div>
            </div>
          ))
        )}
      </div>

      {/* Info Section */}
      <div className="mt-6 space-y-2">
        <div className="p-3 bg-gray-800/50 border border-gray-700 rounded">
          <div className="text-xs font-semibold text-gray-300 mb-2">How It Works</div>
          <div className="text-xs text-gray-400 space-y-1">
            <div>
              1. <strong>Buy</strong> the asset on the exchange with the lower price
            </div>
            <div>
              2. <strong>Transfer</strong> the asset to the exchange with the higher price
            </div>
            <div>
              3. <strong>Sell</strong> the asset for profit
            </div>
          </div>
        </div>

        <div className="p-3 bg-blue-500/10 border border-blue-500/20 rounded">
          <div className="text-xs text-blue-400">
            <strong>Cost Considerations:</strong> Factor in trading fees (maker/taker), withdrawal
            fees, network fees, and execution time. The spread must exceed total costs for
            profitable arbitrage.
          </div>
        </div>

        <div className="p-3 bg-purple-500/10 border border-purple-500/20 rounded">
          <div className="text-xs text-purple-400">
            <strong>Auto-Scan:</strong> Opportunities are automatically scanned every 10 seconds.
            Click "Scan" to manually refresh.
          </div>
        </div>
      </div>
    </div>
  );
}
