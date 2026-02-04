import { useReducer, useCallback, useRef, useEffect } from 'react';
import { getSetting, saveSetting } from '@/services/core/sqliteService';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import type {
  Provider,
  Dataset,
  Series,
  DataPoint,
  Observation,
  ChartType,
  DBnomicsState,
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

// ============================================================================
// Constants
// ============================================================================

const API_TIMEOUT_MS = 30000;

const DEFAULT_PAGINATION: PaginationState = {
  offset: 0,
  limit: 50,
  total: 0,
  hasMore: false,
};

// ============================================================================
// State Machine (Point #2: Explicit states for async flows)
// ============================================================================

type AsyncStatus = 'idle' | 'loading' | 'success' | 'error';

interface DBnomicsHookState {
  // Data
  providers: Provider[];
  datasets: Dataset[];
  seriesList: Series[];
  currentData: DataPoint | null;
  globalSearchResults: SearchResult[];

  // Selection
  selectedProvider: string | null;
  selectedDataset: string | null;
  selectedSeries: string | null;

  // Search
  providerSearch: string;
  datasetSearch: string;
  seriesSearch: string;
  globalSearch: string;

  // Pagination
  datasetsPagination: PaginationState;
  seriesPagination: PaginationState;
  searchPagination: PaginationState;

  // View
  slots: DataPoint[][];
  slotChartTypes: ChartType[];
  singleViewSeries: DataPoint[];
  singleChartType: ChartType;
  activeView: 'single' | 'comparison';

  // Async state machine
  asyncStatus: AsyncStatus;
  statusMessage: string;
}

// Point #1 & #9: Discriminated union for atomic updates
type DBnomicsAction =
  | { type: 'SET_ASYNC'; payload: { status: AsyncStatus; message: string } }
  | { type: 'PROVIDERS_LOADED'; payload: Provider[] }
  | { type: 'DATASETS_LOADED'; payload: { datasets: Dataset[]; pagination: PaginationState; append: boolean } }
  | { type: 'DATASETS_APPEND'; payload: { datasets: Dataset[]; pagination: PaginationState } }
  | { type: 'SERIES_LOADED'; payload: { seriesList: Series[]; pagination: PaginationState; append: boolean } }
  | { type: 'SERIES_DATA_LOADED'; payload: DataPoint }
  | { type: 'SEARCH_RESULTS_LOADED'; payload: { results: SearchResult[]; pagination: PaginationState; append: boolean } }
  | { type: 'SELECT_PROVIDER'; payload: string }
  | { type: 'SELECT_DATASET'; payload: string }
  | { type: 'SELECT_SERIES'; payload: string }
  | { type: 'SET_PROVIDER_SEARCH'; payload: string }
  | { type: 'SET_DATASET_SEARCH'; payload: string }
  | { type: 'SET_SERIES_SEARCH'; payload: string }
  | { type: 'SET_GLOBAL_SEARCH'; payload: string }
  | { type: 'CLEAR_GLOBAL_SEARCH' }
  | { type: 'SELECT_SEARCH_RESULT'; payload: { providerCode: string; datasetCode: string } }
  | { type: 'ADD_SLOT' }
  | { type: 'REMOVE_SLOT'; payload: number }
  | { type: 'ADD_TO_SLOT'; payload: { slotIndex: number; data: DataPoint } }
  | { type: 'REMOVE_FROM_SLOT'; payload: { slotIndex: number; seriesIndex: number } }
  | { type: 'SET_SLOT_CHART_TYPE'; payload: { slotIndex: number; chartType: ChartType } }
  | { type: 'ADD_TO_SINGLE_VIEW'; payload: DataPoint }
  | { type: 'REMOVE_FROM_SINGLE_VIEW'; payload: number }
  | { type: 'SET_SINGLE_CHART_TYPE'; payload: ChartType }
  | { type: 'SET_ACTIVE_VIEW'; payload: 'single' | 'comparison' }
  | { type: 'CLEAR_ALL' }
  | { type: 'RESTORE_STATE'; payload: Partial<DBnomicsHookState> };

const initialState: DBnomicsHookState = {
  providers: [],
  datasets: [],
  seriesList: [],
  currentData: null,
  globalSearchResults: [],
  selectedProvider: null,
  selectedDataset: null,
  selectedSeries: null,
  providerSearch: '',
  datasetSearch: '',
  seriesSearch: '',
  globalSearch: '',
  datasetsPagination: { ...DEFAULT_PAGINATION, limit: DATASETS_PAGE_SIZE },
  seriesPagination: { ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE },
  searchPagination: { ...DEFAULT_PAGINATION, limit: SEARCH_PAGE_SIZE },
  slots: [[]],
  slotChartTypes: ['line'],
  singleViewSeries: [],
  singleChartType: 'line',
  activeView: 'single',
  asyncStatus: 'idle',
  statusMessage: 'Loading providers...',
};

function dbnomicsReducer(state: DBnomicsHookState, action: DBnomicsAction): DBnomicsHookState {
  switch (action.type) {
    case 'SET_ASYNC':
      return { ...state, asyncStatus: action.payload.status, statusMessage: action.payload.message };

    case 'PROVIDERS_LOADED':
      return {
        ...state,
        providers: action.payload,
        asyncStatus: 'success',
        statusMessage: `Loaded ${action.payload.length} providers`,
      };

    case 'DATASETS_LOADED':
      return {
        ...state,
        datasets: action.payload.append
          ? [...state.datasets, ...action.payload.datasets]
          : action.payload.datasets,
        seriesList: action.payload.append ? state.seriesList : [],
        datasetsPagination: action.payload.pagination,
        asyncStatus: 'success',
        statusMessage: `Loaded ${action.payload.append
          ? state.datasets.length + action.payload.datasets.length
          : action.payload.datasets.length} of ${action.payload.pagination.total} datasets`,
      };

    case 'SERIES_LOADED':
      return {
        ...state,
        seriesList: action.payload.append
          ? [...state.seriesList, ...action.payload.seriesList]
          : action.payload.seriesList,
        seriesPagination: action.payload.pagination,
        asyncStatus: 'success',
        statusMessage: `Loaded ${action.payload.append
          ? state.seriesList.length + action.payload.seriesList.length
          : action.payload.seriesList.length} of ${action.payload.pagination.total} series`,
      };

    case 'SERIES_DATA_LOADED':
      return {
        ...state,
        currentData: action.payload,
        asyncStatus: 'success',
        statusMessage: `Loaded ${action.payload.observations.length} points`,
      };

    case 'SEARCH_RESULTS_LOADED':
      return {
        ...state,
        globalSearchResults: action.payload.append
          ? [...state.globalSearchResults, ...action.payload.results]
          : action.payload.results,
        searchPagination: action.payload.pagination,
        asyncStatus: 'success',
        statusMessage: `Found ${action.payload.pagination.total} results`,
      };

    case 'SELECT_PROVIDER':
      return {
        ...state,
        selectedProvider: action.payload,
        selectedDataset: null,
        selectedSeries: null,
        datasets: [],
        seriesList: [],
        datasetSearch: '',
        seriesSearch: '',
        datasetsPagination: { ...DEFAULT_PAGINATION, limit: DATASETS_PAGE_SIZE },
        seriesPagination: { ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE },
      };

    case 'SELECT_DATASET':
      return {
        ...state,
        selectedDataset: action.payload,
        selectedSeries: null,
        seriesList: [],
        seriesSearch: '',
        seriesPagination: { ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE },
      };

    case 'SELECT_SERIES':
      return { ...state, selectedSeries: action.payload };

    case 'SET_PROVIDER_SEARCH':
      return { ...state, providerSearch: action.payload };
    case 'SET_DATASET_SEARCH':
      return { ...state, datasetSearch: action.payload };
    case 'SET_SERIES_SEARCH':
      return { ...state, seriesSearch: action.payload };
    case 'SET_GLOBAL_SEARCH':
      return { ...state, globalSearch: action.payload };

    case 'CLEAR_GLOBAL_SEARCH':
      return {
        ...state,
        globalSearch: '',
        globalSearchResults: [],
        searchPagination: { ...DEFAULT_PAGINATION, limit: SEARCH_PAGE_SIZE },
      };

    case 'SELECT_SEARCH_RESULT':
      return {
        ...state,
        selectedProvider: action.payload.providerCode,
        selectedDataset: action.payload.datasetCode,
        selectedSeries: null,
        datasetSearch: '',
        seriesSearch: '',
        seriesPagination: { ...DEFAULT_PAGINATION, limit: SERIES_PAGE_SIZE },
      };

    case 'ADD_SLOT':
      return {
        ...state,
        slots: [...state.slots, []],
        slotChartTypes: [...state.slotChartTypes, 'line'],
        statusMessage: `Added slot ${state.slots.length + 1}`,
      };

    case 'REMOVE_SLOT':
      if (state.slots.length <= 1) return state;
      return {
        ...state,
        slots: state.slots.filter((_, i) => i !== action.payload),
        slotChartTypes: state.slotChartTypes.filter((_, i) => i !== action.payload),
        statusMessage: `Removed slot ${action.payload + 1}`,
      };

    case 'ADD_TO_SLOT': {
      const { slotIndex, data } = action.payload;
      if (slotIndex < 0 || slotIndex >= state.slots.length) return state;
      const newSlots = [...state.slots];
      newSlots[slotIndex] = [...newSlots[slotIndex], data];
      return { ...state, slots: newSlots, statusMessage: `Added to slot ${slotIndex + 1}` };
    }

    case 'REMOVE_FROM_SLOT': {
      const { slotIndex, seriesIndex } = action.payload;
      const updatedSlots = [...state.slots];
      updatedSlots[slotIndex] = updatedSlots[slotIndex].filter((_, i) => i !== seriesIndex);
      return { ...state, slots: updatedSlots, statusMessage: `Removed from slot ${slotIndex + 1}` };
    }

    case 'SET_SLOT_CHART_TYPE': {
      const newTypes = [...state.slotChartTypes];
      newTypes[action.payload.slotIndex] = action.payload.chartType;
      return { ...state, slotChartTypes: newTypes };
    }

    case 'ADD_TO_SINGLE_VIEW':
      return {
        ...state,
        singleViewSeries: [...state.singleViewSeries, action.payload],
        activeView: 'single',
        statusMessage: 'Added to single view',
      };

    case 'REMOVE_FROM_SINGLE_VIEW':
      return {
        ...state,
        singleViewSeries: state.singleViewSeries.filter((_, i) => i !== action.payload),
      };

    case 'SET_SINGLE_CHART_TYPE':
      return { ...state, singleChartType: action.payload };

    case 'SET_ACTIVE_VIEW':
      return { ...state, activeView: action.payload };

    case 'CLEAR_ALL':
      return {
        ...state,
        slots: [[]],
        slotChartTypes: ['line'],
        singleViewSeries: [],
        statusMessage: 'Cleared all',
      };

    case 'RESTORE_STATE':
      return { ...state, ...action.payload };

    default:
      return state;
  }
}

// ============================================================================
// Cache types (kept in refs, not state)
// ============================================================================

interface CacheEntry<T> {
  data: T;
  timestamp: number;
}

// ============================================================================
// Hook
// ============================================================================

export interface UseDBnomicsDataReturn {
  providers: Provider[];
  datasets: Dataset[];
  seriesList: Series[];
  currentData: DataPoint | null;
  globalSearchResults: SearchResult[];
  selectedProvider: string | null;
  selectedDataset: string | null;
  selectedSeries: string | null;
  providerSearch: string;
  datasetSearch: string;
  seriesSearch: string;
  globalSearch: string;
  datasetsPagination: PaginationState;
  seriesPagination: PaginationState;
  searchPagination: PaginationState;
  slots: DataPoint[][];
  slotChartTypes: ChartType[];
  singleViewSeries: DataPoint[];
  singleChartType: ChartType;
  activeView: 'single' | 'comparison';
  loading: boolean;
  status: string;
  chartColors: string[];
  loadProviders: () => Promise<void>;
  selectProvider: (providerCode: string) => void;
  selectDataset: (datasetCode: string) => void;
  selectSeries: (series: Series) => void;
  refreshCurrentSeries: () => void;
  setProviderSearch: (query: string) => void;
  setDatasetSearch: (query: string) => void;
  setSeriesSearch: (query: string) => void;
  setGlobalSearch: (query: string) => void;
  executeGlobalSearch: (query: string) => Promise<void>;
  selectSearchResult: (result: SearchResult) => void;
  loadMoreDatasets: () => void;
  loadMoreSeries: () => void;
  loadMoreSearchResults: () => void;
  addSlot: () => void;
  removeSlot: (index: number) => void;
  addToSlot: (slotIndex: number) => void;
  removeFromSlot: (slotIndex: number, seriesIndex: number) => void;
  setSlotChartType: (slotIndex: number, chartType: ChartType) => void;
  addToSingleView: () => void;
  removeFromSingleView: (index: number) => void;
  setSingleChartType: (chartType: ChartType) => void;
  setActiveView: (view: 'single' | 'comparison') => void;
  clearAll: () => Promise<void>;
  exportData: () => void;
}

export function useDBnomicsData(themeColors?: string[]): UseDBnomicsDataReturn {
  const chartColors = themeColors || DEFAULT_CHART_COLORS;

  // Point #1 & #9: Single reducer for all related state
  const [state, dispatch] = useReducer(dbnomicsReducer, initialState);

  // Point #3: Refs for cleanup
  const mountedRef = useRef(true);
  const abortControllerRef = useRef<AbortController | null>(null);
  const saveTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const searchDebounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const datasetSearchDebounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const seriesSearchDebounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  // Cache refs (not state - avoids re-renders)
  const providersCacheRef = useRef<CacheEntry<Provider[]> | null>(null);
  const datasetsCacheRef = useRef<Map<string, CacheEntry<Dataset[]>>>(new Map());
  const seriesCacheRef = useRef<Map<string, CacheEntry<Series[]>>>(new Map());

  // Track current provider/dataset for pagination
  const currentProviderRef = useRef<string | null>(null);
  const currentDatasetRef = useRef<string | null>(null);

  const isCacheValid = useCallback((timestamp: number, duration: number): boolean => {
    return Date.now() - timestamp < duration;
  }, []);

  // Point #3: Helper to create a new AbortController and abort previous
  const getSignal = useCallback((): AbortSignal => {
    abortControllerRef.current?.abort();
    const controller = new AbortController();
    abortControllerRef.current = controller;
    return controller.signal;
  }, []);

  // Point #8: Fetcher with timeout
  const fetchWithTimeout = useCallback(async (url: string, signal: AbortSignal): Promise<any> => {
    const response = await withTimeout(
      fetch(url, { signal }),
      API_TIMEOUT_MS,
      `Request timeout: ${url}`,
    );
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    return response.json();
  }, []);

  // ---- Save state with debouncing ----
  const saveState = useCallback(async () => {
    if (saveTimeoutRef.current) {
      clearTimeout(saveTimeoutRef.current);
    }
    saveTimeoutRef.current = setTimeout(async () => {
      if (!mountedRef.current) return;
      try {
        const persistState: DBnomicsState = {
          singleViewSeries: state.singleViewSeries,
          slots: state.slots,
          slotChartTypes: state.slotChartTypes,
          singleChartType: state.singleChartType,
          activeView: state.activeView,
          timestamp: Date.now(),
        };
        await saveSetting('dbnomics_state', JSON.stringify(persistState), 'dbnomics_tab');
      } catch (error) {
        console.error('[DBnomics] Failed to save state:', error);
      }
    }, STATE_SAVE_DEBOUNCE_MS);
  }, [state.singleViewSeries, state.slots, state.slotChartTypes, state.singleChartType, state.activeView]);

  // Auto-save state when it changes
  useEffect(() => {
    if (state.singleViewSeries.length > 0 || state.slots.some(s => s.length > 0)) {
      saveState();
    }
  }, [state.singleViewSeries, state.slots, state.slotChartTypes, state.singleChartType, state.activeView, saveState]);

  // Restore state on mount
  const restoreState = useCallback(async () => {
    try {
      const saved = await getSetting('dbnomics_state');
      if (saved && mountedRef.current) {
        const parsed: DBnomicsState = JSON.parse(saved);
        if (parsed.timestamp && isCacheValid(parsed.timestamp, STATE_CACHE_DURATION)) {
          dispatch({
            type: 'RESTORE_STATE',
            payload: {
              singleViewSeries: parsed.singleViewSeries || [],
              slots: parsed.slots || [[]],
              slotChartTypes: parsed.slotChartTypes || ['line'],
              singleChartType: parsed.singleChartType || 'line',
              activeView: parsed.activeView || 'single',
              statusMessage: 'Restored from cache',
            },
          });
        }
      }
    } catch (error) {
      console.error('[DBnomics] Failed to restore state:', error);
    }
  }, [isCacheValid]);

  // ---- Load providers ----
  const loadProviders = useCallback(async () => {
    if (providersCacheRef.current && isCacheValid(providersCacheRef.current.timestamp, PROVIDERS_CACHE_DURATION)) {
      dispatch({ type: 'PROVIDERS_LOADED', payload: providersCacheRef.current.data });
      return;
    }

    const signal = getSignal();
    dispatch({ type: 'SET_ASYNC', payload: { status: 'loading', message: 'Loading providers...' } });

    try {
      const data = await fetchWithTimeout(`${DBNOMICS_API_BASE}/providers`, signal);
      if (!mountedRef.current) return;

      const providerList: Provider[] = (data?.providers?.docs || []).map((p: any) => ({
        code: p.code || '',
        name: p.name || p.code || '',
      }));
      providersCacheRef.current = { data: providerList, timestamp: Date.now() };
      dispatch({ type: 'PROVIDERS_LOADED', payload: providerList });
    } catch (error) {
      if (!mountedRef.current || (error instanceof Error && error.message === 'Request aborted')) return;
      dispatch({ type: 'SET_ASYNC', payload: { status: 'error', message: `Error: ${error}` } });
    }
  }, [isCacheValid, getSignal, fetchWithTimeout]);

  // ---- Load datasets with pagination ----
  const loadDatasets = useCallback(async (providerCode: string, offset = 0, append = false) => {
    // Point #10: Sanitize
    const sanitized = sanitizeInput(providerCode);
    if (!sanitized) return;

    // Check cache for first page
    if (offset === 0 && !append) {
      const cached = datasetsCacheRef.current.get(sanitized);
      if (cached && isCacheValid(cached.timestamp, DATASETS_CACHE_DURATION)) {
        dispatch({
          type: 'DATASETS_LOADED',
          payload: {
            datasets: cached.data,
            pagination: { offset: 0, limit: DATASETS_PAGE_SIZE, total: cached.data.length, hasMore: false },
            append: false,
          },
        });
        return;
      }
    }

    const signal = getSignal();
    dispatch({ type: 'SET_ASYNC', payload: { status: 'loading', message: append ? 'Loading more datasets...' : 'Loading datasets...' } });

    try {
      const data = await fetchWithTimeout(
        `${DBNOMICS_API_BASE}/datasets/${encodeURIComponent(sanitized)}?limit=${DATASETS_PAGE_SIZE}&offset=${offset}`,
        signal,
      );
      if (!mountedRef.current) return;

      const totalNum = data?.datasets?.num_found ?? 0;
      const datasetList: Dataset[] = (data?.datasets?.docs || []).map((d: any) => ({
        code: d.code || '',
        name: d.name || d.code || '',
      }));

      const newOffset = offset + datasetList.length;

      // Cache first-page loads
      if (offset === 0 && !append && totalNum <= DATASETS_PAGE_SIZE) {
        datasetsCacheRef.current.set(sanitized, { data: datasetList, timestamp: Date.now() });
      }

      dispatch({
        type: 'DATASETS_LOADED',
        payload: {
          datasets: datasetList,
          pagination: { offset: newOffset, limit: DATASETS_PAGE_SIZE, total: totalNum, hasMore: newOffset < totalNum },
          append,
        },
      });
    } catch (error) {
      if (!mountedRef.current || (error instanceof Error && error.message === 'Request aborted')) return;
      dispatch({ type: 'SET_ASYNC', payload: { status: 'error', message: `Error: ${error}` } });
    }
  }, [isCacheValid, getSignal, fetchWithTimeout]);

  // ---- Load series with pagination and search ----
  const loadSeries = useCallback(async (providerCode: string, datasetCode: string, offset = 0, append = false, query = '') => {
    const sanitizedProvider = sanitizeInput(providerCode);
    const sanitizedDataset = sanitizeInput(datasetCode);
    const sanitizedQuery = sanitizeInput(query);
    if (!sanitizedProvider || !sanitizedDataset) return;

    const cacheKey = `${sanitizedProvider}/${sanitizedDataset}`;

    // Check cache for first page without search
    if (offset === 0 && !append && !sanitizedQuery) {
      const cached = seriesCacheRef.current.get(cacheKey);
      if (cached && isCacheValid(cached.timestamp, SERIES_CACHE_DURATION)) {
        dispatch({
          type: 'SERIES_LOADED',
          payload: {
            seriesList: cached.data,
            pagination: { offset: 0, limit: SERIES_PAGE_SIZE, total: cached.data.length, hasMore: false },
            append: false,
          },
        });
        return;
      }
    }

    const signal = getSignal();
    dispatch({ type: 'SET_ASYNC', payload: { status: 'loading', message: append ? 'Loading more series...' : 'Loading series...' } });

    try {
      let url = `${DBNOMICS_API_BASE}/series/${encodeURIComponent(sanitizedProvider)}/${encodeURIComponent(sanitizedDataset)}?limit=${SERIES_PAGE_SIZE}&offset=${offset}&observations=false`;
      if (sanitizedQuery) {
        url += `&q=${encodeURIComponent(sanitizedQuery)}`;
      }

      const data = await fetchWithTimeout(url, signal);
      if (!mountedRef.current) return;

      const totalNum = data?.series?.num_found ?? 0;
      const seriesArray: Series[] = (data?.series?.docs || []).map((s: any) => ({
        code: s.series_code || '',
        name: s.series_name || s.series_code || '',
        full_id: `${sanitizedProvider}/${sanitizedDataset}/${s.series_code}`,
      }));

      const newOffset = offset + seriesArray.length;

      // Cache first-page loads without search
      if (offset === 0 && !append && !sanitizedQuery && totalNum <= SERIES_PAGE_SIZE) {
        seriesCacheRef.current.set(cacheKey, { data: seriesArray, timestamp: Date.now() });
      }

      dispatch({
        type: 'SERIES_LOADED',
        payload: {
          seriesList: seriesArray,
          pagination: { offset: newOffset, limit: SERIES_PAGE_SIZE, total: totalNum, hasMore: newOffset < totalNum },
          append,
        },
      });
    } catch (error) {
      if (!mountedRef.current || (error instanceof Error && error.message === 'Request aborted')) return;
      dispatch({ type: 'SET_ASYNC', payload: { status: 'error', message: `Error: ${error}` } });
    }
  }, [isCacheValid, getSignal, fetchWithTimeout]);

  // ---- Global search ----
  const executeGlobalSearch = useCallback(async (query: string, offset = 0, append = false) => {
    const sanitized = sanitizeInput(query);
    if (!sanitized.trim()) {
      dispatch({ type: 'CLEAR_GLOBAL_SEARCH' });
      return;
    }

    const signal = getSignal();
    dispatch({ type: 'SET_ASYNC', payload: { status: 'loading', message: `Searching "${sanitized}"...` } });

    try {
      const data = await fetchWithTimeout(
        `${DBNOMICS_API_BASE}/search?q=${encodeURIComponent(sanitized)}&limit=${SEARCH_PAGE_SIZE}&offset=${offset}`,
        signal,
      );
      if (!mountedRef.current) return;

      const totalNum = data?.results?.num_found ?? data?.num_found ?? 0;
      const results: SearchResult[] = (data?.results?.docs || []).map((r: any) => ({
        provider_code: r.provider_code || '',
        provider_name: r.provider_name || r.provider_code || '',
        dataset_code: r.dataset_code || '',
        dataset_name: r.dataset_name || r.dataset_code || '',
        nb_series: r.nb_series || 0,
      }));

      const newOffset = offset + results.length;

      dispatch({
        type: 'SEARCH_RESULTS_LOADED',
        payload: {
          results,
          pagination: { offset: newOffset, limit: SEARCH_PAGE_SIZE, total: totalNum, hasMore: newOffset < totalNum },
          append,
        },
      });
    } catch (error) {
      if (!mountedRef.current || (error instanceof Error && error.message === 'Request aborted')) return;
      dispatch({ type: 'SET_ASYNC', payload: { status: 'error', message: `Search error: ${error}` } });
    }
  }, [getSignal, fetchWithTimeout]);

  // ---- Load series data ----
  const loadSeriesData = useCallback(async (fullSeriesId: string, seriesName: string) => {
    const sanitized = sanitizeInput(fullSeriesId);
    const parts = sanitized.split('/');
    if (parts.length < 3) return;

    const [providerCode, datasetCode, seriesCode] = parts;
    const signal = getSignal();
    dispatch({ type: 'SET_ASYNC', payload: { status: 'loading', message: 'Loading data...' } });

    try {
      const data = await fetchWithTimeout(
        `${DBNOMICS_API_BASE}/series/${encodeURIComponent(providerCode)}/${encodeURIComponent(datasetCode)}/${encodeURIComponent(seriesCode)}?observations=1&format=json`,
        signal,
      );
      if (!mountedRef.current) return;

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
          const colorIndex = (state.singleViewSeries.length + state.slots.flat().length) % chartColors.length;
          dispatch({
            type: 'SERIES_DATA_LOADED',
            payload: {
              series_id: fullSeriesId,
              series_name: seriesName,
              observations,
              color: chartColors[colorIndex],
            },
          });
        } else {
          dispatch({ type: 'SET_ASYNC', payload: { status: 'success', message: 'No data found' } });
        }
      } else {
        dispatch({ type: 'SET_ASYNC', payload: { status: 'success', message: 'No data found' } });
      }
    } catch (error) {
      if (!mountedRef.current || (error instanceof Error && error.message === 'Request aborted')) return;
      dispatch({ type: 'SET_ASYNC', payload: { status: 'error', message: `Error: ${error}` } });
    }
  }, [state.singleViewSeries.length, state.slots, chartColors, getSignal, fetchWithTimeout]);

  // ---- Selection handlers ----
  const selectProvider = useCallback((providerCode: string) => {
    dispatch({ type: 'SELECT_PROVIDER', payload: providerCode });
    currentProviderRef.current = providerCode;
    currentDatasetRef.current = null;
    loadDatasets(providerCode);
  }, [loadDatasets]);

  const selectDataset = useCallback((datasetCode: string) => {
    dispatch({ type: 'SELECT_DATASET', payload: datasetCode });
    currentDatasetRef.current = datasetCode;
    if (currentProviderRef.current) {
      loadSeries(currentProviderRef.current, datasetCode);
    }
  }, [loadSeries]);

  const selectSeries = useCallback((series: Series) => {
    dispatch({ type: 'SELECT_SERIES', payload: series.full_id });
    loadSeriesData(series.full_id, series.name);
  }, [loadSeriesData]);

  const refreshCurrentSeries = useCallback(() => {
    if (state.currentData) {
      loadSeriesData(state.currentData.series_id, state.currentData.series_name);
    }
  }, [state.currentData, loadSeriesData]);

  // ---- Search handlers with debounce ----
  const setProviderSearch = useCallback((query: string) => {
    dispatch({ type: 'SET_PROVIDER_SEARCH', payload: query });
  }, []);

  const setDatasetSearch = useCallback((query: string) => {
    dispatch({ type: 'SET_DATASET_SEARCH', payload: query });
  }, []);

  const setSeriesSearch = useCallback((query: string) => {
    dispatch({ type: 'SET_SERIES_SEARCH', payload: query });
    if (seriesSearchDebounceRef.current) {
      clearTimeout(seriesSearchDebounceRef.current);
    }
    seriesSearchDebounceRef.current = setTimeout(() => {
      if (currentProviderRef.current && currentDatasetRef.current && mountedRef.current) {
        loadSeries(currentProviderRef.current, currentDatasetRef.current, 0, false, query);
      }
    }, SEARCH_DEBOUNCE_MS);
  }, [loadSeries]);

  const setGlobalSearch = useCallback((query: string) => {
    dispatch({ type: 'SET_GLOBAL_SEARCH', payload: query });
    if (searchDebounceRef.current) {
      clearTimeout(searchDebounceRef.current);
    }
    if (!query.trim()) {
      dispatch({ type: 'CLEAR_GLOBAL_SEARCH' });
      return;
    }
    searchDebounceRef.current = setTimeout(() => {
      if (mountedRef.current) {
        executeGlobalSearch(query, 0, false);
      }
    }, SEARCH_DEBOUNCE_MS);
  }, [executeGlobalSearch]);

  // ---- Select a global search result ----
  const selectSearchResult = useCallback((result: SearchResult) => {
    dispatch({
      type: 'SELECT_SEARCH_RESULT',
      payload: { providerCode: result.provider_code, datasetCode: result.dataset_code },
    });
    currentProviderRef.current = result.provider_code;
    currentDatasetRef.current = result.dataset_code;
    loadDatasets(result.provider_code);
    loadSeries(result.provider_code, result.dataset_code);
  }, [loadDatasets, loadSeries]);

  // ---- Pagination: load more ----
  const loadMoreDatasets = useCallback(() => {
    if (!state.datasetsPagination.hasMore || state.asyncStatus === 'loading') return;
    if (currentProviderRef.current) {
      loadDatasets(currentProviderRef.current, state.datasetsPagination.offset, true);
    }
  }, [state.datasetsPagination, state.asyncStatus, loadDatasets]);

  const loadMoreSeries = useCallback(() => {
    if (!state.seriesPagination.hasMore || state.asyncStatus === 'loading') return;
    if (currentProviderRef.current && currentDatasetRef.current) {
      loadSeries(currentProviderRef.current, currentDatasetRef.current, state.seriesPagination.offset, true, state.seriesSearch);
    }
  }, [state.seriesPagination, state.asyncStatus, loadSeries, state.seriesSearch]);

  const loadMoreSearchResults = useCallback(() => {
    if (!state.searchPagination.hasMore || state.asyncStatus === 'loading') return;
    executeGlobalSearch(state.globalSearch, state.searchPagination.offset, true);
  }, [state.searchPagination, state.asyncStatus, executeGlobalSearch, state.globalSearch]);

  // ---- Slot management ----
  const addSlot = useCallback(() => {
    dispatch({ type: 'ADD_SLOT' });
  }, []);

  const removeSlot = useCallback((index: number) => {
    dispatch({ type: 'REMOVE_SLOT', payload: index });
  }, []);

  const addToSlot = useCallback((slotIndex: number) => {
    if (state.currentData) {
      dispatch({ type: 'ADD_TO_SLOT', payload: { slotIndex, data: state.currentData } });
    }
  }, [state.currentData]);

  const removeFromSlot = useCallback((slotIndex: number, seriesIndex: number) => {
    dispatch({ type: 'REMOVE_FROM_SLOT', payload: { slotIndex, seriesIndex } });
  }, []);

  const setSlotChartType = useCallback((slotIndex: number, chartType: ChartType) => {
    dispatch({ type: 'SET_SLOT_CHART_TYPE', payload: { slotIndex, chartType } });
  }, []);

  // ---- Single view management ----
  const addToSingleView = useCallback(() => {
    if (state.currentData) {
      dispatch({ type: 'ADD_TO_SINGLE_VIEW', payload: state.currentData });
    }
  }, [state.currentData]);

  const removeFromSingleView = useCallback((index: number) => {
    dispatch({ type: 'REMOVE_FROM_SINGLE_VIEW', payload: index });
  }, []);

  const setSingleChartType = useCallback((chartType: ChartType) => {
    dispatch({ type: 'SET_SINGLE_CHART_TYPE', payload: chartType });
  }, []);

  const setActiveView = useCallback((view: 'single' | 'comparison') => {
    dispatch({ type: 'SET_ACTIVE_VIEW', payload: view });
  }, []);

  // ---- Clear all ----
  const clearAll = useCallback(async () => {
    dispatch({ type: 'CLEAR_ALL' });
    try {
      await saveSetting('dbnomics_state', '', 'dbnomics_tab');
    } catch (error) {
      console.error('[DBnomics] Failed to clear saved state:', error);
    }
  }, []);

  // ---- Export data ----
  const exportData = useCallback(() => {
    if (!state.currentData) return;
    const csv = ['Period,Value', ...state.currentData.observations.map(obs => `${obs.period},${obs.value}`)].join('\n');
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `dbnomics_${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
    dispatch({ type: 'SET_ASYNC', payload: { status: 'success', message: 'Exported' } });
  }, [state.currentData]);

  // Point #3: Initialize on mount + cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    loadProviders();
    restoreState();

    return () => {
      mountedRef.current = false;
      // Abort any in-flight requests
      abortControllerRef.current?.abort();
      // Clear all debounce timers
      if (saveTimeoutRef.current) clearTimeout(saveTimeoutRef.current);
      if (searchDebounceRef.current) clearTimeout(searchDebounceRef.current);
      if (datasetSearchDebounceRef.current) clearTimeout(datasetSearchDebounceRef.current);
      if (seriesSearchDebounceRef.current) clearTimeout(seriesSearchDebounceRef.current);
    };
  }, []); // eslint-disable-line react-hooks/exhaustive-deps
  // loadProviders and restoreState are stable callbacks, intentionally omitted to run only on mount

  return {
    providers: state.providers,
    datasets: state.datasets,
    seriesList: state.seriesList,
    currentData: state.currentData,
    globalSearchResults: state.globalSearchResults,
    selectedProvider: state.selectedProvider,
    selectedDataset: state.selectedDataset,
    selectedSeries: state.selectedSeries,
    providerSearch: state.providerSearch,
    datasetSearch: state.datasetSearch,
    seriesSearch: state.seriesSearch,
    globalSearch: state.globalSearch,
    datasetsPagination: state.datasetsPagination,
    seriesPagination: state.seriesPagination,
    searchPagination: state.searchPagination,
    slots: state.slots,
    slotChartTypes: state.slotChartTypes,
    singleViewSeries: state.singleViewSeries,
    singleChartType: state.singleChartType,
    activeView: state.activeView,
    loading: state.asyncStatus === 'loading',
    status: state.statusMessage,
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
