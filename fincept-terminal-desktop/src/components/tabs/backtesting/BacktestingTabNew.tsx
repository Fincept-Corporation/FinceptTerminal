/**
 * BacktestingTabNew - Orchestrator Component
 *
 * Thin orchestrator managing state, provider registration, and backtest execution.
 * Delegates UI to ConfigPanel, ChartPanel, and ResultsPanel.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { Activity, AlertTriangle, Zap } from 'lucide-react';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { BacktestRequest } from '@/services/backtesting/interfaces/types';
import { sqliteService } from '@/services/core/sqliteService';
import { portfolioService, Portfolio } from '@/services/portfolio/portfolioService';
import {
  F, panelStyle, BacktestConfigExtended, StrategyType,
  ExtendedEquityPoint, ExtendedTrade, AdvancedMetrics, MonthlyReturnsData, RollingMetric,
  getDefaultParameters,
} from './backtestingStyles';
import ConfigPanel from './ConfigPanel';
import ChartPanel from './ChartPanel';
import ResultsPanel from './ResultsPanel';
import OptimizationPanel from './OptimizationPanel';

// ============================================================================
// Component
// ============================================================================

const BacktestingTabNew: React.FC = () => {
  const { t } = useTranslation('backtesting');

  // --- Provider State ---
  const [activeProvider, setActiveProvider] = useState<string | null>(null);
  const [availableProviders, setAvailableProviders] = useState<string[]>([]);

  // --- Config State ---
  const [config, setConfig] = useState<BacktestConfigExtended>({
    symbols: ['SPY'],
    weights: [1.0],
    startDate: new Date(Date.now() - 366 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    endDate: new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    initialCapital: 100000,
    strategyType: 'sma_crossover',
    parameters: { fastPeriod: 10, slowPeriod: 20 },
    commission: 0.001,
    slippage: 0.0005,
    leverage: 1,
    benchmark: 'SPY',
    positionSizing: 'fixed',
    positionSizeValue: 1.0,
    stopLoss: 0,
    takeProfit: 0,
    trailingStop: 0,
    allowShort: false,
    compareRandom: false,
  });

  // --- Result State ---
  const [isRunning, setIsRunning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [equity, setEquity] = useState<ExtendedEquityPoint[]>([]);
  const [trades, setTrades] = useState<ExtendedTrade[]>([]);
  const [performance, setPerformance] = useState<Record<string, number>>({});
  const [statistics, setStatistics] = useState<Record<string, number | string>>({});
  const [advancedMetrics, setAdvancedMetrics] = useState<AdvancedMetrics | undefined>();
  const [monthlyReturns, setMonthlyReturns] = useState<MonthlyReturnsData[]>([]);
  const [usingSyntheticData, setUsingSyntheticData] = useState(false);
  const [extendedStats, setExtendedStats] = useState<Record<string, number | string>>({});
  const [rollingSharpe, setRollingSharpe] = useState<RollingMetric[]>([]);
  const [rollingVolatility, setRollingVolatility] = useState<RollingMetric[]>([]);
  const [rollingDrawdown, setRollingDrawdown] = useState<RollingMetric[]>([]);
  const [tradeAnalysis, setTradeAnalysis] = useState<Record<string, number | string>>({});
  const [drawdownAnalysis, setDrawdownAnalysis] = useState<any>(null);
  const [showOptimization, setShowOptimization] = useState(false);
  const [isOptimizing, setIsOptimizing] = useState(false);

  // --- Portfolio State ---
  const [portfolios, setPortfolios] = useState<Portfolio[]>([]);

  // --- Provider Registration ---
  useEffect(() => {
    registerProviders();
  }, []);

  // --- Fetch User Portfolios ---
  useEffect(() => {
    portfolioService.getPortfolios()
      .then(setPortfolios)
      .catch(() => setPortfolios([]));
  }, []);

  // --- Portfolio Selection Handler ---
  const handlePortfolioSelect = useCallback(async (portfolioId: string) => {
    if (!portfolioId) return;
    try {
      const assets = await portfolioService.getPortfolioAssets(portfolioId);
      if (assets.length === 0) return;

      const symbols = assets.map(a => a.symbol);
      const costBases = assets.map(a => a.quantity * a.avg_buy_price);
      const totalCost = costBases.reduce((s, c) => s + c, 0);

      const weights = totalCost > 0
        ? costBases.map(c => c / totalCost)
        : assets.map(() => 1.0 / assets.length);

      setConfig(prev => ({ ...prev, symbols, weights }));
    } catch (err) {
      console.warn('[Backtesting] Failed to load portfolio assets:', err);
    }
  }, []);

  const registerProviders = async () => {
    try {
      if (!backtestingRegistry.hasProviders()) {
        try {
          const { backtestingPyAdapter } = await import(
            '@/services/backtesting/adapters/backtestingpy/BacktestingPyAdapter'
          );
          backtestingRegistry.registerProvider(backtestingPyAdapter);
        } catch (err) {
          console.warn('[Backtesting] Failed to register default provider:', err);
        }
      }

      const allProviders = backtestingRegistry.listProviders();
      const providerNames = allProviders.map((p: { name: string }) => p.name);
      setAvailableProviders(providerNames);

      try {
        const dbProviders = await sqliteService.getBacktestingProviders();
        const activeDbProvider = dbProviders.find(
          (p: { is_active?: boolean | number; name: string }) =>
            p.is_active === true || (p.is_active as unknown) === 1
        );
        if (activeDbProvider && providerNames.includes(activeDbProvider.name)) {
          setActiveProvider(activeDbProvider.name);
        } else if (providerNames.length > 0) {
          setActiveProvider(providerNames[0]);
        }
      } catch {
        if (providerNames.length > 0) setActiveProvider(providerNames[0]);
      }
    } catch (err) {
      console.error('[Backtesting] Failed to register providers:', err);
      setError('Failed to load backtesting providers');
    }
  };

  // --- Provider Switch ---
  const handleProviderChange = useCallback(async (providerName: string) => {
    const provider = backtestingRegistry.getProvider(providerName);
    if (!provider) return;
    setActiveProvider(providerName);
    setError(null);
    try { await sqliteService.setActiveBacktestingProvider(providerName); } catch { /* non-critical */ }
  }, []);

  // --- Run Backtest ---
  const handleRunBacktest = useCallback(async () => {
    if (!activeProvider) { setError('No provider selected'); return; }
    if (config.symbols.length === 0) { setError('Add at least one symbol'); return; }

    setIsRunning(true);
    setError(null);
    setEquity([]);
    setTrades([]);
    setPerformance({});
    setStatistics({});
    setAdvancedMetrics(undefined);
    setMonthlyReturns([]);
    setExtendedStats({});
    setRollingSharpe([]);
    setRollingVolatility([]);
    setRollingDrawdown([]);
    setTradeAnalysis({});
    setDrawdownAnalysis(null);

    try {
      const provider = backtestingRegistry.getProvider(activeProvider);
      if (!provider) throw new Error('Provider not found');

      const request: BacktestRequest = {
        strategy: {
          name: `${config.strategyType} - ${config.symbols.join(',')}`,
          type: config.strategyType,
          parameters: config.parameters,
        },
        startDate: config.startDate,
        endDate: config.endDate,
        initialCapital: config.initialCapital,
        assets: config.symbols.map((sym, i) => ({
          symbol: sym,
          assetClass: 'stocks' as const,
          weight: config.weights[i] || 1.0 / config.symbols.length,
        })),
        commission: config.commission,
        slippage: config.slippage,
        tradeOnClose: false,
        hedging: false,
        exclusiveOrders: true,
        margin: config.leverage,
        leverage: config.leverage,
        benchmark: config.benchmark || undefined,
        stopLoss: config.stopLoss || undefined,
        takeProfit: config.takeProfit || undefined,
        trailingStop: config.trailingStop || undefined,
        positionSizing: config.positionSizing,
        positionSizeValue: config.positionSizeValue,
        allowShort: config.allowShort,
        compareRandom: config.compareRandom || undefined,
      };

      const result = await provider.runBacktest(request);

      // Parse result
      if (result) {
        setPerformance((result.performance || {}) as unknown as Record<string, number>);
        setStatistics((result.statistics || {}) as unknown as Record<string, number | string>);
        setEquity((result.equity || []) as ExtendedEquityPoint[]);
        setTrades((result.trades || []) as ExtendedTrade[]);
        setUsingSyntheticData(!!(result as any).usingSyntheticData);

        // Advanced metrics (may come from Python as advancedMetrics or advanced_metrics)
        const am = (result as any).advancedMetrics || (result as any).advanced_metrics;
        if (am) setAdvancedMetrics(am);

        const mr = (result as any).monthlyReturns || (result as any).monthly_returns;
        if (mr) setMonthlyReturns(mr);

        // Extended stats from backtesting.py (SQN, Kelly, CAGR, etc.)
        const es = (result as any).extendedStats || (result as any).extended_stats;
        if (es) setExtendedStats(es);

        // Rolling metrics
        const rm = (result as any).rollingMetrics || (result as any).rolling_metrics;
        if (rm) {
          if (rm.rollingSharpe) setRollingSharpe(rm.rollingSharpe);
          if (rm.rollingVolatility) setRollingVolatility(rm.rollingVolatility);
          if (rm.rollingDrawdown) setRollingDrawdown(rm.rollingDrawdown);
        }
        // Also check advancedMetrics for rolling data
        if (am) {
          if (am.rollingSharpe && am.rollingSharpe.length > 0) setRollingSharpe(am.rollingSharpe);
          if (am.rollingVolatility && am.rollingVolatility.length > 0) setRollingVolatility(am.rollingVolatility);
          if (am.rollingDrawdown && am.rollingDrawdown.length > 0) setRollingDrawdown(am.rollingDrawdown);
        }

        // Trade analysis & drawdown analysis
        const ta = (result as any).tradeAnalysis || (result as any).trade_analysis;
        if (ta) setTradeAnalysis(ta);
        const da = (result as any).drawdownAnalysis || (result as any).drawdown_analysis;
        if (da) setDrawdownAnalysis(da);
      }
    } catch (err) {
      console.error('[Backtesting] Error:', err);
      // Clean up double-wrapped error messages
      let errMsg = String(err).replace(/^Error:\s*/g, '');
      errMsg = errMsg.replace(/VectorBT backtest failed:\s*VectorBT backtest failed:\s*/g, 'VectorBT backtest failed: ');
      setError(errMsg);
    } finally {
      setIsRunning(false);
    }
  }, [activeProvider, config]);

  // --- Optimization Handler ---
  const handleOptimize = useCallback(async (params: {
    parameterRanges: { key: string; min: number; max: number; step: number }[];
    objective: string;
    algorithm: 'grid' | 'random';
    maxIterations?: number;
  }) => {
    if (!activeProvider) return null;
    setIsOptimizing(true);
    try {
      const provider = backtestingRegistry.getProvider(activeProvider);
      if (!provider || !(provider as any).optimize) throw new Error('Provider does not support optimization');

      const result = await (provider as any).optimize({
        strategy: { type: config.strategyType, parameters: config.parameters },
        parameterGrid: params.parameterRanges.map(r => ({ name: r.key, min: r.min, max: r.max, step: r.step })),
        objective: params.objective,
        algorithm: params.algorithm,
        maxIterations: params.maxIterations,
        startDate: config.startDate,
        endDate: config.endDate,
        initialCapital: config.initialCapital,
        assets: config.symbols.map((sym, i) => ({
          symbol: sym, assetClass: 'stocks' as const,
          weight: config.weights[i] || 1.0 / config.symbols.length,
        })),
      });
      return result;
    } catch (err) {
      setError(String(err));
      return null;
    } finally {
      setIsOptimizing(false);
    }
  }, [activeProvider, config]);

  // --- Walk-Forward Handler ---
  const handleWalkForward = useCallback(async (params: {
    parameterRanges: { key: string; min: number; max: number; step: number }[];
    objective: string;
    nSplits: number;
    trainRatio: number;
  }) => {
    if (!activeProvider) return null;
    setIsOptimizing(true);
    try {
      const provider = backtestingRegistry.getProvider(activeProvider);
      if (!provider || !(provider as any).walkForward) throw new Error('Provider does not support walk-forward');

      const result = await (provider as any).walkForward({
        strategy: { type: config.strategyType, parameters: config.parameters },
        parameterRanges: Object.fromEntries(
          params.parameterRanges.map(r => [r.key, { min: r.min, max: r.max, step: r.step }])
        ),
        objective: params.objective,
        nSplits: params.nSplits,
        trainRatio: params.trainRatio,
        startDate: config.startDate,
        endDate: config.endDate,
        initialCapital: config.initialCapital,
        assets: config.symbols.map((sym, i) => ({
          symbol: sym, assetClass: 'stocks' as const,
          weight: config.weights[i] || 1.0 / config.symbols.length,
        })),
      });
      return result;
    } catch (err) {
      setError(String(err));
      return null;
    } finally {
      setIsOptimizing(false);
    }
  }, [activeProvider, config]);

  // ============================================================================
  // Render
  // ============================================================================

  return (
    <div style={{
      display: 'flex', flexDirection: 'column', height: '100%',
      backgroundColor: F.DARK_BG, fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* Title Bar */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '8px',
        padding: '8px 14px', borderBottom: `1px solid ${F.BORDER}`,
        backgroundColor: F.PANEL_BG,
      }}>
        <Activity size={14} color={F.ORANGE} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
          BACKTESTING
        </span>
        {activeProvider && (
          <span style={{ fontSize: '9px', color: F.GRAY, marginLeft: '8px' }}>
            [{activeProvider}]
          </span>
        )}
        <button
          onClick={() => setShowOptimization(!showOptimization)}
          style={{
            marginLeft: 'auto', padding: '4px 8px', fontSize: '9px', fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace', cursor: 'pointer',
            backgroundColor: showOptimization ? F.ORANGE : F.HEADER_BG,
            color: showOptimization ? F.DARK_BG : F.GRAY,
            border: `1px solid ${showOptimization ? F.ORANGE : F.BORDER}`,
            borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px',
          }}
        >
          <Zap size={10} />OPTIMIZE
        </button>
        {(isRunning || isOptimizing) && (
          <span style={{ fontSize: '9px', color: F.ORANGE }}>
            {isOptimizing ? 'OPTIMIZING...' : 'RUNNING...'}
          </span>
        )}
      </div>

      {/* Error Banner */}
      {error && (
        <div style={{
          display: 'flex', alignItems: 'flex-start', gap: '8px',
          padding: '8px 14px', backgroundColor: '#1A0000',
          borderBottom: `1px solid ${F.RED}`, fontSize: '10px', color: F.RED,
          maxHeight: '120px', overflowY: 'auto', whiteSpace: 'pre-wrap', wordBreak: 'break-word',
        }}>
          <AlertTriangle size={12} style={{ flexShrink: 0, marginTop: 2 }} />
          <span>{error}</span>
        </div>
      )}

      {/* Main Layout: Config | Charts | Results */}
      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left: Config Panel */}
        <div style={{ width: '280px', borderRight: `1px solid ${F.BORDER}`, flexShrink: 0 }}>
          <ConfigPanel
            config={config}
            onConfigChange={setConfig}
            onRunBacktest={handleRunBacktest}
            isRunning={isRunning}
            availableProviders={availableProviders}
            activeProvider={activeProvider}
            onProviderChange={handleProviderChange}
            portfolios={portfolios}
            onPortfolioSelect={handlePortfolioSelect}
          />
        </div>

        {/* Center: Chart Panel */}
        <div style={{ flex: 1, minWidth: 0 }}>
          <ChartPanel
            equity={equity}
            initialCapital={config.initialCapital}
            advancedMetrics={advancedMetrics}
            monthlyReturns={monthlyReturns}
            rollingSharpe={rollingSharpe}
            rollingVolatility={rollingVolatility}
            rollingDrawdown={rollingDrawdown}
          />
        </div>

        {/* Right: Results or Optimization Panel */}
        <div style={{ width: '300px', borderLeft: `1px solid ${F.BORDER}`, flexShrink: 0 }}>
          {showOptimization ? (
            <OptimizationPanel
              config={config}
              onOptimize={handleOptimize}
              onWalkForward={handleWalkForward}
              isRunning={isOptimizing}
            />
          ) : (
            <ResultsPanel
              performance={performance}
              statistics={statistics}
              trades={trades}
              advancedMetrics={advancedMetrics}
              usingSyntheticData={usingSyntheticData}
              extendedStats={extendedStats}
              tradeAnalysis={tradeAnalysis}
              drawdownAnalysis={drawdownAnalysis}
            />
          )}
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '12px',
        padding: '4px 14px', borderTop: `1px solid ${F.BORDER}`,
        backgroundColor: F.PANEL_BG, fontSize: '9px', color: F.GRAY,
      }}>
        <span>SYMBOLS: {config.symbols.join(', ') || 'None'}</span>
        <span>|</span>
        <span>RANGE: {config.startDate} → {config.endDate}</span>
        <span>|</span>
        <span>CAPITAL: ${config.initialCapital.toLocaleString()}</span>
        {equity.length > 0 && (
          <>
            <span>|</span>
            <span style={{ color: (performance.totalReturn || 0) >= 0 ? F.GREEN : F.RED }}>
              RETURN: {((performance.totalReturn || 0) * 100).toFixed(2)}%
            </span>
          </>
        )}
      </div>
    </div>
  );
};

export default BacktestingTabNew;
