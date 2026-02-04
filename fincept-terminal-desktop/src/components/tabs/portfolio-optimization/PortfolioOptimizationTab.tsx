import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { TrendingUp, BarChart3, PieChart, LineChart, Target, Shield, Calculator, Download } from 'lucide-react';

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

const PortfolioOptimizationTab: React.FC = () => {
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

  const [assetSymbols, setAssetSymbols] = useState<string>('AAPL,GOOGL,MSFT,AMZN,TSLA');
  const [results, setResults] = useState<OptimizationResults | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'optimize' | 'frontier' | 'backtest' | 'allocation' | 'risk'>('optimize');

  // Main optimization
  const handleOptimize = async () => {
    setLoading(true);
    setError(null);

    try {
      let response: string;

      if (library === 'skfolio') {
        // Use skfolio
        const skfolioParams = {
          ...skfolioConfig,
          optimization_method: config.optimization_method,
          objective_function: config.objective,
        };

        response = await invoke<string>('skfolio_optimize_portfolio', {
          pricesData: assetSymbols, // Will be fetched by Python script
          optimizationMethod: config.optimization_method,
          objectiveFunction: config.objective,
          riskMeasure: skfolioConfig.risk_measure,
          config: JSON.stringify(skfolioParams),
        });
      } else {
        // Use PyPortfolioOpt with PyO3
        // First, fetch price data (in production, this would come from your data source)
        const pricesJson = JSON.stringify({
          // This is placeholder - in production you'd fetch actual price data
          symbols: assetSymbols.split(',').map(s => s.trim()),
          // You'll need to integrate with your price data fetching service here
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

        response = await invoke<string>('pyportfolioopt_optimize', {
          pricesJson,
          config: pyportfoliooptConfig,
        });
      }

      const data = JSON.parse(response);
      setResults(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Optimization failed');
    } finally {
      setLoading(false);
    }
  };

  // Generate efficient frontier
  const handleGenerateFrontier = async () => {
    setLoading(true);
    setError(null);

    try {
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

      const response = await invoke<string>('pyportfolioopt_efficient_frontier', {
        pricesJson,
        config: pyportfoliooptConfig,
        numPoints: 100,
      });

      const data = JSON.parse(response);
      setResults({ ...results, efficient_frontier: data });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Frontier generation failed');
    } finally {
      setLoading(false);
    }
  };

  // Run backtest
  const handleBacktest = async () => {
    setLoading(true);
    setError(null);

    try {
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

      const response = await invoke<string>('pyportfolioopt_backtest', {
        pricesJson,
        config: pyportfoliooptConfig,
        rebalanceFrequency: 21,
        lookbackPeriod: 252,
      });

      const data = JSON.parse(response);
      setResults({ ...results, backtest_results: data });
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Backtest failed');
    } finally {
      setLoading(false);
    }
  };

  // Discrete allocation
  const handleDiscreteAllocation = async () => {
    if (!results?.weights) {
      setError('Run optimization first');
      return;
    }

    setLoading(true);
    setError(null);

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
    }
  };

  // Export results
  const handleExport = async (format: 'json' | 'csv') => {
    if (!results) return;

    try {
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

      const reportJson = await invoke<string>('pyportfolioopt_generate_report', {
        pricesJson,
        config: pyportfoliooptConfig,
      });

      // Download the report
      const blob = new Blob([reportJson], { type: format === 'json' ? 'application/json' : 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `portfolio_optimization_${Date.now()}.${format}`;
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
      {/* Header */}
      <div style={{
        borderBottom: '2px solid #FF8800',
        paddingBottom: '12px',
        marginBottom: '24px'
      }}>
        <h1 style={{
          color: '#FF8800',
          fontSize: '18px',
          fontWeight: 'bold',
          margin: 0,
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <TrendingUp size={24} />
          PORTFOLIO OPTIMIZATION - {library === 'skfolio' ? 'skfolio (2025)' : 'PyPortfolioOpt'}
        </h1>
      </div>

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

            {/* Asset Symbols */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{ color: '#787878', fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                ASSET SYMBOLS (comma-separated)
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
                type="text"
                inputMode="decimal"
                value={config.risk_free_rate * 100}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, risk_free_rate: (parseFloat(v) || 0) / 100 }); }}
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
                  type="text"
                  inputMode="decimal"
                  value={config.weight_bounds_min * 100}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, weight_bounds_min: (parseFloat(v) || 0) / 100 }); }}
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
                  type="text"
                  inputMode="decimal"
                  value={config.weight_bounds_max * 100}
                  onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, weight_bounds_max: (parseFloat(v) || 0) / 100 }); }}
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
                type="text"
                inputMode="decimal"
                value={config.gamma}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, gamma: parseFloat(v) || 0 }); }}
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
                type="text"
                inputMode="decimal"
                value={config.total_portfolio_value}
                onChange={(e) => { const v = e.target.value; if (v === '' || /^\d*\.?\d*$/.test(v)) setConfig({ ...config, total_portfolio_value: parseFloat(v) || 0 }); }}
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
              disabled={loading}
              style={{
                width: '100%',
                padding: '12px',
                backgroundColor: loading ? '#666' : '#FF8800',
                color: '#000',
                border: 'none',
                fontFamily: 'inherit',
                fontSize: '12px',
                fontWeight: 'bold',
                cursor: loading ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px'
              }}
            >
              <Target size={16} />
              {loading ? 'OPTIMIZING...' : 'RUN OPTIMIZATION'}
            </button>
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
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', fontSize: '10px' }}>
                      <div>
                        <div style={{ color: '#787878' }}>Expected Annual Return</div>
                        <div style={{ color: '#00D66F', fontSize: '14px', fontWeight: 'bold' }}>
                          {(results.performance_metrics.expected_annual_return * 100).toFixed(2)}%
                        </div>
                      </div>
                      <div>
                        <div style={{ color: '#787878' }}>Annual Volatility</div>
                        <div style={{ color: '#FFD700', fontSize: '14px', fontWeight: 'bold' }}>
                          {(results.performance_metrics.annual_volatility * 100).toFixed(2)}%
                        </div>
                      </div>
                      <div style={{ gridColumn: '1 / -1' }}>
                        <div style={{ color: '#787878' }}>Sharpe Ratio</div>
                        <div style={{ color: '#FF8800', fontSize: '18px', fontWeight: 'bold' }}>
                          {results.performance_metrics.sharpe_ratio.toFixed(3)}
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
          <button
            onClick={handleGenerateFrontier}
            disabled={loading}
            style={{
              padding: '12px 24px',
              backgroundColor: loading ? '#666' : '#FF8800',
              color: '#000',
              border: 'none',
              fontFamily: 'inherit',
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: loading ? 'not-allowed' : 'pointer',
              marginBottom: '24px'
            }}
          >
            {loading ? 'GENERATING...' : 'GENERATE EFFICIENT FRONTIER'}
          </button>

          {results?.efficient_frontier && (
            <div style={{ backgroundColor: '#0F0F0F', padding: '24px', border: '1px solid #333' }}>
              <div style={{ color: '#00E5FF', fontSize: '12px', fontWeight: 'bold', marginBottom: '16px' }}>
                EFFICIENT FRONTIER ({results.efficient_frontier.returns.length} points)
              </div>
              <div style={{ color: '#787878', fontSize: '10px' }}>
                Chart visualization would be rendered here using Recharts or similar
              </div>
            </div>
          )}
        </div>
      )}

      {/* BACKTEST TAB */}
      {activeTab === 'backtest' && (
        <div>
          <button
            onClick={handleBacktest}
            disabled={loading}
            style={{
              padding: '12px 24px',
              backgroundColor: loading ? '#666' : '#FF8800',
              color: '#000',
              border: 'none',
              fontFamily: 'inherit',
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: loading ? 'not-allowed' : 'pointer',
              marginBottom: '24px'
            }}
          >
            {loading ? 'RUNNING BACKTEST...' : 'RUN BACKTEST'}
          </button>

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
          <button
            onClick={handleDiscreteAllocation}
            disabled={loading || !results?.weights}
            style={{
              padding: '12px 24px',
              backgroundColor: (loading || !results?.weights) ? '#666' : '#FF8800',
              color: '#000',
              border: 'none',
              fontFamily: 'inherit',
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: (loading || !results?.weights) ? 'not-allowed' : 'pointer',
              marginBottom: '24px'
            }}
          >
            {loading ? 'CALCULATING...' : 'CALCULATE DISCRETE ALLOCATION'}
          </button>

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

export default PortfolioOptimizationTab;
