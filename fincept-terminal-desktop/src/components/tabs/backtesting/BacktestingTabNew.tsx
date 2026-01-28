/**
 * BacktestingTabNew - Full VectorBT Capabilities UI
 *
 * 9 sub-tabs exposing all VectorBT Python modules:
 * Backtest | Optimize | Data | Indicators | Signals | Labels | Splitters | Catalog | Returns
 *
 * All results displayed as raw JSON in a scrollable <pre> block.
 */

import React, { useState, useEffect, useCallback } from 'react';
import { Activity, AlertTriangle, Play, Loader, Plus, Trash2 } from 'lucide-react';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { BacktestRequest } from '@/services/backtesting/interfaces/types';
import { sqliteService } from '@/services/core/sqliteService';
import { VectorBTAdapter } from '@/services/backtesting/adapters/vectorbt/VectorBTAdapter';
import {
  F, BacktestConfigExtended, StrategyType, BacktestingSubTab,
  OptimizationObjective, DataSourceType, SignalGeneratorType, LabelGeneratorType, SplitterType,
  ReturnsAnalysisType, RollingMetric, IndicatorMode,
  STRATEGY_DEFINITIONS, getDefaultParameters, ParamDef,
  SIGNAL_GENERATOR_DEFINITIONS, LABEL_GENERATOR_DEFINITIONS,
  SPLITTER_DEFINITIONS, DATA_SOURCE_DEFINITIONS, INDICATOR_DEFINITIONS,
  RETURNS_ANALYSIS_DEFINITIONS, ROLLING_METRIC_OPTIONS, INDICATOR_MODE_DEFINITIONS,
} from './backtestingStyles';

// ============================================================================
// Reusable Styles
// ============================================================================

const inputStyle: React.CSSProperties = {
  padding: '6px 10px', backgroundColor: '#111', color: F.WHITE,
  border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '11px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace', outline: 'none', width: '100%',
};

const labelStyle: React.CSSProperties = {
  fontSize: '10px', fontWeight: 600, color: F.GRAY, marginBottom: '4px', display: 'block',
};

const rowStyle: React.CSSProperties = {
  display: 'flex', alignItems: 'flex-end', gap: '12px',
  padding: '10px 14px', borderBottom: `1px solid ${F.BORDER}`,
  backgroundColor: F.PANEL_BG, flexWrap: 'wrap',
};

const checkboxLabelStyle: React.CSSProperties = {
  fontSize: '10px', color: F.GRAY, display: 'flex', alignItems: 'center', gap: '4px', cursor: 'pointer',
};

const runBtnStyle = (disabled: boolean): React.CSSProperties => ({
  padding: '6px 16px', fontSize: '11px', fontWeight: 700,
  fontFamily: '"IBM Plex Mono", monospace', cursor: disabled ? 'wait' : 'pointer',
  backgroundColor: disabled ? F.MUTED : F.ORANGE,
  color: F.DARK_BG, border: 'none', borderRadius: '2px',
  display: 'flex', alignItems: 'center', gap: '6px',
  opacity: disabled ? 0.6 : 1, flexShrink: 0,
});

// ============================================================================
// Reusable: Dynamic Param Inputs
// ============================================================================

function DynamicParams({ defs, values, onChange }: {
  defs: ParamDef[];
  values: Record<string, any>;
  onChange: (key: string, value: any) => void;
}) {
  if (defs.length === 0) return null;
  return (
    <div style={rowStyle}>
      {defs.map(p => (
        <div key={p.key} style={{ minWidth: '80px', maxWidth: '140px' }}>
          <label style={labelStyle}>{p.label}</label>
          {p.type === 'string' && p.options ? (
            <select value={values[p.key] ?? p.default} onChange={e => onChange(p.key, e.target.value)}
              style={{ ...inputStyle, cursor: 'pointer' }}>
              {p.options.map(o => <option key={o} value={o}>{o}</option>)}
            </select>
          ) : p.type === 'string' ? (
            <input type="text" value={values[p.key] ?? p.default}
              onChange={e => onChange(p.key, e.target.value)} style={inputStyle} />
          ) : (
            <input type="number" value={values[p.key] ?? p.default}
              min={p.min} max={p.max} step={p.step}
              onChange={e => onChange(p.key, Number(e.target.value))} style={inputStyle} />
          )}
        </div>
      ))}
    </div>
  );
}

// ============================================================================
// Main Component
// ============================================================================

