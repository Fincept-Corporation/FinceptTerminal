// Main component for Momentum Indicators Analysis

import React, { useState, useEffect, useCallback } from 'react';
import { Search, RefreshCw, Activity } from 'lucide-react';
import { SimpleCandlestickChart } from './components/SimpleCandlestickChart';
import { IndicatorControls } from './components/IndicatorControls';
import {
  CandleData,
  MomentumIndicatorParams,
  MomentumIndicatorResult,
  DEFAULT_MOMENTUM_PARAMS,
  IndicatorType,
  TimeFrame,
  TIME_FRAMES,
  TimeFrameConfig,
} from './types';
import { fetchHistoricalData, calculateAllMomentumIndicators } from './services/momentumIndicators';
import { aggregateSignals } from './utils/signalAnalyzer';

export const MomentumAnalysis: React.FC = () => {
  const [symbol, setSymbol] = useState<string>('AAPL');
  const [searchInput, setSearchInput] = useState<string>('AAPL');
  const [timeFrame, setTimeFrame] = useState<TimeFrameConfig>(TIME_FRAMES[5]); // Default: 1 year
  const [candleData, setCandleData] = useState<CandleData[]>([]);
  const [indicators, setIndicators] = useState<MomentumIndicatorResult>({});
  const [params, setParams] = useState<MomentumIndicatorParams>(DEFAULT_MOMENTUM_PARAMS);
  const [activeIndicators, setActiveIndicators] = useState<Set<IndicatorType>>(
    new Set(['rsi', 'macd', 'stochastic'])
  );
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<string | null>(null);

  // Fetch data and calculate indicators
  const fetchData = useCallback(async () => {
    if (!symbol) return;

    setLoading(true);
    setError(null);

    try {
      // Fetch historical price data
      const priceData = await fetchHistoricalData(symbol, timeFrame.value, timeFrame.interval);
      setCandleData(priceData);

      // Calculate only active indicators
      const activeParams: MomentumIndicatorParams = {};
      activeIndicators.forEach((indicator) => {
        if (params[indicator]) {
          (activeParams as any)[indicator] = params[indicator];
        }
      });

      if (Object.keys(activeParams).length > 0) {
        const indicatorResults = await calculateAllMomentumIndicators(
          symbol,
          activeParams,
          timeFrame.value,
          timeFrame.interval
        );
        setIndicators(indicatorResults);
      } else {
        setIndicators({});
      }
    } catch (err) {
      console.error('Error fetching data:', err);
      setError(err instanceof Error ? err.message : 'Failed to fetch data');
    } finally {
      setLoading(false);
    }
  }, [symbol, timeFrame, params, activeIndicators]);

  // Initial load
  useEffect(() => {
    fetchData();
  }, []);

  // Handle search
  const handleSearch = () => {
    if (searchInput.trim()) {
      setSymbol(searchInput.trim().toUpperCase());
    }
  };

  // Handle parameter changes
  const handleParamsChange = (newParams: MomentumIndicatorParams) => {
    setParams(newParams);
  };

  // Handle indicator toggle
  const handleToggleIndicator = (indicator: IndicatorType) => {
    const newActiveIndicators = new Set(activeIndicators);
    if (newActiveIndicators.has(indicator)) {
      newActiveIndicators.delete(indicator);
    } else {
      newActiveIndicators.add(indicator);
    }
    setActiveIndicators(newActiveIndicators);
  };

  // Handle refresh
  const handleRefresh = () => {
    fetchData();
  };

  // Calculate aggregate signal
  const getAggregateSignal = () => {
    const signals = Object.values(indicators)
      .filter((ind) => ind && 'signal' in ind)
      .map((ind) => ({
        signal: ind.signal,
        strength: 70, // Default strength
        reason: '',
      }));

    return aggregateSignals(signals);
  };

  const aggregateSignal = indicators && Object.keys(indicators).length > 0 ? getAggregateSignal() : null;

  return (
    <div className="h-full bg-[#0a0a0a] text-white overflow-hidden flex flex-col">
      {/* Header */}
      <div className="border-b border-[#333333] bg-[#0f0f0f] p-4">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <Activity className="w-6 h-6 text-orange-500" />
            <h2 className="text-2xl font-bold">Momentum Indicators Analysis</h2>
          </div>
          {aggregateSignal && (
            <div className="flex items-center gap-2">
              <span className="text-sm text-gray-400">Overall Signal:</span>
              <span
                className={`px-3 py-1 rounded font-semibold ${
                  aggregateSignal.signal === 'BUY'
                    ? 'bg-green-500/20 text-green-400'
                    : aggregateSignal.signal === 'SELL'
                    ? 'bg-red-500/20 text-red-400'
                    : 'bg-gray-500/20 text-gray-400'
                }`}
              >
                {aggregateSignal.signal}
              </span>
              <span className="text-xs text-gray-400">({aggregateSignal.strength.toFixed(0)}% confidence)</span>
            </div>
          )}
        </div>

        {/* Search and Controls */}
        <div className="flex items-center gap-4">
          <div className="flex-1 flex items-center gap-2">
            <div className="relative flex-1 max-w-md">
              <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 w-4 h-4 text-gray-400" />
              <input
                type="text"
                value={searchInput}
                onChange={(e) => setSearchInput(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
                placeholder="Enter stock symbol (e.g., AAPL)"
                className="w-full bg-[#1a1a1a] border border-[#333333] rounded-lg pl-10 pr-4 py-2 text-white placeholder-gray-500 focus:outline-none focus:ring-2 focus:ring-orange-500"
              />
            </div>
            <button
              onClick={handleSearch}
              disabled={loading}
              className="px-4 py-2 bg-orange-500 hover:bg-orange-600 disabled:bg-gray-600 rounded-lg font-semibold transition-colors"
            >
              Search
            </button>
          </div>

          {/* Timeframe Selector */}
          <select
            value={TIME_FRAMES.findIndex((tf) => tf.value === timeFrame.value)}
            onChange={(e) => setTimeFrame(TIME_FRAMES[parseInt(e.target.value)])}
            className="bg-[#1a1a1a] border border-[#333333] rounded-lg px-4 py-2 text-white focus:outline-none focus:ring-2 focus:ring-orange-500"
          >
            {TIME_FRAMES.map((tf, index) => (
              <option key={tf.value} value={index}>
                {tf.label}
              </option>
            ))}
          </select>

          <button
            onClick={handleRefresh}
            disabled={loading}
            className="p-2 bg-[#1a1a1a] hover:bg-[#222222] disabled:bg-gray-600 border border-[#333333] rounded-lg transition-colors"
            title="Refresh data"
          >
            <RefreshCw className={`w-5 h-5 ${loading ? 'animate-spin' : ''}`} />
          </button>
        </div>
      </div>

      {/* Content */}
      <div className="flex-1 overflow-hidden flex">
        {/* Chart Area */}
        <div className="flex-1 overflow-auto p-4">
          {error && (
            <div className="mb-4 p-4 bg-red-500/20 border border-red-500/30 rounded-lg text-red-400">
              <p className="font-semibold">Error:</p>
              <p className="text-sm">{error}</p>
            </div>
          )}

          {loading && (
            <div className="flex items-center justify-center h-64">
              <div className="text-center">
                <RefreshCw className="w-8 h-8 animate-spin text-orange-500 mx-auto mb-2" />
                <p className="text-gray-400">Loading data for {symbol}...</p>
              </div>
            </div>
          )}

          {!loading && !error && candleData.length > 0 && (
            <div className="space-y-4">
              <div className="bg-[#0f0f0f] border border-[#333333] rounded-lg p-4">
                <div className="flex items-center justify-between mb-4">
                  <h3 className="text-lg font-semibold">
                    {symbol} - {timeFrame.label}
                  </h3>
                  <div className="text-sm text-gray-400">
                    {candleData.length} candles | Interval: {timeFrame.interval}
                  </div>
                </div>
                <SimpleCandlestickChart
                  data={candleData}
                  indicators={indicators}
                  activeIndicators={activeIndicators}
                  height={700}
                />
              </div>
            </div>
          )}

          {!loading && !error && candleData.length === 0 && (
            <div className="flex items-center justify-center h-64">
              <div className="text-center text-gray-400">
                <Activity className="w-12 h-12 mx-auto mb-2 opacity-50" />
                <p>Enter a stock symbol and click Search to analyze</p>
              </div>
            </div>
          )}
        </div>

        {/* Indicator Controls Sidebar */}
        <div className="w-96 border-l border-[#333333] bg-[#0f0f0f] overflow-y-auto p-4">
          <IndicatorControls
            params={params}
            results={indicators}
            activeIndicators={activeIndicators}
            onParamsChange={handleParamsChange}
            onToggleIndicator={handleToggleIndicator}
          />

          <div className="mt-6 p-4 bg-[#1a1a1a] border border-[#333333] rounded-lg">
            <h4 className="font-semibold mb-2">How to use:</h4>
            <ul className="text-sm text-gray-400 space-y-1">
              <li>• Check indicators to enable them on the chart</li>
              <li>• Click on an indicator to expand and adjust parameters</li>
              <li>• Changes update instantly on the chart</li>
              <li>• Use the reset button to restore default values</li>
              <li>• Green = BUY signal, Red = SELL signal</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
};
