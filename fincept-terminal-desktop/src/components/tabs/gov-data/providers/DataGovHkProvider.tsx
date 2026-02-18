// DataGovHkProvider - Hong Kong Government open data portal (data.gov.hk)
// CKAN-based: Categories (groups) -> Datasets -> Resources + Historical Archive + Search

import React, { useEffect, useCallback } from 'react';
import { RefreshCw, AlertCircle, Database, Download, Search, ChevronRight, ExternalLink, FileText, ArrowLeft, Archive, FolderOpen } from 'lucide-react';
import { FINCEPT } from '../constants';
import { useDataGovHkData } from '../hooks/useDataGovHkData';
import type { HkGovView, HkDataset, HkResource, HkHistoricalFile, HkCategory } from '../types';

const VIEWS: { id: HkGovView; label: string }[] = [
  { id: 'categories', label: 'CATEGORIES' },
  { id: 'datasets', label: 'DATASETS' },
  { id: 'historical', label: 'HISTORICAL' },
  { id: 'search', label: 'SEARCH' },
];

const PROVIDER_COLOR = '#F43F5E';

const truncate = (text: string, max: number): string =>
  text.length > max ? text.substring(0, max) + '...' : text;

const formatDate = (dateStr: string | null): string => {
  if (!dateStr) return '-';
  return dateStr.substring(0, 10);
};

const formatSize = (bytes: number | null): string => {
  if (!bytes) return '-';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
};

// Format YYYYMMDD to display-friendly YYYY-MM-DD
const formatYMD = (ymd: string): string => {
  if (ymd.length !== 8) return ymd;
  return `${ymd.slice(0, 4)}-${ymd.slice(4, 6)}-${ymd.slice(6, 8)}`;
};

