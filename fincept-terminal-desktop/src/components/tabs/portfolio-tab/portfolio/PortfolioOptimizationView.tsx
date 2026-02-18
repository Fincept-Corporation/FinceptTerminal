import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { TrendingUp, BarChart3, PieChart, LineChart, Target, Shield, Calculator, Download, Layers, Zap, AlertTriangle, GitCompare, Brain } from 'lucide-react';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { LineChart as RechartsLine, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Scatter, ScatterChart, ZAxis } from 'recharts';
import { getSetting, saveSetting } from '@/services/core/sqliteService';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';

// Library selection
const LIBRARIES = [
  { value: 'pyportfolioopt', label: 'PyPortfolioOpt', description: 'Classical optimization' },
  { value: 'skfolio', label: 'skfolio', description: 'Modern ML-integrated optimization (2025)' },
];

// PyPortfolioOpt methods
const PYPORTFOLIOOPT_METHODS = [
  { value: 'efficient_frontier', label: 'Efficient Frontier', description: 'Mean-variance optimization' },
  { value: 'hrp', label: 'HRP (Hierarchical Risk Parity)', description: 'Machine learning based' },
  { value: 'cla', label: 'CLA (Critical Line Algorithm)', description: 'Analytical solution' },
  { value: 'black_litterman', label: 'Black-Litterman', description: 'Views-based approach' },
  { value: 'efficient_semivariance', label: 'Efficient Semivariance', description: 'Downside risk focus' },
  { value: 'efficient_cvar', label: 'Efficient CVaR', description: 'Conditional Value at Risk' },
  { value: 'efficient_cdar', label: 'Efficient CDaR', description: 'Conditional Drawdown at Risk' },
];

// skfolio methods
const SKFOLIO_METHODS = [
  { value: 'mean_risk', label: 'Mean-Risk', description: 'Mean-variance with advanced estimators' },
  { value: 'risk_parity', label: 'Risk Parity', description: 'Equal risk contribution' },
  { value: 'hrp', label: 'Hierarchical Risk Parity', description: 'Clustering-based allocation' },
  { value: 'max_div', label: 'Maximum Diversification', description: 'Maximize portfolio diversification' },
  { value: 'equal_weight', label: 'Equal Weight', description: '1/N allocation' },
  { value: 'inverse_vol', label: 'Inverse Volatility', description: 'Weight by inverse volatility' },
];

// PyPortfolioOpt objectives
const PYPORTFOLIOOPT_OBJECTIVES = [
  { value: 'max_sharpe', label: 'Maximize Sharpe Ratio' },
  { value: 'min_volatility', label: 'Minimize Volatility' },
  { value: 'max_quadratic_utility', label: 'Maximize Quadratic Utility' },
  { value: 'efficient_risk', label: 'Target Risk Level' },
  { value: 'efficient_return', label: 'Target Return Level' },
];

// skfolio objectives
const SKFOLIO_OBJECTIVES = [
  { value: 'minimize_risk', label: 'Minimize Risk' },
  { value: 'maximize_return', label: 'Maximize Return' },
  { value: 'maximize_ratio', label: 'Maximize Sharpe Ratio' },
  { value: 'maximize_utility', label: 'Maximize Utility' },
];

// skfolio risk measures
const SKFOLIO_RISK_MEASURES = [
  { value: 'variance', label: 'Variance' },
  { value: 'semi_variance', label: 'Semi-Variance' },
  { value: 'cvar', label: 'CVaR (Conditional Value at Risk)' },
  { value: 'evar', label: 'EVaR (Entropic Value at Risk)' },
  { value: 'max_drawdown', label: 'Maximum Drawdown' },
  { value: 'cdar', label: 'CDaR (Conditional Drawdown at Risk)' },
  { value: 'ulcer_index', label: 'Ulcer Index' },
];

const EXPECTED_RETURNS_METHODS = [
  { value: 'mean_historical_return', label: 'Mean Historical Return' },
  { value: 'ema_historical_return', label: 'EMA Historical Return' },
  { value: 'capm_return', label: 'CAPM Return' },
];

const RISK_MODELS = [
  { value: 'sample_cov', label: 'Sample Covariance' },
  { value: 'semicovariance', label: 'Semicovariance' },
  { value: 'exp_cov', label: 'Exponential Covariance' },
  { value: 'shrunk_covariance', label: 'Shrunk Covariance' },
  { value: 'ledoit_wolf', label: 'Ledoit-Wolf' },
];

interface OptimizationConfig {
  optimization_method: string;
  objective: string;
  expected_returns_method: string;
  risk_model_method: string;
  risk_free_rate: number;
  weight_bounds_min: number;
  weight_bounds_max: number;
  gamma: number;
  total_portfolio_value: number;
}

interface OptimizationResults {
  weights?: Record<string, number>;
  performance_metrics?: {
    expected_annual_return: number;
    annual_volatility: number;
    sharpe_ratio: number;
    sortino_ratio?: number;
    calmar_ratio?: number;
    max_drawdown?: number;
  };
  discrete_allocation?: {
    allocation: Record<string, number>;
    leftover_cash: number;
    total_value: number;
  };
  efficient_frontier?: {
    returns: number[];
    volatility: number[];
    sharpe_ratios: number[];
  };
  backtest_results?: {
    annual_return: number;
    annual_volatility: number;
    sharpe_ratio: number;
    max_drawdown: number;
    calmar_ratio: number;
  };
}

interface PortfolioOptimizationViewProps {
  portfolioSummary: PortfolioSummary | null;
}

