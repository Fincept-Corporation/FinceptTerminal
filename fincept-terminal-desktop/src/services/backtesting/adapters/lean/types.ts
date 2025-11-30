/**
 * Lean-Specific Types
 *
 * Type definitions specific to QuantConnect Lean integration.
 * These types map to Lean's CLI arguments, configuration, and response formats.
 */

import { ProviderCapabilities, AssetClass, Timeframe } from '../../interfaces/types';

/**
 * Lean CLI Configuration
 * Maps to Lean CLI arguments and config.json settings
 */
export interface LeanConfig {
  // Paths
  leanCliPath: string;                    // Path to lean executable
  projectsPath: string;                   // Path to Lean projects directory
  dataPath: string;                       // Path to Lean data directory
  resultsPath: string;                    // Path to results output

  // API Configuration (optional - for cloud features)
  apiUrl?: string;                        // QuantConnect API URL
  apiToken?: string;                      // User API token
  organizationId?: string;                // Organization ID

  // Environment
  environment: 'backtesting' | 'live' | 'optimization' | 'research';

  // Advanced Settings
  maxConcurrentBacktests?: number;        // Max parallel backtests
  pythonPath?: string;                    // Custom Python interpreter path
  enableLogging?: boolean;                // Enable detailed logging
  logLevel?: 'debug' | 'info' | 'warn' | 'error';

  // Data Settings
  dataProvider?: 'local' | 'quantconnect' | 'iqfeed' | 'interactive-brokers';
  resolution?: Timeframe;
}

/**
 * Lean Project Structure
 * Represents a Lean algorithm project
 */
export interface LeanProject {
  name: string;
  description: string;
  created: string;
  modified: string;

  // Files
  mainFile: string;                       // main.py
  configFile?: string;                    // config.json
  requirementsFile?: string;              // requirements.txt

  // Settings
  language: 'Python';                     // We only support Python
  parameters?: Record<string, any>;       // Algorithm parameters
}

/**
 * Lean CLI Arguments
 * Command-line arguments for running Lean
 */
export interface LeanCliArgs {
  // Required
  algorithm: string;                      // Algorithm file path

  // Backtest Settings
  startDate?: string;                     // YYYYMMDD format
  endDate?: string;                       // YYYYMMDD format
  cash?: number;                          // Initial capital

  // Data Settings
  dataFolder?: string;
  resultsFolder?: string;

  // Advanced
  parameters?: Record<string, any>;       // Algorithm parameters as JSON
  environment?: string;                   // Environment name

  // Optimization Settings
  optimizationConfig?: any;               // Optimization configuration

  // Flags
  release?: boolean;                      // Release mode
  update?: boolean;                       // Update data
}

/**
 * Lean Backtest Result (Raw from Lean)
 * Structure returned by Lean CLI after backtest
 */
export interface LeanRawBacktestResult {
  // Run Information
  AlgorithmId?: string;

  // Statistics
  Statistics?: Array<{
    Key: string;
    Value: string;
  }>;

  // Performance
  TotalPerformance?: {
    PortfolioStatistics?: {
      TotalReturn?: string;
      AnnualReturn?: string;
      AnnualStandardDeviation?: string;
      SharpeRatio?: string;
      SortinoRatio?: string;
      MaxDrawdown?: string;
      Alpha?: string;
      Beta?: string;
      InformationRatio?: string;
      TreynorRatio?: string;
      // ... more statistics
    };
    TradeStatistics?: {
      TotalNumberOfTrades?: number;
      WinRate?: string;
      LossRate?: string;
      AverageWin?: string;
      AverageLoss?: string;
      LargestWin?: string;
      LargestLoss?: string;
      AverageProfit?: string;
      ProfitLossRatio?: string;
      // ... more trade stats
    };
  };

  // Charts
  Charts?: Record<string, LeanChartSeries>;

  // Orders
  Orders?: Record<string, LeanOrder>;

  // Runtime Statistics
  RuntimeStatistics?: Array<{
    Key: string;
    Value: string;
  }>;

  // Alpha Runtime Statistics
  AlphaRuntimeStatistics?: any;

  // Logs
  Logs?: string[];

  // State (for live)
  State?: string;
}

/**
 * Lean Chart Series
 */
