// Types for Screener Tab

export interface SeriesData {
  date: string;
  value: number;
}

export interface FREDSeries {
  series_id: string;
  title: string;
  units: string;
  frequency: string;
  seasonal_adjustment: string;
  last_updated: string;
  observations: SeriesData[];
  observation_count: number;
  error?: string;
}

export interface SearchResult {
  id: string;
  title: string;
  frequency: string;
  units: string;
  seasonal_adjustment: string;
  last_updated: string;
  popularity: number;
}

export interface Category {
  id: number;
  name: string;
  parent_id: number;
}

export interface CategoryPath {
  id: number;
  name: string;
}

export interface PopularSeries {
  id: string;
  name: string;
  cat: string;
}

export interface PopularCategory {
  id: number;
  name: string;
  desc: string;
}

// Cache types
export interface CachedData<T> {
  data: T;
  timestamp: number;
}

export type SeriesCache = Map<string, CachedData<FREDSeries>>;
