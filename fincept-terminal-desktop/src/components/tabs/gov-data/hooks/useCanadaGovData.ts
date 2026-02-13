// useCanadaGovData Hook - Fetch and manage Government of Canada open data

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  CanadaGovView,
  CanadaPublisher,
  CanadaDataset,
  CanadaResource,
  CanadaGovResponse,
} from '../types';

interface UseCanadaGovDataReturn {
  // View
  activeView: CanadaGovView;
  setActiveView: (view: CanadaGovView) => void;

  // Publishers
  publishers: CanadaPublisher[];
  selectedPublisher: string | null;
  setSelectedPublisher: (id: string | null) => void;
  fetchPublishers: () => Promise<void>;

  // Datasets
  datasets: CanadaDataset[];
  selectedDataset: string | null;
  setSelectedDataset: (id: string | null) => void;
  fetchDatasets: (publisherId: string) => Promise<void>;
  datasetsTotalCount: number;

  // Resources
  resources: CanadaResource[];
  fetchResources: (datasetId: string) => Promise<void>;

  // Search
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  searchResults: CanadaDataset[];
  searchTotalCount: number;
  searchDatasets: () => Promise<void>;

  // Recent
  recentDatasets: CanadaDataset[];
  fetchRecentDatasets: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

export function useCanadaGovData(): UseCanadaGovDataReturn {
  const [activeView, setActiveView] = useState<CanadaGovView>('publishers');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Publishers
  const [publishers, setPublishers] = useState<CanadaPublisher[]>([]);
  const [selectedPublisher, setSelectedPublisher] = useState<string | null>(null);

  // Datasets
  const [datasets, setDatasets] = useState<CanadaDataset[]>([]);
  const [datasetsTotalCount, setDatasetsTotalCount] = useState(0);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);

  // Resources
  const [resources, setResources] = useState<CanadaResource[]>([]);

  // Search
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<CanadaDataset[]>([]);
  const [searchTotalCount, setSearchTotalCount] = useState(0);

  // Recent
  const [recentDatasets, setRecentDatasets] = useState<CanadaDataset[]>([]);

  const fetchPublishers = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_canada_gov_command', {
        command: 'publishers',
        args: [] as string[],
      }) as string;

      const parsed: CanadaGovResponse<CanadaPublisher[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setPublishers([]);
      } else {
        setPublishers(parsed.data || []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch publishers: ${msg}`);
      setPublishers([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const fetchDatasets = useCallback(async (publisherId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_canada_gov_command', {
        command: 'datasets',
        args: [publisherId, '100'],
      }) as string;

      const parsed: CanadaGovResponse<CanadaDataset[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
        setDatasetsTotalCount(0);
      } else {
        setDatasets(parsed.data || []);
        setDatasetsTotalCount(parsed.metadata?.total_count || 0);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch datasets: ${msg}`);
      setDatasets([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const fetchResources = useCallback(async (datasetId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_canada_gov_command', {
        command: 'resources',
        args: [datasetId],
      }) as string;

      const parsed: CanadaGovResponse<CanadaResource[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResources([]);
      } else {
        setResources(parsed.data || []);
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
      const result = await invoke('execute_canada_gov_command', {
        command: 'search',
        args: [searchQuery.trim(), '50'],
      }) as string;

      const parsed: CanadaGovResponse<CanadaDataset[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
        setSearchTotalCount(0);
      } else {
        setSearchResults(parsed.data || []);
        setSearchTotalCount(parsed.metadata?.total_count || 0);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to search datasets: ${msg}`);
      setSearchResults([]);
    } finally {
      setLoading(false);
    }
  }, [searchQuery]);

  const fetchRecentDatasets = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_canada_gov_command', {
        command: 'recent-datasets',
        args: ['50'],
      }) as string;

      const parsed: CanadaGovResponse<CanadaDataset[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setRecentDatasets([]);
      } else {
        setRecentDatasets(parsed.data || []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch recent datasets: ${msg}`);
      setRecentDatasets([]);
    } finally {
      setLoading(false);
    }
  }, []);

  // CSV export for current view data
  const exportCSV = useCallback(() => {
    let csv = '';
    let filename = 'fincept_canada_gov_';

    if (activeView === 'publishers' && publishers.length > 0) {
      csv = [
        '# Government of Canada - Publishers',
        '',
        'ID,Name',
        ...publishers.map(p => `${p.id},"${p.name}"`),
      ].join('\n');
      filename += 'publishers.csv';
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# Government of Canada - Datasets for ${selectedPublisher}`,
        '',
        'ID,Title,Resources,Modified,Tags',
        ...datasets.map(d =>
          `${d.id},"${d.title.replace(/"/g, '""')}",${d.num_resources},${d.metadata_modified || ''},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += `datasets_${selectedPublisher || 'all'}.csv`;
    } else if (activeView === 'resources' && resources.length > 0) {
      csv = [
        `# Government of Canada - Resources for ${selectedDataset}`,
        '',
        'ID,Name,Format,Size,URL,Last Modified',
        ...resources.map(r =>
          `${r.id},"${(r.name || '').replace(/"/g, '""')}",${r.format},${r.size || ''},${r.url},${r.last_modified || ''}`
        ),
      ].join('\n');
      filename += `resources_${selectedDataset || 'unknown'}.csv`;
    } else if (activeView === 'search' && searchResults.length > 0) {
      csv = [
        `# Government of Canada - Search: "${searchQuery}"`,
        '',
        'ID,Title,Organization,Resources,Modified,Tags',
        ...searchResults.map(d =>
          `${d.id},"${d.title.replace(/"/g, '""')}",${d.organization || ''},${d.num_resources},${d.metadata_modified || ''},"${d.tags.join('; ')}"`
        ),
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
  }, [activeView, publishers, datasets, resources, searchResults, selectedPublisher, selectedDataset, searchQuery]);

  return {
    activeView,
    setActiveView,
    publishers,
    selectedPublisher,
    setSelectedPublisher,
    fetchPublishers,
    datasets,
    selectedDataset,
    setSelectedDataset,
    fetchDatasets,
    datasetsTotalCount,
    resources,
    fetchResources,
    searchQuery,
    setSearchQuery,
    searchResults,
    searchTotalCount,
    searchDatasets: searchDatasetsAction,
    recentDatasets,
    fetchRecentDatasets,
    exportCSV,
    loading,
    error,
  };
}

export default useCanadaGovData;