export interface LeanChartSeries {
  Name: string;
  ChartType: 'Stock' | 'Line' | 'Scatter' | 'Pie';
  Series: Record<string, {
    Name: string;
    Unit: string;
    Values: Array<{
      x: number;  // Unix timestamp
      y: number;
    }>;
  }>;
}

/**
 * Lean Order
 */
export interface LeanOrder {
  Id: number;
  Type: string;
  Status: string;
  Time: string;
  Symbol: string;
  Quantity: number;
  Price: number;
  Value: number;
  Tag: string;
}

/**
 * Lean Optimization Result (Raw)
 */
export interface LeanRawOptimizationResult {
  ParameterSet: Record<string, any>;
  Statistics: Array<{
    Key: string;
    Value: string;
  }>;
  BacktestId?: string;
}

/**
 * Lean Data Request Format
 * Maps to Lean's data download CLI
 */
export interface LeanDataRequest {
  tickers: string[];
  resolution: 'daily' | 'hour' | 'minute' | 'second' | 'tick';
  startDate: string;                      // YYYYMMDD
  endDate: string;                        // YYYYMMDD
  market?: string;                        // us-equity, forex, crypto, etc.
  securityType?: 'Equity' | 'Forex' | 'Crypto' | 'Option' | 'Future';
}

/**
 * Lean Indicator Configuration
 */
export interface LeanIndicatorConfig {
  name: string;                           // SMA, EMA, RSI, etc.
  period?: number;
  symbol: string;
  resolution: Timeframe;
  parameters?: Record<string, any>;
}

/**
 * Lean Capabilities (Static)
 * What Lean supports as a platform
 */
export const LEAN_CAPABILITIES: ProviderCapabilities = {
  backtesting: true,
  optimization: true,
  liveTrading: true,
  research: true,

  multiAsset: [
    'equity' as AssetClass,
    'forex' as AssetClass,
    'crypto' as AssetClass,
    'option' as AssetClass,
    'future' as AssetClass,
    'cfd' as AssetClass
  ],

  indicators: true,
  customStrategies: true,
  maxConcurrentBacktests: 5,

  supportedTimeframes: [
    'tick' as Timeframe,
    'second' as Timeframe,
    'minute' as Timeframe,
    'hour' as Timeframe,
    'daily' as Timeframe
  ],

  supportedMarkets: [
    'us-equity',
    'us-options',
    'forex',
    'crypto',
    'futures',
    'cfd'
  ]
};

/**
 * Lean Error Response
 */
export interface LeanError {
  message: string;
  stackTrace?: string;
  type?: string;
  timestamp: string;
}

/**
 * Lean Process Status
 */
export interface LeanProcessStatus {
  pid?: number;
  status: 'idle' | 'running' | 'completed' | 'failed';
  progress?: number;                      // 0-100
  currentDate?: string;                   // Current backtest date
  logs: string[];
  error?: string;
}

/**
 * Lean Strategy Template
 * Pre-built strategy templates
 */
export interface LeanStrategyTemplate {
  id: string;
  name: string;
  description: string;
  category: 'momentum' | 'mean-reversion' | 'arbitrage' | 'ml' | 'custom';
  pythonCode: string;
  parameters: Array<{
    name: string;
    type: 'number' | 'string' | 'boolean';
    default: any;
    min?: number;
    max?: number;
    options?: any[];
  }>;
  requiredData: {
    symbols: string[];
    resolution: Timeframe;
    assetClass: AssetClass;
  };
}

/**
 * Lean Config.json Structure
 * The configuration file Lean reads
 */
export interface LeanConfigJson {
  'algorithm-type-name': string;
  'algorithm-language': 'Python';
  'algorithm-location': string;

  'data-folder': string;
  'data-provider': string;

  'job-user-id': string;
  'api-access-token': string;

  'parameters': Record<string, string>;

  'debugging': boolean;
  'debugging-method': string;

  'log-handler': string;
  'messaging-handler': string;
  'job-queue-handler': string;
  'api-handler': string;

  'result-handler': string;
  'map-file-provider': string;
  'factor-file-provider': string;
  'data-downloader': string;

  'environments': {
    backtesting: any;
    live: any;
  };
}
