// useFrenchGovData Hook - French Government APIs (data.gouv.fr, API Géo, Tabulaire, Entreprise)

import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type {
  FrenchGovView,
  FrenchMunicipality,
  FrenchDepartment,
  FrenchRegion,
  FrenchDataset,
  FrenchResourceProfile,
  FrenchResourceLines,
  FrenchCompany,
  FrenchGovResponse,
} from '../types';

// Geo search type
export type GeoSearchType = 'municipalities' | 'departments' | 'regions';

interface UseFrenchGovDataReturn {
  // View
  activeView: FrenchGovView;
  setActiveView: (view: FrenchGovView) => void;

  // Geo
  geoSearchType: GeoSearchType;
  setGeoSearchType: (type: GeoSearchType) => void;
  geoQuery: string;
  setGeoQuery: (query: string) => void;
  municipalities: FrenchMunicipality[];
  departments: FrenchDepartment[];
  regions: FrenchRegion[];
  fetchGeo: () => Promise<void>;

  // Datasets (data.gouv.fr catalog)
  datasetQuery: string;
  setDatasetQuery: (query: string) => void;
  datasets: FrenchDataset[];
  datasetsTotalCount: number;
  datasetsPage: number;
  fetchDatasets: (page?: number) => Promise<void>;

  // Tabular
  tabularResourceId: string;
  setTabularResourceId: (id: string) => void;
  resourceProfile: FrenchResourceProfile | null;
  resourceLines: FrenchResourceLines | null;
  fetchProfile: () => Promise<void>;
  fetchLines: (page?: number) => Promise<void>;

  // Company
  companyQuery: string;
  setCompanyQuery: (query: string) => void;
  companies: FrenchCompany[];
  fetchCompany: () => Promise<void>;

  // CSV export
  exportCSV: () => void;

  // Shared state
  loading: boolean;
  error: string | null;
}

