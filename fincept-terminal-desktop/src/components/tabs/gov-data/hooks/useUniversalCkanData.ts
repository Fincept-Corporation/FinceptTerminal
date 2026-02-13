// useUniversalCkanData Hook - Multi-portal CKAN data access
// Supports 8 CKAN portals: US, UK, AU, IT, BR, LV, SI, UY

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  CkanView,
  CkanOrganization,
  CkanDataset,
  CkanResource,
  CkanResponse,
} from '../types';

interface UseUniversalCkanDataReturn {
  // View
  activeView: CkanView;
  setActiveView: (view: CkanView) => void;

  // Portal selection
  activePortal: string;
  setActivePortal: (code: string) => void;

  // Organizations
  organizations: CkanOrganization[];
  selectedOrg: string | null;
  setSelectedOrg: (id: string | null) => void;
  fetchOrganizations: () => Promise<void>;

  // Datasets
  datasets: CkanDataset[];
  selectedDataset: string | null;
  setSelectedDataset: (id: string | null) => void;
  fetchDatasets: (orgId: string) => Promise<void>;
  datasetsTotalCount: number;

  // Resources
  resources: CkanResource[];
  fetchResources: (datasetId: string) => Promise<void>;

  // Search
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  searchResults: CkanDataset[];
  searchTotalCount: number;
  searchDatasets: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

function parseTags(tags: any): string[] {
  if (!Array.isArray(tags)) return [];
  return tags.map((t: any) => (typeof t === 'string' ? t : t?.name || t?.display_name || '')).filter(Boolean);
}

export function useUniversalCkanData(): UseUniversalCkanDataReturn {
  const [activeView, setActiveView] = useState<CkanView>('organizations');
  const [activePortal, setActivePortal] = useState('us');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Organizations
  const [organizations, setOrganizations] = useState<CkanOrganization[]>([]);
  const [selectedOrg, setSelectedOrg] = useState<string | null>(null);

  // Datasets
  const [datasets, setDatasets] = useState<CkanDataset[]>([]);
  const [datasetsTotalCount, setDatasetsTotalCount] = useState(0);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);

  // Resources
  const [resources, setResources] = useState<CkanResource[]>([]);

  // Search
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<CkanDataset[]>([]);
  const [searchTotalCount, setSearchTotalCount] = useState(0);

  const fetchOrganizations = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('list_ckan_organizations', {
        country: activePortal,
        limit: 200,
      }) as string;

