// US Treasury Prices Panel - End-of-day prices for Treasury securities

import React, { useState, useMemo } from 'react';
import {
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  ScatterChart, Scatter, Cell, Legend,
} from 'recharts';
import { BarChart3, Table2 } from 'lucide-react';
import { FINCEPT } from '../constants';
import type { TreasuryPricesResponse, TreasuryPriceRecord } from '../types';

interface Props {
  data: TreasuryPricesResponse;
}

const formatPrice = (val: number | null): string => {
  if (val === null || val === undefined) return '-';
  return val.toFixed(6);
};

const formatRate = (val: number | null): string => {
  if (val === null || val === undefined || val === 0) return '-';
  return `${(val * 100).toFixed(3)}%`;
};

const formatDate = (val: string | null): string => {
  if (!val) return '-';
  return val;
};

type SortField = keyof TreasuryPriceRecord;
type SortDir = 'asc' | 'desc';

const TYPE_COLORS: Record<string, string> = {
  'MARKET BASED BILL': '#3B82F6',
  'MARKET BASED NOTE': '#10B981',
  'MARKET BASED BOND': '#F59E0B',
  'TIPS': '#8B5CF6',
  'MARKET BASED FRN': '#EC4899',
};

const tooltipStyle = {
  backgroundColor: '#1A1A1A',
  border: '1px solid #2A2A2A',
  borderRadius: '4px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", monospace',
};

type ViewMode = 'table' | 'chart';

