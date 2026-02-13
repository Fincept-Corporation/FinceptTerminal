// PxWebProvider - Statistics Finland (PxWeb) portal interface
// Browse database tree -> View table metadata -> Fetch data

import React, { useEffect, useMemo } from 'react';
import { RefreshCw, AlertCircle, Database, Download, FolderOpen, Table2, ArrowLeft, ChevronRight, Play, BarChart3 } from 'lucide-react';
import { FINCEPT } from '../constants';
import { usePxWebData } from '../hooks/usePxWebData';
import type { PxWebView, PxWebNode, PxWebVariable, PxWebStatData } from '../types';

const VIEWS: { id: PxWebView; label: string }[] = [
  { id: 'browse', label: 'BROWSE' },
  { id: 'metadata', label: 'TABLE INFO' },
  { id: 'data', label: 'DATA' },
];

const PROVIDER_COLOR = '#0EA5E9';

const truncate = (text: string, max: number): string =>
  text.length > max ? text.substring(0, max) + '...' : text;

export function PxWebProvider() {
  const {
    activeView, setActiveView,
    nodes, pathStack, currentPath, fetchNodes, navigateInto, navigateBack,
    variables, tableLabel, selectedTable,
    statData, fetchData,
    exportCSV,
    loading, error,
  } = usePxWebData();

  useEffect(() => {
    fetchNodes('');
  }, [fetchNodes]);

  const handleFetchAllData = () => {
    if (!selectedTable) return;
    // Fetch with empty query = all values for first few time periods
    fetchData(selectedTable, []);
  };

  const hasData = (activeView === 'browse' && nodes.length > 0)
    || (activeView === 'metadata' && variables.length > 0)
    || (activeView === 'data' && statData?.value && statData.value.length > 0);

  // Build breadcrumb from pathStack
  const breadcrumb = pathStack.filter(p => p !== '');

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
        {(activeView === 'browse' && pathStack.length > 1) && (
          <button
            onClick={navigateBack}
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
        {(activeView === 'metadata' || activeView === 'data') && (
          <button
            onClick={() => setActiveView('browse')}
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
            BROWSE
          </button>
        )}

        {/* View selector */}
        <div style={{ display: 'flex', gap: '2px', marginRight: '12px' }}>
          {VIEWS.map(v => (
            <button
              key={v.id}
              onClick={() => setActiveView(v.id)}
              disabled={v.id === 'metadata' && !selectedTable}
              style={{
                padding: '5px 12px',
                backgroundColor: activeView === v.id ? PROVIDER_COLOR : 'transparent',
                color: activeView === v.id ? FINCEPT.WHITE : FINCEPT.GRAY,
                border: `1px solid ${activeView === v.id ? PROVIDER_COLOR : FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: v.id === 'metadata' && !selectedTable ? 'not-allowed' : 'pointer',
                opacity: v.id === 'metadata' && !selectedTable ? 0.4 : 1,
              }}
            >
              {v.label}
            </button>
          ))}
        </div>

        {/* Breadcrumb */}
        {activeView === 'browse' && breadcrumb.length > 0 && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>StatFin</span>
            {breadcrumb.map((seg, i) => (
              <React.Fragment key={i}>
                <ChevronRight size={10} />
                <span style={{ color: i === breadcrumb.length - 1 ? PROVIDER_COLOR : FINCEPT.GRAY }}>
                  {seg.split('/').pop()}
                </span>
              </React.Fragment>
            ))}
          </div>
        )}
        {(activeView === 'metadata' || activeView === 'data') && selectedTable && (
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span>Table:</span>
            <span style={{ color: PROVIDER_COLOR }}>{truncate(tableLabel || selectedTable, 50)}</span>
          </div>
        )}

        {/* Actions */}
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '4px' }}>
          {activeView === 'metadata' && selectedTable && (
            <button
              onClick={handleFetchAllData}
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
              <Play size={12} />
              FETCH DATA
            </button>
          )}
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
          {activeView === 'browse' && (
            <button
              onClick={() => fetchNodes(currentPath)}
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
            <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading Statistics Finland data...</div>
          </div>
        ) : error ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
          </div>
        ) : activeView === 'browse' ? (
          <NodesTable nodes={nodes} onSelect={navigateInto} />
        ) : activeView === 'metadata' ? (
          <MetadataPanel variables={variables} tableLabel={tableLabel} />
        ) : activeView === 'data' ? (
          statData ? <DataPanel statData={statData} /> : <EmptyState icon={BarChart3} message="No data loaded" sub="Select a table and click FETCH DATA" />
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

function NodesTable({ nodes, onSelect }: { nodes: PxWebNode[]; onSelect: (n: PxWebNode) => void }) {
  const folders = nodes.filter(n => n.type === 'l');
  const tables = nodes.filter(n => n.type === 't');

  return (
    <div>
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {folders.length} folders, {tables.length} tables
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={{ ...thStyle, width: '40px' }}></th>
            <th style={thStyle}>NAME</th>
            <th style={{ ...thStyle, width: '120px' }}>ID</th>
            <th style={{ ...thStyle, width: '120px' }}>UPDATED</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {nodes.map((n, i) => (
            <tr
              key={n.id || i}
              onClick={() => onSelect(n)}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={{ ...tdStyle, textAlign: 'center' }}>
                {n.type === 'l'
                  ? <FolderOpen size={14} color="#F59E0B" />
                  : <Table2 size={14} color={PROVIDER_COLOR} />
                }
              </td>
              <td style={tdStyle}>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                  {truncate(n.text, 80)}
                </div>
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.MUTED }}>{n.id}</td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>
                {n.updated ? n.updated.substring(0, 10) : '-'}
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

function MetadataPanel({ variables, tableLabel }: { variables: PxWebVariable[]; tableLabel: string }) {
  return (
    <div>
      {tableLabel && (
        <div style={{
          padding: '10px 16px',
          fontSize: '12px',
          color: FINCEPT.WHITE,
          fontWeight: 700,
          backgroundColor: FINCEPT.PANEL_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}>
          {tableLabel}
        </div>
      )}
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        {variables.length} variables/dimensions
      </div>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={thStyle}>VARIABLE</th>
            <th style={{ ...thStyle, width: '80px' }}>VALUES</th>
            <th style={{ ...thStyle, width: '60px' }}>TIME</th>
            <th style={thStyle}>SAMPLE VALUES</th>
          </tr>
        </thead>
        <tbody>
          {variables.map((v, i) => (
            <tr key={v.code || i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
              <td style={tdStyle}>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>{v.text}</div>
                <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{v.code}</div>
              </td>
              <td style={{ ...tdStyle, textAlign: 'center' }}>
                <span style={{ color: PROVIDER_COLOR, fontWeight: 600 }}>{v.values.length}</span>
              </td>
              <td style={{ ...tdStyle, textAlign: 'center', fontSize: '10px' }}>
                {v.time ? <span style={{ color: '#22C55E' }}>Yes</span> : <span style={{ color: FINCEPT.MUTED }}>No</span>}
              </td>
              <td style={tdStyle}>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                  {v.valueTexts.slice(0, 5).map((vt, j) => (
                    <span key={j} style={{
                      padding: '1px 6px',
                      backgroundColor: `${PROVIDER_COLOR}15`,
                      color: PROVIDER_COLOR,
                      borderRadius: '2px',
                      fontSize: '8px',
                      fontWeight: 600,
                    }}>
                      {truncate(vt, 20)}
                    </span>
                  ))}
                  {v.valueTexts.length > 5 && (
                    <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>+{v.valueTexts.length - 5}</span>
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

function DataPanel({ statData }: { statData: PxWebStatData }) {
  // Flatten json-stat2 into a table
  const { headers, rows } = useMemo(() => {
    const dims = statData.id || [];
    const hdrs = dims.map(d => statData.dimension?.[d]?.label || d);
    hdrs.push('Value');

    const dimCats = dims.map(d => {
      const cat = statData.dimension?.[d]?.category;
      const labels = cat?.label || {};
      const indexEntries = Object.entries(cat?.index || {}).sort((a, b) => a[1] - b[1]);
      return indexEntries.map(([k]) => labels[k] || k);
    });

    const sizes = statData.size || dims.map((_, i) => dimCats[i].length);
    const totalValues = statData.value?.length || 0;
    const maxRows = Math.min(totalValues, 500); // Cap display at 500 rows

    const rws: string[][] = [];
    for (let i = 0; i < maxRows; i++) {
      let remainder = i;
      const parts: string[] = [];
      for (let d = dims.length - 1; d >= 0; d--) {
        const dimSize = sizes[d];
        const idx = remainder % dimSize;
        remainder = Math.floor(remainder / dimSize);
        parts.unshift(dimCats[d]?.[idx] || '');
      }
      parts.push(statData.value?.[i] != null ? String(statData.value[i]) : '');
      rws.push(parts);
    }

    return { headers: hdrs, rows: rws };
  }, [statData]);

  return (
    <div>
      <div style={{ padding: '8px 16px', fontSize: '9px', color: FINCEPT.GRAY, backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', justifyContent: 'space-between' }}>
        <span>{statData.label || 'Data'} — {statData.value?.length || 0} values</span>
        {(statData.value?.length || 0) > 500 && (
          <span style={{ color: '#F59E0B' }}>Showing first 500 rows (export CSV for full data)</span>
        )}
      </div>
      <div style={{ overflow: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
              {headers.map((h, i) => (
                <th key={i} style={thStyle}>{h.toUpperCase()}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((row, i) => (
              <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                {row.map((cell, j) => (
                  <td key={j} style={{
                    ...tdStyle,
                    textAlign: j === row.length - 1 ? 'right' : 'left',
                    fontWeight: j === row.length - 1 ? 600 : 400,
                    color: j === row.length - 1 ? PROVIDER_COLOR : FINCEPT.WHITE,
                  }}>
                    {cell}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
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

export default PxWebProvider;
