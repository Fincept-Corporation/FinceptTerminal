/**
 * FFN Analytics Types
 * TypeScript interfaces and types
 */

import type {
  PerformanceMetrics,
  DrawdownInfo,
  FFNConfig,
  AssetComparisonResponse,
  PortfolioOptimizationResponse,
  BenchmarkComparisonResponse
} from '@/services/aiQuantLab/ffnService';
import type { Portfolio } from '@/services/portfolio/portfolioService';
import type { ReactNode } from 'react';

// Data source types
export type DataSourceType = 'manual' | 'portfolio' | 'symbol';

// Analysis mode types
export type AnalysisMode = 'performance' | 'comparison' | 'monthly' | 'rolling' | 'optimize' | 'benchmark';

// Optimization method types
export type OptimizationMethod = 'erc' | 'inv_vol' | 'mean_var' | 'equal';

// Risk metrics interface
export interface RiskMetrics {
  ulcer_index?: number;
  skewness?: number;
  kurtosis?: number;
  var_95?: number;
  cvar_95?: number;
}

// Rolling metrics interface
export interface RollingMetrics {
  rolling_sharpe?: Record<string, number>;
  rolling_volatility?: Record<string, number>;
}

// Drawdowns result interface
export interface DrawdownsResult {
  max_drawdown: number;
  top_drawdowns: DrawdownInfo[];
}

// Monthly returns type
export type MonthlyReturns = Record<string, Record<string, number | null>>;

// Main analysis result interface
export interface AnalysisResult {
  performance?: PerformanceMetrics;
  drawdowns?: DrawdownsResult;
  riskMetrics?: RiskMetrics;
  monthlyReturns?: MonthlyReturns;
  rollingMetrics?: RollingMetrics;
  comparison?: AssetComparisonResponse;
  optimization?: PortfolioOptimizationResponse;
  benchmark?: BenchmarkComparisonResponse;
}

// Expanded sections state
export interface ExpandedSections {
  performance: boolean;
  drawdowns: boolean;
  risk: boolean;
  monthly: boolean;
  rolling: boolean;
  comparison: boolean;
  optimization: boolean;
  benchmark: boolean;
  dataSource: boolean;
}

// FFN State interface - returned by useFFFNState hook
export interface FFNState {
  // Status
  isFFNAvailable: boolean | null;
  isLoading: boolean;
  error: string | null;

  // Data
  priceInput: string;
  setPriceInput: (value: string) => void;
  analysisResult: AnalysisResult | null;

  // Data source
  dataSourceType: DataSourceType;
  setDataSourceType: (type: DataSourceType) => void;
  portfolios: Portfolio[];
  selectedPortfolioId: string;
  setSelectedPortfolioId: (id: string) => void;
  symbolInput: string;
  setSymbolInput: (symbol: string) => void;
  symbolsInput: string;
  setSymbolsInput: (symbols: string) => void;
  historicalDays: number;
  setHistoricalDays: (days: number) => void;
  priceDataLoading: boolean;

  // Analysis mode
  analysisMode: AnalysisMode;
  setAnalysisMode: (mode: AnalysisMode) => void;

  // Optimization
  optimizationMethod: OptimizationMethod;
  setOptimizationMethod: (method: OptimizationMethod) => void;

  // Benchmark
  benchmarkSymbol: string;
  setBenchmarkSymbol: (symbol: string) => void;

  // Config
  config: FFNConfig;
  setConfig: (config: FFNConfig) => void;

  // UI state
  expandedSections: ExpandedSections;
  toggleSection: (section: keyof ExpandedSections) => void;

  // Actions
  checkFFNStatus: () => Promise<void>;
  fetchData: () => Promise<void>;
  runAnalysis: () => Promise<void>;
}

// Component Props

// MetricCard props
export interface MetricCardProps {
  label: string;
  value: string;
  color: string;
  icon: ReactNode;
  large?: boolean;
}

// DataSourceSection props
export interface DataSourceSectionProps {
  dataSourceType: DataSourceType;
  setDataSourceType: (type: DataSourceType) => void;
  portfolios: Portfolio[];
  selectedPortfolioId: string;
  setSelectedPortfolioId: (id: string) => void;
  symbolInput: string;
  setSymbolInput: (symbol: string) => void;
  symbolsInput: string;
  setSymbolsInput: (symbols: string) => void;
  analysisMode: AnalysisMode;
  historicalDays: number;
  setHistoricalDays: (days: number) => void;
  priceDataLoading: boolean;
  fetchData: () => void;
  expanded: boolean;
  toggleSection: () => void;
}

// AnalysisModeSelector props
export interface AnalysisModeSelectorProps {
  analysisMode: AnalysisMode;
  setAnalysisMode: (mode: AnalysisMode) => void;
}

// OptimizationMethodSelector props
export interface OptimizationMethodSelectorProps {
  optimizationMethod: OptimizationMethod;
  setOptimizationMethod: (method: OptimizationMethod) => void;
}

// BenchmarkInput props
export interface BenchmarkInputProps {
  benchmarkSymbol: string;
  setBenchmarkSymbol: (symbol: string) => void;
}

// ConfigSection props
export interface ConfigSectionProps {
  config: FFNConfig;
  setConfig: (config: FFNConfig) => void;
}

// PriceInputSection props
export interface PriceInputSectionProps {
  priceInput: string;
  setPriceInput: (value: string) => void;
}

// Result section props
export interface PerformanceResultsProps {
  performance: PerformanceMetrics;
  expanded: boolean;
  toggleSection: () => void;
}

export interface DrawdownResultsProps {
  drawdowns: DrawdownsResult;
  expanded: boolean;
  toggleSection: () => void;
}

export interface RiskResultsProps {
  riskMetrics: RiskMetrics;
  expanded: boolean;
  toggleSection: () => void;
}

export interface MonthlyResultsProps {
  monthlyReturns: MonthlyReturns;
  expanded: boolean;
  toggleSection: () => void;
}

export interface RollingResultsProps {
  rollingMetrics: RollingMetrics;
  expanded: boolean;
  toggleSection: () => void;
}

export interface ComparisonResultsProps {
  comparison: AssetComparisonResponse;
  expanded: boolean;
  toggleSection: () => void;
}

export interface OptimizationResultsProps {
  optimization: PortfolioOptimizationResponse;
  expanded: boolean;
  toggleSection: () => void;
}

export interface BenchmarkResultsProps {
  benchmark: BenchmarkComparisonResponse;
  expanded: boolean;
  toggleSection: () => void;
}
