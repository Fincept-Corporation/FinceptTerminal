/**
 * Surface Analytics - Type Definitions
 * Professional-grade 3D financial analytics types
 */

// Chart types available
export type ChartType = 'volatility' | 'correlation' | 'yield-curve' | 'pca';

// Data source configuration
export interface DataSourceConfig {
  apiKey: string;
  dataset: string;
  schema: string;
}

// Volatility Surface Types
export interface VolatilitySurfacePoint {
  strike: number;
  expiration: number; // Days to expiry
  impliedVol: number;
  delta?: number;
  gamma?: number;
  vega?: number;
  theta?: number;
}

export interface VolatilitySurfaceData {
  strikes: number[];
  expirations: number[];
  z: number[][]; // IV matrix
  underlying: string;
  spotPrice: number;
  timestamp: number;
}

// Correlation Matrix Types
export interface CorrelationMatrixData {
  assets: string[];
  timePoints: number[];
  z: number[][]; // Correlation values over time
  window: number; // Rolling window in days
  timestamp: number;
}

// Yield Curve Types
export interface YieldCurveData {
  maturities: number[]; // In months
  timePoints: number[]; // Historical dates
  z: number[][]; // Yield values
  source: string;
  timestamp: number;
}

// PCA Types
export interface PCAData {
  factors: string[];
  assets: string[];
  z: number[][]; // Factor loadings
  varianceExplained: number[];
  timestamp: number;
}

// Options Chain from Databento
export interface OptionsChainRecord {
  ts_recv: number;
  raw_symbol: string;
  expiration: number;
  instrument_class: 'C' | 'P'; // Call or Put
  strike_price: number;
  underlying: string;
  instrument_id: number;
  bid_px?: number;
  ask_px?: number;
  mid_price?: number;
  implied_vol?: number;
}

// Historical Bar Data
export interface HistoricalBar {
  ts_event: number;
  symbol: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

// Chart Metrics for display panels
export interface ChartMetric {
  label: string;
  value: string;
  change: string;
  positive: boolean | null;
}

// Loading and Error States
export interface DataState<T> {
  data: T | null;
  loading: boolean;
  error: string | null;
  lastUpdate: Date | null;
}

// Available OHLCV timeframes from Databento
export type OHLCVTimeframe = 'ohlcv-1d' | 'ohlcv-1h' | 'ohlcv-1m' | 'ohlcv-1s' | 'ohlcv-eod';

// Timeframe metadata for UI display
export interface TimeframeInfo {
  value: OHLCVTimeframe;
  label: string;
  description: string;
  costLevel: 'low' | 'medium' | 'high';
  recordsPerDay: string; // Approximate for display
}

// User Configuration
export interface SurfaceAnalyticsConfig {
  selectedSymbol: string;
  correlationAssets: string[];
  dateRange: {
    start: string;
    end: string;
  };
  refreshInterval: number; // milliseconds, 0 = manual
  timeframe: OHLCVTimeframe; // OHLCV timeframe selection
  dataset: string; // Selected dataset
}

// API Response wrapper
export interface DatabentoResponse<T = any> {
  error: boolean;
  message?: string;
  data?: T;
  symbol?: string;
  records?: number;
  timestamp: number;
}
