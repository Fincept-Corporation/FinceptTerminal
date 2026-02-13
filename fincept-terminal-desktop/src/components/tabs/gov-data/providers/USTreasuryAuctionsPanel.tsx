// US Treasury Auctions Panel - Auction results with detailed bid data

import React, { useState, useMemo } from 'react';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Line, ComposedChart, Cell,
} from 'recharts';
import { BarChart3, Table2 } from 'lucide-react';
import { FINCEPT, AUCTION_DISPLAY_FIELDS } from '../constants';
import type { TreasuryAuctionsResponse, TreasuryAuctionRecord } from '../types';

const tooltipStyle = {
  backgroundColor: '#1A1A1A',
  border: '1px solid #2A2A2A',
  borderRadius: '4px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", monospace',
};

const SEC_COLORS: Record<string, string> = {
  Bill: '#3B82F6',
  Note: '#10B981',
  Bond: '#F59E0B',
  TIPS: '#8B5CF6',
  FRN: '#EC4899',
  CMB: '#06B6D4',
};

type ViewMode = 'table' | 'chart';

interface Props {
  data: TreasuryAuctionsResponse;
  pageNum: number;
  pageSize: number;
  onPageChange: (page: number) => void;
}

const formatCurrency = (val: number | null): string => {
  if (val === null || val === undefined) return '-';
  if (Math.abs(val) >= 1e12) return `$${(val / 1e12).toFixed(1)}T`;
  if (Math.abs(val) >= 1e9) return `$${(val / 1e9).toFixed(1)}B`;
  if (Math.abs(val) >= 1e6) return `$${(val / 1e6).toFixed(1)}M`;
  return `$${val.toLocaleString()}`;
};

const formatRate = (val: number | null): string => {
  if (val === null || val === undefined) return '-';
  return `${(val * 100).toFixed(3)}%`;
};

const formatRatio = (val: number | null): string => {
  if (val === null || val === undefined) return '-';
  return val.toFixed(2);
};

const formatPct = (val: number | null): string => {
  if (val === null || val === undefined) return '-';
  return `${(val * 100).toFixed(2)}%`;
};

const formatPrice = (val: number | null): string => {
  if (val === null || val === undefined) return '-';
  return val.toFixed(6);
};

const formatDate = (val: string | null): string => {
  if (!val) return '-';
  return val.split('T')[0];
};

const formatField = (val: any, format?: string): string => {
  if (val === null || val === undefined) return '-';
  switch (format) {
    case 'currency': return formatCurrency(val);
    case 'rate': return formatRate(val);
    case 'ratio': return formatRatio(val);
    case 'pct': return formatPct(val);
    case 'price': return formatPrice(val);
    default:
      if (typeof val === 'string' && val.includes('T')) return formatDate(val);
      return String(val);
  }
};

type SortField = string;
type SortDir = 'asc' | 'desc';

