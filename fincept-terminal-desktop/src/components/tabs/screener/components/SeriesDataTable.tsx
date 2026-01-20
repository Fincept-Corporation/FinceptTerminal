import React, { memo } from 'react';
import type { FREDSeries } from '../types';

interface SeriesDataTableProps {
  series: FREDSeries;
  colors: any;
  fontSize: any;
}

function SeriesDataTable({ series, colors, fontSize }: SeriesDataTableProps) {
  return (
    <div
      style={{
        background: colors.panel,
        border: `1px solid ${colors.primary}33`,
        borderRadius: '4px',
        padding: '12px',
        flexShrink: 0
      }}
    >
      <div style={{ borderBottom: `1px solid ${colors.textMuted}33`, paddingBottom: '8px', marginBottom: '8px' }}>
        <h4 style={{ color: colors.primary, fontSize: fontSize.body, fontWeight: 'bold', marginBottom: '4px' }}>
          {series.series_id}
        </h4>
        <p style={{ color: colors.text, fontSize: fontSize.tiny, marginBottom: '6px' }}>{series.title}</p>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', fontSize: fontSize.tiny, color: colors.textMuted }}>
          <span>Units: {series.units}</span>
          <span>Freq: {series.frequency}</span>
          <span>Obs: {series.observation_count}</span>
          <span>Updated: {series.last_updated?.split('T')[0]}</span>
        </div>
      </div>

      <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
        <table style={{ width: '100%', fontSize: fontSize.tiny, borderCollapse: 'collapse' }}>
          <thead style={{ position: 'sticky', top: 0, background: colors.panel }}>
            <tr style={{ borderBottom: `1px solid ${colors.textMuted}33` }}>
              <th style={{ color: colors.textMuted, textAlign: 'left', padding: '6px 4px', fontWeight: 'bold' }}>Date</th>
              <th style={{ color: colors.textMuted, textAlign: 'right', padding: '6px 4px', fontWeight: 'bold' }}>Value</th>
            </tr>
          </thead>
          <tbody>
            {series.observations.slice(-20).reverse().map((obs, idx) => (
              <tr key={`${obs.date}-${idx}`} style={{ borderBottom: `1px solid ${colors.background}` }}>
                <td style={{ color: colors.textMuted, padding: '4px' }}>{obs.date}</td>
                <td style={{ color: colors.text, textAlign: 'right', padding: '4px', fontFamily: 'monospace', fontSize: fontSize.tiny }}>
                  {obs.value.toFixed(2)}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

export default memo(SeriesDataTable);
