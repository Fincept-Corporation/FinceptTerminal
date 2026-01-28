import type { ChartType } from './types';

export const DBNOMICS_API_BASE = 'https://api.db.nomics.world/v22';

// Cache duration in milliseconds (10 minutes for providers, 5 minutes for datasets/series)
export const PROVIDERS_CACHE_DURATION = 10 * 60 * 1000;
export const DATASETS_CACHE_DURATION = 5 * 60 * 1000;
export const SERIES_CACHE_DURATION = 5 * 60 * 1000;

// State persistence duration (24 hours)
export const STATE_CACHE_DURATION = 24 * 60 * 60 * 1000;

// Debounce delay for state saving
export const STATE_SAVE_DEBOUNCE_MS = 1000;

// Search debounce delay
export const SEARCH_DEBOUNCE_MS = 400;

// Chart types
export const CHART_TYPES: ChartType[] = ['line', 'bar', 'area', 'scatter', 'candlestick'];

// Pagination defaults
export const DATASETS_PAGE_SIZE = 50;
export const DATASETS_MAX_LIMIT = 500;
export const SERIES_PAGE_SIZE = 50;
export const SERIES_MAX_LIMIT = 1000;
export const SEARCH_PAGE_SIZE = 20;
export const SEARCH_MAX_LIMIT = 100;

// Default chart colors (will be overridden by theme)
export const DEFAULT_CHART_COLORS = [
  '#ea580c', // primary/orange
  '#3b82f6', // info/blue
  '#22c55e', // secondary/green
  '#eab308', // warning/yellow
  '#a855f7', // purple
  '#ec4899', // pink
  '#06b6d4', // cyan
  '#f97316', // orange variant
];
