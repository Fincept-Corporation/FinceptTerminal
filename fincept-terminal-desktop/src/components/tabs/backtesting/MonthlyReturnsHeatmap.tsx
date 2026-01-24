/**
 * MonthlyReturnsHeatmap - Color-coded grid of monthly returns (year x month)
 */

import React from 'react';
import { F, MonthlyReturnsData, formatPercent } from './backtestingStyles';

interface MonthlyReturnsHeatmapProps {
  data: MonthlyReturnsData[];
}

const MONTHS = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

function getCellColor(value: number | null): string {
  if (value === null) return 'transparent';
  if (value > 0.05) return '#00D66F';
  if (value > 0.02) return '#00A858';
  if (value > 0) return '#2D5A3E';
  if (value > -0.02) return '#5A2D2D';
  if (value > -0.05) return '#A83030';
  return '#FF3B3B';
}

function getCellTextColor(value: number | null): string {
  if (value === null) return F.MUTED;
  if (Math.abs(value) > 0.02) return F.WHITE;
  return F.GRAY;
}

const MonthlyReturnsHeatmap: React.FC<MonthlyReturnsHeatmapProps> = ({ data }) => {
  if (!data || data.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        No monthly returns data available
      </div>
    );
  }

  const cellBase: React.CSSProperties = {
    padding: '4px 2px',
    textAlign: 'center',
    fontSize: '9px',
    fontFamily: '"IBM Plex Mono", monospace',
    borderRadius: '1px',
    minWidth: '38px',
  };

  return (
    <div style={{ padding: '8px', overflowX: 'auto', overflowY: 'auto', height: '100%' }}>
      <table style={{ borderCollapse: 'separate', borderSpacing: '2px', width: '100%' }}>
        <thead>
          <tr>
            <th style={{ ...cellBase, color: F.GRAY, fontWeight: 700 }}>YEAR</th>
            {MONTHS.map(m => (
              <th key={m} style={{ ...cellBase, color: F.GRAY, fontWeight: 700 }}>{m}</th>
            ))}
            <th style={{ ...cellBase, color: F.GRAY, fontWeight: 700 }}>TOTAL</th>
          </tr>
        </thead>
        <tbody>
          {data.map(row => (
            <tr key={row.year}>
              <td style={{ ...cellBase, color: F.WHITE, fontWeight: 700 }}>{row.year}</td>
              {row.months.map((val, i) => (
                <td key={i} style={{
                  ...cellBase,
                  backgroundColor: getCellColor(val),
                  color: getCellTextColor(val),
                }}>
                  {val !== null ? (val * 100).toFixed(1) : '-'}
                </td>
              ))}
              <td style={{
                ...cellBase,
                backgroundColor: getCellColor(row.yearTotal),
                color: getCellTextColor(row.yearTotal),
                fontWeight: 700,
              }}>
                {(row.yearTotal * 100).toFixed(1)}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};

export default MonthlyReturnsHeatmap;
