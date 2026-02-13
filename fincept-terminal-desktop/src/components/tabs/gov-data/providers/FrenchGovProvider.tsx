// FrenchGovProvider - French Government API portal (data.gouv.fr, API Géo, Tabulaire, Entreprise)
// Four distinct sections: Geographic data, Dataset catalog, Tabular querying, Company lookup

import React, { useCallback } from 'react';
import { RefreshCw, AlertCircle, Download, Search, MapPin, Database, Table2, Building2, ChevronLeft, ChevronRight } from 'lucide-react';
import { FINCEPT } from '../constants';
import { useFrenchGovData, type GeoSearchType } from '../hooks/useFrenchGovData';
import type { FrenchGovView, FrenchMunicipality, FrenchDepartment, FrenchRegion, FrenchDataset, FrenchResourceProfile, FrenchResourceLines, FrenchCompany } from '../types';

const VIEWS: { id: FrenchGovView; label: string; icon: React.ElementType }[] = [
  { id: 'geo', label: 'GEOGRAPHY', icon: MapPin },
  { id: 'datasets', label: 'DATASETS', icon: Database },
  { id: 'tabular', label: 'TABULAR', icon: Table2 },
  { id: 'company', label: 'COMPANY', icon: Building2 },
];

const GEO_TYPES: { id: GeoSearchType; label: string }[] = [
  { id: 'municipalities', label: 'Municipalities' },
  { id: 'departments', label: 'Departments' },
  { id: 'regions', label: 'Regions' },
];

const PROVIDER_COLOR = '#2563EB';

const truncate = (text: string, max: number): string =>
  text.length > max ? text.substring(0, max) + '...' : text;

const formatDate = (dateStr: string | null): string => {
  if (!dateStr) return '-';
  return dateStr.substring(0, 10);
};

const formatNum = (n: number | null): string => {
  if (n == null) return '-';
  return n.toLocaleString();
};

