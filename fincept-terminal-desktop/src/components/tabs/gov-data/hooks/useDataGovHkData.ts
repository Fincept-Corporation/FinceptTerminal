// useDataGovHkData Hook - Fetch and manage Hong Kong Government open data (data.gov.hk)
// CKAN Catalog API (categories/groups, datasets, resources) + Historical Archive API

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  HkGovView,
  HkCategory,
  HkDataset,
  HkResource,
  HkHistoricalFile,
  HkGovResponse,
} from '../types';

interface UseDataGovHkDataReturn {
  // View
  activeView: HkGovView;
  setActiveView: (view: HkGovView) => void;

  // Categories (groups)
  categories: HkCategory[];
  selectedCategory: string | null;
  setSelectedCategory: (id: string | null) => void;
  fetchCategories: () => Promise<void>;

  // Datasets
  datasets: HkDataset[];
  selectedDataset: string | null;
  setSelectedDataset: (id: string | null) => void;
  fetchDatasets: (categoryId: string) => Promise<void>;
  fetchAllDatasets: (limit?: number) => Promise<void>;

  // Resources (from dataset details)
  resources: HkResource[];
  fetchResources: (datasetId: string) => Promise<void>;

  // Historical archive
  historicalFiles: HkHistoricalFile[];
  historicalStartDate: string;
  setHistoricalStartDate: (d: string) => void;
  historicalEndDate: string;
  setHistoricalEndDate: (d: string) => void;
  historicalCategory: string;
  setHistoricalCategory: (c: string) => void;
  historicalFormat: string;
  setHistoricalFormat: (f: string) => void;
  fetchHistoricalFiles: () => Promise<void>;

  // Search (uses datasets_list with search-like browsing)
  searchQuery: string;
  setSearchQuery: (query: string) => void;
  searchResults: HkDataset[];
  searchDatasets: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

function parseDatasets(data: any): HkDataset[] {
  if (!Array.isArray(data)) return [];
  return data.map((d: any) => ({
    id: d.id || d.name || '',
    name: d.name || '',
    title: d.title || d.name || '',
    notes: d.notes || '',
    organization: d.organization || undefined,
    num_resources: d.num_resources || d.resources?.length || 0,
    metadata_created: d.metadata_created || null,
    metadata_modified: d.metadata_modified || null,
    tags: Array.isArray(d.tags)
      ? d.tags.map((t: any) => (typeof t === 'string' ? t : t?.display_name || t?.name || '')).filter(Boolean)
      : [],
    resources: d.resources || undefined,
  }));
}

function parseResources(data: any): HkResource[] {
  if (!Array.isArray(data)) return [];
  return data.map((r: any) => ({
    id: r.id || '',
    name: r.name || r.description || '',
    description: r.description || '',
    format: r.format || '',
    url: r.url || '',
    size: r.size || null,
    mimetype: r.mimetype || null,
    created: r.created || null,
    last_modified: r.last_modified || null,
    package_id: r.package_id || '',
  }));
}

// Default date range: last 30 days in YYYYMMDD format
function defaultStartDate(): string {
  const d = new Date();
  d.setDate(d.getDate() - 30);
  return d.toISOString().slice(0, 10).replace(/-/g, '');
}

function defaultEndDate(): string {
  return new Date().toISOString().slice(0, 10).replace(/-/g, '');
}

export function useDataGovHkData(): UseDataGovHkDataReturn {
  const [activeView, setActiveView] = useState<HkGovView>('categories');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Categories
  const [categories, setCategories] = useState<HkCategory[]>([]);
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);

  // Datasets
  const [datasets, setDatasets] = useState<HkDataset[]>([]);
  const [selectedDataset, setSelectedDataset] = useState<string | null>(null);

  // Resources
  const [resources, setResources] = useState<HkResource[]>([]);

  // Historical
  const [historicalFiles, setHistoricalFiles] = useState<HkHistoricalFile[]>([]);
  const [historicalStartDate, setHistoricalStartDate] = useState(defaultStartDate());
  const [historicalEndDate, setHistoricalEndDate] = useState(defaultEndDate());
  const [historicalCategory, setHistoricalCategory] = useState('');
  const [historicalFormat, setHistoricalFormat] = useState('');

  // Search
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<HkDataset[]>([]);

