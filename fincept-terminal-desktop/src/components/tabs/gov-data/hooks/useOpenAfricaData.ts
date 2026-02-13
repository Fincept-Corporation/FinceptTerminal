// useOpenAfricaData Hook - Fetch and manage openAFRICA open data portal

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  OpenAfricaView,
  OpenAfricaOrganization,
  OpenAfricaDataset,
  OpenAfricaResource,
  OpenAfricaResponse,
} from '../types';

interface UseOpenAfricaDataReturn {
  // View
  activeView: OpenAfricaView;
  setActiveView: (view: OpenAfricaView) => void;

  // Organizations
  organizations: OpenAfricaOrganization[];
  selectedOrg: string | null;
  setSelectedOrg: (id: string | null) => void;
  fetchOrganizations: () => Promise<void>;

  // Datasets
  datasets: OpenAfricaDataset[];
  selectedDataset: string | null;
  setSelectedDataset: (id: string | null) => void;
  fetchDatasets: (orgId: string) => Promise<void>;
  datasetsTotalCount: number;

  // Resources
  resources: OpenAfricaResource[];
  fetchResources: (datasetId: string) => Promise<void>;

  // Search
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  searchResults: OpenAfricaDataset[];
  searchTotalCount: number;
  searchDatasets: () => Promise<void>;

  // Recent
  recentDatasets: OpenAfricaDataset[];
  fetchRecentDatasets: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

// Parse datasets from the search/recent response format
function parseDatasets(data: any): OpenAfricaDataset[] {
  const raw = data?.datasets || data || [];
  if (!Array.isArray(raw)) return [];
  return raw.map((d: any) => ({
    id: d.id || '',
    name: d.name || '',
    title: d.title || '',
    notes: d.notes || '',
    organization: d.organization || {},
    author: d.author || '',
    license: d.license || '',
    created: d.created || '',
    modified: d.modified || '',
    tags: d.tags || [],
    resource_count: d.resource_count || 0,
  }));
}

export function useOpenAfricaData(): UseOpenAfricaDataReturn {
  const [activeView, setActiveView] = useState<OpenAfricaView>('organizations');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Organizations
  const [organizations, setOrganizations] = useState<OpenAfricaOrganization[]>([]);
  const [selectedOrg, setSelectedOrg] = useState<string | null>(null);

  // Datasets
  const [datasets, setDatasets] = useState<OpenAfricaDataset[]>([]);
  const [datasetsTotalCount, setDatasetsTotalCount] = useState(0);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);

  // Resources
  const [resources, setResources] = useState<OpenAfricaResource[]>([]);

  // Search
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<OpenAfricaDataset[]>([]);
  const [searchTotalCount, setSearchTotalCount] = useState(0);

  // Recent
  const [recentDatasets, setRecentDatasets] = useState<OpenAfricaDataset[]>([]);

  const fetchOrganizations = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_openafrica_command', {
        command: 'organizations',
        args: [] as string[],
      }) as string;

      const parsed: OpenAfricaResponse<OpenAfricaOrganization[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setOrganizations([]);
      } else {
        setOrganizations(parsed.data || []);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch organizations: ${msg}`);
      setOrganizations([]);
    } finally {
      setLoading(false);
    }
  }, []);

  const fetchDatasets = useCallback(async (orgId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_openafrica_command', {
        command: 'org-datasets',
        args: [orgId, '100'],
      }) as string;

      const parsed: OpenAfricaResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
        setDatasetsTotalCount(0);
      } else {
        const ds = parseDatasets(parsed.data);
        setDatasets(ds);
        setDatasetsTotalCount(parsed.data?.count || parsed.metadata?.total_count || ds.length);
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
      const result = await invoke('execute_openafrica_command', {
        command: 'dataset-details',
        args: [datasetId],
      }) as string;

      const parsed: OpenAfricaResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResources([]);
      } else {
        const rawResources = parsed.data?.resources || [];
        setResources(rawResources.map((r: any) => ({
          id: r.id || '',
          name: r.name || '',
          description: r.description || '',
          url: r.url || '',
          format: r.format || '',
          size: r.size || null,
          created: r.created || '',
          last_modified: r.last_modified || '',
          package_id: r.package_id || '',
          position: r.position || 0,
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
      const result = await invoke('execute_openafrica_command', {
        command: 'search',
        args: [searchQuery.trim(), '50'],
      }) as string;

      const parsed: OpenAfricaResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
        setSearchTotalCount(0);
      } else {
        const ds = parseDatasets(parsed.data);
        setSearchResults(ds);
        setSearchTotalCount(parsed.data?.count || parsed.metadata?.total_count || ds.length);
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
      const result = await invoke('execute_openafrica_command', {
        command: 'recent',
        args: ['50'],
      }) as string;

      const parsed: OpenAfricaResponse<any> = JSON.parse(result);
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
    let filename = 'fincept_openafrica_';

    if (activeView === 'organizations' && organizations.length > 0) {
      csv = [
        '# openAFRICA - Organizations',
        '',
        'ID,Name,Title,Datasets,State',
        ...organizations.map(o =>
          `${o.id},"${(o.name || '').replace(/"/g, '""')}","${(o.title || '').replace(/"/g, '""')}",${o.dataset_count},${o.state}`
        ),
      ].join('\n');
      filename += 'organizations.csv';
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# openAFRICA - Datasets for ${selectedOrg}`,
        '',
        'ID,Title,Author,License,Resources,Modified,Tags',
        ...datasets.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.author || '').replace(/"/g, '""')}","${d.license}",${d.resource_count},${d.modified},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += `datasets_${selectedOrg || 'all'}.csv`;
    } else if (activeView === 'resources' && resources.length > 0) {
      csv = [
        `# openAFRICA - Resources for ${selectedDataset}`,
        '',
        'ID,Name,Format,Size,URL,Last Modified',
        ...resources.map(r =>
          `${r.id},"${(r.name || '').replace(/"/g, '""')}",${r.format},${r.size || ''},${r.url},${r.last_modified || ''}`
        ),
      ].join('\n');
      filename += `resources_${selectedDataset || 'unknown'}.csv`;
    } else if (activeView === 'search' && searchResults.length > 0) {
      csv = [
        `# openAFRICA - Search: "${searchQuery}"`,
        '',
        'ID,Title,Author,License,Resources,Modified,Tags',
        ...searchResults.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.author || '').replace(/"/g, '""')}","${d.license}",${d.resource_count},${d.modified},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += `search_${searchQuery.replace(/\s+/g, '_')}.csv`;
    } else if (activeView === 'recent' && recentDatasets.length > 0) {
      csv = [
        '# openAFRICA - Recent Datasets',
        '',
        'ID,Title,Author,License,Resources,Modified,Tags',
        ...recentDatasets.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.author || '').replace(/"/g, '""')}","${d.license}",${d.resource_count},${d.modified},"${d.tags.join('; ')}"`
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
  }, [activeView, organizations, datasets, resources, searchResults, recentDatasets, selectedOrg, selectedDataset, searchQuery]);

  return {
    activeView,
    setActiveView,
    organizations,
    selectedOrg,
    setSelectedOrg,
    fetchOrganizations,
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

export default useOpenAfricaData;