const BacktestingTabNew: React.FC = () => {
  // --- Sub Tab ---
  const [activeSubTab, setActiveSubTab] = useState<BacktestingSubTab>('backtest');

  // --- Provider State ---
  const [activeProvider, setActiveProvider] = useState<string | null>(null);
  const [availableProviders, setAvailableProviders] = useState<string[]>([]);
  const [vbtAdapter] = useState(() => new VectorBTAdapter());

  // --- Shared Config State (Backtest + Optimize) ---
  const [config, setConfig] = useState<BacktestConfigExtended>({
    symbols: ['SPY'], weights: [1.0],
    startDate: new Date(Date.now() - 366 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    endDate: new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    initialCapital: 100000, strategyType: 'sma_crossover',
    parameters: { fastPeriod: 10, slowPeriod: 20 },
    commission: 0.001, slippage: 0.0005, leverage: 1, benchmark: 'SPY',
    positionSizing: 'fixed', positionSizeValue: 1.0,
    stopLoss: 0, takeProfit: 0, trailingStop: 0,
    allowShort: false, compareRandom: false,
  });
  const [symbolInput, setSymbolInput] = useState('SPY');

  // --- Optimize State ---
  const [optMethod, setOptMethod] = useState<'grid' | 'random'>('grid');
  const [optObjective, setOptObjective] = useState<OptimizationObjective>('sharpe');
  const [optMaxIter, setOptMaxIter] = useState(500);
  const [paramRanges, setParamRanges] = useState<Record<string, { min: number; max: number; step: number }>>({});
  const [wfEnabled, setWfEnabled] = useState(false);
  const [wfSplits, setWfSplits] = useState(5);
  const [wfTrainRatio, setWfTrainRatio] = useState(0.7);

  // --- Data Tab State ---
  const [dataSource, setDataSource] = useState<DataSourceType>('yfinance');
  const [dataTimeframe, setDataTimeframe] = useState('1d');
  const [dataExtraParams, setDataExtraParams] = useState<Record<string, any>>({});

  // --- Indicators State ---
  const [selectedIndicator, setSelectedIndicator] = useState('rsi');
  const [indicatorParams, setIndicatorParams] = useState<Record<string, any>>({});

  // --- Signals State ---
  const [signalGenerator, setSignalGenerator] = useState<SignalGeneratorType>('RAND');
  const [signalParams, setSignalParams] = useState<Record<string, any>>({});

  // --- Labels State ---
  const [labelGenerator, setLabelGenerator] = useState<LabelGeneratorType>('FIXLB');
  const [labelParams, setLabelParams] = useState<Record<string, any>>({});

  // --- Splitters State ---
  const [splitterType, setSplitterType] = useState<SplitterType>('RollingSplitter');
  const [splitterParams, setSplitterParams] = useState<Record<string, any>>({});
  const [rangeRows, setRangeRows] = useState<{ trainStart: string; trainEnd: string; testStart: string; testEnd: string }[]>([
    { trainStart: '2022-01-01', trainEnd: '2022-12-31', testStart: '2023-01-01', testEnd: '2023-06-30' },
  ]);

  // --- Returns State ---
  const [returnsAnalysisType, setReturnsAnalysisType] = useState<ReturnsAnalysisType>('returns_stats');
  const [returnsBenchmark, setReturnsBenchmark] = useState('');
  const [returnsParams, setReturnsParams] = useState<Record<string, any>>({});
  const [rollingMetric, setRollingMetric] = useState<RollingMetric>('sharpe');

  // --- Indicator Mode State ---
  const [indicatorMode, setIndicatorMode] = useState<IndicatorMode>('calculate');
  const [indicatorModeParams, setIndicatorModeParams] = useState<Record<string, any>>({});
  const [sweepRanges, setSweepRanges] = useState<Record<string, { min: number; max: number; step: number }>>({});

  // --- Labels-to-Signals State ---
  const [entryLabel, setEntryLabel] = useState(1);
  const [exitLabel, setExitLabel] = useState(-1);

  // --- Signals Clean State ---
  const [cleanSignals, setCleanSignals] = useState(false);

  // --- Data Extra Options ---
  const [alignIndex, setAlignIndex] = useState(false);
  const [timezone, setTimezone] = useState('none');

  // --- Result State ---
  const [isRunning, setIsRunning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const [rawResult, setRawResult] = useState<any>(null);

  // --- Provider Registration ---
  useEffect(() => {
    registerProviders();
  }, []);

  // Initialize param ranges when strategy changes
  useEffect(() => {
    const def = STRATEGY_DEFINITIONS.find(s => s.type === config.strategyType);
    if (def) {
      const ranges: Record<string, { min: number; max: number; step: number }> = {};
      for (const p of def.parameters) {
        ranges[p.key] = { min: p.min, max: p.max, step: p.step };
      }
      setParamRanges(ranges);
    }
  }, [config.strategyType]);

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

  const handleSymbolsChange = (value: string) => {
    setSymbolInput(value);
    const symbols = value.split(',').map(s => s.trim().toUpperCase()).filter(Boolean);
    const weights = symbols.map(() => 1.0 / (symbols.length || 1));
    setConfig(prev => ({ ...prev, symbols, weights }));
  };

  const handleStrategyChange = (strategyType: StrategyType) => {
    setConfig(prev => ({ ...prev, strategyType, parameters: getDefaultParameters(strategyType) }));
  };

  const handleStrategyParamChange = (key: string, value: number) => {
    setConfig(prev => ({ ...prev, parameters: { ...prev.parameters, [key]: value } }));
  };

  const runAsync = async (fn: () => Promise<any>) => {
    setIsRunning(true); setError(null); setRawResult(null);
    try {
      const result = await fn();
      setRawResult(result);
    } catch (err) {
      let errMsg = String(err).replace(/^Error:\s*/g, '');
      errMsg = errMsg.replace(/VectorBT.*?failed:\s*VectorBT.*?failed:\s*/g, 'VectorBT failed: ');
      setError(errMsg);
    } finally {
      setIsRunning(false);
    }
  };

  // ============================================================================
  // Tab: Backtest
  // ============================================================================

  const handleRunBacktest = useCallback(async () => {
    if (!activeProvider) { setError('No provider selected'); return; }
    if (config.symbols.length === 0) { setError('Add at least one symbol'); return; }
    const provider = backtestingRegistry.getProvider(activeProvider);
    if (!provider) { setError('Provider not found'); return; }

    const request: BacktestRequest = {
      strategy: { name: `${config.strategyType} - ${config.symbols.join(',')}`, type: config.strategyType, parameters: config.parameters },
      startDate: config.startDate, endDate: config.endDate, initialCapital: config.initialCapital,
      assets: config.symbols.map((sym, i) => ({ symbol: sym, assetClass: 'stocks' as const, weight: config.weights[i] || 1.0 / config.symbols.length })),
      commission: config.commission, slippage: config.slippage,
      tradeOnClose: false, hedging: false, exclusiveOrders: true,
      margin: config.leverage, leverage: config.leverage,
      benchmark: config.benchmark || undefined,
      stopLoss: config.stopLoss || undefined, takeProfit: config.takeProfit || undefined,
      trailingStop: config.trailingStop || undefined,
      positionSizing: config.positionSizing, positionSizeValue: config.positionSizeValue,
      allowShort: config.allowShort, compareRandom: config.compareRandom || undefined,
    };
    await runAsync(() => provider.runBacktest(request));
  }, [activeProvider, config]);

  const currentStrategyDef = STRATEGY_DEFINITIONS.find(s => s.type === config.strategyType);

  const renderBacktestTab = () => (
    <>
      {/* Row 1: Core params */}
      <div style={rowStyle}>
        {availableProviders.length > 1 && (
          <div style={{ minWidth: '130px' }}>
            <label style={labelStyle}>Provider</label>
            <select value={activeProvider || ''} onChange={e => { setActiveProvider(e.target.value); sqliteService.setActiveBacktestingProvider(e.target.value).catch(() => {}); }}
              style={{ ...inputStyle, cursor: 'pointer' }}>
              {availableProviders.map(p => <option key={p} value={p}>{p}</option>)}
            </select>
          </div>
        )}
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbols</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} placeholder="SPY, AAPL" style={inputStyle} />
        </div>
        <div style={{ minWidth: '150px' }}>
          <label style={labelStyle}>Strategy</label>
          <select value={config.strategyType} onChange={e => handleStrategyChange(e.target.value as StrategyType)} style={{ ...inputStyle, cursor: 'pointer' }}>
            {STRATEGY_DEFINITIONS.map(s => <option key={s.type} value={s.type}>{s.label}</option>)}
          </select>
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '90px' }}>
          <label style={labelStyle}>Capital</label>
          <input type="number" value={config.initialCapital} onChange={e => setConfig(prev => ({ ...prev, initialCapital: Number(e.target.value) || 100000 }))} style={inputStyle} />
        </div>
        <button onClick={handleRunBacktest} disabled={isRunning || !activeProvider} style={runBtnStyle(isRunning || !activeProvider)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          {isRunning ? 'RUNNING...' : 'RUN BACKTEST'}
        </button>
      </div>
      {/* Row 2: Dynamic strategy params */}
      {currentStrategyDef && currentStrategyDef.parameters.length > 0 && (
        <div style={rowStyle}>
          {currentStrategyDef.parameters.map(p => (
            <div key={p.key} style={{ minWidth: '80px', maxWidth: '120px' }}>
              <label style={labelStyle}>{p.label}</label>
              <input type="number" value={config.parameters[p.key] ?? p.default} min={p.min} max={p.max} step={p.step}
                onChange={e => handleStrategyParamChange(p.key, Number(e.target.value))} style={inputStyle} />
            </div>
          ))}
        </div>
      )}
      {/* Row 3: Portfolio / Execution Config */}
      <div style={rowStyle}>
        <div style={{ minWidth: '100px' }}>
          <label style={labelStyle}>Position Sizing</label>
          <select value={config.positionSizing} onChange={e => setConfig(prev => ({ ...prev, positionSizing: e.target.value as any }))} style={{ ...inputStyle, cursor: 'pointer' }}>
            <option value="fixed">Fixed</option><option value="kelly">Kelly</option>
            <option value="fixed_fractional">Fixed Frac</option><option value="vol_target">Vol Target</option>
          </select>
        </div>
        <div style={{ minWidth: '70px' }}>
          <label style={labelStyle}>Size Value</label>
          <input type="number" value={config.positionSizeValue} step={0.1} onChange={e => setConfig(prev => ({ ...prev, positionSizeValue: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '70px' }}>
          <label style={labelStyle}>Commission</label>
          <input type="number" value={config.commission} step={0.0001} onChange={e => setConfig(prev => ({ ...prev, commission: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '70px' }}>
          <label style={labelStyle}>Slippage</label>
          <input type="number" value={config.slippage} step={0.0001} onChange={e => setConfig(prev => ({ ...prev, slippage: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '60px' }}>
          <label style={labelStyle}>Leverage</label>
          <input type="number" value={config.leverage} min={1} step={1} onChange={e => setConfig(prev => ({ ...prev, leverage: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '70px' }}>
          <label style={labelStyle}>Stop Loss</label>
          <input type="number" value={config.stopLoss} min={0} step={0.01} onChange={e => setConfig(prev => ({ ...prev, stopLoss: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '70px' }}>
          <label style={labelStyle}>Take Profit</label>
          <input type="number" value={config.takeProfit} min={0} step={0.01} onChange={e => setConfig(prev => ({ ...prev, takeProfit: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '70px' }}>
          <label style={labelStyle}>Trail Stop</label>
          <input type="number" value={config.trailingStop} min={0} step={0.01} onChange={e => setConfig(prev => ({ ...prev, trailingStop: Number(e.target.value) }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '80px' }}>
          <label style={labelStyle}>Benchmark</label>
          <input type="text" value={config.benchmark} onChange={e => setConfig(prev => ({ ...prev, benchmark: e.target.value }))} style={inputStyle} />
        </div>
        <label style={checkboxLabelStyle}>
          <input type="checkbox" checked={config.allowShort} onChange={e => setConfig(prev => ({ ...prev, allowShort: e.target.checked }))} /> Short
        </label>
        <label style={checkboxLabelStyle}>
          <input type="checkbox" checked={config.compareRandom} onChange={e => setConfig(prev => ({ ...prev, compareRandom: e.target.checked }))} /> vs Random
        </label>
      </div>
    </>
  );

  // ============================================================================
  // Tab: Optimize
  // ============================================================================

  const handleRunOptimize = () => runAsync(async () => {
    return vbtAdapter.optimize({
      strategy: { type: config.strategyType, parameters: config.parameters },
      parameterGrid: Object.entries(paramRanges).map(([name, r]) => ({ name, min: r.min, max: r.max, step: r.step })),
      objective: optObjective as any,
      algorithm: optMethod,
      maxIterations: optMaxIter,
      startDate: config.startDate, endDate: config.endDate,
      initialCapital: config.initialCapital,
      assets: config.symbols.map((sym, i) => ({ symbol: sym, assetClass: 'stocks' as const, weight: config.weights[i] || 1.0 / config.symbols.length })),
      config: { startDate: config.startDate, endDate: config.endDate, initialCapital: config.initialCapital, assets: [] },
    } as any);
  });

  const handleRunWalkForward = () => runAsync(async () => {
    return vbtAdapter.walkForward({
      strategy: { type: config.strategyType, parameters: config.parameters },
      parameterRanges: paramRanges,
      objective: optObjective, nSplits: wfSplits, trainRatio: wfTrainRatio,
      startDate: config.startDate, endDate: config.endDate,
      initialCapital: config.initialCapital,
      assets: config.symbols.map((sym, i) => ({ symbol: sym, assetClass: 'stocks' as const, weight: config.weights[i] || 1.0 / config.symbols.length })),
    });
  });

  const renderOptimizeTab = () => (
    <>
      <div style={rowStyle}>
        <div style={{ minWidth: '100px' }}>
          <label style={labelStyle}>Method</label>
          <select value={optMethod} onChange={e => setOptMethod(e.target.value as any)} style={{ ...inputStyle, cursor: 'pointer' }}>
            <option value="grid">Grid Search</option><option value="random">Random Search</option>
          </select>
        </div>
        <div style={{ minWidth: '120px' }}>
          <label style={labelStyle}>Objective</label>
          <select value={optObjective} onChange={e => setOptObjective(e.target.value as OptimizationObjective)} style={{ ...inputStyle, cursor: 'pointer' }}>
            <option value="sharpe">Sharpe</option><option value="sortino">Sortino</option>
            <option value="return">Return</option><option value="calmar">Calmar</option>
            <option value="profit_factor">Profit Factor</option><option value="sqn">SQN</option>
          </select>
        </div>
        <div style={{ minWidth: '80px' }}>
          <label style={labelStyle}>Max Iter</label>
          <input type="number" value={optMaxIter} min={1} onChange={e => setOptMaxIter(Number(e.target.value))} style={inputStyle} />
        </div>
        <button onClick={handleRunOptimize} disabled={isRunning} style={runBtnStyle(isRunning)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          OPTIMIZE
        </button>
      </div>
      {/* Parameter ranges */}
      {currentStrategyDef && (
        <div style={rowStyle}>
          <span style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, textTransform: 'uppercase', width: '100%' }}>PARAMETER RANGES</span>
        </div>
      )}
      {currentStrategyDef && currentStrategyDef.parameters.map(p => (
        <div key={p.key} style={{ ...rowStyle, paddingTop: '6px', paddingBottom: '6px' }}>
          <span style={{ fontSize: '10px', color: F.WHITE, minWidth: '100px' }}>{p.label}</span>
          {['min', 'max', 'step'].map(field => (
            <div key={field} style={{ minWidth: '70px' }}>
              <label style={labelStyle}>{field.toUpperCase()}</label>
              <input type="number" value={(paramRanges[p.key] as any)?.[field] ?? (field === 'step' ? p.step : field === 'min' ? p.min : p.max)}
                onChange={e => setParamRanges(prev => ({ ...prev, [p.key]: { ...prev[p.key], [field]: Number(e.target.value) } }))}
                style={inputStyle} />
            </div>
          ))}
        </div>
      ))}
      {/* Walk-forward */}
      <div style={rowStyle}>
        <label style={checkboxLabelStyle}>
          <input type="checkbox" checked={wfEnabled} onChange={e => setWfEnabled(e.target.checked)} /> Walk-Forward
        </label>
        {wfEnabled && (
          <>
            <div style={{ minWidth: '70px' }}>
              <label style={labelStyle}>N Splits</label>
              <input type="number" value={wfSplits} min={2} onChange={e => setWfSplits(Number(e.target.value))} style={inputStyle} />
            </div>
            <div style={{ minWidth: '80px' }}>
              <label style={labelStyle}>Train Ratio</label>
              <input type="number" value={wfTrainRatio} min={0.1} max={0.95} step={0.05} onChange={e => setWfTrainRatio(Number(e.target.value))} style={inputStyle} />
            </div>
            <button onClick={handleRunWalkForward} disabled={isRunning} style={runBtnStyle(isRunning)}>
              {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
              WALK-FORWARD
            </button>
          </>
        )}
      </div>
    </>
  );

  // ============================================================================
  // Tab: Data
  // ============================================================================

  const handleDownloadData = () => runAsync(async () => {
    return vbtAdapter.downloadHistoricalData({
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      timeframe: dataTimeframe, source: dataSource,
      sourceConfig: { ...dataExtraParams, align_index: alignIndex, tz_convert: timezone !== 'none' ? timezone : undefined },
    });
  });

  const handleUpdateData = () => runAsync(async () => {
    return vbtAdapter.downloadHistoricalData({
      symbols: config.symbols, startDate: config.startDate, endDate: new Date().toISOString().split('T')[0],
      timeframe: dataTimeframe, source: dataSource,
      sourceConfig: { ...dataExtraParams, update: true, align_index: alignIndex, tz_convert: timezone !== 'none' ? timezone : undefined },
    });
  });

  const currentDataSource = DATA_SOURCE_DEFINITIONS[dataSource];

  const renderDataTab = () => (
    <>
      <div style={rowStyle}>
        <div style={{ minWidth: '130px' }}>
          <label style={labelStyle}>Data Source</label>
          <select value={dataSource} onChange={e => { setDataSource(e.target.value as DataSourceType); setDataExtraParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {Object.entries(DATA_SOURCE_DEFINITIONS).map(([k, v]) => <option key={k} value={k}>{v.label}</option>)}
          </select>
        </div>
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbols</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} placeholder="SPY, AAPL" style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '80px' }}>
          <label style={labelStyle}>Timeframe</label>
          <select value={dataTimeframe} onChange={e => setDataTimeframe(e.target.value)} style={{ ...inputStyle, cursor: 'pointer' }}>
            {['1m','5m','15m','1h','4h','1d','1w'].map(t => <option key={t} value={t}>{t}</option>)}
          </select>
        </div>
        <button onClick={handleDownloadData} disabled={isRunning} style={runBtnStyle(isRunning)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          DOWNLOAD
        </button>
        <button onClick={handleUpdateData} disabled={isRunning}
          style={{ ...runBtnStyle(isRunning), backgroundColor: isRunning ? F.MUTED : F.CYAN }}>
          UPDATE
        </button>
      </div>
      {currentDataSource.extraParams.length > 0 && (
        <DynamicParams defs={currentDataSource.extraParams} values={dataExtraParams}
          onChange={(k, v) => setDataExtraParams(prev => ({ ...prev, [k]: v }))} />
      )}
      {/* Data utilities row */}
      <div style={rowStyle}>
        <label style={checkboxLabelStyle}>
          <input type="checkbox" checked={alignIndex} onChange={e => setAlignIndex(e.target.checked)} /> Align Index
        </label>
        <div style={{ minWidth: '120px' }}>
          <label style={labelStyle}>Timezone</label>
          <select value={timezone} onChange={e => setTimezone(e.target.value)} style={{ ...inputStyle, cursor: 'pointer' }}>
            <option value="none">None</option>
            <option value="UTC">UTC</option>
            <option value="US/Eastern">US/Eastern</option>
            <option value="Europe/London">Europe/London</option>
            <option value="Asia/Tokyo">Asia/Tokyo</option>
          </select>
        </div>
      </div>
    </>
  );

  // ============================================================================
  // Tab: Indicators
  // ============================================================================

  const handleCalculateIndicator = () => runAsync(async () => {
    return vbtAdapter.calculateIndicator(selectedIndicator as any, {
      ...indicatorParams, symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
    });
  });

  const handleGetIndicatorCatalog = () => runAsync(async () => vbtAdapter.getAvailableIndicators());

  const handleIndicatorSignals = () => runAsync(async () => {
    return vbtAdapter.indicatorSignals({
      mode: indicatorMode,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      indicator: selectedIndicator,
      params: { ...indicatorParams, ...indicatorModeParams },
    });
  });

  const handleIndicatorParamSweep = () => runAsync(async () => {
    return vbtAdapter.indicatorParamSweep({
      indicator: selectedIndicator,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      paramRanges: sweepRanges,
    });
  });

  const currentIndicator = INDICATOR_DEFINITIONS.find(i => i.id === selectedIndicator);
  const currentIndMode = INDICATOR_MODE_DEFINITIONS[indicatorMode];

  const renderIndicatorsTab = () => (
    <>
      {/* Mode selector row */}
      <div style={rowStyle}>
        <div style={{ minWidth: '180px' }}>
          <label style={labelStyle}>Mode</label>
          <select value={indicatorMode} onChange={e => { setIndicatorMode(e.target.value as IndicatorMode); setIndicatorModeParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {(Object.entries(INDICATOR_MODE_DEFINITIONS) as [IndicatorMode, any][]).map(([k, v]) => (
              <option key={k} value={k}>{v.label}</option>
            ))}
          </select>
        </div>
        <div style={{ minWidth: '160px' }}>
          <label style={labelStyle}>Indicator</label>
          <select value={selectedIndicator} onChange={e => { setSelectedIndicator(e.target.value); setIndicatorParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {INDICATOR_DEFINITIONS.map(ind => <option key={ind.id} value={ind.id}>[{ind.category}] {ind.label}</option>)}
          </select>
        </div>
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbol</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        {indicatorMode === 'calculate' && (
          <>
            <button onClick={handleCalculateIndicator} disabled={isRunning} style={runBtnStyle(isRunning)}>
              {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
              CALCULATE
            </button>
            <button onClick={handleGetIndicatorCatalog} disabled={isRunning}
              style={{ ...runBtnStyle(isRunning), backgroundColor: isRunning ? F.MUTED : F.HEADER_BG, color: F.GRAY, border: `1px solid ${F.BORDER}` }}>
              CATALOG
            </button>
          </>
        )}
        {indicatorMode === 'param_sweep' && (
          <button onClick={handleIndicatorParamSweep} disabled={isRunning} style={runBtnStyle(isRunning)}>
            {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
            SWEEP
          </button>
        )}
        {indicatorMode !== 'calculate' && indicatorMode !== 'param_sweep' && (
          <button onClick={handleIndicatorSignals} disabled={isRunning} style={runBtnStyle(isRunning)}>
            {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
            GENERATE
          </button>
        )}
      </div>
      {/* Description */}
      {indicatorMode !== 'calculate' && (
        <div style={{ padding: '4px 14px', fontSize: '9px', color: F.MUTED, borderBottom: `1px solid ${F.BORDER}` }}>
          {currentIndMode.description}
        </div>
      )}
      {/* Indicator params (for calculate mode) */}
      {indicatorMode === 'calculate' && currentIndicator && currentIndicator.params.length > 0 && (
        <DynamicParams defs={currentIndicator.params} values={indicatorParams}
          onChange={(k, v) => setIndicatorParams(prev => ({ ...prev, [k]: v }))} />
      )}
      {/* Mode-specific params */}
      {indicatorMode !== 'calculate' && indicatorMode !== 'param_sweep' && currentIndMode.params.length > 0 && (
        <DynamicParams defs={currentIndMode.params} values={indicatorModeParams}
          onChange={(k, v) => setIndicatorModeParams(prev => ({ ...prev, [k]: v }))} />
      )}
      {/* Param sweep: ranges per indicator param */}
      {indicatorMode === 'param_sweep' && currentIndicator && currentIndicator.params.length > 0 && (
        <>
          <div style={rowStyle}>
            <span style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, textTransform: 'uppercase', width: '100%' }}>PARAMETER RANGES FOR SWEEP</span>
          </div>
          {currentIndicator.params.map(p => (
            <div key={p.key} style={{ ...rowStyle, paddingTop: '6px', paddingBottom: '6px' }}>
              <span style={{ fontSize: '10px', color: F.WHITE, minWidth: '100px' }}>{p.label}</span>
              {['min', 'max', 'step'].map(field => (
                <div key={field} style={{ minWidth: '70px' }}>
                  <label style={labelStyle}>{field.toUpperCase()}</label>
                  <input type="number" value={(sweepRanges[p.key] as any)?.[field] ?? (field === 'step' ? (p.step || 1) : field === 'min' ? (p.min || 5) : (p.max || 50))}
                    onChange={e => setSweepRanges(prev => ({ ...prev, [p.key]: { ...prev[p.key], [field]: Number(e.target.value) } }))}
                    style={inputStyle} />
                </div>
              ))}
            </div>
          ))}
        </>
      )}
    </>
  );

  // ============================================================================
  // Tab: Signals
  // ============================================================================

  const handleGenerateSignals = () => runAsync(async () => {
    return vbtAdapter.generateSignals({
      generatorType: signalGenerator,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      params: { ...signalParams, clean: cleanSignals },
    });
  });

  const currentSignalDef = SIGNAL_GENERATOR_DEFINITIONS[signalGenerator];

  const renderSignalsTab = () => (
    <>
      <div style={rowStyle}>
        <div style={{ minWidth: '180px' }}>
          <label style={labelStyle}>Generator</label>
          <select value={signalGenerator} onChange={e => { setSignalGenerator(e.target.value as SignalGeneratorType); setSignalParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {(Object.entries(SIGNAL_GENERATOR_DEFINITIONS) as [SignalGeneratorType, any][]).map(([k, v]) => (
              <option key={k} value={k}>{v.label}</option>
            ))}
          </select>
        </div>
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbol</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        <button onClick={handleGenerateSignals} disabled={isRunning} style={runBtnStyle(isRunning)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          GENERATE
        </button>
        <label style={checkboxLabelStyle}>
          <input type="checkbox" checked={cleanSignals} onChange={e => setCleanSignals(e.target.checked)} /> Clean (remove overlaps)
        </label>
      </div>
      <div style={{ padding: '4px 14px', fontSize: '9px', color: F.MUTED, borderBottom: `1px solid ${F.BORDER}` }}>
        {currentSignalDef.description}
      </div>
      {currentSignalDef.params.length > 0 && (
        <DynamicParams defs={currentSignalDef.params} values={signalParams}
          onChange={(k, v) => setSignalParams(prev => ({ ...prev, [k]: v }))} />
      )}
    </>
  );

  // ============================================================================
  // Tab: Labels
  // ============================================================================

  const handleGenerateLabels = () => runAsync(async () => {
    return vbtAdapter.generateLabels({
      labelType: labelGenerator,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      params: labelParams,
    });
  });

  const handleLabelsToSignals = () => runAsync(async () => {
    return vbtAdapter.labelsToSignals({
      labelType: labelGenerator,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      params: labelParams,
      entryLabel, exitLabel,
    });
  });

  const currentLabelDef = LABEL_GENERATOR_DEFINITIONS[labelGenerator];

  const renderLabelsTab = () => (
    <>
      <div style={rowStyle}>
        <div style={{ minWidth: '160px' }}>
          <label style={labelStyle}>Label Type</label>
          <select value={labelGenerator} onChange={e => { setLabelGenerator(e.target.value as LabelGeneratorType); setLabelParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {(Object.entries(LABEL_GENERATOR_DEFINITIONS) as [LabelGeneratorType, any][]).map(([k, v]) => (
              <option key={k} value={k}>{v.label}</option>
            ))}
          </select>
        </div>
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbol</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        <button onClick={handleGenerateLabels} disabled={isRunning} style={runBtnStyle(isRunning)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          GENERATE
        </button>
        <button onClick={handleLabelsToSignals} disabled={isRunning}
          style={{ ...runBtnStyle(isRunning), backgroundColor: isRunning ? F.MUTED : F.CYAN }}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          TO SIGNALS
        </button>
      </div>
      <div style={{ padding: '4px 14px', fontSize: '9px', color: F.MUTED, borderBottom: `1px solid ${F.BORDER}` }}>
        {currentLabelDef.description}
      </div>
      {currentLabelDef.params.length > 0 && (
        <DynamicParams defs={currentLabelDef.params} values={labelParams}
          onChange={(k, v) => setLabelParams(prev => ({ ...prev, [k]: v }))} />
      )}
      {/* Labels-to-Signals config */}
      <div style={rowStyle}>
        <span style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, textTransform: 'uppercase' }}>LABELS TO SIGNALS CONFIG</span>
        <div style={{ minWidth: '80px' }}>
          <label style={labelStyle}>Entry Label</label>
          <input type="number" value={entryLabel} onChange={e => setEntryLabel(Number(e.target.value))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '80px' }}>
          <label style={labelStyle}>Exit Label</label>
          <input type="number" value={exitLabel} onChange={e => setExitLabel(Number(e.target.value))} style={inputStyle} />
        </div>
      </div>
    </>
  );

  // ============================================================================
  // Tab: Splitters
  // ============================================================================

  const handleGenerateSplits = () => runAsync(async () => {
    const params = splitterType === 'RangeSplitter'
      ? { ...splitterParams, ranges: rangeRows }
      : splitterParams;
    return vbtAdapter.generateSplits({
      splitterType,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      params,
    });
  });

  const currentSplitterDef = SPLITTER_DEFINITIONS[splitterType];

  const renderSplittersTab = () => (
    <>
      <div style={rowStyle}>
        <div style={{ minWidth: '170px' }}>
          <label style={labelStyle}>Splitter</label>
          <select value={splitterType} onChange={e => { setSplitterType(e.target.value as SplitterType); setSplitterParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {(Object.entries(SPLITTER_DEFINITIONS) as [SplitterType, any][]).map(([k, v]) => (
              <option key={k} value={k}>{v.label}</option>
            ))}
          </select>
        </div>
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbol</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} placeholder="SPY" style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        <button onClick={handleGenerateSplits} disabled={isRunning} style={runBtnStyle(isRunning)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          SPLIT
        </button>
      </div>
      <div style={{ padding: '4px 14px', fontSize: '9px', color: F.MUTED, borderBottom: `1px solid ${F.BORDER}` }}>
        {currentSplitterDef.description}
      </div>
      {currentSplitterDef.params.length > 0 && (
        <DynamicParams defs={currentSplitterDef.params} values={splitterParams}
          onChange={(k, v) => setSplitterParams(prev => ({ ...prev, [k]: v }))} />
      )}
      {/* RangeSplitter: manual date range rows */}
      {splitterType === 'RangeSplitter' && (
        <>
          <div style={rowStyle}>
            <span style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, textTransform: 'uppercase', flex: 1 }}>DATE RANGES</span>
            <button onClick={() => setRangeRows(prev => [...prev, { trainStart: '2023-01-01', trainEnd: '2023-06-30', testStart: '2023-07-01', testEnd: '2023-12-31' }])}
              style={{ ...runBtnStyle(false), backgroundColor: F.HEADER_BG, color: F.GRAY, border: `1px solid ${F.BORDER}`, padding: '4px 10px' }}>
              <Plus size={10} /> ADD
            </button>
          </div>
          {rangeRows.map((row, idx) => (
            <div key={idx} style={{ ...rowStyle, paddingTop: '4px', paddingBottom: '4px' }}>
              <span style={{ fontSize: '9px', color: F.MUTED, minWidth: '30px' }}>#{idx + 1}</span>
              <div style={{ minWidth: '100px' }}>
                <label style={labelStyle}>Train Start</label>
                <input type="date" value={row.trainStart} onChange={e => setRangeRows(prev => prev.map((r, i) => i === idx ? { ...r, trainStart: e.target.value } : r))} style={inputStyle} />
              </div>
              <div style={{ minWidth: '100px' }}>
                <label style={labelStyle}>Train End</label>
                <input type="date" value={row.trainEnd} onChange={e => setRangeRows(prev => prev.map((r, i) => i === idx ? { ...r, trainEnd: e.target.value } : r))} style={inputStyle} />
              </div>
              <div style={{ minWidth: '100px' }}>
                <label style={labelStyle}>Test Start</label>
                <input type="date" value={row.testStart} onChange={e => setRangeRows(prev => prev.map((r, i) => i === idx ? { ...r, testStart: e.target.value } : r))} style={inputStyle} />
              </div>
              <div style={{ minWidth: '100px' }}>
                <label style={labelStyle}>Test End</label>
                <input type="date" value={row.testEnd} onChange={e => setRangeRows(prev => prev.map((r, i) => i === idx ? { ...r, testEnd: e.target.value } : r))} style={inputStyle} />
              </div>
              {rangeRows.length > 1 && (
                <button onClick={() => setRangeRows(prev => prev.filter((_, i) => i !== idx))}
                  style={{ background: 'none', border: 'none', cursor: 'pointer', color: F.RED, padding: '2px' }}>
                  <Trash2 size={12} />
                </button>
              )}
            </div>
          ))}
        </>
      )}
    </>
  );

  // ============================================================================
  // Tab: Catalog
  // ============================================================================

  const handleGetStrategyCatalog = () => runAsync(async () => vbtAdapter.getStrategyCatalog());
  const handleGetIndicatorCatalogFull = () => runAsync(async () => vbtAdapter.getAvailableIndicators());

  const renderCatalogTab = () => (
    <div style={rowStyle}>
      <button onClick={handleGetStrategyCatalog} disabled={isRunning} style={runBtnStyle(isRunning)}>
        {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
        GET STRATEGY CATALOG
      </button>
      <button onClick={handleGetIndicatorCatalogFull} disabled={isRunning}
        style={{ ...runBtnStyle(isRunning), backgroundColor: isRunning ? F.MUTED : F.CYAN }}>
        {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
        GET INDICATOR CATALOG
      </button>
    </div>
  );

  // ============================================================================
  // Tab: Returns (vbt_returns + vbt_generic)
  // ============================================================================

  const handleAnalyzeReturns = () => runAsync(async () => {
    return vbtAdapter.analyzeReturns({
      analysisType: returnsAnalysisType,
      symbols: config.symbols, startDate: config.startDate, endDate: config.endDate,
      benchmark: returnsBenchmark || undefined,
      params: {
        ...returnsParams,
        ...(returnsAnalysisType === 'rolling' ? { metric: rollingMetric } : {}),
      },
    });
  });

  const currentReturnsAnalysis = RETURNS_ANALYSIS_DEFINITIONS[returnsAnalysisType];

  const renderReturnsTab = () => (
    <>
      {/* Row 1: Core params */}
      <div style={rowStyle}>
        <div style={{ minWidth: '140px' }}>
          <label style={labelStyle}>Analysis Type</label>
          <select value={returnsAnalysisType} onChange={e => { setReturnsAnalysisType(e.target.value as ReturnsAnalysisType); setReturnsParams({}); }}
            style={{ ...inputStyle, cursor: 'pointer' }}>
            {(Object.entries(RETURNS_ANALYSIS_DEFINITIONS) as [ReturnsAnalysisType, any][]).map(([k, v]) => (
              <option key={k} value={k}>{v.label}</option>
            ))}
          </select>
        </div>
        <div style={{ minWidth: '120px', flex: 1, maxWidth: '200px' }}>
          <label style={labelStyle}>Symbol</label>
          <input type="text" value={symbolInput} onChange={e => handleSymbolsChange(e.target.value)} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>Start Date</label>
          <input type="date" value={config.startDate} onChange={e => setConfig(prev => ({ ...prev, startDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '110px' }}>
          <label style={labelStyle}>End Date</label>
          <input type="date" value={config.endDate} onChange={e => setConfig(prev => ({ ...prev, endDate: e.target.value }))} style={inputStyle} />
        </div>
        <div style={{ minWidth: '90px' }}>
          <label style={labelStyle}>Benchmark</label>
          <input type="text" value={returnsBenchmark} onChange={e => setReturnsBenchmark(e.target.value)} placeholder="SPY" style={inputStyle} />
        </div>
        <button onClick={handleAnalyzeReturns} disabled={isRunning} style={runBtnStyle(isRunning)}>
          {isRunning ? <Loader size={12} className="animate-spin" /> : <Play size={12} />}
          ANALYZE
        </button>
      </div>
      {/* Description */}
      <div style={{ padding: '4px 14px', fontSize: '9px', color: F.MUTED, borderBottom: `1px solid ${F.BORDER}` }}>
        {currentReturnsAnalysis.description}
      </div>
      {/* Dynamic params per analysis type */}
      {currentReturnsAnalysis.params.length > 0 && (
        <DynamicParams defs={currentReturnsAnalysis.params} values={returnsParams}
          onChange={(k, v) => setReturnsParams(prev => ({ ...prev, [k]: v }))} />
      )}
      {/* Rolling metric selector */}
      {returnsAnalysisType === 'rolling' && (
        <div style={rowStyle}>
          <div style={{ minWidth: '200px' }}>
            <label style={labelStyle}>Rolling Metric</label>
            <select value={rollingMetric} onChange={e => setRollingMetric(e.target.value as RollingMetric)}
              style={{ ...inputStyle, cursor: 'pointer' }}>
              {ROLLING_METRIC_OPTIONS.map(opt => <option key={opt.value} value={opt.value}>{opt.label}</option>)}
            </select>
          </div>
        </div>
      )}
    </>
  );

  // ============================================================================
  // Tab Renderers Map
  // ============================================================================

  const tabRenderers: Record<BacktestingSubTab, () => React.ReactNode> = {
    backtest: renderBacktestTab,
    optimize: renderOptimizeTab,
    data: renderDataTab,
    indicators: renderIndicatorsTab,
    signals: renderSignalsTab,
    labels: renderLabelsTab,
    splitters: renderSplittersTab,
    catalog: renderCatalogTab,
    returns: renderReturnsTab,
  };

  const SUB_TABS: { key: BacktestingSubTab; label: string }[] = [
    { key: 'backtest', label: 'BACKTEST' },
    { key: 'optimize', label: 'OPTIMIZE' },
    { key: 'data', label: 'DATA' },
    { key: 'indicators', label: 'INDICATORS' },
    { key: 'signals', label: 'SIGNALS' },
    { key: 'labels', label: 'LABELS' },
    { key: 'splitters', label: 'SPLITTERS' },
    { key: 'catalog', label: 'CATALOG' },
    { key: 'returns', label: 'RETURNS' },
  ];

  // ============================================================================
  // Render
  // ============================================================================

  return (
    <div style={{
      display: 'flex', flexDirection: 'column', height: '100%',
      backgroundColor: F.DARK_BG, fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>
      {/* Header */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '8px',
        padding: '8px 14px', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG,
      }}>
        <Activity size={14} color={F.ORANGE} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
          BACKTESTING
        </span>
        {activeProvider && (
          <span style={{ fontSize: '9px', color: F.GRAY, marginLeft: '4px' }}>[{activeProvider}]</span>
        )}
      </div>

      {/* Sub-Tab Bar */}
      <div style={{
        display: 'flex', gap: '0px', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG, overflowX: 'auto',
      }}>
        {SUB_TABS.map(tab => (
          <button key={tab.key} onClick={() => { setActiveSubTab(tab.key); setError(null); setRawResult(null); }}
            style={{
              padding: '8px 14px', fontSize: '10px', fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace', textTransform: 'uppercase',
              letterSpacing: '0.5px', border: 'none', cursor: 'pointer',
              color: activeSubTab === tab.key ? F.ORANGE : F.GRAY,
              backgroundColor: activeSubTab === tab.key ? F.HEADER_BG : 'transparent',
              borderBottom: activeSubTab === tab.key ? `2px solid ${F.ORANGE}` : '2px solid transparent',
              transition: 'color 0.15s',
            }}>
            {tab.label}
          </button>
        ))}
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

      {/* Active Tab Controls */}
      <div style={{ flexShrink: 0, overflowY: 'auto', maxHeight: '45%' }}>
        {tabRenderers[activeSubTab]()}
      </div>

      {/* Result Display */}
      <div style={{ flex: 1, overflow: 'auto', padding: '14px', backgroundColor: F.DARK_BG, minHeight: 0 }}>
        {!rawResult && !isRunning && !error && (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '12px' }}>
            Configure parameters and run an operation.
          </div>
        )}
        {isRunning && (
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.ORANGE, fontSize: '12px', gap: '8px' }}>
            <Loader size={16} className="animate-spin" /> Running...
          </div>
        )}
        {rawResult && !isRunning && (
          <pre style={{
            margin: 0, padding: 0, color: F.WHITE, fontSize: '11px', lineHeight: '1.5',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            whiteSpace: 'pre-wrap', wordBreak: 'break-word',
          }}>
            {JSON.stringify(rawResult, null, 2)}
          </pre>
        )}
      </div>
    </div>
  );
};

export default BacktestingTabNew;