  // Fetch categories (groups)
  const fetchCategories = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_data_gov_hk_command', {
        command: 'categories_list',
        args: [] as string[],
      }) as string;

      const parsed: HkGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setCategories([]);
      } else {
        // categories_list returns an array of category name strings
        const data = parsed.data;
        if (Array.isArray(data)) {
          // If simple string array, convert to HkCategory objects
          if (typeof data[0] === 'string') {
            setCategories(data.map((name: string) => ({
              id: name,
              name,
              display_name: name,
              title: name,
              description: '',
              dataset_count: 0,
            })));
          } else {
            // Already objects
            setCategories(data.map((c: any) => ({
              id: c.id || c.name || '',
              name: c.name || '',
              display_name: c.display_name || c.title || c.name || '',
              title: c.title || c.display_name || c.name || '',
              description: c.description || '',
              dataset_count: c.package_count || c.dataset_count || 0,
            })));
          }
        } else {
          setCategories([]);
        }
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch categories: ${msg}`);
      setCategories([]);
    } finally {
      setLoading(false);
    }
  }, []);

  // Fetch datasets for a category (using category_details which returns group_show with packages)
  const fetchDatasets = useCallback(async (categoryId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_data_gov_hk_command', {
        command: 'category_details',
        args: [categoryId],
      }) as string;

      const parsed: HkGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
      } else {
        // group_show returns { packages: [...], ... }
        const packages = parsed.data?.packages || [];
        setDatasets(parseDatasets(packages));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch datasets: ${msg}`);
      setDatasets([]);
    } finally {
      setLoading(false);
    }
  }, []);

  // Fetch all datasets (paginated package_list)
  const fetchAllDatasets = useCallback(async (limit: number = 50) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_data_gov_hk_command', {
        command: 'datasets_list',
        args: [String(limit)],
      }) as string;

      const parsed: HkGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
      } else {
        // datasets_list returns string[] of dataset names
        const data = parsed.data;
        if (Array.isArray(data) && data.length > 0 && typeof data[0] === 'string') {
          setSearchResults(data.map((name: string) => ({
            id: name,
            name,
            title: name,
            notes: '',
            num_resources: 0,
            metadata_created: null,
            metadata_modified: null,
            tags: [],
          })));
        } else {
          setSearchResults(parseDatasets(data));
        }
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch datasets: ${msg}`);
      setSearchResults([]);
    } finally {
      setLoading(false);
    }
  }, []);

  // Fetch resources for a dataset (using dataset_details → package_show)
  const fetchResources = useCallback(async (datasetId: string) => {
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_data_gov_hk_command', {
        command: 'dataset_details',
        args: [datasetId],
      }) as string;

      const parsed: HkGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResources([]);
      } else {
        const resData = parsed.data?.resources || [];
        setResources(parseResources(resData));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch resources: ${msg}`);
      setResources([]);
    } finally {
      setLoading(false);
    }
  }, []);

  // Fetch historical archive files
  const fetchHistoricalFiles = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const args = [historicalStartDate, historicalEndDate];
      if (historicalCategory) args.push(historicalCategory);
      else if (historicalFormat) args.push(''); // placeholder for category
      if (historicalFormat) args.push(historicalFormat);

      const result = await invoke('execute_data_gov_hk_command', {
        command: 'historical_files',
        args,
      }) as string;

      const parsed: HkGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setHistoricalFiles([]);
      } else {
        const data = parsed.data;
        if (Array.isArray(data)) {
          setHistoricalFiles(data.map((f: any) => ({
            name: f.name || f.dataset_name || f.file_name || '',
            url: f.url || f.download_url || '',
            format: f.format || f.file_format || '',
            category: f.category || '',
            date: f.date || f.snapshot_date || '',
            size: f.size || f.file_size || null,
          })));
        } else if (data && typeof data === 'object') {
          // Could be a nested structure with files array
          const files = data.files || data.results || [];
          setHistoricalFiles(Array.isArray(files) ? files.map((f: any) => ({
            name: f.name || f.dataset_name || f.file_name || '',
            url: f.url || f.download_url || '',
            format: f.format || f.file_format || '',
            category: f.category || '',
            date: f.date || f.snapshot_date || '',
            size: f.size || f.file_size || null,
          })) : []);
        } else {
          setHistoricalFiles([]);
        }
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch historical files: ${msg}`);
      setHistoricalFiles([]);
    } finally {
      setLoading(false);
    }
  }, [historicalStartDate, historicalEndDate, historicalCategory, historicalFormat]);

  // Search datasets (browse all and filter, since HK CKAN doesn't have fq-based search via our commands)
  const searchDatasetsAction = useCallback(async () => {
    if (!searchQuery.trim()) return;
    setLoading(true);
    setError(null);
    try {
      // Use datasets_list to get names, then we can filter client-side
      // or use catalogue_overview for broader results
      const result = await invoke('execute_data_gov_hk_command', {
        command: 'datasets_list',
        args: ['200'],
      }) as string;

      const parsed: HkGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setSearchResults([]);
      } else {
        const data = parsed.data;
        const query = searchQuery.trim().toLowerCase();
        if (Array.isArray(data)) {
          if (typeof data[0] === 'string') {
            const filtered = (data as string[]).filter(n => n.toLowerCase().includes(query));
            setSearchResults(filtered.map((name: string) => ({
              id: name,
              name,
              title: name,
              notes: '',
              num_resources: 0,
              metadata_created: null,
              metadata_modified: null,
              tags: [],
            })));
          } else {
            const all = parseDatasets(data);
            setSearchResults(all.filter(d =>
              d.title.toLowerCase().includes(query) ||
              d.name.toLowerCase().includes(query) ||
              d.notes.toLowerCase().includes(query)
            ));
          }
        } else {
          setSearchResults([]);
        }
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
    let filename = 'fincept_hk_gov_';

    if (activeView === 'categories' && categories.length > 0) {
      csv = [
        '# Data.gov.hk - Categories',
        '',
        'ID,Name,Datasets',
        ...categories.map(c => `${c.id},"${(c.display_name || '').replace(/"/g, '""')}",${c.dataset_count}`),
      ].join('\n');
      filename += 'categories.csv';
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# Data.gov.hk - Datasets (Category: ${selectedCategory})`,
        '',
        'Name,Title,Resources,Modified,Tags',
        ...datasets.map(d =>
          `${d.name},"${(d.title || '').replace(/"/g, '""')}",${d.num_resources},${d.metadata_modified || ''},"${d.tags.join('; ')}"`
        ),
      ].join('\n');
      filename += `datasets_${selectedCategory || 'all'}.csv`;
    } else if (activeView === 'resources' && resources.length > 0) {
      csv = [
        `# Data.gov.hk - Resources (Dataset: ${selectedDataset})`,
        '',
        'ID,Name,Format,Size,URL,Last Modified',
        ...resources.map(r =>
          `${r.id},"${(r.name || '').replace(/"/g, '""')}",${r.format},${r.size || ''},${r.url},${r.last_modified || ''}`
        ),
      ].join('\n');
      filename += `resources_${selectedDataset || 'unknown'}.csv`;
    } else if (activeView === 'historical' && historicalFiles.length > 0) {
      csv = [
        `# Data.gov.hk - Historical Files (${historicalStartDate} to ${historicalEndDate})`,
        '',
        'Name,Format,Category,Date,Size,URL',
        ...historicalFiles.map(f =>
          `"${(f.name || '').replace(/"/g, '""')}",${f.format},${f.category},${f.date},${f.size || ''},${f.url}`
        ),
      ].join('\n');
      filename += `historical_${historicalStartDate}_${historicalEndDate}.csv`;
    } else if (activeView === 'search' && searchResults.length > 0) {
      csv = [
        `# Data.gov.hk - Search: "${searchQuery}"`,
        '',
        'Name,Title,Resources,Tags',
        ...searchResults.map(d =>
          `${d.name},"${(d.title || '').replace(/"/g, '""')}",${d.num_resources},"${d.tags.join('; ')}"`
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
  }, [activeView, categories, datasets, resources, historicalFiles, searchResults, selectedCategory, selectedDataset, searchQuery, historicalStartDate, historicalEndDate]);

  return {
    activeView,
    setActiveView,
    categories,
    selectedCategory,
    setSelectedCategory,
    fetchCategories,
    datasets,
    selectedDataset,
    setSelectedDataset,
    fetchDatasets,
    fetchAllDatasets,
    resources,
    fetchResources,
    historicalFiles,
    historicalStartDate,
    setHistoricalStartDate,
    historicalEndDate,
    setHistoricalEndDate,
    historicalCategory,
    setHistoricalCategory,
    historicalFormat,
    setHistoricalFormat,
    fetchHistoricalFiles,
    searchQuery,
    setSearchQuery,
    searchResults,
    searchDatasets: searchDatasetsAction,
    exportCSV,
    loading,
    error,
  };
}

export default useDataGovHkData;
