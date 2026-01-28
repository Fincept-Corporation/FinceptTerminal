import { useState, useCallback, useRef, useEffect } from 'react';
import { getSetting, saveSetting } from '@/services/core/sqliteService';
import type {
  Provider,
  Dataset,
  Series,
  DataPoint,
  Observation,
  ChartType,
  DBnomicsState,
  ProvidersCache,
  DatasetsCache,
  SeriesCache,
  PaginationState,
  SearchResult,
} from '../types';
import {
  DBNOMICS_API_BASE,
  PROVIDERS_CACHE_DURATION,
  DATASETS_CACHE_DURATION,
  SERIES_CACHE_DURATION,
  STATE_CACHE_DURATION,
  STATE_SAVE_DEBOUNCE_MS,
  SEARCH_DEBOUNCE_MS,
  DEFAULT_CHART_COLORS,
  DATASETS_PAGE_SIZE,
  SERIES_PAGE_SIZE,
  SEARCH_PAGE_SIZE,
} from '../constants';

const DEFAULT_PAGINATION: PaginationState = {
  offset: 0,
  limit: 50,
  total: 0,
  hasMore: false,
};

export interface UseDBnomicsDataReturn {
  // Data
  providers: Provider[];
  datasets: Dataset[];
  seriesList: Series[];
  currentData: DataPoint | null;
  globalSearchResults: SearchResult[];

  // Selection state
  selectedProvider: string | null;
  selectedDataset: string | null;
  selectedSeries: string | null;

  // Search state
  providerSearch: string;
  datasetSearch: string;
  seriesSearch: string;
  globalSearch: string;

  // Pagination state
  datasetsPagination: PaginationState;
  seriesPagination: PaginationState;
  searchPagination: PaginationState;

  // View state
  slots: DataPoint[][];
  slotChartTypes: ChartType[];
  singleViewSeries: DataPoint[];
  singleChartType: ChartType;
  activeView: 'single' | 'comparison';

  // Loading states
  loading: boolean;
  status: string;

  // Colors for charts
  chartColors: string[];

  // Actions
  loadProviders: () => Promise<void>;
  selectProvider: (providerCode: string) => void;
  selectDataset: (datasetCode: string) => void;
  selectSeries: (series: Series) => void;
  refreshCurrentSeries: () => void;

  // Search actions
  setProviderSearch: (query: string) => void;
  setDatasetSearch: (query: string) => void;
  setSeriesSearch: (query: string) => void;
  setGlobalSearch: (query: string) => void;
  executeGlobalSearch: (query: string) => Promise<void>;
  selectSearchResult: (result: SearchResult) => void;

  // Pagination actions
  loadMoreDatasets: () => void;
  loadMoreSeries: () => void;
  loadMoreSearchResults: () => void;

  // Slot management
  addSlot: () => void;
  removeSlot: (index: number) => void;
  addToSlot: (slotIndex: number) => void;
  removeFromSlot: (slotIndex: number, seriesIndex: number) => void;
  setSlotChartType: (slotIndex: number, chartType: ChartType) => void;

  // Single view management
  addToSingleView: () => void;
  removeFromSingleView: (index: number) => void;
  setSingleChartType: (chartType: ChartType) => void;
  setActiveView: (view: 'single' | 'comparison') => void;

  // Actions
  clearAll: () => Promise<void>;
  exportData: () => void;
}

