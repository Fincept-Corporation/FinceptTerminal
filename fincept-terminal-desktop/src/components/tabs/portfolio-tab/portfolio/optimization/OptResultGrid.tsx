import React from 'react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../finceptStyles';

/** Generic result grid to display Python response data */
export const OptResultGrid: React.FC<{ data: any; color: string }> = ({ data, color }) => {
  if (!data || typeof data !== 'object') {
    return <div style={{ color: FINCEPT.MUTED, fontSize: TYPOGRAPHY.SMALL, fontFamily: TYPOGRAPHY.MONO, padding: SPACING.MEDIUM, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, whiteSpace: 'pre-wrap', maxHeight: '250px', overflow: 'auto' }}>
      {typeof data === 'string' ? data : JSON.stringify(data, null, 2)}
    </div>;
  }

  const flat = Object.entries(data).filter(([, v]) => v !== null && v !== undefined && typeof v !== 'object');
  const nested = Object.entries(data).filter(([, v]) => typeof v === 'object' && v !== null && !Array.isArray(v));

  return (
    <div>
      {flat.length > 0 && (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))', gap: SPACING.MEDIUM, marginBottom: SPACING.MEDIUM }}>
          {flat.map(([k, v]) => (
            <div key={k} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${color}30` }}>
              <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{k.replace(/_/g, ' ').toUpperCase()}</div>
              <div style={{ color, fontSize: '12px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                {typeof v === 'number' ? (Math.abs(v as number) < 1 ? (v as number).toFixed(4) : (v as number).toFixed(2)) : String(v)}
              </div>
            </div>
          ))}
        </div>
      )}
      {nested.map(([sectionKey, sectionData]) => (
        <div key={sectionKey} style={{ marginBottom: SPACING.MEDIUM }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.3px', marginBottom: SPACING.SMALL }}>
            {sectionKey.replace(/_/g, ' ').toUpperCase()}
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))', gap: SPACING.MEDIUM }}>
            {Object.entries(sectionData as Record<string, any>).filter(([, v]) => v !== null && typeof v !== 'object').map(([k, v]) => (
              <div key={k} style={{ ...COMMON_STYLES.metricCard, padding: SPACING.MEDIUM, borderRadius: '2px', border: `1px solid ${color}20` }}>
                <div style={{ ...COMMON_STYLES.dataLabel, fontSize: '8px' }}>{k.replace(/_/g, ' ').toUpperCase()}</div>
                <div style={{ color, fontSize: '11px', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                  {typeof v === 'number' ? (Math.abs(v as number) < 1 ? (v as number).toFixed(4) : (v as number).toFixed(2)) : String(v)}
                </div>
              </div>
            ))}
          </div>
        </div>
      ))}
      {flat.length === 0 && nested.length === 0 && (
        <div style={{ color: FINCEPT.MUTED, fontSize: TYPOGRAPHY.SMALL, fontFamily: TYPOGRAPHY.MONO, padding: SPACING.MEDIUM, backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, whiteSpace: 'pre-wrap', maxHeight: '250px', overflow: 'auto' }}>
          {JSON.stringify(data, null, 2)}
        </div>
      )}
    </div>
  );
};
