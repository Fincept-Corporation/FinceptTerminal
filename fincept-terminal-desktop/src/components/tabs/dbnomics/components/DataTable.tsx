import React, { memo, useMemo } from 'react';
import type { DataPoint } from '../types';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface DataTableProps {
  seriesArray: DataPoint[];
}

function DataTable({ seriesArray }: DataTableProps) {
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

  return (
    <div style={{ marginTop: '12px' }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px 2px 0 0',
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
          DATA TABLE
        </span>
      </div>

      <div style={{
        overflowX: 'auto',
        overflowY: 'auto',
        maxHeight: '300px',
        border: `1px solid ${FINCEPT.BORDER}`,
        borderTop: 'none',
        borderRadius: '0 0 2px 2px',
        backgroundColor: FINCEPT.PANEL_BG,
      }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px', fontFamily: FONT_FAMILY }}>
          <thead style={{ position: 'sticky', top: 0, backgroundColor: FINCEPT.HEADER_BG, zIndex: 1 }}>
            <tr style={{ borderBottom: `2px solid ${FINCEPT.BORDER}` }}>
              <th style={{
                padding: '8px 12px',
                textAlign: 'left',
                color: FINCEPT.ORANGE,
                fontWeight: 700,
                whiteSpace: 'nowrap',
                fontSize: '9px',
                letterSpacing: '0.5px',
              }}>
                PERIOD
              </th>
              {seriesArray.map((series, idx) => (
                <th
                  key={idx}
                  style={{
                    padding: '8px 12px',
                    textAlign: 'right',
                    fontWeight: 700,
                    whiteSpace: 'nowrap',
                    fontSize: '9px',
                    letterSpacing: '0.5px',
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
              <tr
                key={idx}
                style={{
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <td style={{
                  padding: '6px 12px',
                  color: FINCEPT.WHITE,
                  whiteSpace: 'nowrap',
                  fontWeight: 700,
                  fontSize: '10px',
                }}>
                  {row.period}
                </td>
                {row.values.map((value, sIdx) => (
                  <td
                    key={sIdx}
                    style={{
                      padding: '6px 12px',
                      textAlign: 'right',
                      color: value !== null ? FINCEPT.CYAN : FINCEPT.MUTED,
                      whiteSpace: 'nowrap',
                      fontSize: '10px',
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
      <div style={{
        color: FINCEPT.GRAY,
        fontSize: '9px',
        marginTop: '6px',
        textAlign: 'right',
        fontFamily: FONT_FAMILY,
        letterSpacing: '0.5px',
      }}>
        SHOWING LATEST 50 PERIODS
      </div>
    </div>
  );
}

export default memo(DataTable);
