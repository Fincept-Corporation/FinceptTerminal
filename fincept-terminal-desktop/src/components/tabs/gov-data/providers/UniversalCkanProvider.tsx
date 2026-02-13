// UniversalCkanProvider - Multi-portal CKAN data browser
// Supports 8 CKAN portals: US, UK, AU, IT, BR, LV, SI, UY
// Portal selector + Organizations → Datasets → Resources + Search

import React, { useEffect, useCallback } from 'react';
import { RefreshCw, AlertCircle, Database, Download, Search, ChevronRight, ExternalLink, FileText, ArrowLeft, Globe } from 'lucide-react';
import { FINCEPT, CKAN_PORTALS } from '../constants';
import { useUniversalCkanData } from '../hooks/useUniversalCkanData';
import type { CkanView, CkanDataset, CkanResource } from '../types';

const VIEWS: { id: CkanView; label: string }[] = [
  { id: 'organizations', label: 'ORGANIZATIONS' },
  { id: 'datasets', label: 'DATASETS' },
  { id: 'search', label: 'SEARCH' },
];

const PROVIDER_COLOR = '#10B981';

const truncate = (text: string, max: number): string =>
  text.length > max ? text.substring(0, max) + '...' : text;

const formatDate = (dateStr: string | null | undefined): string => {
  if (!dateStr) return '-';
  return dateStr.substring(0, 10);
};

const formatSize = (bytes: number | null): string => {
  if (!bytes) return '-';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
};

function parseTags(tags: any): string[] {
  if (!Array.isArray(tags)) return [];
  return tags.map((t: any) => (typeof t === 'string' ? t : t?.name || t?.display_name || '')).filter(Boolean);
}