export function useFrenchGovData(): UseFrenchGovDataReturn {
  const [activeView, setActiveView] = useState<FrenchGovView>('geo');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Geo state
  const [geoSearchType, setGeoSearchType] = useState<GeoSearchType>('municipalities');
  const [geoQuery, setGeoQuery] = useState('');
  const [municipalities, setMunicipalities] = useState<FrenchMunicipality[]>([]);
  const [departments, setDepartments] = useState<FrenchDepartment[]>([]);
  const [regions, setRegions] = useState<FrenchRegion[]>([]);

  // Datasets state
  const [datasetQuery, setDatasetQuery] = useState('');
  const [datasets, setDatasets] = useState<FrenchDataset[]>([]);
  const [datasetsTotalCount, setDatasetsTotalCount] = useState(0);
  const [datasetsPage, setDatasetsPage] = useState(1);

  // Tabular state
  const [tabularResourceId, setTabularResourceId] = useState('');
  const [resourceProfile, setResourceProfile] = useState<FrenchResourceProfile | null>(null);
  const [resourceLines, setResourceLines] = useState<FrenchResourceLines | null>(null);

  // Company state
  const [companyQuery, setCompanyQuery] = useState('');
  const [companies, setCompanies] = useState<FrenchCompany[]>([]);

  // ===== GEO =====
  const fetchGeo = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const args: string[] = geoQuery.trim() ? [geoQuery.trim(), '50'] : [];

      const result = await invoke('execute_french_gov_command', {
        command: geoSearchType,
        args,
      }) as string;

      const parsed: FrenchGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        if (geoSearchType === 'municipalities') {
          setMunicipalities(data.map((m: any) => ({
            code: m.code || '',
            name: m.name || m.original_name || '',
            original_name: m.original_name || m.name || '',
            postal_codes: m.postal_codes || [],
            department_code: m.department_code || null,
            region_code: m.region_code || null,
            population: m.population || null,
            area: m.area || null,
            center: m.center || null,
            search_score: m.search_score || 0,
          })));
        } else if (geoSearchType === 'departments') {
          setDepartments(data.map((d: any) => ({
            code: d.code || '',
            name: d.name || d.original_name || '',
            original_name: d.original_name || d.name || '',
            region_code: d.region_code || null,
            search_score: d.search_score || 0,
          })));
        } else if (geoSearchType === 'regions') {
          setRegions(data.map((r: any) => ({
            code: r.code || '',
            name: r.name || r.original_name || '',
            original_name: r.original_name || r.name || '',
            search_score: r.search_score || 0,
          })));
        }
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch geo data: ${msg}`);
    } finally {
      setLoading(false);
    }
  }, [geoSearchType, geoQuery]);

  // ===== DATASETS =====
  const fetchDatasets = useCallback(async (page: number = 1) => {
    setLoading(true);
    setError(null);
    try {
      const args: string[] = [];
      if (datasetQuery.trim()) {
        args.push(datasetQuery.trim());
      }
      args.push(String(page));
      args.push('20');

      const result = await invoke('execute_french_gov_command', {
        command: 'datasets',
        args,
      }) as string;

      const parsed: FrenchGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setDatasets([]);
        setDatasetsTotalCount(0);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        setDatasets(data.map((d: any) => ({
          id: d.id || '',
          title: d.title || '',
          original_title: d.original_title || d.title || '',
          description: d.description || '',
          original_description: d.original_description || d.description || '',
          url: d.url || null,
          owner: d.owner || null,
          organization: d.organization || null,
          created_at: d.created_at || null,
          last_modified: d.last_modified || null,
          tags: d.tags || [],
          frequency: d.frequency || null,
          license: d.license || null,
          resources_count: d.resources_count || 0,
        })));
        setDatasetsTotalCount(parsed.metadata?.pagination?.total || data.length);
        setDatasetsPage(page);
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch datasets: ${msg}`);
      setDatasets([]);
    } finally {
      setLoading(false);
    }
  }, [datasetQuery]);

  // ===== TABULAR =====
  const fetchProfile = useCallback(async () => {
    if (!tabularResourceId.trim()) return;
    setLoading(true);
    setError(null);
    setResourceLines(null);
    try {
      const result = await invoke('execute_french_gov_command', {
        command: 'profile',
        args: [tabularResourceId.trim()],
      }) as string;

      const parsed: FrenchGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResourceProfile(null);
      } else {
        const d = parsed.data || {};
        setResourceProfile({
          resource_id: d.resource_id || tabularResourceId.trim(),
          name: d.name || '',
          original_name: d.original_name || d.name || '',
          title: d.title || '',
          original_title: d.original_title || d.title || '',
          format: d.format || null,
          size: d.size || null,
          columns: (d.columns || []).map((c: any) => ({
            name: c.name || '',
            original_name: c.original_name || c.name || '',
            title: c.title || '',
            original_title: c.original_title || c.title || '',
            type: c.type || null,
            format: c.format || null,
            description: c.description || '',
            original_description: c.original_description || c.description || '',
          })),
          total_rows: d.total_rows || null,
          encoding: d.encoding || null,
          delimiter: d.delimiter || null,
        });
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch profile: ${msg}`);
      setResourceProfile(null);
    } finally {
      setLoading(false);
    }
  }, [tabularResourceId]);

  const fetchLines = useCallback(async (page: number = 1) => {
    if (!tabularResourceId.trim()) return;
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_french_gov_command', {
        command: 'lines',
        args: [tabularResourceId.trim(), String(page), '20'],
      }) as string;

      const parsed: FrenchGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setResourceLines(null);
      } else {
        const d = parsed.data || {};
        setResourceLines({
          resource_id: d.resource_id || tabularResourceId.trim(),
          lines: d.lines || [],
          total_rows: d.total_rows || 0,
          page: d.page || page,
          page_size: d.page_size || 20,
          has_more: d.has_more || false,
        });
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch lines: ${msg}`);
      setResourceLines(null);
    } finally {
      setLoading(false);
    }
  }, [tabularResourceId]);

  // ===== COMPANY =====
  const fetchCompany = useCallback(async () => {
    if (!companyQuery.trim()) return;
    setLoading(true);
    setError(null);
    try {
      const result = await invoke('execute_french_gov_command', {
        command: 'company',
        args: [companyQuery.trim()],
      }) as string;

      const parsed: FrenchGovResponse<any> = JSON.parse(result);
      if (parsed.error) {
        setError(parsed.error);
        setCompanies([]);
      } else {
        const data = Array.isArray(parsed.data) ? parsed.data : [];
        setCompanies(data.map((c: any) => ({
          siren: c.siren || null,
          siret: c.siret || null,
          complete_name: c.complete_name || '',
          original_complete_name: c.original_complete_name || '',
          company_name: c.company_name || '',
          original_company_name: c.original_company_name || '',
          company_category: c.company_category || null,
          creation_date: c.creation_date || null,
          administrative_status: c.administrative_status || null,
          legal_form: c.legal_form || '',
          original_legal_form: c.original_legal_form || '',
          main_activity: c.main_activity || '',
          original_main_activity: c.original_main_activity || '',
          employee_size_range: c.employee_size_range || null,
          address: c.address || null,
          matching_score: c.matching_score || null,
        })));
      }
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setError(`Failed to fetch company: ${msg}`);
      setCompanies([]);
    } finally {
      setLoading(false);
    }
  }, [companyQuery]);

  // ===== CSV EXPORT =====
  const exportCSV = useCallback(() => {
    let csv = '';
    let filename = 'fincept_france_gov_';

    if (activeView === 'geo') {
      if (geoSearchType === 'municipalities' && municipalities.length > 0) {
        csv = [
          '# France - Municipalities',
          '',
          'Code,Name,Original Name,Postal Codes,Department,Region,Population,Area',
          ...municipalities.map(m =>
            `${m.code},"${(m.name || '').replace(/"/g, '""')}","${(m.original_name || '').replace(/"/g, '""')}","${(m.postal_codes || []).join('; ')}",${m.department_code || ''},${m.region_code || ''},${m.population || ''},${m.area || ''}`
          ),
        ].join('\n');
        filename += 'municipalities.csv';
      } else if (geoSearchType === 'departments' && departments.length > 0) {
        csv = [
          '# France - Departments',
          '',
          'Code,Name,Original Name,Region Code',
          ...departments.map(d =>
            `${d.code},"${(d.name || '').replace(/"/g, '""')}","${(d.original_name || '').replace(/"/g, '""')}",${d.region_code || ''}`
          ),
        ].join('\n');
        filename += 'departments.csv';
      } else if (geoSearchType === 'regions' && regions.length > 0) {
        csv = [
          '# France - Regions',
          '',
          'Code,Name,Original Name',
          ...regions.map(r =>
            `${r.code},"${(r.name || '').replace(/"/g, '""')}","${(r.original_name || '').replace(/"/g, '""')}"`
          ),
        ].join('\n');
        filename += 'regions.csv';
      } else {
        return;
      }
    } else if (activeView === 'datasets' && datasets.length > 0) {
      csv = [
        `# data.gouv.fr - Datasets${datasetQuery ? `: "${datasetQuery}"` : ''}`,
        '',
        'ID,Title,Original Title,Organization,Resources,Modified,Tags',
        ...datasets.map(d =>
          `${d.id},"${(d.title || '').replace(/"/g, '""')}","${(d.original_title || '').replace(/"/g, '""')}",${d.organization || ''},${d.resources_count},${d.last_modified || ''},"${(d.tags || []).join('; ')}"`
        ),
      ].join('\n');
      filename += `datasets_${datasetQuery ? datasetQuery.replace(/\s+/g, '_') : 'all'}.csv`;
    } else if (activeView === 'tabular' && resourceLines?.lines?.length) {
      const lines = resourceLines.lines;
      const keys = lines.length > 0 ? Object.keys(lines[0]) : [];
      csv = [
        `# data.gouv.fr - Tabular Data: ${tabularResourceId}`,
        '',
        keys.join(','),
        ...lines.map(row => keys.map(k => `"${String(row[k] ?? '').replace(/"/g, '""')}"`).join(',')),
      ].join('\n');
      filename += `tabular_${tabularResourceId}.csv`;
    } else if (activeView === 'company' && companies.length > 0) {
      csv = [
        '# France - Company Registry',
        '',
        'SIREN,SIRET,Name,Original Name,Category,Status,Legal Form,Activity,Created',
        ...companies.map(c =>
          `${c.siren || ''},${c.siret || ''},"${(c.complete_name || '').replace(/"/g, '""')}","${(c.original_complete_name || '').replace(/"/g, '""')}",${c.company_category || ''},${c.administrative_status || ''},"${(c.legal_form || '').replace(/"/g, '""')}","${(c.main_activity || '').replace(/"/g, '""')}",${c.creation_date || ''}`
        ),
      ].join('\n');
      filename += 'companies.csv';
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
  }, [activeView, geoSearchType, municipalities, departments, regions, datasets, datasetQuery, resourceLines, tabularResourceId, companies]);

  return {
    activeView,
    setActiveView,
    geoSearchType,
    setGeoSearchType,
    geoQuery,
    setGeoQuery,
    municipalities,
    departments,
    regions,
    fetchGeo,
    datasetQuery,
    setDatasetQuery,
    datasets,
    datasetsTotalCount,
    datasetsPage,
    fetchDatasets,
    tabularResourceId,
    setTabularResourceId,
    resourceProfile,
    resourceLines,
    fetchProfile,
    fetchLines,
    companyQuery,
    setCompanyQuery,
    companies,
    fetchCompany,
    exportCSV,
    loading,
    error,
  };
}

export default useFrenchGovData;
