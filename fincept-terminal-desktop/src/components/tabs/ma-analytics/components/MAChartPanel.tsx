import React, { useState } from 'react';
import { ChevronDown, ChevronUp, Maximize2, Minimize2 } from 'lucide-react';
import { ResponsiveContainer } from 'recharts';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';

interface MAChartPanelProps {
  title: string;
  children: React.ReactNode;
  height?: number;
  collapsible?: boolean;
  icon?: React.ReactNode;
  accentColor?: string;
  actions?: React.ReactNode;
}

export const MAChartPanel: React.FC<MAChartPanelProps> = ({
  title,
  children,
  height = 280,
  collapsible = true,
  icon,
  accentColor = FINCEPT.ORANGE,
  actions,
}) => {
  const [collapsed, setCollapsed] = useState(false);

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderRadius: '2px',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div
        onClick={collapsible ? () => setCollapsed(!collapsed) : undefined}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          borderBottom: collapsed ? 'none' : `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.HEADER_BG,
          cursor: collapsible ? 'pointer' : 'default',
          userSelect: 'none' as const,
        }}
      >
        {icon && <span style={{ color: accentColor, display: 'flex' }}>{icon}</span>}
        <span style={{
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO,
          fontWeight: TYPOGRAPHY.BOLD,
          color: accentColor,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase' as const,
        }}>
          {title}
        </span>
        <div style={{ flex: 1 }} />
        {actions}
        {collapsible && (
          collapsed
            ? <ChevronDown size={12} color={FINCEPT.GRAY} />
            : <ChevronUp size={12} color={FINCEPT.GRAY} />
        )}
      </div>

      {/* Chart Content */}
      {!collapsed && (
        <div style={{ padding: '12px', height }}>
          <ResponsiveContainer width="100%" height="100%">
            {children as React.ReactElement}
          </ResponsiveContainer>
        </div>
      )}
    </div>
  );
};
