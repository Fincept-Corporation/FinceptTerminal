// CongressGovProvider - US Congress.gov legislative data interface
// Bills browsing, bill detail (markdown), and summary by congress session

import React, { useEffect, useCallback, useState } from 'react';
import { RefreshCw, AlertCircle, Database, Download, ArrowLeft, ChevronLeft, ChevronRight, Key, Save, Eye } from 'lucide-react';
import { DateRangePicker } from '@/components/common/charts';
import { FINCEPT, CONGRESS_BILL_TYPES } from '../constants';
import { useCongressGovData } from '../hooks/useCongressGovData';
import type { CongressGovView, CongressBill, CongressBillSummary } from '../types';

const VIEWS: { id: CongressGovView; label: string }[] = [
  { id: 'bills', label: 'BILLS' },
  { id: 'summary', label: 'SUMMARY' },
];

const PROVIDER_COLOR = '#8B5CF6';

const truncate = (text: string, max: number): string =>
  text.length > max ? text.substring(0, max) + '...' : text;

const formatDate = (dateStr: string | null): string => {
  if (!dateStr) return '-';
  return dateStr.substring(0, 10);
};

export function CongressGovProvider() {
  const {
    activeView, setActiveView,
    apiKey, apiKeyLoaded, loadApiKey, saveApiKey,
    dateRangePreset, setDateRangePreset,
    startDate, endDate, setStartDate, setEndDate,
    bills, billsTotalCount, congressNumber, setCongressNumber,
    billType, setBillType, billsOffset, setBillsOffset,
    fetchBills,
    billDetail, fetchBillDetail,
    billSummary, fetchSummary,
    exportCSV,
    loading, error,
  } = useCongressGovData();

  const [showApiKeyInput, setShowApiKeyInput] = useState(false);
  const [tempApiKey, setTempApiKey] = useState('');

  useEffect(() => { loadApiKey(); }, [loadApiKey]);
  useEffect(() => { setTempApiKey(apiKey); }, [apiKey]);

  const handleViewChange = (view: CongressGovView) => {
    setActiveView(view);
  };

  const handleFetch = () => {
    if (activeView === 'bills') fetchBills();
    else if (activeView === 'summary') fetchSummary();
  };

  const handleBillClick = (bill: CongressBill) => {
    // Use the bill identifier path e.g. "119/hr/1"
    const billPath = `${bill.congress}/${bill.bill_type.toLowerCase()}/${bill.bill_number}`;
    setActiveView('bill-detail');
    fetchBillDetail(billPath);
  };

  const handleBack = () => {
    setActiveView('bills');
  };

  const handleSaveApiKey = async () => {
    await saveApiKey(tempApiKey);
    setShowApiKeyInput(false);
  };

  const handlePageChange = (direction: 'prev' | 'next') => {
    const newOffset = direction === 'next' ? billsOffset + 100 : Math.max(0, billsOffset - 100);
    setBillsOffset(newOffset);
    setTimeout(() => fetchBills(), 50);
  };

  const inputStyle: React.CSSProperties = {
    padding: '4px 8px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '10px',
    outline: 'none',
    fontFamily: 'inherit',
  };

  const labelStyle: React.CSSProperties = {
    fontSize: '9px',
    color: FINCEPT.GRAY,
    fontWeight: 700,
  };

  const needsApiKey = apiKeyLoaded && !apiKey;

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
        {activeView === 'bill-detail' && (
          <button onClick={handleBack} style={{
            padding: '5px 8px', backgroundColor: 'transparent', color: FINCEPT.GRAY,
            border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px',
            fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <ArrowLeft size={11} /> BACK
          </button>
        )}

        {/* View tabs */}
        <div style={{ display: 'flex', gap: '2px', marginRight: '12px' }}>
          {VIEWS.map(v => (
            <button
              key={v.id}
              onClick={() => handleViewChange(v.id)}
              style={{
                padding: '5px 12px',
                backgroundColor: activeView === v.id || (v.id === 'bills' && activeView === 'bill-detail') ? PROVIDER_COLOR : 'transparent',
                color: activeView === v.id || (v.id === 'bills' && activeView === 'bill-detail') ? FINCEPT.WHITE : FINCEPT.GRAY,
                border: `1px solid ${activeView === v.id || (v.id === 'bills' && activeView === 'bill-detail') ? PROVIDER_COLOR : FINCEPT.BORDER}`,
                borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              }}
            >
              {v.label}
            </button>
          ))}
        </div>

        {/* Filters for bills view */}
        {(activeView === 'bills' || activeView === 'bill-detail') && (
          <>
            <DateRangePicker
              dateRangePreset={dateRangePreset}
              setDateRangePreset={setDateRangePreset}
              startDate={startDate} endDate={endDate}
              setStartDate={setStartDate} setEndDate={setEndDate}
              sourceColor={PROVIDER_COLOR}
            />
            <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={labelStyle}>CONGRESS:</span>
              <input
                type="number"
                value={congressNumber}
                onChange={e => setCongressNumber(Number(e.target.value))}
                min={74} max={130}
                style={{ ...inputStyle, width: '55px', textAlign: 'center' }}
              />
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={labelStyle}>TYPE:</span>
              <select value={billType} onChange={e => setBillType(e.target.value)} style={inputStyle}>
                {CONGRESS_BILL_TYPES.map(t => (
                  <option key={t.value} value={t.value}>{t.label}</option>
                ))}
              </select>
            </div>
          </>
        )}

        {activeView === 'summary' && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span style={labelStyle}>CONGRESS:</span>
            <input
              type="number"
              value={congressNumber}
              onChange={e => setCongressNumber(Number(e.target.value))}
              min={74} max={130}
              style={{ ...inputStyle, width: '55px', textAlign: 'center' }}
            />
          </div>
        )}

        {/* Actions */}
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '4px' }}>
          {/* API Key toggle */}
          <button
            onClick={() => setShowApiKeyInput(!showApiKeyInput)}
            title={apiKey ? 'API key configured' : 'Set API key'}
            style={{
              padding: '5px 8px', backgroundColor: 'transparent',
              color: apiKey ? FINCEPT.GREEN : FINCEPT.GRAY,
              border: `1px solid ${apiKey ? FINCEPT.GREEN : FINCEPT.BORDER}`,
              borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}
          >
            <Key size={11} /> API KEY
          </button>

          {activeView === 'bills' && bills.length > 0 && (
            <button onClick={exportCSV} title="Export CSV" style={{
              padding: '5px 10px', backgroundColor: 'transparent', color: FINCEPT.GRAY,
              border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px',
              fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              <Download size={11} /> CSV
            </button>
          )}

          {activeView !== 'bill-detail' && (
            <button onClick={handleFetch} disabled={loading} style={{
              padding: '5px 14px', backgroundColor: PROVIDER_COLOR, color: FINCEPT.WHITE,
              border: 'none', borderRadius: '2px', fontSize: '10px', fontWeight: 700,
              cursor: loading ? 'wait' : 'pointer', display: 'flex', alignItems: 'center',
              gap: '6px', opacity: loading ? 0.7 : 1,
            }}>
              <RefreshCw size={12} className={loading ? 'animate-spin' : ''} /> FETCH
            </button>
          )}
        </div>
      </div>

      {/* API Key input bar */}
      {showApiKeyInput && (
        <div style={{
          padding: '8px 16px', backgroundColor: FINCEPT.PANEL_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex', alignItems: 'center', gap: '8px',
        }}>
          <Key size={14} color={FINCEPT.GRAY} />
          <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>CONGRESS_GOV_API_KEY:</span>
          <input
            type="password"
            value={tempApiKey}
            onChange={e => setTempApiKey(e.target.value)}
            placeholder="Enter your Congress.gov API key..."
            style={{ ...inputStyle, flex: 1, maxWidth: '400px' }}
          />
          <button onClick={handleSaveApiKey} style={{
            padding: '4px 12px', backgroundColor: PROVIDER_COLOR, color: FINCEPT.WHITE,
            border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Save size={11} /> SAVE
          </button>
          <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
            Get a key at api.congress.gov
          </span>
        </div>
      )}

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {loading ? (
          <LoadingState />
        ) : error ? (
          <ErrorState error={error} onRetry={handleFetch} />
        ) : needsApiKey ? (
          <EmptyState message="API key required" sub="Click the API KEY button above to configure your Congress.gov API key" />
        ) : activeView === 'bills' ? (
          bills.length > 0
            ? <BillsTable bills={bills} totalCount={billsTotalCount} offset={billsOffset} onBillClick={handleBillClick} onPageChange={handlePageChange} />
            : <EmptyState message="Browse Congressional Bills" sub="Set filters and click FETCH to load bills" />
        ) : activeView === 'bill-detail' ? (
          billDetail
            ? <BillDetailView detail={billDetail} />
            : <EmptyState message="Select a bill" sub="Click on a bill from the list to view details" />
        ) : activeView === 'summary' ? (
          billSummary
            ? <SummaryView summary={billSummary} />
            : <EmptyState message="Congressional Summary" sub="Click FETCH to load bill counts by type" />
        ) : null}
      </div>
    </div>
  );
}

// ===== Sub-components =====

function LoadingState() {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <RefreshCw size={32} className="animate-spin" style={{ color: PROVIDER_COLOR, marginBottom: '16px' }} />
      <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading Congress.gov data...</div>
    </div>
  );
}

function ErrorState({ error, onRetry }: { error: string; onRetry: () => void }) {
  const isApiKeyError = error.includes('API key') || error.includes('api_key');
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
      <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>
        {isApiKeyError ? 'API Key Required' : 'Error loading data'}
      </div>
      <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px', marginBottom: '16px' }}>{error}</div>
      {!isApiKeyError && (
        <button onClick={onRetry} style={{
          padding: '8px 16px', backgroundColor: PROVIDER_COLOR, color: FINCEPT.WHITE,
          border: 'none', borderRadius: '2px', fontSize: '10px', fontWeight: 700, cursor: 'pointer',
        }}>
          TRY AGAIN
        </button>
      )}
    </div>
  );
}

function EmptyState({ message, sub }: { message: string; sub: string }) {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
      <Database size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
      <div style={{ fontSize: '12px', color: FINCEPT.WHITE, marginBottom: '8px' }}>{message}</div>
      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{sub}</div>
    </div>
  );
}

