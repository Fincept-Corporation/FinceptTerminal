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
// DATA SOURCE OPTIONS - Removed (data command removed)
// ============================================================================

// ============================================================================
// INDICATOR OPTIONS
// ============================================================================

const INDICATORS = [
  // Moving Averages
  { id: 'ma', label: 'Moving Average (SMA)', params: ['period'] },
  { id: 'ema', label: 'Moving Average (EMA)', params: ['period'] },
  { id: 'mstd', label: 'Moving Std Dev', params: ['period'] },

  // Oscillators
  { id: 'rsi', label: 'RSI', params: ['period'] },
  { id: 'stoch', label: 'Stochastic', params: ['k_period', 'd_period'] },
  { id: 'cci', label: 'CCI', params: ['period'] },
  { id: 'williams_r', label: 'Williams %R', params: ['period'] },

  // Trend Indicators
  { id: 'macd', label: 'MACD', params: ['fast', 'slow', 'signal'] },
  { id: 'adx', label: 'ADX', params: ['period'] },
  { id: 'momentum', label: 'Momentum', params: ['lookback'] },

  // Volatility
  { id: 'atr', label: 'ATR', params: ['period'] },
  { id: 'bbands', label: 'Bollinger Bands', params: ['period'] },
  { id: 'keltner', label: 'Keltner Channel', params: ['period'] },

  // Channels
  { id: 'donchian', label: 'Donchian Channel', params: ['period'] },

  // Statistical
  { id: 'zscore', label: 'Z-Score', params: ['period'] },

  // Volume
  { id: 'obv', label: 'OBV', params: [] },
  { id: 'vwap', label: 'VWAP', params: [] },
];

// ============================================================================
// LABEL GENERATORS - Removed (labels command removed)
// ============================================================================

