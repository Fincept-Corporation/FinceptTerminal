// useSwissGovData Hook - Fetch and manage Swiss Government open data (opendata.swiss)

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  SwissGovView,
  SwissPublisher,
  SwissDataset,
  SwissResource,
  SwissGovResponse,
} from '../types';

interface UseSwissGovDataReturn {
  // View
  activeView: SwissGovView;
  setActiveView: (view: SwissGovView) => void;

  // Publishers
  publishers: SwissPublisher[];
  selectedPublisher: string | null;
  setSelectedPublisher: (id: string | null) => void;
  fetchPublishers: () => Promise<void>;

  // Datasets
  datasets: SwissDataset[];
  selectedDataset: string | null;
  setSelectedDataset: (id: string | null) => void;
  fetchDatasets: (publisherId: string) => Promise<void>;
  datasetsTotalCount: number;

  // Resources
  resources: SwissResource[];
  fetchResources: (datasetId: string) => Promise<void>;

  // Search
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  searchResults: SwissDataset[];
  searchTotalCount: number;
  searchDatasets: () => Promise<void>;

  // Recent
  recentDatasets: SwissDataset[];
  fetchRecentDatasets: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

function parseDatasets(data: any): SwissDataset[] {
  if (!Array.isArray(data)) return [];
  return data.map((d: any) => ({
    id: d.id || '',
    name: d.name || '',
    title: d.title || d.name || '',
    original_title: d.original_title || d.title || '',
    notes: d.notes || '',
    original_notes: d.original_notes || d.notes || '',
    publisher_id: d.publisher_id || '',
    organization: d.organization || null,
    metadata_created: d.metadata_created || null,
    metadata_modified: d.metadata_modified || null,
    state: d.state || '',
    num_resources: d.num_resources || 0,
    tags: d.tags || [],
    language_detected: d.language_detected || undefined,
  }));
}

export function useSwissGovData(): UseSwissGovDataReturn {
  const [activeView, setActiveView] = useState<SwissGovView>('publishers');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Publishers
  const [publishers, setPublishers] = useState<SwissPublisher[]>([]);
  const [selectedPublisher, setSelectedPublisher] = useState<string | null>(null);

  // Datasets
  const [datasets, setDatasets] = useState<SwissDataset[]>([]);
  const [datasetsTotalCount, setDatasetsTotalCount] = useState(0);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);

  // Resources
  const [resources, setResources] = useState<SwissResource[]>([]);

  // Search
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<SwissDataset[]>([]);
  const [searchTotalCount, setSearchTotalCount] = useState(0);

  // Recent
  const [recentDatasets, setRecentDatasets] = useState<SwissDataset[]>([]);

  const fetchPublishers = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_swiss_gov_command', {
        command: 'publishers',
        args: [] as string[],
      }) as string;

      const parsed: SwissGovResponse<SwissPublisher[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setPublishers([]);
      } else {
        const data = parsed.data || [];
        setPublishers(data.map((p: any) => ({
          id: p.id || '',
          name: p.name || p.display_name || '',
          original_name: p.original_name || p.name || '',
          display_name: p.display_name || p.name || '',
        })));
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
      const result = await invoke('execute_swiss_gov_command', {
        command: 'datasets',
        args: [publisherId, '100'],
      }) as string;

      const parsed: SwissGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
        setDatasetsTotalCount(0);
      } else {
        setDatasets(parseDatasets(parsed.data));
        setDatasetsTotalCount(parsed.metadata?.total_count || parsed.metadata?.returned_count || 0);
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
      const result = await invoke('execute_swiss_gov_command', {
        command: 'resources',
        args: [datasetId],
      }) as string;

      const parsed: SwissGovResponse<any[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResources([]);
      } else {
        const data = parsed.data || [];
        setResources(data.map((r: any) => ({
          id: r.id || '',
          name: r.name || '',
          original_name: r.original_name || r.name || '',
          description: r.description || '',
          original_description: r.original_description || r.description || '',
          format: r.format || '',
          url: r.url || '',
          size: r.size || null,
          mimetype: r.mimetype || null,
          created: r.created || null,
          last_modified: r.last_modified || null,
          resource_type: r.resource_type || null,
          package_id: r.package_id || datasetId,
          position: r.position ?? null,
          language_detected: r.language_detected || undefined,
        })));
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
      const result = await invoke('execute_swiss_gov_command', {
        command: 'search',
        args: [searchQuery.trim(), '50'],
      }) as string;

      const parsed: SwissGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
        setSearchTotalCount(0);
      } else {
        setSearchResults(parseDatasets(parsed.data));
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
      const result = await invoke('execute_swiss_gov_command', {
        command: 'recent-datasets',
        args: ['50'],
      }) as string;

      const parsed: SwissGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setRecentDatasets([]);
      } else {
        setRecentDatasets(parseDatasets(parsed.data));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch recent datasets: ${msg}`);
      setRecentDatasets([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const exportCSV = useCallback(() => {
    let csv = '';
    let filename = 'fincept_swiss_gov_';

    if (activeView === 'publishers' && publishers.length > 0) {
      csv = [
        '# opendata.swiss - Publishers',
        '',
        'ID,Name,Original Name',
        ...publishers.map(p => `${p.id},"${(p.name || '').replace(/"/g, '""')}","${(p.original_name || '').replace(/"/g, '""')}"`),
      ].join('\n');
      filename += 'publishers.csv';
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# opendata.swiss - Datasets for ${selectedPublisher}`,
        '',
        'ID,Title,Original Title,Resources,Modified,Tags',
        ...datasets.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.original_title || '').replace(/"/g, '""')}",${d.num_resources},${d.metadata_modified || ''},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += `datasets_${selectedPublisher || 'all'}.csv`;
    } else if (activeView === 'resources' && resources.length > 0) {
      csv = [
        `# opendata.swiss - Resources for ${selectedDataset}`,
        '',
        'ID,Name,Original Name,Format,Size,URL,Last Modified',
        ...resources.map(r =>
          `${r.id},"${(r.name || '').replace(/"/g, '""')}","${(r.original_name || '').replace(/"/g, '""')}",${r.format},${r.size || ''},${r.url},${r.last_modified || ''}`
        ),
      ].join('\n');
      filename += `resources_${selectedDataset || 'unknown'}.csv`;
    } else if (activeView === 'search' && searchResults.length > 0) {
      csv = [
        `# opendata.swiss - Search: "${searchQuery}"`,
        '',
        'ID,Title,Original Title,Organization,Resources,Modified,Tags',
        ...searchResults.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.original_title || '').replace(/"/g, '""')}",${d.organization || ''},${d.num_resources},${d.metadata_modified || ''},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += `search_${searchQuery.replace(/\s+/g, '_')}.csv`;
    } else if (activeView === 'recent' && recentDatasets.length > 0) {
      csv = [
        '# opendata.swiss - Recent Datasets',
        '',
        'ID,Title,Original Title,Organization,Resources,Modified,Tags',
        ...recentDatasets.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.original_title || '').replace(/"/g, '""')}",${d.organization || ''},${d.num_resources},${d.metadata_modified || ''},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += 'recent.csv';
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
  }, [activeView, publishers, datasets, resources, searchResults, recentDatasets, selectedPublisher, selectedDataset, searchQuery]);

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

export default useSwissGovData;
