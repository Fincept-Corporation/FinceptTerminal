// useSpainData Hook - Fetch and manage Spain Open Data (datos.gob.es)

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  SpainDataView,
  SpainCatalogue,
  SpainDataset,
  SpainResource,
  SpainDataResponse,
} from '../types';

interface UseSpainDataReturn {
  // View
  activeView: SpainDataView;
  setActiveView: (view: SpainDataView) => void;

  // Catalogues (publishers)
  catalogues: SpainCatalogue[];
  selectedCatalogue: string | null;
  setSelectedCatalogue: (id: string | null) => void;
  fetchCatalogues: () => Promise<void>;

  // Datasets
  datasets: SpainDataset[];
  selectedDataset: string | null;
  setSelectedDataset: (id: string | null) => void;
  fetchDatasets: (publisherId?: string) => Promise<void>;
  datasetsPage: number;
  setDatasetsPage: (page: number) => void;

  // Resources
  resources: SpainResource[];
  fetchResources: (datasetId: string) => Promise<void>;

  // Search
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  searchResults: SpainDataset[];
  searchDatasets: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

/** Extract a displayable title from the multilingual title field */
function extractTitle(title: string | Array<{ _lang: string; _value: string }> | undefined): string {
  if (!title) return '';
  if (typeof title === 'string') return title;
  if (Array.isArray(title)) {
    // Prefer English, then Spanish, then first available
    const en = title.find(t => t._lang === 'en');
    if (en) return en._value;
    const es = title.find(t => t._lang === 'es');
    if (es) return es._value;
    return title[0]?._value || '';
  }
  return String(title);
}

/** Extract format label from format field */
function extractFormat(format: string | { label: string } | undefined): string {
  if (!format) return '?';
  if (typeof format === 'string') {
    if (format.startsWith('http')) return format.split('/').pop()?.toUpperCase() || '?';
    return format;
  }
  if (typeof format === 'object' && 'label' in format) return format.label;
  return '?';
}

export function useSpainData(): UseSpainDataReturn {
  const [activeView, setActiveView] = useState<SpainDataView>('catalogues');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Catalogues
  const [catalogues, setCatalogues] = useState<SpainCatalogue[]>([]);
  const [selectedCatalogue, setSelectedCatalogue] = useState<string | null>(null);

  // Datasets
  const [datasets, setDatasets] = useState<SpainDataset[]>([]);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);
  const [datasetsPage, setDatasetsPage] = useState(1);

  // Resources
  const [resources, setResources] = useState<SpainResource[]>([]);

  // Search
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<SpainDataset[]>([]);

  const fetchCatalogues = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_spain_data_command', {
        command: 'catalogues',
        args: [] as string[],
      }) as string;

      const parsed: SpainDataResponse<{ items: SpainCatalogue[] }> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setCatalogues([]);
      } else {
        setCatalogues(parsed.data?.items || []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch catalogues: ${msg}`);
      setCatalogues([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const fetchDatasets = useCallback(async (publisherId?: string) => {
    setLoading(true);
    setError(null);
    try {
      const args: string[] = [];
      if (publisherId) args.push(publisherId);
      args.push(String(datasetsPage));
      args.push('50');

      const result = await invoke('execute_spain_data_command', {
        command: 'datasets',
        args,
      }) as string;

      const parsed: SpainDataResponse<{ items: SpainDataset[] }> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
      } else {
        setDatasets(parsed.data?.items || []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch datasets: ${msg}`);
      setDatasets([]);
    } finally {
      setLoading(false);
    }
  }, [datasetsPage]);

  const fetchResources = useCallback(async (datasetId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_spain_data_command', {
        command: 'resources',
        args: [datasetId],
      }) as string;

      const parsed: SpainDataResponse<SpainResource[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResources([]);
      } else {
        setResources(Array.isArray(parsed.data) ? parsed.data : []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch resources: ${msg}`);
      setResources([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const searchDatasetsAction = useCallback(async () => {
    if (!searchQuery.trim()) return;
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_spain_data_command', {
        command: 'search',
        args: [searchQuery.trim()],
      }) as string;

      const parsed: SpainDataResponse<{ items: SpainDataset[] }> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
      } else {
        setSearchResults(parsed.data?.items || []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to search datasets: ${msg}`);
      setSearchResults([]);
    } finally {
      setLoading(false);
    }
  }, [searchQuery]);

  const exportCSV = useCallback(() => {
    let csv = '';
    let filename = 'fincept_spain_';

    if (activeView === 'catalogues' && catalogues.length > 0) {
      csv = [
        '# Spain Open Data - Catalogues (Publishers)',
        '',
        'ID,URI',
        ...catalogues.map(c => `${c.extracted_id},${c.uri}`),
      ].join('\n');
      filename += 'catalogues.csv';
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# Spain Open Data - Datasets${selectedCatalogue ? ` for ${selectedCatalogue}` : ''}`,
        '',
        'ID,Title,Modified,Distributions',
        ...datasets.map(d => {
          const title = extractTitle(d.title);
          return `${d.extracted_id || ''},"${title.replace(/"/g, '""')}",${d.modified || ''},${d.distribution?.length || 0}`;
        }),
      ].join('\n');
      filename += `datasets_${selectedCatalogue || 'all'}.csv`;
    } else if (activeView === 'resources' && resources.length > 0) {
      csv = [
        `# Spain Open Data - Resources for ${selectedDataset}`,
        '',
        'Index,Title,Format,URL',
        ...resources.map((r, i) => {
          const title = extractTitle(r.title);
          return `${i},"${title.replace(/"/g, '""')}",${extractFormat(r.format)},${r.accessURL || ''}`;
        }),
      ].join('\n');
      filename += `resources_${selectedDataset || 'unknown'}.csv`;
    } else if (activeView === 'search' && searchResults.length > 0) {
      csv = [
        `# Spain Open Data - Search: "${searchQuery}"`,
        '',
        'ID,Title,Modified,Distributions',
        ...searchResults.map(d => {
          const title = extractTitle(d.title);
          return `${d.extracted_id || ''},"${title.replace(/"/g, '""')}",${d.modified || ''},${d.distribution?.length || 0}`;
        }),
      ].join('\n');
      filename += `search_${searchQuery.replace(/\s+/g, '_')}.csv`;
    } else {
      return;
    }

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  }, [activeView, catalogues, datasets, resources, searchResults, selectedCatalogue, selectedDataset, searchQuery]);

  return {
    activeView,
    setActiveView,
    catalogues,
    selectedCatalogue,
    setSelectedCatalogue,
    fetchCatalogues,
    datasets,
    selectedDataset,
    setSelectedDataset,
    fetchDatasets,
    datasetsPage,
    setDatasetsPage,
    resources,
    fetchResources,
    searchQuery,
    setSearchQuery,
    searchResults,
    searchDatasets: searchDatasetsAction,
    exportCSV,
    loading,
    error,
  };
}

export { extractTitle, extractFormat };
export default useSpainData;