// ============================================================================
// SPLITTERS - Removed (splits command removed)
// ============================================================================

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

  // --- Indicator Config ---
  const [selectedIndicator, setSelectedIndicator] = useState('ma');
  const [indicatorParams, setIndicatorParams] = useState<Record<string, any>>({});

  // --- Indicator Signals Config ---
  const [indSignalMode, setIndSignalMode] = useState('crossover_signals');
  const [indSignalParams, setIndSignalParams] = useState<Record<string, any>>({});

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

        case 'indicator':
          // Indicator command uses parameter sweeping (ranges)
          const indicatorParamRanges: Record<string, { min: number; max: number; step: number }> = {};
          const selectedInd = INDICATORS.find(i => i.id === selectedIndicator);

          if (selectedInd) {
            // Default values per parameter type
            const paramDefaults: Record<string, number> = {
              'period': 20,
              'k_period': 14,
              'd_period': 3,
              'fast': 12,
              'slow': 26,
              'signal': 9,
              'alpha': 2,
              'window': 20,
              'atrMult': 2,
              'lookback': 20,
            };

            // Filter to only numeric parameters (skip boolean flags like 'ewm')
            const numericParams = selectedInd.params.filter(p =>
              !['ewm'].includes(p) // Skip non-numeric flags
            );

            console.log('[INDICATOR] selectedInd:', selectedInd.id, 'params:', selectedInd.params);
            console.log('[INDICATOR] numericParams:', numericParams);
            console.log('[INDICATOR] indicatorParams:', indicatorParams);

            numericParams.forEach(paramName => {
              const defaultValue = paramDefaults[paramName] ?? 14;
              const value = indicatorParams[paramName] ?? defaultValue;

              // Create a range of ±20% around the value for sweep
              const minVal = Math.max(2, Math.floor(value * 0.8));
              const maxVal = Math.ceil(value * 1.2);
              const stepVal = Math.max(1, Math.floor((maxVal - minVal) / 5));

              indicatorParamRanges[paramName] = { min: minVal, max: maxVal, step: stepVal };
              console.log(`[INDICATOR] ${paramName}: value=${value}, range=[${minVal}, ${maxVal}], step=${stepVal}`);
            });
          }

          console.log('[INDICATOR] Final paramRanges:', indicatorParamRanges);

          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            indicator: selectedIndicator,
            paramRanges: indicatorParamRanges,
          };
          break;

        case 'indicator_signals':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            mode: indSignalMode,
            indicator: selectedIndicator,
            params: indSignalParams,
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

      // Check if Python returned an error
      if (!parsed.success || parsed.error) {
        setError(parsed.error || 'Backtest failed with unknown error');
        setResult(null);
      } else {
        setResult(parsed);
      }
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
    maxIterations, paramRanges, wfSplits, wfTrainRatio,
    selectedIndicator, indicatorParams, indSignalMode, indSignalParams
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
        {activeCommand === 'indicator' && renderIndicatorConfig()}
        {activeCommand === 'indicator_signals' && renderIndicatorSignalsConfig()}
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

  const renderIndicatorConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {renderSection('market', 'MARKET DATA', Database, (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {renderInput('Symbols', symbols, setSymbols, 'text')}
          {renderInput('Start Date', startDate, setStartDate, 'date')}
          {renderInput('End Date', endDate, setEndDate, 'date')}
        </div>
      ))}

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

  const renderIndicatorSignalsConfig = () => {
    const SIGNAL_MODES = [
      { id: 'crossover_signals', label: 'Crossover', description: 'MA/EMA crossover signals' },
      { id: 'threshold_signals', label: 'Threshold', description: 'RSI/CCI threshold signals' },
      { id: 'breakout_signals', label: 'Breakout', description: 'Channel breakout signals' },
      { id: 'mean_reversion_signals', label: 'Mean Reversion', description: 'Z-score mean reversion' },
      { id: 'signal_filter', label: 'Signal Filter', description: 'Filter signals with indicators' },
    ];

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Market Data */}
        {renderSection('market', 'MARKET DATA', Database, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            {renderInput('Symbols', symbols, setSymbols, 'text')}
            {renderInput('Start Date', startDate, setStartDate, 'date')}
            {renderInput('End Date', endDate, setEndDate, 'date')}
          </div>
        ))}

        {/* Signal Mode Selection */}
        {renderSection('signal_mode', 'SIGNAL MODE', Zap, (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
            {SIGNAL_MODES.map(mode => (
              <button
                key={mode.id}
                onClick={() => {
                  setIndSignalMode(mode.id);
                  setIndSignalParams({}); // Reset params when mode changes
                }}
                style={{
                  padding: '8px',
                  backgroundColor: indSignalMode === mode.id ? FINCEPT.YELLOW : 'transparent',
                  color: indSignalMode === mode.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: '"IBM Plex Mono", monospace',
                  textAlign: 'left',
                }}
              >
                <div>{mode.label}</div>
                <div style={{ fontSize: '7px', fontWeight: 400, marginTop: '2px', opacity: 0.7 }}>
                  {mode.description}
                </div>
              </button>
            ))}
          </div>
        ))}

        {/* Mode-Specific Parameters */}
        {indSignalMode === 'crossover_signals' && renderSection('crossover_params', 'CROSSOVER PARAMETERS', Activity, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>FAST INDICATOR</div>
              <select
                value={indSignalParams.fast_indicator ?? 'ma'}
                onChange={(e) => setIndSignalParams(prev => ({ ...prev, fast_indicator: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="ma">SMA</option>
                <option value="ema">EMA</option>
              </select>
            </div>
            {renderInput('Fast Period', indSignalParams.fast_period ?? 10, (v) => setIndSignalParams(prev => ({ ...prev, fast_period: v })), 'number')}
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>SLOW INDICATOR</div>
              <select
                value={indSignalParams.slow_indicator ?? 'ma'}
                onChange={(e) => setIndSignalParams(prev => ({ ...prev, slow_indicator: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="ma">SMA</option>
                <option value="ema">EMA</option>
              </select>
            </div>
            {renderInput('Slow Period', indSignalParams.slow_period ?? 20, (v) => setIndSignalParams(prev => ({ ...prev, slow_period: v })), 'number')}
          </div>
        ))}

        {indSignalMode === 'threshold_signals' && renderSection('threshold_params', 'THRESHOLD PARAMETERS', Activity, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>INDICATOR</div>
              <select
                value={selectedIndicator}
                onChange={(e) => setSelectedIndicator(e.target.value)}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="rsi">RSI</option>
                <option value="cci">CCI</option>
                <option value="williams_r">Williams %R</option>
                <option value="stoch">Stochastic</option>
              </select>
            </div>
            {renderInput('Period', indSignalParams.period ?? 14, (v) => setIndSignalParams(prev => ({ ...prev, period: v })), 'number')}
            {renderInput('Lower Threshold', indSignalParams.lower ?? 30, (v) => setIndSignalParams(prev => ({ ...prev, lower: v })), 'number')}
            {renderInput('Upper Threshold', indSignalParams.upper ?? 70, (v) => setIndSignalParams(prev => ({ ...prev, upper: v })), 'number')}
          </div>
        ))}

        {indSignalMode === 'breakout_signals' && renderSection('breakout_params', 'BREAKOUT PARAMETERS', Activity, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>CHANNEL TYPE</div>
              <select
                value={indSignalParams.channel ?? 'donchian'}
                onChange={(e) => setIndSignalParams(prev => ({ ...prev, channel: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="donchian">Donchian Channel</option>
                <option value="bbands">Bollinger Bands</option>
                <option value="keltner">Keltner Channel</option>
              </select>
            </div>
            {renderInput('Period', indSignalParams.period ?? 20, (v) => setIndSignalParams(prev => ({ ...prev, period: v })), 'number')}
          </div>
        ))}

        {indSignalMode === 'mean_reversion_signals' && renderSection('mean_reversion_params', 'MEAN REVERSION PARAMETERS', Activity, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
            {renderInput('Period', indSignalParams.period ?? 20, (v) => setIndSignalParams(prev => ({ ...prev, period: v })), 'number')}
            {renderInput('Z-Score Entry', indSignalParams.z_entry ?? 2.0, (v) => setIndSignalParams(prev => ({ ...prev, z_entry: v })), 'number')}
            {renderInput('Z-Score Exit', indSignalParams.z_exit ?? 0.0, (v) => setIndSignalParams(prev => ({ ...prev, z_exit: v })), 'number')}
          </div>
        ))}

        {indSignalMode === 'signal_filter' && renderSection('filter_params', 'SIGNAL FILTER PARAMETERS', Activity, (
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>BASE INDICATOR</div>
              <select
                value={indSignalParams.base_indicator ?? 'rsi'}
                onChange={(e) => setIndSignalParams(prev => ({ ...prev, base_indicator: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="rsi">RSI</option>
                <option value="cci">CCI</option>
                <option value="williams_r">Williams %R</option>
                <option value="momentum">Momentum</option>
                <option value="stoch">Stochastic</option>
                <option value="macd">MACD</option>
              </select>
            </div>
            {renderInput('Base Period', indSignalParams.base_period ?? 14, (v) => setIndSignalParams(prev => ({ ...prev, base_period: v })), 'number')}
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>FILTER INDICATOR</div>
              <select
                value={indSignalParams.filter_indicator ?? 'adx'}
                onChange={(e) => setIndSignalParams(prev => ({ ...prev, filter_indicator: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="adx">ADX</option>
                <option value="atr">ATR</option>
                <option value="mstd">Moving Std Dev</option>
                <option value="zscore">Z-Score</option>
                <option value="rsi">RSI</option>
              </select>
            </div>
            {renderInput('Filter Period', indSignalParams.filter_period ?? 14, (v) => setIndSignalParams(prev => ({ ...prev, filter_period: v })), 'number')}
            {renderInput('Filter Threshold', indSignalParams.filter_threshold ?? 25, (v) => setIndSignalParams(prev => ({ ...prev, filter_threshold: v })), 'number')}
            <div>
              <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>FILTER TYPE</div>
              <select
                value={indSignalParams.filter_type ?? 'above'}
                onChange={(e) => setIndSignalParams(prev => ({ ...prev, filter_type: e.target.value }))}
                style={{
                  width: '100%',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                }}
              >
                <option value="above">Above Threshold</option>
                <option value="below">Below Threshold</option>
              </select>
            </div>
          </div>
        ))}
      </div>
    );
  };






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
            {/* Render based on command type */}
            {activeCommand === 'backtest' && (
              <>
                {resultView === 'summary' && renderResultSummary()}
                {resultView === 'metrics' && renderResultMetrics()}
                {resultView === 'trades' && renderResultTrades()}
                {resultView === 'raw' && renderResultRaw()}

                {/* Show message if backtest has no performance data */}
                {!result?.data?.performance && (
                  <div style={{
                    padding: '12px',
                    backgroundColor: `${FINCEPT.YELLOW}20`,
                    border: `${FINCEPT.YELLOW}`,
                    borderRadius: '2px',
                  }}>
                    <div style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: FINCEPT.YELLOW,
                      marginBottom: '4px',
                    }}>
                      EMPTY RESULT
                    </div>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, lineHeight: 1.4 }}>
                      Backtest completed but returned no performance data. Check console logs for details.
                    </div>
                  </div>
                )}
              </>
            )}

            {/* Indicator Signals - show signal counts */}
            {activeCommand === 'indicator_signals' && renderSignalsResult()}

            {/* Indicator - show indicator values */}
            {activeCommand === 'indicator' && renderIndicatorResult()}

            {/* Other commands - show raw data */}
            {!['backtest', 'indicator_signals', 'indicator'].includes(activeCommand) && renderResultRaw()}
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
    const stats = result.data.statistics;

    const totalReturn = (perf.totalReturn ?? 0) * 100;
    const isProfit = totalReturn > 0;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* Status Badge */}
        <div style={{
          padding: '4px 8px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          color: FINCEPT.GREEN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          display: 'inline-block',
          alignSelf: 'flex-start',
        }}>
          ✓ COMPLETED
        </div>

        {/* Main Performance Card */}
        <div style={{
          padding: '12px',
          backgroundColor: `${isProfit ? FINCEPT.GREEN : FINCEPT.RED}10`,
          border: `2px solid ${isProfit ? FINCEPT.GREEN : FINCEPT.RED}`,
          borderRadius: '4px',
        }}>
          <div style={{
            fontSize: '9px',
            color: FINCEPT.GRAY,
            marginBottom: '4px',
            fontWeight: 600,
          }}>
            TOTAL RETURN
          </div>
          <div style={{
            fontSize: '20px',
            color: isProfit ? FINCEPT.GREEN : FINCEPT.RED,
            fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace',
          }}>
            {totalReturn >= 0 ? '+' : ''}{totalReturn.toFixed(2)}%
          </div>
          <div style={{
            fontSize: '8px',
            color: FINCEPT.MUTED,
            marginTop: '4px',
          }}>
            {stats?.initialCapital ? `$${stats.initialCapital.toLocaleString()}` : ''} → {stats?.finalCapital ? `$${stats.finalCapital.toLocaleString()}` : ''}
          </div>
        </div>

        {/* Key Metrics Grid */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {/* Sharpe Ratio */}
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '2px' }}>SHARPE</div>
            <div style={{
              fontSize: '14px',
              color: (perf.sharpeRatio ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.CYAN,
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {(perf.sharpeRatio ?? 0).toFixed(2)}
            </div>
          </div>

          {/* Max Drawdown */}
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '2px' }}>MAX DD</div>
            <div style={{
              fontSize: '14px',
              color: FINCEPT.RED,
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {((perf.maxDrawdown ?? 0) * 100).toFixed(2)}%
            </div>
          </div>

          {/* Win Rate */}
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '2px' }}>WIN RATE</div>
            <div style={{
              fontSize: '14px',
              color: (perf.winRate ?? 0) > 0.5 ? FINCEPT.GREEN : FINCEPT.YELLOW,
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {((perf.winRate ?? 0) * 100).toFixed(1)}%
            </div>
          </div>

          {/* Total Trades */}
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '2px' }}>TRADES</div>
            <div style={{
              fontSize: '14px',
              color: FINCEPT.CYAN,
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              {perf.totalTrades ?? 0}
            </div>
          </div>
        </div>

        {/* Trade Breakdown */}
        {(perf.totalTrades ?? 0) > 0 && (
          <div style={{
            padding: '8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
            fontSize: '8px',
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
              <span style={{ color: FINCEPT.GRAY }}>Winning Trades</span>
              <span style={{ color: FINCEPT.GREEN, fontWeight: 600 }}>{perf.winningTrades ?? 0}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
              <span style={{ color: FINCEPT.GRAY }}>Losing Trades</span>
              <span style={{ color: FINCEPT.RED, fontWeight: 600 }}>{perf.losingTrades ?? 0}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.GRAY }}>Profit Factor</span>
              <span style={{
                color: (perf.profitFactor ?? 0) > 1 ? FINCEPT.GREEN : FINCEPT.RED,
                fontWeight: 600,
              }}>
                {(perf.profitFactor ?? 0).toFixed(2)}
              </span>
            </div>
          </div>
        )}

        {/* Warning for zero trades */}
        {(perf.totalTrades ?? 0) === 0 && (
          <div style={{
            padding: '8px',
            backgroundColor: `${FINCEPT.YELLOW}10`,
            border: `1px solid ${FINCEPT.YELLOW}`,
            borderRadius: '3px',
            fontSize: '8px',
            color: FINCEPT.YELLOW,
          }}>
            ⚠ No trades executed. Strategy generated no signals or all entries were skipped.
          </div>
        )}
      </div>
    );
  };

  const formatMetricValue = (key: string, value: any): string => {
    if (value === null || value === undefined) return 'N/A';

    // Percentage metrics (convert from decimal to %)
    const percentageKeys = ['totalReturn', 'annualizedReturn', 'maxDrawdown', 'winRate', 'lossRate', 'volatility'];
    if (percentageKeys.includes(key)) {
      return `${(value * 100).toFixed(2)}%`;
    }

    // Ratio metrics (2 decimals)
    const ratioKeys = ['sharpeRatio', 'sortinoRatio', 'calmarRatio', 'profitFactor', 'informationRatio', 'treynorRatio'];
    if (ratioKeys.includes(key)) {
      return value.toFixed(2);
    }

    // Count metrics (no decimals)
    const countKeys = ['totalTrades', 'winningTrades', 'losingTrades', 'maxDrawdownDuration'];
    if (countKeys.includes(key)) {
      return Math.round(value).toString();
    }

    // Currency metrics (2 decimals with $)
    const currencyKeys = ['averageWin', 'averageLoss', 'largestWin', 'largestLoss', 'averageTradeReturn', 'expectancy'];
    if (currencyKeys.includes(key)) {
      return `$${value.toFixed(2)}`;
    }

    // Beta/Alpha (2 decimals)
    if (key === 'alpha' || key === 'beta') {
      return value.toFixed(2);
    }

    // Default
    return typeof value === 'number' ? value.toFixed(2) : String(value);
  };

  const getMetricColor = (key: string, value: any): string => {
    if (value === null || value === undefined) return FINCEPT.MUTED;

    // Green for positive, red for negative
    const directionalKeys = ['totalReturn', 'annualizedReturn', 'sharpeRatio', 'sortinoRatio', 'calmarRatio', 'profitFactor', 'alpha'];
    if (directionalKeys.includes(key)) {
      return value > 0 ? FINCEPT.GREEN : value < 0 ? FINCEPT.RED : FINCEPT.GRAY;
    }

    // Red for drawdown (worse when bigger)
    if (key === 'maxDrawdown') {
      return value > 0 ? FINCEPT.RED : FINCEPT.GRAY;
    }

    // Green for high win rate
    if (key === 'winRate') {
      return value > 0.5 ? FINCEPT.GREEN : value > 0 ? FINCEPT.YELLOW : FINCEPT.RED;
    }

    return FINCEPT.CYAN;
  };

  const getMetricDescription = (key: string): string => {
    const descriptions: Record<string, string> = {
      totalReturn: 'Total percentage gain/loss',
      annualizedReturn: 'Return normalized to yearly basis',
      sharpeRatio: 'Risk-adjusted return (>1 is good)',
      sortinoRatio: 'Downside risk-adjusted return',
      maxDrawdown: 'Largest peak-to-trough decline',
      winRate: 'Percentage of winning trades',
      lossRate: 'Percentage of losing trades',
      profitFactor: 'Gross profit / gross loss',
      volatility: 'Standard deviation of returns',
      calmarRatio: 'Return / max drawdown',
      totalTrades: 'Number of completed trades',
      winningTrades: 'Number of profitable trades',
      losingTrades: 'Number of losing trades',
      averageWin: 'Average profit per winning trade',
      averageLoss: 'Average loss per losing trade',
      largestWin: 'Biggest single winning trade',
      largestLoss: 'Biggest single losing trade',
      averageTradeReturn: 'Mean return across all trades',
      expectancy: 'Expected profit per trade',
      alpha: 'Excess return vs benchmark (requires benchmark)',
      beta: 'Volatility vs benchmark (requires benchmark)',
      maxDrawdownDuration: 'Longest drawdown period in bars',
      informationRatio: 'Risk-adjusted excess return (requires benchmark)',
      treynorRatio: 'Return per unit of systematic risk (requires benchmark)',
    };
    return descriptions[key] || '';
  };

  const renderResultMetrics = () => {
    if (!result?.data?.performance) return null;
    const perf = result.data.performance;

    // Group metrics by category
    const categories = {
      'Returns': ['totalReturn', 'annualizedReturn', 'expectancy'],
      'Risk Metrics': ['sharpeRatio', 'sortinoRatio', 'calmarRatio', 'volatility', 'maxDrawdown'],
      'Trade Statistics': ['totalTrades', 'winningTrades', 'losingTrades', 'winRate', 'lossRate', 'profitFactor'],
      'Trade Performance': ['averageWin', 'averageLoss', 'largestWin', 'largestLoss', 'averageTradeReturn'],
      'Advanced Metrics': ['alpha', 'beta', 'informationRatio', 'treynorRatio', 'maxDrawdownDuration'],
    };

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {Object.entries(categories).map(([category, metrics]) => {
          // Filter out metrics that don't exist or are null
          const availableMetrics = metrics.filter(key => key in perf);

          // Skip "Advanced Metrics" if all values are null/undefined
          if (category === 'Advanced Metrics') {
            const hasData = availableMetrics.some(key =>
              perf[key as keyof typeof perf] !== null &&
              perf[key as keyof typeof perf] !== undefined
            );
            if (!hasData) return null;
          }

          return (
            <div key={category}>
              {/* Category Header */}
              <div style={{
                fontSize: '8px',
                fontWeight: 700,
                color: FINCEPT.ORANGE,
                marginBottom: '8px',
                paddingBottom: '4px',
                borderBottom: `2px solid ${FINCEPT.ORANGE}40`,
                letterSpacing: '0.5px',
              }}>
                {category.toUpperCase()}
              </div>

              {/* Metrics Grid */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {availableMetrics.map(key => {
                  const value = perf[key as keyof typeof perf];

                  // Skip if value is null/undefined and it's an advanced metric
                  if ((value === null || value === undefined) && category === 'Advanced Metrics') {
                    return null;
                  }

                  return (
                    <div key={key} style={{
                      padding: '6px 8px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      display: 'flex',
                      flexDirection: 'column',
                      gap: '2px',
                    }}>
                      {/* Metric Name and Value */}
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                        <span style={{
                          color: FINCEPT.WHITE,
                          fontSize: '8px',
                          fontWeight: 600,
                        }}>
                          {key.replace(/([A-Z])/g, ' $1').trim().split(' ').map(w =>
                            w.charAt(0).toUpperCase() + w.slice(1)
                          ).join(' ')}
                        </span>
                        <span style={{
                          color: getMetricColor(key, value),
                          fontFamily: '"IBM Plex Mono", monospace',
                          fontSize: '10px',
                          fontWeight: 700,
                        }}>
                          {formatMetricValue(key, value)}
                        </span>
                      </div>

                      {/* Description */}
                      <div style={{
                        fontSize: '7px',
                        color: FINCEPT.MUTED,
                        lineHeight: 1.3,
                      }}>
                        {getMetricDescription(key)}
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>
          );
        })}

        {/* Note about missing advanced metrics */}
        {(!perf.alpha && !perf.beta && !perf.informationRatio && !perf.treynorRatio) && (
          <div style={{
            padding: '8px',
            backgroundColor: `${FINCEPT.GRAY}10`,
            border: `1px solid ${FINCEPT.GRAY}40`,
            borderRadius: '3px',
            fontSize: '7px',
            color: FINCEPT.MUTED,
            lineHeight: 1.4,
          }}>
            <div style={{ color: FINCEPT.GRAY, fontWeight: 600, marginBottom: '4px' }}>
              💡 BENCHMARK METRICS
            </div>
            Advanced metrics (Alpha, Beta, Information Ratio, Treynor Ratio) require a benchmark symbol.
            Enable benchmark in settings and run again to see these metrics.
          </div>
        )}
      </div>
    );
  };

  const renderResultTrades = () => {
    if (!result?.data?.trades) return null;
    const trades = result.data.trades;

    if (trades.length === 0) {
      return (
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '40px 20px',
          color: FINCEPT.MUTED,
          fontSize: '9px',
          textAlign: 'center',
        }}>
          <TrendingUp size={24} style={{ opacity: 0.3, marginBottom: '8px' }} />
          <div>No trades executed</div>
          <div style={{ fontSize: '8px', marginTop: '4px' }}>
            Strategy generated no entry/exit signals
          </div>
        </div>
      );
    }

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
        <div style={{
          fontSize: '8px',
          color: FINCEPT.GRAY,
          padding: '4px 0',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          SHOWING {Math.min(20, trades.length)} OF {trades.length} TRADES
        </div>
        {trades.slice(0, 20).map((trade: any, i: number) => {
          const isWin = (trade.pnl ?? 0) > 0;
          const pnlPercent = trade.pnlPercent ?? ((trade.exitPrice && trade.entryPrice)
            ? ((trade.exitPrice - trade.entryPrice) / trade.entryPrice * 100)
            : 0);

          return (
            <div key={i} style={{
              padding: '8px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${isWin ? FINCEPT.GREEN : FINCEPT.RED}40`,
              borderLeft: `3px solid ${isWin ? FINCEPT.GREEN : FINCEPT.RED}`,
              borderRadius: '2px',
            }}>
              {/* Header: Trade ID and P&L */}
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px', alignItems: 'center' }}>
                <span style={{
                  color: FINCEPT.GRAY,
                  fontSize: '7px',
                  fontWeight: 600,
                }}>
                  TRADE #{i + 1}
                </span>
                <div style={{ textAlign: 'right' }}>
                  <div style={{
                    color: isWin ? FINCEPT.GREEN : FINCEPT.RED,
                    fontSize: '11px',
                    fontWeight: 700,
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    {isWin ? '+' : ''}{trade.pnl ? `$${trade.pnl.toFixed(2)}` : 'N/A'}
                  </div>
                  <div style={{
                    color: isWin ? FINCEPT.GREEN : FINCEPT.RED,
                    fontSize: '7px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}>
                    {isWin ? '+' : ''}{pnlPercent.toFixed(2)}%
                  </div>
                </div>
              </div>

              {/* Trade Details */}
              <div style={{ fontSize: '7px', color: FINCEPT.MUTED, display: 'flex', flexDirection: 'column', gap: '3px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Symbol</span>
                  <span style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>{trade.symbol}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Side</span>
                  <span style={{
                    color: trade.side === 'long' ? FINCEPT.GREEN : FINCEPT.RED,
                    fontWeight: 600,
                    textTransform: 'uppercase',
                  }}>
                    {trade.side}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Entry</span>
                  <span style={{ color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
                    ${trade.entryPrice?.toFixed(2) ?? 'N/A'}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Exit</span>
                  <span style={{ color: FINCEPT.WHITE, fontFamily: '"IBM Plex Mono", monospace' }}>
                    ${trade.exitPrice?.toFixed(2) ?? 'N/A'}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span>Quantity</span>
                  <span style={{ color: FINCEPT.CYAN }}>{trade.quantity?.toFixed(4) ?? 'N/A'}</span>
                </div>
                {trade.holdingPeriod && (
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span>Duration</span>
                    <span style={{ color: FINCEPT.YELLOW }}>{trade.holdingPeriod} bars</span>
                  </div>
                )}
                {trade.exitReason && (
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span>Exit Reason</span>
                    <span style={{ color: FINCEPT.ORANGE, textTransform: 'uppercase' }}>{trade.exitReason}</span>
                  </div>
                )}
              </div>
            </div>
          );
        })}
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
  // RENDER: Command-Specific Results
  // ============================================================================


  const renderSignalsResult = () => {
    if (!result?.data) return renderResultRaw();

    const data = result.data;
    const entrySignals = data.entrySignals ?? data.entries ?? [];
    const exitSignals = data.exitSignals ?? data.exits ?? [];
    const hasMetrics = data.avgReturn !== undefined;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div style={{
          padding: '4px 8px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          color: FINCEPT.GREEN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          display: 'inline-block',
          alignSelf: 'flex-start',
        }}>
          ✓ SIGNALS GENERATED{data.symbol ? ` - ${data.symbol}` : ''}
        </div>

        {/* Performance Metrics */}
        {hasMetrics && (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '12px' }}>
              SIGNAL PERFORMANCE METRICS
            </div>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>WIN RATE</div>
                <div style={{ fontSize: '16px', fontWeight: 700, color: data.winRate >= 50 ? FINCEPT.GREEN : FINCEPT.RED }}>
                  {data.winRate?.toFixed(1) ?? 'N/A'}%
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>AVG RETURN</div>
                <div style={{ fontSize: '16px', fontWeight: 700, color: data.avgReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED }}>
                  {data.avgReturn?.toFixed(2) ?? 'N/A'}%
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>PROFIT FACTOR</div>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN }}>
                  {data.profitFactor !== null && data.profitFactor !== undefined
                    ? (data.profitFactor === Infinity ? '∞' : data.profitFactor.toFixed(2))
                    : 'N/A'}
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>SIGNALS</div>
                <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.WHITE }}>
                  {data.totalSignals ?? 'N/A'}
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>BEST TRADE</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.GREEN }}>
                  +{data.bestTrade?.toFixed(2) ?? 'N/A'}%
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>WORST TRADE</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.RED }}>
                  {data.worstTrade?.toFixed(2) ?? 'N/A'}%
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>AVG HOLD (DAYS)</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.WHITE }}>
                  {data.avgHoldingPeriod?.toFixed(0) ?? 'N/A'}
                </div>
              </div>
              <div>
                <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '4px' }}>SIGNAL DENSITY</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.YELLOW }}>
                  {data.signalDensity?.toFixed(1) ?? 'N/A'}%
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Signal Counts */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '3px',
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr',
          gap: '12px',
        }}>
          <div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>ENTRY SIGNALS</div>
            <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.GREEN }}>
              {data.entryCount ?? 0}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>EXIT SIGNALS</div>
            <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.RED }}>
              {data.exitCount ?? 0}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '4px' }}>TOTAL BARS</div>
            <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.WHITE }}>
              {data.totalBars ?? 0}
            </div>
          </div>
        </div>

        {/* Detailed Entry Signals */}
        {entrySignals.length > 0 && (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 600 }}>
              ENTRY SIGNALS (showing first 10)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {entrySignals.slice(0, 10).map((signal: any, idx: number) => (
                <div key={idx} style={{
                  padding: '6px 8px',
                  backgroundColor: `${FINCEPT.GREEN}15`,
                  borderLeft: `2px solid ${FINCEPT.GREEN}`,
                  borderRadius: '2px',
                  fontSize: '8px',
                  display: 'grid',
                  gridTemplateColumns: typeof signal === 'object' ? '2fr 1fr 1fr 1fr' : '1fr',
                  gap: '8px',
                }}>
                  {typeof signal === 'object' ? (
                    <>
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Date: </span>
                        <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{signal.date}</span>
                      </div>
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Price: </span>
                        <span style={{ color: FINCEPT.CYAN }}>${signal.price?.toFixed(2)}</span>
                      </div>
                      {signal.fastMA !== undefined && (
                        <div>
                          <span style={{ color: FINCEPT.GRAY }}>Fast: </span>
                          <span style={{ color: FINCEPT.YELLOW }}>{signal.fastMA?.toFixed(2)}</span>
                        </div>
                      )}
                      {signal.indicatorValue !== undefined && (
                        <div>
                          <span style={{ color: FINCEPT.GRAY }}>Indicator: </span>
                          <span style={{ color: FINCEPT.PURPLE }}>{signal.indicatorValue?.toFixed(2)}</span>
                        </div>
                      )}
                    </>
                  ) : (
                    <span style={{ color: FINCEPT.WHITE }}>{signal}</span>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Detailed Exit Signals */}
        {exitSignals.length > 0 && (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 600 }}>
              EXIT SIGNALS (showing first 10)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {exitSignals.slice(0, 10).map((signal: any, idx: number) => (
                <div key={idx} style={{
                  padding: '6px 8px',
                  backgroundColor: `${FINCEPT.RED}15`,
                  borderLeft: `2px solid ${FINCEPT.RED}`,
                  borderRadius: '2px',
                  fontSize: '8px',
                  display: 'grid',
                  gridTemplateColumns: typeof signal === 'object' ? '2fr 1fr 1fr 1fr' : '1fr',
                  gap: '8px',
                }}>
                  {typeof signal === 'object' ? (
                    <>
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Date: </span>
                        <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{signal.date}</span>
                      </div>
                      <div>
                        <span style={{ color: FINCEPT.GRAY }}>Price: </span>
                        <span style={{ color: FINCEPT.CYAN }}>${signal.price?.toFixed(2)}</span>
                      </div>
                      {signal.slowMA !== undefined && (
                        <div>
                          <span style={{ color: FINCEPT.GRAY }}>Slow: </span>
                          <span style={{ color: FINCEPT.ORANGE }}>{signal.slowMA?.toFixed(2)}</span>
                        </div>
                      )}
                      {signal.indicatorValue !== undefined && (
                        <div>
                          <span style={{ color: FINCEPT.GRAY }}>Indicator: </span>
                          <span style={{ color: FINCEPT.PURPLE }}>{signal.indicatorValue?.toFixed(2)}</span>
                        </div>
                      )}
                    </>
                  ) : (
                    <span style={{ color: FINCEPT.WHITE }}>{signal}</span>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    );
  };


  const renderIndicatorResult = () => {
    if (!result?.data) return renderResultRaw();

    const data = result.data;
    const indicator = data.indicator ?? 'Unknown';
    const totalCombinations = data.totalCombinations ?? 0;
    const results = data.results ?? [];

    // Helper to render single stat value
    const renderStat = (label: string, value: number | undefined, color: string) => (
      <div>
        <span style={{ color: FINCEPT.GRAY }}>{label}: </span>
        <span style={{ color }}>{value?.toFixed(2) ?? 'N/A'}</span>
      </div>
    );

    // Render single-output indicator result
    const renderSingleOutput = (r: any, idx: number) => (
      <div key={idx} style={{
        padding: '6px 8px',
        backgroundColor: `${FINCEPT.ORANGE}10`,
        borderLeft: `2px solid ${FINCEPT.PURPLE}`,
        borderRadius: '2px',
        display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr 1fr',
        gap: '8px',
        fontSize: '8px',
      }}>
        <div>
          <span style={{ color: FINCEPT.GRAY }}>Params: </span>
          <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
            {JSON.stringify(r.params)}
          </span>
        </div>
        {renderStat('Mean', r.mean, FINCEPT.CYAN)}
        {renderStat('Last', r.last, FINCEPT.ORANGE)}
        <div>
          <span style={{ color: FINCEPT.GRAY }}>Range: </span>
          <span style={{ color: FINCEPT.WHITE }}>
            {r.min?.toFixed(2) ?? 'N/A'} - {r.max?.toFixed(2) ?? 'N/A'}
          </span>
        </div>
      </div>
    );

    // Render multi-output indicator result (MACD, BBands, Stoch)
    const renderMultiOutput = (r: any, idx: number) => {
      const outputs = r.outputs ?? {};
      const componentNames = Object.keys(outputs);

      return (
        <div key={idx} style={{
          padding: '8px',
          backgroundColor: `${FINCEPT.ORANGE}10`,
          borderLeft: `2px solid ${FINCEPT.PURPLE}`,
          borderRadius: '2px',
          fontSize: '8px',
        }}>
          {/* Parameters */}
          <div style={{ marginBottom: '8px', paddingBottom: '6px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <span style={{ color: FINCEPT.GRAY }}>Params: </span>
            <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>
              {JSON.stringify(r.params)}
            </span>
          </div>

          {/* Components */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {componentNames.map(componentName => {
              const component = outputs[componentName];
              return (
                <div key={componentName} style={{
                  padding: '4px 6px',
                  backgroundColor: `${FINCEPT.CYAN}15`,
                  borderRadius: '2px',
                }}>
                  <div style={{ color: FINCEPT.CYAN, fontWeight: 700, marginBottom: '4px', fontSize: '7px' }}>
                    {componentName.toUpperCase()}
                  </div>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '6px' }}>
                    {renderStat('Mean', component.mean, FINCEPT.WHITE)}
                    {renderStat('Std', component.std, FINCEPT.GRAY)}
                    {renderStat('Last', component.last, FINCEPT.ORANGE)}
                    <div>
                      <span style={{ color: FINCEPT.GRAY }}>Range: </span>
                      <span style={{ color: FINCEPT.WHITE }}>
                        {component.min?.toFixed(2) ?? 'N/A'} - {component.max?.toFixed(2) ?? 'N/A'}
                      </span>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      );
    };

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div style={{
          padding: '4px 8px',
          backgroundColor: `${FINCEPT.GREEN}20`,
          color: FINCEPT.GREEN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          display: 'inline-block',
          alignSelf: 'flex-start',
        }}>
          ✓ INDICATOR SWEEP COMPLETED
        </div>

        {/* Summary Card */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '3px',
        }}>
          <div style={{ marginBottom: '8px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE }}>
              {indicator.toUpperCase()} PARAMETER SWEEP
            </div>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
              Total Combinations: {totalCombinations}
            </div>
          </div>
        </div>

        {/* Results Table */}
        {results.length > 0 ? (
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '3px',
          }}>
            <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px', fontWeight: 600 }}>
              PARAMETER COMBINATIONS (showing first 20)
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {results.slice(0, 20).map((r: any, idx: number) => {
                // Check if this is multi-output (has 'outputs' field) or single-output
                const isMultiOutput = r.outputs && Object.keys(r.outputs).length > 0;
                return isMultiOutput ? renderMultiOutput(r, idx) : renderSingleOutput(r, idx);
              })}
            </div>
          </div>
        ) : (
          <div style={{
            padding: '12px',
            backgroundColor: `${FINCEPT.YELLOW}20`,
            border: `1px solid ${FINCEPT.YELLOW}`,
            borderRadius: '2px',
          }}>
            <div style={{ fontSize: '9px', color: FINCEPT.YELLOW }}>
              No parameter combinations generated. Check parameter ranges.
            </div>
          </div>
        )}
      </div>
    );
  };

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