const PortfolioOptimizationView: React.FC<PortfolioOptimizationViewProps> = ({ portfolioSummary }) => {
  // PURE SQLite - Load state from database
  const loadState = async <T,>(key: string, defaultValue: T): Promise<T> => {
    try {
      const saved = await getSetting(`portfolio_optimization_${key}`);
      return saved ? JSON.parse(saved) : defaultValue;
    } catch {
      return defaultValue;
    }
  };

  // PURE SQLite - Save state to database
  const saveState = async <T,>(key: string, value: T) => {
    try {
      await saveSetting(`portfolio_optimization_${key}`, JSON.stringify(value), 'portfolio_optimization');
    } catch {
      // Ignore storage errors
    }
  };

  const [library, setLibrary] = useState<'pyportfolioopt' | 'skfolio'>('skfolio');
  const [config, setConfig] = useState<OptimizationConfig>({
      optimization_method: 'mean_risk',
      objective: 'maximize_ratio',
      expected_returns_method: 'mean_historical_return',
      risk_model_method: 'sample_cov',
      risk_free_rate: 0.02,
      weight_bounds_min: 0,
      weight_bounds_max: 1,
      gamma: 0,
      total_portfolio_value: 10000,
    });

  const [skfolioConfig, setSkfolioConfig] = useState({
    risk_measure: 'cvar',
    covariance_estimator: 'ledoit_wolf',
    train_test_split: 0.7,
    l1_coef: 0.0,
    l2_coef: 0.0,
  });

  const [assetSymbols, setAssetSymbols] = useState<string>('');
  const [results, setResults] = useState<OptimizationResults | null>(null);
  const [loading, setLoading] = useState(false);
  const [loadingAction, setLoadingAction] = useState<string | null>(null); // Track which action is loading
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'optimize' | 'frontier' | 'backtest' | 'allocation' | 'risk' | 'strategies' | 'stress' | 'compare' | 'blacklitterman'>('optimize');

  // Risk analysis state
  const [riskDecomp, setRiskDecomp] = useState<{
    marginal_contribution: Record<string, number>;
    component_contribution: Record<string, number>;
    percentage_contribution: Record<string, number>;
    portfolio_volatility: number;
  } | null>(null);
  const [sensitivityResults, setSensitivityResults] = useState<{
    parameter: string;
    results: Array<Record<string, number>>;
  } | null>(null);
  const [sensitivityParam, setSensitivityParam] = useState('risk_free_rate');

  // Strategies state
  const [selectedStrategy, setSelectedStrategy] = useState('risk_parity');
  const [strategyResults, setStrategyResults] = useState<{
    weights: Record<string, number>;
    performance: { expected_return: number; volatility: number; sharpe_ratio: number; [key: string]: any };
    [key: string]: any;
  } | null>(null);
  const [marketNeutralLong, setMarketNeutralLong] = useState(1.3);
  const [marketNeutralShort, setMarketNeutralShort] = useState(-0.3);
  const [benchmarkWeightsStr, setBenchmarkWeightsStr] = useState('{}');

  // Load initial state
  useEffect(() => {
    const loadInitialState = async () => {
      const savedLibrary = await loadState('library', 'skfolio');
      const savedConfig = await loadState('config', config);
      const savedSkfolioConfig = await loadState('skfolioConfig', skfolioConfig);
      const savedResults = await loadState('results', null);
      const savedActiveTab = await loadState('activeTab', 'optimize');

      setLibrary(savedLibrary as 'pyportfolioopt' | 'skfolio');
      setConfig(savedConfig);
      setSkfolioConfig(savedSkfolioConfig);
      setResults(savedResults);
      setActiveTab(savedActiveTab as typeof activeTab);
    };
    loadInitialState();
  }, []);

  // Save state when it changes
  useEffect(() => {
    saveState('library', library);
  }, [library]);

  useEffect(() => {
    saveState('config', config);
  }, [config]);

  useEffect(() => {
    saveState('skfolioConfig', skfolioConfig);
  }, [skfolioConfig]);

  useEffect(() => {
    saveState('results', results);
  }, [results]);

  useEffect(() => {
    saveState('activeTab', activeTab);
  }, [activeTab]);

  // Update asset symbols when portfolio changes
  useEffect(() => {
    if (portfolioSummary?.holdings && portfolioSummary.holdings.length > 0) {
      const symbols = portfolioSummary.holdings.map(h => h.symbol).join(',');
      setAssetSymbols(symbols);
      console.log('[PortfolioOptimization] Updated symbols from portfolio:', symbols);
    } else {
      // Fallback to default symbols if no portfolio
      setAssetSymbols('AAPL,GOOGL,MSFT,AMZN,TSLA');
      console.log('[PortfolioOptimization] No portfolio holdings, using default symbols');
    }
  }, [portfolioSummary]);

  // Main optimization
  const handleOptimize = async () => {
    if (loadingAction === 'optimize') return;

    console.log('[PortfolioOptimization] Starting optimization...');
    console.log('[PortfolioOptimization] Library:', library);
    console.log('[PortfolioOptimization] Symbols:', assetSymbols);
    console.log('[PortfolioOptimization] Config:', config);

    setLoadingAction('optimize');
    setLoading(true);
    setError(null);

    // Allow UI to update before heavy operation
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      let response: string;

      if (library === 'skfolio') {
        // Use skfolio
        const skfolioParams = {
          ...skfolioConfig,
          optimization_method: config.optimization_method,
          objective_function: config.objective,
        };

        console.log('[PortfolioOptimization] Calling skfolio_optimize_portfolio with params:', {
          pricesData: assetSymbols,
          optimizationMethod: config.optimization_method,
          objectiveFunction: config.objective,
          riskMeasure: skfolioConfig.risk_measure,
          config: skfolioParams
        });

        response = await invoke<string>('skfolio_optimize_portfolio', {
          pricesData: assetSymbols, // Will be fetched by Python script
          optimizationMethod: config.optimization_method,
          objectiveFunction: config.objective,
          riskMeasure: skfolioConfig.risk_measure,
          config: JSON.stringify(skfolioParams),
        });

        console.log('[PortfolioOptimization] skfolio response received:', response);
      } else {
        // Use PyPortfolioOpt with PyO3
        const pricesJson = JSON.stringify({
          symbols: assetSymbols.split(',').map(s => s.trim()),
        });

        const pyportfoliooptConfig = {
          optimization_method: config.optimization_method,
          objective: config.objective,
          expected_returns_method: config.expected_returns_method,
          risk_model_method: config.risk_model_method,
          risk_free_rate: config.risk_free_rate,
          risk_aversion: 1.0,
          market_neutral: false,
          weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
          gamma: config.gamma,
          span: 500,
          frequency: 252,
          delta: 0.95,
          shrinkage_target: "constant_variance",
          beta: 0.95,
          tau: 0.1,
          linkage_method: "ward",
          distance_metric: "euclidean",
          total_portfolio_value: config.total_portfolio_value,
        };

        console.log('[PortfolioOptimization] Calling pyportfolioopt_optimize with config:', {
          pricesJson,
          config: pyportfoliooptConfig
        });

        response = await invoke<string>('pyportfolioopt_optimize', {
          pricesJson,
          config: pyportfoliooptConfig,
        });

        console.log('[PortfolioOptimization] PyPortfolioOpt response received:', response);
      }

      console.log('[PortfolioOptimization] Parsing response...');
      const data = JSON.parse(response);
      console.log('[PortfolioOptimization] Parsed data:', data);

      // Transform skfolio response to match UI expectations
      if (library === 'skfolio') {
        const transformedData = {
          weights: data.weights,
          performance_metrics: {
            expected_annual_return: data.return || 0,
            annual_volatility: data.volatility || 0,
            sharpe_ratio: data.sharpe_ratio || 0,
            sortino_ratio: data.sortino_ratio || 0,
            calmar_ratio: data.calmar_ratio || 0,
            max_drawdown: data.max_drawdown || 0,
          },
          model_type: data.model_type,
          objective: data.objective,
          risk_measure: data.risk_measure,
          train_period: data.train_period,
          test_period: data.test_period,
        };
        console.log('[PortfolioOptimization] Transformed skfolio data:', transformedData);
        setResults(transformedData);
      } else {
        setResults(data);
      }
      console.log('[PortfolioOptimization] Optimization successful!');
    } catch (err) {
      console.error('[PortfolioOptimization] ERROR:', err);
      console.error('[PortfolioOptimization] Error details:', {
        message: err instanceof Error ? err.message : 'Unknown error',
        stack: err instanceof Error ? err.stack : undefined,
        type: typeof err,
        value: err
      });
      setError(err instanceof Error ? err.message : JSON.stringify(err));
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // Generate efficient frontier
  const handleGenerateFrontier = async () => {
    console.log('[EfficientFrontier] Function called, loadingAction:', loadingAction);
    if (loadingAction === 'frontier') {
      console.log('[EfficientFrontier] Already loading, ignoring duplicate call');
      return; // Prevent double-click
    }

    console.log('[EfficientFrontier] Starting generation...');
    console.log('[EfficientFrontier] Library:', library);
    console.log('[EfficientFrontier] Symbols:', assetSymbols);
    console.log('[EfficientFrontier] Number of points:', config.total_portfolio_value);

    // Set loading state and wait for next render cycle
    setLoadingAction('frontier');
    setLoading(true);
    setError(null);

    // Use setTimeout to ensure state updates are rendered before heavy operation
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      let response: string;

      if (library === 'skfolio') {
        const configParams = {
          ...skfolioConfig,
          optimization_method: config.optimization_method,
          objective_function: config.objective,
        };

        console.log('[EfficientFrontier] Calling skfolio_efficient_frontier with:', {
          pricesData: assetSymbols,
          nPortfolios: config.total_portfolio_value,
          config: configParams
        });

        response = await invoke<string>('skfolio_efficient_frontier', {
          pricesData: assetSymbols,
          nPortfolios: config.total_portfolio_value,
          config: JSON.stringify(configParams),
        });

        console.log('[EfficientFrontier] Raw response:', response);
      } else {
        const pricesJson = JSON.stringify({
          symbols: assetSymbols.split(',').map(s => s.trim()),
        });

        const pyportfoliooptConfig = {
          optimization_method: config.optimization_method,
          objective: config.objective,
          expected_returns_method: config.expected_returns_method,
          risk_model_method: config.risk_model_method,
          risk_free_rate: config.risk_free_rate,
          risk_aversion: 1.0,
          market_neutral: false,
          weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
          gamma: config.gamma,
          span: 500,
          frequency: 252,
          delta: 0.95,
          shrinkage_target: "constant_variance",
          beta: 0.95,
          tau: 0.1,
          linkage_method: "ward",
          distance_metric: "euclidean",
          total_portfolio_value: config.total_portfolio_value,
        };

        console.log('[EfficientFrontier] Calling pyportfolioopt_efficient_frontier');
        response = await invoke<string>('pyportfolioopt_efficient_frontier', {
          pricesJson,
          config: pyportfoliooptConfig,
          numPoints: config.total_portfolio_value,
        });
      }

      console.log('[EfficientFrontier] Parsing response...');
      const data = JSON.parse(response);
      console.log('[EfficientFrontier] Parsed data:', data);
      console.log('[EfficientFrontier] Number of points received:', data.n_portfolios || data.returns?.length || 0);

      setResults({ ...results, efficient_frontier: data });
      console.log('[EfficientFrontier] Frontier generation successful!');
    } catch (err) {
      console.error('[EfficientFrontier] ERROR:', err);
      console.error('[EfficientFrontier] Error details:', {
        message: err instanceof Error ? err.message : 'Unknown error',
        stack: err instanceof Error ? err.stack : undefined,
      });
      setError(err instanceof Error ? err.message : 'Frontier generation failed');
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // Run backtest
  const handleBacktest = async () => {
    if (loadingAction === 'backtest') return;

    setLoadingAction('backtest');
    setLoading(true);
    setError(null);

    // Allow UI to update before heavy operation
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      let response: string;

      if (library === 'skfolio') {
        response = await invoke<string>('skfolio_backtest_strategy', {
          pricesData: assetSymbols,
          rebalanceFreq: 21,
          windowSize: 252,
          config: JSON.stringify({
            ...skfolioConfig,
            optimization_method: config.optimization_method,
            objective_function: config.objective,
          }),
        });
      } else {
        const pricesJson = JSON.stringify({
          symbols: assetSymbols.split(',').map(s => s.trim()),
        });

        const pyportfoliooptConfig = {
          optimization_method: config.optimization_method,
          objective: config.objective,
          expected_returns_method: config.expected_returns_method,
          risk_model_method: config.risk_model_method,
          risk_free_rate: config.risk_free_rate,
          risk_aversion: 1.0,
          market_neutral: false,
          weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
          gamma: config.gamma,
          span: 500,
          frequency: 252,
          delta: 0.95,
          shrinkage_target: "constant_variance",
          beta: 0.95,
          tau: 0.1,
          linkage_method: "ward",
          distance_metric: "euclidean",
          total_portfolio_value: config.total_portfolio_value,
        };

        response = await invoke<string>('pyportfolioopt_backtest', {
          pricesJson,
          config: pyportfoliooptConfig,
          rebalanceFrequency: 21,
          lookbackPeriod: 252,
        });
      }

      const data = JSON.parse(response);
      setResults({ ...results, backtest_results: data });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Backtest failed');
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // Discrete allocation (PyPortfolioOpt only)
  const handleDiscreteAllocation = async () => {
    if (!results?.weights) {
      setError('Run optimization first');
      return;
    }

    if (library === 'skfolio') {
      setError('Discrete allocation is only available for PyPortfolioOpt');
      return;
    }

    if (loadingAction === 'allocation') return;

    setLoadingAction('allocation');
    setLoading(true);
    setError(null);

    // Allow UI to update before heavy operation
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      const pricesJson = JSON.stringify({
        symbols: assetSymbols.split(',').map(s => s.trim()),
      });

      const response = await invoke<string>('pyportfolioopt_discrete_allocation', {
        pricesJson,
        weights: results.weights,
        totalPortfolioValue: config.total_portfolio_value,
      });

      const data = JSON.parse(response);
      setResults({ ...results, discrete_allocation: data });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Allocation failed');
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // Export results
  const handleExport = async (format: 'json' | 'csv') => {
    if (!results) return;

    try {
      let reportJson: string;

      if (library === 'skfolio') {
        // skfolio export
        reportJson = await invoke<string>('skfolio_generate_report', {
          pricesData: assetSymbols,
          weights: JSON.stringify(results.weights || {}),
          config: JSON.stringify({
            ...skfolioConfig,
            optimization_method: config.optimization_method,
            objective_function: config.objective,
          }),
        });
      } else {
        // PyPortfolioOpt export
        const pricesJson = JSON.stringify({
          symbols: assetSymbols.split(',').map(s => s.trim()),
        });

        const pyportfoliooptConfig = {
          optimization_method: config.optimization_method,
          objective: config.objective,
          expected_returns_method: config.expected_returns_method,
          risk_model_method: config.risk_model_method,
          risk_free_rate: config.risk_free_rate,
          risk_aversion: 1.0,
          market_neutral: false,
          weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
          gamma: config.gamma,
          span: 500,
          frequency: 252,
          delta: 0.95,
          shrinkage_target: "constant_variance",
          beta: 0.95,
          tau: 0.1,
          linkage_method: "ward",
          distance_metric: "euclidean",
          total_portfolio_value: config.total_portfolio_value,
        };

        reportJson = await invoke<string>('pyportfolioopt_generate_report', {
          pricesJson,
          config: pyportfoliooptConfig,
        });
      }

      // Download the report
      const blob = new Blob([reportJson], { type: format === 'json' ? 'application/json' : 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `portfolio_optimization_${library}_${Date.now()}.${format}`;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Export failed');
    }
  };

  // Risk Decomposition
  const handleRiskDecomposition = async () => {
    if (loadingAction === 'risk') return;
    setLoadingAction('risk');
    setLoading(true);
    setError(null);
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      const pyportfoliooptConfig = {
        optimization_method: config.optimization_method,
        objective: config.objective,
        expected_returns_method: config.expected_returns_method,
        risk_model_method: config.risk_model_method,
        risk_free_rate: config.risk_free_rate,
        risk_aversion: 1.0,
        market_neutral: false,
        weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
        gamma: config.gamma, span: 500, frequency: 252, delta: 0.95,
        shrinkage_target: "constant_variance", beta: 0.95, tau: 0.1,
        linkage_method: "ward", distance_metric: "euclidean",
        total_portfolio_value: config.total_portfolio_value,
      };

      const response = await invoke<string>('pyportfolioopt_risk_decomposition', {
        pricesJson,
        weights: results?.weights || {},
        config: pyportfoliooptConfig,
      });
      setRiskDecomp(JSON.parse(response));
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Risk decomposition failed');
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // Sensitivity Analysis
  const handleSensitivityAnalysis = async () => {
    if (loadingAction === 'sensitivity') return;
    setLoadingAction('sensitivity');
    setLoading(true);
    setError(null);
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      const pyportfoliooptConfig = {
        optimization_method: config.optimization_method,
        objective: config.objective,
        expected_returns_method: config.expected_returns_method,
        risk_model_method: config.risk_model_method,
        risk_free_rate: config.risk_free_rate,
        risk_aversion: 1.0,
        market_neutral: false,
        weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
        gamma: config.gamma, span: 500, frequency: 252, delta: 0.95,
        shrinkage_target: "constant_variance", beta: 0.95, tau: 0.1,
        linkage_method: "ward", distance_metric: "euclidean",
        total_portfolio_value: config.total_portfolio_value,
      };

      const response = await invoke<string>('pyportfolioopt_sensitivity_analysis', {
        pricesJson,
        config: pyportfoliooptConfig,
        parameter: sensitivityParam,
        values: null,
      });
      setSensitivityResults(JSON.parse(response));
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Sensitivity analysis failed');
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // Strategy Optimization (additional optimizers)
  const handleStrategyOptimize = async () => {
    if (loadingAction === 'strategy') return;
    setLoadingAction('strategy');
    setLoading(true);
    setError(null);
    await new Promise(resolve => setTimeout(resolve, 50));

    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      let response: string;

      switch (selectedStrategy) {
        case 'risk_parity':
          response = await invoke<string>('pyportfolioopt_risk_parity', { pricesJson });
          break;
        case 'equal_weight':
          response = await invoke<string>('pyportfolioopt_equal_weight', { pricesJson });
          break;
        case 'inverse_volatility':
          response = await invoke<string>('pyportfolioopt_inverse_volatility', { pricesJson });
          break;
        case 'market_neutral':
          response = await invoke<string>('pyportfolioopt_market_neutral', {
            pricesJson,
            longExposure: marketNeutralLong,
            shortExposure: marketNeutralShort,
          });
          break;
        case 'min_tracking_error':
          response = await invoke<string>('pyportfolioopt_min_tracking_error', {
            pricesJson,
            benchmarkWeights: JSON.parse(benchmarkWeightsStr),
          });
          break;
        case 'max_diversification':
          response = await invoke<string>('pyportfolioopt_max_diversification', { pricesJson });
          break;
        default:
          throw new Error(`Unknown strategy: ${selectedStrategy}`);
      }

      setStrategyResults(JSON.parse(response));
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Strategy optimization failed');
    } finally {
      setLoading(false);
      setLoadingAction(null);
    }
  };

  // ── Stress Test state ──
  const [stressTestResults, setStressTestResults] = useState<any>(null);
  const [scenarioResults, setScenarioResults] = useState<any>(null);

  // ── Compare / Advanced skfolio state ──
  const [compareResults, setCompareResults] = useState<any>(null);
  const [riskAttribution, setRiskAttribution] = useState<any>(null);
  const [hypertuneResults, setHypertuneResults] = useState<any>(null);
  const [skfolioRiskMetrics, setSkfolioRiskMetrics] = useState<any>(null);
  const [validateResults, setValidateResults] = useState<any>(null);

  // ── Black-Litterman state ──
  const [blViews, setBlViews] = useState<string>('{"AAPL": 0.10, "MSFT": 0.05}');
  const [blConfidences, setBlConfidences] = useState<string>('[0.8, 0.6]');
  const [blResults, setBlResults] = useState<any>(null);
  const [hrpResults, setHrpResults] = useState<any>(null);
  const [constraintsResults, setConstraintsResults] = useState<any>(null);
  const [viewsOptResults, setViewsOptResults] = useState<any>(null);
  const [sectorMapper, setSectorMapper] = useState<string>('{"AAPL": "Tech", "MSFT": "Tech", "JPM": "Finance"}');
  const [sectorLower, setSectorLower] = useState<string>('{"Tech": 0.2}');
  const [sectorUpper, setSectorUpper] = useState<string>('{"Tech": 0.6}');

  // ── Stress Test handlers ──
  const handleStressTest = async () => {
    if (loadingAction === 'stress') return;
    setLoadingAction('stress'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const weightsStr = JSON.stringify(results?.weights || {});
      const response = await invoke<string>('skfolio_stress_test', {
        pricesData: assetSymbols, weights: weightsStr, nSimulations: 10000,
      });
      setStressTestResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleScenarioAnalysis = async () => {
    if (loadingAction === 'scenario') return;
    setLoadingAction('scenario'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const weightsStr = JSON.stringify(results?.weights || {});
      const scenariosStr = JSON.stringify([
        { name: 'Market Crash (-30%)', returns: Object.fromEntries(assetSymbols.split(',').map(s => [s.trim(), -0.30])) },
        { name: 'Bull Rally (+20%)', returns: Object.fromEntries(assetSymbols.split(',').map(s => [s.trim(), 0.20])) },
        { name: 'Tech Selloff', returns: Object.fromEntries(assetSymbols.split(',').map(s => [s.trim(), -0.15])) },
      ]);
      const response = await invoke<string>('skfolio_scenario_analysis', {
        pricesData: assetSymbols, weights: weightsStr, scenarios: scenariosStr,
      });
      setScenarioResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  // ── Compare handlers ──
  const handleCompareStrategies = async () => {
    if (loadingAction === 'compare') return;
    setLoadingAction('compare'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const strategiesStr = JSON.stringify(['mean_risk', 'risk_parity', 'hrp', 'max_div', 'equal_weight', 'inverse_vol']);
      const response = await invoke<string>('skfolio_compare_strategies', {
        pricesData: assetSymbols, strategies: strategiesStr, metric: 'sharpe_ratio',
      });
      setCompareResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleRiskAttribution = async () => {
    if (loadingAction === 'riskattr') return;
    setLoadingAction('riskattr'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const weightsStr = JSON.stringify(results?.weights || {});
      const response = await invoke<string>('skfolio_risk_attribution', {
        pricesData: assetSymbols, weights: weightsStr,
      });
      setRiskAttribution(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleHyperparameterTuning = async () => {
    if (loadingAction === 'hypertune') return;
    setLoadingAction('hypertune'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const response = await invoke<string>('skfolio_hyperparameter_tuning', {
        pricesData: assetSymbols, cvMethod: 'walk_forward',
      });
      setHypertuneResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleSkfolioRiskMetrics = async () => {
    if (loadingAction === 'skrm') return;
    setLoadingAction('skrm'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const weightsStr = JSON.stringify(results?.weights || {});
      const [metricsRes, validateRes] = await Promise.all([
        invoke<string>('skfolio_risk_metrics', { pricesData: assetSymbols, weights: weightsStr }),
        invoke<string>('skfolio_validate_model', { pricesData: assetSymbols, validationMethod: 'walk_forward', riskMeasure: skfolioConfig.risk_measure }),
      ]);
      setSkfolioRiskMetrics(JSON.parse(metricsRes));
      setValidateResults(JSON.parse(validateRes));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  // ── Black-Litterman handlers ──
  const handleBlackLitterman = async () => {
    if (loadingAction === 'bl') return;
    setLoadingAction('bl'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      const views = JSON.parse(blViews);
      const viewConfidences = JSON.parse(blConfidences);
      const pyConfig = {
        optimization_method: config.optimization_method, objective: config.objective,
        expected_returns_method: config.expected_returns_method, risk_model_method: config.risk_model_method,
        risk_free_rate: config.risk_free_rate, risk_aversion: 1.0, market_neutral: false,
        weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
        gamma: config.gamma, span: 500, frequency: 252, delta: 0.95,
        shrinkage_target: "constant_variance", beta: 0.95, tau: 0.1,
        linkage_method: "ward", distance_metric: "euclidean",
        total_portfolio_value: config.total_portfolio_value,
      };
      const response = await invoke<string>('pyportfolioopt_black_litterman', {
        pricesJson, views, viewConfidences, config: pyConfig,
      });
      setBlResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleHRP = async () => {
    if (loadingAction === 'hrp') return;
    setLoadingAction('hrp'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      const pyConfig = {
        optimization_method: 'hrp', objective: config.objective,
        expected_returns_method: config.expected_returns_method, risk_model_method: config.risk_model_method,
        risk_free_rate: config.risk_free_rate, risk_aversion: 1.0, market_neutral: false,
        weight_bounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
        gamma: config.gamma, span: 500, frequency: 252, delta: 0.95,
        shrinkage_target: "constant_variance", beta: 0.95, tau: 0.1,
        linkage_method: "ward", distance_metric: "euclidean",
        total_portfolio_value: config.total_portfolio_value,
      };
      const response = await invoke<string>('pyportfolioopt_hrp', { pricesJson, config: pyConfig });
      setHrpResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleCustomConstraints = async () => {
    if (loadingAction === 'constraints') return;
    setLoadingAction('constraints'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      const response = await invoke<string>('pyportfolioopt_custom_constraints', {
        pricesJson, objective: config.objective,
        sectorMapper: JSON.parse(sectorMapper),
        sectorLower: JSON.parse(sectorLower),
        sectorUpper: JSON.parse(sectorUpper),
        weightBounds: [config.weight_bounds_min, config.weight_bounds_max] as [number, number],
      });
      setConstraintsResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  const handleViewsOptimization = async () => {
    if (loadingAction === 'viewsopt') return;
    setLoadingAction('viewsopt'); setLoading(true); setError(null);
    await new Promise(r => setTimeout(r, 50));
    try {
      const pricesJson = JSON.stringify({ symbols: assetSymbols.split(',').map(s => s.trim()) });
      const views = JSON.parse(blViews);
      const viewConfidences = JSON.parse(blConfidences);
      const response = await invoke<string>('pyportfolioopt_views_optimization', {
        pricesJson, views, viewConfidences, riskAversion: 1.0,
      });
      setViewsOptResults(JSON.parse(response));
    } catch (err: any) {
      setError(err?.message || String(err));
    } finally { setLoading(false); setLoadingAction(null); }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: TYPOGRAPHY.MONO,
      overflow: 'auto',
      padding: SPACING.LARGE
    }}>
      {/* Navigation Tabs */}
      <div style={{ display: 'flex', gap: SPACING.MEDIUM, marginBottom: SPACING.XLARGE, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {[
          { id: 'optimize', label: 'OPTIMIZE', icon: Calculator },
          { id: 'frontier', label: 'EFFICIENT FRONTIER', icon: LineChart },
          { id: 'backtest', label: 'BACKTEST', icon: BarChart3 },
          { id: 'allocation', label: 'ALLOCATION', icon: PieChart },
          { id: 'risk', label: 'RISK ANALYSIS', icon: Shield },
          { id: 'strategies', label: 'STRATEGIES', icon: Layers },
          { id: 'stress', label: 'STRESS TEST', icon: AlertTriangle },
          { id: 'compare', label: 'COMPARE', icon: GitCompare },
          { id: 'blacklitterman', label: 'BLACK-LITTERMAN', icon: Brain },
        ].map(({ id, label, icon: Icon }) => (
          <button
            key={id}
            onClick={() => setActiveTab(id as any)}
            style={{
              padding: '6px 12px',
              backgroundColor: activeTab === id ? FINCEPT.ORANGE : 'transparent',
              color: activeTab === id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              borderBottom: activeTab === id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
              borderRadius: '2px',
              cursor: 'pointer',
              fontFamily: TYPOGRAPHY.MONO,
              fontSize: TYPOGRAPHY.TINY,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase' as const,
              transition: EFFECTS.TRANSITION_STANDARD,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL
            }}
          >
            <Icon size={14} />
            {label}
          </button>
        ))}
      </div>

      {/* Loading Overlay - Only for non-frontier actions */}
      {loading && loadingAction !== 'frontier' && (
        <div style={{
          position: 'relative',
          minHeight: '200px',
          backgroundColor: 'rgba(0,0,0,0.5)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          border: BORDERS.STANDARD,
          borderRadius: '2px',
          flexDirection: 'column',
          gap: SPACING.MEDIUM,
          padding: '20px'
        }}>
          <div style={{
            width: '32px',
            height: '32px',
            border: `3px solid ${FINCEPT.BORDER}`,
            borderTop: `3px solid ${FINCEPT.CYAN}`,
            borderRadius: '50%',
            animation: 'spin 1s linear infinite'
          }} />
          <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: 500 }}>
            {activeTab === 'optimize' && 'Optimizing...'}
            {activeTab === 'backtest' && 'Running Backtest...'}
            {activeTab === 'allocation' && 'Calculating...'}
            {activeTab === 'risk' && 'Analyzing...'}
            {activeTab === 'stress' && 'Running stress test...'}
            {activeTab === 'compare' && 'Comparing strategies...'}
            {activeTab === 'blacklitterman' && 'Running Black-Litterman...'}
          </div>
          <style>{`
            @keyframes spin {
              0% { transform: rotate(0deg); }
              100% { transform: rotate(360deg); }
            }
          `}</style>
        </div>
      )}

      {/* Error Display */}
      {error && (
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: `${FINCEPT.RED}1A`,
          border: BORDERS.RED,
          borderRadius: '2px',
          color: FINCEPT.RED,
          marginBottom: SPACING.LARGE,
          fontSize: TYPOGRAPHY.BODY
        }}>
          ERROR: {error}
        </div>
      )}

      {/* OPTIMIZE TAB */}
      {activeTab === 'optimize' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
          {/* Configuration Panel */}
          <div>
            <div style={{
              ...COMMON_STYLES.sectionHeader,
              borderBottom: `1px solid ${FINCEPT.ORANGE}`,
              paddingBottom: SPACING.MEDIUM
            }}>
              OPTIMIZATION CONFIGURATION
            </div>

            {/* Library Selection */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                OPTIMIZATION LIBRARY
              </label>
              <select
                value={library}
                onChange={(e) => {
                  setLibrary(e.target.value as 'pyportfolioopt' | 'skfolio');
                  // Reset config based on library
                  if (e.target.value === 'skfolio') {
                    setConfig(prev => ({ ...prev, optimization_method: 'mean_risk', objective: 'maximize_ratio' }));
                  } else {
                    setConfig(prev => ({ ...prev, optimization_method: 'efficient_frontier', objective: 'max_sharpe' }));
                  }
                }}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              >
                {LIBRARIES.map(lib => (
                  <option key={lib.value} value={lib.value}>{lib.label} - {lib.description}</option>
                ))}
              </select>
            </div>

            {/* Portfolio Info */}
            {portfolioSummary && portfolioSummary.portfolio && (
              <div style={{
                marginBottom: SPACING.LARGE,
                padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                backgroundColor: FINCEPT.PANEL_BG,
                border: BORDERS.STANDARD,
                borderLeft: `3px solid ${FINCEPT.CYAN}`,
                borderRadius: '2px'
              }}>
                <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>
                  OPTIMIZING PORTFOLIO
                </div>
                <div style={{ fontSize: TYPOGRAPHY.BODY, color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>
                  {portfolioSummary.portfolio.name}
                </div>
                <div style={{ ...COMMON_STYLES.dataLabel, marginTop: SPACING.SMALL }}>
                  {portfolioSummary.holdings?.length || 0} holdings • Total Value: {new Intl.NumberFormat('en-US', {
                    style: 'currency',
                    currency: portfolioSummary.portfolio.currency || 'USD'
                  }).format(portfolioSummary.total_market_value || 0)}
                </div>
              </div>
            )}

            {/* Asset Symbols */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                ASSET SYMBOLS (comma-separated)
                {portfolioSummary?.holdings && portfolioSummary.holdings.length > 0 && (
                  <span style={{ color: FINCEPT.CYAN, marginLeft: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
                    • From portfolio
                  </span>
                )}
              </label>
              <input
                type="text"
                value={assetSymbols}
                onChange={(e) => setAssetSymbols(e.target.value)}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
                placeholder="AAPL,GOOGL,MSFT,AMZN,TSLA"
              />
              {(!portfolioSummary || portfolioSummary.holdings.length === 0) && (
                <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.ORANGE, marginTop: SPACING.SMALL }}>
                  No portfolio selected. Using default symbols.
                </div>
              )}
            </div>

            {/* Optimization Method */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                OPTIMIZATION METHOD
              </label>
              <select
                value={config.optimization_method}
                onChange={(e) => setConfig({ ...config, optimization_method: e.target.value })}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              >
                {(library === 'skfolio' ? SKFOLIO_METHODS : PYPORTFOLIOOPT_METHODS).map(m => (
                  <option key={m.value} value={m.value}>{m.label} - {m.description}</option>
                ))}
              </select>
            </div>

            {/* Objective */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                OBJECTIVE FUNCTION
              </label>
              <select
                value={config.objective}
                onChange={(e) => setConfig({ ...config, objective: e.target.value })}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              >
                {(library === 'skfolio' ? SKFOLIO_OBJECTIVES : PYPORTFOLIOOPT_OBJECTIVES).map(o => (
                  <option key={o.value} value={o.value}>{o.label}</option>
                ))}
              </select>
            </div>

            {/* skfolio-specific: Risk Measure */}
            {library === 'skfolio' && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                  RISK MEASURE
                </label>
                <select
                  value={skfolioConfig.risk_measure}
                  onChange={(e) => setSkfolioConfig({ ...skfolioConfig, risk_measure: e.target.value })}
                  style={{
                    ...COMMON_STYLES.inputField,
                    transition: EFFECTS.TRANSITION_STANDARD,
                  }}
                >
                  {SKFOLIO_RISK_MEASURES.map(r => (
                    <option key={r.value} value={r.value}>{r.label}</option>
                  ))}
                </select>
              </div>
            )}

            {/* Expected Returns Method */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                EXPECTED RETURNS METHOD
              </label>
              <select
                value={config.expected_returns_method}
                onChange={(e) => setConfig({ ...config, expected_returns_method: e.target.value })}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              >
                {EXPECTED_RETURNS_METHODS.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Risk Model */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                RISK MODEL
              </label>
              <select
                value={config.risk_model_method}
                onChange={(e) => setConfig({ ...config, risk_model_method: e.target.value })}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              >
                {RISK_MODELS.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Risk-Free Rate */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                RISK-FREE RATE (%)
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={config.risk_free_rate * 100}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, risk_free_rate: (parseFloat(v) || 0) / 100 }); }}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              />
            </div>

            {/* Weight Bounds */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                  MIN WEIGHT (%)
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={config.weight_bounds_min * 100}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, weight_bounds_min: (parseFloat(v) || 0) / 100 }); }}
                  style={{
                    ...COMMON_STYLES.inputField,
                    transition: EFFECTS.TRANSITION_STANDARD,
                  }}
                />
              </div>
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                  MAX WEIGHT (%)
                </label>
                <input
                  type="text"
                  inputMode="decimal"
                  value={config.weight_bounds_max * 100}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, weight_bounds_max: (parseFloat(v) || 0) / 100 }); }}
                  style={{
                    ...COMMON_STYLES.inputField,
                    transition: EFFECTS.TRANSITION_STANDARD,
                  }}
                />
              </div>
            </div>

            {/* L2 Regularization */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                L2 REGULARIZATION (Gamma)
              </label>
              <input
                type="text"
                inputMode="decimal"
                value={config.gamma}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, gamma: parseFloat(v) || 0 }); }}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              />
            </div>

            {/* Portfolio Value */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                TOTAL PORTFOLIO VALUE ($)
              </label>
              <input
                type="text"
                inputMode="numeric"
                value={config.total_portfolio_value}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setConfig({ ...config, total_portfolio_value: parseInt(v) || 0 }); }}
                style={{
                  ...COMMON_STYLES.inputField,
                  transition: EFFECTS.TRANSITION_STANDARD,
                }}
              />
            </div>

            {/* Optimize Button */}
            <button
              onClick={handleOptimize}
              disabled={loadingAction === 'optimize'}
              style={{
                width: '100%',
                padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
                backgroundColor: loadingAction === 'optimize' ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: loadingAction === 'optimize' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                border: loadingAction === 'optimize' ? `1px solid ${FINCEPT.GRAY}` : 'none',
                borderRadius: '2px',
                fontFamily: TYPOGRAPHY.MONO,
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase' as const,
                cursor: loadingAction === 'optimize' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'optimize' ? 0.6 : 1,
                transition: EFFECTS.TRANSITION_STANDARD,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: SPACING.MEDIUM
              }}
            >
              <Target size={16} />
              {loadingAction === 'optimize' ? 'OPTIMIZING...' : 'RUN OPTIMIZATION'}
            </button>
            {loadingAction === 'optimize' && (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM, marginTop: SPACING.DEFAULT }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: `2px solid ${FINCEPT.BORDER}`,
                  borderTop: `2px solid ${FINCEPT.CYAN}`,
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Running optimization...</span>
              </div>
            )}
          </div>

          {/* Results Panel */}
          <div>
            <div style={{
              ...COMMON_STYLES.sectionHeader,
              borderBottom: `1px solid ${FINCEPT.ORANGE}`,
              paddingBottom: SPACING.MEDIUM
            }}>
              OPTIMIZATION RESULTS
            </div>

            {results?.performance_metrics && (
              <>
                {/* Performance Metrics */}
                <div style={{ marginBottom: SPACING.XLARGE }}>
                  <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                    PERFORMANCE METRICS
                  </div>
                  <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.DEFAULT, border: BORDERS.STANDARD, borderRadius: '2px' }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT, fontSize: TYPOGRAPHY.SMALL }}>
                      <div>
                        <div style={{ color: FINCEPT.GRAY }}>Expected Annual Return</div>
                        <div style={{ color: FINCEPT.GREEN, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                          {(results.performance_metrics.expected_annual_return * 100).toFixed(2)}%
                        </div>
                      </div>
                      <div>
                        <div style={{ color: FINCEPT.GRAY }}>Annual Volatility</div>
                        <div style={{ color: FINCEPT.YELLOW, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                          {results.performance_metrics.annual_volatility > 0
                            ? `${(results.performance_metrics.annual_volatility * 100).toFixed(2)}%`
                            : 'N/A'}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</div>
                        <div style={{ color: FINCEPT.ORANGE, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                          {results.performance_metrics.sharpe_ratio.toFixed(3)}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: FINCEPT.GRAY }}>Sortino Ratio</div>
                        <div style={{ color: FINCEPT.CYAN, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                          {results.performance_metrics.sortino_ratio?.toFixed(3) || 'N/A'}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: FINCEPT.GRAY }}>Calmar Ratio</div>
                        <div style={{ color: FINCEPT.PURPLE, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                          {results.performance_metrics.calmar_ratio?.toFixed(3) || 'N/A'}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: FINCEPT.GRAY }}>Max Drawdown</div>
                        <div style={{ color: FINCEPT.RED, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                          {results.performance_metrics.max_drawdown
                            ? `${(results.performance_metrics.max_drawdown * 100).toFixed(2)}%`
                            : 'N/A'}
                        </div>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Portfolio Weights */}
                {results.weights && (
                  <div style={{ marginBottom: SPACING.XLARGE }}>
                    <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                      OPTIMAL WEIGHTS
                    </div>
                    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', maxHeight: '300px', overflow: 'auto' }}>
                      {Object.entries(results.weights)
                        .sort(([, a], [, b]) => b - a)
                        .map(([symbol, weight]) => (
                          <div
                            key={symbol}
                            style={{
                              padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                              borderBottom: `1px solid ${FINCEPT.HEADER_BG}`,
                              display: 'flex',
                              justifyContent: 'space-between',
                              alignItems: 'center',
                              fontSize: TYPOGRAPHY.SMALL,
                              transition: EFFECTS.TRANSITION_STANDARD,
                            }}
                          >
                            <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{symbol}</span>
                            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                              <div
                                style={{
                                  width: '100px',
                                  height: '6px',
                                  backgroundColor: FINCEPT.HEADER_BG,
                                  borderRadius: '2px',
                                  position: 'relative',
                                  overflow: 'hidden'
                                }}
                              >
                                <div
                                  style={{
                                    position: 'absolute',
                                    left: 0,
                                    top: 0,
                                    height: '100%',
                                    width: `${weight * 100}%`,
                                    backgroundColor: FINCEPT.ORANGE
                                  }}
                                />
                              </div>
                              <span style={{ color: FINCEPT.WHITE, width: '60px', textAlign: 'right' }}>
                                {(weight * 100).toFixed(2)}%
                              </span>
                            </div>
                          </div>
                        ))}
                    </div>
                  </div>
                )}

                {/* Export Buttons */}
                <div style={{ display: 'flex', gap: SPACING.MEDIUM }}>
                  <button
                    onClick={() => handleExport('json')}
                    style={{
                      flex: 1,
                      padding: `${SPACING.MEDIUM} ${SPACING.LARGE}`,
                      backgroundColor: FINCEPT.GREEN,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      fontFamily: TYPOGRAPHY.MONO,
                      fontSize: TYPOGRAPHY.TINY,
                      fontWeight: TYPOGRAPHY.BOLD,
                      letterSpacing: TYPOGRAPHY.WIDE,
                      textTransform: 'uppercase' as const,
                      cursor: 'pointer',
                      transition: EFFECTS.TRANSITION_STANDARD,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: SPACING.SMALL
                    }}
                  >
                    <Download size={12} />
                    EXPORT JSON
                  </button>
                  <button
                    onClick={() => handleExport('csv')}
                    style={{
                      flex: 1,
                      padding: `${SPACING.MEDIUM} ${SPACING.LARGE}`,
                      backgroundColor: FINCEPT.CYAN,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      fontFamily: TYPOGRAPHY.MONO,
                      fontSize: TYPOGRAPHY.TINY,
                      fontWeight: TYPOGRAPHY.BOLD,
                      letterSpacing: TYPOGRAPHY.WIDE,
                      textTransform: 'uppercase' as const,
                      cursor: 'pointer',
                      transition: EFFECTS.TRANSITION_STANDARD,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: SPACING.SMALL
                    }}
                  >
                    <Download size={12} />
                    EXPORT CSV
                  </button>
                </div>
              </>
            )}

            {!results && !loading && (
              <div style={{
                padding: '48px',
                textAlign: 'center',
                color: FINCEPT.GRAY,
                fontSize: TYPOGRAPHY.BODY,
                backgroundColor: FINCEPT.PANEL_BG,
                border: BORDERS.STANDARD,
                borderRadius: '2px'
              }}>
                Configure parameters and click "RUN OPTIMIZATION" to see results
              </div>
            )}
          </div>
        </div>
      )}

      {/* EFFICIENT FRONTIER TAB */}
      {activeTab === 'frontier' && (
        <div>
          {/* Configuration Panel */}
          <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.LARGE, border: BORDERS.STANDARD, borderRadius: '2px', marginBottom: SPACING.LARGE }}>
            <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.DEFAULT }}>
              FRONTIER PARAMETERS
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
              {/* Number of portfolios */}
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                  NUMBER OF POINTS
                </label>
                <input
                  type="text"
                  inputMode="numeric"
                  value={config.total_portfolio_value}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d+$/.test(v)) setConfig({ ...config, total_portfolio_value: parseInt(v) || 100 }); }}
                  style={{
                    ...COMMON_STYLES.inputField,
                    transition: EFFECTS.TRANSITION_STANDARD,
                  }}
                />
              </div>

              {/* Risk Measure for skfolio */}
              {library === 'skfolio' && (
                <div>
                  <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                    RISK MEASURE
                  </label>
                  <select
                    value={skfolioConfig.risk_measure}
                    onChange={(e) => setSkfolioConfig({ ...skfolioConfig, risk_measure: e.target.value })}
                    style={{
                      ...COMMON_STYLES.inputField,
                      transition: EFFECTS.TRANSITION_STANDARD,
                    }}
                  >
                    {SKFOLIO_RISK_MEASURES.map(rm => (
                      <option key={rm.value} value={rm.value}>{rm.label}</option>
                    ))}
                  </select>
                </div>
              )}
            </div>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
            <button
              onClick={handleGenerateFrontier}
              disabled={loadingAction === 'frontier'}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
                backgroundColor: loadingAction === 'frontier' ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: loadingAction === 'frontier' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                border: loadingAction === 'frontier' ? `1px solid ${FINCEPT.GRAY}` : 'none',
                cursor: loadingAction === 'frontier' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'frontier' ? 0.6 : 1,
              }}
            >
              {loadingAction === 'frontier' ? 'GENERATING...' : 'GENERATE EFFICIENT FRONTIER'}
            </button>
            {loadingAction === 'frontier' && (
              <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: `2px solid ${FINCEPT.BORDER}`,
                  borderTop: `2px solid ${FINCEPT.CYAN}`,
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Generating frontier...</span>
                <style>{`
                  @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                  }
                `}</style>
              </div>
            )}
          </div>

          {results?.efficient_frontier && results.efficient_frontier.returns && (
            <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.XLARGE, border: BORDERS.STANDARD, borderRadius: '2px', marginTop: SPACING.LARGE }}>
              <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.LARGE }}>
                EFFICIENT FRONTIER ({results.efficient_frontier.returns?.length || 0} points)
              </div>

              {/* Prepare chart data */}
              {(() => {
                const chartData = results.efficient_frontier!.returns.map((ret: number, idx: number) => ({
                  volatility: parseFloat((results.efficient_frontier!.volatility[idx] * 100).toFixed(2)),
                  returns: parseFloat((ret * 100).toFixed(2)),
                  sharpe: parseFloat(results.efficient_frontier!.sharpe_ratios?.[idx]?.toFixed(3) || '0'),
                }));

                // Custom dot component for better visualization
                const CustomDot = (props: any) => {
                  const { cx, cy, payload } = props;
                  const sharpe = payload.sharpe || 0;

                  // Color gradient based on Sharpe ratio (green for high, yellow for medium, red for low)
                  let fillColor: string = FINCEPT.RED; // Red for low Sharpe
                  if (sharpe > 1.5) fillColor = FINCEPT.GREEN; // Green for high Sharpe
                  else if (sharpe > 0.8) fillColor = FINCEPT.YELLOW; // Gold for medium Sharpe
                  else if (sharpe > 0.3) fillColor = FINCEPT.ORANGE; // Orange for okay Sharpe

                  return (
                    <circle
                      cx={cx}
                      cy={cy}
                      r={1.5}
                      fill={fillColor}
                      fillOpacity={0.8}
                      stroke={fillColor}
                      strokeWidth={0.5}
                    />
                  );
                };

                return (
                  <ResponsiveContainer width="100%" height={450}>
                    <ScatterChart margin={{ top: 20, right: 30, bottom: 40, left: 60 }}>
                      <defs>
                        <linearGradient id="frontierGradient" x1="0" y1="0" x2="1" y2="1">
                          <stop offset="0%" stopColor={FINCEPT.RED} stopOpacity={0.8} />
                          <stop offset="50%" stopColor={FINCEPT.YELLOW} stopOpacity={0.8} />
                          <stop offset="100%" stopColor={FINCEPT.GREEN} stopOpacity={0.8} />
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="#333" strokeOpacity={0.3} />
                      <XAxis
                        type="number"
                        dataKey="volatility"
                        name="Volatility"
                        unit="%"
                        stroke={FINCEPT.GRAY}
                        tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
                        label={{ value: 'Annual Volatility (%)', position: 'insideBottom', offset: -15, fill: FINCEPT.GRAY, fontSize: 11 }}
                      />
                      <YAxis
                        type="number"
                        dataKey="returns"
                        name="Returns"
                        unit="%"
                        stroke={FINCEPT.GRAY}
                        tick={{ fill: FINCEPT.GRAY, fontSize: 10 }}
                        label={{ value: 'Annual Returns (%)', angle: -90, position: 'insideLeft', offset: -40, fill: FINCEPT.GRAY, fontSize: 11 }}
                      />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: FINCEPT.HEADER_BG,
                          border: `1px solid ${FINCEPT.CYAN}`,
                          borderRadius: '2px',
                          fontSize: TYPOGRAPHY.SMALL,
                          padding: SPACING.MEDIUM
                        }}
                        labelStyle={{ color: FINCEPT.CYAN }}
                        itemStyle={{ color: FINCEPT.WHITE }}
                        formatter={(value: any, name: string) => {
                          if (name === 'Volatility') return [`${value}%`, 'Volatility'];
                          if (name === 'Returns') return [`${value}%`, 'Returns'];
                          if (name === 'sharpe') return [value, 'Sharpe Ratio'];
                          return [value, name];
                        }}
                      />
                      <Scatter
                        name="Efficient Frontier"
                        data={chartData}
                        fill="url(#frontierGradient)"
                        line={{ stroke: FINCEPT.CYAN, strokeWidth: 1, strokeOpacity: 0.5 }}
                        shape={<CustomDot />}
                        isAnimationActive={false}
                      />
                    </ScatterChart>
                  </ResponsiveContainer>
                );
              })()}

              {/* Legend */}
              <div style={{ marginTop: SPACING.LARGE, fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY, textAlign: 'center' }}>
                Each point represents an optimal portfolio. Color indicates Sharpe ratio:
                <span style={{ color: FINCEPT.GREEN, marginLeft: SPACING.MEDIUM }}>● High {'>1.5'}</span>
                <span style={{ color: FINCEPT.YELLOW, marginLeft: SPACING.MEDIUM }}>● Good (0.8-1.5)</span>
                <span style={{ color: FINCEPT.ORANGE, marginLeft: SPACING.MEDIUM }}>● Medium (0.3-0.8)</span>
                <span style={{ color: FINCEPT.RED, marginLeft: SPACING.MEDIUM }}>● Low {'<0.3'}</span>
              </div>
            </div>
          )}
        </div>
      )}

      {/* BACKTEST TAB */}
      {activeTab === 'backtest' && (
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
            <button
              onClick={handleBacktest}
              disabled={loadingAction === 'backtest'}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
                backgroundColor: loadingAction === 'backtest' ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: loadingAction === 'backtest' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                border: loadingAction === 'backtest' ? `1px solid ${FINCEPT.GRAY}` : 'none',
                cursor: loadingAction === 'backtest' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'backtest' ? 0.6 : 1,
              }}
            >
              {loadingAction === 'backtest' ? 'RUNNING BACKTEST...' : 'RUN BACKTEST'}
            </button>
            {loadingAction === 'backtest' && (
              <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: `2px solid ${FINCEPT.BORDER}`,
                  borderTop: `2px solid ${FINCEPT.CYAN}`,
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Running backtest...</span>
                <style>{`
                  @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                  }
                `}</style>
              </div>
            )}
          </div>

          {results?.backtest_results && (
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: SPACING.DEFAULT }}>
              {[
                { label: 'Annual Return', value: results.backtest_results.annual_return, color: FINCEPT.GREEN, format: '%' },
                { label: 'Annual Volatility', value: results.backtest_results.annual_volatility, color: FINCEPT.YELLOW, format: '%' },
                { label: 'Sharpe Ratio', value: results.backtest_results.sharpe_ratio, color: FINCEPT.ORANGE, format: '' },
                { label: 'Max Drawdown', value: results.backtest_results.max_drawdown, color: FINCEPT.RED, format: '%' },
                { label: 'Calmar Ratio', value: results.backtest_results.calmar_ratio, color: FINCEPT.CYAN, format: '' },
              ].map(metric => (
                <div key={metric.label} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
                  <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>{metric.label}</div>
                  <div style={{ color: metric.color, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                    {metric.format === '%'
                      ? `${(metric.value * 100).toFixed(2)}%`
                      : metric.value.toFixed(3)
                    }
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {/* ALLOCATION TAB */}
      {activeTab === 'allocation' && (
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
            <button
              onClick={handleDiscreteAllocation}
              disabled={loadingAction === 'allocation' || !results?.weights}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
                backgroundColor: (loadingAction === 'allocation' || !results?.weights) ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: (loadingAction === 'allocation' || !results?.weights) ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                border: (loadingAction === 'allocation' || !results?.weights) ? `1px solid ${FINCEPT.GRAY}` : 'none',
                cursor: (loadingAction === 'allocation' || !results?.weights) ? 'not-allowed' : 'pointer',
                opacity: (loadingAction === 'allocation' || !results?.weights) ? 0.6 : 1,
              }}
            >
              {loadingAction === 'allocation' ? 'CALCULATING...' : 'CALCULATE DISCRETE ALLOCATION'}
            </button>
            {loadingAction === 'allocation' && (
              <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: `2px solid ${FINCEPT.BORDER}`,
                  borderTop: `2px solid ${FINCEPT.CYAN}`,
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY }}>Calculating allocation...</span>
                <style>{`
                  @keyframes spin {
                    0% { transform: rotate(0deg); }
                    100% { transform: rotate(360deg); }
                  }
                `}</style>
              </div>
            )}
          </div>

          {!results?.weights && (
            <div style={{
              padding: SPACING.LARGE,
              backgroundColor: `${FINCEPT.ORANGE}1A`,
              border: BORDERS.ORANGE,
              color: FINCEPT.ORANGE,
              marginBottom: SPACING.XLARGE,
              fontSize: TYPOGRAPHY.BODY,
              borderRadius: '2px'
            }}>
              Please run optimization first to get portfolio weights
            </div>
          )}

          {results?.discrete_allocation && (
            <div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
                <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>Total Value</div>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                    ${results.discrete_allocation.total_value.toLocaleString()}
                  </div>
                </div>
                <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>Allocated</div>
                  <div style={{ color: FINCEPT.GREEN, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                    ${(results.discrete_allocation.total_value - results.discrete_allocation.leftover_cash).toLocaleString()}
                  </div>
                </div>
                <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>Leftover Cash</div>
                  <div style={{ color: FINCEPT.YELLOW, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                    ${results.discrete_allocation.leftover_cash.toLocaleString()}
                  </div>
                </div>
              </div>

              <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.LARGE, border: BORDERS.STANDARD, borderRadius: '2px' }}>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.DEFAULT }}>
                  SHARE ALLOCATION
                </div>
                {Object.entries(results.discrete_allocation.allocation).map(([symbol, shares]) => (
                  <div
                    key={symbol}
                    style={{
                      padding: `${SPACING.MEDIUM} 0`,
                      borderBottom: `1px solid ${FINCEPT.HEADER_BG}`,
                      display: 'flex',
                      justifyContent: 'space-between',
                      fontSize: TYPOGRAPHY.BODY,
                      transition: EFFECTS.TRANSITION_STANDARD,
                    }}
                  >
                    <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{symbol}</span>
                    <span style={{ color: FINCEPT.WHITE }}>{shares} shares</span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {/* RISK ANALYSIS TAB */}
      {activeTab === 'risk' && (
        <div>
          {/* Action Buttons */}
          <div style={{ display: 'flex', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
            <button
              onClick={handleRiskDecomposition}
              disabled={loadingAction === 'risk'}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                padding: `${SPACING.DEFAULT} ${SPACING.XLARGE}`,
                backgroundColor: loadingAction === 'risk' ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: loadingAction === 'risk' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                cursor: loadingAction === 'risk' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'risk' ? 0.6 : 1,
              }}
            >
              {loadingAction === 'risk' ? 'ANALYZING...' : 'RUN RISK DECOMPOSITION'}
            </button>
            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
              <select
                value={sensitivityParam}
                onChange={(e) => setSensitivityParam(e.target.value)}
                style={{ ...COMMON_STYLES.inputField, width: '180px' }}
              >
                <option value="risk_free_rate">Risk-Free Rate</option>
                <option value="gamma">L2 Regularization</option>
                <option value="risk_aversion">Risk Aversion</option>
              </select>
              <button
                onClick={handleSensitivityAnalysis}
                disabled={loadingAction === 'sensitivity'}
                style={{
                  ...COMMON_STYLES.buttonSecondary,
                  padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
                  color: loadingAction === 'sensitivity' ? FINCEPT.GRAY : FINCEPT.CYAN,
                  borderColor: loadingAction === 'sensitivity' ? FINCEPT.GRAY : FINCEPT.CYAN,
                  cursor: loadingAction === 'sensitivity' ? 'not-allowed' : 'pointer',
                }}
              >
                {loadingAction === 'sensitivity' ? 'RUNNING...' : 'SENSITIVITY ANALYSIS'}
              </button>
            </div>
          </div>

          {/* Risk Decomposition Results */}
          {riskDecomp && (
            <div style={{ marginBottom: SPACING.XLARGE }}>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
                RISK DECOMPOSITION
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.LARGE }}>
                <div style={{ ...COMMON_STYLES.metricCard, padding: SPACING.LARGE }}>
                  <div style={{ ...COMMON_STYLES.dataLabel }}>Portfolio Volatility</div>
                  <div style={{ color: FINCEPT.YELLOW, fontSize: '18px', fontWeight: TYPOGRAPHY.BOLD }}>
                    {(riskDecomp.portfolio_volatility * 100).toFixed(2)}%
                  </div>
                </div>
              </div>

              {/* Risk contribution table */}
              <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', overflow: 'hidden' }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', ...COMMON_STYLES.tableHeader }}>
                  <span>Asset</span>
                  <span>Marginal</span>
                  <span>Component</span>
                  <span>% Contribution</span>
                </div>
                {Object.keys(riskDecomp.percentage_contribution)
                  .sort((a, b) => (riskDecomp.percentage_contribution[b] || 0) - (riskDecomp.percentage_contribution[a] || 0))
                  .map((asset, idx) => (
                    <div
                      key={asset}
                      style={{
                        display: 'grid',
                        gridTemplateColumns: '1fr 1fr 1fr 1fr',
                        padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                        borderBottom: BORDERS.STANDARD,
                        fontSize: TYPOGRAPHY.SMALL,
                        backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                      }}
                    >
                      <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{asset}</span>
                      <span style={{ color: FINCEPT.WHITE }}>{(riskDecomp.marginal_contribution[asset] * 100).toFixed(4)}%</span>
                      <span style={{ color: FINCEPT.WHITE }}>{(riskDecomp.component_contribution[asset] * 100).toFixed(4)}%</span>
                      <span style={{ color: FINCEPT.ORANGE, fontWeight: TYPOGRAPHY.BOLD }}>
                        {(riskDecomp.percentage_contribution[asset] * 100).toFixed(2)}%
                      </span>
                    </div>
                  ))}
              </div>
            </div>
          )}

          {/* Sensitivity Analysis Results */}
          {sensitivityResults && (
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.CYAN}`, paddingBottom: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
                SENSITIVITY ANALYSIS: {sensitivityResults.parameter.toUpperCase().replace(/_/g, ' ')}
              </div>

              {/* Chart */}
              {sensitivityResults.results.length > 0 && (
                <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.LARGE, border: BORDERS.STANDARD, borderRadius: '2px', marginBottom: SPACING.LARGE }}>
                  <ResponsiveContainer width="100%" height={300}>
                    <RechartsLine data={sensitivityResults.results}>
                      <CartesianGrid strokeDasharray="3 3" stroke="#333" strokeOpacity={0.3} />
                      <XAxis dataKey={sensitivityResults.parameter} stroke={FINCEPT.GRAY} tick={{ fill: FINCEPT.GRAY, fontSize: 10 }} />
                      <YAxis stroke={FINCEPT.GRAY} tick={{ fill: FINCEPT.GRAY, fontSize: 10 }} />
                      <Tooltip
                        contentStyle={{ backgroundColor: FINCEPT.HEADER_BG, border: `1px solid ${FINCEPT.CYAN}`, borderRadius: '2px', fontSize: TYPOGRAPHY.SMALL }}
                        labelStyle={{ color: FINCEPT.CYAN }}
                      />
                      <Line type="monotone" dataKey="expected_return" stroke={FINCEPT.GREEN} dot={false} name="Expected Return" />
                      <Line type="monotone" dataKey="volatility" stroke={FINCEPT.YELLOW} dot={false} name="Volatility" />
                      <Line type="monotone" dataKey="sharpe_ratio" stroke={FINCEPT.ORANGE} dot={false} name="Sharpe Ratio" />
                    </RechartsLine>
                  </ResponsiveContainer>
                </div>
              )}

              {/* Table */}
              <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', overflow: 'auto', maxHeight: '300px' }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', ...COMMON_STYLES.tableHeader }}>
                  <span>{sensitivityResults.parameter.replace(/_/g, ' ')}</span>
                  <span>Expected Return</span>
                  <span>Volatility</span>
                  <span>Sharpe Ratio</span>
                </div>
                {sensitivityResults.results.map((row, idx) => (
                  <div
                    key={idx}
                    style={{
                      display: 'grid',
                      gridTemplateColumns: '1fr 1fr 1fr 1fr',
                      padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                      borderBottom: BORDERS.STANDARD,
                      fontSize: TYPOGRAPHY.SMALL,
                      backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                    }}
                  >
                    <span style={{ color: FINCEPT.CYAN }}>{(row[sensitivityResults.parameter] ?? 0).toFixed(4)}</span>
                    <span style={{ color: FINCEPT.GREEN }}>{((row.expected_return ?? 0) * 100).toFixed(2)}%</span>
                    <span style={{ color: FINCEPT.YELLOW }}>{((row.volatility ?? 0) * 100).toFixed(2)}%</span>
                    <span style={{ color: FINCEPT.ORANGE }}>{(row.sharpe_ratio ?? 0).toFixed(3)}</span>
                  </div>
                ))}
              </div>
            </div>
          )}

          {!riskDecomp && !sensitivityResults && !loading && (
            <div style={{ padding: '48px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px' }}>
              Run risk decomposition or sensitivity analysis to see results. Run optimization first if you haven't already.
            </div>
          )}
        </div>
      )}

      {/* STRATEGIES TAB */}
      {activeTab === 'strategies' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
          {/* Strategy Configuration */}
          <div>
            <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM }}>
              ADDITIONAL STRATEGIES (PyPortfolioOpt)
            </div>

            {/* Strategy Selector */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>STRATEGY</label>
              <select
                value={selectedStrategy}
                onChange={(e) => setSelectedStrategy(e.target.value)}
                style={{ ...COMMON_STYLES.inputField, transition: EFFECTS.TRANSITION_STANDARD }}
              >
                <option value="risk_parity">Risk Parity - Equal risk contribution</option>
                <option value="equal_weight">Equal Weight - 1/N portfolio</option>
                <option value="inverse_volatility">Inverse Volatility - Weight by inverse vol</option>
                <option value="market_neutral">Market Neutral - Long/short zero net exposure</option>
                <option value="min_tracking_error">Min Tracking Error - Stay close to benchmark</option>
                <option value="max_diversification">Max Diversification - Maximize diversification ratio</option>
              </select>
            </div>

            {/* Asset Symbols */}
            <div style={{ marginBottom: SPACING.LARGE }}>
              <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>ASSET SYMBOLS</label>
              <input
                type="text"
                value={assetSymbols}
                onChange={(e) => setAssetSymbols(e.target.value)}
                style={{ ...COMMON_STYLES.inputField }}
                placeholder="AAPL,GOOGL,MSFT,AMZN,TSLA"
              />
            </div>

            {/* Market Neutral Config */}
            {selectedStrategy === 'market_neutral' && (
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM, marginBottom: SPACING.LARGE }}>
                <div>
                  <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>LONG EXPOSURE</label>
                  <input
                    type="text" inputMode="decimal"
                    value={marketNeutralLong}
                    onChange={(e) => setMarketNeutralLong(parseFloat(e.target.value) || 1.0)}
                    style={{ ...COMMON_STYLES.inputField }}
                  />
                </div>
                <div>
                  <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>SHORT EXPOSURE</label>
                  <input
                    type="text" inputMode="decimal"
                    value={marketNeutralShort}
                    onChange={(e) => setMarketNeutralShort(parseFloat(e.target.value) || -1.0)}
                    style={{ ...COMMON_STYLES.inputField }}
                  />
                </div>
              </div>
            )}

            {/* Min Tracking Error Config */}
            {selectedStrategy === 'min_tracking_error' && (
              <div style={{ marginBottom: SPACING.LARGE }}>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>
                  BENCHMARK WEIGHTS (JSON)
                </label>
                <input
                  type="text"
                  value={benchmarkWeightsStr}
                  onChange={(e) => setBenchmarkWeightsStr(e.target.value)}
                  style={{ ...COMMON_STYLES.inputField }}
                  placeholder='{"AAPL": 0.3, "MSFT": 0.3, "GOOGL": 0.4}'
                />
              </div>
            )}

            {/* Run Button */}
            <button
              onClick={handleStrategyOptimize}
              disabled={loadingAction === 'strategy'}
              style={{
                width: '100%',
                padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
                backgroundColor: loadingAction === 'strategy' ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: loadingAction === 'strategy' ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                border: 'none', borderRadius: '2px',
                fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD, letterSpacing: TYPOGRAPHY.WIDE,
                textTransform: 'uppercase' as const,
                cursor: loadingAction === 'strategy' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'strategy' ? 0.6 : 1,
                transition: EFFECTS.TRANSITION_STANDARD,
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM
              }}
            >
              <Layers size={16} />
              {loadingAction === 'strategy' ? 'RUNNING...' : 'RUN STRATEGY'}
            </button>
          </div>

          {/* Strategy Results */}
          <div>
            <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM }}>
              STRATEGY RESULTS
            </div>

            {strategyResults ? (
              <>
                {/* Performance Metrics */}
                {strategyResults.performance && (
                  <div style={{ marginBottom: SPACING.XLARGE }}>
                    <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                      PERFORMANCE METRICS
                    </div>
                    <div style={{ backgroundColor: FINCEPT.PANEL_BG, padding: SPACING.DEFAULT, border: BORDERS.STANDARD, borderRadius: '2px' }}>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.DEFAULT, fontSize: TYPOGRAPHY.SMALL }}>
                        <div>
                          <div style={{ color: FINCEPT.GRAY }}>Expected Return</div>
                          <div style={{ color: FINCEPT.GREEN, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                            {(strategyResults.performance.expected_return * 100).toFixed(2)}%
                          </div>
                        </div>
                        <div>
                          <div style={{ color: FINCEPT.GRAY }}>Volatility</div>
                          <div style={{ color: FINCEPT.YELLOW, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                            {(strategyResults.performance.volatility * 100).toFixed(2)}%
                          </div>
                        </div>
                        <div>
                          <div style={{ color: FINCEPT.GRAY }}>Sharpe Ratio</div>
                          <div style={{ color: FINCEPT.ORANGE, fontSize: '14px', fontWeight: TYPOGRAPHY.BOLD }}>
                            {strategyResults.performance.sharpe_ratio.toFixed(3)}
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                )}

                {/* Weights */}
                {strategyResults.weights && (
                  <div>
                    <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>
                      OPTIMAL WEIGHTS
                    </div>
                    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', maxHeight: '300px', overflow: 'auto' }}>
                      {Object.entries(strategyResults.weights)
                        .sort(([, a], [, b]) => Math.abs(b) - Math.abs(a))
                        .map(([symbol, weight]) => (
                          <div
                            key={symbol}
                            style={{
                              padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
                              borderBottom: `1px solid ${FINCEPT.HEADER_BG}`,
                              display: 'flex',
                              justifyContent: 'space-between',
                              alignItems: 'center',
                              fontSize: TYPOGRAPHY.SMALL,
                            }}
                          >
                            <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{symbol}</span>
                            <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
                              <div style={{ width: '100px', height: '6px', backgroundColor: FINCEPT.HEADER_BG, borderRadius: '2px', position: 'relative', overflow: 'hidden' }}>
                                <div style={{
                                  position: 'absolute',
                                  left: weight < 0 ? `${50 + weight * 50}%` : '50%',
                                  top: 0, height: '100%',
                                  width: `${Math.abs(weight) * 50}%`,
                                  backgroundColor: weight >= 0 ? FINCEPT.ORANGE : FINCEPT.RED,
                                }} />
                              </div>
                              <span style={{ color: weight >= 0 ? FINCEPT.WHITE : FINCEPT.RED, width: '60px', textAlign: 'right' }}>
                                {(weight * 100).toFixed(2)}%
                              </span>
                            </div>
                          </div>
                        ))}
                    </div>
                  </div>
                )}
              </>
            ) : (
              <div style={{ padding: '48px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px' }}>
                Select a strategy and click "RUN STRATEGY" to see results
              </div>
            )}
          </div>
        </div>
      )}
      {/* STRESS TEST TAB */}
      {activeTab === 'stress' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE }}>
          {/* Stress Test */}
          <div>
            <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.RED}`, paddingBottom: SPACING.MEDIUM }}>
              MONTE CARLO STRESS TEST (skfolio)
            </div>
            <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
              Run 10,000 simulations to stress-test your optimized portfolio weights.
            </p>
            <button
              onClick={handleStressTest}
              disabled={loadingAction === 'stress'}
              style={{
                width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
                backgroundColor: loadingAction === 'stress' ? FINCEPT.MUTED : FINCEPT.RED,
                color: FINCEPT.WHITE, border: 'none', borderRadius: '2px',
                fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD,
                cursor: loadingAction === 'stress' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE,
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM,
              }}
            >
              <AlertTriangle size={14} />
              {loadingAction === 'stress' ? 'RUNNING...' : 'RUN STRESS TEST'}
            </button>
            {stressTestResults && (
              <OptResultGrid data={stressTestResults} color={FINCEPT.RED} />
            )}
          </div>

          {/* Scenario Analysis */}
          <div>
            <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.MEDIUM }}>
              SCENARIO ANALYSIS (skfolio)
            </div>
            <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
              Predefined scenarios: Market Crash (-30%), Bull Rally (+20%), Tech Selloff (-15%).
            </p>
            <button
              onClick={handleScenarioAnalysis}
              disabled={loadingAction === 'scenario'}
              style={{
                width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`,
                backgroundColor: loadingAction === 'scenario' ? FINCEPT.MUTED : FINCEPT.YELLOW,
                color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px',
                fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD,
                cursor: loadingAction === 'scenario' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE,
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM,
              }}
            >
              <Target size={14} />
              {loadingAction === 'scenario' ? 'RUNNING...' : 'RUN SCENARIOS'}
            </button>
            {scenarioResults && (
              <OptResultGrid data={scenarioResults} color={FINCEPT.YELLOW} />
            )}
          </div>
        </div>
      )}

      {/* COMPARE TAB */}
      {activeTab === 'compare' && (
        <div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE, marginBottom: SPACING.XLARGE }}>
            {/* Compare Strategies */}
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.CYAN}`, paddingBottom: SPACING.MEDIUM }}>
                COMPARE STRATEGIES (skfolio)
              </div>
              <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
                Compare mean-risk, risk parity, HRP, max diversification, equal weight, inverse vol.
              </p>
              <button onClick={handleCompareStrategies} disabled={loadingAction === 'compare'}
                style={{ width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'compare' ? FINCEPT.MUTED : FINCEPT.CYAN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'compare' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
              >
                <GitCompare size={14} />
                {loadingAction === 'compare' ? 'COMPARING...' : 'COMPARE ALL'}
              </button>
              {compareResults && <OptResultGrid data={compareResults} color={FINCEPT.CYAN} />}
            </div>

            {/* Risk Attribution */}
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.ORANGE}`, paddingBottom: SPACING.MEDIUM }}>
                RISK ATTRIBUTION (skfolio)
              </div>
              <p style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.LARGE }}>
                Risk contribution by asset. Run optimization first to populate weights.
              </p>
              <button onClick={handleRiskAttribution} disabled={loadingAction === 'riskattr'}
                style={{ width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'riskattr' ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'riskattr' ? 'not-allowed' : 'pointer', marginBottom: SPACING.LARGE, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
              >
                <Shield size={14} />
                {loadingAction === 'riskattr' ? 'ANALYZING...' : 'RISK ATTRIBUTION'}
              </button>
              {riskAttribution && <OptResultGrid data={riskAttribution} color={FINCEPT.ORANGE} />}
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: SPACING.XLARGE }}>
            {/* Hyperparameter Tuning */}
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.GREEN}`, paddingBottom: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
                HYPERPARAMETER TUNING
              </div>
              <button onClick={handleHyperparameterTuning} disabled={loadingAction === 'hypertune'}
                style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: loadingAction === 'hypertune' ? FINCEPT.MUTED : FINCEPT.GREEN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'hypertune' ? 'not-allowed' : 'pointer', marginBottom: SPACING.MEDIUM, marginTop: SPACING.MEDIUM }}
              >
                {loadingAction === 'hypertune' ? 'TUNING...' : 'TUNE'}
              </button>
              {hypertuneResults && <OptResultGrid data={hypertuneResults} color={FINCEPT.GREEN} />}
            </div>

            {/* Risk Metrics + Validate */}
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
                RISK METRICS + VALIDATE
              </div>
              <button onClick={handleSkfolioRiskMetrics} disabled={loadingAction === 'skrm'}
                style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: loadingAction === 'skrm' ? FINCEPT.MUTED : FINCEPT.YELLOW, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'skrm' ? 'not-allowed' : 'pointer', marginBottom: SPACING.MEDIUM, marginTop: SPACING.MEDIUM }}
              >
                {loadingAction === 'skrm' ? 'RUNNING...' : 'COMPUTE'}
              </button>
              {skfolioRiskMetrics && <OptResultGrid data={skfolioRiskMetrics} color={FINCEPT.YELLOW} />}
              {validateResults && <OptResultGrid data={validateResults} color={FINCEPT.CYAN} />}
            </div>

            {/* Export Weights */}
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.WHITE}`, paddingBottom: SPACING.MEDIUM, fontSize: TYPOGRAPHY.TINY }}>
                EXPORT WEIGHTS
              </div>
              <button
                onClick={async () => {
                  try {
                    const weightsStr = JSON.stringify(results?.weights || {});
                    const assetNamesStr = JSON.stringify(assetSymbols.split(',').map(s => s.trim()));
                    const res = await invoke<string>('skfolio_export_weights', { weights: weightsStr, assetNames: assetNamesStr, format: 'json' });
                    const blob = new Blob([res], { type: 'application/json' });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url; a.download = `skfolio_weights_${Date.now()}.json`;
                    document.body.appendChild(a); a.click(); document.body.removeChild(a);
                    URL.revokeObjectURL(url);
                  } catch (err: any) { setError(err?.message || String(err)); }
                }}
                style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.ORANGE}`, color: FINCEPT.ORANGE, borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.TINY, fontWeight: TYPOGRAPHY.BOLD, cursor: 'pointer', marginTop: SPACING.MEDIUM, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.SMALL }}
              >
                <Download size={12} /> EXPORT JSON
              </button>
            </div>
          </div>
        </div>
      )}

      {/* BLACK-LITTERMAN TAB */}
      {activeTab === 'blacklitterman' && (
        <div>
          {/* Views Input */}
          <div style={{ marginBottom: SPACING.XLARGE }}>
            <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.CYAN}`, paddingBottom: SPACING.MEDIUM }}>
              BLACK-LITTERMAN VIEWS (PyPortfolioOpt)
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.LARGE, marginBottom: SPACING.LARGE }}>
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>VIEWS (JSON: symbol → expected return)</label>
                <input type="text" value={blViews} onChange={(e) => setBlViews(e.target.value)}
                  style={{ ...COMMON_STYLES.inputField }} placeholder='{"AAPL": 0.10, "MSFT": 0.05}' />
              </div>
              <div>
                <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: SPACING.SMALL }}>CONFIDENCES (JSON array)</label>
                <input type="text" value={blConfidences} onChange={(e) => setBlConfidences(e.target.value)}
                  style={{ ...COMMON_STYLES.inputField }} placeholder='[0.8, 0.6]' />
              </div>
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.LARGE }}>
              <button onClick={handleBlackLitterman} disabled={loadingAction === 'bl'}
                style={{ padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'bl' ? FINCEPT.MUTED : FINCEPT.CYAN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'bl' ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
              >
                <Brain size={14} /> {loadingAction === 'bl' ? 'RUNNING...' : 'BLACK-LITTERMAN'}
              </button>
              <button onClick={handleViewsOptimization} disabled={loadingAction === 'viewsopt'}
                style={{ padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'viewsopt' ? FINCEPT.MUTED : FINCEPT.ORANGE, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'viewsopt' ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
              >
                <Target size={14} /> {loadingAction === 'viewsopt' ? 'RUNNING...' : 'VIEWS OPTIMIZATION'}
              </button>
            </div>
          </div>

          {/* BL Results */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE, marginBottom: SPACING.XLARGE }}>
            {blResults && (
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>BLACK-LITTERMAN RESULTS</div>
                <OptResultGrid data={blResults} color={FINCEPT.CYAN} />
              </div>
            )}
            {viewsOptResults && (
              <div>
                <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.MEDIUM }}>VIEWS OPTIMIZATION RESULTS</div>
                <OptResultGrid data={viewsOptResults} color={FINCEPT.ORANGE} />
              </div>
            )}
          </div>

          {/* HRP */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.XLARGE, marginBottom: SPACING.XLARGE }}>
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.GREEN}`, paddingBottom: SPACING.MEDIUM }}>
                HIERARCHICAL RISK PARITY (PyPortfolioOpt)
              </div>
              <button onClick={handleHRP} disabled={loadingAction === 'hrp'}
                style={{ width: '100%', padding: `${SPACING.DEFAULT} ${SPACING.LARGE}`, backgroundColor: loadingAction === 'hrp' ? FINCEPT.MUTED : FINCEPT.GREEN, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'hrp' ? 'not-allowed' : 'pointer', marginTop: SPACING.MEDIUM, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
              >
                <Layers size={14} /> {loadingAction === 'hrp' ? 'RUNNING...' : 'RUN HRP'}
              </button>
              {hrpResults && <div style={{ marginTop: SPACING.MEDIUM }}><OptResultGrid data={hrpResults} color={FINCEPT.GREEN} /></div>}
            </div>

            {/* Custom Constraints */}
            <div>
              <div style={{ ...COMMON_STYLES.sectionHeader, borderBottom: `1px solid ${FINCEPT.YELLOW}`, paddingBottom: SPACING.MEDIUM }}>
                SECTOR CONSTRAINTS (PyPortfolioOpt)
              </div>
              <div style={{ display: 'grid', gap: SPACING.MEDIUM, marginTop: SPACING.MEDIUM }}>
                <div>
                  <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>SECTOR MAP (JSON)</label>
                  <input type="text" value={sectorMapper} onChange={(e) => setSectorMapper(e.target.value)}
                    style={{ ...COMMON_STYLES.inputField, fontSize: TYPOGRAPHY.TINY }} />
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.SMALL }}>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>LOWER BOUNDS</label>
                    <input type="text" value={sectorLower} onChange={(e) => setSectorLower(e.target.value)}
                      style={{ ...COMMON_STYLES.inputField, fontSize: TYPOGRAPHY.TINY }} />
                  </div>
                  <div>
                    <label style={{ ...COMMON_STYLES.dataLabel, display: 'block', marginBottom: '2px' }}>UPPER BOUNDS</label>
                    <input type="text" value={sectorUpper} onChange={(e) => setSectorUpper(e.target.value)}
                      style={{ ...COMMON_STYLES.inputField, fontSize: TYPOGRAPHY.TINY }} />
                  </div>
                </div>
                <button onClick={handleCustomConstraints} disabled={loadingAction === 'constraints'}
                  style={{ width: '100%', padding: SPACING.DEFAULT, backgroundColor: loadingAction === 'constraints' ? FINCEPT.MUTED : FINCEPT.YELLOW, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '2px', fontFamily: TYPOGRAPHY.MONO, fontSize: TYPOGRAPHY.SMALL, fontWeight: TYPOGRAPHY.BOLD, cursor: loadingAction === 'constraints' ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: SPACING.MEDIUM }}
                >
                  <Shield size={14} /> {loadingAction === 'constraints' ? 'RUNNING...' : 'OPTIMIZE WITH CONSTRAINTS'}
                </button>
              </div>
              {constraintsResults && <div style={{ marginTop: SPACING.MEDIUM }}><OptResultGrid data={constraintsResults} color={FINCEPT.YELLOW} /></div>}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

