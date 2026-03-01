import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { TrendingUp, BarChart3, PieChart, LineChart, Target, Shield, Calculator, Download, Layers, Zap, AlertTriangle, GitCompare, Brain } from 'lucide-react';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { getSetting, saveSetting } from '@/services/core/sqliteService';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';
import { OptimizationConfig, OptimizationResults } from './optimization/types';
import { OptimizeTab } from './optimization/tabs/OptimizeTab';
import { FrontierTab } from './optimization/tabs/FrontierTab';
import { BacktestTab } from './optimization/tabs/BacktestTab';
import { AllocationTab } from './optimization/tabs/AllocationTab';
import { RiskTab } from './optimization/tabs/RiskTab';
import { StrategiesTab } from './optimization/tabs/StrategiesTab';
import { StressTestTab } from './optimization/tabs/StressTestTab';
import { CompareTab } from './optimization/tabs/CompareTab';
import { BlackLittermanTab } from './optimization/tabs/BlackLittermanTab';

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

      {/* Tab Content */}
      {activeTab === 'optimize' && (
        <OptimizeTab
          library={library}
          setLibrary={setLibrary}
          config={config}
          setConfig={setConfig}
          skfolioConfig={skfolioConfig}
          setSkfolioConfig={setSkfolioConfig}
          assetSymbols={assetSymbols}
          setAssetSymbols={setAssetSymbols}
          results={results}
          loading={loading}
          loadingAction={loadingAction}
          portfolioSummary={portfolioSummary}
          handleOptimize={handleOptimize}
          handleExport={handleExport}
        />
      )}

      {activeTab === 'frontier' && (
        <FrontierTab
          library={library}
          config={config}
          setConfig={setConfig}
          skfolioConfig={skfolioConfig}
          setSkfolioConfig={setSkfolioConfig}
          results={results}
          loadingAction={loadingAction}
          handleGenerateFrontier={handleGenerateFrontier}
        />
      )}

      {activeTab === 'backtest' && (
        <BacktestTab
          results={results}
          loadingAction={loadingAction}
          handleBacktest={handleBacktest}
        />
      )}

      {activeTab === 'allocation' && (
        <AllocationTab
          results={results}
          loadingAction={loadingAction}
          handleDiscreteAllocation={handleDiscreteAllocation}
        />
      )}

      {activeTab === 'risk' && (
        <RiskTab
          riskDecomp={riskDecomp}
          sensitivityResults={sensitivityResults}
          sensitivityParam={sensitivityParam}
          setSensitivityParam={setSensitivityParam}
          loadingAction={loadingAction}
          handleRiskDecomposition={handleRiskDecomposition}
          handleSensitivityAnalysis={handleSensitivityAnalysis}
        />
      )}

      {activeTab === 'strategies' && (
        <StrategiesTab
          assetSymbols={assetSymbols}
          setAssetSymbols={setAssetSymbols}
          selectedStrategy={selectedStrategy}
          setSelectedStrategy={setSelectedStrategy}
          strategyResults={strategyResults}
          marketNeutralLong={marketNeutralLong}
          setMarketNeutralLong={setMarketNeutralLong}
          marketNeutralShort={marketNeutralShort}
          setMarketNeutralShort={setMarketNeutralShort}
          benchmarkWeightsStr={benchmarkWeightsStr}
          setBenchmarkWeightsStr={setBenchmarkWeightsStr}
          loadingAction={loadingAction}
          handleStrategyOptimize={handleStrategyOptimize}
        />
      )}

      {activeTab === 'stress' && (
        <StressTestTab
          stressTestResults={stressTestResults}
          scenarioResults={scenarioResults}
          loadingAction={loadingAction}
          handleStressTest={handleStressTest}
          handleScenarioAnalysis={handleScenarioAnalysis}
        />
      )}

      {activeTab === 'compare' && (
        <CompareTab
          assetSymbols={assetSymbols}
          results={results}
          compareResults={compareResults}
          riskAttribution={riskAttribution}
          hypertuneResults={hypertuneResults}
          skfolioRiskMetrics={skfolioRiskMetrics}
          validateResults={validateResults}
          skfolioConfig={skfolioConfig}
          loadingAction={loadingAction}
          setError={setError}
          handleCompareStrategies={handleCompareStrategies}
          handleRiskAttribution={handleRiskAttribution}
          handleHyperparameterTuning={handleHyperparameterTuning}
          handleSkfolioRiskMetrics={handleSkfolioRiskMetrics}
        />
      )}

      {activeTab === 'blacklitterman' && (
        <BlackLittermanTab
          blViews={blViews}
          setBlViews={setBlViews}
          blConfidences={blConfidences}
          setBlConfidences={setBlConfidences}
          blResults={blResults}
          hrpResults={hrpResults}
          constraintsResults={constraintsResults}
          viewsOptResults={viewsOptResults}
          sectorMapper={sectorMapper}
          setSectorMapper={setSectorMapper}
          sectorLower={sectorLower}
          setSectorLower={setSectorLower}
          sectorUpper={sectorUpper}
          setSectorUpper={setSectorUpper}
          loadingAction={loadingAction}
          handleBlackLitterman={handleBlackLitterman}
          handleHRP={handleHRP}
          handleCustomConstraints={handleCustomConstraints}
          handleViewsOptimization={handleViewsOptimization}
        />
      )}
    </div>
  );
};

export default PortfolioOptimizationView;
