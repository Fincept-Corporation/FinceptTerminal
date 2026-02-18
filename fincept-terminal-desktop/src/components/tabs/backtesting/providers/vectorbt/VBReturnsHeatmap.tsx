/**
 * VBReturnsHeatmap - Monthly returns color matrix
 * Rows = years, Columns = Jan-Dec + YTD
 */

import React, { useMemo } from 'react';
import { FINCEPT, TYPOGRAPHY } from '../../../portfolio-tab/finceptStyles';

interface VBReturnsHeatmapProps {
  equity: any;
}

const MONTHS = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

function getHeatColor(value: number): string {
  if (value === 0) return '#2A2A2A';
  if (value > 0) {
    const intensity = Math.min(value / 10, 1); // 10% = max green
    const r = Math.round(0 + (0 - 0) * intensity);
    const g = Math.round(42 + (214 - 42) * intensity);
    const b = Math.round(42 + (111 - 42) * intensity);
    return `rgb(${r},${g},${b})`;
  }
  // negative
  const intensity = Math.min(Math.abs(value) / 10, 1);
  const r = Math.round(42 + (255 - 42) * intensity);
  const g = Math.round(42 + (59 - 42) * intensity);
  const b = Math.round(42 + (59 - 42) * intensity);
  return `rgb(${r},${g},${b})`;
}

export const VBReturnsHeatmap: React.FC<VBReturnsHeatmapProps> = ({ equity }) => {
  const monthlyData = useMemo(() => {
    if (!equity) return null;

    let points: { date: string; value: number }[] = [];

    if (Array.isArray(equity)) {
      points = equity.map((pt: any) => ({
        date: pt.time || pt.date || '',
        value: pt.value ?? pt.equity ?? 0,
      }));
    } else if (typeof equity === 'object') {
      points = Object.entries(equity).map(([date, value]) => ({
        date,
        value: Number(value),
      }));
    }

    if (points.length === 0) return null;

    points.sort((a, b) => a.date.localeCompare(b.date));

    // Group by year-month, compute monthly returns
    const byMonth: Record<string, Record<number, number>> = {};
    let prevValue: number | null = null;
    let prevMonth: string | null = null;
    let monthStart: number | null = null;

    for (const pt of points) {
      const d = new Date(pt.date);
      const year = d.getFullYear().toString();
      const month = d.getMonth(); // 0-indexed
      const key = `${year}-${month}`;

      if (key !== prevMonth) {
        // Close previous month
        if (prevMonth && monthStart !== null && prevValue !== null) {
          const [pYear] = prevMonth.split('-');
          const pMonth = parseInt(prevMonth.split('-')[1]);
          if (!byMonth[pYear]) byMonth[pYear] = {};
          byMonth[pYear][pMonth] = ((prevValue - monthStart) / monthStart) * 100;
        }
        monthStart = prevValue ?? pt.value;
        prevMonth = key;
      }
      prevValue = pt.value;
    }

    // Close last month
    if (prevMonth && monthStart !== null && prevValue !== null) {
      const [pYear] = prevMonth.split('-');
      const pMonth = parseInt(prevMonth.split('-')[1]);
      if (!byMonth[pYear]) byMonth[pYear] = {};
      byMonth[pYear][pMonth] = ((prevValue - monthStart) / monthStart) * 100;
    }

    return byMonth;
  }, [equity]);

  if (!monthlyData || Object.keys(monthlyData).length === 0) {
    return (
      <div style={{ padding: '24px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '10px' }}>
        No data for monthly returns heatmap
      </div>
    );
  }

  const years = Object.keys(monthlyData).sort();

  return (
    <div style={{ padding: '8px', overflow: 'auto' }}>
      <table style={{ borderCollapse: 'collapse', width: '100%' }}>
        <thead>
          <tr>
            <th style={{
              padding: '4px 6px', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
              textAlign: 'left', borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              YEAR
            </th>
            {MONTHS.map(m => (
              <th key={m} style={{
                padding: '4px 3px', fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE,
                textAlign: 'center', borderBottom: `1px solid ${FINCEPT.BORDER}`,
              }}>
                {m}
              </th>
            ))}
            <th style={{
              padding: '4px 6px', fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE,
              textAlign: 'center', borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}>
              YTD
            </th>
          </tr>
        </thead>
        <tbody>
          {years.map(year => {
            const months = monthlyData[year];
            const ytd = Object.values(months).reduce((sum, v) => sum + v, 0);

            return (
              <tr key={year}>
                <td style={{
                  padding: '3px 6px', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY,
                }}>
                  {year}
                </td>
                {Array.from({ length: 12 }, (_, i) => {
                  const val = months[i];
                  const hasVal = val !== undefined;
                  return (
                    <td key={i} style={{
                      padding: '3px 2px',
                      textAlign: 'center',
                    }}>
                      <div style={{
                        backgroundColor: hasVal ? getHeatColor(val) : 'transparent',
                        color: hasVal ? '#FFFFFF' : FINCEPT.MUTED,
                        fontSize: '9px',
                        fontWeight: 600,
                        fontFamily: TYPOGRAPHY.MONO,
                        padding: '3px 2px',
                        borderRadius: '1px',
                        minWidth: '36px',
                      }}>
                        {hasVal ? `${val >= 0 ? '+' : ''}${val.toFixed(1)}%` : '--'}
                      </div>
                    </td>
                  );
                })}
                <td style={{ padding: '3px 4px', textAlign: 'center' }}>
                  <div style={{
                    backgroundColor: getHeatColor(ytd),
                    color: '#FFFFFF',
                    fontSize: '9px',
                    fontWeight: 700,
                    fontFamily: TYPOGRAPHY.MONO,
                    padding: '3px 4px',
                    borderRadius: '1px',
                    minWidth: '40px',
                  }}>
                    {ytd >= 0 ? '+' : ''}{ytd.toFixed(1)}%
                  </div>
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};
