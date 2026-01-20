import { useState, useCallback, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { sqliteService } from '@/services/core/sqliteService';
import type { FREDSeries, SearchResult, Category, SeriesCache, CachedData } from '../types';
import { CACHE_DURATION } from '../constants';

interface UseScreenerDataReturn {
  // Data
  data: FREDSeries[];
  searchResults: SearchResult[];
  categories: Category[];
  categorySeriesResults: SearchResult[];

  // Loading states
  loading: boolean;
  loadingMessage: string;
  searchLoading: boolean;
  categoryLoading: boolean;
  categorySeriesLoading: boolean;

  // Error
  error: string | null;

  // API key
  apiKeyConfigured: boolean;

  // Actions
  fetchData: (seriesIds: string, startDate: string, endDate: string) => Promise<void>;
  searchSeries: (query: string) => Promise<void>;
  loadCategories: (categoryId?: number) => Promise<void>;
  loadCategorySeries: (categoryId: number) => Promise<void>;
  checkApiKey: () => Promise<void>;
  clearError: () => void;
}

export function useScreenerData(): UseScreenerDataReturn {
  const [data, setData] = useState<FREDSeries[]>([]);
  const [searchResults, setSearchResults] = useState<SearchResult[]>([]);
  const [categories, setCategories] = useState<Category[]>([]);
  const [categorySeriesResults, setCategorySeriesResults] = useState<SearchResult[]>([]);

  const [loading, setLoading] = useState(false);
  const [loadingMessage, setLoadingMessage] = useState('');
  const [searchLoading, setSearchLoading] = useState(false);
  const [categoryLoading, setCategoryLoading] = useState(false);
  const [categorySeriesLoading, setCategorySeriesLoading] = useState(false);

  const [error, setError] = useState<string | null>(null);
  const [apiKeyConfigured, setApiKeyConfigured] = useState(false);

  // Cache for series data
  const seriesCache = useRef<SeriesCache>(new Map());

  const isCacheValid = useCallback((cached: CachedData<FREDSeries> | undefined): boolean => {
    if (!cached) return false;
    return Date.now() - cached.timestamp < CACHE_DURATION;
  }, []);

  const checkApiKey = useCallback(async () => {
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      setApiKeyConfigured(!!apiKey);
    } catch (err) {
      console.error('[ScreenerData] Failed to check FRED API key:', err);
      setApiKeyConfigured(false);
    }
  }, []);

  const clearError = useCallback(() => {
    setError(null);
  }, []);

  const fetchData = useCallback(async (seriesIds: string, startDate: string, endDate: string) => {
    setLoading(true);
    setError(null);
    setLoadingMessage('Initializing...');

    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) {
        setError('FRED API key not configured. Please add it in Settings > Credentials.');
        setApiKeyConfigured(false);
        return;
      }

      // Remove duplicates and trim
      const ids = [...new Set(seriesIds.split(',').map(s => s.trim()).filter(Boolean))];

      if (ids.length === 0) {
        setError('Please enter at least one series ID');
        return;
      }

      // Check cache for already loaded series
      const cachedSeries: FREDSeries[] = [];
      const idsToFetch: string[] = [];

      for (const id of ids) {
        const cacheKey = `${id}-${startDate}-${endDate}`;
        const cached = seriesCache.current.get(cacheKey);
        if (isCacheValid(cached)) {
          cachedSeries.push(cached!.data);
        } else {
          idsToFetch.push(id);
        }
      }

      if (idsToFetch.length === 0) {
        // All data is cached
        setData(cachedSeries);
        setLoadingMessage('Loaded from cache!');
        setTimeout(() => setLoadingMessage(''), 1000);
        return;
      }

      setLoadingMessage(`Fetching ${idsToFetch.length} series from FRED API...`);
      const args = ['multiple', ...idsToFetch, startDate, endDate];

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args,
        env: { FRED_API_KEY: apiKey }
      });

      setLoadingMessage('Processing data...');
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(`API Error: ${parsed.error}`);
      } else if (Array.isArray(parsed)) {
        const errorSeries = parsed.find((s: FREDSeries) => s.error);
        if (errorSeries) {
          setError(`Failed to fetch ${errorSeries.series_id}: ${errorSeries.error}`);
        } else {
          // Cache the new data
          for (const series of parsed) {
            const cacheKey = `${series.series_id}-${startDate}-${endDate}`;
            seriesCache.current.set(cacheKey, {
              data: series,
              timestamp: Date.now()
            });
          }

          // Combine cached and new data, maintaining order
          const allData = ids.map(id => {
            const cached = cachedSeries.find(s => s.series_id === id);
            if (cached) return cached;
            return parsed.find((s: FREDSeries) => s.series_id === id);
          }).filter(Boolean) as FREDSeries[];

          setData(allData);
          setError(null);
          setLoadingMessage(`Successfully loaded ${allData.length} series!`);
          setTimeout(() => setLoadingMessage(''), 2000);
        }
      } else {
        setData(parsed);
      }
    } catch (err: any) {
      setError(err.message || 'Failed to fetch data');
    } finally {
      setLoading(false);
    }
  }, [isCacheValid]);

  const searchSeries = useCallback(async (query: string) => {
    if (!query.trim()) {
      setSearchResults([]);
      return;
    }

    setSearchLoading(true);
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) return;

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args: ['search', query, '50'],
        env: { FRED_API_KEY: apiKey }
      });

      const parsed = JSON.parse(result);
      if (!parsed.error && parsed.series) {
        setSearchResults(parsed.series);
      } else {
        setSearchResults([]);
      }
    } catch (err) {
      setSearchResults([]);
    } finally {
      setSearchLoading(false);
    }
  }, []);

  const loadCategories = useCallback(async (categoryId: number = 0) => {
    setCategoryLoading(true);
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) return;

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args: ['categories', categoryId.toString()],
        env: { FRED_API_KEY: apiKey }
      });

      const parsed = JSON.parse(result);
      if (!parsed.error && parsed.categories) {
        setCategories(parsed.categories);
      } else {
        setCategories([]);
      }
    } catch (err) {
      setCategories([]);
    } finally {
      setCategoryLoading(false);
    }
  }, []);

  const loadCategorySeries = useCallback(async (categoryId: number) => {
    setCategorySeriesLoading(true);
    try {
      const apiKey = await sqliteService.getApiKey('FRED');
      if (!apiKey) {
        setCategorySeriesLoading(false);
        return;
      }

      const result = await invoke<string>('execute_python_script', {
        scriptName: 'fred_data.py',
        args: ['category_series', categoryId.toString(), '50'],
        env: { FRED_API_KEY: apiKey }
      });

      const parsed = JSON.parse(result);
      if (!parsed.error && parsed.series) {
        setCategorySeriesResults(parsed.series);
      } else {
        setCategorySeriesResults([]);
      }
    } catch (err) {
      setCategorySeriesResults([]);
    } finally {
      setCategorySeriesLoading(false);
    }
  }, []);

  return {
    data,
    searchResults,
    categories,
    categorySeriesResults,
    loading,
    loadingMessage,
    searchLoading,
    categoryLoading,
    categorySeriesLoading,
    error,
    apiKeyConfigured,
    fetchData,
    searchSeries,
    loadCategories,
    loadCategorySeries,
    checkApiKey,
    clearError,
  };
}
