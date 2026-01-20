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
} from '../types';
import {
  DBNOMICS_API_BASE,
  PROVIDERS_CACHE_DURATION,
  DATASETS_CACHE_DURATION,
  SERIES_CACHE_DURATION,
  STATE_CACHE_DURATION,
  STATE_SAVE_DEBOUNCE_MS,
  DEFAULT_CHART_COLORS,
} from '../constants';

interface UseDBnomicsDataReturn {
  // Data
  providers: Provider[];
  datasets: Dataset[];
  seriesList: Series[];
  currentData: DataPoint | null;

  // Selection state
  selectedProvider: string | null;
  selectedDataset: string | null;
  selectedSeries: string | null;

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

  // Selection state
  const [selectedProvider, setSelectedProvider] = useState<string | null>(null);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);
  const [selectedSeries, setSelectedSeries] = useState<string | null>(null);

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

  // Debounced state saving
  const saveTimeoutRef = useRef<NodeJS.Timeout | null>(null);

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

  // Load providers
  const loadProviders = useCallback(async () => {
    // Check cache first
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

        // Cache the results
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

  // Load datasets for a provider
  const loadDatasets = useCallback(async (providerCode: string) => {
    // Check cache
    const cached = datasetsCache.current.get(providerCode);
    if (cached && isCacheValid(cached.timestamp, DATASETS_CACHE_DURATION)) {
      setDatasets(cached.data);
      setSeriesList([]);
      setStatus(`Loaded ${cached.data.length} datasets (cached)`);
      return;
    }

    try {
      setLoading(true);
      setStatus('Loading datasets...');
      const response = await fetch(`${DBNOMICS_API_BASE}/datasets/${providerCode}`);
      if (response.ok) {
        const data = await response.json();
        const datasetList: Dataset[] = (data?.datasets?.docs || []).map((d: any) => ({
          code: d.code || '',
          name: d.name || d.code || '',
        }));

        // Cache the results
        datasetsCache.current.set(providerCode, { data: datasetList, timestamp: Date.now() });
        setDatasets(datasetList);
        setSeriesList([]);
        setStatus(`Loaded ${datasetList.length} datasets`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, [isCacheValid]);

  // Load series for a dataset
  const loadSeries = useCallback(async (providerCode: string, datasetCode: string) => {
    const cacheKey = `${providerCode}/${datasetCode}`;

    // Check cache
    const cached = seriesCache.current.get(cacheKey);
    if (cached && isCacheValid(cached.timestamp, SERIES_CACHE_DURATION)) {
      setSeriesList(cached.data);
      setStatus(`Loaded ${cached.data.length} series (cached)`);
      return;
    }

    try {
      setLoading(true);
      setStatus('Loading series...');
      const response = await fetch(
        `${DBNOMICS_API_BASE}/series/${providerCode}/${datasetCode}?limit=50&observations=false`
      );
      if (response.ok) {
        const data = await response.json();
        const seriesArray: Series[] = (data?.series?.docs || []).map((s: any) => ({
          code: s.series_code || '',
          name: s.series_name || s.series_code || '',
          full_id: `${providerCode}/${datasetCode}/${s.series_code}`,
        }));

        // Cache the results
        seriesCache.current.set(cacheKey, { data: seriesArray, timestamp: Date.now() });
        setSeriesList(seriesArray);
        setStatus(`Loaded ${seriesArray.length} series`);
      } else {
        setStatus(`Error: ${response.status}`);
      }
    } catch (error) {
      setStatus(`Error: ${error}`);
    } finally {
      setLoading(false);
    }
  }, [isCacheValid]);

  // Load series data
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

  // Selection handlers
  const selectProvider = useCallback((providerCode: string) => {
    setSelectedProvider(providerCode);
    setSelectedDataset(null);
    setSelectedSeries(null);
    setDatasets([]);
    setSeriesList([]);
    loadDatasets(providerCode);
  }, [loadDatasets]);

  const selectDataset = useCallback((datasetCode: string) => {
    setSelectedDataset(datasetCode);
    setSelectedSeries(null);
    setSeriesList([]);
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

  // Slot management
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

  // Single view management
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

  // Clear all
  const clearAll = useCallback(async () => {
    setSlots([[]]);
    setSlotChartTypes(['line']);
    setSingleViewSeries([]);
    await saveSetting('dbnomics_state', '', 'dbnomics_tab');
    setStatus('Cleared all');
  }, []);

  // Export data
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
    selectedProvider,
    selectedDataset,
    selectedSeries,
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
