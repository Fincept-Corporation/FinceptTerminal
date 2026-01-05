/**
 * FFN Analytics Panel - Portfolio Performance Analysis
 * Bloomberg Professional Design
 * Integrated with FFN library via PyO3
 *
 * Features:
 * - Multiple data sources (Manual, Portfolio, Symbol)
 * - Performance metrics analysis
 * - Drawdown analysis
 * - Risk metrics
 * - Monthly returns heatmap
 * - Rolling metrics charts
 * - Asset comparison with correlation matrix
 * - Portfolio optimization (ERC, Inv Vol, Mean-Var, Equal Weight)
 * - Benchmark comparison (Alpha, Beta, Tracking Error, Information Ratio, Capture Ratios)
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  TrendingUp,
  TrendingDown,
  Activity,
  BarChart2,
  Calendar,
  RefreshCw,
  AlertCircle,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  LineChart,
  PieChart,
  Percent,
  DollarSign,
  Clock,
  Target,
  Database,
  Briefcase,
  Search,
  Grid,
  Table,
  Layers,
  Scale,
  Sliders,
  GitCompare
} from 'lucide-react';
import { ffnService, type PerformanceMetrics, type DrawdownInfo, type FFNConfig, type MonthlyReturnsResponse, type RollingMetricsResponse, type AssetComparisonResponse, type PortfolioOptimizationResponse, type BenchmarkComparisonResponse } from '@/services/aiQuantLab/ffnService';
import { portfolioService, type Portfolio } from '@/services/portfolioService';
import { yfinanceService } from '@/services/yfinanceService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

// Sample data for demonstration
const SAMPLE_PRICES = {
  '2023-01-03': 100.0,
  '2023-01-04': 101.2,
  '2023-01-05': 99.8,
  '2023-01-06': 102.5,
  '2023-01-09': 103.1,
  '2023-01-10': 104.0,
  '2023-01-11': 102.3,
  '2023-01-12': 105.2,
  '2023-01-13': 106.8,
  '2023-01-17': 105.5,
  '2023-01-18': 107.2,
  '2023-01-19': 106.0,
  '2023-01-20': 108.5,
  '2023-01-23': 109.3,
  '2023-01-24': 110.0,
  '2023-01-25': 108.7,
  '2023-01-26': 111.2,
  '2023-01-27': 112.5,
  '2023-01-30': 111.0,
  '2023-01-31': 113.8
};

// Data source types
type DataSourceType = 'manual' | 'portfolio' | 'symbol';

// Analysis mode types
type AnalysisMode = 'performance' | 'comparison' | 'monthly' | 'rolling' | 'optimize' | 'benchmark';

// Optimization method types
type OptimizationMethod = 'erc' | 'inv_vol' | 'mean_var' | 'equal';

interface AnalysisResult {
  performance?: PerformanceMetrics;
  drawdowns?: {
    max_drawdown: number;
    top_drawdowns: DrawdownInfo[];
  };
  riskMetrics?: {
    ulcer_index?: number;
    skewness?: number;
    kurtosis?: number;
    var_95?: number;
    cvar_95?: number;
  };
  monthlyReturns?: Record<string, Record<string, number | null>>;
  rollingMetrics?: {
    rolling_sharpe?: Record<string, number>;
    rolling_volatility?: Record<string, number>;
  };
  comparison?: AssetComparisonResponse;
  optimization?: PortfolioOptimizationResponse;
  benchmark?: BenchmarkComparisonResponse;
}

export function FFNAnalyticsPanel() {
  const [isFFNAvailable, setIsFFNAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<AnalysisResult | null>(null);
  const [priceInput, setPriceInput] = useState(JSON.stringify(SAMPLE_PRICES, null, 2));

  // Data source configuration
  const [dataSourceType, setDataSourceType] = useState<DataSourceType>('manual');
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioId, setSelectedPortfolioId] = useState<string>('');
  const [symbolInput, setSymbolInput] = useState('AAPL');
  const [symbolsInput, setSymbolsInput] = useState('AAPL,MSFT,GOOGL'); // For comparison
  const [historicalDays, setHistoricalDays] = useState(365);
  const [priceDataLoading, setPriceDataLoading] = useState(false);

  // Analysis mode
  const [analysisMode, setAnalysisMode] = useState<AnalysisMode>('performance');

  // Portfolio optimization settings
  const [optimizationMethod, setOptimizationMethod] = useState<OptimizationMethod>('erc');

  // Benchmark comparison settings
  const [benchmarkSymbol, setBenchmarkSymbol] = useState('SPY');

  // Config
  const [config, setConfig] = useState<FFNConfig>({
    risk_free_rate: 0.02,
    annualization_factor: 252,
    drawdown_threshold: 0.05
  });

  const [expandedSections, setExpandedSections] = useState({
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
  }, []);

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

  const toggleSection = (section: keyof typeof expandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const formatPercent = (value: number | null | undefined, decimals = 2): string => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return `${(value * 100).toFixed(decimals)}%`;
  };

  const formatRatio = (value: number | null | undefined, decimals = 2): string => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(decimals);
  };

  const getValueColor = (value: number | null | undefined, inverse = false): string => {
    if (value === null || value === undefined) return BLOOMBERG.GRAY;
    if (inverse) return value < 0 ? BLOOMBERG.GREEN : value > 0 ? BLOOMBERG.RED : BLOOMBERG.WHITE;
    return value > 0 ? BLOOMBERG.GREEN : value < 0 ? BLOOMBERG.RED : BLOOMBERG.WHITE;
  };

  const getHeatmapColor = (value: number | null): string => {
    if (value === null || value === undefined) return BLOOMBERG.PANEL_BG;
    if (value > 0.05) return '#00D66F';
    if (value > 0.02) return '#00A050';
    if (value > 0) return '#006030';
    if (value > -0.02) return '#603000';
    if (value > -0.05) return '#A05000';
    return '#FF3B3B';
  };

  // Loading/Error states
  if (isFFNAvailable === null) {
    return (
      <div className="flex items-center justify-center h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <RefreshCw size={32} color={BLOOMBERG.ORANGE} className="animate-spin" />
      </div>
    );
  }

  if (!isFFNAvailable) {
    return (
      <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
        <div className="text-center max-w-md">
          <AlertCircle size={48} color={BLOOMBERG.RED} className="mx-auto mb-4" />
          <h3 className="text-lg font-bold uppercase mb-2" style={{ color: BLOOMBERG.WHITE }}>
            FFN Library Not Installed
          </h3>
          <code
            className="block p-4 rounded text-sm font-mono mt-4"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, color: BLOOMBERG.ORANGE }}
          >
            pip install ffn
          </code>
          <button
            onClick={checkFFNStatus}
            className="mt-6 px-6 py-2 rounded font-bold text-sm uppercase flex items-center gap-2 mx-auto"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            <RefreshCw size={14} />
            Check Again
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="flex h-full" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
      {/* Left Panel - Input */}
      <div
        className="w-80 flex flex-col border-r overflow-auto"
        style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderColor: BLOOMBERG.BORDER }}
      >
        {/* Data Source Section */}
        <div
          className="rounded overflow-hidden m-2"
          style={{ border: `1px solid ${BLOOMBERG.BORDER}` }}
        >
          <button
            onClick={() => toggleSection('dataSource')}
            className="w-full px-3 py-2 flex items-center justify-between"
            style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
          >
            <div className="flex items-center gap-2">
              <Database size={14} color={BLOOMBERG.CYAN} />
              <span className="text-xs font-bold uppercase" style={{ color: BLOOMBERG.WHITE }}>
                Data Source
              </span>
            </div>
            {expandedSections.dataSource ? (
              <ChevronUp size={14} color={BLOOMBERG.GRAY} />
            ) : (
              <ChevronDown size={14} color={BLOOMBERG.GRAY} />
            )}
          </button>

          {expandedSections.dataSource && (
            <div className="p-3 space-y-3">
              {/* Data source type buttons */}
              <div className="flex gap-1">
                <button
                  onClick={() => setDataSourceType('manual')}
                  className="flex-1 px-2 py-1.5 rounded text-xs font-mono"
                  style={{
                    backgroundColor: dataSourceType === 'manual' ? BLOOMBERG.ORANGE : 'transparent',
                    color: dataSourceType === 'manual' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                    border: `1px solid ${BLOOMBERG.BORDER}`
                  }}
                >
                  Manual
                </button>
                <button
                  onClick={() => setDataSourceType('portfolio')}
                  className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
                  style={{
                    backgroundColor: dataSourceType === 'portfolio' ? BLOOMBERG.ORANGE : 'transparent',
                    color: dataSourceType === 'portfolio' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                    border: `1px solid ${BLOOMBERG.BORDER}`
                  }}
                >
                  <Briefcase size={12} />
                  Portfolio
                </button>
                <button
                  onClick={() => setDataSourceType('symbol')}
                  className="flex-1 px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
                  style={{
                    backgroundColor: dataSourceType === 'symbol' ? BLOOMBERG.ORANGE : 'transparent',
                    color: dataSourceType === 'symbol' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                    border: `1px solid ${BLOOMBERG.BORDER}`
                  }}
                >
                  <Search size={12} />
                  Symbol
                </button>
              </div>

              {/* Portfolio selector */}
              {dataSourceType === 'portfolio' && (
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    SELECT PORTFOLIO
                  </label>
                  <select
                    value={selectedPortfolioId}
                    onChange={(e) => setSelectedPortfolioId(e.target.value)}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  >
                    {portfolios.map(p => (
                      <option key={p.id} value={p.id}>{p.name}</option>
                    ))}
                  </select>
                </div>
              )}

              {/* Symbol input */}
              {dataSourceType === 'symbol' && (
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    {(analysisMode === 'comparison' || analysisMode === 'optimize') ? 'SYMBOLS (comma-separated)' : 'SYMBOL'}
                  </label>
                  <input
                    type="text"
                    value={(analysisMode === 'comparison' || analysisMode === 'optimize') ? symbolsInput : symbolInput}
                    onChange={(e) => (analysisMode === 'comparison' || analysisMode === 'optimize')
                      ? setSymbolsInput(e.target.value.toUpperCase())
                      : setSymbolInput(e.target.value.toUpperCase())
                    }
                    placeholder={(analysisMode === 'comparison' || analysisMode === 'optimize') ? "AAPL,MSFT,GOOGL" : "AAPL"}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  />
                </div>
              )}

              {/* Historical days */}
              {dataSourceType !== 'manual' && (
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
                    HISTORICAL DAYS
                  </label>
                  <input
                    type="number"
                    value={historicalDays}
                    onChange={(e) => setHistoricalDays(parseInt(e.target.value) || 365)}
                    className="w-full p-2 rounded text-xs font-mono"
                    style={{
                      backgroundColor: BLOOMBERG.DARK_BG,
                      color: BLOOMBERG.WHITE,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}
                  />
                </div>
              )}

              {/* Fetch data button */}
              {dataSourceType !== 'manual' && (
                <button
                  onClick={fetchData}
                  disabled={priceDataLoading}
                  className="w-full py-2 rounded text-xs font-bold uppercase flex items-center justify-center gap-2"
                  style={{
                    backgroundColor: BLOOMBERG.CYAN,
                    color: BLOOMBERG.DARK_BG,
                    opacity: priceDataLoading ? 0.5 : 1
                  }}
                >
                  {priceDataLoading ? (
                    <>
                      <RefreshCw size={12} className="animate-spin" />
                      Loading...
                    </>
                  ) : (
                    <>
                      <Database size={12} />
                      Fetch Data
                    </>
                  )}
                </button>
              )}
            </div>
          )}
        </div>

        {/* Analysis Mode */}
        <div className="px-2 py-2">
          <label className="block text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
            ANALYSIS MODE
          </label>
          <div className="grid grid-cols-2 gap-1">
            <button
              onClick={() => setAnalysisMode('performance')}
              className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: analysisMode === 'performance' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'performance' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <Activity size={12} />
              Performance
            </button>
            <button
              onClick={() => setAnalysisMode('monthly')}
              className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: analysisMode === 'monthly' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'monthly' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <Calendar size={12} />
              Monthly
            </button>
            <button
              onClick={() => setAnalysisMode('rolling')}
              className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: analysisMode === 'rolling' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'rolling' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <LineChart size={12} />
              Rolling
            </button>
            <button
              onClick={() => setAnalysisMode('comparison')}
              className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: analysisMode === 'comparison' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'comparison' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <Layers size={12} />
              Compare
            </button>
            <button
              onClick={() => setAnalysisMode('optimize')}
              className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: analysisMode === 'optimize' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'optimize' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <Scale size={12} />
              Optimize
            </button>
            <button
              onClick={() => setAnalysisMode('benchmark')}
              className="px-2 py-1.5 rounded text-xs font-mono flex items-center justify-center gap-1"
              style={{
                backgroundColor: analysisMode === 'benchmark' ? BLOOMBERG.ORANGE : 'transparent',
                color: analysisMode === 'benchmark' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            >
              <GitCompare size={12} />
              Benchmark
            </button>
          </div>
        </div>

        {/* Optimization Method Selector */}
        {analysisMode === 'optimize' && (
          <div className="px-2 py-2">
            <label className="block text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
              OPTIMIZATION METHOD
            </label>
            <div className="grid grid-cols-2 gap-1">
              <button
                onClick={() => setOptimizationMethod('erc')}
                className="px-2 py-1.5 rounded text-xs font-mono"
                style={{
                  backgroundColor: optimizationMethod === 'erc' ? BLOOMBERG.CYAN : 'transparent',
                  color: optimizationMethod === 'erc' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              >
                ERC
              </button>
              <button
                onClick={() => setOptimizationMethod('inv_vol')}
                className="px-2 py-1.5 rounded text-xs font-mono"
                style={{
                  backgroundColor: optimizationMethod === 'inv_vol' ? BLOOMBERG.CYAN : 'transparent',
                  color: optimizationMethod === 'inv_vol' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              >
                Inv Vol
              </button>
              <button
                onClick={() => setOptimizationMethod('mean_var')}
                className="px-2 py-1.5 rounded text-xs font-mono"
                style={{
                  backgroundColor: optimizationMethod === 'mean_var' ? BLOOMBERG.CYAN : 'transparent',
                  color: optimizationMethod === 'mean_var' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              >
                Mean-Var
              </button>
              <button
                onClick={() => setOptimizationMethod('equal')}
                className="px-2 py-1.5 rounded text-xs font-mono"
                style={{
                  backgroundColor: optimizationMethod === 'equal' ? BLOOMBERG.CYAN : 'transparent',
                  color: optimizationMethod === 'equal' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}
              >
                Equal Wt
              </button>
            </div>
          </div>
        )}

        {/* Benchmark Symbol Input */}
        {analysisMode === 'benchmark' && (
          <div className="px-2 py-2">
            <label className="block text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
              BENCHMARK SYMBOL
            </label>
            <input
              type="text"
              value={benchmarkSymbol}
              onChange={(e) => setBenchmarkSymbol(e.target.value.toUpperCase())}
              placeholder="SPY"
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: BLOOMBERG.DARK_BG,
                color: BLOOMBERG.WHITE,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            />
            <p className="text-xs font-mono mt-1" style={{ color: BLOOMBERG.MUTED }}>
              Common: SPY, QQQ, IWM, DIA
            </p>
          </div>
        )}

        {/* Price Input (for manual mode) */}
        {dataSourceType === 'manual' && (
          <div className="flex-1 p-2 overflow-auto">
            <label className="block text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
              PRICE DATA (JSON)
            </label>
            <textarea
              value={priceInput}
              onChange={(e) => setPriceInput(e.target.value)}
              className="w-full h-48 p-3 rounded text-xs font-mono resize-none"
              style={{
                backgroundColor: BLOOMBERG.DARK_BG,
                color: BLOOMBERG.WHITE,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
              placeholder='{"2023-01-01": 100, "2023-01-02": 101, ...}'
            />
          </div>
        )}

        {/* Config Options */}
        <div className="p-2 space-y-3">
          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
              RISK-FREE RATE
            </label>
            <input
              type="number"
              step="0.01"
              value={config.risk_free_rate}
              onChange={(e) => setConfig({ ...config, risk_free_rate: parseFloat(e.target.value) || 0 })}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: BLOOMBERG.DARK_BG,
                color: BLOOMBERG.WHITE,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            />
          </div>

          <div>
            <label className="block text-xs font-mono mb-1" style={{ color: BLOOMBERG.GRAY }}>
              DRAWDOWN THRESHOLD
            </label>
            <input
              type="number"
              step="0.01"
              value={config.drawdown_threshold}
              onChange={(e) => setConfig({ ...config, drawdown_threshold: parseFloat(e.target.value) || 0.05 })}
              className="w-full p-2 rounded text-xs font-mono"
              style={{
                backgroundColor: BLOOMBERG.DARK_BG,
                color: BLOOMBERG.WHITE,
                border: `1px solid ${BLOOMBERG.BORDER}`
              }}
            />
          </div>
        </div>

        {/* Run Button */}
        <div className="p-2 border-t" style={{ borderColor: BLOOMBERG.BORDER }}>
          <button
            onClick={runAnalysis}
            disabled={isLoading}
            className="w-full py-3 rounded font-bold text-sm uppercase flex items-center justify-center gap-2 disabled:opacity-50"
            style={{ backgroundColor: BLOOMBERG.ORANGE, color: BLOOMBERG.DARK_BG }}
          >
            {isLoading ? (
              <>
                <RefreshCw size={16} className="animate-spin" />
                ANALYZING...
              </>
            ) : (
              <>
                <Activity size={16} />
                RUN ANALYSIS
              </>
            )}
          </button>
        </div>
      </div>

      {/* Right Panel - Results */}
      <div className="flex-1 overflow-auto p-4">
        {error && (
          <div
            className="mb-4 p-4 rounded flex items-center gap-3"
            style={{ backgroundColor: BLOOMBERG.PANEL_BG, borderLeft: `3px solid ${BLOOMBERG.RED}` }}
          >
            <AlertCircle size={20} color={BLOOMBERG.RED} />
            <span className="text-sm font-mono" style={{ color: BLOOMBERG.RED }}>{error}</span>
          </div>
        )}

        {!analysisResult && !error && (
          <div className="flex items-center justify-center h-full">
            <div className="text-center">
              <BarChart2 size={64} color={BLOOMBERG.MUTED} className="mx-auto mb-4" />
              <p className="text-sm font-mono" style={{ color: BLOOMBERG.GRAY }}>
                {analysisMode === 'performance' && 'Enter price data and click "Run Analysis" to see performance metrics'}
                {analysisMode === 'monthly' && 'Click "Run Analysis" to see monthly returns heatmap'}
                {analysisMode === 'rolling' && 'Click "Run Analysis" to see rolling performance metrics'}
                {analysisMode === 'comparison' && 'Enter multiple symbols and click "Run Analysis" to compare assets'}
                {analysisMode === 'optimize' && 'Enter multiple symbols and click "Run Analysis" to optimize portfolio weights'}
                {analysisMode === 'benchmark' && 'Enter portfolio data and benchmark symbol, then click "Run Analysis" to compare against benchmark'}
              </p>
            </div>
          </div>
        )}

        {analysisResult && (
          <div className="space-y-4">
            {/* Performance Metrics Section */}
            {analysisResult.performance && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('performance')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <TrendingUp size={16} color={BLOOMBERG.GREEN} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Performance Metrics
                    </span>
                  </div>
                  {expandedSections.performance ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.performance && (
                  <div className="p-4 grid grid-cols-3 gap-4">
                    <MetricCard
                      label="Total Return"
                      value={formatPercent(analysisResult.performance.total_return)}
                      color={getValueColor(analysisResult.performance.total_return)}
                      icon={<Percent size={14} />}
                    />
                    <MetricCard
                      label="CAGR"
                      value={formatPercent(analysisResult.performance.cagr)}
                      color={getValueColor(analysisResult.performance.cagr)}
                      icon={<TrendingUp size={14} />}
                    />
                    <MetricCard
                      label="Sharpe Ratio"
                      value={formatRatio(analysisResult.performance.sharpe_ratio)}
                      color={getValueColor(analysisResult.performance.sharpe_ratio)}
                      icon={<Target size={14} />}
                    />
                    <MetricCard
                      label="Sortino Ratio"
                      value={formatRatio(analysisResult.performance.sortino_ratio)}
                      color={getValueColor(analysisResult.performance.sortino_ratio)}
                      icon={<Target size={14} />}
                    />
                    <MetricCard
                      label="Volatility"
                      value={formatPercent(analysisResult.performance.volatility)}
                      color={BLOOMBERG.YELLOW}
                      icon={<Activity size={14} />}
                    />
                    <MetricCard
                      label="Calmar Ratio"
                      value={formatRatio(analysisResult.performance.calmar_ratio)}
                      color={getValueColor(analysisResult.performance.calmar_ratio)}
                      icon={<BarChart2 size={14} />}
                    />
                    <MetricCard
                      label="Best Day"
                      value={formatPercent(analysisResult.performance.best_day)}
                      color={BLOOMBERG.GREEN}
                      icon={<TrendingUp size={14} />}
                    />
                    <MetricCard
                      label="Worst Day"
                      value={formatPercent(analysisResult.performance.worst_day)}
                      color={BLOOMBERG.RED}
                      icon={<TrendingDown size={14} />}
                    />
                    <MetricCard
                      label="Daily Mean"
                      value={formatPercent(analysisResult.performance.daily_mean, 4)}
                      color={getValueColor(analysisResult.performance.daily_mean)}
                      icon={<LineChart size={14} />}
                    />
                  </div>
                )}
              </div>
            )}

            {/* Drawdowns Section */}
            {analysisResult.drawdowns && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('drawdowns')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <TrendingDown size={16} color={BLOOMBERG.RED} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Drawdown Analysis
                    </span>
                  </div>
                  {expandedSections.drawdowns ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.drawdowns && (
                  <div className="p-4">
                    <div className="mb-4">
                      <MetricCard
                        label="Max Drawdown"
                        value={formatPercent(analysisResult.drawdowns.max_drawdown)}
                        color={BLOOMBERG.RED}
                        icon={<TrendingDown size={14} />}
                        large
                      />
                    </div>

                    {analysisResult.drawdowns.top_drawdowns && analysisResult.drawdowns.top_drawdowns.length > 0 && (
                      <div>
                        <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                          TOP DRAWDOWN PERIODS
                        </h4>
                        <div className="space-y-2">
                          {analysisResult.drawdowns.top_drawdowns.map((dd, idx) => (
                            <div
                              key={idx}
                              className="p-3 rounded flex items-center justify-between"
                              style={{ backgroundColor: BLOOMBERG.DARK_BG }}
                            >
                              <div className="flex items-center gap-3">
                                <span
                                  className="w-6 h-6 rounded-full flex items-center justify-center text-xs font-bold"
                                  style={{ backgroundColor: BLOOMBERG.RED, color: BLOOMBERG.WHITE }}
                                >
                                  {idx + 1}
                                </span>
                                <div>
                                  <span className="text-xs font-mono" style={{ color: BLOOMBERG.WHITE }}>
                                    {dd.start} → {dd.end}
                                  </span>
                                </div>
                              </div>
                              <span className="text-sm font-bold" style={{ color: BLOOMBERG.RED }}>
                                {formatPercent(dd.drawdown)}
                              </span>
                            </div>
                          ))}
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            )}

            {/* Risk Metrics Section */}
            {analysisResult.riskMetrics && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('risk')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <AlertCircle size={16} color={BLOOMBERG.YELLOW} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Risk Metrics
                    </span>
                  </div>
                  {expandedSections.risk ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.risk && (
                  <div className="p-4 grid grid-cols-3 gap-4">
                    <MetricCard
                      label="Ulcer Index"
                      value={formatRatio(analysisResult.riskMetrics.ulcer_index, 4)}
                      color={BLOOMBERG.YELLOW}
                      icon={<Activity size={14} />}
                    />
                    <MetricCard
                      label="Skewness"
                      value={formatRatio(analysisResult.riskMetrics.skewness)}
                      color={BLOOMBERG.CYAN}
                      icon={<BarChart2 size={14} />}
                    />
                    <MetricCard
                      label="Kurtosis"
                      value={formatRatio(analysisResult.riskMetrics.kurtosis)}
                      color={BLOOMBERG.CYAN}
                      icon={<BarChart2 size={14} />}
                    />
                    <MetricCard
                      label="VaR (95%)"
                      value={formatPercent(analysisResult.riskMetrics.var_95)}
                      color={BLOOMBERG.RED}
                      icon={<TrendingDown size={14} />}
                    />
                    <MetricCard
                      label="CVaR (95%)"
                      value={formatPercent(analysisResult.riskMetrics.cvar_95)}
                      color={BLOOMBERG.RED}
                      icon={<TrendingDown size={14} />}
                    />
                  </div>
                )}
              </div>
            )}

            {/* Monthly Returns Heatmap */}
            {analysisResult.monthlyReturns && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('monthly')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <Calendar size={16} color={BLOOMBERG.CYAN} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Monthly Returns Heatmap
                    </span>
                  </div>
                  {expandedSections.monthly ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.monthly && (
                  <div className="p-4 overflow-auto">
                    <div className="min-w-[600px]">
                      {/* Month headers */}
                      <div className="flex">
                        <div className="w-16 text-xs font-mono text-center" style={{ color: BLOOMBERG.GRAY }}>
                          Year
                        </div>
                        {['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'].map(m => (
                          <div key={m} className="flex-1 text-xs font-mono text-center" style={{ color: BLOOMBERG.GRAY }}>
                            {m}
                          </div>
                        ))}
                      </div>
                      {/* Year rows */}
                      {Object.entries(analysisResult.monthlyReturns).sort().map(([year, months]) => (
                        <div key={year} className="flex mt-1">
                          <div className="w-16 text-xs font-mono text-center py-2" style={{ color: BLOOMBERG.WHITE }}>
                            {year}
                          </div>
                          {[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map(m => {
                            const value = months[String(m)] ?? null;
                            return (
                              <div
                                key={m}
                                className="flex-1 text-xs font-mono text-center py-2 mx-0.5 rounded"
                                style={{
                                  backgroundColor: getHeatmapColor(value),
                                  color: BLOOMBERG.WHITE
                                }}
                                title={value !== null ? `${(value * 100).toFixed(2)}%` : 'N/A'}
                              >
                                {value !== null ? `${(value * 100).toFixed(1)}` : '-'}
                              </div>
                            );
                          })}
                        </div>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* Rolling Metrics */}
            {analysisResult.rollingMetrics && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('rolling')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <LineChart size={16} color={BLOOMBERG.PURPLE} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Rolling Metrics (63-Day)
                    </span>
                  </div>
                  {expandedSections.rolling ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.rolling && (
                  <div className="p-4">
                    {analysisResult.rollingMetrics.rolling_sharpe && (
                      <div className="mb-4">
                        <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                          ROLLING SHARPE RATIO
                        </h4>
                        <div className="h-32 flex items-end gap-px">
                          {Object.values(analysisResult.rollingMetrics.rolling_sharpe).slice(-50).map((value, idx) => {
                            const height = Math.min(100, Math.max(10, ((value + 2) / 4) * 100));
                            return (
                              <div
                                key={idx}
                                className="flex-1 rounded-t"
                                style={{
                                  height: `${height}%`,
                                  backgroundColor: value > 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
                                }}
                              />
                            );
                          })}
                        </div>
                      </div>
                    )}
                    {analysisResult.rollingMetrics.rolling_volatility && (
                      <div>
                        <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                          ROLLING VOLATILITY
                        </h4>
                        <div className="h-32 flex items-end gap-px">
                          {Object.values(analysisResult.rollingMetrics.rolling_volatility).slice(-50).map((value, idx) => {
                            const height = Math.min(100, Math.max(10, (value / 0.5) * 100));
                            return (
                              <div
                                key={idx}
                                className="flex-1 rounded-t"
                                style={{
                                  height: `${height}%`,
                                  backgroundColor: BLOOMBERG.YELLOW
                                }}
                              />
                            );
                          })}
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            )}

            {/* Asset Comparison */}
            {analysisResult.comparison && analysisResult.comparison.success && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('comparison')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <Layers size={16} color={BLOOMBERG.BLUE} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Asset Comparison
                    </span>
                  </div>
                  {expandedSections.comparison ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.comparison && (
                  <div className="p-4 space-y-4">
                    {/* Correlation Matrix */}
                    {analysisResult.comparison.correlation_matrix && (
                      <div>
                        <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                          CORRELATION MATRIX
                        </h4>
                        <div className="overflow-auto">
                          <table className="w-full text-xs font-mono">
                            <thead>
                              <tr>
                                <th className="p-2" style={{ color: BLOOMBERG.GRAY }}></th>
                                {Object.keys(analysisResult.comparison.correlation_matrix).map(symbol => (
                                  <th key={symbol} className="p-2" style={{ color: BLOOMBERG.WHITE }}>
                                    {symbol}
                                  </th>
                                ))}
                              </tr>
                            </thead>
                            <tbody>
                              {Object.entries(analysisResult.comparison.correlation_matrix).map(([symbol, correlations]) => (
                                <tr key={symbol}>
                                  <td className="p-2" style={{ color: BLOOMBERG.WHITE }}>{symbol}</td>
                                  {Object.entries(correlations).map(([otherSymbol, corr]) => (
                                    <td
                                      key={otherSymbol}
                                      className="p-2 text-center rounded"
                                      style={{
                                        backgroundColor: `rgba(0, 136, 255, ${Math.abs(corr)})`,
                                        color: BLOOMBERG.WHITE
                                      }}
                                    >
                                      {corr.toFixed(2)}
                                    </td>
                                  ))}
                                </tr>
                              ))}
                            </tbody>
                          </table>
                        </div>
                      </div>
                    )}

                    {/* Asset Stats Comparison */}
                    {analysisResult.comparison.asset_stats && (
                      <div>
                        <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                          PERFORMANCE COMPARISON
                        </h4>
                        <div className="overflow-auto">
                          <table className="w-full text-xs font-mono">
                            <thead>
                              <tr>
                                <th className="p-2 text-left" style={{ color: BLOOMBERG.GRAY }}>Asset</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Return</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Vol</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Sharpe</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>MaxDD</th>
                              </tr>
                            </thead>
                            <tbody>
                              {Object.entries(analysisResult.comparison.asset_stats).map(([symbol, stats]) => (
                                <tr key={symbol} className="border-t" style={{ borderColor: BLOOMBERG.BORDER }}>
                                  <td className="p-2" style={{ color: BLOOMBERG.WHITE }}>{symbol}</td>
                                  <td className="p-2 text-right" style={{ color: getValueColor(stats.total_return) }}>
                                    {formatPercent(stats.total_return)}
                                  </td>
                                  <td className="p-2 text-right" style={{ color: BLOOMBERG.YELLOW }}>
                                    {formatPercent(stats.volatility)}
                                  </td>
                                  <td className="p-2 text-right" style={{ color: getValueColor(stats.sharpe_ratio) }}>
                                    {formatRatio(stats.sharpe_ratio)}
                                  </td>
                                  <td className="p-2 text-right" style={{ color: BLOOMBERG.RED }}>
                                    {formatPercent(stats.max_drawdown)}
                                  </td>
                                </tr>
                              ))}
                            </tbody>
                          </table>
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            )}

            {/* Portfolio Optimization Results */}
            {analysisResult.optimization && analysisResult.optimization.success && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('optimization')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <Scale size={16} color={BLOOMBERG.CYAN} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Portfolio Optimization - {analysisResult.optimization.method}
                    </span>
                  </div>
                  {expandedSections.optimization ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.optimization && (
                  <div className="p-4 space-y-4">
                    {/* Optimal Weights */}
                    {analysisResult.optimization.weights && (
                      <div>
                        <h4 className="text-xs font-mono mb-3" style={{ color: BLOOMBERG.GRAY }}>
                          OPTIMAL WEIGHTS
                        </h4>
                        <div className="space-y-2">
                          {Object.entries(analysisResult.optimization.weights)
                            .sort(([, a], [, b]) => b - a)
                            .map(([symbol, weight]) => (
                              <div key={symbol} className="flex items-center gap-3">
                                <span className="w-16 text-xs font-mono font-bold" style={{ color: BLOOMBERG.WHITE }}>
                                  {symbol}
                                </span>
                                <div className="flex-1 h-6 rounded overflow-hidden" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                  <div
                                    className="h-full rounded"
                                    style={{
                                      width: `${weight * 100}%`,
                                      backgroundColor: BLOOMBERG.CYAN
                                    }}
                                  />
                                </div>
                                <span className="w-16 text-right text-xs font-mono font-bold" style={{ color: BLOOMBERG.CYAN }}>
                                  {formatPercent(weight)}
                                </span>
                              </div>
                            ))}
                        </div>
                      </div>
                    )}

                    {/* Portfolio Stats */}
                    {analysisResult.optimization.portfolio_stats && (
                      <div>
                        <h4 className="text-xs font-mono mb-3" style={{ color: BLOOMBERG.GRAY }}>
                          OPTIMIZED PORTFOLIO METRICS
                        </h4>
                        <div className="grid grid-cols-3 gap-3">
                          <MetricCard
                            label="Expected Return"
                            value={formatPercent(analysisResult.optimization.portfolio_stats.total_return)}
                            color={getValueColor(analysisResult.optimization.portfolio_stats.total_return)}
                            icon={<TrendingUp size={14} />}
                          />
                          <MetricCard
                            label="CAGR"
                            value={formatPercent(analysisResult.optimization.portfolio_stats.cagr)}
                            color={getValueColor(analysisResult.optimization.portfolio_stats.cagr)}
                            icon={<TrendingUp size={14} />}
                          />
                          <MetricCard
                            label="Volatility"
                            value={formatPercent(analysisResult.optimization.portfolio_stats.volatility)}
                            color={BLOOMBERG.YELLOW}
                            icon={<Activity size={14} />}
                          />
                          <MetricCard
                            label="Sharpe Ratio"
                            value={formatRatio(analysisResult.optimization.portfolio_stats.sharpe_ratio)}
                            color={getValueColor(analysisResult.optimization.portfolio_stats.sharpe_ratio)}
                            icon={<Target size={14} />}
                          />
                          <MetricCard
                            label="Sortino Ratio"
                            value={formatRatio(analysisResult.optimization.portfolio_stats.sortino_ratio)}
                            color={getValueColor(analysisResult.optimization.portfolio_stats.sortino_ratio)}
                            icon={<Target size={14} />}
                          />
                          <MetricCard
                            label="Max Drawdown"
                            value={formatPercent(analysisResult.optimization.portfolio_stats.max_drawdown)}
                            color={BLOOMBERG.RED}
                            icon={<TrendingDown size={14} />}
                          />
                        </div>
                      </div>
                    )}

                    {/* Asset Contributions */}
                    {analysisResult.optimization.asset_contributions && (
                      <div>
                        <h4 className="text-xs font-mono mb-2" style={{ color: BLOOMBERG.GRAY }}>
                          ASSET CONTRIBUTIONS
                        </h4>
                        <div className="overflow-auto">
                          <table className="w-full text-xs font-mono">
                            <thead>
                              <tr>
                                <th className="p-2 text-left" style={{ color: BLOOMBERG.GRAY }}>Asset</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Weight</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Vol</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Return</th>
                                <th className="p-2 text-right" style={{ color: BLOOMBERG.GRAY }}>Risk Contrib</th>
                              </tr>
                            </thead>
                            <tbody>
                              {Object.entries(analysisResult.optimization.asset_contributions).map(([symbol, contrib]) => (
                                <tr key={symbol} className="border-t" style={{ borderColor: BLOOMBERG.BORDER }}>
                                  <td className="p-2" style={{ color: BLOOMBERG.WHITE }}>{symbol}</td>
                                  <td className="p-2 text-right" style={{ color: BLOOMBERG.CYAN }}>
                                    {formatPercent(contrib.weight)}
                                  </td>
                                  <td className="p-2 text-right" style={{ color: BLOOMBERG.YELLOW }}>
                                    {formatPercent(contrib.volatility)}
                                  </td>
                                  <td className="p-2 text-right" style={{ color: getValueColor(contrib.return) }}>
                                    {formatPercent(contrib.return)}
                                  </td>
                                  <td className="p-2 text-right" style={{ color: BLOOMBERG.PURPLE }}>
                                    {formatPercent(contrib.risk_contribution)}
                                  </td>
                                </tr>
                              ))}
                            </tbody>
                          </table>
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            )}

            {/* Benchmark Comparison Results */}
            {analysisResult.benchmark && analysisResult.benchmark.success && (
              <div
                className="rounded overflow-hidden"
                style={{ backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}` }}
              >
                <button
                  onClick={() => toggleSection('benchmark')}
                  className="w-full px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: BLOOMBERG.HEADER_BG }}
                >
                  <div className="flex items-center gap-2">
                    <GitCompare size={16} color={BLOOMBERG.PURPLE} />
                    <span className="text-xs font-bold uppercase tracking-wide" style={{ color: BLOOMBERG.WHITE }}>
                      Benchmark Comparison - {analysisResult.benchmark.benchmark_name || 'Benchmark'}
                    </span>
                  </div>
                  {expandedSections.benchmark ? (
                    <ChevronUp size={16} color={BLOOMBERG.GRAY} />
                  ) : (
                    <ChevronDown size={16} color={BLOOMBERG.GRAY} />
                  )}
                </button>

                {expandedSections.benchmark && (
                  <div className="p-4 space-y-4">
                    {/* Side by Side Performance Comparison */}
                    <div className="grid grid-cols-2 gap-4">
                      {/* Portfolio Stats */}
                      <div>
                        <h4 className="text-xs font-mono mb-3 flex items-center gap-2" style={{ color: BLOOMBERG.CYAN }}>
                          <Briefcase size={14} />
                          PORTFOLIO
                        </h4>
                        <div className="space-y-2">
                          {analysisResult.benchmark.portfolio_stats && (
                            <>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Total Return</span>
                                <span className="text-xs font-mono font-bold" style={{ color: getValueColor(analysisResult.benchmark.portfolio_stats.total_return) }}>
                                  {formatPercent(analysisResult.benchmark.portfolio_stats.total_return)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>CAGR</span>
                                <span className="text-xs font-mono font-bold" style={{ color: getValueColor(analysisResult.benchmark.portfolio_stats.cagr) }}>
                                  {formatPercent(analysisResult.benchmark.portfolio_stats.cagr)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Volatility</span>
                                <span className="text-xs font-mono font-bold" style={{ color: BLOOMBERG.YELLOW }}>
                                  {formatPercent(analysisResult.benchmark.portfolio_stats.volatility)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Sharpe Ratio</span>
                                <span className="text-xs font-mono font-bold" style={{ color: getValueColor(analysisResult.benchmark.portfolio_stats.sharpe_ratio) }}>
                                  {formatRatio(analysisResult.benchmark.portfolio_stats.sharpe_ratio)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Max Drawdown</span>
                                <span className="text-xs font-mono font-bold" style={{ color: BLOOMBERG.RED }}>
                                  {formatPercent(analysisResult.benchmark.portfolio_stats.max_drawdown)}
                                </span>
                              </div>
                            </>
                          )}
                        </div>
                      </div>

                      {/* Benchmark Stats */}
                      <div>
                        <h4 className="text-xs font-mono mb-3 flex items-center gap-2" style={{ color: BLOOMBERG.ORANGE }}>
                          <Target size={14} />
                          {analysisResult.benchmark.benchmark_name || 'BENCHMARK'}
                        </h4>
                        <div className="space-y-2">
                          {analysisResult.benchmark.benchmark_stats && (
                            <>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Total Return</span>
                                <span className="text-xs font-mono font-bold" style={{ color: getValueColor(analysisResult.benchmark.benchmark_stats.total_return) }}>
                                  {formatPercent(analysisResult.benchmark.benchmark_stats.total_return)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>CAGR</span>
                                <span className="text-xs font-mono font-bold" style={{ color: getValueColor(analysisResult.benchmark.benchmark_stats.cagr) }}>
                                  {formatPercent(analysisResult.benchmark.benchmark_stats.cagr)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Volatility</span>
                                <span className="text-xs font-mono font-bold" style={{ color: BLOOMBERG.YELLOW }}>
                                  {formatPercent(analysisResult.benchmark.benchmark_stats.volatility)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Sharpe Ratio</span>
                                <span className="text-xs font-mono font-bold" style={{ color: getValueColor(analysisResult.benchmark.benchmark_stats.sharpe_ratio) }}>
                                  {formatRatio(analysisResult.benchmark.benchmark_stats.sharpe_ratio)}
                                </span>
                              </div>
                              <div className="flex justify-between p-2 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                                <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>Max Drawdown</span>
                                <span className="text-xs font-mono font-bold" style={{ color: BLOOMBERG.RED }}>
                                  {formatPercent(analysisResult.benchmark.benchmark_stats.max_drawdown)}
                                </span>
                              </div>
                            </>
                          )}
                        </div>
                      </div>
                    </div>

                    {/* Relative Metrics */}
                    {analysisResult.benchmark.relative_metrics && (
                      <div>
                        <h4 className="text-xs font-mono mb-3" style={{ color: BLOOMBERG.GRAY }}>
                          RELATIVE METRICS
                        </h4>
                        <div className="grid grid-cols-4 gap-3">
                          <MetricCard
                            label="Alpha"
                            value={formatPercent(analysisResult.benchmark.relative_metrics.alpha)}
                            color={getValueColor(analysisResult.benchmark.relative_metrics.alpha)}
                            icon={<TrendingUp size={14} />}
                          />
                          <MetricCard
                            label="Beta"
                            value={formatRatio(analysisResult.benchmark.relative_metrics.beta)}
                            color={BLOOMBERG.CYAN}
                            icon={<Activity size={14} />}
                          />
                          <MetricCard
                            label="Correlation"
                            value={formatRatio(analysisResult.benchmark.relative_metrics.correlation)}
                            color={BLOOMBERG.BLUE}
                            icon={<LineChart size={14} />}
                          />
                          <MetricCard
                            label="Tracking Error"
                            value={formatPercent(analysisResult.benchmark.relative_metrics.tracking_error)}
                            color={BLOOMBERG.YELLOW}
                            icon={<Target size={14} />}
                          />
                          <MetricCard
                            label="Info Ratio"
                            value={formatRatio(analysisResult.benchmark.relative_metrics.information_ratio)}
                            color={getValueColor(analysisResult.benchmark.relative_metrics.information_ratio)}
                            icon={<BarChart2 size={14} />}
                          />
                          <MetricCard
                            label="Up Capture"
                            value={formatPercent(analysisResult.benchmark.relative_metrics.up_capture)}
                            color={BLOOMBERG.GREEN}
                            icon={<TrendingUp size={14} />}
                          />
                          <MetricCard
                            label="Down Capture"
                            value={formatPercent(analysisResult.benchmark.relative_metrics.down_capture)}
                            color={BLOOMBERG.RED}
                            icon={<TrendingDown size={14} />}
                          />
                        </div>
                      </div>
                    )}

                    {/* Date Range Info */}
                    {analysisResult.benchmark.date_range && (
                      <div className="flex items-center justify-between p-3 rounded" style={{ backgroundColor: BLOOMBERG.DARK_BG }}>
                        <div className="flex items-center gap-2">
                          <Calendar size={14} color={BLOOMBERG.GRAY} />
                          <span className="text-xs font-mono" style={{ color: BLOOMBERG.GRAY }}>
                            {analysisResult.benchmark.date_range.start} to {analysisResult.benchmark.date_range.end}
                          </span>
                        </div>
                        <span className="text-xs font-mono" style={{ color: BLOOMBERG.MUTED }}>
                          {analysisResult.benchmark.date_range.data_points} data points
                        </span>
                      </div>
                    )}
                  </div>
                )}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}

// Metric Card Component
function MetricCard({
  label,
  value,
  color,
  icon,
  large = false
}: {
  label: string;
  value: string;
  color: string;
  icon: React.ReactNode;
  large?: boolean;
}) {
  return (
    <div
      className={`p-3 rounded ${large ? 'col-span-3' : ''}`}
      style={{ backgroundColor: BLOOMBERG.DARK_BG }}
    >
      <div className="flex items-center gap-2 mb-1">
        <span style={{ color: BLOOMBERG.GRAY }}>{icon}</span>
        <span className="text-xs font-mono uppercase" style={{ color: BLOOMBERG.GRAY }}>
          {label}
        </span>
      </div>
      <span
        className={`font-bold font-mono ${large ? 'text-2xl' : 'text-lg'}`}
        style={{ color }}
      >
        {value}
      </span>
    </div>
  );
}
