// USTreasuryProvider - Full US Treasury data interface
// Uses shared DateRangePicker from common/charts (same pattern as Economics tab)

import React from 'react';
import { RefreshCw, AlertCircle, Database, Download } from 'lucide-react';
import { DateRangePicker } from '@/components/common/charts';
import { FINCEPT, SECURITY_TYPES, AUCTION_SECURITY_TYPES } from '../constants';
import { useUSTreasuryData } from '../hooks';
import { USTreasuryPricesPanel } from './USTreasuryPricesPanel';
import { USTreasuryAuctionsPanel } from './USTreasuryAuctionsPanel';
import { USTreasurySummaryPanel } from './USTreasurySummaryPanel';
import type { USTreasuryEndpoint, SecurityTypeFilter } from '../types';

const ENDPOINTS: { id: USTreasuryEndpoint; label: string }[] = [
  { id: 'treasury_prices', label: 'PRICES' },
  { id: 'treasury_auctions', label: 'AUCTIONS' },
  { id: 'summary', label: 'SUMMARY' },
];

const PROVIDER_COLOR = '#3B82F6';

export function USTreasuryProvider() {
  const {
    activeEndpoint, setActiveEndpoint,
    dateRangePreset, setDateRangePreset,
    startDate, endDate, setStartDate, setEndDate,
    pricesData, pricesSecurityType, setPricesSecurityType,
    pricesCusip, setPricesCusip, fetchPrices,
    auctionsData,
    auctionsSecurityType, setAuctionsSecurityType, auctionsPageSize, setAuctionsPageSize,
    auctionsPageNum, setAuctionsPageNum, fetchAuctions,
    summaryData, fetchSummary,
    exportCSV,
    loading, error,
  } = useUSTreasuryData();

  const handleFetch = () => {
    switch (activeEndpoint) {
      case 'treasury_prices': fetchPrices(); break;
      case 'treasury_auctions': fetchAuctions(); break;
      case 'summary': fetchSummary(); break;
    }
  };

  const handlePageChange = (page: number) => {
    if (page >= 1) {
      setAuctionsPageNum(page);
      setTimeout(() => fetchAuctions(), 50);
    }
  };

  const inputStyle: React.CSSProperties = {
    padding: '4px 8px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '10px',
    outline: 'none',
  };

  const labelStyle: React.CSSProperties = {
    fontSize: '9px',
    color: FINCEPT.GRAY,
    fontWeight: 700,
  };

  const hasData = activeEndpoint === 'treasury_prices' ? pricesData
    : activeEndpoint === 'treasury_auctions' ? auctionsData
    : summaryData;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Endpoint tabs */}
      <div style={{
        padding: '8px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        flexWrap: 'wrap',
      }}>
        {/* Endpoint selector */}
        <div style={{ display: 'flex', gap: '2px', marginRight: '12px' }}>
          {ENDPOINTS.map(ep => (
            <button
              key={ep.id}
              onClick={() => setActiveEndpoint(ep.id)}
              style={{
                padding: '5px 12px',
                backgroundColor: activeEndpoint === ep.id ? PROVIDER_COLOR : 'transparent',
                color: activeEndpoint === ep.id ? FINCEPT.WHITE : FINCEPT.GRAY,
                border: `1px solid ${activeEndpoint === ep.id ? PROVIDER_COLOR : FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
              }}
            >
              {ep.label}
            </button>
          ))}
        </div>

        {/* Shared DateRangePicker — same component used by Economics tab */}
        <DateRangePicker
          dateRangePreset={dateRangePreset}
          setDateRangePreset={setDateRangePreset}
          startDate={startDate}
          endDate={endDate}
          setStartDate={setStartDate}
          setEndDate={setEndDate}
          sourceColor={PROVIDER_COLOR}
        />

        <div style={{ width: '1px', height: '20px', backgroundColor: FINCEPT.BORDER }} />

        {/* Endpoint-specific filters */}
        {activeEndpoint === 'treasury_prices' && (
          <>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={labelStyle}>TYPE:</span>
              <select
                value={pricesSecurityType}
                onChange={e => setPricesSecurityType(e.target.value as SecurityTypeFilter)}
                style={inputStyle}
              >
                {SECURITY_TYPES.map(t => (
                  <option key={t.value} value={t.value}>{t.label}</option>
                ))}
              </select>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={labelStyle}>CUSIP:</span>
              <input
                type="text"
                value={pricesCusip}
                onChange={e => setPricesCusip(e.target.value)}
                placeholder="Filter..."
                style={{ ...inputStyle, width: '100px' }}
              />
            </div>
          </>
        )}

        {activeEndpoint === 'treasury_auctions' && (
          <>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={labelStyle}>TYPE:</span>
              <select
                value={auctionsSecurityType}
                onChange={e => setAuctionsSecurityType(e.target.value)}
                style={inputStyle}
              >
                {AUCTION_SECURITY_TYPES.map(t => (
                  <option key={t.value} value={t.value}>{t.label}</option>
                ))}
              </select>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={labelStyle}>PER PAGE:</span>
              <select
                value={auctionsPageSize}
                onChange={e => { setAuctionsPageSize(Number(e.target.value)); setAuctionsPageNum(1); }}
                style={inputStyle}
              >
                {[10, 25, 50, 100].map(n => (
                  <option key={n} value={n}>{n}</option>
                ))}
              </select>
            </div>
          </>
        )}

        {/* Actions: Fetch + Export */}
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
          <button
            onClick={handleFetch}
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
            FETCH
          </button>
        </div>
      </div>

      {/* Content area */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        {loading ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <RefreshCw size={32} className="animate-spin" style={{ color: PROVIDER_COLOR, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.WHITE }}>Loading US Treasury data...</div>
          </div>
        ) : error ? (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <AlertCircle size={32} style={{ color: FINCEPT.RED, marginBottom: '16px' }} />
            <div style={{ fontSize: '12px', color: FINCEPT.RED, marginBottom: '8px' }}>Error loading data</div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>{error}</div>
            <button
              onClick={handleFetch}
              style={{
                marginTop: '16px',
                padding: '8px 16px',
                backgroundColor: PROVIDER_COLOR,
                color: FINCEPT.WHITE,
                border: 'none',
                borderRadius: '2px',
                fontSize: '10px',
                fontWeight: 700,
                cursor: 'pointer',
              }}
            >
              TRY AGAIN
            </button>
          </div>
        ) : hasData ? (
          <>
            {activeEndpoint === 'treasury_prices' && pricesData && (
              <USTreasuryPricesPanel data={pricesData} />
            )}
            {activeEndpoint === 'treasury_auctions' && auctionsData && (
              <USTreasuryAuctionsPanel
                data={auctionsData}
                pageNum={auctionsPageNum}
                pageSize={auctionsPageSize}
                onPageChange={handlePageChange}
              />
            )}
            {activeEndpoint === 'summary' && summaryData && (
              <USTreasurySummaryPanel data={summaryData} />
            )}
          </>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
            <Database size={48} style={{ color: FINCEPT.MUTED, marginBottom: '16px', opacity: 0.5 }} />
            <div style={{ fontSize: '12px', color: FINCEPT.WHITE, marginBottom: '8px' }}>
              Select an endpoint and click FETCH
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, textAlign: 'center', maxWidth: '400px' }}>
              {activeEndpoint === 'treasury_prices'
                ? 'Get end-of-day prices for all Treasury securities including Bills, Notes, Bonds, TIPS, and FRN.'
                : activeEndpoint === 'treasury_auctions'
                ? 'Browse Treasury auction results with bid-to-cover ratios, accepted amounts, and pricing details.'
                : 'View aggregate market statistics including security counts, yield and price distributions.'
              }
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default USTreasuryProvider;
