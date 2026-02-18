import React from 'react';
import { TrendingUp, TrendingDown, Minus } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING } from '../../portfolio-tab/finceptStyles';

interface MAMetricCardProps {
  label: string;
  value: string | number;
  accentColor?: string;
  trend?: 'up' | 'down' | 'neutral';
  trendValue?: string;
  subtitle?: string;
  format?: 'currency' | 'percentage' | 'multiple' | 'number' | 'text';
  compact?: boolean;
}

export const MAMetricCard: React.FC<MAMetricCardProps> = ({
  label,
  value,
  accentColor = FINCEPT.ORANGE,
  trend,
  trendValue,
  subtitle,
  compact = false,
}) => {
  const trendColor = trend === 'up' ? FINCEPT.GREEN : trend === 'down' ? FINCEPT.RED : FINCEPT.GRAY;
  const TrendIcon = trend === 'up' ? TrendingUp : trend === 'down' ? TrendingDown : Minus;

  return (
    <div style={{
      padding: compact ? '8px 10px' : '10px 14px',
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderLeft: `3px solid ${accentColor}`,
      borderRadius: '2px',
      minWidth: 0,
    }}>
      <div style={{
        fontSize: TYPOGRAPHY.TINY,
        fontFamily: TYPOGRAPHY.MONO,
        fontWeight: TYPOGRAPHY.BOLD,
        color: FINCEPT.MUTED,
        letterSpacing: TYPOGRAPHY.WIDE,
        textTransform: 'uppercase' as const,
        marginBottom: compact ? '3px' : '6px',
        whiteSpace: 'nowrap' as const,
        overflow: 'hidden',
        textOverflow: 'ellipsis',
      }}>
        {label}
      </div>
      <div style={{ display: 'flex', alignItems: 'baseline', gap: '6px' }}>
        <span style={{
          fontSize: compact ? '13px' : '16px',
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: accentColor,
          lineHeight: 1,
        }}>
          {value}
        </span>
        {trend && (
          <span style={{
            display: 'flex',
            alignItems: 'center',
            gap: '2px',
            fontSize: '9px',
            fontFamily: TYPOGRAPHY.MONO,
            color: trendColor,
          }}>
            <TrendIcon size={10} />
            {trendValue}
          </span>
        )}
      </div>
      {subtitle && (
        <div style={{
          fontSize: '8px',
          fontFamily: TYPOGRAPHY.MONO,
          color: FINCEPT.GRAY,
          marginTop: '4px',
        }}>
          {subtitle}
        </div>
      )}
    </div>
  );
};