export function useDBnomicsData(themeColors?: string[]): UseDBnomicsDataReturn {
  // Chart colors from theme or defaults
  const chartColors = themeColors || DEFAULT_CHART_COLORS;

  // Data state
  const [providers, setProviders] = useState<Provider[]>([]);
  const [datasets, setDatasets] = useState<Dataset[]>([]);
  const [seriesList, setSeriesList] = useState<Series[]>([]);
  const [currentData, setCurrentData] = useState<DataPoint | null>(null);
  const [globalSearchResults, setGlobalSearchResults] = useState<SearchResult[]>([]);

  // Selection state
  const [selectedProvider, setSelectedProvider] = useState<string | null>(null);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);
  const [selectedSeries, setSelectedSeries] = useState<string | null>(null);

  // Search state
  const [providerSearch, setProviderSearchState] = useState('');
  const [datasetSearch, setDatasetSearchState] = useState('');
  const [seriesSearch, setSeriesSearchState] = useState('');
  const [globalSearch, setGlobalSearchState] = useState('');

  // Pagination state
  const [datasetsPagination, setDatasetsPagination] = useState<PaginationState>({ ...DEFAULT_PAGINATION, limit: DATASETS_PAGE_SIZE });
  const [seriesPagination, setSeriesPagination] = useState<PaginationState>({ ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE });
  const [searchPagination, setSearchPagination] = useState<PaginationState>({ ...DEFAULT_PAGINATION, limit: SEARCH_PAGE_SIZE });

  // View state
  const [slots, setSlots] = useState<DataPoint[][]>([[]]);
  const [slotChartTypes, setSlotChartTypes] = useState<ChartType[]>(['line']);
  const [singleViewSeries, setSingleViewSeries] = useState<DataPoint[]>([]);
  const [singleChartType, setSingleChartTypeState] = useState<ChartType>('line');
  const [activeView, setActiveViewState] = useState<'single' | 'comparison'>('single');

  // Loading state
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState('Loading providers...');

  // Caches
  const providersCache = useRef<ProvidersCache | null>(null);
  const datasetsCache = useRef<DatasetsCache>(new Map());
  const seriesCache = useRef<SeriesCache>(new Map());

  // Debounce refs
  const saveTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const searchDebounceRef = useRef<NodeJS.Timeout | null>(null);
  const datasetSearchDebounceRef = useRef<NodeJS.Timeout | null>(null);
  const seriesSearchDebounceRef = useRef<NodeJS.Timeout | null>(null);

  // Keep track of current provider/dataset for pagination
  const currentProviderRef = useRef<string | null>(null);
  const currentDatasetRef = useRef<string | null>(null);

  const isCacheValid = useCallback((timestamp: number, duration: number): boolean => {
    return Date.now() - timestamp < duration;
  }, []);

  // Save state with debouncing
  const saveState = useCallback(async () => {
    if (saveTimeoutRef.current) {
      clearTimeout(saveTimeoutRef.current);
    }

    saveTimeoutRef.current = setTimeout(async () => {
      try {
        const state: DBnomicsState = {
          singleViewSeries,
          slots,
          slotChartTypes,
          singleChartType,
          activeView,
          timestamp: Date.now(),
        };
        await saveSetting('dbnomics_state', JSON.stringify(state), 'dbnomics_tab');
      } catch (error) {
        console.error('[DBnomics] Failed to save state:', error);
      }
    }, STATE_SAVE_DEBOUNCE_MS);
  }, [singleViewSeries, slots, slotChartTypes, singleChartType, activeView]);

  // Auto-save state when it changes
  useEffect(() => {
    if (singleViewSeries.length > 0 || slots.some(s => s.length > 0)) {
      saveState();
    }
  }, [singleViewSeries, slots, slotChartTypes, singleChartType, activeView, saveState]);

  // Restore state on mount
  const restoreState = useCallback(async () => {
    try {
      const saved = await getSetting('dbnomics_state');
      if (saved) {
        const state: DBnomicsState = JSON.parse(saved);
        if (state.timestamp && isCacheValid(state.timestamp, STATE_CACHE_DURATION)) {
          if (state.singleViewSeries) setSingleViewSeries(state.singleViewSeries);
          if (state.slots) setSlots(state.slots);
          if (state.slotChartTypes) setSlotChartTypes(state.slotChartTypes);
          if (state.singleChartType) setSingleChartTypeState(state.singleChartType);
          if (state.activeView) setActiveViewState(state.activeView);
          setStatus('Restored from cache');
        }
      }
    } catch (error) {
      console.error('[DBnomics] Failed to restore state:', error);
    }
  }, [isCacheValid]);

  // ─── Load providers ───
  const loadProviders = useCallback(async () => {
    if (providersCache.current && isCacheValid(providersCache.current.timestamp, PROVIDERS_CACHE_DURATION)) {
      setProviders(providersCache.current.data);
      setStatus(`Loaded ${providersCache.current.data.length} providers (cached)`);
      return;
    }

    try {
      setLoading(true);
      setStatus('Loading providers...');
      const response = await fetch(`${DBNOMICS_API_BASE}/providers`);
      if (response.ok) {
        const data = await response.json();
        const providerList: Provider[] = (data?.providers?.docs || []).map((p: any) => ({
          code: p.code || '',
          name: p.name || p.code || '',
        }));
        providersCache.current = { data: providerList, timestamp: Date.now() };
        setProviders(providerList);
        setStatus(`Loaded ${providerList.length} providers`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, [isCacheValid]);

  // ─── Load datasets with pagination ───
  const loadDatasets = useCallback(async (providerCode: string, offset = 0, append = false) => {
    // Check cache only for first page with no search
    if (offset === 0 && !append) {
      const cached = datasetsCache.current.get(providerCode);
      if (cached && isCacheValid(cached.timestamp, DATASETS_CACHE_DURATION)) {
        setDatasets(cached.data);
        setSeriesList([]);
        setDatasetsPagination({
          offset: 0,
          limit: DATASETS_PAGE_SIZE,
          total: cached.data.length,
          hasMore: false, // cached = all loaded
        });
        setStatus(`Loaded ${cached.data.length} datasets (cached)`);
        return;
      }
    }

    try {
      setLoading(true);
      setStatus(append ? 'Loading more datasets...' : 'Loading datasets...');
      const response = await fetch(
        `${DBNOMICS_API_BASE}/datasets/${providerCode}?limit=${DATASETS_PAGE_SIZE}&offset=${offset}`
      );
      if (response.ok) {
        const data = await response.json();
        const totalNum = data?.datasets?.num_found ?? 0;
        const datasetList: Dataset[] = (data?.datasets?.docs || []).map((d: any) => ({
          code: d.code || '',
          name: d.name || d.code || '',
        }));

        const newOffset = offset + datasetList.length;

        if (append) {
          setDatasets(prev => [...prev, ...datasetList]);
        } else {
          setDatasets(datasetList);
          setSeriesList([]);
          // Cache only full first-page loads
          if (offset === 0 && totalNum <= DATASETS_PAGE_SIZE) {
            datasetsCache.current.set(providerCode, { data: datasetList, timestamp: Date.now() });
          }
        }

        setDatasetsPagination({
          offset: newOffset,
          limit: DATASETS_PAGE_SIZE,
          total: totalNum,
          hasMore: newOffset < totalNum,
        });
        setStatus(`Loaded ${append ? newOffset : datasetList.length} of ${totalNum} datasets`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, [isCacheValid]);

  // ─── Load series with pagination and search ───
  const loadSeries = useCallback(async (providerCode: string, datasetCode: string, offset = 0, append = false, query = '') => {
    const cacheKey = `${providerCode}/${datasetCode}`;

    // Check cache only for first page with no search
    if (offset === 0 && !append && !query) {
      const cached = seriesCache.current.get(cacheKey);
      if (cached && isCacheValid(cached.timestamp, SERIES_CACHE_DURATION)) {
        setSeriesList(cached.data);
        setSeriesPagination({
          offset: 0,
          limit: SERIES_PAGE_SIZE,
          total: cached.data.length,
          hasMore: false,
        });
        setStatus(`Loaded ${cached.data.length} series (cached)`);
        return;
      }
    }

    try {
      setLoading(true);
      setStatus(append ? 'Loading more series...' : 'Loading series...');
      let url = `${DBNOMICS_API_BASE}/series/${providerCode}/${datasetCode}?limit=${SERIES_PAGE_SIZE}&offset=${offset}&observations=false`;
      if (query) {
        url += `&q=${encodeURIComponent(query)}`;
      }
      const response = await fetch(url);
      if (response.ok) {
        const data = await response.json();
        const totalNum = data?.series?.num_found ?? 0;
        const seriesArray: Series[] = (data?.series?.docs || []).map((s: any) => ({
          code: s.series_code || '',
          name: s.series_name || s.series_code || '',
          full_id: `${providerCode}/${datasetCode}/${s.series_code}`,
        }));

        const newOffset = offset + seriesArray.length;

        if (append) {
          setSeriesList(prev => [...prev, ...seriesArray]);
        } else {
          setSeriesList(seriesArray);
          // Cache only full first-page loads with no search
          if (offset === 0 && !query && totalNum <= SERIES_PAGE_SIZE) {
            seriesCache.current.set(cacheKey, { data: seriesArray, timestamp: Date.now() });
          }
        }

        setSeriesPagination({
          offset: newOffset,
          limit: SERIES_PAGE_SIZE,
          total: totalNum,
          hasMore: newOffset < totalNum,
        });
        setStatus(`Loaded ${append ? newOffset : seriesArray.length} of ${totalNum} series`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, [isCacheValid]);

  // ─── Global search (searches across ALL of DBnomics) ───
  const executeGlobalSearch = useCallback(async (query: string, offset = 0, append = false) => {
    if (!query.trim()) {
      setGlobalSearchResults([]);
      setSearchPagination({ ...DEFAULT_PAGINATION, limit: SEARCH_PAGE_SIZE });
      return;
    }

    try {
      setLoading(true);
      setStatus(`Searching "${query}"...`);
      const response = await fetch(
        `${DBNOMICS_API_BASE}/search?q=${encodeURIComponent(query)}&limit=${SEARCH_PAGE_SIZE}&offset=${offset}`
      );
      if (response.ok) {
        const data = await response.json();
        const totalNum = data?.results?.num_found ?? data?.num_found ?? 0;
        const results: SearchResult[] = (data?.results?.docs || []).map((r: any) => ({
          provider_code: r.provider_code || '',
          provider_name: r.provider_name || r.provider_code || '',
          dataset_code: r.dataset_code || '',
          dataset_name: r.dataset_name || r.dataset_code || '',
          nb_series: r.nb_series || 0,
        }));

        const newOffset = offset + results.length;

        if (append) {
          setGlobalSearchResults(prev => [...prev, ...results]);
        } else {
          setGlobalSearchResults(results);
        }

        setSearchPagination({
          offset: newOffset,
          limit: SEARCH_PAGE_SIZE,
          total: totalNum,
          hasMore: newOffset < totalNum,
        });
        setStatus(`Found ${totalNum} results for "${query}"`);
      } else {
        setStatus(`Search error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Search error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, []);

  // ─── Load series data ───
  const loadSeriesData = useCallback(async (fullSeriesId: string, seriesName: string) => {
    try {
      setLoading(true);
      setStatus('Loading data...');
      const [providerCode, datasetCode, seriesCode] = fullSeriesId.split('/');
      const response = await fetch(
        `${DBNOMICS_API_BASE}/series/${providerCode}/${datasetCode}/${seriesCode}?observations=1&format=json`
      );
      if (response.ok) {
        const data = await response.json();
        const seriesDocs = data?.series?.docs || [];
        if (seriesDocs.length > 0) {
          const firstSeries = seriesDocs[0];
          const observations: Observation[] = [];
          if (firstSeries.period && firstSeries.value) {
            for (let i = 0; i < Math.min(firstSeries.period.length, firstSeries.value.length); i++) {
              if (firstSeries.value[i] !== null) {
                observations.push({ period: firstSeries.period[i], value: firstSeries.value[i] });
              }
            }
          }
          if (observations.length > 0) {
            const colorIndex = (singleViewSeries.length + slots.flat().length) % chartColors.length;
            setCurrentData({
              series_id: fullSeriesId,
              series_name: seriesName,
              observations,
              color: chartColors[colorIndex],
            });
            setStatus(`Loaded ${observations.length} points`);
          } else {
            setStatus('No data found');
          }
        } else {
          setStatus('No data found');
        }
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, [singleViewSeries.length, slots, chartColors]);

  // ─── Selection handlers ───
  const selectProvider = useCallback((providerCode: string) => {
    setSelectedProvider(providerCode);
    setSelectedDataset(null);
    setSelectedSeries(null);
    setDatasets([]);
    setSeriesList([]);
    setDatasetSearchState('');
    setSeriesSearchState('');
    currentProviderRef.current = providerCode;
    currentDatasetRef.current = null;
    setDatasetsPagination({ ...DEFAULT_PAGINATION, limit: DATASETS_PAGE_SIZE });
    setSeriesPagination({ ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE });
    loadDatasets(providerCode);
  }, [loadDatasets]);

  const selectDataset = useCallback((datasetCode: string) => {
    setSelectedDataset(datasetCode);
    setSelectedSeries(null);
    setSeriesList([]);
    setSeriesSearchState('');
    currentDatasetRef.current = datasetCode;
    setSeriesPagination({ ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE });
    if (selectedProvider) {
      loadSeries(selectedProvider, datasetCode);
    }
  }, [selectedProvider, loadSeries]);

  const selectSeries = useCallback((series: Series) => {
    setSelectedSeries(series.full_id);
    loadSeriesData(series.full_id, series.name);
  }, [loadSeriesData]);

  const refreshCurrentSeries = useCallback(() => {
    if (currentData) {
      loadSeriesData(currentData.series_id, currentData.series_name);
    }
  }, [currentData, loadSeriesData]);

  // ─── Search handlers with debounce ───
  const setProviderSearch = useCallback((query: string) => {
    setProviderSearchState(query);
    // Provider search is client-side only (all providers loaded)
  }, []);

  const setDatasetSearch = useCallback((query: string) => {
    setDatasetSearchState(query);
    // Datasets don't have server-side search in the API,
    // but we can reload to re-filter client-side. The API doesn't support q param on /datasets.
    // So dataset search remains client-side filtering.
  }, []);

  const setSeriesSearch = useCallback((query: string) => {
    setSeriesSearchState(query);
    // Series search uses the API's q parameter - debounced
    if (seriesSearchDebounceRef.current) {
      clearTimeout(seriesSearchDebounceRef.current);
    }
    seriesSearchDebounceRef.current = setTimeout(() => {
      if (currentProviderRef.current && currentDatasetRef.current) {
        setSeriesPagination({ ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE });
        loadSeries(currentProviderRef.current, currentDatasetRef.current, 0, false, query);
      }
    }, SEARCH_DEBOUNCE_MS);
  }, [loadSeries]);

  const setGlobalSearch = useCallback((query: string) => {
    setGlobalSearchState(query);
    if (searchDebounceRef.current) {
      clearTimeout(searchDebounceRef.current);
    }
    if (!query.trim()) {
      setGlobalSearchResults([]);
      setSearchPagination({ ...DEFAULT_PAGINATION, limit: SEARCH_PAGE_SIZE });
      return;
    }
    searchDebounceRef.current = setTimeout(() => {
      setSearchPagination({ ...DEFAULT_PAGINATION, limit: SEARCH_PAGE_SIZE });
      executeGlobalSearch(query, 0, false);
    }, SEARCH_DEBOUNCE_MS);
  }, [executeGlobalSearch]);

  // ─── Select a global search result ───
  const selectSearchResult = useCallback((result: SearchResult) => {
    // Set the provider and dataset from the search result
    setSelectedProvider(result.provider_code);
    currentProviderRef.current = result.provider_code;
    setSelectedDataset(result.dataset_code);
    currentDatasetRef.current = result.dataset_code;
    setSelectedSeries(null);
    setDatasetSearchState('');
    setSeriesSearchState('');

    // Load datasets for the provider (so the dataset list populates)
    loadDatasets(result.provider_code);
    // Load series for the specific dataset
    setSeriesPagination({ ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE });
    loadSeries(result.provider_code, result.dataset_code);
  }, [loadDatasets, loadSeries]);

  // ─── Pagination: load more ───
  const loadMoreDatasets = useCallback(() => {
    if (!datasetsPagination.hasMore || loading) return;
    if (currentProviderRef.current) {
      loadDatasets(currentProviderRef.current, datasetsPagination.offset, true);
    }
  }, [datasetsPagination, loading, loadDatasets]);

  const loadMoreSeries = useCallback(() => {
    if (!seriesPagination.hasMore || loading) return;
    if (currentProviderRef.current && currentDatasetRef.current) {
      loadSeries(currentProviderRef.current, currentDatasetRef.current, seriesPagination.offset, true, seriesSearch);
    }
  }, [seriesPagination, loading, loadSeries, seriesSearch]);

  const loadMoreSearchResults = useCallback(() => {
    if (!searchPagination.hasMore || loading) return;
    executeGlobalSearch(globalSearch, searchPagination.offset, true);
  }, [searchPagination, loading, executeGlobalSearch, globalSearch]);

  // ─── Slot management ───
  const addSlot = useCallback(() => {
    setSlots(prev => [...prev, []]);
    setSlotChartTypes(prev => [...prev, 'line']);
    setStatus(`Added slot ${slots.length + 1}`);
  }, [slots.length]);

  const removeSlot = useCallback((index: number) => {
    if (slots.length > 1) {
      setSlots(prev => prev.filter((_, i) => i !== index));
      setSlotChartTypes(prev => prev.filter((_, i) => i !== index));
      setStatus(`Removed slot ${index + 1}`);
    }
  }, [slots.length]);

  const addToSlot = useCallback((slotIndex: number) => {
    if (currentData && slotIndex >= 0 && slotIndex < slots.length) {
      setSlots(prev => {
        const newSlots = [...prev];
        newSlots[slotIndex] = [...newSlots[slotIndex], currentData];
        return newSlots;
      });
      setStatus(`Added to slot ${slotIndex + 1}`);
    }
  }, [currentData, slots.length]);

  const removeFromSlot = useCallback((slotIndex: number, seriesIndex: number) => {
    setSlots(prev => {
      const newSlots = [...prev];
      newSlots[slotIndex] = newSlots[slotIndex].filter((_, i) => i !== seriesIndex);
      return newSlots;
    });
    setStatus(`Removed from slot ${slotIndex + 1}`);
  }, []);

  const setSlotChartType = useCallback((slotIndex: number, chartType: ChartType) => {
    setSlotChartTypes(prev => {
      const newTypes = [...prev];
      newTypes[slotIndex] = chartType;
      return newTypes;
    });
  }, []);

  // ─── Single view management ───
  const addToSingleView = useCallback(() => {
    if (currentData) {
      setSingleViewSeries(prev => [...prev, currentData]);
      setActiveViewState('single');
      setStatus('Added to single view');
    }
  }, [currentData]);

  const removeFromSingleView = useCallback((index: number) => {
    setSingleViewSeries(prev => prev.filter((_, i) => i !== index));
  }, []);

  const setSingleChartType = useCallback((chartType: ChartType) => {
    setSingleChartTypeState(chartType);
  }, []);

  const setActiveView = useCallback((view: 'single' | 'comparison') => {
    setActiveViewState(view);
  }, []);

  // ─── Clear all ───
  const clearAll = useCallback(async () => {
    setSlots([[]]);
    setSlotChartTypes(['line']);
    setSingleViewSeries([]);
    await saveSetting('dbnomics_state', '', 'dbnomics_tab');
    setStatus('Cleared all');
  }, []);

  // ─── Export data ───
  const exportData = useCallback(() => {
    if (!currentData) return;
    const csv = ['Period,Value', ...currentData.observations.map(obs => `${obs.period},${obs.value}`)].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `dbnomics_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
    setStatus('Exported');
  }, [currentData]);

  // Initialize on mount
  useEffect(() => {
    loadProviders();
    restoreState();
  }, []);

  return {
    providers,
    datasets,
    seriesList,
    currentData,
    globalSearchResults,
    selectedProvider,
    selectedDataset,
    selectedSeries,
    providerSearch,
    datasetSearch,
    seriesSearch,
    globalSearch,
    datasetsPagination,
    seriesPagination,
    searchPagination,
    slots,
    slotChartTypes,
    singleViewSeries,
    singleChartType,
    activeView,
    loading,
    status,
    chartColors,
    loadProviders,
    selectProvider,
    selectDataset,
    selectSeries,
    refreshCurrentSeries,
    setProviderSearch,
    setDatasetSearch,
    setSeriesSearch,
    setGlobalSearch,
    executeGlobalSearch,
    selectSearchResult,
    loadMoreDatasets,
    loadMoreSeries,
    loadMoreSearchResults,
    addSlot,
    removeSlot,
    addToSlot,
    removeFromSlot,
    setSlotChartType,
    addToSingleView,
    removeFromSingleView,
    setSingleChartType,
    setActiveView,
    clearAll,
    exportData,
  };
}