export function DataGovHkProvider() {
  const {
    activeView, setActiveView,
    categories, selectedCategory, setSelectedCategory, fetchCategories,
    datasets, selectedDataset, setSelectedDataset, fetchDatasets, fetchAllDatasets,
    resources, fetchResources,
    historicalFiles, historicalStartDate, setHistoricalStartDate, historicalEndDate, setHistoricalEndDate,
    historicalCategory, setHistoricalCategory, historicalFormat, setHistoricalFormat, fetchHistoricalFiles,
    searchQuery, setSearchQuery, searchResults, searchDatasets,
    exportCSV,
    loading, error,
  } = useDataGovHkData();

  // Load categories on mount
  useEffect(() => {
    fetchCategories();
  }, [fetchCategories]);

  const handleViewChange = (view: HkGovView) => {
    setActiveView(view);
  };

  const handleCategoryClick = (categoryId: string) => {
    setSelectedCategory(categoryId);
    setSelectedDataset(null);
    setActiveView('datasets');
    fetchDatasets(categoryId);
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
      setActiveView('categories');
      setSelectedCategory(null);
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

  const hasData = (activeView === 'categories' && categories.length > 0)
    || (activeView === 'datasets' && datasets.length > 0)
    || (activeView === 'resources' && resources.length > 0)
    || (activeView === 'historical' && historicalFiles.length > 0)
    || (activeView === 'search' && searchResults.length > 0);

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
        {/* Back button for drill-down views */}
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
              placeholder="Search HK datasets..."
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

        {/* Historical filter controls */}
        {activeView === 'historical' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 700 }}>FROM:</span>
            <input
              type="text"
              value={historicalStartDate}
              onChange={e => setHistoricalStartDate(e.target.value)}
              placeholder="YYYYMMDD"
              maxLength={8}
              style={{ ...inputStyle, width: '90px' }}
            />
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 700 }}>TO:</span>
            <input
              type="text"
              value={historicalEndDate}
              onChange={e => setHistoricalEndDate(e.target.value)}
              placeholder="YYYYMMDD"
              maxLength={8}
              style={{ ...inputStyle, width: '90px' }}
            />
            <input
              type="text"
              value={historicalCategory}
              onChange={e => setHistoricalCategory(e.target.value)}
              placeholder="Category (opt)"
              style={{ ...inputStyle, width: '110px' }}
            />
            <input
              type="text"
              value={historicalFormat}
              onChange={e => setHistoricalFormat(e.target.value)}
              placeholder="Format (opt)"
              style={{ ...inputStyle, width: '90px' }}
            />
            <button
              onClick={fetchHistoricalFiles}
              disabled={loading}
              style={{
                padding: '6px 14px',
                backgroundColor: PROVIDER_COLOR,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: '2px',
                fontSize: '10px',
                fontWeight: 700,
                cursor: loading ? 'wait' : 'pointer',
                opacity: loading ? 0.7 : 1,
              }}
            >
              FETCH
            </button>
          </div>
        )}

        {/* Breadcrumb */}
        {(activeView === 'datasets' && selectedCategory) && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>Categories</span>
            <ChevronRight size={10} />
            <span style={{ color: PROVIDER_COLOR }}>{truncate(selectedCategory, 40)}</span>
          </div>
        )}
        {(activeView === 'resources' && selectedDataset) && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>Categories</span>
            <ChevronRight size={10} />
            <span>{truncate(selectedCategory || '', 20)}</span>
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
          {activeView === 'categories' && (
            <button
              onClick={fetchCategories}
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
            <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading Hong Kong government data...</div>
          </div>
        ) : error ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
          </div>
        ) : activeView === 'categories' ? (
          <CategoriesTable categories={categories} onSelect={handleCategoryClick} />
        ) : activeView === 'datasets' ? (
          <DatasetsTable datasets={datasets} onSelect={handleDatasetClick} />
        ) : activeView === 'resources' ? (
          <ResourcesTable resources={resources} />
        ) : activeView === 'historical' ? (
          historicalFiles.length > 0
            ? <HistoricalTable files={historicalFiles} />
            : <EmptyState icon={Archive} message="Query historical archive" sub="Set a date range (YYYYMMDD) and click FETCH" />
        ) : activeView === 'search' ? (
          searchResults.length > 0
            ? <DatasetsTable datasets={searchResults} onSelect={handleDatasetClick} />
            : <EmptyState icon={Search} message="Search HK government datasets" sub="Enter a query above and press SEARCH or Enter" />
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

function CategoriesTable({ categories, onSelect }: { categories: HkCategory[]; onSelect: (id: string) => void }) {
  return (
    <div style={{ padding: '0' }}>
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {categories.length} categor{categories.length !== 1 ? 'ies' : 'y'}
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>CATEGORY</th>
            <th style={{ ...thStyle, width: '120px' }}>DISPLAY NAME</th>
            <th style={{ ...thStyle, width: '300px' }}>DESCRIPTION</th>
            <th style={{ ...thStyle, width: '90px' }}>DATASETS</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {categories.map(c => (
            <tr
              key={c.id}
              onClick={() => onSelect(c.id)}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 600 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <FolderOpen size={14} color={PROVIDER_COLOR} />
                  {c.id}
                </div>
              </td>
              <td style={tdStyle}>{c.display_name !== c.id ? c.display_name : ''}</td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.MUTED }}>
                {c.description ? truncate(c.description, 80) : ''}
              </td>
              <td style={{ ...tdStyle, textAlign: 'center' }}>
                {c.dataset_count > 0 && (
                  <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{c.dataset_count}</span>
                )}
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

function DatasetsTable({ datasets, onSelect }: { datasets: HkDataset[]; onSelect: (id: string) => void }) {
  return (
    <div>
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {datasets.length} dataset{datasets.length !== 1 ? 's' : ''}
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>TITLE</th>
            <th style={{ ...thStyle, width: '80px' }}>RESOURCES</th>
            <th style={{ ...thStyle, width: '100px' }}>MODIFIED</th>
            <th style={{ ...thStyle, width: '200px' }}>TAGS</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {datasets.map(d => (
            <tr
              key={d.id || d.name}
              onClick={() => onSelect(d.id || d.name)}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={tdStyle}>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600, marginBottom: '2px' }}>
                  {truncate(d.title, 80)}
                </div>
                {d.notes && (
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{truncate(d.notes, 100)}</div>
                )}
              </td>
              <td style={{ ...tdStyle, textAlign: 'center' }}>
                {d.num_resources > 0 && (
                  <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{d.num_resources}</span>
                )}
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatDate(d.metadata_modified)}</td>
              <td style={tdStyle}>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                  {d.tags.slice(0, 3).map((tag, i) => (
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
                  {d.tags.length > 3 && (
                    <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>+{d.tags.length - 3}</span>
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

function ResourcesTable({ resources }: { resources: HkResource[] }) {
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
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {resources.length} resource{resources.length !== 1 ? 's' : ''} available
      </div>
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

function HistoricalTable({ files }: { files: HkHistoricalFile[] }) {
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
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {files.length} historical file{files.length !== 1 ? 's' : ''} found
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>FILE NAME</th>
            <th style={{ ...thStyle, width: '80px' }}>FORMAT</th>
            <th style={{ ...thStyle, width: '120px' }}>CATEGORY</th>
            <th style={{ ...thStyle, width: '100px' }}>DATE</th>
            <th style={{ ...thStyle, width: '90px' }}>SIZE</th>
            <th style={{ ...thStyle, width: '60px' }}>LINK</th>
          </tr>
        </thead>
        <tbody>
          {files.map((f, i) => (
            <tr key={`${f.name}-${i}`} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <td style={tdStyle}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  <Archive size={14} color={FINCEPT.MUTED} style={{ flexShrink: 0 }} />
                  <span style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                    {truncate(f.name, 60)}
                  </span>
                </div>
              </td>
              <td style={tdStyle}>
                {f.format && (
                  <span style={{
                    padding: '2px 8px',
                    backgroundColor: `${PROVIDER_COLOR}15`,
                    color: PROVIDER_COLOR,
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                  }}>
                    {f.format}
                  </span>
                )}
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{f.category || '-'}</td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{f.date ? formatYMD(f.date) : '-'}</td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatSize(f.size)}</td>
              <td style={tdStyle}>
                {f.url && (
                  <button
                    onClick={() => handleOpenUrl(f.url)}
                    style={{
                      background: 'none',
                      border: 'none',
                      color: PROVIDER_COLOR,
                      cursor: 'pointer',
                      padding: '4px',
                      display: 'flex',
                      alignItems: 'center',
                    }}
                    title={f.url}
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

export default DataGovHkProvider;
