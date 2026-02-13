// SpainDataProvider - Spain Open Data (datos.gob.es) portal interface
// Hierarchical browsing: Catalogues (Publishers) -> Datasets -> Resources + Search

import React, { useEffect, useCallback } from 'react';
import { RefreshCw, AlertCircle, Database, Download, Search, ChevronRight, ChevronLeft, ExternalLink, FileText, ArrowLeft, BookOpen } from 'lucide-react';
import { FINCEPT } from '../constants';
import { useSpainData, extractTitle, extractFormat } from '../hooks/useSpainData';
import type { SpainDataView, SpainCatalogue, SpainDataset, SpainResource } from '../types';

const VIEWS: { id: SpainDataView; label: string }[] = [
  { id: 'catalogues', label: 'CATALOGUES' },
  { id: 'datasets', label: 'DATASETS' },
  { id: 'search', label: 'SEARCH' },
];

const PROVIDER_COLOR = '#DC2626';

const truncate = (text: string, max: number): string =>
  text.length > max ? text.substring(0, max) + '...' : text;

const formatDate = (dateStr: string | null | undefined): string => {
  if (!dateStr) return '-';
  return dateStr.substring(0, 10);
};

export function SpainDataProvider() {
  const {
    activeView, setActiveView,
    catalogues, selectedCatalogue, setSelectedCatalogue, fetchCatalogues,
    datasets, selectedDataset, setSelectedDataset, fetchDatasets,
    datasetsPage, setDatasetsPage,
    resources, fetchResources,
    searchQuery, setSearchQuery, searchResults, searchDatasets,
    exportCSV,
    loading, error,
  } = useSpainData();

  useEffect(() => {
    fetchCatalogues();
  }, [fetchCatalogues]);

  const handleViewChange = (view: SpainDataView) => {
    setActiveView(view);
  };

  const handleCatalogueClick = (cat: SpainCatalogue) => {
    const pubId = cat.extracted_id;
    setSelectedCatalogue(pubId);
    setSelectedDataset(null);
    setDatasetsPage(1);
    setActiveView('datasets');
    fetchDatasets(pubId);
  };

  const handleDatasetClick = (d: SpainDataset) => {
    const id = d.extracted_id || '';
    setSelectedDataset(id);
    setActiveView('resources');
    fetchResources(id);
  };

  const handleBack = () => {
    if (activeView === 'resources') {
      setActiveView('datasets');
      setSelectedDataset(null);
    } else if (activeView === 'datasets') {
      setActiveView('catalogues');
      setSelectedCatalogue(null);
    }
  };

  const handleSearchKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') searchDatasets();
  };

  const handleNextPage = () => {
    const next = datasetsPage + 1;
    setDatasetsPage(next);
    fetchDatasets(selectedCatalogue || undefined);
  };

  const handlePrevPage = () => {
    if (datasetsPage <= 1) return;
    const prev = datasetsPage - 1;
    setDatasetsPage(prev);
    fetchDatasets(selectedCatalogue || undefined);
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

  const hasData = (activeView === 'catalogues' && catalogues.length > 0)
    || (activeView === 'datasets' && datasets.length > 0)
    || (activeView === 'resources' && resources.length > 0)
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
              placeholder="Search Spanish datasets..."
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
        {(activeView === 'datasets' && selectedCatalogue) && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>Catalogues</span>
            <ChevronRight size={10} />
            <span style={{ color: PROVIDER_COLOR }}>{selectedCatalogue}</span>
          </div>
        )}
        {(activeView === 'resources' && selectedDataset) && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>Catalogues</span>
            <ChevronRight size={10} />
            <span>{selectedCatalogue}</span>
            <ChevronRight size={10} />
            <span style={{ color: PROVIDER_COLOR }}>{truncate(selectedDataset, 30)}</span>
          </div>
        )}

        {/* Pagination for datasets */}
        {activeView === 'datasets' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginLeft: '8px' }}>
            <button
              onClick={handlePrevPage}
              disabled={datasetsPage <= 1 || loading}
              style={{
                padding: '4px 8px',
                backgroundColor: 'transparent',
                color: datasetsPage <= 1 ? FINCEPT.MUTED : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                cursor: datasetsPage <= 1 || loading ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
              }}
            >
              <ChevronLeft size={12} />
            </button>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Page {datasetsPage}</span>
            <button
              onClick={handleNextPage}
              disabled={loading || datasets.length === 0}
              style={{
                padding: '4px 8px',
                backgroundColor: 'transparent',
                color: loading || datasets.length === 0 ? FINCEPT.MUTED : FINCEPT.GRAY,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                cursor: loading || datasets.length === 0 ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
              }}
            >
              <ChevronRight size={12} />
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
          {activeView === 'catalogues' && (
            <button
              onClick={fetchCatalogues}
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
            <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading datos.gob.es data...</div>
          </div>
        ) : error ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
          </div>
        ) : activeView === 'catalogues' ? (
          <CataloguesTable catalogues={catalogues} onSelect={handleCatalogueClick} />
        ) : activeView === 'datasets' ? (
          <DatasetsTable datasets={datasets} onSelect={handleDatasetClick} />
        ) : activeView === 'resources' ? (
          <ResourcesTable resources={resources} />
        ) : activeView === 'search' ? (
          searchResults.length > 0
            ? <DatasetsTable datasets={searchResults} onSelect={handleDatasetClick} />
            : <EmptyState icon={Search} message="Search Spanish government datasets" sub="Enter a query above and press SEARCH or Enter" />
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

function CataloguesTable({ catalogues, onSelect }: { catalogues: SpainCatalogue[]; onSelect: (c: SpainCatalogue) => void }) {
  return (
    <div>
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {catalogues.length} publishers
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>PUBLISHER ID</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {catalogues.map((c, i) => (
            <tr
              key={c.extracted_id || i}
              onClick={() => onSelect(c)}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={tdStyle}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <BookOpen size={14} color={FINCEPT.MUTED} />
                  <div>
                    <div style={{ fontSize: '11px', color: PROVIDER_COLOR, fontWeight: 600 }}>
                      {c.extracted_id}
                    </div>
                    {c.uri && (
                      <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>{truncate(c.uri, 80)}</div>
                    )}
                  </div>
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

function DatasetsTable({ datasets, onSelect }: { datasets: SpainDataset[]; onSelect: (d: SpainDataset) => void }) {
  return (
    <div>
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {datasets.length} datasets
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>TITLE</th>
            <th style={{ ...thStyle, width: '80px' }}>FILES</th>
            <th style={{ ...thStyle, width: '100px' }}>MODIFIED</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {datasets.map((d, i) => {
            const title = extractTitle(d.title);
            const translated = d.title_translation?.translated;
            const distCount = d.distribution?.length || 0;

            return (
              <tr
                key={d.extracted_id || i}
                onClick={() => onSelect(d)}
                style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
                onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
                onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
              >
                <td style={tdStyle}>
                  <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600, marginBottom: '2px' }}>
                    {truncate(title, 80)}
                  </div>
                  {translated && translated !== title && (
                    <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                      {truncate(translated, 80)}
                    </div>
                  )}
                </td>
                <td style={{ ...tdStyle, textAlign: 'center' }}>
                  <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{distCount}</span>
                </td>
                <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatDate(d.modified)}</td>
                <td style={tdStyle}>
                  <ChevronRight size={14} color={FINCEPT.MUTED} />
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}

function ResourcesTable({ resources }: { resources: SpainResource[] }) {
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
            <th style={{ ...thStyle, width: '60px' }}>LINK</th>
          </tr>
        </thead>
        <tbody>
          {resources.map((r, i) => {
            const title = extractTitle(r.title);
            const format = extractFormat(r.format);

            return (
              <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                <td style={tdStyle}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                    <FileText size={14} color={FINCEPT.MUTED} />
                    <div>
                      <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                        {title || `Resource ${i}`}
                      </div>
                      {r.title_translation?.translated && r.title_translation.translated !== title && (
                        <div style={{ fontSize: '9px', color: FINCEPT.MUTED, fontStyle: 'italic' }}>
                          {truncate(r.title_translation.translated, 80)}
                        </div>
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
                    {format}
                  </span>
                </td>
                <td style={tdStyle}>
                  {r.accessURL && (
                    <button
                      onClick={() => handleOpenUrl(r.accessURL!)}
                      style={{
                        background: 'none',
                        border: 'none',
                        color: PROVIDER_COLOR,
                        cursor: 'pointer',
                        padding: '4px',
                        display: 'flex',
                        alignItems: 'center',
                      }}
                      title={r.accessURL}
                    >
                      <ExternalLink size={14} />
                    </button>
                  )}
                </td>
              </tr>
            );
          })}
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

export default SpainDataProvider;