export function USTreasuryPricesPanel({ data }: Props) {
  const [sortField, setSortField] = useState<SortField>('maturity_date');
  const [sortDir, setSortDir] = useState<SortDir>('asc');
  const [filterType, setFilterType] = useState<string>('all');
  const [searchCusip, setSearchCusip] = useState('');
  const [viewMode, setViewMode] = useState<ViewMode>('table');

  // Extract unique security types from data
  const securityTypes = useMemo(() => {
    const types = new Set(data.data.map(d => d.security_type));
    return Array.from(types).sort();
  }, [data.data]);

  // Filter and sort
  const filteredData = useMemo(() => {
    let rows = data.data;
    if (filterType !== 'all') {
      rows = rows.filter(r => r.security_type === filterType);
    }
    if (searchCusip.trim()) {
      rows = rows.filter(r => r.cusip.toLowerCase().includes(searchCusip.toLowerCase()));
    }
    rows = [...rows].sort((a, b) => {
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
    return rows;
  }, [data.data, filterType, searchCusip, sortField, sortDir]);

  const handleSort = (field: SortField) => {
    if (sortField === field) {
      setSortDir(d => d === 'asc' ? 'desc' : 'asc');
    } else {
      setSortField(field);
      setSortDir('asc');
    }
  };

  const sortIndicator = (field: SortField) => {
    if (sortField !== field) return '';
    return sortDir === 'asc' ? ' \u25B2' : ' \u25BC';
  };

  // Summary stats
  const stats = useMemo(() => {
    const prices = filteredData.map(d => d.eod_price).filter((v): v is number => v !== null && v !== 0);
    const rates = filteredData.map(d => d.rate).filter((v): v is number => v !== null && v !== 0);
    return {
      count: filteredData.length,
      avgPrice: prices.length > 0 ? prices.reduce((a, b) => a + b, 0) / prices.length : 0,
      minPrice: prices.length > 0 ? Math.min(...prices) : 0,
      maxPrice: prices.length > 0 ? Math.max(...prices) : 0,
      avgRate: rates.length > 0 ? rates.reduce((a, b) => a + b, 0) / rates.length : 0,
    };
  }, [filteredData]);

  // Chart data: security type breakdown
  const typeBreakdown = useMemo(() => {
    const counts: Record<string, { count: number; avgPrice: number; avgRate: number }> = {};
    filteredData.forEach(r => {
      if (!counts[r.security_type]) counts[r.security_type] = { count: 0, avgPrice: 0, avgRate: 0 };
      counts[r.security_type].count++;
      if (r.eod_price) counts[r.security_type].avgPrice += r.eod_price;
      if (r.rate) counts[r.security_type].avgRate += r.rate;
    });
    return Object.entries(counts).map(([type, v]) => ({
      type,
      count: v.count,
      avgPrice: v.count > 0 ? +(v.avgPrice / v.count).toFixed(2) : 0,
      avgRate: v.count > 0 ? +((v.avgRate / v.count) * 100).toFixed(3) : 0,
      fill: TYPE_COLORS[type] || FINCEPT.GRAY,
    })).sort((a, b) => b.count - a.count);
  }, [filteredData]);

  // Scatter data: EOD price vs maturity
  const scatterData = useMemo(() => {
    return filteredData
      .filter(r => r.eod_price && r.maturity_date)
      .map(r => ({
        maturity: r.maturity_date!,
        maturityTs: new Date(r.maturity_date!).getTime(),
        eodPrice: r.eod_price!,
        cusip: r.cusip,
        type: r.security_type,
        fill: TYPE_COLORS[r.security_type] || FINCEPT.GRAY,
      }));
  }, [filteredData]);

  const thStyle: React.CSSProperties = {
    padding: '8px 10px',
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
    padding: '6px 10px',
    fontSize: '11px',
    color: FINCEPT.WHITE,
    borderBottom: `1px solid ${FINCEPT.BORDER}08`,
    whiteSpace: 'nowrap',
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Filter bar */}
      <div style={{
        padding: '10px 16px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexWrap: 'wrap',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 700 }}>TYPE:</span>
          <select
            value={filterType}
            onChange={e => setFilterType(e.target.value)}
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              outline: 'none',
            }}
          >
            <option value="all">All ({data.total_records})</option>
            {securityTypes.map(t => (
              <option key={t} value={t}>{t} ({data.data.filter(d => d.security_type === t).length})</option>
            ))}
          </select>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 700 }}>CUSIP:</span>
          <input
            type="text"
            value={searchCusip}
            onChange={e => setSearchCusip(e.target.value)}
            placeholder="Search..."
            style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.DARK_BG,
              color: FINCEPT.WHITE,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '10px',
              outline: 'none',
              width: '120px',
            }}
          />
        </div>

        {/* View toggle + Stats */}
        <div style={{ marginLeft: 'auto', display: 'flex', alignItems: 'center', gap: '12px', fontSize: '10px' }}>
          <span><span style={{ color: FINCEPT.GRAY }}>SHOWING:</span> <span style={{ color: FINCEPT.WHITE }}>{filteredData.length}</span></span>
          <span><span style={{ color: FINCEPT.GRAY }}>AVG PRICE:</span> <span style={{ color: '#3B82F6' }}>{stats.avgPrice.toFixed(2)}</span></span>
          <span><span style={{ color: FINCEPT.GRAY }}>RANGE:</span> <span style={{ color: FINCEPT.WHITE }}>{stats.minPrice.toFixed(2)} - {stats.maxPrice.toFixed(2)}</span></span>
          <span><span style={{ color: FINCEPT.GRAY }}>AVG RATE:</span> <span style={{ color: FINCEPT.YELLOW }}>{(stats.avgRate * 100).toFixed(3)}%</span></span>
          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ display: 'flex', gap: '2px' }}>
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
      </div>

      {viewMode === 'chart' ? (
        /* Chart View */
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {/* Security Type Distribution */}
          <div style={{
            marginBottom: '20px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            padding: '16px',
          }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '12px' }}>
              Securities by Type — Count & Avg Price
            </div>
            <ResponsiveContainer width="100%" height={220}>
              <BarChart data={typeBreakdown} margin={{ top: 5, right: 30, left: 0, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
                <XAxis dataKey="type" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} interval={0} angle={-15} textAnchor="end" height={50} />
                <YAxis yAxisId="left" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} />
                <YAxis yAxisId="right" orientation="right" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} />
                <Tooltip contentStyle={tooltipStyle} labelStyle={{ color: FINCEPT.WHITE }} />
                <Legend wrapperStyle={{ fontSize: '9px', color: FINCEPT.GRAY }} />
                <Bar yAxisId="left" dataKey="count" name="Count" radius={[2, 2, 0, 0]}>
                  {typeBreakdown.map((entry, i) => (
                    <Cell key={i} fill={entry.fill} />
                  ))}
                </Bar>
                <Bar yAxisId="right" dataKey="avgPrice" name="Avg Price" fill="#10B981" opacity={0.6} radius={[2, 2, 0, 0]} />
              </BarChart>
            </ResponsiveContainer>
          </div>

          {/* Price Distribution Scatter */}
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            padding: '16px',
          }}>
            <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE, marginBottom: '12px' }}>
              EOD Price by Maturity Date
            </div>
            <ResponsiveContainer width="100%" height={280}>
              <ScatterChart margin={{ top: 5, right: 30, left: 0, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
                <XAxis
                  dataKey="maturityTs"
                  type="number"
                  domain={['dataMin', 'dataMax']}
                  tick={{ fill: FINCEPT.GRAY, fontSize: 9 }}
                  tickFormatter={(v) => new Date(v).toLocaleDateString('en-US', { year: '2-digit', month: 'short' })}
                  name="Maturity"
                />
                <YAxis dataKey="eodPrice" tick={{ fill: FINCEPT.GRAY, fontSize: 9 }} name="EOD Price" />
                <Tooltip
                  contentStyle={tooltipStyle}
                  labelStyle={{ color: FINCEPT.WHITE }}
                  formatter={(value: any, name: string) => {
                    if (name === 'EOD Price') return [Number(value).toFixed(4), name];
                    return [value, name];
                  }}
                  labelFormatter={(v) => `Maturity: ${new Date(v).toLocaleDateString()}`}
                />
                <Scatter data={scatterData} name="Securities">
                  {scatterData.map((entry, i) => (
                    <Cell key={i} fill={entry.fill} opacity={0.7} />
                  ))}
                </Scatter>
              </ScatterChart>
            </ResponsiveContainer>
            {/* Legend */}
            <div style={{ display: 'flex', gap: '12px', marginTop: '8px', flexWrap: 'wrap' }}>
              {Object.entries(TYPE_COLORS).map(([type, color]) => (
                <div key={type} style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
                  <span style={{ width: '8px', height: '8px', borderRadius: '50%', backgroundColor: color, display: 'inline-block' }} />
                  {type}
                </div>
              ))}
            </div>
          </div>
        </div>
      ) : (
        /* Table View */
        <div style={{ flex: 1, overflow: 'auto' }}>
          <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: '"IBM Plex Mono", monospace' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: FINCEPT.HEADER_BG, zIndex: 1 }}>
              <tr>
                <th style={thStyle} onClick={() => handleSort('cusip')}>CUSIP{sortIndicator('cusip')}</th>
                <th style={thStyle} onClick={() => handleSort('security_type')}>TYPE{sortIndicator('security_type')}</th>
                <th style={thStyle} onClick={() => handleSort('rate')}>RATE{sortIndicator('rate')}</th>
                <th style={thStyle} onClick={() => handleSort('maturity_date')}>MATURITY{sortIndicator('maturity_date')}</th>
                <th style={thStyle} onClick={() => handleSort('bid')}>BID{sortIndicator('bid')}</th>
                <th style={thStyle} onClick={() => handleSort('offer')}>OFFER{sortIndicator('offer')}</th>
                <th style={thStyle} onClick={() => handleSort('eod_price')}>EOD PRICE{sortIndicator('eod_price')}</th>
                <th style={thStyle}>SPREAD</th>
              </tr>
            </thead>
            <tbody>
              {filteredData.map((row, i) => {
                const spread = row.bid && row.offer ? (row.offer - row.bid) : null;
                return (
                  <tr
                    key={`${row.cusip}-${i}`}
                    style={{ backgroundColor: i % 2 === 0 ? 'transparent' : `${FINCEPT.PANEL_BG}80` }}
                    onMouseEnter={e => (e.currentTarget.style.backgroundColor = FINCEPT.HOVER)}
                    onMouseLeave={e => (e.currentTarget.style.backgroundColor = i % 2 === 0 ? 'transparent' : `${FINCEPT.PANEL_BG}80`)}
                  >
                    <td style={{ ...tdStyle, color: '#3B82F6', fontWeight: 600 }}>{row.cusip}</td>
                    <td style={{ ...tdStyle, fontSize: '9px' }}>{row.security_type}</td>
                    <td style={{ ...tdStyle, color: FINCEPT.YELLOW }}>{formatRate(row.rate)}</td>
                    <td style={tdStyle}>{formatDate(row.maturity_date)}</td>
                    <td style={tdStyle}>{formatPrice(row.bid)}</td>
                    <td style={tdStyle}>{formatPrice(row.offer)}</td>
                    <td style={{ ...tdStyle, fontWeight: 600, color: FINCEPT.GREEN }}>{formatPrice(row.eod_price)}</td>
                    <td style={{ ...tdStyle, color: spread !== null && spread > 0 ? FINCEPT.ORANGE : FINCEPT.GRAY }}>
                      {spread !== null ? spread.toFixed(6) : '-'}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

export default USTreasuryPricesPanel;
