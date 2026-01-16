import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { TrendingUp, BarChart3, PieChart, LineChart, Target, Shield, Calculator, Download } from 'lucide-react';
import { PortfolioSummary } from '../../../../services/portfolioService';
import { LineChart as RechartsLine, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Scatter, ScatterChart, ZAxis } from 'recharts';
import { getSetting, saveSetting } from '@/services/sqliteService';

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
  const [activeTab, setActiveTab] = useState<'optimize' | 'frontier' | 'backtest' | 'allocation' | 'risk'>('optimize');

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
      setActiveTab(savedActiveTab as 'optimize' | 'backtest' | 'frontier' | 'allocation' | 'risk');
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

  return (
    <div style={{
      height: '100%',
      backgroundColor: '#000000',
      color: '#FFFFFF',
      fontFamily: 'IBM Plex Mono, monospace',
      overflow: 'auto',
      padding: '16px'
    }}>
      {/* Navigation Tabs */}
      <div style={{ display: 'flex', gap: '8px', marginBottom: '24px', borderBottom: '1px solid #333' }}>
        {[
          { id: 'optimize', label: 'OPTIMIZE', icon: Calculator },
          { id: 'frontier', label: 'EFFICIENT FRONTIER', icon: LineChart },
          { id: 'backtest', label: 'BACKTEST', icon: BarChart3 },
          { id: 'allocation', label: 'ALLOCATION', icon: PieChart },
          { id: 'risk', label: 'RISK ANALYSIS', icon: Shield },
        ].map(({ id, label, icon: Icon }) => (
          <button
            key={id}
            onClick={() => setActiveTab(id as any)}
            style={{
              padding: '8px 16px',
              backgroundColor: activeTab === id ? '#FF8800' : 'transparent',
              color: activeTab === id ? '#000' : '#FFF',
              border: 'none',
              borderBottom: activeTab === id ? '2px solid #FF8800' : '2px solid transparent',
              cursor: 'pointer',
              fontFamily: 'inherit',
              fontSize: '11px',
              fontWeight: 'bold',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
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
          border: '1px solid #333',
          flexDirection: 'column',
          gap: '8px',
          padding: '20px'
        }}>
          <div style={{
            width: '32px',
            height: '32px',
            border: '3px solid #333',
            borderTop: '3px solid #00E5FF',
            borderRadius: '50%',
            animation: 'spin 1s linear infinite'
          }} />
          <div style={{ color: '#00E5FF', fontSize: '11px', fontWeight: '500' }}>
            {activeTab === 'optimize' && 'Optimizing...'}
            {activeTab === 'backtest' && 'Running Backtest...'}
            {activeTab === 'allocation' && 'Calculating...'}
            {activeTab === 'risk' && 'Analyzing...'}
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
          padding: '12px',
          backgroundColor: 'rgba(255,59,59,0.1)',
          border: '1px solid #FF3B3B',
          color: '#FF3B3B',
          marginBottom: '16px',
          fontSize: '11px'
        }}>
          ERROR: {error}
        </div>
      )}

      {/* OPTIMIZE TAB */}
      {activeTab === 'optimize' && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '24px' }}>
          {/* Configuration Panel */}
          <div>
            <div style={{
              color: '#FF8800',
              fontSize: '12px',
              fontWeight: 'bold',
              marginBottom: '16px',
              borderBottom: '1px solid #FF8800',
              paddingBottom: '8px'
            }}>
              OPTIMIZATION CONFIGURATION
            </div>

            {/* Library Selection */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
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
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
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
                marginBottom: '16px',
                padding: '8px 12px',
                backgroundColor: '#0F0F0F',
                border: '1px solid #333',
                borderLeft: '3px solid #00E5FF'
              }}>
                <div style={{ fontSize: '9px', color: '#787878', marginBottom: '4px' }}>
                  OPTIMIZING PORTFOLIO
                </div>
                <div style={{ fontSize: '11px', color: '#00E5FF', fontWeight: 'bold' }}>
                  {portfolioSummary.portfolio.name}
                </div>
                <div style={{ fontSize: '9px', color: '#787878', marginTop: '4px' }}>
                  {portfolioSummary.holdings?.length || 0} holdings • Total Value: {new Intl.NumberFormat('en-US', {
                    style: 'currency',
                    currency: portfolioSummary.portfolio.currency || 'USD'
                  }).format(portfolioSummary.total_market_value || 0)}
                </div>
              </div>
            )}

            {/* Asset Symbols */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                ASSET SYMBOLS (comma-separated)
                {portfolioSummary?.holdings && portfolioSummary.holdings.length > 0 && (
                  <span style={{ color: '#00E5FF', marginLeft: '8px', fontSize: '9px' }}>
                    • From portfolio
                  </span>
                )}
              </label>
              <input
                type="text"
                value={assetSymbols}
                onChange={(e) => setAssetSymbols(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
                placeholder="AAPL,GOOGL,MSFT,AMZN,TSLA"
              />
              {(!portfolioSummary || portfolioSummary.holdings.length === 0) && (
                <div style={{ fontSize: '9px', color: '#FF8800', marginTop: '4px' }}>
                  No portfolio selected. Using default symbols.
                </div>
              )}
            </div>

            {/* Optimization Method */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                OPTIMIZATION METHOD
              </label>
              <select
                value={config.optimization_method}
                onChange={(e) => setConfig({ ...config, optimization_method: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              >
                {(library === 'skfolio' ? SKFOLIO_METHODS : PYPORTFOLIOOPT_METHODS).map(m => (
                  <option key={m.value} value={m.value}>{m.label} - {m.description}</option>
                ))}
              </select>
            </div>

            {/* Objective */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                OBJECTIVE FUNCTION
              </label>
              <select
                value={config.objective}
                onChange={(e) => setConfig({ ...config, objective: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              >
                {(library === 'skfolio' ? SKFOLIO_OBJECTIVES : PYPORTFOLIOOPT_OBJECTIVES).map(o => (
                  <option key={o.value} value={o.value}>{o.label}</option>
                ))}
              </select>
            </div>

            {/* skfolio-specific: Risk Measure */}
            {library === 'skfolio' && (
              <div style={{ marginBottom: '16px' }}>
                <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  RISK MEASURE
                </label>
                <select
                  value={skfolioConfig.risk_measure}
                  onChange={(e) => setSkfolioConfig({ ...skfolioConfig, risk_measure: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #333',
                    color: '#FFF',
                    fontFamily: 'inherit',
                    fontSize: '11px'
                  }}
                >
                  {SKFOLIO_RISK_MEASURES.map(r => (
                    <option key={r.value} value={r.value}>{r.label}</option>
                  ))}
                </select>
              </div>
            )}

            {/* Expected Returns Method */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                EXPECTED RETURNS METHOD
              </label>
              <select
                value={config.expected_returns_method}
                onChange={(e) => setConfig({ ...config, expected_returns_method: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              >
                {EXPECTED_RETURNS_METHODS.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Risk Model */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                RISK MODEL
              </label>
              <select
                value={config.risk_model_method}
                onChange={(e) => setConfig({ ...config, risk_model_method: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              >
                {RISK_MODELS.map(m => (
                  <option key={m.value} value={m.value}>{m.label}</option>
                ))}
              </select>
            </div>

            {/* Risk-Free Rate */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                RISK-FREE RATE (%)
              </label>
              <input
                type="number"
                step="0.01"
                value={config.risk_free_rate * 100}
                onChange={(e) => setConfig({ ...config, risk_free_rate: parseFloat(e.target.value) / 100 })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              />
            </div>

            {/* Weight Bounds */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '16px' }}>
              <div>
                <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  MIN WEIGHT (%)
                </label>
                <input
                  type="number"
                  step="1"
                  value={config.weight_bounds_min * 100}
                  onChange={(e) => setConfig({ ...config, weight_bounds_min: parseFloat(e.target.value) / 100 })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #333',
                    color: '#FFF',
                    fontFamily: 'inherit',
                    fontSize: '11px'
                  }}
                />
              </div>
              <div>
                <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  MAX WEIGHT (%)
                </label>
                <input
                  type="number"
                  step="1"
                  value={config.weight_bounds_max * 100}
                  onChange={(e) => setConfig({ ...config, weight_bounds_max: parseFloat(e.target.value) / 100 })}
                  style={{
                    width: '100%',
                    padding: '8px',
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #333',
                    color: '#FFF',
                    fontFamily: 'inherit',
                    fontSize: '11px'
                  }}
                />
              </div>
            </div>

            {/* L2 Regularization */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                L2 REGULARIZATION (Gamma)
              </label>
              <input
                type="number"
                step="0.1"
                value={config.gamma}
                onChange={(e) => setConfig({ ...config, gamma: parseFloat(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              />
            </div>

            {/* Portfolio Value */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                TOTAL PORTFOLIO VALUE ($)
              </label>
              <input
                type="number"
                step="1000"
                value={config.total_portfolio_value}
                onChange={(e) => setConfig({ ...config, total_portfolio_value: parseFloat(e.target.value) })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #333',
                  color: '#FFF',
                  fontFamily: 'inherit',
                  fontSize: '11px'
                }}
              />
            </div>

            {/* Optimize Button */}
            <button
              onClick={handleOptimize}
              disabled={loadingAction === 'optimize'}
              style={{
                width: '100%',
                padding: '12px',
                backgroundColor: loadingAction === 'optimize' ? '#4A4A4A' : '#FF8800',
                color: loadingAction === 'optimize' ? '#787878' : '#000',
                border: loadingAction === 'optimize' ? '1px solid #666' : 'none',
                fontFamily: 'inherit',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: loadingAction === 'optimize' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'optimize' ? 0.6 : 1,
                transition: 'all 0.2s ease',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px'
              }}
            >
              <Target size={16} />
              {loadingAction === 'optimize' ? 'OPTIMIZING...' : 'RUN OPTIMIZATION'}
            </button>
            {loadingAction === 'optimize' && (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px', marginTop: '12px' }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: '2px solid #333',
                  borderTop: '2px solid #00E5FF',
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: '#00E5FF', fontSize: '11px' }}>Running optimization...</span>
              </div>
            )}
          </div>

          {/* Results Panel */}
          <div>
            <div style={{
              color: '#FF8800',
              fontSize: '12px',
              fontWeight: 'bold',
              marginBottom: '16px',
              borderBottom: '1px solid #FF8800',
              paddingBottom: '8px'
            }}>
              OPTIMIZATION RESULTS
            </div>

            {results?.performance_metrics && (
              <>
                {/* Performance Metrics */}
                <div style={{ marginBottom: '24px' }}>
                  <div style={{ color: '#00E5FF', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
                    PERFORMANCE METRICS
                  </div>
                  <div style={{ backgroundColor: '#0F0F0F', padding: '12px', border: '1px solid #333' }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px', fontSize: '10px' }}>
                      <div>
                        <div style={{ color: '#787878' }}>Expected Annual Return</div>
                        <div style={{ color: '#00D66F', fontSize: '14px', fontWeight: 'bold' }}>
                          {(results.performance_metrics.expected_annual_return * 100).toFixed(2)}%
                        </div>
                      </div>
                      <div>
                        <div style={{ color: '#787878' }}>Annual Volatility</div>
                        <div style={{ color: '#FFD700', fontSize: '14px', fontWeight: 'bold' }}>
                          {results.performance_metrics.annual_volatility > 0
                            ? `${(results.performance_metrics.annual_volatility * 100).toFixed(2)}%`
                            : 'N/A'}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: '#787878' }}>Sharpe Ratio</div>
                        <div style={{ color: '#FF8800', fontSize: '14px', fontWeight: 'bold' }}>
                          {results.performance_metrics.sharpe_ratio.toFixed(3)}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: '#787878' }}>Sortino Ratio</div>
                        <div style={{ color: '#00E5FF', fontSize: '14px', fontWeight: 'bold' }}>
                          {results.performance_metrics.sortino_ratio?.toFixed(3) || 'N/A'}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: '#787878' }}>Calmar Ratio</div>
                        <div style={{ color: '#9C27B0', fontSize: '14px', fontWeight: 'bold' }}>
                          {results.performance_metrics.calmar_ratio?.toFixed(3) || 'N/A'}
                        </div>
                      </div>
                      <div>
                        <div style={{ color: '#787878' }}>Max Drawdown</div>
                        <div style={{ color: '#FF5252', fontSize: '14px', fontWeight: 'bold' }}>
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
                  <div style={{ marginBottom: '24px' }}>
                    <div style={{ color: '#00E5FF', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
                      OPTIMAL WEIGHTS
                    </div>
                    <div style={{ backgroundColor: '#0F0F0F', border: '1px solid #333', maxHeight: '300px', overflow: 'auto' }}>
                      {Object.entries(results.weights)
                        .sort(([, a], [, b]) => b - a)
                        .map(([symbol, weight]) => (
                          <div
                            key={symbol}
                            style={{
                              padding: '8px 12px',
                              borderBottom: '1px solid #1A1A1A',
                              display: 'flex',
                              justifyContent: 'space-between',
                              alignItems: 'center',
                              fontSize: '10px'
                            }}
                          >
                            <span style={{ color: '#00E5FF', fontWeight: 'bold' }}>{symbol}</span>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                              <div
                                style={{
                                  width: '100px',
                                  height: '6px',
                                  backgroundColor: '#1A1A1A',
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
                                    backgroundColor: '#FF8800'
                                  }}
                                />
                              </div>
                              <span style={{ color: '#FFF', width: '60px', textAlign: 'right' }}>
                                {(weight * 100).toFixed(2)}%
                              </span>
                            </div>
                          </div>
                        ))}
                    </div>
                  </div>
                )}

                {/* Export Buttons */}
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button
                    onClick={() => handleExport('json')}
                    style={{
                      flex: 1,
                      padding: '8px',
                      backgroundColor: '#00D66F',
                      color: '#000',
                      border: 'none',
                      fontFamily: 'inherit',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '4px'
                    }}
                  >
                    <Download size={12} />
                    EXPORT JSON
                  </button>
                  <button
                    onClick={() => handleExport('csv')}
                    style={{
                      flex: 1,
                      padding: '8px',
                      backgroundColor: '#00E5FF',
                      color: '#000',
                      border: 'none',
                      fontFamily: 'inherit',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '4px'
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
                color: '#787878',
                fontSize: '11px',
                backgroundColor: '#0F0F0F',
                border: '1px solid #333'
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
          <div style={{ backgroundColor: '#0F0F0F', padding: '16px', border: '1px solid #333', marginBottom: '16px' }}>
            <div style={{ color: '#00E5FF', fontSize: '11px', fontWeight: 'bold', marginBottom: '12px' }}>
              FRONTIER PARAMETERS
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              {/* Number of portfolios */}
              <div>
                <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                  NUMBER OF POINTS
                </label>
                <input
                  type="number"
                  value={config.total_portfolio_value}
                  onChange={(e) => setConfig({ ...config, total_portfolio_value: parseInt(e.target.value) || 100 })}
                  min="10"
                  max="500"
                  style={{
                    width: '100%',
                    padding: '6px',
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #333',
                    color: '#FFF',
                    fontFamily: 'inherit',
                    fontSize: '11px'
                  }}
                />
              </div>

              {/* Risk Measure for skfolio */}
              {library === 'skfolio' && (
                <div>
                  <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                    RISK MEASURE
                  </label>
                  <select
                    value={skfolioConfig.risk_measure}
                    onChange={(e) => setSkfolioConfig({ ...skfolioConfig, risk_measure: e.target.value })}
                    style={{
                      width: '100%',
                      padding: '6px',
                      backgroundColor: '#1A1A1A',
                      border: '1px solid #333',
                      color: '#FFF',
                      fontFamily: 'inherit',
                      fontSize: '11px'
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

          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '24px' }}>
            <button
              onClick={handleGenerateFrontier}
              disabled={loadingAction === 'frontier'}
              style={{
                padding: '12px 24px',
                backgroundColor: loadingAction === 'frontier' ? '#4A4A4A' : '#FF8800',
                color: loadingAction === 'frontier' ? '#787878' : '#000',
                border: loadingAction === 'frontier' ? '1px solid #666' : 'none',
                fontFamily: 'inherit',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: loadingAction === 'frontier' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'frontier' ? 0.6 : 1,
                transition: 'all 0.2s ease',
              }}
            >
              {loadingAction === 'frontier' ? 'GENERATING...' : 'GENERATE EFFICIENT FRONTIER'}
            </button>
            {loadingAction === 'frontier' && (
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: '2px solid #333',
                  borderTop: '2px solid #00E5FF',
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: '#00E5FF', fontSize: '11px' }}>Generating frontier...</span>
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
            <div style={{ backgroundColor: '#0F0F0F', padding: '24px', border: '1px solid #333', marginTop: '16px' }}>
              <div style={{ color: '#00E5FF', fontSize: '12px', fontWeight: 'bold', marginBottom: '16px' }}>
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
                  let fillColor = '#FF3B3B'; // Red for low Sharpe
                  if (sharpe > 1.5) fillColor = '#00FF88'; // Green for high Sharpe
                  else if (sharpe > 0.8) fillColor = '#FFD700'; // Gold for medium Sharpe
                  else if (sharpe > 0.3) fillColor = '#FF8800'; // Orange for okay Sharpe

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
                          <stop offset="0%" stopColor="#FF3B3B" stopOpacity={0.8} />
                          <stop offset="50%" stopColor="#FFD700" stopOpacity={0.8} />
                          <stop offset="100%" stopColor="#00FF88" stopOpacity={0.8} />
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="#333" strokeOpacity={0.3} />
                      <XAxis
                        type="number"
                        dataKey="volatility"
                        name="Volatility"
                        unit="%"
                        stroke="#787878"
                        tick={{ fill: '#787878', fontSize: 10 }}
                        label={{ value: 'Annual Volatility (%)', position: 'insideBottom', offset: -15, fill: '#787878', fontSize: 11 }}
                      />
                      <YAxis
                        type="number"
                        dataKey="returns"
                        name="Returns"
                        unit="%"
                        stroke="#787878"
                        tick={{ fill: '#787878', fontSize: 10 }}
                        label={{ value: 'Annual Returns (%)', angle: -90, position: 'insideLeft', offset: -40, fill: '#787878', fontSize: 11 }}
                      />
                      <Tooltip
                        contentStyle={{
                          backgroundColor: '#1A1A1A',
                          border: '1px solid #00E5FF',
                          borderRadius: '4px',
                          fontSize: '10px',
                          padding: '8px'
                        }}
                        labelStyle={{ color: '#00E5FF' }}
                        itemStyle={{ color: '#FFF' }}
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
                        line={{ stroke: '#00E5FF', strokeWidth: 1, strokeOpacity: 0.5 }}
                        shape={<CustomDot />}
                        isAnimationActive={false}
                      />
                    </ScatterChart>
                  </ResponsiveContainer>
                );
              })()}

              {/* Legend */}
              <div style={{ marginTop: '16px', fontSize: '10px', color: '#787878', textAlign: 'center' }}>
                Each point represents an optimal portfolio. Color indicates Sharpe ratio:
                <span style={{ color: '#00FF88', marginLeft: '8px' }}>● High {'>1.5'}</span>
                <span style={{ color: '#FFD700', marginLeft: '8px' }}>● Good (0.8-1.5)</span>
                <span style={{ color: '#FF8800', marginLeft: '8px' }}>● Medium (0.3-0.8)</span>
                <span style={{ color: '#FF3B3B', marginLeft: '8px' }}>● Low {'<0.3'}</span>
              </div>
            </div>
          )}
        </div>
      )}

      {/* BACKTEST TAB */}
      {activeTab === 'backtest' && (
        <div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '24px' }}>
            <button
              onClick={handleBacktest}
              disabled={loadingAction === 'backtest'}
              style={{
                padding: '12px 24px',
                backgroundColor: loadingAction === 'backtest' ? '#4A4A4A' : '#FF8800',
                color: loadingAction === 'backtest' ? '#787878' : '#000',
                border: loadingAction === 'backtest' ? '1px solid #666' : 'none',
                fontFamily: 'inherit',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: loadingAction === 'backtest' ? 'not-allowed' : 'pointer',
                opacity: loadingAction === 'backtest' ? 0.6 : 1,
                transition: 'all 0.2s ease',
              }}
            >
              {loadingAction === 'backtest' ? 'RUNNING BACKTEST...' : 'RUN BACKTEST'}
            </button>
            {loadingAction === 'backtest' && (
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: '2px solid #333',
                  borderTop: '2px solid #00E5FF',
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: '#00E5FF', fontSize: '11px' }}>Running backtest...</span>
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
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '12px' }}>
              {[
                { label: 'Annual Return', value: results.backtest_results.annual_return, color: '#00D66F', format: '%' },
                { label: 'Annual Volatility', value: results.backtest_results.annual_volatility, color: '#FFD700', format: '%' },
                { label: 'Sharpe Ratio', value: results.backtest_results.sharpe_ratio, color: '#FF8800', format: '' },
                { label: 'Max Drawdown', value: results.backtest_results.max_drawdown, color: '#FF3B3B', format: '%' },
                { label: 'Calmar Ratio', value: results.backtest_results.calmar_ratio, color: '#00E5FF', format: '' },
              ].map(metric => (
                <div key={metric.label} style={{ backgroundColor: '#0F0F0F', padding: '16px', border: '1px solid #333' }}>
                  <div style={{ color: '#787878', fontSize: '9px', marginBottom: '4px' }}>{metric.label}</div>
                  <div style={{ color: metric.color, fontSize: '18px', fontWeight: 'bold' }}>
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
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '24px' }}>
            <button
              onClick={handleDiscreteAllocation}
              disabled={loadingAction === 'allocation' || !results?.weights}
              style={{
                padding: '12px 24px',
                backgroundColor: (loadingAction === 'allocation' || !results?.weights) ? '#4A4A4A' : '#FF8800',
                color: (loadingAction === 'allocation' || !results?.weights) ? '#787878' : '#000',
                border: (loadingAction === 'allocation' || !results?.weights) ? '1px solid #666' : 'none',
                fontFamily: 'inherit',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: (loadingAction === 'allocation' || !results?.weights) ? 'not-allowed' : 'pointer',
                opacity: (loadingAction === 'allocation' || !results?.weights) ? 0.6 : 1,
                transition: 'all 0.2s ease',
              }}
            >
              {loadingAction === 'allocation' ? 'CALCULATING...' : 'CALCULATE DISCRETE ALLOCATION'}
            </button>
            {loadingAction === 'allocation' && (
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{
                  width: '16px',
                  height: '16px',
                  border: '2px solid #333',
                  borderTop: '2px solid #00E5FF',
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite'
                }} />
                <span style={{ color: '#00E5FF', fontSize: '11px' }}>Calculating allocation...</span>
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
              padding: '16px',
              backgroundColor: 'rgba(255,136,0,0.1)',
              border: '1px solid #FF8800',
              color: '#FF8800',
              marginBottom: '24px',
              fontSize: '11px',
              borderRadius: '4px'
            }}>
              ℹ️ Please run optimization first to get portfolio weights
            </div>
          )}

          {results?.discrete_allocation && (
            <div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '24px' }}>
                <div style={{ backgroundColor: '#0F0F0F', padding: '16px', border: '1px solid #333' }}>
                  <div style={{ color: '#787878', fontSize: '9px' }}>Total Value</div>
                  <div style={{ color: '#00E5FF', fontSize: '18px', fontWeight: 'bold' }}>
                    ${results.discrete_allocation.total_value.toLocaleString()}
                  </div>
                </div>
                <div style={{ backgroundColor: '#0F0F0F', padding: '16px', border: '1px solid #333' }}>
                  <div style={{ color: '#787878', fontSize: '9px' }}>Allocated</div>
                  <div style={{ color: '#00D66F', fontSize: '18px', fontWeight: 'bold' }}>
                    ${(results.discrete_allocation.total_value - results.discrete_allocation.leftover_cash).toLocaleString()}
                  </div>
                </div>
                <div style={{ backgroundColor: '#0F0F0F', padding: '16px', border: '1px solid #333' }}>
                  <div style={{ color: '#787878', fontSize: '9px' }}>Leftover Cash</div>
                  <div style={{ color: '#FFD700', fontSize: '18px', fontWeight: 'bold' }}>
                    ${results.discrete_allocation.leftover_cash.toLocaleString()}
                  </div>
                </div>
              </div>

              <div style={{ backgroundColor: '#0F0F0F', padding: '16px', border: '1px solid #333' }}>
                <div style={{ color: '#00E5FF', fontSize: '11px', fontWeight: 'bold', marginBottom: '12px' }}>
                  SHARE ALLOCATION
                </div>
                {Object.entries(results.discrete_allocation.allocation).map(([symbol, shares]) => (
                  <div
                    key={symbol}
                    style={{
                      padding: '8px 0',
                      borderBottom: '1px solid #1A1A1A',
                      display: 'flex',
                      justifyContent: 'space-between',
                      fontSize: '11px'
                    }}
                  >
                    <span style={{ color: '#00E5FF', fontWeight: 'bold' }}>{symbol}</span>
                    <span style={{ color: '#FFF' }}>{shares} shares</span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {/* RISK ANALYSIS TAB */}
      {activeTab === 'risk' && (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          backgroundColor: '#0F0F0F',
          border: '1px solid #333',
          color: '#787878',
          fontSize: '11px'
        }}>
          Risk decomposition and correlation matrix analysis coming soon...
        </div>
      )}
    </div>
  );
};

export default PortfolioOptimizationView;
