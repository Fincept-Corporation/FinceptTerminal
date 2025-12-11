/**
 * FuturesPanel - Kraken futures trading interface
 */

import React, { useState, useCallback, useEffect } from 'react';
import { TrendingUp, Activity, AlertTriangle, ExternalLink, Loader } from 'lucide-react';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';

interface FuturesMarket {
  symbol: string;
  baseAsset: string;
  quoteAsset: string;
  price: number;
  change24h: number;
  volume24h: number;
  openInterest: number;
  fundingRate: number;
  nextFunding: string;
  maxLeverage: number;
}

export function FuturesPanel() {
  const { activeBroker, activeAdapter } = useBrokerContext();
  const [futuresMarkets, setFuturesMarkets] = useState<FuturesMarket[]>([]);
  const [selectedMarket, setSelectedMarket] = useState<FuturesMarket | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  // Fetch futures markets on mount
  useEffect(() => {
    if (activeBroker === 'kraken') {
      fetchFuturesMarkets();
    }
  }, [activeBroker]);

  const fetchFuturesMarkets = useCallback(async () => {
    setIsLoading(true);
    setError(null);
    try {
      // Mock futures data (in production, fetch from Kraken Futures API)
      const mockMarkets: FuturesMarket[] = [
        {
          symbol: 'PF_BTCUSD',
          baseAsset: 'BTC',
          quoteAsset: 'USD',
          price: 43250.5,
          change24h: 2.35,
          volume24h: 1234567890,
          openInterest: 567890000,
          fundingRate: 0.0001,
          nextFunding: '4h 23m',
          maxLeverage: 50,
        },
        {
          symbol: 'PF_ETHUSD',
          baseAsset: 'ETH',
          quoteAsset: 'USD',
          price: 2345.67,
          change24h: -1.23,
          volume24h: 456789012,
          openInterest: 234567000,
          fundingRate: 0.00005,
          nextFunding: '4h 23m',
          maxLeverage: 50,
        },
        {
          symbol: 'PF_SOLUSD',
          baseAsset: 'SOL',
          quoteAsset: 'USD',
          price: 98.45,
          change24h: 5.67,
          volume24h: 123456789,
          openInterest: 45678900,
          fundingRate: 0.00015,
          nextFunding: '4h 23m',
          maxLeverage: 25,
        },
        {
          symbol: 'PF_XRPUSD',
          baseAsset: 'XRP',
          quoteAsset: 'USD',
          price: 0.5234,
          change24h: 3.45,
          volume24h: 89012345,
          openInterest: 23456789,
          fundingRate: -0.00002,
          nextFunding: '4h 23m',
          maxLeverage: 20,
        },
      ];

      setFuturesMarkets(mockMarkets);
      if (mockMarkets.length > 0 && !selectedMarket) {
        setSelectedMarket(mockMarkets[0]);
      }
    } catch (err) {
      setError('Failed to fetch futures markets');
      console.error('Futures fetch error:', err);
    } finally {
      setIsLoading(false);
    }
  }, [activeAdapter, selectedMarket]);

  const formatNumber = (num: number, decimals: number = 2): string => {
    return num.toLocaleString('en-US', {
      minimumFractionDigits: decimals,
      maximumFractionDigits: decimals,
    });
  };

  const formatLargeNumber = (num: number): string => {
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(2)}M`;
    if (num >= 1e3) return `$${(num / 1e3).toFixed(2)}K`;
    return `$${num.toFixed(2)}`;
  };

  if (activeBroker !== 'kraken') {
    return null;
  }

  if (isLoading) {
    return (
      <div className="bg-gray-900 border border-gray-800 rounded p-6">
        <div className="flex items-center justify-center py-8">
          <Loader className="w-6 h-6 text-blue-500 animate-spin" />
        </div>
      </div>
    );
  }

  return (
    <div className="bg-gray-900 border border-gray-800 rounded p-6">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div className="flex items-center gap-2">
          <Activity className="w-5 h-5 text-purple-500" />
          <span className="text-lg font-semibold text-white">Kraken Futures</span>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={fetchFuturesMarkets}
            className="px-3 py-1 text-xs text-blue-400 hover:text-blue-300 border border-blue-500/30 hover:border-blue-400/50 rounded transition"
          >
            Refresh
          </button>
          <a
            href="https://futures.kraken.com"
            target="_blank"
            rel="noopener noreferrer"
            className="px-3 py-1 text-xs text-gray-400 hover:text-gray-300 border border-gray-700 hover:border-gray-600 rounded transition flex items-center gap-1"
          >
            <ExternalLink className="w-3 h-3" />
            Kraken Futures
          </a>
        </div>
      </div>

      {/* Warning Banner */}
      <div className="mb-6 p-3 bg-yellow-500/10 border border-yellow-500/20 rounded flex items-start gap-2">
        <AlertTriangle className="w-4 h-4 text-yellow-500 flex-shrink-0 mt-0.5" />
        <div className="text-xs text-yellow-400">
          <strong>Futures Trading Risk Warning:</strong> Futures trading involves high risk and
          leverage. Only trade with funds you can afford to lose. Requires separate Kraken Futures
          account.
        </div>
      </div>

      {/* Markets Grid */}
      {futuresMarkets.length === 0 ? (
        <div className="text-center py-8 text-gray-500">
          <Activity className="w-12 h-12 mx-auto mb-3 text-gray-600" />
          <div className="text-sm">No futures markets available</div>
        </div>
      ) : (
        <div className="space-y-4">
          {/* Market Selector */}
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
            {futuresMarkets.map((market) => (
              <button
                key={market.symbol}
                onClick={() => setSelectedMarket(market)}
                className={`p-3 rounded border-2 transition ${
                  selectedMarket?.symbol === market.symbol
                    ? 'border-purple-500 bg-purple-500/10'
                    : 'border-gray-700 bg-gray-800/50 hover:border-gray-600'
                }`}
              >
                <div className="text-left">
                  <div className="text-white font-semibold mb-1">{market.baseAsset}/USD</div>
                  <div className="text-sm text-gray-400">${formatNumber(market.price)}</div>
                  <div
                    className={`text-xs font-semibold ${
                      market.change24h >= 0 ? 'text-green-500' : 'text-red-500'
                    }`}
                  >
                    {market.change24h >= 0 ? '+' : ''}
                    {market.change24h.toFixed(2)}%
                  </div>
                </div>
              </button>
            ))}
          </div>

          {/* Selected Market Details */}
          {selectedMarket && (
            <div className="border-t border-gray-800 pt-6 space-y-4">
              {/* Market Stats */}
              <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
                {/* Price */}
                <div className="p-4 bg-gray-800/50 border border-gray-700 rounded">
                  <div className="text-xs text-gray-400 mb-1">Mark Price</div>
                  <div className="text-2xl font-bold text-white">
                    ${formatNumber(selectedMarket.price)}
                  </div>
                  <div
                    className={`text-sm font-semibold ${
                      selectedMarket.change24h >= 0 ? 'text-green-500' : 'text-red-500'
                    }`}
                  >
                    {selectedMarket.change24h >= 0 ? '+' : ''}
                    {selectedMarket.change24h.toFixed(2)}% 24h
                  </div>
                </div>

                {/* Volume */}
                <div className="p-4 bg-gray-800/50 border border-gray-700 rounded">
                  <div className="text-xs text-gray-400 mb-1">24h Volume</div>
                  <div className="text-lg font-bold text-white">
                    {formatLargeNumber(selectedMarket.volume24h)}
                  </div>
                  <div className="text-xs text-gray-500">Trading Volume</div>
                </div>

                {/* Open Interest */}
                <div className="p-4 bg-gray-800/50 border border-gray-700 rounded">
                  <div className="text-xs text-gray-400 mb-1">Open Interest</div>
                  <div className="text-lg font-bold text-white">
                    {formatLargeNumber(selectedMarket.openInterest)}
                  </div>
                  <div className="text-xs text-gray-500">Total Positions</div>
                </div>

                {/* Funding Rate */}
                <div className="p-4 bg-gray-800/50 border border-gray-700 rounded">
                  <div className="text-xs text-gray-400 mb-1">Funding Rate</div>
                  <div
                    className={`text-lg font-bold ${
                      selectedMarket.fundingRate >= 0 ? 'text-green-500' : 'text-red-500'
                    }`}
                  >
                    {(selectedMarket.fundingRate * 100).toFixed(4)}%
                  </div>
                  <div className="text-xs text-gray-500">Every 8 hours</div>
                </div>

                {/* Next Funding */}
                <div className="p-4 bg-gray-800/50 border border-gray-700 rounded">
                  <div className="text-xs text-gray-400 mb-1">Next Funding</div>
                  <div className="text-lg font-bold text-white">{selectedMarket.nextFunding}</div>
                  <div className="text-xs text-gray-500">Countdown</div>
                </div>

                {/* Max Leverage */}
                <div className="p-4 bg-gray-800/50 border border-gray-700 rounded">
                  <div className="text-xs text-gray-400 mb-1">Max Leverage</div>
                  <div className="text-lg font-bold text-orange-500">
                    {selectedMarket.maxLeverage}x
                  </div>
                  <div className="text-xs text-gray-500">Available</div>
                </div>
              </div>

              {/* Contract Details */}
              <div className="p-4 bg-blue-500/10 border border-blue-500/20 rounded">
                <div className="text-sm font-semibold text-blue-400 mb-2">Contract Details</div>
                <div className="grid grid-cols-2 gap-3 text-xs">
                  <div>
                    <span className="text-gray-400">Contract Type:</span>
                    <span className="text-white ml-2">Perpetual</span>
                  </div>
                  <div>
                    <span className="text-gray-400">Settlement:</span>
                    <span className="text-white ml-2">USD</span>
                  </div>
                  <div>
                    <span className="text-gray-400">Contract Size:</span>
                    <span className="text-white ml-2">1 {selectedMarket.baseAsset}</span>
                  </div>
                  <div>
                    <span className="text-gray-400">Min Order:</span>
                    <span className="text-white ml-2">0.0001 {selectedMarket.baseAsset}</span>
                  </div>
                </div>
              </div>

              {/* Trading Info */}
              <div className="p-4 bg-gray-800/30 border border-gray-700 rounded">
                <div className="text-xs text-gray-400 space-y-2">
                  <div>
                    <strong className="text-gray-300">How to trade:</strong> Futures trading on
                    Kraken requires a separate Kraken Futures account. Visit{' '}
                    <a
                      href="https://futures.kraken.com"
                      target="_blank"
                      rel="noopener noreferrer"
                      className="text-blue-400 hover:text-blue-300 underline"
                    >
                      futures.kraken.com
                    </a>{' '}
                    to create an account and start trading.
                  </div>
                  <div>
                    <strong className="text-gray-300">API Integration:</strong> To trade futures
                    directly from this terminal, configure Kraken Futures API credentials in
                    Settings.
                  </div>
                </div>
              </div>

              {/* Action Button */}
              <div className="flex gap-3">
                <a
                  href={`https://futures.kraken.com/trade/${selectedMarket.symbol}`}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="flex-1 px-4 py-3 bg-purple-600 hover:bg-purple-700 text-white font-semibold rounded transition flex items-center justify-center gap-2"
                >
                  <ExternalLink className="w-4 h-4" />
                  Trade on Kraken Futures
                </a>
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