/** Generic result grid to display Python response data */
const OptResultGrid: React.FC<{ data: any; color: string }> = ({ data, color }) => {
  if (!data || typeof data !== 'object') {
    return <div style={{ color: FINCEPT.MUTED, fontSize: TYPOGRAPHY.SMALL, fontFamily: TYPOGRAPHY.MONO, padding: SPACING.MEDIUM, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, whiteSpace: 'pre-wrap', maxHeight: '250px', overflow: 'auto' }}>
      {typeof data === 'string' ? data : JSON.stringify(data, null, 2)}
    </div>;
  }

  const flat = Object.entries(data).filter(([, v]) => v !== null && v !== undefined && typeof v !== 'object');
  const nested = Object.entries(data).filter(([, v]) => typeof v === 'object' && v !== null && !Array.isArray(v));

  return (
    <div>
      {flat.length > 0 && (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))', gap: SPACING.MEDIUM, marginBottom: SPACING.MEDIUM }}>
          {flat.map(([k, v]) => (
            <div key={k} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${color}30` }}>
              <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{k.replace(/_/g, ' ').toUpperCase()}</div>
              <div style={{ color, fontSize: '12px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                {typeof v === 'number' ? (Math.abs(v as number) < 1 ? (v as number).toFixed(4) : (v as number).toFixed(2)) : String(v)}
              </div>
            </div>
          ))}
        </div>
      )}
      {nested.map(([sectionKey, sectionData]) => (
        <div key={sectionKey} style={{ marginBottom: SPACING.MEDIUM }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.3px', marginBottom: SPACING.SMALL }}>
            {sectionKey.replace(/_/g, ' ').toUpperCase()}
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))', gap: SPACING.MEDIUM }}>
            {Object.entries(sectionData as Record<string, any>).filter(([, v]) => v !== null && typeof v !== 'object').map(([k, v]) => (
              <div key={k} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${color}20` }}>
                <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{k.replace(/_/g, ' ').toUpperCase()}</div>
                <div style={{ color, fontSize: '11px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                  {typeof v === 'number' ? (Math.abs(v as number) < 1 ? (v as number).toFixed(4) : (v as number).toFixed(2)) : String(v)}
                </div>
              </div>
            ))}
          </div>
        </div>
      ))}
      {flat.length === 0 && nested.length === 0 && (
        <div style={{ color: FINCEPT.MUTED, fontSize: TYPOGRAPHY.SMALL, fontFamily: TYPOGRAPHY.MONO, padding: SPACING.MEDIUM, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, whiteSpace: 'pre-wrap', maxHeight: '250px', overflow: 'auto' }}>
          {JSON.stringify(data, null, 2)}
        </div>
      )}
    </div>
  );
};

export default PortfolioOptimizationView;