export function UniversalCkanProvider() {
  const {
    activeView, setActiveView,
    activePortal, setActivePortal,
    organizations, selectedOrg, setSelectedOrg, fetchOrganizations,
    datasets, selectedDataset, setSelectedDataset, fetchDatasets, datasetsTotalCount,
    resources, fetchResources,
    searchQuery, setSearchQuery, searchResults, searchTotalCount, searchDatasets,
    exportCSV,
    loading, error,
  } = useUniversalCkanData();

  // Load organizations when portal changes
  useEffect(() => {
    fetchOrganizations();
  }, [fetchOrganizations]);

  const handlePortalChange = (code: string) => {
    setActivePortal(code);
    setActiveView('organizations');
    setSelectedOrg(null);
    setSelectedDataset(null);
  };

  const handleViewChange = (view: CkanView) => {
    setActiveView(view);
  };

  const handleOrgClick = (orgName: string) => {
    setSelectedOrg(orgName);
    setSelectedDataset(null);
    setActiveView('datasets');
    fetchDatasets(orgName);
  };

  const handleDatasetClick = (datasetId: string) => {
    setSelectedDataset(datasetId);
    setActiveView('resources');
    fetchResources(datasetId);
  };

  const handleBack = () => {
    if (activeView === 'resources') {
      setActiveView('datasets');
      setSelectedDataset(null);
    } else if (activeView === 'datasets') {
      setActiveView('organizations');
      setSelectedOrg(null);
    }
  };

  const handleSearchKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') searchDatasets();
  };

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

  const currentPortal = CKAN_PORTALS.find(p => p.code === activePortal) || CKAN_PORTALS[0];

  const hasData = (activeView === 'organizations' && organizations.length > 0)
    || (activeView === 'datasets' && datasets.length > 0)
    || (activeView === 'resources' && resources.length > 0)
    || (activeView === 'search' && searchResults.length > 0);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Portal selector bar */}
      <div style={{
        padding: '6px 16px',
        backgroundColor: `${PROVIDER_COLOR}08`,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '4px',
        flexWrap: 'wrap',
      }}>
        <Globe size={12} color={PROVIDER_COLOR} />
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginRight: '4px' }}>PORTAL:</span>
        {CKAN_PORTALS.map(portal => (
          <button
            key={portal.code}
            onClick={() => handlePortalChange(portal.code)}
            style={{
              padding: '3px 8px',
              backgroundColor: activePortal === portal.code ? PROVIDER_COLOR : 'transparent',
              color: activePortal === portal.code ? FINCEPT.WHITE : FINCEPT.GRAY,
              border: `1px solid ${activePortal === portal.code ? PROVIDER_COLOR : FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 600,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <span>{portal.flag}</span>
            <span>{portal.code.toUpperCase()}</span>
          </button>
        ))}
      </div>

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
        {/* Back button */}
        {(activeView === 'datasets' || activeView === 'resources') && (
          <button
            onClick={handleBack}
            style={{
              padding: '5px 8px',
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
            <ArrowLeft size={11} />
            BACK
          </button>
        )}

        {/* View selector */}
        <div style={{ display: 'flex', gap: '2px', marginRight: '12px' }}>
          {VIEWS.map(v => (
            <button
              key={v.id}
              onClick={() => handleViewChange(v.id)}
              style={{
                padding: '5px 12px',
                backgroundColor: activeView === v.id ? PROVIDER_COLOR : 'transparent',
                color: activeView === v.id ? FINCEPT.WHITE : FINCEPT.GRAY,
                border: `1px solid ${activeView === v.id ? PROVIDER_COLOR : FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
              }}
            >
              {v.label}
            </button>
          ))}
        </div>

        {/* Search input */}
        {activeView === 'search' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, maxWidth: '400px' }}>
            <Search size={14} color={FINCEPT.GRAY} />
            <input
              type="text"
              value={searchQuery}
              onChange={e => setSearchQuery(e.target.value)}
              onKeyDown={handleSearchKeyDown}
              placeholder={`Search ${currentPortal.name} datasets...`}
              style={{ ...inputStyle, flex: 1 }}
            />
            <button
              onClick={searchDatasets}
              disabled={loading || !searchQuery.trim()}
              style={{
                padding: '6px 14px',
                backgroundColor: PROVIDER_COLOR,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: '2px',
                fontSize: '10px',
                fontWeight: 700,
                cursor: loading || !searchQuery.trim() ? 'not-allowed' : 'pointer',
                opacity: loading || !searchQuery.trim() ? 0.5 : 1,
              }}
            >
              SEARCH
            </button>
          </div>
        )}

        {/* Breadcrumb */}
        {activeView === 'datasets' && selectedOrg && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>{currentPortal.flag}</span>
            <span>Organizations</span>
            <ChevronRight size={10} />
            <span style={{ color: PROVIDER_COLOR }}>{selectedOrg}</span>
            <span style={{ color: FINCEPT.MUTED }}>({datasetsTotalCount} datasets)</span>
          </div>
        )}
        {activeView === 'resources' && selectedDataset && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>{currentPortal.flag}</span>
            <span>{selectedOrg || 'Org'}</span>
            <ChevronRight size={10} />
            <span style={{ color: PROVIDER_COLOR }}>{truncate(selectedDataset, 30)}</span>
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
          {activeView === 'organizations' && (
            <button
              onClick={fetchOrganizations}
              disabled={loading}
              style={{
                padding: '5px 14px',
                backgroundColor: PROVIDER_COLOR,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: '2px',
                fontSize: '10px',
                fontWeight: 700,
                cursor: loading ? 'wait' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                opacity: loading ? 0.7 : 1,
              }}
            >
              <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
              REFRESH
            </button>
          )}
        </div>
      </div>

      {/* Content area */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {loading ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <RefreshCw size={32} className="animate-spin" style={{ color: PROVIDER_COLOR, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading {currentPortal.name} data...</div>
          </div>
        ) : error ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
          </div>
        ) : activeView === 'organizations' ? (
          <OrganizationsTable organizations={organizations} onSelect={handleOrgClick} portalName={currentPortal.name} />
        ) : activeView === 'datasets' ? (
          <DatasetsTable datasets={datasets} onSelect={handleDatasetClick} totalCount={datasetsTotalCount} />
        ) : activeView === 'resources' ? (
          <ResourcesTable resources={resources} />
        ) : activeView === 'search' ? (
          searchResults.length > 0
            ? <DatasetsTable datasets={searchResults} onSelect={handleDatasetClick} totalCount={searchTotalCount} />
            : <EmptyState icon={Search} message={`Search ${currentPortal.name} datasets`} sub="Enter a query above and press SEARCH or Enter" />
        ) : (
          <EmptyState icon={Database} message="Select a view" sub="" />
        )}
      </div>
    </div>
  );
}

// ===== Sub-components =====

function EmptyState({ icon: Icon, message, sub }: { icon: React.ElementType; message: string; sub: string }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <Icon size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
      <div style={{ fontSize: '12px', color: FINCEPT.WHITE, marginBottom: '8px' }}>{message}</div>
      {sub && <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{sub}</div>}
    </div>
  );
}

