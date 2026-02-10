/**
 * Backtesting Tab - Shared Types
 */

import type { CommandType, BacktestingProviderSlug } from './backtestingProviderConfigs';

export type ResultView = 'summary' | 'metrics' | 'trades' | 'charts' | 'raw';

export interface BacktestingState {
  // Provider
  selectedProvider: BacktestingProviderSlug;
  setSelectedProvider: (p: BacktestingProviderSlug) => void;

  // Core
  activeCommand: CommandType;
  setActiveCommand: (c: CommandType) => void;
  selectedCategory: string;
  setSelectedCategory: (c: string) => void;
  selectedStrategy: string;
  setSelectedStrategy: (s: string) => void;
  strategyParams: Record<string, any>;
  setStrategyParams: React.Dispatch<React.SetStateAction<Record<string, any>>>;
  customCode: string;
  setCustomCode: (c: string) => void;

  // Backtest Config
  symbols: string;
  setSymbols: (s: string) => void;
  startDate: string;
  setStartDate: (d: string) => void;
  endDate: string;
  setEndDate: (d: string) => void;
  initialCapital: number;
  setInitialCapital: (n: number) => void;
  commission: number;
  setCommission: (n: number) => void;
  slippage: number;
  setSlippage: (n: number) => void;

  // Advanced Config
  leverage: number;
  setLeverage: (n: number) => void;
  margin: number;
  setMargin: (n: number) => void;
  stopLoss: number | null;
  setStopLoss: (n: number | null) => void;
  takeProfit: number | null;
  setTakeProfit: (n: number | null) => void;
  trailingStop: number | null;
  setTrailingStop: (n: number | null) => void;
  positionSizing: string;
  setPositionSizing: (s: string) => void;
  positionSizeValue: number;
  setPositionSizeValue: (n: number) => void;
  allowShort: boolean;
  setAllowShort: (b: boolean) => void;
  benchmarkSymbol: string;
  setBenchmarkSymbol: (s: string) => void;
  enableBenchmark: boolean;
  setEnableBenchmark: (b: boolean) => void;
  randomBenchmark: boolean;
  setRandomBenchmark: (b: boolean) => void;

  // Optimize
  optimizeObjective: 'sharpe' | 'sortino' | 'calmar' | 'return';
  setOptimizeObjective: (o: 'sharpe' | 'sortino' | 'calmar' | 'return') => void;
  optimizeMethod: 'grid' | 'random';
  setOptimizeMethod: (m: 'grid' | 'random') => void;
  maxIterations: number;
  setMaxIterations: (n: number) => void;
  paramRanges: Record<string, { min: number; max: number; step: number }>;
  setParamRanges: React.Dispatch<React.SetStateAction<Record<string, { min: number; max: number; step: number }>>>;

  // Walk Forward
  wfSplits: number;
  setWfSplits: (n: number) => void;
  wfTrainRatio: number;
  setWfTrainRatio: (n: number) => void;

  // Indicator
  selectedIndicator: string;
  setSelectedIndicator: (i: string) => void;
  indicatorParams: Record<string, any>;
  setIndicatorParams: React.Dispatch<React.SetStateAction<Record<string, any>>>;

  // Indicator Signals
  indSignalMode: string;
  setIndSignalMode: (m: string) => void;
  indSignalParams: Record<string, any>;
  setIndSignalParams: React.Dispatch<React.SetStateAction<Record<string, any>>>;

  // Labels (ML)
  labelsType: string;
  setLabelsType: (t: string) => void;
  labelsParams: Record<string, any>;
  setLabelsParams: React.Dispatch<React.SetStateAction<Record<string, any>>>;

  // Splitters (CV)
  splitterType: string;
  setSplitterType: (t: string) => void;
  splitterParams: Record<string, any>;
  setSplitterParams: React.Dispatch<React.SetStateAction<Record<string, any>>>;

  // Returns Analysis
  returnsAnalysisType: string;
  setReturnsAnalysisType: (t: string) => void;
  returnsRollingWindow: number;
  setReturnsRollingWindow: (n: number) => void;

  // Random Signal Generators
  signalGeneratorType: string;
  setSignalGeneratorType: (t: string) => void;
  signalGeneratorParams: Record<string, any>;
  setSignalGeneratorParams: React.Dispatch<React.SetStateAction<Record<string, any>>>;

  // Execution
  isRunning: boolean;
  result: any;
  error: string | null;
  resultView: ResultView;
  setResultView: (v: ResultView) => void;

  // UI
  showAdvanced: boolean;
  setShowAdvanced: (b: boolean) => void;
  expandedSections: Record<string, boolean>;
  toggleSection: (id: string) => void;

  // Derived
  providerConfig: any;
  providerColor: string;
  providerCommands: any[];
  providerStrategies: Record<string, any[]>;
  providerCategoryInfo: Record<string, any>;
  currentStrategy: any;

  // Actions
  handleRunCommand: () => Promise<void>;
}