export function FrenchGovProvider() {
  const {
    activeView, setActiveView,
    geoSearchType, setGeoSearchType, geoQuery, setGeoQuery,
    municipalities, departments, regions, fetchGeo,
    datasetQuery, setDatasetQuery, datasets, datasetsTotalCount, datasetsPage, fetchDatasets,
    tabularResourceId, setTabularResourceId, resourceProfile, resourceLines, fetchProfile, fetchLines,
    companyQuery, setCompanyQuery, companies, fetchCompany,
    exportCSV,
    loading, error,
  } = useFrenchGovData();

  const inputStyle: React.CSSProperties = {
    padding: '6px 10px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '11px',
    outline: 'none',
    fontFamily: 'inherit',
  };

  const btnStyle = (active: boolean): React.CSSProperties => ({
    padding: '5px 12px',
    backgroundColor: active ? PROVIDER_COLOR : 'transparent',
    color: active ? FINCEPT.WHITE : FINCEPT.GRAY,
    border: `1px solid ${active ? PROVIDER_COLOR : FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '9px',
    fontWeight: 700,
    cursor: 'pointer',
  });

  const actionBtnStyle: React.CSSProperties = {
    padding: '6px 14px',
    backgroundColor: PROVIDER_COLOR,
    color: FINCEPT.WHITE,
    border: 'none',
    borderRadius: '2px',
    fontSize: '10px',
    fontWeight: 700,
    cursor: loading ? 'wait' : 'pointer',
    opacity: loading ? 0.7 : 1,
    display: 'flex',
    alignItems: 'center',
    gap: '6px',
  };

  const hasData =
    (activeView === 'geo' && (municipalities.length > 0 || departments.length > 0 || regions.length > 0))
    || (activeView === 'datasets' && datasets.length > 0)
    || (activeView === 'tabular' && (resourceProfile !== null || (resourceLines?.lines?.length ?? 0) > 0))
    || (activeView === 'company' && companies.length > 0);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Header bar */}
      <div style={{
        padding: '8px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        flexWrap: 'wrap',
      }}>
        {/* View selector */}
        <div style={{ display: 'flex', gap: '2px', marginRight: '8px' }}>
          {VIEWS.map(v => (
            <button key={v.id} onClick={() => setActiveView(v.id)} style={btnStyle(activeView === v.id)}>
              {v.label}
            </button>
          ))}
        </div>

        {/* Geo controls */}
        {activeView === 'geo' && (
          <>
            <div style={{ display: 'flex', gap: '2px' }}>
              {GEO_TYPES.map(g => (
                <button key={g.id} onClick={() => setGeoSearchType(g.id)} style={{
                  ...btnStyle(geoSearchType === g.id),
                  backgroundColor: geoSearchType === g.id ? `${PROVIDER_COLOR}30` : 'transparent',
                  color: geoSearchType === g.id ? PROVIDER_COLOR : FINCEPT.GRAY,
                  border: `1px solid ${geoSearchType === g.id ? PROVIDER_COLOR : FINCEPT.BORDER}`,
                }}>
                  {g.label}
                </button>
              ))}
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, maxWidth: '300px' }}>
              <Search size={14} color={FINCEPT.GRAY} />
              <input
                type="text"
                value={geoQuery}
                onChange={e => setGeoQuery(e.target.value)}
                onKeyDown={e => { if (e.key === 'Enter') fetchGeo(); }}
                placeholder={`Search ${geoSearchType}...`}
                style={{ ...inputStyle, flex: 1 }}
              />
              <button onClick={fetchGeo} disabled={loading} style={actionBtnStyle}>
                FETCH
              </button>
            </div>
          </>
        )}

        {/* Dataset controls */}
        {activeView === 'datasets' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, maxWidth: '400px' }}>
            <Search size={14} color={FINCEPT.GRAY} />
            <input
              type="text"
              value={datasetQuery}
              onChange={e => setDatasetQuery(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') fetchDatasets(1); }}
              placeholder="Search data.gouv.fr datasets..."
              style={{ ...inputStyle, flex: 1 }}
            />
            <button onClick={() => fetchDatasets(1)} disabled={loading} style={actionBtnStyle}>
              SEARCH
            </button>
          </div>
        )}

        {/* Tabular controls */}
        {activeView === 'tabular' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, maxWidth: '500px' }}>
            <Table2 size={14} color={FINCEPT.GRAY} />
            <input
              type="text"
              value={tabularResourceId}
              onChange={e => setTabularResourceId(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') fetchProfile(); }}
              placeholder="Resource ID (UUID)..."
              style={{ ...inputStyle, flex: 1 }}
            />
            <button onClick={fetchProfile} disabled={loading || !tabularResourceId.trim()} style={{
              ...actionBtnStyle,
              cursor: loading || !tabularResourceId.trim() ? 'not-allowed' : 'pointer',
              opacity: loading || !tabularResourceId.trim() ? 0.5 : 1,
            }}>
              PROFILE
            </button>
            <button onClick={() => fetchLines(1)} disabled={loading || !tabularResourceId.trim()} style={{
              ...actionBtnStyle,
              cursor: loading || !tabularResourceId.trim() ? 'not-allowed' : 'pointer',
              opacity: loading || !tabularResourceId.trim() ? 0.5 : 1,
            }}>
              LINES
            </button>
          </div>
        )}

        {/* Company controls */}
        {activeView === 'company' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, maxWidth: '350px' }}>
            <Building2 size={14} color={FINCEPT.GRAY} />
            <input
              type="text"
              value={companyQuery}
              onChange={e => setCompanyQuery(e.target.value)}
              onKeyDown={e => { if (e.key === 'Enter') fetchCompany(); }}
              placeholder="SIREN (9-digit) or SIRET (14-digit)..."
              style={{ ...inputStyle, flex: 1 }}
            />
            <button onClick={fetchCompany} disabled={loading || !companyQuery.trim()} style={{
              ...actionBtnStyle,
              cursor: loading || !companyQuery.trim() ? 'not-allowed' : 'pointer',
              opacity: loading || !companyQuery.trim() ? 0.5 : 1,
            }}>
              LOOKUP
            </button>
          </div>
        )}

        {/* Actions */}
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '4px' }}>
          {hasData && (
            <button
              onClick={exportCSV}
              title="Export CSV"
              style={{
                padding: '5px 10px',
                backgroundColor: 'transparent',
                color: FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <Download size={11} />
              CSV
            </button>
          )}
        </div>
      </div>

      {/* Content area */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {loading ? (
          <LoadingState />
        ) : error ? (
          <ErrorState error={error} />
        ) : activeView === 'geo' ? (
          <GeoContent
            geoSearchType={geoSearchType}
            municipalities={municipalities}
            departments={departments}
            regions={regions}
          />
        ) : activeView === 'datasets' ? (
          <DatasetsContent
            datasets={datasets}
            totalCount={datasetsTotalCount}
            page={datasetsPage}
            onPageChange={fetchDatasets}
            loading={loading}
          />
        ) : activeView === 'tabular' ? (
          <TabularContent
            profile={resourceProfile}
            lines={resourceLines}
            onFetchLines={fetchLines}
            loading={loading}
          />
        ) : activeView === 'company' ? (
          <CompanyContent companies={companies} />
        ) : (
          <EmptyState message="Select a view to get started" />
        )}
      </div>
    </div>
  );
}

// ===== Sub-components =====

function LoadingState() {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <RefreshCw size={32} className="animate-spin" style={{ color: PROVIDER_COLOR, marginBottom: '16px' }} />
      <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading French government data...</div>
    </div>
  );
}

function ErrorState({ error }: { error: string }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
      <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
      <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
    </div>
  );
}

function EmptyState({ message }: { message: string }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <Database size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
      <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>{message}</div>
    </div>
  );
}

// ===== GEO CONTENT =====

function GeoContent({
  geoSearchType,
  municipalities,
  departments,
  regions,
}: {
  geoSearchType: GeoSearchType;
  municipalities: FrenchMunicipality[];
  departments: FrenchDepartment[];
  regions: FrenchRegion[];
}) {
  if (geoSearchType === 'municipalities') {
    if (municipalities.length === 0) return <EmptyState message="Search for French municipalities by name, postal code, or coordinates" />;
    return (
      <div>
        <div style={summaryBarStyle}>{municipalities.length} municipalities</div>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <th style={thStyle}>CODE</th>
              <th style={thStyle}>NAME</th>
              <th style={thStyle}>ORIGINAL</th>
              <th style={{ ...thStyle, width: '120px' }}>POSTAL CODES</th>
              <th style={{ ...thStyle, width: '60px' }}>DEPT</th>
              <th style={{ ...thStyle, width: '60px' }}>REGION</th>
              <th style={{ ...thStyle, width: '90px', textAlign: 'right' }}>POPULATION</th>
              <th style={{ ...thStyle, width: '80px', textAlign: 'right' }}>AREA (ha)</th>
            </tr>
          </thead>
          <tbody>
            {municipalities.map(m => (
              <tr key={m.code} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 600 }}>{m.code}</td>
                <td style={tdStyle}>{m.name}</td>
                <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                  {m.original_name !== m.name ? m.original_name : ''}
                </td>
                <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{(m.postal_codes || []).slice(0, 3).join(', ')}{(m.postal_codes || []).length > 3 ? '...' : ''}</td>
                <td style={{ ...tdStyle, textAlign: 'center', color: FINCEPT.GRAY }}>{m.department_code || '-'}</td>
                <td style={{ ...tdStyle, textAlign: 'center', color: FINCEPT.GRAY }}>{m.region_code || '-'}</td>
                <td style={{ ...tdStyle, textAlign: 'right', color: PROVIDER_COLOR, fontWeight: 600 }}>{formatNum(m.population)}</td>
                <td style={{ ...tdStyle, textAlign: 'right', color: FINCEPT.GRAY }}>{formatNum(m.area)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    );
  }

  if (geoSearchType === 'departments') {
    if (departments.length === 0) return <EmptyState message="Search for French departments by name or code" />;
    return (
      <div>
        <div style={summaryBarStyle}>{departments.length} departments</div>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <th style={{ ...thStyle, width: '80px' }}>CODE</th>
              <th style={thStyle}>NAME</th>
              <th style={thStyle}>ORIGINAL</th>
              <th style={{ ...thStyle, width: '100px' }}>REGION CODE</th>
            </tr>
          </thead>
          <tbody>
            {departments.map(d => (
              <tr key={d.code} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 600 }}>{d.code}</td>
                <td style={tdStyle}>{d.name}</td>
                <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                  {d.original_name !== d.name ? d.original_name : ''}
                </td>
                <td style={{ ...tdStyle, textAlign: 'center', color: FINCEPT.GRAY }}>{d.region_code || '-'}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    );
  }

  if (geoSearchType === 'regions') {
    if (regions.length === 0) return <EmptyState message="Search for French regions by name or code" />;
    return (
      <div>
        <div style={summaryBarStyle}>{regions.length} regions</div>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
              <th style={{ ...thStyle, width: '80px' }}>CODE</th>
              <th style={thStyle}>NAME</th>
              <th style={thStyle}>ORIGINAL</th>
            </tr>
          </thead>
          <tbody>
            {regions.map(r => (
              <tr key={r.code} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 600 }}>{r.code}</td>
                <td style={tdStyle}>{r.name}</td>
                <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                  {r.original_name !== r.name ? r.original_name : ''}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    );
  }

  return <EmptyState message="Select a geographic entity type" />;
}

// ===== DATASETS CONTENT =====

function DatasetsContent({
  datasets,
  totalCount,
  page,
  onPageChange,
  loading,
}: {
  datasets: FrenchDataset[];
  totalCount: number;
  page: number;
  onPageChange: (page: number) => Promise<void>;
  loading: boolean;
}) {
  const handleOpenUrl = useCallback(async (url: string) => {
    try {
      const { openUrl } = await import('@tauri-apps/plugin-opener');
      await openUrl(url);
    } catch {
      window.open(url, '_blank');
    }
  }, []);

  if (datasets.length === 0) return <EmptyState message="Search for datasets on data.gouv.fr" />;

  const totalPages = Math.ceil(totalCount / 20);

  return (
    <div>
      <div style={{ ...summaryBarStyle, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <span>Showing {datasets.length} of {totalCount.toLocaleString()} datasets (page {page})</span>
        <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
          <button
            onClick={() => onPageChange(page - 1)}
            disabled={page <= 1 || loading}
            style={{ ...pageBtnStyle, opacity: page <= 1 ? 0.3 : 1, cursor: page <= 1 ? 'not-allowed' : 'pointer' }}
          >
            <ChevronLeft size={12} /> Prev
          </button>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{page} / {totalPages}</span>
          <button
            onClick={() => onPageChange(page + 1)}
            disabled={page >= totalPages || loading}
            style={{ ...pageBtnStyle, opacity: page >= totalPages ? 0.3 : 1, cursor: page >= totalPages ? 'not-allowed' : 'pointer' }}
          >
            Next <ChevronRight size={12} />
          </button>
        </div>
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>TITLE</th>
            <th style={{ ...thStyle, width: '140px' }}>ORGANIZATION</th>
            <th style={{ ...thStyle, width: '70px' }}>FILES</th>
            <th style={{ ...thStyle, width: '100px' }}>MODIFIED</th>
            <th style={{ ...thStyle, width: '180px' }}>TAGS</th>
          </tr>
        </thead>
        <tbody>
          {datasets.map(d => (
            <tr
              key={d.id}
              style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, cursor: d.url ? 'pointer' : 'default' }}
              onClick={() => d.url && handleOpenUrl(d.url)}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={tdStyle}>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600, marginBottom: '2px' }}>
                  {truncate(d.title, 80)}
                </div>
                {d.original_title && d.original_title !== d.title && (
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontStyle: 'italic', marginBottom: '2px' }}>
                    {truncate(d.original_title, 90)}
                  </div>
                )}
                {d.description && (
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{truncate(d.description, 100)}</div>
                )}
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{d.organization || d.owner || '-'}</td>
              <td style={{ ...tdStyle, textAlign: 'center' }}>
                <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{d.resources_count}</span>
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatDate(d.last_modified)}</td>
              <td style={tdStyle}>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                  {(d.tags || []).slice(0, 3).map((tag, i) => (
                    <span key={i} style={{
                      padding: '1px 6px',
                      backgroundColor: `${PROVIDER_COLOR}15`,
                      color: PROVIDER_COLOR,
                      borderRadius: '2px',
                      fontSize: '8px',
                      fontWeight: 600,
                    }}>
                      {truncate(tag, 20)}
                    </span>
                  ))}
                  {(d.tags || []).length > 3 && (
                    <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>+{d.tags.length - 3}</span>
                  )}
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

// ===== TABULAR CONTENT =====

function TabularContent({
  profile,
  lines,
  onFetchLines,
  loading,
}: {
  profile: FrenchResourceProfile | null;
  lines: FrenchResourceLines | null;
  onFetchLines: (page: number) => Promise<void>;
  loading: boolean;
}) {
  if (!profile && !lines) {
    return <EmptyState message="Enter a data.gouv.fr resource UUID above and click PROFILE or LINES" />;
  }

  return (
    <div>
      {/* Profile section */}
      {profile && (
        <div style={{ marginBottom: '8px' }}>
          <div style={{ ...summaryBarStyle, backgroundColor: `${PROVIDER_COLOR}10` }}>
            <div style={{ display: 'flex', gap: '16px', flexWrap: 'wrap' }}>
              <span><span style={{ color: FINCEPT.GRAY }}>Resource:</span> <span style={{ color: PROVIDER_COLOR }}>{profile.resource_id}</span></span>
              {profile.format && <span><span style={{ color: FINCEPT.GRAY }}>Format:</span> {profile.format}</span>}
              {profile.total_rows != null && <span><span style={{ color: FINCEPT.GRAY }}>Rows:</span> {profile.total_rows.toLocaleString()}</span>}
              {profile.encoding && <span><span style={{ color: FINCEPT.GRAY }}>Encoding:</span> {profile.encoding}</span>}
            </div>
          </div>
          {profile.columns.length > 0 && (
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <thead>
                <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
                  <th style={{ ...thStyle, width: '40px' }}>#</th>
                  <th style={thStyle}>COLUMN</th>
                  <th style={thStyle}>ORIGINAL</th>
                  <th style={{ ...thStyle, width: '80px' }}>TYPE</th>
                  <th style={thStyle}>DESCRIPTION</th>
                </tr>
              </thead>
              <tbody>
                {profile.columns.map((col, idx) => (
                  <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    <td style={{ ...tdStyle, color: FINCEPT.MUTED, textAlign: 'center' }}>{idx + 1}</td>
                    <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 600 }}>{col.name || col.title || `col_${idx}`}</td>
                    <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                      {col.original_name && col.original_name !== col.name ? col.original_name : ''}
                    </td>
                    <td style={tdStyle}>
                      {col.type && (
                        <span style={{
                          padding: '1px 6px',
                          backgroundColor: `${PROVIDER_COLOR}15`,
                          color: PROVIDER_COLOR,
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 600,
                        }}>
                          {col.type}
                        </span>
                      )}
                    </td>
                    <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{col.description ? truncate(col.description, 60) : '-'}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      )}

      {/* Lines section */}
      {lines && lines.lines.length > 0 && (
        <div>
          <div style={{ ...summaryBarStyle, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <span>
              Page {lines.page} — {lines.lines.length} rows of {lines.total_rows.toLocaleString()}
            </span>
            <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
              <button
                onClick={() => onFetchLines(lines.page - 1)}
                disabled={lines.page <= 1 || loading}
                style={{ ...pageBtnStyle, opacity: lines.page <= 1 ? 0.3 : 1, cursor: lines.page <= 1 ? 'not-allowed' : 'pointer' }}
              >
                <ChevronLeft size={12} /> Prev
              </button>
              <button
                onClick={() => onFetchLines(lines.page + 1)}
                disabled={!lines.has_more || loading}
                style={{ ...pageBtnStyle, opacity: !lines.has_more ? 0.3 : 1, cursor: !lines.has_more ? 'not-allowed' : 'pointer' }}
              >
                Next <ChevronRight size={12} />
              </button>
            </div>
          </div>
          <div style={{ overflowX: 'auto' }}>
            <table style={{ width: '100%', borderCollapse: 'collapse', minWidth: '600px' }}>
              <thead>
                <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
                  {Object.keys(lines.lines[0]).map(key => (
                    <th key={key} style={{ ...thStyle, whiteSpace: 'nowrap' }}>{key}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {lines.lines.map((row, idx) => (
                  <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                    {Object.values(row).map((val, colIdx) => (
                      <td key={colIdx} style={{ ...tdStyle, fontSize: '10px', whiteSpace: 'nowrap', maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                        {val != null ? String(val) : '-'}
                      </td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}

// ===== COMPANY CONTENT =====

function CompanyContent({ companies }: { companies: FrenchCompany[] }) {
  if (companies.length === 0) return <EmptyState message="Enter a 9-digit SIREN or 14-digit SIRET to look up a French company" />;

  return (
    <div>
      <div style={summaryBarStyle}>{companies.length} compan{companies.length !== 1 ? 'ies' : 'y'} found</div>
      {companies.map((c, idx) => (
        <div key={idx} style={{
          padding: '12px 16px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          <div style={{ fontSize: '13px', color: FINCEPT.WHITE, fontWeight: 700, marginBottom: '4px' }}>
            {c.complete_name || c.company_name || 'Unknown'}
          </div>
          {c.original_complete_name && c.original_complete_name !== c.complete_name && (
            <div style={{ fontSize: '10px', color: FINCEPT.MUTED, fontStyle: 'italic', marginBottom: '8px' }}>
              {c.original_complete_name}
            </div>
          )}
          <div style={{ display: 'flex', gap: '20px', flexWrap: 'wrap', fontSize: '10px' }}>
            {c.siren && (
              <div>
                <span style={{ color: FINCEPT.GRAY }}>SIREN:</span>{' '}
                <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{c.siren}</span>
              </div>
            )}
            {c.siret && (
              <div>
                <span style={{ color: FINCEPT.GRAY }}>SIRET:</span>{' '}
                <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{c.siret}</span>
              </div>
            )}
            {c.company_category && (
              <div><span style={{ color: FINCEPT.GRAY }}>Category:</span> {c.company_category}</div>
            )}
            {c.administrative_status && (
              <div>
                <span style={{ color: FINCEPT.GRAY }}>Status:</span>{' '}
                <span style={{ color: c.administrative_status === 'A' ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: 600 }}>
                  {c.administrative_status === 'A' ? 'Active' : c.administrative_status === 'C' ? 'Ceased' : c.administrative_status}
                </span>
              </div>
            )}
            {c.creation_date && (
              <div><span style={{ color: FINCEPT.GRAY }}>Created:</span> {formatDate(c.creation_date)}</div>
            )}
            {c.legal_form && (
              <div><span style={{ color: FINCEPT.GRAY }}>Legal form:</span> {c.legal_form}</div>
            )}
            {c.main_activity && (
              <div><span style={{ color: FINCEPT.GRAY }}>Activity:</span> {c.main_activity}</div>
            )}
            {c.employee_size_range && (
              <div><span style={{ color: FINCEPT.GRAY }}>Employees:</span> {c.employee_size_range}</div>
            )}
          </div>
        </div>
      ))}
    </div>
  );
}

// ===== Shared styles =====

const thStyle: React.CSSProperties = {
  padding: '8px 12px',
  textAlign: 'left',
  fontSize: '9px',
  fontWeight: 700,
  color: PROVIDER_COLOR,
  borderBottom: `1px solid ${FINCEPT.BORDER}`,
};

const tdStyle: React.CSSProperties = {
  padding: '8px 12px',
  fontSize: '11px',
  color: FINCEPT.WHITE,
};

const summaryBarStyle: React.CSSProperties = {
  padding: '8px 16px',
  fontSize: '9px',
  color: FINCEPT.GRAY,
  backgroundColor: FINCEPT.PANEL_BG,
  borderBottom: `1px solid ${FINCEPT.BORDER}`,
};

const pageBtnStyle: React.CSSProperties = {
  padding: '3px 8px',
  backgroundColor: 'transparent',
  color: FINCEPT.GRAY,
  border: `1px solid ${FINCEPT.BORDER}`,
  borderRadius: '2px',
  fontSize: '9px',
  fontWeight: 600,
  display: 'flex',
  alignItems: 'center',
  gap: '2px',
};

export default FrenchGovProvider;
