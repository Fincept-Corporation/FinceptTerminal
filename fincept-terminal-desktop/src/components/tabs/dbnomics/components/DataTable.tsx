import React, { memo, useMemo } from 'react';
import type { DataPoint } from '../types';

interface DataTableProps {
  seriesArray: DataPoint[];
  colors: any;
}

function DataTable({ seriesArray, colors }: DataTableProps) {
  // Memoize unique periods across all series
  const allPeriods = useMemo(() => {
    return Array.from(
      new Set(seriesArray.flatMap(s => s.observations.map(o => o.period)))
    ).sort((a, b) => new Date(b).getTime() - new Date(a).getTime());
  }, [seriesArray]);

  // Memoize table rows (limited to 50)
  const tableRows = useMemo(() => {
    return allPeriods.slice(0, 50).map(period => {
      const values = seriesArray.map(series => {
        const obs = series.observations.find(o => o.period === period);
        return obs ? (typeof obs.value === 'number' ? obs.value : Number(obs.value)) : null;
      });
      return { period, values };
    });
  }, [allPeriods, seriesArray]);

  const containerStyle = useMemo(() => ({
    overflowX: 'auto' as const,
    overflowY: 'auto' as const,
    maxHeight: '300px',
    border: `1px solid ${colors.textMuted}`,
    backgroundColor: colors.panel,
  }), [colors.textMuted, colors.panel]);

  return (
    <div style={{ marginTop: '24px' }}>
      <div style={{ color: colors.primary, fontSize: '11px', marginBottom: '8px' }}>DATA TABLE</div>
      <div style={containerStyle}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
          <thead style={{ position: 'sticky', top: 0, backgroundColor: '#1a1a1a', zIndex: 1 }}>
            <tr style={{ borderBottom: `2px solid ${colors.textMuted}` }}>
              <th style={{
                padding: '8px 12px',
                textAlign: 'left',
                color: colors.primary,
                fontWeight: 'bold',
                whiteSpace: 'nowrap',
              }}>
                Period
              </th>
              {seriesArray.map((series, idx) => (
                <th
                  key={idx}
                  style={{
                    padding: '8px 12px',
                    textAlign: 'right',
                    fontWeight: 'bold',
                    whiteSpace: 'nowrap',
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '6px' }}>
                    <div style={{
                      width: '8px',
                      height: '8px',
                      backgroundColor: series.color,
                      borderRadius: '50%',
                    }} />
                    <span style={{ color: series.color }}>{series.series_name.substring(0, 25)}</span>
                  </div>
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {tableRows.map((row, idx) => (
              <tr key={idx} style={{ borderBottom: '1px solid #1a1a1a' }}>
                <td style={{
                  padding: '6px 12px',
                  color: colors.text,
                  whiteSpace: 'nowrap',
                  fontWeight: 'bold',
                }}>
                  {row.period}
                </td>
                {row.values.map((value, sIdx) => (
                  <td
                    key={sIdx}
                    style={{
                      padding: '6px 12px',
                      textAlign: 'right',
                      color: value !== null ? '#a3a3a3' : colors.textMuted,
                      whiteSpace: 'nowrap',
                    }}
                  >
                    {value !== null && !isNaN(value) ? value.toFixed(4) : '—'}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <div style={{ color: colors.textMuted, fontSize: '9px', marginTop: '6px', textAlign: 'right' }}>
        Showing latest 50 periods
      </div>
    </div>
  );
}

export default memo(DataTable);
