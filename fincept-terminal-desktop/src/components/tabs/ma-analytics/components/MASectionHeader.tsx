import React from 'react';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

interface MASectionHeaderProps {
  title: string;
  subtitle?: string;
  icon?: React.ReactNode;
  action?: React.ReactNode;
  accentColor?: string;
}

export const MASectionHeader: React.FC<MASectionHeaderProps> = ({
  title,
  subtitle,
  icon,
  action,
  accentColor = FINCEPT.ORANGE,
}) => (
  <div style={{
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
    padding: '10px 0',
    borderBottom: `1px solid ${FINCEPT.BORDER}`,
    marginBottom: '12px',
  }}>
    {icon && <span style={{ color: accentColor, display: 'flex' }}>{icon}</span>}
    <span style={{
      fontSize: '11px',
      fontFamily: TYPOGRAPHY.MONO,
      fontWeight: TYPOGRAPHY.BOLD,
      color: FINCEPT.WHITE,
      letterSpacing: TYPOGRAPHY.WIDE,
      textTransform: 'uppercase' as const,
    }}>
      {title}
    </span>
    {subtitle && (
      <span style={{
        fontSize: '9px',
        fontFamily: TYPOGRAPHY.MONO,
        color: FINCEPT.GRAY,
      }}>
        {subtitle}
      </span>
    )}
    <div style={{ flex: 1 }} />
    {action}
  </div>
);