export function USTreasuryAuctionsPanel({ data, pageNum, pageSize, onPageChange }: Props) {
  const [sortField, setSortField] = useState<SortField>('auctionDate');
  const [sortDir, setSortDir] = useState<SortDir>('desc');
  const [expandedRow, setExpandedRow] = useState<number | null>(null);
  const [viewMode, setViewMode] = useState<ViewMode>('table');

  const sortedData = useMemo(() => {
    return [...data.data].sort((a, b) => {
      const aVal = a[sortField];
      const bVal = b[sortField];
      if (aVal === null || aVal === undefined) return 1;
      if (bVal === null || bVal === undefined) return -1;
      if (typeof aVal === 'number' && typeof bVal === 'number') {
        return sortDir === 'asc' ? aVal - bVal : bVal - aVal;
      }
      return sortDir === 'asc'
        ? String(aVal).localeCompare(String(bVal))
        : String(bVal).localeCompare(String(aVal));
    });
  }, [data.data, sortField, sortDir]);

  const handleSort = (field: SortField) => {
    if (sortField === field) {
      setSortDir(d => d === 'asc' ? 'desc' : 'asc');
    } else {
      setSortField(field);
      setSortDir('desc');
    }
  };

  const sortIndicator = (field: SortField) => {
    if (sortField !== field) return '';
    return sortDir === 'asc' ? ' \u25B2' : ' \u25BC';
  };

  // Aggregate stats
  const stats = useMemo(() => {
    const bids = data.data.map(d => d.bidToCoverRatio).filter((v): v is number => v !== null);
    const offerings = data.data.map(d => d.offeringAmount).filter((v): v is number => v !== null);
    return {
      avgBidCover: bids.length > 0 ? bids.reduce((a, b) => a + b, 0) / bids.length : 0,
      totalOffering: offerings.reduce((a, b) => a + b, 0),
    };
  }, [data.data]);

  // Chart data: bid-to-cover by auction date
  const chartData = useMemo(() => {
    return [...data.data]
      .filter(r => r.auctionDate && r.bidToCoverRatio)
      .sort((a, b) => (a.auctionDate || '').localeCompare(b.auctionDate || ''))
      .map(r => ({
        date: formatDate(r.auctionDate),
        bidCover: r.bidToCoverRatio,
        offering: r.offeringAmount ? r.offeringAmount / 1e9 : 0, // in billions
        type: r.securityType,
        term: r.securityTerm,
        cusip: r.cusip,
        fill: SEC_COLORS[r.securityType] || FINCEPT.GRAY,
      }));
  }, [data.data]);

  // Type distribution for chart
  const typeDistribution = useMemo(() => {
    const counts: Record<string, { count: number; totalOffering: number }> = {};
    data.data.forEach(r => {
      const t = r.securityType;
      if (!counts[t]) counts[t] = { count: 0, totalOffering: 0 };
      counts[t].count++;
      if (r.offeringAmount) counts[t].totalOffering += r.offeringAmount;
    });
    return Object.entries(counts).map(([type, v]) => ({
      type,
      count: v.count,
      totalOffering: +(v.totalOffering / 1e9).toFixed(1),
      fill: SEC_COLORS[type] || FINCEPT.GRAY,
    })).sort((a, b) => b.totalOffering - a.totalOffering);
  }, [data.data]);

  const thStyle: React.CSSProperties = {
    padding: '8px 8px',
    fontSize: '9px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    textAlign: 'left',
    borderBottom: `1px solid ${FINCEPT.BORDER}`,
    cursor: 'pointer',
    userSelect: 'none',
    whiteSpace: 'nowrap',
  };

  const tdStyle: React.CSSProperties = {
    padding: '6px 8px',
    fontSize: '10px',
    color: FINCEPT.WHITE,
    borderBottom: `1px solid ${FINCEPT.BORDER}08`,
    whiteSpace: 'nowrap',
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Stats bar */}
      <div style={{
        padding: '8px 16px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '20px',
        fontSize: '10px',
      }}>
        <span><span style={{ color: FINCEPT.GRAY }}>RESULTS:</span> <span style={{ color: FINCEPT.WHITE }}>{data.total_records}</span></span>
        <span><span style={{ color: FINCEPT.GRAY }}>AVG BID/COVER:</span> <span style={{ color: '#3B82F6' }}>{stats.avgBidCover.toFixed(2)}x</span></span>
        <span><span style={{ color: FINCEPT.GRAY }}>TOTAL OFFERING:</span> <span style={{ color: FINCEPT.GREEN }}>{formatCurrency(stats.totalOffering)}</span></span>
        {data.filters.security_type && (
          <span><span style={{ color: FINCEPT.GRAY }}>TYPE:</span> <span style={{ color: FINCEPT.ORANGE }}>{data.filters.security_type}</span></span>
        )}
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '2px' }}>
          <button
            onClick={() => setViewMode('table')}
            style={{
              padding: '3px 6px', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
              backgroundColor: viewMode === 'table' ? '#3B82F6' : 'transparent',
              color: viewMode === 'table' ? FINCEPT.WHITE : FINCEPT.GRAY,
              cursor: 'pointer', display: 'flex', alignItems: 'center',
            }}
          >
            <Table2 size={12} />
          </button>
          <button
            onClick={() => setViewMode('chart')}
            style={{
              padding: '3px 6px', border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
              backgroundColor: viewMode === 'chart' ? '#3B82F6' : 'transparent',
              color: viewMode === 'chart' ? FINCEPT.WHITE : FINCEPT.GRAY,
              cursor: 'pointer', display: 'flex', alignItems: 'center',
            }}
          >
            <BarChart3 size={12} />
          </button>
        </div>
      </div>

      {viewMode === 'chart' ? (
        /* Chart View */
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {/* Bid-to-Cover Ratio over time */}
          <div style={{
            marginBottom: '20px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            padding: '16px',
          }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '12px' }}>
              Bid-to-Cover Ratio & Offering Amount by Auction
            </div>
            <ResponsiveContainer width="100%" height={280}>
              <ComposedChart data={chartData} margin={{ top: 5, right: 30, left: 0, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
                <XAxis dataKey="date" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} interval={Math.max(0, Math.floor(chartData.length / 12))} angle={-20} textAnchor="end" height={50} />
                <YAxis yAxisId="left" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} label={{ value: 'Bid/Cover', angle: -90, position: 'insideLeft', fill: FINCEPT.GRAY, fontSize: 9 }} />
                <YAxis yAxisId="right" orientation="right" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} label={{ value: 'Offering ($B)', angle: 90, position: 'insideRight', fill: FINCEPT.GRAY, fontSize: 9 }} />
                <Tooltip
                  contentStyle={tooltipStyle}
                  labelStyle={{ color: FINCEPT.WHITE }}
                  formatter={(value: any, name: string) => {
                    if (name === 'Bid/Cover') return [Number(value).toFixed(2) + 'x', name];
                    if (name === 'Offering ($B)') return ['$' + Number(value).toFixed(1) + 'B', name];
                    return [value, name];
                  }}
                />
                <Bar yAxisId="right" dataKey="offering" name="Offering ($B)" opacity={0.4} radius={[2, 2, 0, 0]}>
                  {chartData.map((entry, i) => (
                    <Cell key={i} fill={entry.fill} />
                  ))}
                </Bar>
                <Line yAxisId="left" type="monotone" dataKey="bidCover" name="Bid/Cover" stroke="#3B82F6" strokeWidth={2} dot={{ r: 3, fill: '#3B82F6' }} />
              </ComposedChart>
            </ResponsiveContainer>
          </div>

          {/* Type Distribution */}
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            padding: '16px',
          }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '12px' }}>
              Auction Count & Total Offering by Security Type
            </div>
            <ResponsiveContainer width="100%" height={200}>
              <BarChart data={typeDistribution} margin={{ top: 5, right: 30, left: 0, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
                <XAxis dataKey="type" tick={{ fill: FINCEPT.GRAY, fontSize: 10 }} />
                <YAxis yAxisId="left" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} />
                <YAxis yAxisId="right" orientation="right" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} />
                <Tooltip contentStyle={tooltipStyle} labelStyle={{ color: FINCEPT.WHITE }} />
                <Bar yAxisId="left" dataKey="count" name="Auctions" radius={[2, 2, 0, 0]}>
                  {typeDistribution.map((entry, i) => (
                    <Cell key={i} fill={entry.fill} />
                  ))}
                </Bar>
                <Bar yAxisId="right" dataKey="totalOffering" name="Total ($B)" fill="#10B981" opacity={0.5} radius={[2, 2, 0, 0]} />
              </BarChart>
            </ResponsiveContainer>
          </div>
        </div>
      ) : (
        /* Table View */
        <div style={{ flex: 1, overflow: 'auto' }}>
          <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: '"IBM Plex Mono", monospace' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: FINCEPT.HEADER_BG, zIndex: 1 }}>
              <tr>
                {AUCTION_DISPLAY_FIELDS.map(f => (
                  <th
                    key={f.key}
                    style={{ ...thStyle, minWidth: f.width }}
                    onClick={() => handleSort(f.key)}
                  >
                    {f.label}{sortIndicator(f.key)}
                  </th>
                ))}
                <th style={{ ...thStyle, width: 40 }}></th>
              </tr>
            </thead>
            <tbody>
              {sortedData.map((row, i) => (
                <React.Fragment key={`${row.cusip}-${i}`}>
                  <tr
                    style={{ backgroundColor: i % 2 === 0 ? 'transparent' : `${FINCEPT.PANEL_BG}80` }}
                    onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
                    onMouseLeave={e => (e.currentTarget.style.backgroundColor = i % 2 === 0 ? 'transparent' : `${FINCEPT.PANEL_BG}80`)}
                  >
                    {AUCTION_DISPLAY_FIELDS.map(f => (
                      <td
                        key={f.key}
                        style={{
                          ...tdStyle,
                          color: f.key === 'cusip' ? '#3B82F6' :
                                 f.format === 'rate' ? FINCEPT.YELLOW :
                                 f.format === 'currency' ? FINCEPT.GREEN :
                                 f.format === 'ratio' ? '#3B82F6' :
                                 FINCEPT.WHITE,
                          fontWeight: f.key === 'cusip' ? 600 : 400,
                        }}
                      >
                        {formatField(row[f.key], f.format)}
                      </td>
                    ))}
                    <td style={tdStyle}>
                      <button
                        onClick={() => setExpandedRow(expandedRow === i ? null : i)}
                        style={{
                          background: 'none',
                          border: 'none',
                          color: FINCEPT.GRAY,
                          cursor: 'pointer',
                          fontSize: '12px',
                          padding: '2px 4px',
                        }}
                      >
                        {expandedRow === i ? '\u25B4' : '\u25BE'}
                      </button>
                    </td>
                  </tr>
                  {expandedRow === i && (
                    <tr>
                      <td colSpan={AUCTION_DISPLAY_FIELDS.length + 1} style={{ padding: 0 }}>
                        <AuctionDetailRow record={row} />
                      </td>
                    </tr>
                  )}
                </React.Fragment>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Pagination */}
      <div style={{
        padding: '8px 16px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontSize: '10px',
      }}>
        <span style={{ color: FINCEPT.GRAY }}>
          Page {pageNum} ({data.total_records} records)
        </span>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => onPageChange(pageNum - 1)}
            disabled={pageNum <= 1}
            style={{
              padding: '4px 10px',
              backgroundColor: pageNum <= 1 ? FINCEPT.MUTED : FINCEPT.PANEL_BG,
              color: pageNum <= 1 ? FINCEPT.GRAY : FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: pageNum <= 1 ? 'default' : 'pointer',
            }}
          >
            PREV
          </button>
          <button
            onClick={() => onPageChange(pageNum + 1)}
            disabled={data.total_records < pageSize}
            style={{
              padding: '4px 10px',
              backgroundColor: data.total_records < pageSize ? FINCEPT.MUTED : FINCEPT.PANEL_BG,
              color: data.total_records < pageSize ? FINCEPT.GRAY : FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: data.total_records < pageSize ? 'default' : 'pointer',
            }}
          >
            NEXT
          </button>
        </div>
      </div>
    </div>
  );
}

