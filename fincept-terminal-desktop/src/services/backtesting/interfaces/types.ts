/**
 * Shared types for backtesting system
 * Platform-independent types used across all providers
 */

import { Node, Edge } from 'reactflow';

// ============================================================================
// Provider Capabilities
// ============================================================================

export interface ProviderCapabilities {
  backtesting: boolean;
  optimization: boolean;
  liveTrading: boolean;
  research: boolean;
  multiAsset: AssetClass[];
  indicators: boolean;
  customStrategies: boolean;
  maxConcurrentBacktests: number;
  supportedTimeframes: Timeframe[];
  supportedMarkets?: string[];
}

export type AssetClass = 'stocks' | 'options' | 'futures' | 'crypto' | 'forex' | 'cfd' | 'bonds';
export type Timeframe = 'tick' | 'second' | 'minute' | 'hour' | 'daily' | 'weekly' | 'monthly';

// ============================================================================
// Provider Configuration
// ============================================================================

export interface ProviderConfig {
  name: string;
  adapterType: string;
  apiKey?: string;
  apiSecret?: string;
  endpoint?: string;
  mode?: 'cli' | 'api' | 'local';
  settings?: Record<string, any>;
  options?: Record<string, any>;
}

export interface ProviderInfo {
  name: string;
  version: string;
  capabilities: ProviderCapabilities;
  description: string;
  isActive: boolean;
  isEnabled: boolean;
  config?: ProviderConfig;
}

// ============================================================================
// Initialization & Connection
// ============================================================================

export interface InitResult {
  success: boolean;
  message: string;
  version?: string;
  error?: string;
}

export interface TestResult {
  success: boolean;
  message: string;
  latency?: number;
  metadata?: Record<string, any>;
  error?: string;
}

// ============================================================================
// Strategy Parameters
// ============================================================================

export interface StrategyParameter {
  name: string;
  type: 'int' | 'float' | 'bool' | 'string' | 'enum';
  defaultValue: any;
  min?: number;
  max?: number;
  step?: number;
  options?: string[];
  description: string;
  required?: boolean;
}

// ============================================================================
// Asset Selection
// ============================================================================

export interface AssetSelection {
  symbol: string;
  assetClass: AssetClass;
  exchange?: string;
  weight?: number;
}

// ============================================================================
// Slippage & Commission Models
// ============================================================================

export interface SlippageModel {
  type: 'fixed' | 'percentage' | 'volumeShare';
  value: number;
}

export interface CommissionModel {
  type: 'fixed' | 'percentage' | 'perShare' | 'tiered';
  value: number;
  minimum?: number;
  maximum?: number;
}

// ============================================================================
// Backtest Request & Configuration
// ============================================================================

export interface BacktestRequest {
  strategy: any; // Will be StrategyDefinition from IStrategyDefinition.ts
  startDate: string;
  endDate: string;
  initialCapital: number;
  assets: AssetSelection[];
  parameters?: Record<string, any>;
  benchmark?: string;
  slippageModel?: SlippageModel;
  commissionModel?: CommissionModel;
  timeframe?: Timeframe;
  warmupPeriod?: number;
}

export interface BacktestConfig {
  startDate: string;
  endDate: string;
  initialCapital: number;
  assets: AssetSelection[];
  benchmark?: string;
  slippageModel?: SlippageModel;
  commissionModel?: CommissionModel;
  timeframe?: Timeframe;
}

// ============================================================================
// Backtest Results
// ============================================================================

export interface BacktestResult {
  id: string;
  status: 'completed' | 'failed' | 'running' | 'cancelled';
  performance: PerformanceMetrics;
  trades: Trade[];
  equity: EquityPoint[];
  statistics: BacktestStatistics;
  charts?: ChartData[];
  logs: string[];
  error?: string;
  startTime?: string;
  endTime?: string;
  duration?: number;
}

export interface BacktestStatus {
  id: string;
  status: 'completed' | 'failed' | 'running' | 'cancelled';
  progress?: number;
  currentDate?: string;
  message?: string;
  error?: string;
}

// ============================================================================
// Performance Metrics
// ============================================================================

export interface PerformanceMetrics {
  totalReturn: number;
  annualizedReturn: number;
  sharpeRatio: number;
  sortinoRatio: number;
  maxDrawdown: number;
  maxDrawdownDuration?: number;
  winRate: number;
  lossRate: number;
  profitFactor: number;
  alpha?: number;
  beta?: number;
  volatility: number;
  calmarRatio: number;
  informationRatio?: number;
  treynorRatio?: number;
  totalTrades: number;
  winningTrades: number;
  losingTrades: number;
  averageWin: number;
  averageLoss: number;
  largestWin: number;
  largestLoss: number;
  averageTradeReturn: number;
  expectancy: number;
}

export interface BacktestStatistics {
  startDate: string;
  endDate: string;
  initialCapital: number;
  finalCapital: number;
  totalFees: number;
  totalSlippage: number;
  totalTrades: number;
  winningDays: number;
  losingDays: number;
  averageDailyReturn: number;
  bestDay: number;
  worstDay: number;
  consecutiveWins: number;
  consecutiveLosses: number;
  maxDrawdownDate?: string;
  recoveryTime?: number;
}

// ============================================================================
// Trades
// ============================================================================

export interface Trade {
  id: string;
  symbol: string;
  entryDate: string;
  exitDate?: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  exitPrice?: number;
  commission: number;
  slippage: number;
  pnl?: number;
  pnlPercent?: number;
  holdingPeriod?: number;
  exitReason?: 'signal' | 'stop_loss' | 'take_profit' | 'time_limit';
}