      const parsed: CkanResponse<CkanOrganization[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setOrganizations([]);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        setOrganizations(data.map((o: any) => ({
          name: o.name || '',
          display_name: o.display_name || o.name || '',
          country: o.country || activePortal.toUpperCase(),
        })));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch organizations: ${msg}`);
      setOrganizations([]);
    } finally {
      setLoading(false);
    }
  }, [activePortal]);

  const fetchDatasets = useCallback(async (orgId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('search_ckan_datasets', {
        country: activePortal,
        query: null as string | null,
        limit: 50,
        organization: orgId,
      }) as string;

      const parsed: CkanResponse<any[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
        setDatasetsTotalCount(0);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        setDatasets(data.map((d: any) => ({
          id: d.id || d.name || '',
          name: d.name || '',
          display_name: d.display_name || d.title || d.name || '',
          title: d.title || d.display_name || '',
          notes: d.notes || '',
          organization_name: d.organization_name || d.organization?.title || '',
          organization: d.organization,
          num_resources: d.num_resources || 0,
          metadata_created: d.metadata_created || '',
          metadata_modified: d.metadata_modified || '',
          tags: d.tags || [],
          resource_types: d.resource_types || [],
        })));
        setDatasetsTotalCount(parsed.metadata?.count || data.length);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch datasets: ${msg}`);
      setDatasets([]);
    } finally {
      setLoading(false);
    }
  }, [activePortal]);

  const fetchResources = useCallback(async (datasetId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('get_ckan_dataset_resources', {
        country: activePortal,
        datasetId,
      }) as string;

      const parsed: CkanResponse<any[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResources([]);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        setResources(data.map((r: any) => ({
          id: r.id || '',
          name: r.name || r.description || '',
          description: r.description || '',
          format: r.format || '',
          url: r.url || '',
          size: r.size || null,
          mimetype: r.mimetype || null,
          created: r.created || null,
          last_modified: r.last_modified || null,
          dataset_name: r.dataset_name || '',
          dataset_title: r.dataset_title || '',
          organization: r.organization || '',
        })));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch resources: ${msg}`);
      setResources([]);
    } finally {
      setLoading(false);
    }
  }, [activePortal]);

  const searchDatasetsAction = useCallback(async () => {
    if (!searchQuery.trim()) return;
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('search_ckan_datasets', {
        country: activePortal,
        query: searchQuery.trim(),
        limit: 50,
        organization: null as string | null,
      }) as string;

      const parsed: CkanResponse<any[]> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
        setSearchTotalCount(0);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        setSearchResults(data.map((d: any) => ({
          id: d.id || d.name || '',
          name: d.name || '',
          display_name: d.display_name || d.title || d.name || '',
          title: d.title || d.display_name || '',
          notes: d.notes || '',
          organization_name: d.organization_name || d.organization?.title || '',
          organization: d.organization,
          num_resources: d.num_resources || 0,
          metadata_created: d.metadata_created || '',
          metadata_modified: d.metadata_modified || '',
          tags: d.tags || [],
          resource_types: d.resource_types || [],
        })));
        setSearchTotalCount(parsed.metadata?.count || data.length);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to search datasets: ${msg}`);
      setSearchResults([]);
    } finally {
      setLoading(false);
    }
  }, [activePortal, searchQuery]);

  const exportCSV = useCallback(() => {
    let csv = '';
    let filename = `fincept_ckan_${activePortal}_`;

    if (activeView === 'organizations' && organizations.length > 0) {
      csv = [
        `# CKAN Portal ${activePortal.toUpperCase()} - Organizations`,
        '',
        'Name,Display Name',
        ...organizations.map(o => `${o.name},"${(o.display_name || '').replace(/"/g, '""')}"`),
      ].join('\n');
      filename += 'organizations.csv';
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# CKAN Portal ${activePortal.toUpperCase()} - Datasets (Org: ${selectedOrg})`,
        '',
        'Name,Title,Organization,Resources,Modified,Tags',
        ...datasets.map(d =>
          `${d.name},"${(d.title || '').replace(/"/g, '""')}","${(d.organization_name || '').replace(/"/g, '""')}",${d.num_resources || 0},${d.metadata_modified || ''},"${parseTags(d.tags).join('; ')}"`
        ),
      ].join('\n');
      filename += `datasets_${selectedOrg || 'all'}.csv`;
    } else if (activeView === 'resources' && resources.length > 0) {
      csv = [
        `# CKAN Portal ${activePortal.toUpperCase()} - Resources (Dataset: ${selectedDataset})`,
        '',
        'ID,Name,Format,Size,URL,Last Modified',
        ...resources.map(r =>
          `${r.id},"${(r.name || '').replace(/"/g, '""')}",${r.format},${r.size || ''},${r.url},${r.last_modified || ''}`
        ),
      ].join('\n');
      filename += `resources_${selectedDataset || 'unknown'}.csv`;
    } else if (activeView === 'search' && searchResults.length > 0) {
      csv = [
        `# CKAN Portal ${activePortal.toUpperCase()} - Search: "${searchQuery}"`,
        '',
        'Name,Title,Organization,Resources,Modified,Tags',
        ...searchResults.map(d =>
          `${d.name},"${(d.title || '').replace(/"/g, '""')}","${(d.organization_name || '').replace(/"/g, '""')}",${d.num_resources || 0},${d.metadata_modified || ''},"${parseTags(d.tags).join('; ')}"`
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
  }, [activeView, activePortal, organizations, datasets, resources, searchResults, selectedOrg, selectedDataset, searchQuery]);

  return {
    activeView,
    setActiveView,
    activePortal,
    setActivePortal,
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
    exportCSV,
    loading,
    error,
  };
}

export default useUniversalCkanData;
