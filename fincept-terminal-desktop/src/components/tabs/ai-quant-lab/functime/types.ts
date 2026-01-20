/**
 * Functime Panel TypeScript Interfaces
 */

import type {
  ForecastResult,
  ForecastConfig,
  PreprocessConfig,
  FullPipelineResponse,
  AnomalyDetectionConfig,
  SeasonalityConfig,
  ConfidenceIntervalsConfig,
  BacktestConfig
} from '@/services/aiQuantLab/functimeService';
import type {
  PortfolioPriceData,
  PortfolioForecastResult,
  PortfolioAnomalyResult,
  PortfolioSeasonalityResult
} from '@/services/aiQuantLab/portfolioFunctimeService';
import type { Portfolio } from '@/services/portfolio/portfolioService';

// Data source types
export type DataSourceType = 'manual' | 'portfolio' | 'symbol';

// Analysis mode types
export type AnalysisMode = 'forecast' | 'anomaly' | 'seasonality' | 'backtest';

// Preprocess type
export type PreprocessType = 'none' | 'scale' | 'difference';

// Anomaly method type
export type AnomalyMethod = 'zscore' | 'iqr' | 'isolation_forest';

// Forecast analysis result interface
export interface ForecastAnalysisResult {
  forecast?: ForecastResult[];
  model?: string;
  horizon?: number;
  frequency?: string;
  best_params?: Record<string, unknown>;
  evaluation_metrics?: {
    mae?: number;
    rmse?: number;
    smape?: number;
  };
  data_summary?: {
    total_rows: number;
    entities: number;
    train_size: number;
    test_size: number;
  };
}

// Expandable sections state
export interface ExpandedSections {
  config: boolean;
  forecast: boolean;
  metrics: boolean;
  summary: boolean;
  chart: boolean;
  dataSource: boolean;
  anomaly: boolean;
  seasonality: boolean;
  backtest: boolean;
}

// Chart data point interface
export interface ChartDataPoint {
  time: string;
  value: number;
  type: 'historical' | 'forecast';
  entity_id?: string;
}

// Tooltip state interface
export interface TooltipState {
  visible: boolean;
  x: number;
  y: number;
  data: ChartDataPoint | null;
}

// Component props interfaces
export interface DataSourceSectionProps {
  dataSourceType: DataSourceType;
  setDataSourceType: (type: DataSourceType) => void;
  portfolios: Portfolio[];
  selectedPortfolioId: string;
  setSelectedPortfolioId: (id: string) => void;
  symbolInput: string;
  setSymbolInput: (symbol: string) => void;
  historicalDays: number;
  setHistoricalDays: (days: number) => void;
  priceDataLoading: boolean;
  priceData: PortfolioPriceData | null;
  loadDataFromSource: () => Promise<void>;
  expanded: boolean;
  toggleSection: () => void;
}

export interface ForecastConfigProps {
  selectedModel: string;
  setSelectedModel: (model: string) => void;
  horizon: number;
  setHorizon: (horizon: number) => void;
  frequency: string;
  setFrequency: (freq: string) => void;
  alpha: number;
  setAlpha: (alpha: number) => void;
  testSize: number;
  setTestSize: (size: number) => void;
  preprocess: PreprocessType;
  setPreprocess: (preprocess: PreprocessType) => void;
  expanded: boolean;
  toggleSection: () => void;
}

export interface AnomalyConfigProps {
  anomalyMethod: AnomalyMethod;
  setAnomalyMethod: (method: AnomalyMethod) => void;
  anomalyThreshold: number;
  setAnomalyThreshold: (threshold: number) => void;
}

export interface SeasonalityConfigProps {
  // Currently just informational panel, no configuration
}

export interface MetricCardProps {
  label: string;
  value: string;
  color: string;
  icon: React.ReactNode;
  large?: boolean;
}

export interface ForecastChartProps {
  historicalData: { time: string; value: number }[];
  forecastData: ForecastResult[];
  height?: number;
}

export interface AnomalyResultsProps {
  anomalyResult: PortfolioAnomalyResult;
}

export interface SeasonalityResultsProps {
  seasonalityResult: PortfolioSeasonalityResult;
}

export interface ForecastResultsProps {
  analysisResult: ForecastAnalysisResult;
  expandedSections: ExpandedSections;
  toggleSection: (section: keyof ExpandedSections) => void;
  historicalData: { time: string; value: number }[];
}

// Re-export service types for convenience
export type {
  ForecastResult,
  ForecastConfig,
  PreprocessConfig,
  FullPipelineResponse,
  AnomalyDetectionConfig,
  SeasonalityConfig,
  ConfidenceIntervalsConfig,
  BacktestConfig,
  PortfolioPriceData,
  PortfolioForecastResult,
  PortfolioAnomalyResult,
  PortfolioSeasonalityResult,
  Portfolio
};