function OrganizationsTable({ organizations, onSelect, portalName }: { organizations: { name: string; display_name: string }[]; onSelect: (name: string) => void; portalName: string }) {
  return (
    <div>
      <div style={summaryBarStyle}>{organizations.length} organizations on {portalName}</div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>ORGANIZATION ID</th>
            <th style={thStyle}>DISPLAY NAME</th>
            <th style={{ ...thStyle, width: '60px' }}></th>
          </tr>
        </thead>
        <tbody>
          {organizations.map(o => (
            <tr
              key={o.name}
              onClick={() => onSelect(o.name)}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 600 }}>{o.name}</td>
              <td style={tdStyle}>{o.display_name}</td>
              <td style={tdStyle}>
                <ChevronRight size={14} color={FINCEPT.MUTED} />
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function DatasetsTable({ datasets, onSelect, totalCount }: { datasets: CkanDataset[]; onSelect: (id: string) => void; totalCount: number }) {
  return (
    <div>
      <div style={summaryBarStyle}>Showing {datasets.length} of {totalCount} datasets</div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>TITLE</th>
            <th style={{ ...thStyle, width: '140px' }}>ORGANIZATION</th>
            <th style={{ ...thStyle, width: '70px' }}>FILES</th>
            <th style={{ ...thStyle, width: '100px' }}>MODIFIED</th>
            <th style={{ ...thStyle, width: '180px' }}>TAGS</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {datasets.map(d => (
            <tr
              key={d.id || d.name}
              onClick={() => onSelect(d.name || d.id || '')}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={tdStyle}>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600, marginBottom: '2px' }}>
                  {truncate(d.title || d.display_name || d.name || '', 80)}
                </div>
                {d.notes && (
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{truncate(d.notes, 100)}</div>
                )}
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{d.organization_name || '-'}</td>
              <td style={{ ...tdStyle, textAlign: 'center' }}>
                <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{d.num_resources || 0}</span>
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatDate(d.metadata_modified)}</td>
              <td style={tdStyle}>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                  {parseTags(d.tags).slice(0, 3).map((tag, i) => (
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
                  {parseTags(d.tags).length > 3 && (
                    <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>+{parseTags(d.tags).length - 3}</span>
                  )}
                </div>
              </td>
              <td style={tdStyle}>
                <ChevronRight size={14} color={FINCEPT.MUTED} />
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function ResourcesTable({ resources }: { resources: CkanResource[] }) {
  const handleOpenUrl = useCallback(async (url: string) => {
    try {
      const { openUrl } = await import('@tauri-apps/plugin-opener');
      await openUrl(url);
    } catch {
      window.open(url, '_blank');
    }
  }, []);

  return (
    <div>
      <div style={summaryBarStyle}>{resources.length} resource{resources.length !== 1 ? 's' : ''} available</div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>NAME</th>
            <th style={{ ...thStyle, width: '80px' }}>FORMAT</th>
            <th style={{ ...thStyle, width: '90px' }}>SIZE</th>
            <th style={{ ...thStyle, width: '100px' }}>MODIFIED</th>
            <th style={{ ...thStyle, width: '60px' }}>LINK</th>
          </tr>
        </thead>
        <tbody>
          {resources.map(r => (
            <tr key={r.id} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <td style={tdStyle}>
                <div style={{ display: 'flex', alignItems: 'flex-start', gap: '6px' }}>
                  <FileText size={14} color={FINCEPT.MUTED} style={{ marginTop: '2px', flexShrink: 0 }} />
                  <div>
                    <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                      {r.name || r.id}
                    </div>
                    {r.description && (
                      <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{truncate(r.description, 80)}</div>
                    )}
                  </div>
                </div>
              </td>
              <td style={tdStyle}>
                <span style={{
                  padding: '2px 8px',
                  backgroundColor: `${PROVIDER_COLOR}15`,
                  color: PROVIDER_COLOR,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                }}>
                  {r.format || '?'}
                </span>
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatSize(r.size)}</td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatDate(r.last_modified)}</td>
              <td style={tdStyle}>
                {r.url && (
                  <button
                    onClick={() => handleOpenUrl(r.url)}
                    style={{
                      background: 'none',
                      border: 'none',
                      color: PROVIDER_COLOR,
                      cursor: 'pointer',
                      padding: '4px',
                      display: 'flex',
                      alignItems: 'center',
                    }}
                    title={r.url}
                  >
                    <ExternalLink size={14} />
                  </button>
                )}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

// Shared styles
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

export default UniversalCkanProvider;
