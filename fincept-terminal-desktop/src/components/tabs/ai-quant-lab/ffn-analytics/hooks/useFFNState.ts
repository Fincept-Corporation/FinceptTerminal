/**
 * useFFNState Hook
 * Manages all state for the FFN Analytics Panel
 */

import { useState, useEffect, useCallback } from 'react';
import {
  ffnService,
  type PerformanceMetrics,
  type FFNConfig
} from '@/services/aiQuantLab/ffnService';
import { portfolioService, type Portfolio } from '@/services/portfolio/portfolioService';
import { yfinanceService } from '@/services/markets/yfinanceService';
import { DEFAULT_CONFIG, DEFAULT_HISTORICAL_DAYS, DEFAULT_BENCHMARK_SYMBOL } from '../constants';
import type {
  DataSourceType,
  AnalysisMode,
  OptimizationMethod,
  AnalysisResult,
  ExpandedSections,
  FFNState
} from '../types';

export function useFFNState(): FFNState {
  // Status
  const [isFFNAvailable, setIsFFNAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<AnalysisResult | null>(null);
  const [priceInput, setPriceInput] = useState('{}');

  // Data source configuration
  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('symbol');
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioId, setSelectedPortfolioId] = useState<string>('');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [symbolsInput, setSymbolsInput] = useState('AAPL,MSFT,GOOGL');
  const [historicalDays, setHistoricalDays] = useState(DEFAULT_HISTORICAL_DAYS);
  const [priceDataLoading, setPriceDataLoading] = useState(false);

  // Analysis mode
  const [analysisMode, setAnalysisMode] = useState<AnalysisMode>('performance');

  // Portfolio optimization settings
  const [optimizationMethod, setOptimizationMethod] = useState<OptimizationMethod>('erc');

  // Benchmark comparison settings
  const [benchmarkSymbol, setBenchmarkSymbol] = useState(DEFAULT_BENCHMARK_SYMBOL);

  // Config
  const [config, setConfig] = useState<FFNConfig>(DEFAULT_CONFIG);

  const [expandedSections, setExpandedSections] = useState<ExpandedSections>({
    performance: true,
    drawdowns: true,
    risk: true,
    monthly: true,
    rolling: true,
    comparison: true,
    optimization: true,
    benchmark: true,
    dataSource: true
  });

  // Check FFN availability and load portfolios on mount
  useEffect(() => {
    checkFFNStatus();
    loadPortfolios();
    // Auto-fetch data on mount if using symbol mode
    if (dataSourceType === 'symbol') {
      fetchData();
    }
  }, []);

  // Auto-fetch data when switching to symbol mode or when symbol changes
  useEffect(() => {
    if (dataSourceType === 'symbol' && (symbolInput || symbolsInput)) {
      const timer = setTimeout(() => {
        fetchData();
      }, 500); // Debounce to avoid excessive fetching
      return () => clearTimeout(timer);
    }
  }, [dataSourceType]);

  const checkFFNStatus = async () => {
    try {
      const status = await ffnService.checkStatus();
      setIsFFNAvailable(status.available);
      if (!status.available) {
        setError('FFN library not installed. Run: pip install ffn');
      }
    } catch (err) {
      setIsFFNAvailable(false);
      setError('Failed to check FFN status');
    }
  };

  const loadPortfolios = async () => {
    try {
      const portfolioList = await portfolioService.getPortfolios();
      setPortfolios(portfolioList);
      if (portfolioList.length > 0) {
        setSelectedPortfolioId(portfolioList[0].id);
      }
    } catch (err) {
      console.error('Failed to load portfolios:', err);
    }
  };

  // Fetch data based on source type
  const fetchData = useCallback(async () => {
    setPriceDataLoading(true);
    setError(null);

    try {
      if (dataSourceType === 'portfolio' && selectedPortfolioId) {
        const summary = await portfolioService.getPortfolioSummary(selectedPortfolioId);
        if (!summary || !summary.holdings || summary.holdings.length === 0) {
          setError('Portfolio has no holdings');
          return;
        }

        // Fetch historical data for all holdings
        const priceData: Record<string, Record<string, number>> = {};
        const endDate = new Date();
        const startDate = new Date();
        startDate.setDate(startDate.getDate() - historicalDays);

        for (const holding of summary.holdings) {
          try {
            const data = await yfinanceService.getHistoricalData(
              holding.symbol,
              startDate.toISOString().split('T')[0],
              endDate.toISOString().split('T')[0]
            );
            if (data && data.length > 0) {
              priceData[holding.symbol] = {};
              data.forEach(point => {
                const dateStr = new Date(point.timestamp * 1000).toISOString().split('T')[0];
                priceData[holding.symbol][dateStr] = point.adj_close || point.close;
              });
            }
          } catch (err) {
            console.error(`Failed to fetch data for ${holding.symbol}:`, err);
          }
        }

        if (Object.keys(priceData).length > 0) {
          setPriceInput(JSON.stringify(priceData, null, 2));
        } else {
          setError('Failed to fetch historical data for portfolio');
        }
      } else if (dataSourceType === 'symbol') {
        // Handle single symbol or multiple symbols based on analysis mode
        const symbols = (analysisMode === 'comparison' || analysisMode === 'optimize')
          ? symbolsInput.split(',').map(s => s.trim()).filter(Boolean)
          : [symbolInput];

        const endDate = new Date();
        const startDate = new Date();
        startDate.setDate(startDate.getDate() - historicalDays);

        if (symbols.length === 1) {
          // Single symbol - simple format
          const data = await yfinanceService.getHistoricalData(
            symbols[0],
            startDate.toISOString().split('T')[0],
            endDate.toISOString().split('T')[0]
          );

          if (data && data.length > 0) {
            const priceData: Record<string, number> = {};
            data.forEach(point => {
              const dateStr = new Date(point.timestamp * 1000).toISOString().split('T')[0];
              priceData[dateStr] = point.adj_close || point.close;
            });
            setPriceInput(JSON.stringify(priceData, null, 2));
          } else {
            setError(`No data available for ${symbols[0]}`);
          }
        } else {
          // Multiple symbols - multi-asset format
          const priceData: Record<string, Record<string, number>> = {};

          for (const symbol of symbols) {
            try {
              const data = await yfinanceService.getHistoricalData(
                symbol,
                startDate.toISOString().split('T')[0],
                endDate.toISOString().split('T')[0]
              );
              if (data && data.length > 0) {
                priceData[symbol] = {};
                data.forEach(point => {
                  const dateStr = new Date(point.timestamp * 1000).toISOString().split('T')[0];
                  priceData[symbol][dateStr] = point.adj_close || point.close;
                });
              }
            } catch (err) {
              console.error(`Failed to fetch data for ${symbol}:`, err);
            }
          }

          if (Object.keys(priceData).length > 0) {
            setPriceInput(JSON.stringify(priceData, null, 2));
          } else {
            setError('Failed to fetch data for any symbols');
          }
        }
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load data');
    } finally {
      setPriceDataLoading(false);
    }
  }, [dataSourceType, selectedPortfolioId, symbolInput, symbolsInput, historicalDays, analysisMode]);

  const runAnalysis = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const prices = JSON.parse(priceInput);

      if (analysisMode === 'performance') {
        // Run full analysis
        const result = await ffnService.fullAnalysis(prices, config);
        if (!result.success) {
          setError(result.error || 'Analysis failed');
          return;
        }

        // Get additional risk metrics
        const riskResult = await ffnService.riskMetrics(prices, config.risk_free_rate);

        setAnalysisResult({
          performance: result.performance as PerformanceMetrics,
          drawdowns: result.drawdowns,
          riskMetrics: riskResult.success ? {
            ulcer_index: riskResult.ulcer_index,
            skewness: riskResult.skewness,
            kurtosis: riskResult.kurtosis,
            var_95: riskResult.var_95,
            cvar_95: riskResult.cvar_95
          } : undefined
        });
      } else if (analysisMode === 'monthly') {
        // Monthly returns analysis
        const result = await ffnService.monthlyReturns(prices);
        if (!result.success) {
          setError(result.error || 'Monthly returns analysis failed');
          return;
        }

        setAnalysisResult({
          monthlyReturns: result.monthly_returns
        });
      } else if (analysisMode === 'rolling') {
        // Rolling metrics analysis
        const result = await ffnService.calculateRollingMetrics(prices, 63, ['sharpe', 'volatility']);
        if (!result.success) {
          setError(result.error || 'Rolling metrics analysis failed');
          return;
        }

        setAnalysisResult({
          rollingMetrics: result.metrics
        });
      } else if (analysisMode === 'comparison') {
        // Asset comparison
        const result = await ffnService.compareAssets(prices, undefined, config.risk_free_rate);
        if (!result.success) {
          setError(result.error || 'Asset comparison failed');
          return;
        }

        setAnalysisResult({
          comparison: result
        });
      } else if (analysisMode === 'optimize') {
        // Portfolio optimization
        const result = await ffnService.portfolioOptimization(
          prices,
          optimizationMethod,
          config.risk_free_rate,
          [0, 1]
        );
        if (!result.success) {
          setError(result.error || 'Portfolio optimization failed');
          return;
        }

        setAnalysisResult({
          optimization: result
        });
      } else if (analysisMode === 'benchmark') {
        // Benchmark comparison
        // Fetch benchmark data
        const endDate = new Date();
        const startDate = new Date();
        startDate.setDate(startDate.getDate() - historicalDays);

        const benchmarkData = await yfinanceService.getHistoricalData(
          benchmarkSymbol,
          startDate.toISOString().split('T')[0],
          endDate.toISOString().split('T')[0]
        );

        if (!benchmarkData || benchmarkData.length === 0) {
          setError(`No data available for benchmark ${benchmarkSymbol}`);
          return;
        }

        // Convert to price format
        const benchmarkPrices: Record<string, number> = {};
        benchmarkData.forEach(point => {
          const dateStr = new Date(point.timestamp * 1000).toISOString().split('T')[0];
          benchmarkPrices[dateStr] = point.adj_close || point.close;
        });

        const result = await ffnService.benchmarkComparison(
          prices,
          benchmarkPrices,
          benchmarkSymbol,
          config.risk_free_rate
        );

        if (!result.success) {
          setError(result.error || 'Benchmark comparison failed');
          return;
        }

        setAnalysisResult({
          benchmark: result
        });
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Analysis failed');
    } finally {
      setIsLoading(false);
    }
  };

  const toggleSection = (section: keyof ExpandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  return {
    // Status
    isFFNAvailable,
    isLoading,
    error,

    // Data
    priceInput,
    setPriceInput,
    analysisResult,

    // Data source
    dataSourceType,
    setDataSourceType,
    portfolios,
    selectedPortfolioId,
    setSelectedPortfolioId,
    symbolInput,
    setSymbolInput,
    symbolsInput,
    setSymbolsInput,
    historicalDays,
    setHistoricalDays,
    priceDataLoading,

    // Analysis mode
    analysisMode,
    setAnalysisMode,

    // Optimization
    optimizationMethod,
    setOptimizationMethod,

    // Benchmark
    benchmarkSymbol,
    setBenchmarkSymbol,

    // Config
    config,
    setConfig,

    // UI state
    expandedSections,
    toggleSection,

    // Actions
    checkFFNStatus,
    fetchData,
    runAnalysis
  };
}
