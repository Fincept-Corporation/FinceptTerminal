// Types for DBnomics Tab

export interface Provider {
  code: string;
  name: string;
}

export interface Dataset {
  code: string;
  name: string;
}

export interface Series {
  code: string;
  name: string;
  full_id: string;
}

export interface Observation {
  period: string;
  value: number;
}

export interface DataPoint {
  series_id: string;
  series_name: string;
  observations: Observation[];
  color: string;
}

export type ChartType = 'line' | 'bar' | 'area' | 'scatter' | 'candlestick';

export interface DBnomicsState {
  singleViewSeries: DataPoint[];
  slots: DataPoint[][];
  slotChartTypes: ChartType[];
  singleChartType: ChartType;
  activeView: 'single' | 'comparison';
  timestamp: number;
}

// Cache types
export interface CachedData<T> {
  data: T;
  timestamp: number;
}

export type ProvidersCache = CachedData<Provider[]>;
export type DatasetsCache = Map<string, CachedData<Dataset[]>>;
export type SeriesCache = Map<string, CachedData<Series[]>>;
