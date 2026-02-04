/**
 * Backtesting Tab
 *
 * Multi-provider backtesting with clear separation:
 * - VECTORBT: Full 13-command coverage (backtest, optimize, walk-forward, data, indicators, signals, labels, splits, returns, browse, convert)
 * - BACKTESTING.PY: 8-command coverage (backtest, optimize, walk-forward, data, indicators, signals, browse)
 * - FAST-TRADE: Focused backtest-only provider
 *
 * Each provider uses its own strategies, commands, and command mappings with zero mixing.
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  Activity, Play, Loader, TrendingUp, Settings, BarChart3, Database, Zap, Target, Tag, Split,
  DollarSign, AlertCircle, CheckCircle, XCircle, ChevronRight, Save, RefreshCw, TrendingDown,
  Calendar, Clock, Percent, Hash, Code, Download, Upload, Trash2, Copy, Info, LineChart,
  PieChart, BarChart, Maximize2, Minimize2, Eye, EyeOff, Filter, Search, X, Plus
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import {
  PROVIDER_CONFIGS,
  PROVIDER_OPTIONS,
  ALL_COMMANDS,
  type BacktestingProviderSlug,
  type CommandType,
} from './backtestingProviderConfigs';

// ============================================================================
// FINCEPT DESIGN SYSTEM
// ============================================================================

const FINCEPT = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F', HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A', HOVER: '#1F1F1F', MUTED: '#4A4A4A', CYAN: '#00E5FF',
  YELLOW: '#FFD700', BLUE: '#0088FF', PURPLE: '#9D4EDD',
};

// Provider accent colors for visual separation
const PROVIDER_COLORS: Record<BacktestingProviderSlug, string> = {
  fincept: FINCEPT.ORANGE,
  vectorbt: FINCEPT.CYAN,
  backtestingpy: FINCEPT.GREEN,
  fasttrade: FINCEPT.YELLOW,
  zipline: '#E91E63',
  bt: '#FF6B35',
};

// ============================================================================
// DATA SOURCE OPTIONS
// ============================================================================

const DATA_SOURCES = [
  { id: 'yfinance', label: 'Yahoo Finance', supported: true },
  { id: 'binance', label: 'Binance', supported: true },
  { id: 'ccxt', label: 'CCXT', supported: true },
  { id: 'alpaca', label: 'Alpaca', supported: true },
  { id: 'synthetic', label: 'Synthetic (GBM)', supported: true },
];

const TIMEFRAMES = [
  { id: '1m', label: '1 Minute' },
  { id: '5m', label: '5 Minutes' },
  { id: '15m', label: '15 Minutes' },
  { id: '1h', label: '1 Hour' },
  { id: '4h', label: '4 Hours' },
  { id: '1d', label: '1 Day' },
  { id: '1w', label: '1 Week' },
];

// ============================================================================
// INDICATOR OPTIONS
// ============================================================================

const INDICATORS = [
  { id: 'ma', label: 'Moving Average', params: ['period', 'ewm'] },
  { id: 'mstd', label: 'Moving Std Dev', params: ['period'] },
  { id: 'bbands', label: 'Bollinger Bands', params: ['period', 'alpha'] },
  { id: 'rsi', label: 'RSI', params: ['period'] },
  { id: 'stoch', label: 'Stochastic', params: ['kPeriod', 'dPeriod'] },
  { id: 'macd', label: 'MACD', params: ['fast', 'slow', 'signal'] },
  { id: 'atr', label: 'ATR', params: ['period'] },
  { id: 'obv', label: 'OBV', params: [] },
  { id: 'adx', label: 'ADX', params: ['period'] },
  { id: 'donchian', label: 'Donchian', params: ['period'] },
  { id: 'zscore', label: 'Z-Score', params: ['window'] },
  { id: 'momentum', label: 'Momentum', params: ['period'] },
  { id: 'williams_r', label: 'Williams %R', params: ['period'] },
  { id: 'cci', label: 'CCI', params: ['period'] },
  { id: 'keltner', label: 'Keltner', params: ['period', 'atrMult'] },
];

// ============================================================================
// SIGNAL GENERATORS
// ============================================================================

const SIGNAL_GENERATORS = [
  { id: 'RAND', label: 'Random', params: ['n', 'seed'] },
  { id: 'RANDX', label: 'Random Entry/Exit', params: ['n', 'seed'] },
  { id: 'RANDNX', label: 'Random N Entries', params: ['n', 'seed'] },
  { id: 'RPROB', label: 'Random Probability', params: ['prob', 'seed'] },
  { id: 'RPROBX', label: 'Random Prob Entry/Exit', params: ['entryProb', 'exitProb', 'seed'] },
  { id: 'STX', label: 'Stop/Take-Profit', params: ['stopLoss', 'takeProfit'] },
  { id: 'STCX', label: 'Trailing Stop', params: ['stopLoss', 'trailing'] },
  { id: 'OHLCSTX', label: 'OHLC Stop/TP', params: ['stopLoss', 'takeProfit'] },
];

// ============================================================================
// LABEL GENERATORS
// ============================================================================

const LABEL_GENERATORS = [
  { id: 'FIXLB', label: 'Fixed Horizon', params: ['horizon', 'threshold'] },
  { id: 'MEANLB', label: 'Mean Reversion', params: ['window', 'threshold'] },
  { id: 'LEXLB', label: 'Local Extrema', params: ['window'] },
  { id: 'TRENDLB', label: 'Trend', params: ['window', 'threshold'] },
  { id: 'BOLB', label: 'Bollinger', params: ['window', 'alpha'] },
];

// ============================================================================
// SPLITTERS
// ============================================================================

const SPLITTERS = [
  { id: 'range', label: 'Range Splitter', params: [] },
  { id: 'rolling', label: 'Rolling Window', params: ['windowLen', 'testLen', 'step'] },
  { id: 'expanding', label: 'Expanding Window', params: ['minLen', 'testLen', 'step'] },
  { id: 'purged_kfold', label: 'Purged K-Fold', params: ['nSplits', 'purgeLen', 'embargoLen'] },
];

// ============================================================================
// POSITION SIZING OPTIONS
// ============================================================================

const POSITION_SIZING = [
  { id: 'fixed', label: 'Fixed Size', params: ['size'] },
  { id: 'percent', label: 'Percent of Capital', params: ['percent'] },
  { id: 'kelly', label: 'Kelly Criterion', params: [] },
  { id: 'volatility', label: 'Volatility Target', params: ['targetVol'] },
  { id: 'risk', label: 'Risk-Based', params: ['riskPercent'] },
];

// ============================================================================
// MAIN COMPONENT
// ============================================================================

const BacktestingTab: React.FC = () => {
  // --- Provider State ---
  const [selectedProvider, setSelectedProvider] = useState<BacktestingProviderSlug>('vectorbt');

  // --- Core State ---
  const [activeCommand, setActiveCommand] = useState<CommandType>('backtest');
  const [selectedCategory, setSelectedCategory] = useState<string>('trend');
  const [selectedStrategy, setSelectedStrategy] = useState<string>('');
  const [strategyParams, setStrategyParams] = useState<Record<string, any>>({});
  const [customCode, setCustomCode] = useState<string>('# Custom strategy code\n');

  // --- Backtest Config ---
  const [symbols, setSymbols] = useState('SPY');
  const [startDate, setStartDate] = useState(new Date(Date.now() - 366 * 24 * 60 * 60 * 1000).toISOString().split('T')[0]);
  const [endDate, setEndDate] = useState(new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString().split('T')[0]);
  const [initialCapital, setInitialCapital] = useState(100000);
  const [commission, setCommission] = useState(0.001);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    selectedProvider, activeCommand, selectedCategory, selectedStrategy, symbols, startDate, endDate, initialCapital, commission,
  }), [selectedProvider, activeCommand, selectedCategory, selectedStrategy, symbols, startDate, endDate, initialCapital, commission]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.selectedProvider === "string" && state.selectedProvider in PROVIDER_CONFIGS) setSelectedProvider(state.selectedProvider as BacktestingProviderSlug);
    if (typeof state.activeCommand === "string") setActiveCommand(state.activeCommand as CommandType);
    if (typeof state.selectedCategory === "string") setSelectedCategory(state.selectedCategory);
    if (typeof state.selectedStrategy === "string") setSelectedStrategy(state.selectedStrategy);
    if (typeof state.symbols === "string") setSymbols(state.symbols);
    if (typeof state.startDate === "string") setStartDate(state.startDate);
    if (typeof state.endDate === "string") setEndDate(state.endDate);
    if (typeof state.initialCapital === "number") setInitialCapital(state.initialCapital);
    if (typeof state.commission === "number") setCommission(state.commission);
  }, []);

  useWorkspaceTabState("backtesting", getWorkspaceState, setWorkspaceState);

  // --- Derived provider config (single source of truth) ---
  const providerConfig = useMemo(() => PROVIDER_CONFIGS[selectedProvider], [selectedProvider]);
  const providerColor = PROVIDER_COLORS[selectedProvider];
  const providerCommands = useMemo(() =>
    ALL_COMMANDS.filter(cmd => providerConfig.commands.includes(cmd.id)),
    [providerConfig]
  );
  const [providerStrategies, setProviderStrategies] = useState(providerConfig.strategies);
  const providerCategoryInfo = providerConfig.categoryInfo;

  // --- Load Fincept strategies dynamically ---
  useEffect(() => {
    if (selectedProvider === 'fincept') {
      invoke<string>('list_strategies')
        .then((response) => {
          const parsed = JSON.parse(response);
          if (parsed.success && parsed.data) {
            // Group strategies by category
            const grouped: Record<string, any[]> = {};
            parsed.data.forEach((strategy: any) => {
              const category = strategy.category.toLowerCase();
              if (!grouped[category]) {
                grouped[category] = [];
              }
              grouped[category].push({
                id: strategy.id,
                name: strategy.name,
                description: `${strategy.category} strategy`,
                params: [] // Fincept strategies don't have UI params
              });
            });
            setProviderStrategies(grouped);
          }
        })
        .catch((err) => console.error('Failed to load Fincept strategies:', err));
    } else {
      setProviderStrategies(providerConfig.strategies);
    }
  }, [selectedProvider, providerConfig.strategies]);

  // Reset command/strategy when switching providers
  useEffect(() => {
    // Reset to first available command
    const firstCmd = providerConfig.commands[0] || 'backtest';
    setActiveCommand(firstCmd);
    // Reset to first available category
    const categories = Object.keys(providerStrategies);
    const firstCategory = categories[0] || 'trend';
    setSelectedCategory(firstCategory);
    // Reset to first strategy in that category
    const firstStrategies = providerStrategies[firstCategory];
    if (firstStrategies && firstStrategies.length > 0) {
      setSelectedStrategy(firstStrategies[0].id);
    }
    // Clear results on provider switch
    setResult(null);
    setError(null);
  }, [selectedProvider, providerStrategies]); // eslint-disable-line react-hooks/exhaustive-deps
  const [slippage, setSlippage] = useState(0.0005);

  // --- Advanced Config ---
  const [leverage, setLeverage] = useState(1.0);
  const [margin, setMargin] = useState(1.0);
  const [stopLoss, setStopLoss] = useState<number | null>(null);
  const [takeProfit, setTakeProfit] = useState<number | null>(null);
  const [trailingStop, setTrailingStop] = useState<number | null>(null);
  const [positionSizing, setPositionSizing] = useState<string>('percent');
  const [positionSizeValue, setPositionSizeValue] = useState(1.0);
  const [allowShort, setAllowShort] = useState(false);
  const [benchmarkSymbol, setBenchmarkSymbol] = useState('SPY');
  const [enableBenchmark, setEnableBenchmark] = useState(false);
  const [randomBenchmark, setRandomBenchmark] = useState(false);

  // --- Optimize Config ---
  const [optimizeObjective, setOptimizeObjective] = useState<'sharpe' | 'sortino' | 'calmar' | 'return'>('sharpe');
  const [optimizeMethod, setOptimizeMethod] = useState<'grid' | 'random'>('grid');
  const [maxIterations, setMaxIterations] = useState(500);
  const [paramRanges, setParamRanges] = useState<Record<string, { min: number; max: number; step: number }>>({});

  // --- Walk Forward Config ---
  const [wfSplits, setWfSplits] = useState(5);
  const [wfTrainRatio, setWfTrainRatio] = useState(0.7);

  // --- Data Config ---
  const [dataSource, setDataSource] = useState('yfinance');
  const [timeframe, setTimeframe] = useState('1d');

  // --- Indicator Config ---
  const [selectedIndicator, setSelectedIndicator] = useState('ma');
  const [indicatorParams, setIndicatorParams] = useState<Record<string, any>>({});

  // --- Signal Config ---
  const [selectedSignalGen, setSelectedSignalGen] = useState('RAND');
  const [signalParams, setSignalParams] = useState<Record<string, any>>({});

  // --- Label Config ---
  const [selectedLabelGen, setSelectedLabelGen] = useState('FIXLB');
  const [labelParams, setLabelParams] = useState<Record<string, any>>({});

  // --- Split Config ---
  const [selectedSplitter, setSelectedSplitter] = useState('rolling');
  const [splitParams, setSplitParams] = useState<Record<string, any>>({});

  // --- Execution State ---
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);
  const [resultView, setResultView] = useState<'summary' | 'metrics' | 'trades' | 'charts' | 'raw'>('summary');

  // --- UI State ---
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [expandedSections, setExpandedSections] = useState<Record<string, boolean>>({
    market: true,
    strategy: true,
    execution: false,
    advanced: false,
  });

  // Get current strategy from the active provider's strategies only
  const currentStrategy = useMemo(() => {
    const categoryStrategies = providerStrategies[selectedCategory] || [];
    return categoryStrategies.find(s => s.id === selectedStrategy) || categoryStrategies[0];
  }, [providerStrategies, selectedCategory, selectedStrategy]);

  // Initialize strategy params when strategy changes
  useEffect(() => {
    if (currentStrategy) {
      const defaults: Record<string, any> = {};
      currentStrategy.params.forEach(param => {
        defaults[param.name] = param.default;
      });
      setStrategyParams(defaults);
    }
  }, [currentStrategy]);

  // ============================================================================
  // HANDLERS
  // ============================================================================

  const handleRunCommand = useCallback(async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);

    try {
      const symbolList = symbols.split(',').map(s => s.trim().toUpperCase()).filter(Boolean);

      // Python will fetch market data directly via yfinance - no need to pre-fetch
      let commandArgs: any = {
        symbols: symbolList,
        startDate,
        endDate,
        initialCapital,
      };

      // Use provider-specific command mapping (no cross-provider mixing)
      const pythonCommand = providerConfig.commandMap[activeCommand] || activeCommand;

      // Build request based on active command
      switch (activeCommand) {
        case 'backtest':
          commandArgs = {
            strategy: {
              type: selectedStrategy === 'code' ? 'code' : selectedStrategy,
              name: currentStrategy?.name || selectedStrategy,
              parameters: selectedStrategy === 'code' ? { code: customCode } : strategyParams,
            },
            startDate,
            endDate,
            initialCapital,
            assets: symbolList.map(sym => ({
              symbol: sym,
              assetClass: 'stocks',
              weight: 1.0 / symbolList.length,
            })),
            commission,
            slippage,
            leverage,
            margin,
            stopLoss,
            takeProfit,
            trailingStop,
            positionSizing,
            positionSizeValue,
            allowShort,
            benchmarkSymbol: enableBenchmark ? benchmarkSymbol : null,
            randomBenchmark,
          };
          break;

        case 'optimize':
          commandArgs = {
            strategy: {
              type: selectedStrategy,
              name: currentStrategy?.name || selectedStrategy,
            },
            startDate,
            endDate,
            initialCapital,
            assets: symbolList.map(sym => ({
              symbol: sym,
              assetClass: 'stocks',
              weight: 1.0 / symbolList.length,
            })),
            parameters: paramRanges,
            objective: optimizeObjective,
            method: optimizeMethod,
            maxIterations,
          };
          break;

        case 'walk_forward':
          commandArgs = {
            strategy: {
              type: selectedStrategy,
              name: currentStrategy?.name || selectedStrategy,
              parameters: strategyParams,
            },
            startDate,
            endDate,
            initialCapital,
            assets: symbolList.map(sym => ({
              symbol: sym,
              assetClass: 'stocks',
              weight: 1.0 / symbolList.length,
            })),
            nSplits: wfSplits,
            trainRatio: wfTrainRatio,
          };
          break;

        case 'data':
          commandArgs = {
            symbols: symbolList,
            dataSource,
            timeframe,
            startDate,
            endDate,
          };
          break;

        case 'indicator':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            indicator: selectedIndicator,
            parameters: indicatorParams,
          };
          break;

        case 'indicator_signals':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            indicator: selectedIndicator,
            parameters: indicatorParams,
          };
          break;

        case 'signals':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            generator: selectedSignalGen,
            parameters: signalParams,
          };
          break;

        case 'labels':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            generator: selectedLabelGen,
            parameters: labelParams,
          };
          break;

        case 'splits':
          commandArgs = {
            startDate,
            endDate,
            splitter: selectedSplitter,
            parameters: splitParams,
          };
          break;

        case 'returns':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
          };
          break;

        case 'browse_strategies':
          commandArgs = {};
          break;

        case 'browse_indicators':
          commandArgs = {};
          break;

        case 'labels_to_signals':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            labelType: selectedLabelGen,
            params: labelParams,
            entryLabel: 1,
            exitLabel: -1,
          };
          break;
      }

      // Execute Python command via Tauri
      const argsJson = JSON.stringify(commandArgs);
      console.log('[BACKTEST] === SENDING TO PYTHON ===');
      console.log('[BACKTEST] Command:', pythonCommand);
      console.log('[BACKTEST] Args JSON length:', argsJson.length, 'bytes');
      console.log('[BACKTEST] Args object:', JSON.stringify(commandArgs, null, 2));

      const response = await invoke<string>('execute_python_backtest', {
        provider: selectedProvider,
        command: pythonCommand,
        args: argsJson,
      });

      console.log('[BACKTEST] === RAW RESPONSE FROM RUST ===');
      console.log('[BACKTEST] Response type:', typeof response);
      console.log('[BACKTEST] Response length:', response?.length ?? 0);
      console.log('[BACKTEST] Response (first 2000 chars):', response?.substring(0, 2000));

      const parsed = JSON.parse(response);
      console.log('[BACKTEST] === PARSED RESULT ===');
      console.log('[BACKTEST] Top-level keys:', Object.keys(parsed));
      console.log('[BACKTEST] success:', parsed.success);
      console.log('[BACKTEST] error:', parsed.error);
      if (parsed.data) {
        console.log('[BACKTEST] data keys:', Object.keys(parsed.data));
        if (parsed.data.performance) {
          console.log('[BACKTEST] performance keys:', Object.keys(parsed.data.performance));
          console.log('[BACKTEST] performance:', JSON.stringify(parsed.data.performance));
        }
        if (parsed.data.trades) {
          console.log('[BACKTEST] trades count:', parsed.data.trades?.length);
          if (parsed.data.trades.length > 0) {
            console.log('[BACKTEST] first trade:', JSON.stringify(parsed.data.trades[0]));
          }
        }
        if (parsed.data.equity) {
          console.log('[BACKTEST] equity points:', parsed.data.equity?.length);
        }
        if (parsed.data.logs) {
          console.log('[BACKTEST] Python logs:', parsed.data.logs);
        }
      }
      setResult(parsed);
    } catch (err: any) {
      setError(err.toString());
    } finally {
      setIsRunning(false);
    }
  }, [
    selectedProvider, providerConfig, activeCommand, symbols, startDate, endDate, initialCapital,
    selectedStrategy, strategyParams, customCode, commission, slippage, leverage, margin,
    stopLoss, takeProfit, trailingStop, positionSizing, positionSizeValue, allowShort,
    benchmarkSymbol, enableBenchmark, randomBenchmark, optimizeObjective, optimizeMethod,
    maxIterations, paramRanges, wfSplits, wfTrainRatio, dataSource, timeframe,
    selectedIndicator, indicatorParams, selectedSignalGen, signalParams,
    selectedLabelGen, labelParams, selectedSplitter, splitParams
  ]);

  const toggleSection = (section: string) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  // ============================================================================
  // RENDER: Top Navigation
  // ============================================================================

  const renderTopNav = () => (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `2px solid ${providerColor}`,
      padding: '8px 16px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      boxShadow: `0 2px 8px ${providerColor}20`,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <Activity size={16} color={providerColor} />
        <span style={{
          fontSize: '12px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          fontFamily: '"IBM Plex Mono", monospace',
          letterSpacing: '0.5px',
        }}>
          BACKTESTING
        </span>
        <div style={{ display: 'flex', gap: '2px', marginLeft: '4px' }}>
          {PROVIDER_OPTIONS.map(p => (
            <button
              key={p.slug}
              onClick={() => setSelectedProvider(p.slug)}
              style={{
                padding: '4px 10px',
                backgroundColor: selectedProvider === p.slug ? PROVIDER_COLORS[p.slug] : 'transparent',
                color: selectedProvider === p.slug ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${selectedProvider === p.slug ? PROVIDER_COLORS[p.slug] : FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={e => {
                if (selectedProvider !== p.slug) {
                  e.currentTarget.style.borderColor = PROVIDER_COLORS[p.slug];
                  e.currentTarget.style.color = PROVIDER_COLORS[p.slug];
                }
              }}
              onMouseLeave={e => {
                if (selectedProvider !== p.slug) {
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              {p.displayName}
            </button>
          ))}
        </div>
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <button
          onClick={() => setShowAdvanced(!showAdvanced)}
          style={{
            padding: '4px 8px',
            backgroundColor: showAdvanced ? `${FINCEPT.ORANGE}20` : 'transparent',
            border: `1px solid ${showAdvanced ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
            color: showAdvanced ? FINCEPT.ORANGE : FINCEPT.GRAY,
            borderRadius: '2px',
            fontSize: '8px',
            fontWeight: 700,
            cursor: 'pointer',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          {showAdvanced ? 'HIDE' : 'SHOW'} ADVANCED
        </button>
        <div style={{
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          color: FINCEPT.GREEN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
        }}>
          ACTIVE
        </div>
      </div>
    </div>
  );

  // ============================================================================
  // RENDER: Left Panel - Commands
  // ============================================================================

  const renderLeftPanel = () => (
    <div style={{
      width: '280px',
      backgroundColor: FINCEPT.DARK_BG,
      borderRight: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Commands Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <span style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GRAY,
          letterSpacing: '0.5px',
          fontFamily: '"IBM Plex Mono", monospace',
        }}>
          COMMANDS ({providerCommands.length})
        </span>
      </div>

      {/* Command List (provider-specific) */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {providerCommands.map(cmd => {
          const Icon = cmd.icon;
          const isActive = activeCommand === cmd.id;

          return (
            <div
              key={cmd.id}
              onClick={() => setActiveCommand(cmd.id)}
              style={{
                padding: '10px 12px',
                backgroundColor: isActive ? `${cmd.color}15` : 'transparent',
                borderLeft: isActive ? `2px solid ${cmd.color}` : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
              }}
              onMouseEnter={e => {
                if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              }}
              onMouseLeave={e => {
                if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
              }}
            >
              <Icon size={14} color={isActive ? cmd.color : FINCEPT.GRAY} />
              <div style={{ flex: 1 }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                  fontFamily: '"IBM Plex Mono", monospace',
                }}>
                  {cmd.label}
                </div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
                  {cmd.description}
                </div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Strategy Categories (provider-specific) */}
      {['backtest', 'optimize', 'walk_forward'].includes(activeCommand) && (
        <>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              STRATEGY CATEGORIES
            </span>
          </div>
          <div style={{ overflowY: 'auto', maxHeight: '300px' }}>
            {Object.entries(providerCategoryInfo).map(([key, info]) => {
              const Icon = info.icon;
              const isActive = selectedCategory === key;
              const count = providerStrategies[key]?.length || 0;

              return (
                <div
                  key={key}
                  onClick={() => setSelectedCategory(key)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: isActive ? `${info.color}15` : 'transparent',
                    borderLeft: isActive ? `2px solid ${info.color}` : '2px solid transparent',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                  }}
                  onMouseEnter={e => {
                    if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={e => {
                    if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                  }}
                >
                  <Icon size={12} color={isActive ? info.color : FINCEPT.GRAY} />
                  <span style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                    fontFamily: '"IBM Plex Mono", monospace',
                    flex: 1,
                  }}>
                    {info.label.toUpperCase()}
                  </span>
                  <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
                    {count}
                  </span>
                </div>
              );
            })}
          </div>
        </>
      )}
    </div>
  );

  // ============================================================================
  // RENDER: Center Panel - Configuration
  // ============================================================================

  const renderCenterPanel = () => (
    <div style={{
      flex: 1,
      backgroundColor: FINCEPT.DARK_BG,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <span style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GRAY,
          letterSpacing: '0.5px',
          fontFamily: '"IBM Plex Mono", monospace',
        }}>
          {activeCommand.toUpperCase()} CONFIGURATION
        </span>
        <button
          onClick={handleRunCommand}
          disabled={isRunning}
          style={{
            padding: '6px 12px',
            backgroundColor: isRunning ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: isRunning ? 'wait' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            opacity: isRunning ? 0.7 : 1,
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          {isRunning ? (
            <>
              <Loader size={10} className="animate-spin" />
              RUNNING...
            </>
          ) : (
            <>
              <Play size={10} />
              RUN {activeCommand.toUpperCase()}
            </>
          )}
        </button>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {activeCommand === 'backtest' && renderBacktestConfig()}
        {activeCommand === 'optimize' && renderOptimizeConfig()}
        {activeCommand === 'walk_forward' && renderWalkForwardConfig()}
        {activeCommand === 'data' && renderDataConfig()}
        {activeCommand === 'indicator' && renderIndicatorConfig()}
        {activeCommand === 'indicator_signals' && renderIndicatorSignalsConfig()}
        {activeCommand === 'signals' && renderSignalsConfig()}
        {activeCommand === 'labels' && renderLabelsConfig()}
        {activeCommand === 'splits' && renderSplitsConfig()}
        {activeCommand === 'returns' && renderReturnsConfig()}
        {activeCommand === 'browse_strategies' && renderBrowseStrategiesConfig()}
        {activeCommand === 'browse_indicators' && renderBrowseIndicatorsConfig()}
        {activeCommand === 'labels_to_signals' && renderLabelsToSignalsConfig()}
      </div>
    </div>
  );

  // ============================================================================
  // RENDER: Backtest Configuration (COMPLETE)
  // ============================================================================

  const renderBacktestConfig = () => {
    const categoryStrategies = providerStrategies[selectedCategory] || [];

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Market Data */}
        {renderSection('market', 'MARKET DATA', Database, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {renderInput('Symbols', symbols, setSymbols, 'text', 'SPY, AAPL, MSFT')}
            {renderInput('Initial Capital', initialCapital, setInitialCapital, 'number')}
            {renderInput('Start Date', startDate, setStartDate, 'date')}
            {renderInput('End Date', endDate, setEndDate, 'date')}
          </div>
        ))}

        {/* Strategy Selection (provider-specific) */}
        {renderSection('strategy', `${providerCategoryInfo[selectedCategory]?.label.toUpperCase() || 'STRATEGIES'}`, Activity, (
          <>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px', marginBottom: '16px' }}>
              {categoryStrategies.map(strategy => (
                <button
                  key={strategy.id}
                  onClick={() => setSelectedStrategy(strategy.id)}
                  style={{
                    padding: '10px 12px',
                    backgroundColor: selectedStrategy === strategy.id ? FINCEPT.ORANGE : 'transparent',
                    color: selectedStrategy === strategy.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    textAlign: 'left',
                    transition: 'all 0.2s',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                  onMouseEnter={e => {
                    if (selectedStrategy !== strategy.id) {
                      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                      e.currentTarget.style.color = FINCEPT.WHITE;
                    }
                  }}
                  onMouseLeave={e => {
                    if (selectedStrategy !== strategy.id) {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      e.currentTarget.style.color = FINCEPT.GRAY;
                    }
                  }}
                >
                  <div>{strategy.name}</div>
                  <div style={{ fontSize: '7px', color: FINCEPT.MUTED, marginTop: '2px' }}>
                    {strategy.description}
                  </div>
                </button>
              ))}
            </div>

            {/* Strategy Parameters or Custom Code */}
            {selectedStrategy === 'code' ? (
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '8px',
                  letterSpacing: '0.5px',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}>
                  CUSTOM PYTHON CODE
                </label>
                <textarea
                  value={customCode}
                  onChange={e => setCustomCode(e.target.value)}
                  rows={12}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.CYAN,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    outline: 'none',
                    resize: 'vertical',
                  }}
                  onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
                  onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
                />
              </div>
            ) : currentStrategy && currentStrategy.params.length > 0 && (
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
                {currentStrategy.params.map(param => (
                  <div key={param.name}>
                    {renderInput(
                      param.label,
                      strategyParams[param.name] ?? param.default,
                      (val) => setStrategyParams(prev => ({ ...prev, [param.name]: val })),
                      'number',
                      undefined,
                      param.min,
                      param.max,
                      param.step
                    )}
                  </div>
                ))}
              </div>
            )}
          </>
        ))}

        {/* Execution Settings */}
        {renderSection('execution', 'EXECUTION SETTINGS', Settings, (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
            {renderInput('Commission (%)', commission * 100, (v) => setCommission(v / 100), 'number', undefined, 0, 5, 0.01)}
            {renderInput('Slippage (%)', slippage * 100, (v) => setSlippage(v / 100), 'number', undefined, 0, 5, 0.01)}
          </div>
        ))}

        {/* Advanced Settings (ALL missing features) */}
        {showAdvanced && renderSection('advanced', 'ADVANCED SETTINGS', Target, (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            {/* Risk Management */}
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                RISK MANAGEMENT
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
                {renderInput('Stop Loss (%)', stopLoss ?? '', (v) => setStopLoss(v || null), 'number', 'Optional', 0, 100, 0.1)}
                {renderInput('Take Profit (%)', takeProfit ?? '', (v) => setTakeProfit(v || null), 'number', 'Optional', 0, 100, 0.1)}
                {renderInput('Trailing Stop (%)', trailingStop ?? '', (v) => setTrailingStop(v || null), 'number', 'Optional', 0, 100, 0.1)}
              </div>
            </div>

            {/* Position Sizing */}
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                POSITION SIZING
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                <div>
                  <label style={{
                    display: 'block',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    METHOD
                  </label>
                  <select
                    value={positionSizing}
                    onChange={e => setPositionSizing(e.target.value)}
                    style={{
                      width: '100%',
                      padding: '8px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      color: FINCEPT.WHITE,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '10px',
                      fontFamily: '"IBM Plex Mono", monospace',
                      outline: 'none',
                    }}
                  >
                    {POSITION_SIZING.map(ps => (
                      <option key={ps.id} value={ps.id}>{ps.label}</option>
                    ))}
                  </select>
                </div>
                {renderInput('Size Value', positionSizeValue, setPositionSizeValue, 'number', undefined, 0, 100)}
              </div>
            </div>

            {/* Leverage & Margin */}
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                LEVERAGE & MARGIN
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
                {renderInput('Leverage', leverage, setLeverage, 'number', undefined, 1, 10, 0.1)}
                {renderInput('Margin', margin, setMargin, 'number', undefined, 0, 1, 0.1)}
                <div>
                  <label style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    cursor: 'pointer',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    <input
                      type="checkbox"
                      checked={allowShort}
                      onChange={e => setAllowShort(e.target.checked)}
                      style={{ cursor: 'pointer' }}
                    />
                    ALLOW SHORT
                  </label>
                </div>
              </div>
            </div>

            {/* Benchmark */}
            <div>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                BENCHMARK COMPARISON
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 2fr', gap: '12px' }}>
                <div>
                  <label style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    cursor: 'pointer',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    <input
                      type="checkbox"
                      checked={enableBenchmark}
                      onChange={e => setEnableBenchmark(e.target.checked)}
                      style={{ cursor: 'pointer' }}
                    />
                    ENABLE
                  </label>
                </div>
                {enableBenchmark && (
                  <>
                    {renderInput('Benchmark Symbol', benchmarkSymbol, setBenchmarkSymbol, 'text', 'SPY')}
                    <div>
                      <label style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                        fontSize: '9px',
                        fontWeight: 700,
                        color: FINCEPT.GRAY,
                        cursor: 'pointer',
                        fontFamily: '"IBM Plex Mono", monospace',
                      }}>
                        <input
                          type="checkbox"
                          checked={randomBenchmark}
                          onChange={e => setRandomBenchmark(e.target.checked)}
                          style={{ cursor: 'pointer' }}
                        />
                        RANDOM BENCHMARK
                      </label>
                    </div>
                  </>
                )}
              </div>
            </div>
          </div>
        ))}
      </div>
    );
  };

  // ============================================================================
  // RENDER: Optimize Configuration (COMPLETE)
  // ============================================================================

  const renderOptimizeConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('market', 'MARKET DATA', Database, (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text', 'SPY, AAPL')}
          {renderInput('Initial Capital', initialCapital, setInitialCapital, 'number')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>
      ))}

      {renderSection('strategy', 'STRATEGY', Target, (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
          {(providerStrategies[selectedCategory] || []).map(strategy => (
            <button
              key={strategy.id}
              onClick={() => setSelectedStrategy(strategy.id)}
              style={{
                padding: '8px 12px',
                backgroundColor: selectedStrategy === strategy.id ? FINCEPT.GREEN : 'transparent',
                color: selectedStrategy === strategy.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                textAlign: 'left',
                transition: 'all 0.2s',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              {strategy.name}
            </button>
          ))}
        </div>
      ))}

      {renderSection('optimization', 'OPTIMIZATION SETTINGS', Target, (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                OBJECTIVE
              </label>
              <select
                value={optimizeObjective}
                onChange={e => setOptimizeObjective(e.target.value as any)}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  outline: 'none',
                }}
              >
                <option value="sharpe">Sharpe Ratio</option>
                <option value="sortino">Sortino Ratio</option>
                <option value="calmar">Calmar Ratio</option>
                <option value="return">Total Return</option>
              </select>
            </div>
            <div>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                METHOD
              </label>
              <select
                value={optimizeMethod}
                onChange={e => setOptimizeMethod(e.target.value as any)}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  outline: 'none',
                }}
              >
                <option value="grid">Grid Search</option>
                <option value="random">Random Search</option>
              </select>
            </div>
            {renderInput('Max Iterations', maxIterations, setMaxIterations, 'number', undefined, 10, 10000)}
          </div>

          {/* Parameter Ranges */}
          {currentStrategy && currentStrategy.params.length > 0 && (
            <div style={{ marginTop: '12px' }}>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.CYAN,
                marginBottom: '8px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}>
                PARAMETER RANGES
              </div>
              {currentStrategy.params.map(param => (
                <div key={param.name} style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px', marginBottom: '8px' }}>
                  <div style={{ fontSize: '9px', color: FINCEPT.GRAY, paddingTop: '8px' }}>
                    {param.label}
                  </div>
                  {renderInput('Min', paramRanges[param.name]?.min ?? param.min ?? 0, (v) => setParamRanges(prev => ({ ...prev, [param.name]: { ...prev[param.name], min: v } })), 'number')}
                  {renderInput('Max', paramRanges[param.name]?.max ?? param.max ?? 100, (v) => setParamRanges(prev => ({ ...prev, [param.name]: { ...prev[param.name], max: v } })), 'number')}
                  {renderInput('Step', paramRanges[param.name]?.step ?? param.step ?? 1, (v) => setParamRanges(prev => ({ ...prev, [param.name]: { ...prev[param.name], step: v } })), 'number')}
                </div>
              ))}
            </div>
          )}
        </div>
      ))}
    </div>
  );

  // ============================================================================
  // RENDER: Other Command Configurations (COMPLETE)
  // ============================================================================

  const renderWalkForwardConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('market', 'MARKET DATA', Database, (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text')}
          {renderInput('Initial Capital', initialCapital, setInitialCapital, 'number')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>
      ))}

      {renderSection('walkforward', 'WALK-FORWARD SETTINGS', ChevronRight, (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {renderInput('Number of Splits', wfSplits, setWfSplits, 'number', undefined, 2, 20)}
          {renderInput('Train Ratio', wfTrainRatio, setWfTrainRatio, 'number', undefined, 0.5, 0.9, 0.05)}
        </div>
      ))}
    </div>
  );

  const renderDataConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('datasource', 'DATA SOURCE', Database, (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div>
            <label style={{
              display: 'block',
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '4px',
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              SOURCE
            </label>
            <select
              value={dataSource}
              onChange={e => setDataSource(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
            >
              {DATA_SOURCES.map(ds => (
                <option key={ds.id} value={ds.id} disabled={!ds.supported}>
                  {ds.label}
                </option>
              ))}
            </select>
          </div>
          <div>
            <label style={{
              display: 'block',
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '4px',
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              TIMEFRAME
            </label>
            <select
              value={timeframe}
              onChange={e => setTimeframe(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
            >
              {TIMEFRAMES.map(tf => (
                <option key={tf.id} value={tf.id}>{tf.label}</option>
              ))}
            </select>
          </div>
          {renderInput('Symbols', symbols, setSymbols, 'text')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>
      ))}
    </div>
  );

  const renderIndicatorConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('indicator', 'INDICATOR SELECTION', Activity, (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
            {INDICATORS.map(ind => (
              <button
                key={ind.id}
                onClick={() => setSelectedIndicator(ind.id)}
                style={{
                  padding: '8px',
                  backgroundColor: selectedIndicator === ind.id ? FINCEPT.PURPLE : 'transparent',
                  color: selectedIndicator === ind.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                {ind.label}
              </button>
            ))}
          </div>
          {/* Show indicator-specific params */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginTop: '12px' }}>
            {INDICATORS.find(i => i.id === selectedIndicator)?.params.map(param => (
              <div key={param}>{renderInput(param.toUpperCase(), indicatorParams[param] ?? 14, (v) => setIndicatorParams(prev => ({ ...prev, [param]: v })), 'number')}</div>
            ))}
          </div>
        </>
      ))}
    </div>
  );

  const renderIndicatorSignalsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderIndicatorConfig()}
    </div>
  );

  const renderSignalsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('signals', 'SIGNAL GENERATOR', Zap, (
        <>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
            {SIGNAL_GENERATORS.map(gen => (
              <button
                key={gen.id}
                onClick={() => setSelectedSignalGen(gen.id)}
                style={{
                  padding: '8px',
                  backgroundColor: selectedSignalGen === gen.id ? FINCEPT.YELLOW : 'transparent',
                  color: selectedSignalGen === gen.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                {gen.label}
              </button>
            ))}
          </div>
        </>
      ))}
    </div>
  );

  const renderLabelsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('labels', 'LABEL GENERATOR', Tag, (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
          {LABEL_GENERATORS.map(gen => (
            <button
              key={gen.id}
              onClick={() => setSelectedLabelGen(gen.id)}
              style={{
                padding: '8px',
                backgroundColor: selectedLabelGen === gen.id ? FINCEPT.BLUE : 'transparent',
                color: selectedLabelGen === gen.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              {gen.label}
            </button>
          ))}
        </div>
      ))}
    </div>
  );

  const renderSplitsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('splits', 'CROSS-VALIDATION SPLITTER', Split, (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
          {SPLITTERS.map(splitter => (
            <button
              key={splitter.id}
              onClick={() => setSelectedSplitter(splitter.id)}
              style={{
                padding: '8px',
                backgroundColor: selectedSplitter === splitter.id ? FINCEPT.PURPLE : 'transparent',
                color: selectedSplitter === splitter.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              {splitter.label}
            </button>
          ))}
        </div>
      ))}
    </div>
  );

  const renderReturnsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('returns', 'RETURNS ANALYSIS', DollarSign, (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>
      ))}
    </div>
  );

  const renderBrowseStrategiesConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('browse_strategies', 'BROWSE STRATEGIES', Search, (
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, lineHeight: 1.6 }}>
          <p style={{ marginBottom: '8px' }}>
            Click <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>RUN</span> to fetch the complete strategy catalog from vbt_strategies module.
          </p>
          <p style={{ color: FINCEPT.MUTED }}>
            Returns all available trading strategies with their parameters, descriptions, and usage examples.
          </p>
        </div>
      ))}
    </div>
  );

  const renderBrowseIndicatorsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('browse_indicators', 'BROWSE INDICATORS', Filter, (
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, lineHeight: 1.6 }}>
          <p style={{ marginBottom: '8px' }}>
            Click <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>RUN</span> to fetch the complete indicator catalog from vbt_indicators module.
          </p>
          <p style={{ color: FINCEPT.MUTED }}>
            Returns all available technical indicators with their parameters, calculation methods, and signal generation capabilities.
          </p>
        </div>
      ))}
    </div>
  );

  const renderLabelsToSignalsConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('labels_to_signals', 'LABELS TO SIGNALS CONVERSION', ChevronRight, (
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, lineHeight: 1.6 }}>
          <p style={{ marginBottom: '8px', color: FINCEPT.CYAN }}>
            Converts ML labels into trading signals (entry/exit).
          </p>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {renderInput('Symbols', symbols, setSymbols, 'text')}
            {renderInput('Start Date', startDate, setStartDate, 'date')}
            {renderInput('End Date', endDate, setEndDate, 'date')}
            {renderSelect('Label Type', selectedLabelGen, setSelectedLabelGen, LABEL_GENERATORS.map(g => ({ value: g.id, label: g.label })))}
          </div>
          <div style={{ marginTop: '12px', padding: '8px', backgroundColor: FINCEPT.DARK_BG, borderRadius: '2px' }}>
            <p style={{ fontSize: '8px', color: FINCEPT.MUTED, marginBottom: '4px' }}>
              Entry Label: <span style={{ color: FINCEPT.GREEN }}>1</span> (buy signal)
            </p>
            <p style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
              Exit Label: <span style={{ color: FINCEPT.RED }}>-1</span> (sell signal)
            </p>
          </div>
        </div>
      ))}
    </div>
  );

  // ============================================================================
  // RENDER: Helper Functions
  // ============================================================================

  const renderSection = (
    id: string,
    title: string,
    icon: React.ElementType,
    content: React.ReactNode
  ) => {
    const Icon = icon;
    const isExpanded = expandedSections[id] ?? true;

    return (
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
      }}>
        <div
          onClick={() => toggleSection(id)}
          style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: isExpanded ? `1px solid ${FINCEPT.BORDER}` : 'none',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Icon size={12} color={FINCEPT.ORANGE} />
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {title}
            </span>
          </div>
          {isExpanded ? <Minimize2 size={10} color={FINCEPT.GRAY} /> : <Maximize2 size={10} color={FINCEPT.GRAY} />}
        </div>
        {isExpanded && (
          <div style={{ padding: '12px' }}>
            {content}
          </div>
        )}
      </div>
    );
  };

  const renderInput = (
    label: string,
    value: any,
    onChange: (value: any) => void,
    type: 'text' | 'number' | 'date' = 'text',
    placeholder?: string,
    min?: number,
    max?: number,
    step?: number
  ) => (
    <div>
      <label style={{
        display: 'block',
        fontSize: '9px',
        fontWeight: 700,
        color: FINCEPT.GRAY,
        marginBottom: '4px',
        letterSpacing: '0.5px',
        fontFamily: '"IBM Plex Mono", monospace',
      }}>
        {label.toUpperCase()}
      </label>
      <input
        type={type}
        value={value}
        onChange={e => onChange(type === 'number' ? Number(e.target.value) : e.target.value)}
        placeholder={placeholder}
        min={min}
        max={max}
        step={step}
        style={{
          width: '100%',
          padding: '8px 10px',
          backgroundColor: FINCEPT.DARK_BG,
          color: type === 'number' ? FINCEPT.CYAN : FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          fontSize: '10px',
          fontFamily: '"IBM Plex Mono", monospace',
          outline: 'none',
        }}
        onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
        onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
      />
    </div>
  );

  const renderSelect = (
    label: string,
    value: string,
    onChange: (value: string) => void,
    options: { value: string; label: string }[]
  ) => (
    <div>
      <label style={{
        display: 'block',
        fontSize: '9px',
        fontWeight: 700,
        color: FINCEPT.GRAY,
        marginBottom: '4px',
        letterSpacing: '0.5px',
        fontFamily: '"IBM Plex Mono", monospace',
      }}>
        {label.toUpperCase()}
      </label>
      <select
        value={value}
        onChange={e => onChange(e.target.value)}
        style={{
          width: '100%',
          padding: '8px 10px',
          backgroundColor: FINCEPT.DARK_BG,
          color: FINCEPT.WHITE,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          fontSize: '10px',
          fontFamily: '"IBM Plex Mono", monospace',
          outline: 'none',
          cursor: 'pointer',
        }}
      >
        {options.map(opt => (
          <option key={opt.value} value={opt.value}>{opt.label}</option>
        ))}
      </select>
    </div>
  );

  // ============================================================================
  // RENDER: Right Panel - Results (with formatted display)
  // ============================================================================

  const renderRightPanel = () => (
    <div style={{
      width: '350px',
      backgroundColor: FINCEPT.DARK_BG,
      borderLeft: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <span style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.GRAY,
          letterSpacing: '0.5px',
          fontFamily: '"IBM Plex Mono", monospace',
        }}>
          RESULTS
        </span>
        {result && (
          <div style={{ display: 'flex', gap: '4px' }}>
            {(['summary', 'metrics', 'trades', 'raw'] as const).map(view => (
              <button
                key={view}
                onClick={() => setResultView(view)}
                style={{
                  padding: '4px 6px',
                  backgroundColor: resultView === view ? FINCEPT.ORANGE : 'transparent',
                  color: resultView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '7px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                {view.toUpperCase()}
              </button>
            ))}
          </div>
        )}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
        {error && (
          <div style={{
            padding: '12px',
            backgroundColor: `${FINCEPT.RED}20`,
            border: `1px solid ${FINCEPT.RED}`,
            borderRadius: '2px',
          }}>
            <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
              <AlertCircle size={14} color={FINCEPT.RED} style={{ marginTop: '2px' }} />
              <div>
                <div style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.RED,
                  marginBottom: '4px',
                }}>
                  ERROR
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.WHITE, lineHeight: 1.4 }}>
                  {error}
                </div>
              </div>
            </div>
          </div>
        )}

        {isRunning && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            padding: '40px 20px',
            gap: '12px',
          }}>
            <Loader size={24} color={FINCEPT.ORANGE} className="animate-spin" />
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              Running {activeCommand}...
            </span>
          </div>
        )}

        {result && !isRunning && (
          <div>
            {resultView === 'summary' && renderResultSummary()}
            {resultView === 'metrics' && renderResultMetrics()}
            {resultView === 'trades' && renderResultTrades()}
            {resultView === 'raw' && renderResultRaw()}
          </div>
        )}

        {!result && !isRunning && !error && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
            gap: '8px',
          }}>
            <BarChart3 size={24} style={{ opacity: 0.5 }} />
            <span>No results yet</span>
            <span style={{ fontSize: '9px' }}>Configure and run</span>
          </div>
        )}
      </div>
    </div>
  );

  const renderResultSummary = () => {
    if (!result?.data?.performance) return null;
    const perf = result.data.performance;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <div style={{
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          color: FINCEPT.GREEN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          display: 'inline-block',
        }}>
          COMPLETED
        </div>
        {renderMetric('Total Return', `${((perf.totalReturn ?? 0) * 100).toFixed(2)}%`, (perf.totalReturn ?? 0) > 0 ? FINCEPT.GREEN : FINCEPT.RED)}
        {renderMetric('Sharpe Ratio', (perf.sharpeRatio ?? 0).toFixed(3), FINCEPT.CYAN)}
        {renderMetric('Max Drawdown', `${((perf.maxDrawdown ?? 0) * 100).toFixed(2)}%`, FINCEPT.RED)}
        {renderMetric('Win Rate', `${((perf.winRate ?? 0) * 100).toFixed(1)}%`, FINCEPT.CYAN)}
        {renderMetric('Total Trades', (perf.totalTrades ?? 0).toString(), FINCEPT.GRAY)}
      </div>
    );
  };

  const renderResultMetrics = () => {
    if (!result?.data?.performance) return null;
    const perf = result.data.performance;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: '8px' }}>
        {Object.entries(perf).map(([key, value]) => (
          <div key={key} style={{ display: 'flex', justifyContent: 'space-between', borderBottom: `1px solid ${FINCEPT.BORDER}`, paddingBottom: '4px' }}>
            <span style={{ color: FINCEPT.GRAY }}>{key.replace(/([A-Z])/g, ' $1').toUpperCase()}</span>
            <span style={{ color: FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>
              {typeof value === 'number' ? value.toFixed(4) : String(value ?? 'N/A')}
            </span>
          </div>
        ))}
      </div>
    );
  };

  const renderResultTrades = () => {
    if (!result?.data?.trades) return null;
    const trades = result.data.trades;

    return (
      <div style={{ fontSize: '7px' }}>
        {trades.slice(0, 20).map((trade: any, i: number) => (
          <div key={i} style={{
            padding: '6px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            marginBottom: '4px',
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
              <span style={{ color: FINCEPT.GRAY }}>#{trade.id}</span>
              <span style={{ color: trade.pnl > 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
                {trade.pnl?.toFixed(2) || 'N/A'}
              </span>
            </div>
            <div style={{ color: FINCEPT.MUTED }}>
              {trade.symbol} • {trade.side} • {trade.entryDate}
            </div>
          </div>
        ))}
      </div>
    );
  };

  const renderResultRaw = () => (
    <pre style={{
      fontSize: '8px',
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", monospace',
      whiteSpace: 'pre-wrap',
      wordBreak: 'break-word',
      lineHeight: 1.4,
    }}>
      {JSON.stringify(result, null, 2)}
    </pre>
  );

  const renderMetric = (label: string, value: string, color: string) => (
    <div style={{
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: '8px',
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
    }}>
      <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontFamily: '"IBM Plex Mono", monospace' }}>
        {label}
      </span>
      <span style={{ fontSize: '11px', fontWeight: 700, color, fontFamily: '"IBM Plex Mono", monospace' }}>
        {value}
      </span>
    </div>
  );

  // ============================================================================
  // RENDER: Status Bar
  // ============================================================================

  const renderStatusBar = () => (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderTop: `1px solid ${FINCEPT.BORDER}`,
      padding: '4px 16px',
      fontSize: '9px',
      color: FINCEPT.GRAY,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      fontFamily: '"IBM Plex Mono", monospace',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <span>PROVIDER: {providerConfig.displayName}</span>
        <span>•</span>
        <span>COMMAND: {activeCommand.toUpperCase()}</span>
        {currentStrategy && (
          <>
            <span>•</span>
            <span>STRATEGY: {currentStrategy.name.toUpperCase()}</span>
          </>
        )}
      </div>
      <div>
        {isRunning ? (
          <span style={{ color: FINCEPT.ORANGE }}>● RUNNING</span>
        ) : (
          <span style={{ color: FINCEPT.GREEN }}>● READY</span>
        )}
      </div>
    </div>
  );

  // ============================================================================
  // MAIN RENDER
  // ============================================================================

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      fontFamily: '"IBM Plex Mono", monospace',
    }}>
      {renderTopNav()}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {renderLeftPanel()}
        {renderCenterPanel()}
        {renderRightPanel()}
      </div>
      {renderStatusBar()}
    </div>
  );
};

export default BacktestingTab;
