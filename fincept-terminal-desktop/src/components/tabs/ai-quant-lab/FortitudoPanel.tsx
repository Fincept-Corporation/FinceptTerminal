/**
 * Fortitudo.tech Analytics Panel - Portfolio Risk Analytics
 * Fincept Professional Design
 * Integrated with fortitudo.tech library via PyO3
 */

import React, { useState, useEffect, useMemo } from 'react';
import {
  Shield,
  TrendingUp,
  TrendingDown,
  Activity,
  BarChart2,
  RefreshCw,
  AlertCircle,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  Play,
  Settings,
  Target,
  Percent,
  DollarSign,
  Layers,
  Grid,
  PieChart as PieChartIcon
} from 'lucide-react';
import {
  ScatterChart,
  Scatter,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  Line,
  ComposedChart,
  ReferenceLine,
  Cell,
  PieChart,
  Pie,
  Legend
} from 'recharts';
import {
  fortitudoService,
  type PortfolioMetrics,
  type FullAnalysisResponse,
  type OptionGreeksResponse,
  type OptimizationResult,
  type EfficientFrontierResponse,
  type OptimizationObjective
} from '@/services/aiQuantLab/fortitudoService';
import {
  portfolioFortitudoService,
  type PortfolioReturnsData
} from '@/services/aiQuantLab/portfolioFortitudoService';
import { type Portfolio, type PortfolioSummary } from '@/services/portfolio/portfolioService';
import { Briefcase, Database } from 'lucide-react';

// Fincept Professional Color Palette
const FINCEPT = {
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

// Sample portfolio data for demonstration
const SAMPLE_RETURNS = {
  'Stocks': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0003 + (Math.random() - 0.5) * 0.02
    ])
  ),
  'Bonds': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0001 + (Math.random() - 0.5) * 0.005
    ])
  ),
  'Commodities': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0002 + (Math.random() - 0.5) * 0.015
    ])
  ),
  'Cash': Object.fromEntries(
    Array.from({ length: 200 }, (_, i) => [
      new Date(2024, 0, i + 1).toISOString().split('T')[0],
      0.0001 + (Math.random() - 0.5) * 0.001
    ])
  )
};

const SAMPLE_WEIGHTS = [0.4, 0.3, 0.2, 0.1];

type AnalysisMode = 'portfolio' | 'options' | 'entropy' | 'optimization';

interface AnalysisResult {
  metrics_equal_weight?: PortfolioMetrics;
  metrics_exp_decay?: PortfolioMetrics;
  n_scenarios?: number;
  n_assets?: number;
  assets?: string[];
  half_life?: number;
  alpha?: number;
}

