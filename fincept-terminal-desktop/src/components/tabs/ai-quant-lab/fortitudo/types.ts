/**
 * Fortitudo Panel TypeScript Interfaces
 */

import type {
  PortfolioMetrics,
  OptionGreeksResponse,
  OptimizationResult,
  EfficientFrontierResponse,
  OptimizationObjective
} from '@/services/aiQuantLab/fortitudoService';
import type { PortfolioReturnsData } from '@/services/aiQuantLab/portfolioFortitudoService';
import type { Portfolio, PortfolioSummary } from '@/services/portfolio/portfolioService';

// Analysis mode types
export type AnalysisMode = 'portfolio' | 'options' | 'entropy' | 'optimization';
export type DataSourceType = 'sample' | 'portfolio';
export type OptimizationType = 'mv' | 'cvar';

// Analysis result interface
export interface AnalysisResult {
  metrics_equal_weight?: PortfolioMetrics;
  metrics_exp_decay?: PortfolioMetrics;
  n_scenarios?: number;
  n_assets?: number;
  assets?: string[];
  half_life?: number;
  alpha?: number;
}

// Expandable sections state
export interface ExpandedSections {
  config: boolean;
  metrics: boolean;
  comparison: boolean;
  matrix: boolean;
}

// Normalized weight item
export interface NormalizedWeight {
  asset: string;
  weight: number;
}

// Main panel state
export interface FortitudoPanelState {
  // Status
  isFortitudoAvailable: boolean | null;
  isLoading: boolean;
  error: string | null;
  activeMode: AnalysisMode;

  // Portfolio config
  weights: string;
  alpha: number;
  halfLife: number;
  analysisResult: AnalysisResult | null;

  // Option pricing config
  spotPrice: number;
  strike: number;
  volatility: number;
  riskFreeRate: number;
  dividendYield: number;
  timeToMaturity: number;
  optionResult: any;
  greeksResult: OptionGreeksResponse | null;

  // Entropy pooling config
  nScenarios: number;
  maxProbability: number;
  entropyResult: any;

  // Optimization config
  optimizationObjective: OptimizationObjective;
  optimizationType: OptimizationType;
  optLongOnly: boolean;
  optMaxWeight: number;
  optMinWeight: number;
  optTargetReturn: number;
  optAlpha: number;
  optimizationResult: OptimizationResult | null;
  frontierResult: EfficientFrontierResponse | null;
  nFrontierPoints: number;
  selectedFrontierIndex: number | null;

  // User portfolio
  userPortfolios: Portfolio[];
  selectedPortfolioId: string | null;
  selectedPortfolioSummary: PortfolioSummary | null;
  portfolioReturnsData: PortfolioReturnsData | null;
  dataSource: DataSourceType;
  isLoadingPortfolio: boolean;
  historicalDays: number;

  // UI state
  expandedSections: ExpandedSections;
}

// Component props interfaces
export interface MetricRowProps {
  label: string;
  value: string;
  positive?: boolean;
  negative?: boolean;
}

export interface GreekRowProps {
  label: string;
  value: number | undefined;
  description: string;
}

export interface PortfolioRiskModeProps {
  weights: string;
  setWeights: (value: string) => void;
  alpha: number;
  setAlpha: (value: number) => void;
  halfLife: number;
  setHalfLife: (value: number) => void;
  isLoading: boolean;
  analysisResult: AnalysisResult | null;
  expandedSections: ExpandedSections;
  toggleSection: (section: keyof ExpandedSections) => void;
  runPortfolioAnalysis: () => Promise<void>;
}

export interface OptionPricingModeProps {
  spotPrice: number;
  setSpotPrice: (value: number) => void;
  strike: number;
  setStrike: (value: number) => void;
  volatility: number;
  setVolatility: (value: number) => void;
  riskFreeRate: number;
  setRiskFreeRate: (value: number) => void;
  dividendYield: number;
  setDividendYield: (value: number) => void;
  timeToMaturity: number;
  setTimeToMaturity: (value: number) => void;
  isLoading: boolean;
  optionResult: any;
  greeksResult: OptionGreeksResponse | null;
  runOptionPricing: () => Promise<void>;
  runOptionGreeks: () => Promise<void>;
}

export interface OptimizationModeProps {
  optimizationType: OptimizationType;
  setOptimizationType: (value: OptimizationType) => void;
  optimizationObjective: OptimizationObjective;
  setOptimizationObjective: (value: OptimizationObjective) => void;
  optMaxWeight: number;
  setOptMaxWeight: (value: number) => void;
  optTargetReturn: number;
  setOptTargetReturn: (value: number) => void;
  optAlpha: number;
  setOptAlpha: (value: number) => void;
  optLongOnly: boolean;
  setOptLongOnly: (value: boolean) => void;
  riskFreeRate: number;
  isLoading: boolean;
  optimizationResult: OptimizationResult | null;
  frontierResult: EfficientFrontierResponse | null;
  selectedFrontierIndex: number | null;
  setSelectedFrontierIndex: (value: number | null) => void;
  runOptimization: () => Promise<void>;
  runEfficientFrontier: () => Promise<void>;
}

export interface EntropyPoolingModeProps {
  nScenarios: number;
  setNScenarios: (value: number) => void;
  maxProbability: number;
  setMaxProbability: (value: number) => void;
  isLoading: boolean;
  entropyResult: any;
  runEntropyPooling: () => Promise<void>;
}

export interface DataSourceSelectorProps {
  dataSource: DataSourceType;
  setDataSource: (value: DataSourceType) => void;
  userPortfolios: Portfolio[];
  selectedPortfolioId: string | null;
  setSelectedPortfolioId: (value: string | null) => void;
  selectedPortfolioSummary: PortfolioSummary | null;
  portfolioReturnsData: PortfolioReturnsData | null;
  historicalDays: number;
  setHistoricalDays: (value: number) => void;
  isLoadingPortfolio: boolean;
  loadPortfolioData: (portfolioId: string) => Promise<void>;
}

// Re-export service types for convenience
export type {
  PortfolioMetrics,
  OptionGreeksResponse,
  OptimizationResult,
  EfficientFrontierResponse,
  OptimizationObjective
};
export type { PortfolioReturnsData };
export type { Portfolio, PortfolioSummary };
