import React, { useState, useEffect } from 'react';
import { Activity } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

interface MAStatusBarProps {
  activeModule: string;
  moduleColor?: string;
  statusItems?: { label: string; value: string; color?: string }[];
}

export const MAStatusBar: React.FC<MAStatusBarProps> = ({
  activeModule,
  moduleColor = FINCEPT.ORANGE,
  statusItems = [],
}) => {
  const [time, setTime] = useState(new Date());

  useEffect(() => {
    const interval = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div style={{
      height: '28px',
      display: 'flex',
      alignItems: 'center',
      gap: '16px',
      padding: '0 12px',
      backgroundColor: FINCEPT.HEADER_BG,
      borderTop: `1px solid ${FINCEPT.BORDER}`,
      fontSize: '9px',
      fontFamily: TYPOGRAPHY.MONO,
      color: FINCEPT.GRAY,
      flexShrink: 0,
    }}>
      {/* Active module */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
        <Activity size={10} color={moduleColor} />
        <span style={{ color: moduleColor, fontWeight: TYPOGRAPHY.BOLD }}>
          ACTIVE:
        </span>
        <span style={{ color: FINCEPT.WHITE }}>{activeModule}</span>
      </div>

      {/* Divider */}
      <div style={{ width: '1px', height: '14px', backgroundColor: FINCEPT.BORDER }} />

      {/* Status items */}
      {statusItems.map((item, idx) => (
        <React.Fragment key={idx}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <span style={{ color: FINCEPT.MUTED }}>{item.label}:</span>
            <span style={{ color: item.color || FINCEPT.WHITE }}>{item.value}</span>
          </div>
          {idx < statusItems.length - 1 && (
            <div style={{ width: '1px', height: '14px', backgroundColor: FINCEPT.BORDER }} />
          )}
        </React.Fragment>
      ))}

      <div style={{ flex: 1 }} />

      {/* Version + Clock */}
      <span style={{ color: FINCEPT.MUTED }}>FINCEPT TERMINAL v3.3</span>
      <div style={{ width: '1px', height: '14px', backgroundColor: FINCEPT.BORDER }} />
      <span style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>
        {time.toLocaleTimeString('en-US', { hour12: false })}
      </span>
    </div>
  );
};