// Expanded detail row for a single auction
function AuctionDetailRow({ record }: { record: TreasuryAuctionRecord }) {
  const detailGroups = [
    {
      title: 'Pricing',
      fields: [
        { label: 'High Discount Rate', value: formatRate(record.highDiscountRate) },
        { label: 'High Investment Rate', value: formatRate(record.highInvestmentRate) },
        { label: 'High Price', value: formatPrice(record.highPrice) },
        { label: 'Price Per $100', value: record.pricePer100 || '-' },
        { label: 'Reopening', value: record.reopening || '-' },
      ],
    },
    {
      title: 'Demand',
      fields: [
        { label: 'Bid-to-Cover', value: formatRatio(record.bidToCoverRatio) },
        { label: 'Total Tendered', value: formatCurrency(record.totalTendered) },
        { label: 'Total Accepted', value: formatCurrency(record.totalAccepted) },
        { label: 'Competitive Tendered', value: formatCurrency(record.competitiveTendered) },
        { label: 'Competitive Accepted', value: formatCurrency(record.competitiveAccepted) },
      ],
    },
    {
      title: 'Bidder Breakdown',
      fields: [
        { label: 'Direct Bidder', value: formatCurrency(record.directBidderAccepted) },
        { label: 'Indirect Bidder', value: formatCurrency(record.indirectBidderAccepted) },
        { label: 'Primary Dealer', value: formatCurrency(record.primaryDealerAccepted) },
        { label: 'Allocation %', value: formatPct(record.allocationPercentage) },
        { label: 'Offering Amount', value: formatCurrency(record.offeringAmount) },
      ],
    },
  ];

  return (
    <div style={{
      padding: '12px 16px',
      backgroundColor: `${FINCEPT.PANEL_BG}`,
      borderBottom: `2px solid ${FINCEPT.BORDER}`,
      display: 'flex',
      gap: '24px',
    }}>
      {detailGroups.map(group => (
        <div key={group.title} style={{ flex: 1 }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: '#3B82F6', marginBottom: '8px', textTransform: 'uppercase' }}>
            {group.title}
          </div>
          {group.fields.map(f => (
            <div key={f.label} style={{ display: 'flex', justifyContent: 'space-between', padding: '2px 0', fontSize: '10px' }}>
              <span style={{ color: FINCEPT.GRAY }}>{f.label}</span>
              <span style={{ color: FINCEPT.WHITE, fontWeight: 500 }}>{f.value}</span>
            </div>
          ))}
        </div>
      ))}
    </div>
  );
}

export default USTreasuryAuctionsPanel;