// ============================================================================
// Equity Curve
// ============================================================================

export interface EquityPoint {
  date: string;
  equity: number;
  returns: number;
  drawdown: number;
  benchmark?: number;
}

// ============================================================================
// Chart Data
// ============================================================================

export interface ChartData {
  type: 'equity' | 'drawdown' | 'returns' | 'exposure' | 'trades';
  title: string;
  data: any[];
  config?: Record<string, any>;
}

// ============================================================================
// Optimization
// ============================================================================

export interface OptimizationRequest {
  strategy: any; // StrategyDefinition
  parameterGrid: ParameterGrid[];
  optimizationTarget?: OptimizationObjective;
  constraints?: Record<string, any>;
  parameterRanges?: Record<string, ParameterGrid>;
  objective: OptimizationObjective;
  config: BacktestConfig;
  startDate?: string;
  endDate?: string;
  initialCapital?: number;
  assets?: AssetSelection[];
  parameters?: Record<string, any>;
  maxIterations?: number;
  algorithm?: 'grid' | 'random' | 'genetic' | 'bayesian';
}

export interface ParameterGrid {
  name: string;
  min: number;
  max: number;
  step: number;
  values?: any[];
}

export type OptimizationObjective =
  | 'sharpe_ratio'
  | 'total_return'
  | 'profit_factor'
  | 'max_drawdown'
  | 'calmar_ratio'
  | 'sortino_ratio';

export interface OptimizationResult {
  id: string;
  status: 'completed' | 'failed' | 'running';
  bestParameters: Record<string, any>;
  bestResult: BacktestResult;
  allResults: OptimizationIteration[];
  iterations: number;
  duration: number;
  error?: string;
}

export interface OptimizationIteration {
  iteration: number;
  parameters: Record<string, any>;
  performance: PerformanceMetrics;
  objectiveValue: number;
}

// ============================================================================
// Historical Data
// ============================================================================

export interface DataRequest {
  symbols: string[];
  startDate: string;
  endDate: string;
  timeframe: Timeframe;
  fields?: string[];
}

export interface HistoricalData {
  symbol: string;
  timeframe: Timeframe;
  data: PriceBar[];
}

export interface PriceBar {
  date: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  adjustedClose?: number;
}

// ============================================================================
// Data Catalog
// ============================================================================

export interface DataCatalog {
  providers?: string[];
  markets?: Array<{
    id: string;
    name: string;
    assetClasses: string[];
  }>;
  assetClasses?: AssetClass[];
  timeframes: Timeframe[];
  dateRange?: {
    start: string;
    end: string;
  };
  startDate?: string;
  endDate?: string;
  totalSymbols?: number;
  dataSize?: string;
  description?: string;
}

// ============================================================================
// Indicators
// ============================================================================

export type IndicatorType =
  | 'sma' | 'ema' | 'rsi' | 'macd' | 'bollinger_bands'
  | 'atr' | 'adx' | 'stochastic' | 'cci' | 'roc'
  | 'williams_r' | 'momentum' | 'volume';

export interface IndicatorParams {
  period?: number;
  fastPeriod?: number;
  slowPeriod?: number;
  signalPeriod?: number;
  stdDev?: number;
  [key: string]: any;
}

export interface IndicatorResult {
  indicator: IndicatorType;
  symbol: string;
  values: IndicatorValue[];
}

export interface IndicatorValue {
  date: string;
  value: number | number[];
  signal?: number;
}

export interface IndicatorCatalog {
  categories?: Array<{
    name: string;
    indicators: Array<{
      id: string;
      name: string;
      params: string[];
      description?: string;
    }>;
  }>;
  indicators?: {
    type: IndicatorType;
    name: string;
    description: string;
    parameters: StrategyParameter[];
  }[];
  total?: number;
  description?: string;
}

// ============================================================================
// Live Trading
// ============================================================================

export interface LiveDeployRequest {
  strategy: any; // StrategyDefinition
  brokerage: string;
  initialCapital: number;
  assets: AssetSelection[];
  parameters?: Record<string, any>;
  riskLimits?: RiskLimits;
}

export interface RiskLimits {
  maxPositionSize?: number;
  maxDrawdown?: number;
  dailyLossLimit?: number;
  maxLeverage?: number;
}

export interface LiveDeployResult {
  id: string;
  status: 'deployed' | 'failed';
  deploymentTime: string;
  message: string;
  error?: string;
}

export interface LiveStatus {
  id: string;
  status: 'running' | 'stopped' | 'paused' | 'error';
  equity: number;
  pnl: number;
  openPositions: Position[];
  todayTrades: number;
  lastUpdate: string;
  error?: string;
}

export interface Position {
  symbol: string;
  quantity: number;
  side: 'long' | 'short';
  entryPrice: number;
  currentPrice: number;
  unrealizedPnl: number;
  unrealizedPnlPercent: number;
}

// ============================================================================
// Research/Notebooks
// ============================================================================

export interface NotebookResult {
  id: string;
  name: string;
  url?: string;
  status: 'created' | 'failed';
  error?: string;
}

// ============================================================================
// Utility Types
// ============================================================================

export interface ExecutionResult {
  nodeId: string;
  success: boolean;
  data?: any;
  error?: string;
}

/**
 * Strategy Validation Result
 */
export interface ValidationResult {
  valid: boolean;
  errors: string[];
  warnings?: string[];
}

/**
 * Provider Health Status
 */
export interface HealthStatus {
  status: 'healthy' | 'degraded' | 'unhealthy';
  message?: string;
  lastCheck?: string;
}