export function FortitudoPanel() {
  const [isFortitudoAvailable, setIsFortitudoAvailable] = useState<boolean | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [analysisResult, setAnalysisResult] = useState<AnalysisResult | null>(null);
  const [activeMode, setActiveMode] = useState<AnalysisMode>('portfolio');

  // Portfolio config
  const [weights, setWeights] = useState('[0.4, 0.3, 0.2, 0.1]');
  const [alpha, setAlpha] = useState(0.05);
  const [halfLife, setHalfLife] = useState(252);

  // Option pricing config
  const [spotPrice, setSpotPrice] = useState(100);
  const [strike, setStrike] = useState(100);
  const [volatility, setVolatility] = useState(0.25);
  const [riskFreeRate, setRiskFreeRate] = useState(0.05);
  const [dividendYield, setDividendYield] = useState(0.02);
  const [timeToMaturity, setTimeToMaturity] = useState(1.0);
  const [optionResult, setOptionResult] = useState<any>(null);
  const [greeksResult, setGreeksResult] = useState<OptionGreeksResponse | null>(null);

  // Entropy pooling config
  const [nScenarios, setNScenarios] = useState(100);
  const [maxProbability, setMaxProbability] = useState(0.05);
  const [entropyResult, setEntropyResult] = useState<any>(null);

  // Optimization config
  const [optimizationObjective, setOptimizationObjective] = useState<OptimizationObjective>('min_variance');
  const [optimizationType, setOptimizationType] = useState<'mv' | 'cvar'>('mv');
  const [optLongOnly, setOptLongOnly] = useState(true);
  const [optMaxWeight, setOptMaxWeight] = useState(0.5);
  const [optMinWeight, setOptMinWeight] = useState(0.0);
  const [optTargetReturn, setOptTargetReturn] = useState(0.08);
  const [optAlpha, setOptAlpha] = useState(0.05);
  const [optimizationResult, setOptimizationResult] = useState<OptimizationResult | null>(null);
  const [frontierResult, setFrontierResult] = useState<EfficientFrontierResponse | null>(null);
  const [nFrontierPoints, setNFrontierPoints] = useState(20);
  const [selectedFrontierIndex, setSelectedFrontierIndex] = useState<number | null>(null);

  const [expandedSections, setExpandedSections] = useState({
    config: true,
    metrics: true,
    comparison: true,
    matrix: false
  });

  // User portfolio integration
  const [userPortfolios, setUserPortfolios] = useState<Portfolio[]>([]);
  const [selectedPortfolioId, setSelectedPortfolioId] = useState<string | null>(null);
  const [selectedPortfolioSummary, setSelectedPortfolioSummary] = useState<PortfolioSummary | null>(null);
  const [portfolioReturnsData, setPortfolioReturnsData] = useState<PortfolioReturnsData | null>(null);
  const [dataSource, setDataSource] = useState<'sample' | 'portfolio'>('sample');
  const [isLoadingPortfolio, setIsLoadingPortfolio] = useState(false);
  const [historicalDays, setHistoricalDays] = useState(252);

  // Check fortitudo availability on mount
  useEffect(() => {
    checkFortitudoStatus();
    loadUserPortfolios();
  }, []);

  // Load portfolio summary and returns when selected
  useEffect(() => {
    if (selectedPortfolioId && dataSource === 'portfolio') {
      loadPortfolioData(selectedPortfolioId);
    }
  }, [selectedPortfolioId, dataSource]);

  const loadUserPortfolios = async () => {
    try {
      const portfolios = await portfolioFortitudoService.getAvailablePortfolios();
      setUserPortfolios(portfolios);
    } catch (err) {
      console.error('[FortitudoPanel] Failed to load portfolios:', err);
    }
  };

  const loadPortfolioData = async (portfolioId: string) => {
    setIsLoadingPortfolio(true);
    setError(null);
    try {
      const summary = await portfolioFortitudoService.getPortfolioSummary(portfolioId);
      setSelectedPortfolioSummary(summary);

      if (summary && summary.holdings.length > 0) {
        const returns = await portfolioFortitudoService.fetchHistoricalReturns(summary.holdings, historicalDays);
        setPortfolioReturnsData(returns);

        // Auto-update weights from portfolio
        if (returns) {
          setWeights(JSON.stringify(Object.values(returns.weights).map(w => Math.round(w * 1000) / 1000)));
        }
      }
    } catch (err) {
      setError(`Failed to load portfolio data: ${err}`);
    } finally {
      setIsLoadingPortfolio(false);
    }
  };

  const checkFortitudoStatus = async () => {
    try {
      const status = await fortitudoService.checkStatus();
      setIsFortitudoAvailable(status.available);
      if (!status.available) {
        setError('Fortitudo.tech library not installed. Run: pip install fortitudo.tech cvxopt');
      }
    } catch (err) {
      setIsFortitudoAvailable(false);
      setError('Failed to check fortitudo.tech status');
    }
  };

  const runPortfolioAnalysis = async () => {
    setIsLoading(true);
    setError(null);

    try {
      let returnsData: Record<string, Record<string, number>>;
      let weightsArray: number[];

      if (dataSource === 'portfolio' && portfolioReturnsData) {
        // Use user portfolio data
        returnsData = portfolioReturnsData.returns;
        weightsArray = Object.values(portfolioReturnsData.weights);
      } else {
        // Use sample data
        returnsData = SAMPLE_RETURNS;
        weightsArray = JSON.parse(weights);
      }

      const result = await fortitudoService.fullAnalysis(
        returnsData,
        weightsArray,
        alpha,
        halfLife
      );

      if (result.success && result.analysis) {
        // Add asset names from portfolio data if available
        if (dataSource === 'portfolio' && portfolioReturnsData) {
          result.analysis.assets = portfolioReturnsData.assets;
        }
        setAnalysisResult(result.analysis);
      } else {
        setError(result.error || 'Analysis failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runOptionPricing = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const result = await fortitudoService.optionPricing(
        spotPrice,
        strike,
        volatility,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      );

      if (result.success) {
        setOptionResult(result);
      } else {
        setError(result.error || 'Option pricing failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runOptionGreeks = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const result = await fortitudoService.optionGreeks(
        spotPrice,
        strike,
        volatility,
        riskFreeRate,
        dividendYield,
        timeToMaturity
      );

      if (result.success) {
        setGreeksResult(result);
      } else {
        setError(result.error || 'Greeks calculation failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runEntropyPooling = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const result = await fortitudoService.entropyPooling(nScenarios, maxProbability);

      if (result.success) {
        setEntropyResult(result);
      } else {
        setError(result.error || 'Entropy pooling failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runOptimization = async () => {
    setIsLoading(true);
    setError(null);

    try {
      // Use portfolio data if available, otherwise sample data
      const returnsData = (dataSource === 'portfolio' && portfolioReturnsData)
        ? portfolioReturnsData.returns
        : SAMPLE_RETURNS;

      let result: OptimizationResult;

      if (optimizationType === 'mv') {
        result = await fortitudoService.optimizeMeanVariance(
          returnsData,
          optimizationObjective,
          optLongOnly,
          optMaxWeight,
          optMinWeight,
          riskFreeRate,
          optimizationObjective === 'target_return' ? optTargetReturn : undefined
        );
      } else {
        result = await fortitudoService.optimizeMeanCVaR(
          returnsData,
          optimizationObjective === 'min_variance' ? 'min_cvar' : optimizationObjective,
          optAlpha,
          optLongOnly,
          optMaxWeight,
          optMinWeight,
          riskFreeRate,
          optimizationObjective === 'target_return' ? optTargetReturn : undefined
        );
      }

      // Add asset names from portfolio if available
      if (result.success && dataSource === 'portfolio' && portfolioReturnsData) {
        result.assets = portfolioReturnsData.assets;
      }

      if (result.success) {
        setOptimizationResult(result);
      } else {
        setError(result.error || 'Optimization failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const runEfficientFrontier = async () => {
    setIsLoading(true);
    setError(null);

    try {
      // Use portfolio data if available, otherwise sample data
      const returnsData = (dataSource === 'portfolio' && portfolioReturnsData)
        ? portfolioReturnsData.returns
        : SAMPLE_RETURNS;

      let result: EfficientFrontierResponse;

      if (optimizationType === 'mv') {
        result = await fortitudoService.efficientFrontierMV(
          returnsData,
          nFrontierPoints,
          optLongOnly,
          optMaxWeight,
          riskFreeRate
        );
      } else {
        result = await fortitudoService.efficientFrontierCVaR(
          returnsData,
          nFrontierPoints,
          optAlpha,
          optLongOnly,
          optMaxWeight
        );
      }

      // Add asset names from portfolio if available
      if (result.success && dataSource === 'portfolio' && portfolioReturnsData) {
        result.assets = portfolioReturnsData.assets;
      }

      if (result.success) {
        setFrontierResult(result);
        setSelectedFrontierIndex(null); // Reset selection on new data
      } else {
        setError(result.error || 'Efficient frontier generation failed');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsLoading(false);
    }
  };

  const toggleSection = (section: keyof typeof expandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const formatPercent = (value: number | null | undefined) => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return `${(value * 100).toFixed(2)}%`;
  };

  const formatRatio = (value: number | null | undefined) => {
    if (value === null || value === undefined || isNaN(value)) return 'N/A';
    return value.toFixed(3);
  };

  // Helper to normalize weights from object or array to array with asset names
  const normalizeWeights = (
    weights: number[] | Record<string, number> | undefined,
    assets: string[] | undefined
  ): { asset: string; weight: number }[] => {
    if (!weights) return [];

    if (Array.isArray(weights)) {
      return weights.map((w, idx) => ({
        asset: assets?.[idx] || `Asset ${idx + 1}`,
        weight: w
      }));
    } else {
      // weights is an object {assetName: weight}
      return Object.entries(weights).map(([asset, weight]) => ({
        asset,
        weight: weight as number
      }));
    }
  };

  // Render unavailable state
  if (isFortitudoAvailable === false) {
    return (
      <div className="flex items-center justify-center h-full p-8" style={{ backgroundColor: FINCEPT.DARK_BG }}>
        <div className="max-w-lg text-center">
          <AlertCircle size={64} color={FINCEPT.RED} className="mx-auto mb-6" />
          <h2 className="text-2xl font-bold uppercase tracking-wide mb-4" style={{ color: FINCEPT.WHITE }}>
            FORTITUDO.TECH NOT INSTALLED
          </h2>
          <p className="text-sm font-mono mb-6" style={{ color: FINCEPT.GRAY }}>
            Install the library to enable portfolio risk analytics:
          </p>
          <code
            className="block p-4 rounded text-sm font-mono mb-6"
            style={{ backgroundColor: FINCEPT.PANEL_BG, color: FINCEPT.ORANGE }}
          >
            pip install fortitudo.tech cvxopt
          </code>
          <button
            onClick={checkFortitudoStatus}
            className="px-6 py-2 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors inline-flex items-center gap-2"
            style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
          >
            <RefreshCw size={16} />
            RETRY
          </button>
        </div>
      </div>
    );
  }

  // Loading state
  if (isFortitudoAvailable === null) {
    return (
      <div className="flex items-center justify-center h-full" style={{ backgroundColor: FINCEPT.DARK_BG }}>
        <RefreshCw size={48} color={FINCEPT.ORANGE} className="animate-spin" />
      </div>
    );
  }

  return (
    <div className="h-full overflow-auto" style={{ backgroundColor: FINCEPT.DARK_BG }}>
      <div className="p-4 space-y-4">
        {/* Header */}
        <div
          className="flex items-center justify-between p-3 rounded border"
          style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
        >
          <div className="flex items-center gap-3">
            <div className="p-2 rounded" style={{ backgroundColor: FINCEPT.ORANGE }}>
              <Shield size={20} color={FINCEPT.DARK_BG} />
            </div>
            <div>
              <h2 className="font-bold text-sm uppercase tracking-wide" style={{ color: FINCEPT.WHITE }}>
                FORTITUDO.TECH RISK ANALYTICS
              </h2>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                VaR, CVaR, Option Pricing, Entropy Pooling
              </p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <CheckCircle2 size={16} color={FINCEPT.GREEN} />
            <span className="text-xs font-mono" style={{ color: FINCEPT.GREEN }}>READY</span>
          </div>
        </div>

        {/* Mode Selector */}
        <div
          className="flex rounded border overflow-hidden"
          style={{ borderColor: FINCEPT.BORDER }}
        >
          {[
            { id: 'portfolio', label: 'PORTFOLIO RISK', icon: Shield },
            { id: 'options', label: 'OPTION PRICING', icon: DollarSign },
            { id: 'optimization', label: 'OPTIMIZATION', icon: Target },
            { id: 'entropy', label: 'ENTROPY', icon: Layers }
          ].map(mode => (
            <button
              key={mode.id}
              onClick={() => setActiveMode(mode.id as AnalysisMode)}
              className="flex-1 flex items-center justify-center gap-2 px-4 py-2.5 text-xs font-semibold uppercase tracking-wide transition-all"
              style={{
                backgroundColor: activeMode === mode.id ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                color: activeMode === mode.id ? FINCEPT.DARK_BG : FINCEPT.GRAY
              }}
            >
              <mode.icon size={14} />
              {mode.label}
            </button>
          ))}
        </div>

        {/* Data Source Selector */}
        <div
          className="p-3 rounded border"
          style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
        >
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <Database size={16} color={FINCEPT.CYAN} />
              <span className="text-xs font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
                DATA SOURCE
              </span>
            </div>
            <div className="flex items-center gap-2">
              <div className="flex rounded border overflow-hidden" style={{ borderColor: FINCEPT.BORDER }}>
                <button
                  onClick={() => setDataSource('sample')}
                  className="px-3 py-1.5 text-xs font-bold uppercase transition-all"
                  style={{
                    backgroundColor: dataSource === 'sample' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                    color: dataSource === 'sample' ? FINCEPT.DARK_BG : FINCEPT.GRAY
                  }}
                >
                  SAMPLE DATA
                </button>
                <button
                  onClick={() => setDataSource('portfolio')}
                  className="px-3 py-1.5 text-xs font-bold uppercase transition-all flex items-center gap-1"
                  style={{
                    backgroundColor: dataSource === 'portfolio' ? FINCEPT.CYAN : FINCEPT.DARK_BG,
                    color: dataSource === 'portfolio' ? FINCEPT.DARK_BG : FINCEPT.GRAY
                  }}
                >
                  <Briefcase size={12} />
                  MY PORTFOLIOS
                </button>
              </div>
            </div>
          </div>

          {/* Portfolio Selector (shown when portfolio data source is selected) */}
          {dataSource === 'portfolio' && (
            <div className="mt-3 pt-3 border-t" style={{ borderColor: FINCEPT.BORDER }}>
              <div className="grid grid-cols-3 gap-4">
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    SELECT PORTFOLIO
                  </label>
                  <select
                    value={selectedPortfolioId || ''}
                    onChange={e => setSelectedPortfolioId(e.target.value || null)}
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  >
                    <option value="">-- Select Portfolio --</option>
                    {userPortfolios.map(p => (
                      <option key={p.id} value={p.id}>{p.name}</option>
                    ))}
                  </select>
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    HISTORICAL DAYS
                  </label>
                  <input
                    type="number"
                    value={historicalDays}
                    onChange={e => setHistoricalDays(parseInt(e.target.value))}
                    min="30"
                    max="1000"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div className="flex items-end">
                  <button
                    onClick={() => selectedPortfolioId && loadPortfolioData(selectedPortfolioId)}
                    disabled={!selectedPortfolioId || isLoadingPortfolio}
                    className="px-4 py-2 rounded font-bold uppercase tracking-wide text-xs hover:bg-opacity-90 transition-colors flex items-center gap-2 disabled:opacity-50"
                    style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
                  >
                    {isLoadingPortfolio ? (
                      <RefreshCw size={14} className="animate-spin" />
                    ) : (
                      <RefreshCw size={14} />
                    )}
                    LOAD DATA
                  </button>
                </div>
              </div>

              {/* Portfolio Summary */}
              {selectedPortfolioSummary && portfolioReturnsData && (
                <div className="mt-3 p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                  <div className="flex items-center gap-6 text-xs font-mono">
                    <span style={{ color: FINCEPT.GRAY }}>
                      Portfolio: <span style={{ color: FINCEPT.WHITE }}>{selectedPortfolioSummary.portfolio.name}</span>
                    </span>
                    <span style={{ color: FINCEPT.GRAY }}>
                      Holdings: <span style={{ color: FINCEPT.CYAN }}>{selectedPortfolioSummary.holdings.length}</span>
                    </span>
                    <span style={{ color: FINCEPT.GRAY }}>
                      Data Points: <span style={{ color: FINCEPT.CYAN }}>{portfolioReturnsData.nScenarios}</span>
                    </span>
                    <span style={{ color: FINCEPT.GRAY }}>
                      Period: <span style={{ color: FINCEPT.WHITE }}>{portfolioReturnsData.startDate} to {portfolioReturnsData.endDate}</span>
                    </span>
                  </div>
                  <div className="mt-2 flex flex-wrap gap-2">
                    {portfolioReturnsData.assets.map((asset, idx) => (
                      <span
                        key={asset}
                        className="px-2 py-0.5 rounded text-xs font-mono"
                        style={{ backgroundColor: FINCEPT.BORDER, color: FINCEPT.WHITE }}
                      >
                        {asset}: {(portfolioReturnsData.weights[asset] * 100).toFixed(1)}%
                      </span>
                    ))}
                  </div>
                </div>
              )}

              {/* No portfolios message */}
              {userPortfolios.length === 0 && (
                <div className="mt-3 p-3 rounded text-center" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                  <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                    No portfolios found. Create a portfolio in the Portfolio tab first.
                  </p>
                </div>
              )}
            </div>
          )}
        </div>

        {/* Error Display */}
        {error && (
          <div
            className="flex items-start gap-3 p-3 rounded border"
            style={{ backgroundColor: '#1a0000', borderColor: FINCEPT.RED }}
          >
            <AlertCircle size={18} color={FINCEPT.RED} className="flex-shrink-0 mt-0.5" />
            <div>
              <p className="text-sm font-bold" style={{ color: FINCEPT.RED }}>Error</p>
              <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>{error}</p>
            </div>
          </div>
        )}

        {/* Portfolio Risk Mode */}
        {activeMode === 'portfolio' && (
          <>
            {/* Configuration */}
            <div
              className="rounded border"
              style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
            >
              <button
                onClick={() => toggleSection('config')}
                className="w-full flex items-center justify-between p-3"
              >
                <div className="flex items-center gap-2">
                  <Settings size={16} color={FINCEPT.ORANGE} />
                  <span className="text-sm font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
                    CONFIGURATION
                  </span>
                </div>
                {expandedSections.config ? (
                  <ChevronUp size={16} color={FINCEPT.GRAY} />
                ) : (
                  <ChevronDown size={16} color={FINCEPT.GRAY} />
                )}
              </button>

              {expandedSections.config && (
                <div className="p-4 border-t space-y-4" style={{ borderColor: FINCEPT.BORDER }}>
                  <div className="grid grid-cols-3 gap-4">
                    <div>
                      <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                        PORTFOLIO WEIGHTS
                      </label>
                      <input
                        type="text"
                        value={weights}
                        onChange={e => setWeights(e.target.value)}
                        className="w-full px-3 py-2 rounded text-sm font-mono border"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          borderColor: FINCEPT.BORDER,
                          color: FINCEPT.WHITE
                        }}
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                        ALPHA (VaR/CVaR)
                      </label>
                      <input
                        type="number"
                        value={alpha}
                        onChange={e => setAlpha(parseFloat(e.target.value))}
                        step="0.01"
                        min="0.01"
                        max="0.50"
                        className="w-full px-3 py-2 rounded text-sm font-mono border"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          borderColor: FINCEPT.BORDER,
                          color: FINCEPT.WHITE
                        }}
                      />
                    </div>
                    <div>
                      <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                        HALF-LIFE (DAYS)
                      </label>
                      <input
                        type="number"
                        value={halfLife}
                        onChange={e => setHalfLife(parseInt(e.target.value))}
                        min="20"
                        max="504"
                        className="w-full px-3 py-2 rounded text-sm font-mono border"
                        style={{
                          backgroundColor: FINCEPT.DARK_BG,
                          borderColor: FINCEPT.BORDER,
                          color: FINCEPT.WHITE
                        }}
                      />
                    </div>
                  </div>

                  <button
                    onClick={runPortfolioAnalysis}
                    disabled={isLoading}
                    className="w-full py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                    style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
                  >
                    {isLoading ? (
                      <RefreshCw size={16} className="animate-spin" />
                    ) : (
                      <Play size={16} />
                    )}
                    RUN PORTFOLIO ANALYSIS
                  </button>
                </div>
              )}
            </div>

            {/* Results */}
            {analysisResult && (
              <>
                {/* Metrics Comparison */}
                <div
                  className="rounded border"
                  style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
                >
                  <button
                    onClick={() => toggleSection('metrics')}
                    className="w-full flex items-center justify-between p-3"
                  >
                    <div className="flex items-center gap-2">
                      <BarChart2 size={16} color={FINCEPT.CYAN} />
                      <span className="text-sm font-bold uppercase" style={{ color: FINCEPT.WHITE }}>
                        RISK METRICS
                      </span>
                    </div>
                    {expandedSections.metrics ? (
                      <ChevronUp size={16} color={FINCEPT.GRAY} />
                    ) : (
                      <ChevronDown size={16} color={FINCEPT.GRAY} />
                    )}
                  </button>

                  {expandedSections.metrics && (
                    <div className="p-4 border-t" style={{ borderColor: FINCEPT.BORDER }}>
                      <div className="grid grid-cols-2 gap-6">
                        {/* Equal Weight Metrics */}
                        <div>
                          <h3 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.ORANGE }}>
                            EQUAL WEIGHT SCENARIOS
                          </h3>
                          {analysisResult.metrics_equal_weight && (
                            <div className="space-y-2">
                              <MetricRow
                                label="Expected Return"
                                value={formatPercent(analysisResult.metrics_equal_weight.expected_return)}
                                positive={analysisResult.metrics_equal_weight.expected_return > 0}
                              />
                              <MetricRow
                                label="Volatility"
                                value={formatPercent(analysisResult.metrics_equal_weight.volatility)}
                              />
                              <MetricRow
                                label={`VaR (${(1 - alpha) * 100}%)`}
                                value={formatPercent(analysisResult.metrics_equal_weight.var)}
                                negative
                              />
                              <MetricRow
                                label={`CVaR (${(1 - alpha) * 100}%)`}
                                value={formatPercent(analysisResult.metrics_equal_weight.cvar)}
                                negative
                              />
                              <MetricRow
                                label="Sharpe Ratio"
                                value={formatRatio(analysisResult.metrics_equal_weight.sharpe_ratio)}
                                positive={analysisResult.metrics_equal_weight.sharpe_ratio > 0}
                              />
                            </div>
                          )}
                        </div>

                        {/* Exp Decay Metrics */}
                        <div>
                          <h3 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.CYAN }}>
                            EXP DECAY WEIGHTED ({halfLife}d)
                          </h3>
                          {analysisResult.metrics_exp_decay && (
                            <div className="space-y-2">
                              <MetricRow
                                label="Expected Return"
                                value={formatPercent(analysisResult.metrics_exp_decay.expected_return)}
                                positive={analysisResult.metrics_exp_decay.expected_return > 0}
                              />
                              <MetricRow
                                label="Volatility"
                                value={formatPercent(analysisResult.metrics_exp_decay.volatility)}
                              />
                              <MetricRow
                                label={`VaR (${(1 - alpha) * 100}%)`}
                                value={formatPercent(analysisResult.metrics_exp_decay.var)}
                                negative
                              />
                              <MetricRow
                                label={`CVaR (${(1 - alpha) * 100}%)`}
                                value={formatPercent(analysisResult.metrics_exp_decay.cvar)}
                                negative
                              />
                              <MetricRow
                                label="Sharpe Ratio"
                                value={formatRatio(analysisResult.metrics_exp_decay.sharpe_ratio)}
                                positive={analysisResult.metrics_exp_decay.sharpe_ratio > 0}
                              />
                            </div>
                          )}
                        </div>
                      </div>

                      {/* Summary */}
                      <div
                        className="mt-4 p-3 rounded"
                        style={{ backgroundColor: FINCEPT.DARK_BG }}
                      >
                        <div className="flex items-center gap-6 text-xs font-mono">
                          <span style={{ color: FINCEPT.GRAY }}>
                            Scenarios: <span style={{ color: FINCEPT.WHITE }}>{analysisResult.n_scenarios}</span>
                          </span>
                          <span style={{ color: FINCEPT.GRAY }}>
                            Assets: <span style={{ color: FINCEPT.WHITE }}>{analysisResult.n_assets}</span>
                          </span>
                          <span style={{ color: FINCEPT.GRAY }}>
                            Alpha: <span style={{ color: FINCEPT.WHITE }}>{alpha}</span>
                          </span>
                        </div>
                      </div>
                    </div>
                  )}
                </div>
              </>
            )}
          </>
        )}

        {/* Option Pricing Mode */}
        {activeMode === 'options' && (
          <div
            className="rounded border"
            style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
          >
            <div className="p-4 space-y-4">
              <div className="grid grid-cols-6 gap-4">
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    SPOT PRICE
                  </label>
                  <input
                    type="number"
                    value={spotPrice}
                    onChange={e => setSpotPrice(parseFloat(e.target.value))}
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    STRIKE
                  </label>
                  <input
                    type="number"
                    value={strike}
                    onChange={e => setStrike(parseFloat(e.target.value))}
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    VOLATILITY
                  </label>
                  <input
                    type="number"
                    value={volatility}
                    onChange={e => setVolatility(parseFloat(e.target.value))}
                    step="0.01"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    RISK-FREE RATE
                  </label>
                  <input
                    type="number"
                    value={riskFreeRate}
                    onChange={e => setRiskFreeRate(parseFloat(e.target.value))}
                    step="0.01"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    DIV YIELD
                  </label>
                  <input
                    type="number"
                    value={dividendYield}
                    onChange={e => setDividendYield(parseFloat(e.target.value))}
                    step="0.01"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    TIME (YEARS)
                  </label>
                  <input
                    type="number"
                    value={timeToMaturity}
                    onChange={e => setTimeToMaturity(parseFloat(e.target.value))}
                    step="0.1"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
              </div>

              <div className="grid grid-cols-2 gap-4">
                <button
                  onClick={runOptionPricing}
                  disabled={isLoading}
                  className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                  style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
                >
                  {isLoading ? (
                    <RefreshCw size={16} className="animate-spin" />
                  ) : (
                    <DollarSign size={16} />
                  )}
                  PRICE OPTIONS
                </button>
                <button
                  onClick={runOptionGreeks}
                  disabled={isLoading}
                  className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                  style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
                >
                  {isLoading ? (
                    <RefreshCw size={16} className="animate-spin" />
                  ) : (
                    <Activity size={16} />
                  )}
                  CALCULATE GREEKS
                </button>
              </div>

              {/* Option Results */}
              {optionResult && optionResult.success && (
                <div className="grid grid-cols-4 gap-4 mt-4">
                  <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                    <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>FORWARD</p>
                    <p className="text-lg font-bold" style={{ color: FINCEPT.WHITE }}>
                      ${optionResult.forward_price?.toFixed(2)}
                    </p>
                  </div>
                  <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                    <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>CALL PRICE</p>
                    <p className="text-lg font-bold" style={{ color: FINCEPT.GREEN }}>
                      ${optionResult.call_price?.toFixed(4)}
                    </p>
                  </div>
                  <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                    <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>PUT PRICE</p>
                    <p className="text-lg font-bold" style={{ color: FINCEPT.RED }}>
                      ${optionResult.put_price?.toFixed(4)}
                    </p>
                  </div>
                  <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                    <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>STRADDLE</p>
                    <p className="text-lg font-bold" style={{ color: FINCEPT.CYAN }}>
                      ${optionResult.straddle?.straddle_price?.toFixed(4)}
                    </p>
                  </div>
                </div>
              )}

              {/* Greeks Results */}
              {greeksResult && greeksResult.success && (
                <div className="mt-4 p-4 rounded border" style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}>
                  <h3 className="text-sm font-bold uppercase mb-4" style={{ color: FINCEPT.CYAN }}>
                    OPTION GREEKS
                  </h3>
                  <div className="grid grid-cols-2 gap-6">
                    {/* Call Greeks */}
                    <div>
                      <h4 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.GREEN }}>
                        CALL OPTION
                      </h4>
                      <div className="space-y-2">
                        <GreekRow label="Delta (Δ)" value={greeksResult.call?.delta} description="Price sensitivity" />
                        <GreekRow label="Gamma (Γ)" value={greeksResult.call?.gamma} description="Delta sensitivity" />
                        <GreekRow label="Vega (ν)" value={greeksResult.call?.vega} description="Volatility sensitivity" />
                        <GreekRow label="Theta (Θ)" value={greeksResult.call?.theta} description="Time decay (daily)" />
                        <GreekRow label="Rho (ρ)" value={greeksResult.call?.rho} description="Rate sensitivity" />
                      </div>
                    </div>
                    {/* Put Greeks */}
                    <div>
                      <h4 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.RED }}>
                        PUT OPTION
                      </h4>
                      <div className="space-y-2">
                        <GreekRow label="Delta (Δ)" value={greeksResult.put?.delta} description="Price sensitivity" />
                        <GreekRow label="Gamma (Γ)" value={greeksResult.put?.gamma} description="Delta sensitivity" />
                        <GreekRow label="Vega (ν)" value={greeksResult.put?.vega} description="Volatility sensitivity" />
                        <GreekRow label="Theta (Θ)" value={greeksResult.put?.theta} description="Time decay (daily)" />
                        <GreekRow label="Rho (ρ)" value={greeksResult.put?.rho} description="Rate sensitivity" />
                      </div>
                    </div>
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Portfolio Optimization Mode */}
        {activeMode === 'optimization' && (
          <div
            className="rounded border"
            style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
          >
            <div className="p-4 space-y-4">
              {/* Optimization Type Toggle */}
              <div className="flex rounded border overflow-hidden" style={{ borderColor: FINCEPT.BORDER }}>
                <button
                  onClick={() => setOptimizationType('mv')}
                  className="flex-1 py-2 text-xs font-bold uppercase tracking-wide"
                  style={{
                    backgroundColor: optimizationType === 'mv' ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                    color: optimizationType === 'mv' ? FINCEPT.DARK_BG : FINCEPT.GRAY
                  }}
                >
                  MEAN-VARIANCE
                </button>
                <button
                  onClick={() => setOptimizationType('cvar')}
                  className="flex-1 py-2 text-xs font-bold uppercase tracking-wide"
                  style={{
                    backgroundColor: optimizationType === 'cvar' ? FINCEPT.CYAN : FINCEPT.DARK_BG,
                    color: optimizationType === 'cvar' ? FINCEPT.DARK_BG : FINCEPT.GRAY
                  }}
                >
                  MEAN-CVaR
                </button>
              </div>

              {/* Configuration */}
              <div className="grid grid-cols-4 gap-4">
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    OBJECTIVE
                  </label>
                  <select
                    value={optimizationObjective}
                    onChange={e => setOptimizationObjective(e.target.value as OptimizationObjective)}
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  >
                    <option value="min_variance">{optimizationType === 'mv' ? 'Min Variance' : 'Min CVaR'}</option>
                    <option value="max_sharpe">Max Sharpe</option>
                    <option value="target_return">Target Return</option>
                  </select>
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    MAX WEIGHT
                  </label>
                  <input
                    type="number"
                    value={optMaxWeight}
                    onChange={e => setOptMaxWeight(parseFloat(e.target.value))}
                    step="0.05"
                    min="0.1"
                    max="1.0"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                {optimizationObjective === 'target_return' && (
                  <div>
                    <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                      TARGET RETURN
                    </label>
                    <input
                      type="number"
                      value={optTargetReturn}
                      onChange={e => setOptTargetReturn(parseFloat(e.target.value))}
                      step="0.01"
                      className="w-full px-3 py-2 rounded text-sm font-mono border"
                      style={{
                        backgroundColor: FINCEPT.DARK_BG,
                        borderColor: FINCEPT.BORDER,
                        color: FINCEPT.WHITE
                      }}
                    />
                  </div>
                )}
                {optimizationType === 'cvar' && (
                  <div>
                    <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                      CVaR ALPHA
                    </label>
                    <input
                      type="number"
                      value={optAlpha}
                      onChange={e => setOptAlpha(parseFloat(e.target.value))}
                      step="0.01"
                      min="0.01"
                      max="0.50"
                      className="w-full px-3 py-2 rounded text-sm font-mono border"
                      style={{
                        backgroundColor: FINCEPT.DARK_BG,
                        borderColor: FINCEPT.BORDER,
                        color: FINCEPT.WHITE
                      }}
                    />
                  </div>
                )}
                <div className="flex items-end">
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={optLongOnly}
                      onChange={e => setOptLongOnly(e.target.checked)}
                      className="w-4 h-4"
                    />
                    <span className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>LONG ONLY</span>
                  </label>
                </div>
              </div>

              {/* Buttons */}
              <div className="grid grid-cols-2 gap-4">
                <button
                  onClick={runOptimization}
                  disabled={isLoading}
                  className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                  style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
                >
                  {isLoading ? (
                    <RefreshCw size={16} className="animate-spin" />
                  ) : (
                    <Target size={16} />
                  )}
                  OPTIMIZE PORTFOLIO
                </button>
                <button
                  onClick={runEfficientFrontier}
                  disabled={isLoading}
                  className="py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                  style={{ backgroundColor: FINCEPT.CYAN, color: FINCEPT.DARK_BG }}
                >
                  {isLoading ? (
                    <RefreshCw size={16} className="animate-spin" />
                  ) : (
                    <TrendingUp size={16} />
                  )}
                  EFFICIENT FRONTIER
                </button>
              </div>

              {/* Optimization Result */}
              {optimizationResult && optimizationResult.success && (() => {
                const normalizedWeights = normalizeWeights(optimizationResult.weights, optimizationResult.assets);
                const PIE_COLORS = [FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.PURPLE, FINCEPT.YELLOW, FINCEPT.BLUE, FINCEPT.RED];
                return (
                <div className="p-4 rounded border" style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}>
                  <h3 className="text-sm font-bold uppercase mb-4" style={{ color: FINCEPT.ORANGE }}>
                    OPTIMAL PORTFOLIO
                  </h3>
                  <div className="grid grid-cols-3 gap-6">
                    {/* Pie Chart */}
                    <div>
                      <h4 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.CYAN }}>
                        ALLOCATION
                      </h4>
                      <div style={{ height: 180 }}>
                        <ResponsiveContainer width="100%" height="100%">
                          <PieChart>
                            <Pie
                              data={normalizedWeights
                                .filter(item => item.weight > 0.001)
                                .map((item, idx) => ({
                                  name: item.asset,
                                  value: Math.round(item.weight * 1000) / 10,
                                  weight: item.weight,
                                  fill: PIE_COLORS[idx % PIE_COLORS.length]
                                }))}
                              cx="50%"
                              cy="50%"
                              innerRadius={35}
                              outerRadius={60}
                              paddingAngle={2}
                              dataKey="value"
                              stroke={FINCEPT.DARK_BG}
                              strokeWidth={2}
                            >
                              {normalizedWeights
                                .filter(item => item.weight > 0.001)
                                .map((_, idx) => (
                                  <Cell key={idx} fill={PIE_COLORS[idx % PIE_COLORS.length]} />
                                ))}
                            </Pie>
                            <Tooltip
                              contentStyle={{
                                backgroundColor: FINCEPT.PANEL_BG,
                                border: `1px solid ${FINCEPT.BORDER}`,
                                borderRadius: 4,
                                padding: '6px 10px'
                              }}
                              formatter={(value: number) => [`${value.toFixed(1)}%`, 'Weight']}
                              labelStyle={{ color: FINCEPT.WHITE, fontWeight: 'bold', fontSize: 11 }}
                              itemStyle={{ color: FINCEPT.GRAY, fontSize: 10 }}
                            />
                          </PieChart>
                        </ResponsiveContainer>
                      </div>
                      {/* Pie Legend */}
                      <div className="mt-2 space-y-1">
                        {normalizedWeights
                          .filter(item => item.weight > 0.001)
                          .map((item, idx) => (
                            <div key={idx} className="flex items-center gap-2">
                              <div className="w-2 h-2 rounded-full" style={{ backgroundColor: PIE_COLORS[idx % PIE_COLORS.length] }} />
                              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>{item.asset}</span>
                              <span className="text-xs font-mono font-bold ml-auto" style={{ color: FINCEPT.WHITE }}>
                                {(item.weight * 100).toFixed(1)}%
                              </span>
                            </div>
                          ))}
                      </div>
                    </div>
                    {/* Weight Bars */}
                    <div>
                      <h4 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.CYAN }}>
                        WEIGHTS
                      </h4>
                      <div className="space-y-2">
                        {normalizedWeights.map((item, i) => (
                          <div key={i} className="flex items-center justify-between">
                            <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                              {item.asset}
                            </span>
                            <div className="flex items-center gap-2">
                              <div
                                className="h-2 rounded"
                                style={{
                                  width: `${Math.max(item.weight * 100, 5)}px`,
                                  backgroundColor: item.weight > 0 ? FINCEPT.GREEN : FINCEPT.RED
                                }}
                              />
                              <span className="text-sm font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                                {(item.weight * 100).toFixed(1)}%
                              </span>
                            </div>
                          </div>
                        ))}
                      </div>
                    </div>
                    {/* Metrics */}
                    <div>
                      <h4 className="text-xs font-bold uppercase mb-3" style={{ color: FINCEPT.CYAN }}>
                        METRICS
                      </h4>
                      <div className="space-y-2">
                        <MetricRow
                          label="Expected Return"
                          value={formatPercent(optimizationResult.expected_return)}
                          positive={(optimizationResult.expected_return ?? 0) > 0}
                        />
                        <MetricRow
                          label="Volatility"
                          value={formatPercent(optimizationResult.volatility)}
                        />
                        <MetricRow
                          label="Sharpe Ratio"
                          value={formatRatio(optimizationResult.sharpe_ratio)}
                          positive={(optimizationResult.sharpe_ratio ?? 0) > 0}
                        />
                        {optimizationResult.cvar !== undefined && (
                          <MetricRow
                            label="CVaR"
                            value={formatPercent(optimizationResult.cvar)}
                            negative
                          />
                        )}
                      </div>
                      {/* Optimization Info */}
                      <div className="mt-4 p-2 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                        <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                          Type: <span style={{ color: optimizationType === 'mv' ? FINCEPT.ORANGE : FINCEPT.CYAN }}>
                            {optimizationType === 'mv' ? 'Mean-Variance' : 'Mean-CVaR'}
                          </span>
                        </p>
                        <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
                          Objective: <span style={{ color: FINCEPT.WHITE }}>{optimizationObjective.replace('_', ' ').toUpperCase()}</span>
                        </p>
                        <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
                          Constraints: <span style={{ color: FINCEPT.WHITE }}>{optLongOnly ? 'Long Only' : 'Allow Short'}</span>
                        </p>
                      </div>
                    </div>
                  </div>
                </div>
                );
              })()}

              {/* Efficient Frontier Result */}
              {frontierResult && frontierResult.success && frontierResult.frontier && (
                <div className="p-4 rounded border" style={{ backgroundColor: FINCEPT.DARK_BG, borderColor: FINCEPT.BORDER }}>
                  <h3 className="text-sm font-bold uppercase mb-4" style={{ color: FINCEPT.CYAN }}>
                    EFFICIENT FRONTIER ({frontierResult.frontier.length} POINTS)
                  </h3>

                  {/* Chart Visualization */}
                  <div className="mb-4" style={{ height: 300 }}>
                    <ResponsiveContainer width="100%" height="100%">
                      <ComposedChart
                        margin={{ top: 20, right: 30, left: 20, bottom: 20 }}
                      >
                        <CartesianGrid
                          strokeDasharray="3 3"
                          stroke={FINCEPT.BORDER}
                          vertical={true}
                        />
                        <XAxis
                          type="number"
                          dataKey="risk"
                          name={optimizationType === 'mv' ? 'Volatility' : 'CVaR'}
                          domain={['auto', 'auto']}
                          tickFormatter={(value) => `${(value * 100).toFixed(1)}%`}
                          stroke={FINCEPT.GRAY}
                          tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
                          label={{
                            value: optimizationType === 'mv' ? 'VOLATILITY (%)' : 'CVaR (%)',
                            position: 'bottom',
                            fill: FINCEPT.GRAY,
                            fontSize: 10,
                            offset: 0
                          }}
                        />
                        <YAxis
                          type="number"
                          dataKey="return"
                          name="Expected Return"
                          domain={['auto', 'auto']}
                          tickFormatter={(value) => `${(value * 100).toFixed(1)}%`}
                          stroke={FINCEPT.GRAY}
                          tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
                          label={{
                            value: 'EXPECTED RETURN (%)',
                            angle: -90,
                            position: 'insideLeft',
                            fill: FINCEPT.GRAY,
                            fontSize: 10
                          }}
                        />
                        <Tooltip
                          contentStyle={{
                            backgroundColor: FINCEPT.PANEL_BG,
                            border: `1px solid ${FINCEPT.BORDER}`,
                            borderRadius: 4,
                            padding: '8px 12px'
                          }}
                          formatter={(value: number, name: string) => {
                            if (name === 'return') return [`${(value * 100).toFixed(2)}%`, 'Return'];
                            if (name === 'risk') return [`${(value * 100).toFixed(2)}%`, optimizationType === 'mv' ? 'Volatility' : 'CVaR'];
                            if (name === 'sharpe') return [value.toFixed(3), 'Sharpe'];
                            return [value, name];
                          }}
                          labelStyle={{ color: FINCEPT.WHITE }}
                          itemStyle={{ color: FINCEPT.GRAY }}
                        />
                        {/* Frontier Line */}
                        <Line
                          data={frontierResult.frontier.map((point, idx) => ({
                            risk: point.volatility ?? point.cvar ?? 0,
                            return: point.expected_return,
                            sharpe: point.sharpe_ratio ?? 0,
                            index: idx
                          }))}
                          type="monotone"
                          dataKey="return"
                          stroke={FINCEPT.CYAN}
                          strokeWidth={2}
                          dot={false}
                        />
                        {/* Frontier Points */}
                        <Scatter
                          data={frontierResult.frontier.map((point, idx) => ({
                            risk: point.volatility ?? point.cvar ?? 0,
                            return: point.expected_return,
                            sharpe: point.sharpe_ratio ?? 0,
                            index: idx,
                            isMaxSharpe: point.sharpe_ratio === Math.max(...frontierResult.frontier!.map(p => p.sharpe_ratio ?? 0))
                          }))}
                          fill={FINCEPT.ORANGE}
                          onClick={(data: any) => {
                            if (data && typeof data.index === 'number') {
                              setSelectedFrontierIndex(data.index);
                            }
                          }}
                          cursor="pointer"
                        >
                          {frontierResult.frontier.map((point, idx) => {
                            const maxSharpe = Math.max(...frontierResult.frontier!.map(p => p.sharpe_ratio ?? 0));
                            const isMaxSharpe = point.sharpe_ratio === maxSharpe;
                            const isMinRisk = idx === 0;
                            const isSelected = selectedFrontierIndex === idx;
                            return (
                              <Cell
                                key={idx}
                                fill={isSelected ? FINCEPT.YELLOW : isMaxSharpe ? FINCEPT.GREEN : isMinRisk ? FINCEPT.ORANGE : FINCEPT.CYAN}
                                stroke={isSelected ? FINCEPT.WHITE : 'none'}
                                strokeWidth={isSelected ? 2 : 0}
                                r={isSelected ? 8 : isMaxSharpe || isMinRisk ? 6 : 4}
                              />
                            );
                          })}
                        </Scatter>
                      </ComposedChart>
                    </ResponsiveContainer>
                  </div>

                  {/* Legend */}
                  <div className="flex items-center justify-center gap-6 mb-4">
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 rounded-full" style={{ backgroundColor: FINCEPT.ORANGE }} />
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>MIN RISK</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 rounded-full" style={{ backgroundColor: FINCEPT.GREEN }} />
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>MAX SHARPE</span>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 rounded-full" style={{ backgroundColor: FINCEPT.CYAN }} />
                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>FRONTIER</span>
                    </div>
                    {selectedFrontierIndex !== null && (
                      <div className="flex items-center gap-2">
                        <div className="w-3 h-3 rounded-full border-2" style={{ backgroundColor: FINCEPT.YELLOW, borderColor: FINCEPT.WHITE }} />
                        <span className="text-xs font-mono" style={{ color: FINCEPT.YELLOW }}>SELECTED</span>
                      </div>
                    )}
                  </div>

                  {/* Selected Portfolio Details */}
                  {selectedFrontierIndex !== null && frontierResult.frontier[selectedFrontierIndex] && (
                    <div
                      className="mb-4 p-4 rounded border"
                      style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.YELLOW }}
                    >
                      <div className="flex items-center justify-between mb-3">
                        <h4 className="text-sm font-bold uppercase" style={{ color: FINCEPT.YELLOW }}>
                          SELECTED PORTFOLIO (#{selectedFrontierIndex + 1})
                        </h4>
                        <button
                          onClick={() => setSelectedFrontierIndex(null)}
                          className="text-xs font-mono px-2 py-1 rounded hover:bg-opacity-80"
                          style={{ backgroundColor: FINCEPT.BORDER, color: FINCEPT.GRAY }}
                        >
                          CLEAR
                        </button>
                      </div>
                      <div className="grid grid-cols-3 gap-6">
                        {/* Pie Chart */}
                        <div>
                          <h5 className="text-xs font-bold uppercase mb-2" style={{ color: FINCEPT.CYAN }}>
                            ALLOCATION
                          </h5>
                          <div style={{ height: 180 }}>
                            <ResponsiveContainer width="100%" height="100%">
                              <PieChart>
                                <Pie
                                  data={(() => {
                                    const selectedPoint = frontierResult.frontier[selectedFrontierIndex];
                                    const weights = selectedPoint.weights;
                                    const PIE_COLORS = [FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.PURPLE, FINCEPT.YELLOW, FINCEPT.BLUE, FINCEPT.RED];
                                    if (typeof weights === 'object' && !Array.isArray(weights)) {
                                      return Object.entries(weights)
                                        .filter(([_, w]) => (w as number) > 0.001)
                                        .map(([asset, weight], idx) => ({
                                          name: asset,
                                          value: Math.round((weight as number) * 1000) / 10,
                                          fill: PIE_COLORS[idx % PIE_COLORS.length]
                                        }));
                                    } else if (Array.isArray(weights)) {
                                      return weights
                                        .filter(w => w > 0.001)
                                        .map((w, idx) => ({
                                          name: `Asset ${idx + 1}`,
                                          value: Math.round(w * 1000) / 10,
                                          fill: PIE_COLORS[idx % PIE_COLORS.length]
                                        }));
                                    }
                                    return [];
                                  })()}
                                  cx="50%"
                                  cy="50%"
                                  innerRadius={35}
                                  outerRadius={60}
                                  paddingAngle={2}
                                  dataKey="value"
                                  stroke={FINCEPT.DARK_BG}
                                  strokeWidth={2}
                                >
                                  {(() => {
                                    const selectedPoint = frontierResult.frontier[selectedFrontierIndex];
                                    const weights = selectedPoint.weights;
                                    const PIE_COLORS = [FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.PURPLE, FINCEPT.YELLOW, FINCEPT.BLUE, FINCEPT.RED];
                                    if (typeof weights === 'object' && !Array.isArray(weights)) {
                                      return Object.entries(weights)
                                        .filter(([_, w]) => (w as number) > 0.001)
                                        .map(([asset, _], idx) => (
                                          <Cell key={asset} fill={PIE_COLORS[idx % PIE_COLORS.length]} />
                                        ));
                                    } else if (Array.isArray(weights)) {
                                      return weights
                                        .filter(w => w > 0.001)
                                        .map((_, idx) => (
                                          <Cell key={idx} fill={PIE_COLORS[idx % PIE_COLORS.length]} />
                                        ));
                                    }
                                    return null;
                                  })()}
                                </Pie>
                                <Tooltip
                                  contentStyle={{
                                    backgroundColor: FINCEPT.PANEL_BG,
                                    border: `1px solid ${FINCEPT.BORDER}`,
                                    borderRadius: 4,
                                    padding: '6px 10px'
                                  }}
                                  formatter={(value: number) => [`${value.toFixed(1)}%`, 'Weight']}
                                  labelStyle={{ color: FINCEPT.WHITE, fontWeight: 'bold', fontSize: 11 }}
                                  itemStyle={{ color: FINCEPT.GRAY, fontSize: 10 }}
                                />
                              </PieChart>
                            </ResponsiveContainer>
                          </div>
                          {/* Pie Legend */}
                          <div className="mt-2 space-y-1">
                            {(() => {
                              const selectedPoint = frontierResult.frontier[selectedFrontierIndex];
                              const weights = selectedPoint.weights;
                              const PIE_COLORS = [FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.PURPLE, FINCEPT.YELLOW, FINCEPT.BLUE, FINCEPT.RED];
                              if (typeof weights === 'object' && !Array.isArray(weights)) {
                                return Object.entries(weights)
                                  .filter(([_, w]) => (w as number) > 0.001)
                                  .map(([asset, weight], idx) => (
                                    <div key={asset} className="flex items-center gap-2">
                                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: PIE_COLORS[idx % PIE_COLORS.length] }} />
                                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>{asset}</span>
                                      <span className="text-xs font-mono font-bold ml-auto" style={{ color: FINCEPT.WHITE }}>
                                        {((weight as number) * 100).toFixed(1)}%
                                      </span>
                                    </div>
                                  ));
                              } else if (Array.isArray(weights)) {
                                return weights
                                  .filter(w => w > 0.001)
                                  .map((w, idx) => (
                                    <div key={idx} className="flex items-center gap-2">
                                      <div className="w-2 h-2 rounded-full" style={{ backgroundColor: PIE_COLORS[idx % PIE_COLORS.length] }} />
                                      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Asset {idx + 1}</span>
                                      <span className="text-xs font-mono font-bold ml-auto" style={{ color: FINCEPT.WHITE }}>
                                        {(w * 100).toFixed(1)}%
                                      </span>
                                    </div>
                                  ));
                              }
                              return null;
                            })()}
                          </div>
                        </div>
                        {/* Weight Bars */}
                        <div>
                          <h5 className="text-xs font-bold uppercase mb-2" style={{ color: FINCEPT.CYAN }}>
                            WEIGHTS
                          </h5>
                          <div className="space-y-1">
                            {(() => {
                              const selectedPoint = frontierResult.frontier[selectedFrontierIndex];
                              const weights = selectedPoint.weights;
                              if (typeof weights === 'object' && !Array.isArray(weights)) {
                                return Object.entries(weights).map(([asset, weight]) => (
                                  <div key={asset} className="flex items-center justify-between">
                                    <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                                      {asset}
                                    </span>
                                    <div className="flex items-center gap-2">
                                      <div
                                        className="h-2 rounded"
                                        style={{
                                          width: `${Math.max((weight as number) * 80, 3)}px`,
                                          backgroundColor: (weight as number) > 0 ? FINCEPT.GREEN : FINCEPT.RED
                                        }}
                                      />
                                      <span className="text-xs font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                                        {((weight as number) * 100).toFixed(1)}%
                                      </span>
                                    </div>
                                  </div>
                                ));
                              } else if (Array.isArray(weights)) {
                                return weights.map((w, i) => (
                                  <div key={i} className="flex items-center justify-between">
                                    <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                                      Asset {i + 1}
                                    </span>
                                    <div className="flex items-center gap-2">
                                      <div
                                        className="h-2 rounded"
                                        style={{
                                          width: `${Math.max(w * 80, 3)}px`,
                                          backgroundColor: w > 0 ? FINCEPT.GREEN : FINCEPT.RED
                                        }}
                                      />
                                      <span className="text-xs font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                                        {(w * 100).toFixed(1)}%
                                      </span>
                                    </div>
                                  </div>
                                ));
                              }
                              return null;
                            })()}
                          </div>
                        </div>
                        {/* Metrics */}
                        <div>
                          <h5 className="text-xs font-bold uppercase mb-2" style={{ color: FINCEPT.CYAN }}>
                            METRICS
                          </h5>
                          <div className="space-y-1">
                            <div className="flex items-center justify-between py-1 border-b" style={{ borderColor: FINCEPT.BORDER }}>
                              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Expected Return</span>
                              <span className="text-sm font-mono font-bold" style={{ color: FINCEPT.GREEN }}>
                                {(frontierResult.frontier[selectedFrontierIndex].expected_return * 100).toFixed(2)}%
                              </span>
                            </div>
                            <div className="flex items-center justify-between py-1 border-b" style={{ borderColor: FINCEPT.BORDER }}>
                              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                                {optimizationType === 'mv' ? 'Volatility' : 'CVaR'}
                              </span>
                              <span className="text-sm font-mono font-bold" style={{ color: FINCEPT.RED }}>
                                {((frontierResult.frontier[selectedFrontierIndex].volatility ?? frontierResult.frontier[selectedFrontierIndex].cvar ?? 0) * 100).toFixed(2)}%
                              </span>
                            </div>
                            <div className="flex items-center justify-between py-1">
                              <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</span>
                              <span className="text-sm font-mono font-bold" style={{ color: FINCEPT.WHITE }}>
                                {frontierResult.frontier[selectedFrontierIndex].sharpe_ratio?.toFixed(3) ?? 'N/A'}
                              </span>
                            </div>
                          </div>
                          {/* Position on Frontier */}
                          <div className="mt-4 p-2 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                            <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                              Position: <span style={{ color: FINCEPT.YELLOW }}>{selectedFrontierIndex + 1}</span> of {frontierResult.frontier.length}
                            </p>
                            <p className="text-xs font-mono mt-1" style={{ color: FINCEPT.GRAY }}>
                              {selectedFrontierIndex === 0 ? (
                                <span style={{ color: FINCEPT.ORANGE }}>Minimum Risk Portfolio</span>
                              ) : selectedFrontierIndex === frontierResult.frontier.length - 1 ? (
                                <span style={{ color: FINCEPT.CYAN }}>Maximum Return Portfolio</span>
                              ) : frontierResult.frontier[selectedFrontierIndex].sharpe_ratio === Math.max(...frontierResult.frontier.map(p => p.sharpe_ratio ?? 0)) ? (
                                <span style={{ color: FINCEPT.GREEN }}>Maximum Sharpe Portfolio</span>
                              ) : (
                                <span style={{ color: FINCEPT.MUTED }}>Intermediate Portfolio</span>
                              )}
                            </p>
                          </div>
                        </div>
                      </div>
                    </div>
                  )}

                  {/* Summary Stats */}
                  <div className="grid grid-cols-3 gap-4 p-3 rounded" style={{ backgroundColor: FINCEPT.PANEL_BG }}>
                    <div className="text-center">
                      <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>MIN RISK</p>
                      <p className="text-sm font-bold" style={{ color: FINCEPT.ORANGE }}>
                        {((frontierResult.frontier[0]?.volatility ?? frontierResult.frontier[0]?.cvar ?? 0) * 100).toFixed(2)}%
                      </p>
                      <p className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
                        Return: {(frontierResult.frontier[0]?.expected_return * 100).toFixed(2)}%
                      </p>
                    </div>
                    <div className="text-center">
                      <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>MAX SHARPE</p>
                      {(() => {
                        const maxSharpePoint = frontierResult.frontier.reduce((max, p) =>
                          (p.sharpe_ratio ?? 0) > (max.sharpe_ratio ?? 0) ? p : max
                        );
                        return (
                          <>
                            <p className="text-sm font-bold" style={{ color: FINCEPT.GREEN }}>
                              {maxSharpePoint.sharpe_ratio?.toFixed(3) ?? 'N/A'}
                            </p>
                            <p className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
                              Return: {(maxSharpePoint.expected_return * 100).toFixed(2)}%
                            </p>
                          </>
                        );
                      })()}
                    </div>
                    <div className="text-center">
                      <p className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>MAX RETURN</p>
                      <p className="text-sm font-bold" style={{ color: FINCEPT.CYAN }}>
                        {(frontierResult.frontier[frontierResult.frontier.length - 1]?.expected_return * 100).toFixed(2)}%
                      </p>
                      <p className="text-xs font-mono" style={{ color: FINCEPT.MUTED }}>
                        Risk: {((frontierResult.frontier[frontierResult.frontier.length - 1]?.volatility ?? frontierResult.frontier[frontierResult.frontier.length - 1]?.cvar ?? 0) * 100).toFixed(2)}%
                      </p>
                    </div>
                  </div>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Entropy Pooling Mode */}
        {activeMode === 'entropy' && (
          <div
            className="rounded border"
            style={{ backgroundColor: FINCEPT.PANEL_BG, borderColor: FINCEPT.BORDER }}
          >
            <div className="p-4 space-y-4">
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    NUMBER OF SCENARIOS
                  </label>
                  <input
                    type="number"
                    value={nScenarios}
                    onChange={e => setNScenarios(parseInt(e.target.value))}
                    min="10"
                    max="10000"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
                <div>
                  <label className="block text-xs font-mono mb-1" style={{ color: FINCEPT.GRAY }}>
                    MAX PROBABILITY (%)
                  </label>
                  <input
                    type="number"
                    value={maxProbability * 100}
                    onChange={e => setMaxProbability(parseFloat(e.target.value) / 100)}
                    step="0.5"
                    min="0.5"
                    max="100"
                    className="w-full px-3 py-2 rounded text-sm font-mono border"
                    style={{
                      backgroundColor: FINCEPT.DARK_BG,
                      borderColor: FINCEPT.BORDER,
                      color: FINCEPT.WHITE
                    }}
                  />
                </div>
              </div>

              <button
                onClick={runEntropyPooling}
                disabled={isLoading}
                className="w-full py-2.5 rounded font-bold uppercase tracking-wide text-sm hover:bg-opacity-90 transition-colors flex items-center justify-center gap-2 disabled:opacity-50"
                style={{ backgroundColor: FINCEPT.ORANGE, color: FINCEPT.DARK_BG }}
              >
                {isLoading ? (
                  <RefreshCw size={16} className="animate-spin" />
                ) : (
                  <Layers size={16} />
                )}
                APPLY ENTROPY POOLING
              </button>

              {/* Entropy Results */}
              {entropyResult && entropyResult.success && (
                <div className="grid grid-cols-2 gap-4 mt-4">
                  <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                    <p className="text-xs font-mono mb-2" style={{ color: FINCEPT.GRAY }}>PRIOR</p>
                    <div className="space-y-1">
                      <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                        Effective Scenarios: {entropyResult.effective_scenarios_prior?.toFixed(1)}
                      </p>
                    </div>
                  </div>
                  <div className="p-3 rounded" style={{ backgroundColor: FINCEPT.DARK_BG }}>
                    <p className="text-xs font-mono mb-2" style={{ color: FINCEPT.CYAN }}>POSTERIOR</p>
                    <div className="space-y-1">
                      <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                        Effective Scenarios: {entropyResult.effective_scenarios_posterior?.toFixed(1)}
                      </p>
                      <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                        Max Prob: {(entropyResult.max_probability * 100).toFixed(2)}%
                      </p>
                      <p className="text-xs font-mono" style={{ color: FINCEPT.WHITE }}>
                        Min Prob: {(entropyResult.min_probability * 100).toFixed(4)}%
                      </p>
                    </div>
                  </div>
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

// Metric Row Component
function MetricRow({
  label,
  value,
  positive,
  negative
}: {
  label: string;
  value: string;
  positive?: boolean;
  negative?: boolean;
}) {
  let valueColor = FINCEPT.WHITE;
  if (positive) valueColor = FINCEPT.GREEN;
  if (negative) valueColor = FINCEPT.RED;

  return (
    <div className="flex items-center justify-between py-1.5 border-b" style={{ borderColor: FINCEPT.BORDER }}>
      <span className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>{label}</span>
      <span className="text-sm font-mono font-semibold" style={{ color: valueColor }}>{value}</span>
    </div>
  );
}

// Greek Row Component for displaying option Greeks
function GreekRow({
  label,
  value,
  description
}: {
  label: string;
  value: number | undefined;
  description: string;
}) {
  const formatGreek = (val: number | undefined) => {
    if (val === undefined || val === null || isNaN(val)) return 'N/A';
    return val.toFixed(4);
  };

  const getValueColor = (val: number | undefined) => {
    if (val === undefined || val === null || isNaN(val)) return FINCEPT.GRAY;
    if (val > 0) return FINCEPT.GREEN;
    if (val < 0) return FINCEPT.RED;
    return FINCEPT.WHITE;
  };

  return (
    <div className="flex items-center justify-between py-1.5 border-b" style={{ borderColor: FINCEPT.BORDER }}>
      <div>
        <span className="text-xs font-mono font-semibold" style={{ color: FINCEPT.WHITE }}>{label}</span>
        <span className="text-xs font-mono ml-2" style={{ color: FINCEPT.MUTED }}>{description}</span>
      </div>
      <span className="text-sm font-mono font-bold" style={{ color: getValueColor(value) }}>
        {formatGreek(value)}
      </span>
    </div>
  );
}
