import React from 'react';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

interface MAEmptyStateProps {
  icon: React.ReactNode;
  title: string;
  description?: string;
  actionLabel?: string;
  onAction?: () => void;
  accentColor?: string;
}

export const MAEmptyState: React.FC<MAEmptyStateProps> = ({
  icon,
  title,
  description,
  actionLabel,
  onAction,
  accentColor = FINCEPT.ORANGE,
}) => (
  <div style={{
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    justifyContent: 'center',
    height: '100%',
    minHeight: '200px',
    gap: '12px',
    padding: '40px 20px',
  }}>
    <div style={{
      color: FINCEPT.BORDER,
      opacity: 0.6,
      animation: 'pulse 3s ease-in-out infinite',
    }}>
      {icon}
    </div>
    <div style={{
      fontSize: '11px',
      fontFamily: TYPOGRAPHY.MONO,
      fontWeight: TYPOGRAPHY.BOLD,
      color: FINCEPT.GRAY,
      letterSpacing: TYPOGRAPHY.WIDE,
      textTransform: 'uppercase' as const,
    }}>
      {title}
    </div>
    {description && (
      <div style={{
        fontSize: '10px',
        fontFamily: TYPOGRAPHY.MONO,
        color: FINCEPT.MUTED,
        textAlign: 'center',
        maxWidth: '300px',
        lineHeight: 1.5,
      }}>
        {description}
      </div>
    )}
    {actionLabel && onAction && (
      <button
        onClick={onAction}
        style={{
          marginTop: '8px',
          padding: '8px 16px',
          backgroundColor: 'transparent',
          border: `1px solid ${accentColor}`,
          color: accentColor,
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          letterSpacing: TYPOGRAPHY.WIDE,
          cursor: 'pointer',
          textTransform: 'uppercase' as const,
          transition: 'all 0.15s',
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = accentColor + '20';
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = 'transparent';
        }}
      >
        {actionLabel}
      </button>
    )}
  </div>
);
