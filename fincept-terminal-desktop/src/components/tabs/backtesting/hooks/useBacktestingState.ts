/**
 * Backtesting Tab - State Management Hook
 * Contains all state declarations and the main execution handler.
 */

import { useState, useEffect, useCallback, useMemo } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  PROVIDER_CONFIGS,
  ALL_COMMANDS,
  type BacktestingProviderSlug,
  type CommandType,
} from '../backtestingProviderConfigs';
import { PROVIDER_COLORS, INDICATORS } from '../constants';
import type { ResultView, BacktestingState } from '../types';

export function useBacktestingState(): BacktestingState {
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
  const [startDate, setStartDate] = useState(
    new Date(Date.now() - 366 * 24 * 60 * 60 * 1000).toISOString().split('T')[0]
  );
  const [endDate, setEndDate] = useState(
    new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString().split('T')[0]
  );
  const [initialCapital, setInitialCapital] = useState(100000);
  const [commission, setCommission] = useState(0.001);
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

  // --- Labels (ML) Config ---
  const [labelsType, setLabelsType] = useState('FIXLB');
  const [labelsParams, setLabelsParams] = useState<Record<string, any>>({ horizon: 5, threshold: 0.02 });

  // --- Splitters (CV) Config ---
  const [splitterType, setSplitterType] = useState('RollingSplitter');
  const [splitterParams, setSplitterParams] = useState<Record<string, any>>({ window_len: 252, test_len: 63, step: 21 });

  // --- Returns Analysis Config ---
  const [returnsAnalysisType, setReturnsAnalysisType] = useState('full');
  const [returnsRollingWindow, setReturnsRollingWindow] = useState(21);

  // --- Signal Generators Config ---
  const [signalGeneratorType, setSignalGeneratorType] = useState('RAND');
  const [signalGeneratorParams, setSignalGeneratorParams] = useState<Record<string, any>>({ n: 10, seed: 42 });

  // --- Execution State ---
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);
  const [resultView, setResultView] = useState<ResultView>('summary');

  // --- UI State ---
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [expandedSections, setExpandedSections] = useState<Record<string, boolean>>({
    market: true,
    strategy: true,
    execution: false,
    advanced: false,
  });

  // --- Workspace tab state persistence ---
  const getWorkspaceState = useCallback(() => ({
    selectedProvider, activeCommand, selectedCategory, selectedStrategy,
    symbols, startDate, endDate, initialCapital, commission,
  }), [selectedProvider, activeCommand, selectedCategory, selectedStrategy,
    symbols, startDate, endDate, initialCapital, commission]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.selectedProvider === 'string' && state.selectedProvider in PROVIDER_CONFIGS)
      setSelectedProvider(state.selectedProvider as BacktestingProviderSlug);
    if (typeof state.activeCommand === 'string') setActiveCommand(state.activeCommand as CommandType);
    if (typeof state.selectedCategory === 'string') setSelectedCategory(state.selectedCategory);
    if (typeof state.selectedStrategy === 'string') setSelectedStrategy(state.selectedStrategy);
    if (typeof state.symbols === 'string') setSymbols(state.symbols);
    if (typeof state.startDate === 'string') setStartDate(state.startDate);
    if (typeof state.endDate === 'string') setEndDate(state.endDate);
    if (typeof state.initialCapital === 'number') setInitialCapital(state.initialCapital);
    if (typeof state.commission === 'number') setCommission(state.commission);
  }, []);

  useWorkspaceTabState('backtesting', getWorkspaceState, setWorkspaceState);

  // --- Derived provider config ---
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
            const grouped: Record<string, any[]> = {};
            parsed.data.forEach((strategy: any) => {
              const category = strategy.category.toLowerCase();
              if (!grouped[category]) grouped[category] = [];
              grouped[category].push({
                id: strategy.id,
                name: strategy.name,
                description: `${strategy.category} strategy`,
                params: [],
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

  // --- Reset on provider switch ---
  useEffect(() => {
    const firstCmd = providerConfig.commands[0] || 'backtest';
    setActiveCommand(firstCmd);
    const categories = Object.keys(providerStrategies);
    const firstCategory = categories[0] || 'trend';
    setSelectedCategory(firstCategory);
    const firstStrategies = providerStrategies[firstCategory];
    if (firstStrategies && firstStrategies.length > 0) {
      setSelectedStrategy(firstStrategies[0].id);
    }
    setResult(null);
    setError(null);
  }, [selectedProvider, providerStrategies]); // eslint-disable-line react-hooks/exhaustive-deps

  // --- Current strategy ---
  const currentStrategy = useMemo(() => {
    const categoryStrategies = providerStrategies[selectedCategory] || [];
    return categoryStrategies.find((s: any) => s.id === selectedStrategy) || categoryStrategies[0];
  }, [providerStrategies, selectedCategory, selectedStrategy]);

  // --- Initialize strategy params ---
  useEffect(() => {
    if (currentStrategy) {
      const defaults: Record<string, any> = {};
      currentStrategy.params.forEach((param: any) => {
        defaults[param.name] = param.default;
      });
      setStrategyParams(defaults);
    }
  }, [currentStrategy]);

  // --- Toggle section ---
  const toggleSection = useCallback((section: string) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  }, []);

  // --- Run command handler ---
  const handleRunCommand = useCallback(async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);

    try {
      const symbolList = symbols.split(',').map(s => s.trim().toUpperCase()).filter(Boolean);
      let commandArgs: any = {
        symbols: symbolList,
        startDate,
        endDate,
        initialCapital,
      };

      const pythonCommand = providerConfig.commandMap[activeCommand] || activeCommand;

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

        case 'indicator': {
          const indicatorParamRanges: Record<string, { min: number; max: number; step: number }> = {};
          const selectedInd = INDICATORS.find(i => i.id === selectedIndicator);

          if (selectedInd) {
            const paramDefaults: Record<string, number> = {
              period: 20, k_period: 14, d_period: 3, fast: 12,
              slow: 26, signal: 9, alpha: 2, window: 20, atrMult: 2, lookback: 20,
            };

            const numericParams = selectedInd.params.filter(p => !['ewm'].includes(p));

            numericParams.forEach(paramName => {
              const defaultValue = paramDefaults[paramName] ?? 14;
              const value = indicatorParams[paramName] ?? defaultValue;
              const minVal = Math.max(2, Math.floor(value * 0.8));
              const maxVal = Math.ceil(value * 1.2);
              const stepVal = Math.max(1, Math.floor((maxVal - minVal) / 5));
              indicatorParamRanges[paramName] = { min: minVal, max: maxVal, step: stepVal };
            });
          }

          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            indicator: selectedIndicator,
            paramRanges: indicatorParamRanges,
          };
          break;
        }

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

        case 'labels':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            labelType: labelsType,
            params: labelsParams,
          };
          break;

        case 'splits':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            splitterType,
            params: splitterParams,
          };
          break;

        case 'returns':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            analysisType: returnsAnalysisType,
            rollingWindow: returnsRollingWindow,
            benchmarkSymbol: returnsAnalysisType === 'benchmark_comparison' ? benchmarkSymbol : null,
          };
          break;

        case 'signals':
          commandArgs = {
            symbols: symbolList,
            startDate,
            endDate,
            generatorType: signalGeneratorType,
            params: signalGeneratorParams,
          };
          break;
      }

      const argsJson = JSON.stringify(commandArgs);

      const response = await invoke<string>('execute_python_backtest', {
        provider: selectedProvider,
        command: pythonCommand,
        args: argsJson,
      });

      const parsed = JSON.parse(response);

      if (!parsed.success || parsed.error) {
        setError(parsed.error || 'Backtest failed with unknown error');
        setResult(null);
      } else {
        setResult(parsed);

        // DB persistence - silently save run
        try {
          const now = new Date().toISOString();
          await invoke('db_save_backtest_run', {
            run: {
              id: `${selectedProvider}_${activeCommand}_${Date.now()}`,
              strategy_id: currentStrategy?.id || null,
              provider_name: selectedProvider,
              config: argsJson,
              results: response,
              status: 'completed',
              performance_metrics: parsed.data?.performance ? JSON.stringify(parsed.data.performance) : null,
              error_message: null,
              created_at: now,
              completed_at: now,
            },
          });
        } catch {
          // Silent fail - DB persistence is best-effort
        }
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
    selectedIndicator, indicatorParams, indSignalMode, indSignalParams, currentStrategy,
    labelsType, labelsParams, splitterType, splitterParams,
    returnsAnalysisType, returnsRollingWindow,
    signalGeneratorType, signalGeneratorParams,
  ]);

  return {
    selectedProvider, setSelectedProvider,
    activeCommand, setActiveCommand,
    selectedCategory, setSelectedCategory,
    selectedStrategy, setSelectedStrategy,
    strategyParams, setStrategyParams,
    customCode, setCustomCode,
    symbols, setSymbols,
    startDate, setStartDate,
    endDate, setEndDate,
    initialCapital, setInitialCapital,
    commission, setCommission,
    slippage, setSlippage,
    leverage, setLeverage,
    margin, setMargin,
    stopLoss, setStopLoss,
    takeProfit, setTakeProfit,
    trailingStop, setTrailingStop,
    positionSizing, setPositionSizing,
    positionSizeValue, setPositionSizeValue,
    allowShort, setAllowShort,
    benchmarkSymbol, setBenchmarkSymbol,
    enableBenchmark, setEnableBenchmark,
    randomBenchmark, setRandomBenchmark,
    optimizeObjective, setOptimizeObjective,
    optimizeMethod, setOptimizeMethod,
    maxIterations, setMaxIterations,
    paramRanges, setParamRanges,
    wfSplits, setWfSplits,
    wfTrainRatio, setWfTrainRatio,
    selectedIndicator, setSelectedIndicator,
    indicatorParams, setIndicatorParams,
    indSignalMode, setIndSignalMode,
    indSignalParams, setIndSignalParams,
    labelsType, setLabelsType,
    labelsParams, setLabelsParams,
    splitterType, setSplitterType,
    splitterParams, setSplitterParams,
    returnsAnalysisType, setReturnsAnalysisType,
    returnsRollingWindow, setReturnsRollingWindow,
    signalGeneratorType, setSignalGeneratorType,
    signalGeneratorParams, setSignalGeneratorParams,
    isRunning,
    result,
    error,
    resultView, setResultView,
    showAdvanced, setShowAdvanced,
    expandedSections,
    toggleSection,
    providerConfig,
    providerColor,
    providerCommands,
    providerStrategies,
    providerCategoryInfo,
    currentStrategy,
    handleRunCommand,
  };
}