function BillsTable({ bills, totalCount, offset, onBillClick, onPageChange }: {
  bills: CongressBill[];
  totalCount: number;
  offset: number;
  onBillClick: (bill: CongressBill) => void;
  onPageChange: (dir: 'prev' | 'next') => void;
}) {
  return (
    <div>
      {/* Pagination header */}
      <div style={{
        padding: '6px 16px', fontSize: '9px', color: FINCEPT.GRAY,
        backgroundColor: FINCEPT.PANEL_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
      }}>
        <span>Showing {offset + 1}-{offset + bills.length} of {totalCount} bills</span>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button onClick={() => onPageChange('prev')} disabled={offset === 0}
            style={{ padding: '2px 8px', backgroundColor: 'transparent', color: offset === 0 ? FINCEPT.MUTED : FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', cursor: offset === 0 ? 'default' : 'pointer', display: 'flex', alignItems: 'center' }}>
            <ChevronLeft size={12} /> PREV
          </button>
          <button onClick={() => onPageChange('next')} disabled={offset + bills.length >= totalCount}
            style={{ padding: '2px 8px', backgroundColor: 'transparent', color: offset + bills.length >= totalCount ? FINCEPT.MUTED : FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', cursor: offset + bills.length >= totalCount ? 'default' : 'pointer', display: 'flex', alignItems: 'center' }}>
            NEXT <ChevronRight size={12} />
          </button>
        </div>
      </div>

      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.DARK_BG }}>
            <th style={{ ...thStyle, width: '50px' }}>#</th>
            <th style={{ ...thStyle, width: '60px' }}>TYPE</th>
            <th style={thStyle}>TITLE</th>
            <th style={{ ...thStyle, width: '80px' }}>CHAMBER</th>
            <th style={{ ...thStyle, width: '100px' }}>UPDATED</th>
            <th style={{ ...thStyle, width: '200px' }}>LATEST ACTION</th>
            <th style={{ ...thStyle, width: '40px' }}></th>
          </tr>
        </thead>
        <tbody>
          {bills.map((b, i) => (
            <tr
              key={`${b.bill_type}-${b.bill_number}-${i}`}
              onClick={() => onBillClick(b)}
              style={{ cursor: 'pointer', borderBottom: `1px solid ${FINCEPT.BORDER}` }}
              onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
              onMouseLeave={e => (e.currentTarget.style.backgroundColor = 'transparent')}
            >
              <td style={{ ...tdStyle, color: PROVIDER_COLOR, fontWeight: 700 }}>{b.bill_number}</td>
              <td style={tdStyle}>
                <span style={{
                  padding: '1px 6px', backgroundColor: `${PROVIDER_COLOR}15`, color: PROVIDER_COLOR,
                  borderRadius: '2px', fontSize: '8px', fontWeight: 700, textTransform: 'uppercase',
                }}>
                  {b.bill_type}
                </span>
              </td>
              <td style={tdStyle}>
                <div style={{ fontSize: '11px', color: FINCEPT.WHITE }}>{truncate(b.title || '', 90)}</div>
              </td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{b.origin_chamber}</td>
              <td style={{ ...tdStyle, fontSize: '10px', color: FINCEPT.GRAY }}>{formatDate(b.update_date)}</td>
              <td style={{ ...tdStyle, fontSize: '9px', color: FINCEPT.MUTED }}>
                {b.latest_action ? truncate(b.latest_action, 50) : '-'}
              </td>
              <td style={tdStyle}><Eye size={13} color={FINCEPT.MUTED} /></td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function BillDetailView({ detail }: { detail: { success: boolean; markdown_content: string; bill_data?: Record<string, any> } }) {
  if (!detail.success) return <EmptyState message="Bill not found" sub="" />;

  const md = detail.markdown_content || '';

  // Render markdown as simple styled blocks
  const lines = md.split('\n');

  return (
    <div style={{ padding: '20px 24px', maxWidth: '900px', margin: '0 auto', overflowY: 'auto', height: '100%' }}>
      {lines.map((line, i) => {
        const trimmed = line.trim();
        if (!trimmed) return <div key={i} style={{ height: '8px' }} />;

        if (trimmed.startsWith('# ')) {
          return <h1 key={i} style={{ fontSize: '18px', color: PROVIDER_COLOR, fontWeight: 700, marginBottom: '12px', lineHeight: 1.3 }}>{trimmed.substring(2)}</h1>;
        }
        if (trimmed.startsWith('## ')) {
          return <h2 key={i} style={{ fontSize: '14px', color: FINCEPT.WHITE, fontWeight: 700, marginTop: '16px', marginBottom: '8px', borderBottom: `1px solid ${FINCEPT.BORDER}`, paddingBottom: '4px' }}>{trimmed.substring(3)}</h2>;
        }
        if (trimmed.startsWith('- **')) {
          const match = trimmed.match(/^- \*\*(.+?)\*\*:?\s*(.*)/);
          if (match) {
            return (
              <div key={i} style={{ fontSize: '11px', padding: '2px 0', display: 'flex', gap: '4px' }}>
                <span style={{ color: FINCEPT.GRAY, fontWeight: 600, minWidth: '120px' }}>{match[1]}:</span>
                <span style={{ color: FINCEPT.WHITE }}>{match[2]}</span>
              </div>
            );
          }
        }
        if (trimmed.startsWith('- ')) {
          return (
            <div key={i} style={{ fontSize: '10px', color: FINCEPT.WHITE, padding: '2px 0 2px 12px' }}>
              <span style={{ color: FINCEPT.MUTED, marginRight: '6px' }}>&bull;</span>
              {trimmed.substring(2)}
            </div>
          );
        }

        return <div key={i} style={{ fontSize: '11px', color: FINCEPT.WHITE, padding: '2px 0', lineHeight: 1.5 }}>{trimmed}</div>;
      })}
    </div>
  );
}

function SummaryView({ summary }: { summary: CongressBillSummary }) {
  const types = summary.bill_types || {};
  const typeLabels: Record<string, string> = {
    hr: 'House Bills', s: 'Senate Bills', hjres: 'House Joint Res.', sjres: 'Senate Joint Res.',
    hconres: 'House Con. Res.', sconres: 'Senate Con. Res.', hres: 'House Simple Res.', sres: 'Senate Simple Res.',
  };

  return (
    <div style={{ padding: '20px 24px' }}>
      <div style={{ fontSize: '16px', color: FINCEPT.WHITE, fontWeight: 700, marginBottom: '4px' }}>
        {summary.congress}th Congress — Bill Summary
      </div>
      <div style={{ fontSize: '11px', color: FINCEPT.GRAY, marginBottom: '20px' }}>
        Total bills fetched: <span style={{ color: PROVIDER_COLOR, fontWeight: 700 }}>{summary.total_bills}</span>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(250px, 1fr))', gap: '12px' }}>
        {Object.entries(types).map(([type, data]) => (
          <div key={type} style={{
            backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px', padding: '14px',
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px' }}>
              <span style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                {typeLabels[type] || type.toUpperCase()}
              </span>
              <span style={{
                padding: '2px 8px', backgroundColor: `${PROVIDER_COLOR}20`, color: PROVIDER_COLOR,
                borderRadius: '2px', fontSize: '12px', fontWeight: 700,
              }}>
                {data.count}
              </span>
            </div>

            {data.error && (
              <div style={{ fontSize: '9px', color: FINCEPT.RED }}>{data.error}</div>
            )}

            {data.latest_bills && data.latest_bills.length > 0 && (
              <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: '8px' }}>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginBottom: '4px', fontWeight: 700 }}>LATEST</div>
                {data.latest_bills.map((b, i) => (
                  <div key={i} style={{ fontSize: '9px', color: FINCEPT.GRAY, padding: '2px 0', borderBottom: `1px solid ${FINCEPT.BORDER}08` }}>
                    <span style={{ color: PROVIDER_COLOR, fontWeight: 600, marginRight: '4px' }}>
                      {b.bill_type}{b.bill_number}
                    </span>
                    {truncate(b.title || '', 50)}
                  </div>
                ))}
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}

// Shared styles
const thStyle: React.CSSProperties = {
  padding: '8px 12px', textAlign: 'left', fontSize: '9px', fontWeight: 700,
  color: PROVIDER_COLOR, borderBottom: `1px solid ${FINCEPT.BORDER}`,
};

const tdStyle: React.CSSProperties = {
  padding: '8px 12px', fontSize: '11px', color: FINCEPT.WHITE,
};

export default CongressGovProvider;
