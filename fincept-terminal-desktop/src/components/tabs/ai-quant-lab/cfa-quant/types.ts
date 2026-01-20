/**
 * CFA Quant Panel Types
 * TypeScript interfaces and types
 */

import type { QuantAnalyticsResult } from '@/services/aiQuantLab/quantAnalyticsService';

export type Step = 'data' | 'analysis' | 'results';
export type DataSourceType = 'symbol' | 'manual';

export interface AnalysisConfig {
  id: string;
  name: string;
  shortName: string;
  description: string;
  icon: React.ElementType;
  category: 'time_series' | 'machine_learning' | 'sampling' | 'data_quality';
  color: string;
  minDataPoints: number;
  helpText: string;
}

export interface DataStats {
  min: number;
  max: number;
  mean: number;
  std: number;
  change: number;
  changePercent: number;
}

export interface ChartDataPoint {
  index: number;
  value: number;
  date: string;
}

export interface ZoomState {
  startIndex: number;
  endIndex: number;
}

export interface KeyMetric {
  label: string;
  value: string;
  color: string;
  icon?: React.ElementType;
}

// Re-export from service
export type { QuantAnalyticsResult };

// Props interfaces for components
export interface DataStepProps {
  dataSourceType: DataSourceType;
  setDataSourceType: (type: DataSourceType) => void;
  symbolInput: string;
  setSymbolInput: (value: string) => void;
  historicalDays: number;
  setHistoricalDays: (value: number) => void;
  manualData: string;
  setManualData: (value: string) => void;
  priceData: number[];
  priceDates: string[];
  priceDataLoading: boolean;
  dataStats: DataStats | null;
  error: string | null;
  chartData: ChartDataPoint[];
  zoomedChartData: ChartDataPoint[];
  dataChartZoom: ZoomState | null;
  setDataChartZoom: (zoom: ZoomState | null) => void;
  fetchSymbolData: () => Promise<void>;
  setCurrentStep: (step: Step) => void;
  getDateForIndex: (index: number) => string;
  handleWheel: (e: React.WheelEvent, chartType: 'data' | 'results') => void;
  handleMouseDown: (e: any, chartType: 'data' | 'results') => void;
  handleMouseMove: (e: any) => void;
  handleMouseUp: () => void;
  isSelecting: boolean;
  activeChart: 'data' | 'results' | null;
  refAreaLeft: number | null;
  refAreaRight: number | null;
}

export interface AnalysisStepProps {
  selectedAnalysis: string;
  setSelectedAnalysis: (id: string) => void;
  priceData: number[];
  isLoading: boolean;
  error: string | null;
  runAnalysis: () => Promise<void>;
  setCurrentStep: (step: Step) => void;
  zoomedChartData: ChartDataPoint[];
  dataChartZoom: ZoomState | null;
  setDataChartZoom: (zoom: ZoomState | null) => void;
}

export interface ResultsStepProps {
  analysisResult: QuantAnalyticsResult<Record<string, unknown>> | null;
  selectedAnalysis: string;
  symbolInput: string;
  priceData: number[];
  priceDates: string[];
  chartData: ChartDataPoint[];
  setCurrentStep: (step: Step) => void;
  setAnalysisResult: (result: QuantAnalyticsResult<Record<string, unknown>> | null) => void;
  resultsChartZoom: ZoomState | null;
  setResultsChartZoom: (zoom: ZoomState | null) => void;
  getDateForIndex: (index: number) => string;
  handleWheel: (e: React.WheelEvent, chartType: 'data' | 'results') => void;
  handleMouseDown: (e: any, chartType: 'data' | 'results') => void;
  handleMouseMove: (e: any) => void;
  handleMouseUp: () => void;
  isSelecting: boolean;
  activeChart: 'data' | 'results' | null;
  refAreaLeft: number | null;
  refAreaRight: number | null;
}

export interface ZoomControlsProps {
  chartType: 'data' | 'results';
  isZoomed: boolean;
  onZoomIn: (chartType: 'data' | 'results') => void;
  onZoomOut: (chartType: 'data' | 'results') => void;
  onResetZoom: (chartType: 'data' | 'results') => void;
  onPanLeft: (chartType: 'data' | 'results') => void;
  onPanRight: (chartType: 'data' | 'results') => void;
  canPanLeft: boolean;
  canPanRight: boolean;
}

export interface HeaderProps {
  currentStep: Step;
  setCurrentStep: (step: Step) => void;
  analysisResult: QuantAnalyticsResult<Record<string, unknown>> | null;
  dataSourceType: DataSourceType;
  symbolInput: string;
  dataStats: DataStats | null;
}
