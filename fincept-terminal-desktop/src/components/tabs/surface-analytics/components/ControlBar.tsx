/**
 * Surface Analytics - Control Bar Component
 * Top navigation with chart type selection and controls
 * Follows UI Design System (UI_DESIGN_SYSTEM.md)
 */

import React from 'react';
import {
  RefreshCw,
  TrendingUp,
  Network,
  Activity,
  BarChart3,
  Settings,
  AlertCircle,
} from 'lucide-react';
import type { ChartType } from '../types';
import { FINCEPT_COLORS, TYPOGRAPHY } from '../constants';

interface ControlBarProps {
  activeChart: ChartType;
  onChartChange: (chart: ChartType) => void;
  onRefresh: () => void;
  onSettings: () => void;
  lastUpdate: Date;
  loading: boolean;
  hasApiKey: boolean;
  accentColor: string;
  textColor: string;
}

export const ControlBar: React.FC<ControlBarProps> = ({
  activeChart,
  onChartChange,
  onRefresh,
  onSettings,
  lastUpdate,
  loading,
  hasApiKey,
  accentColor,
  textColor,
}) => {
  const chartButtons = [
    { id: 'volatility' as ChartType, icon: TrendingUp, label: 'VOL SURFACE' },
    { id: 'correlation' as ChartType, icon: Network, label: 'CORRELATION' },
    { id: 'yield-curve' as ChartType, icon: Activity, label: 'YIELD CURVE' },
    { id: 'pca' as ChartType, icon: BarChart3, label: 'PCA FACTORS' },
  ];

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 16px',
        backgroundColor: FINCEPT_COLORS.HEADER_BG,
        borderBottom: `2px solid ${accentColor}`,
        boxShadow: `0 2px 8px ${accentColor}20`,
        flexShrink: 0,
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        {/* Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <BarChart3 size={14} style={{ color: accentColor }} />
          <span style={{
            fontSize: TYPOGRAPHY.HEADER_SIZE,
            fontWeight: 700,
            color: accentColor,
            letterSpacing: '0.5px',
            fontFamily: TYPOGRAPHY.FONT_FAMILY,
          }}>
            SURFACE ANALYTICS
          </span>
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT_COLORS.BORDER }} />

        {/* Chart Type Buttons - Tab style */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {chartButtons.map(({ id, icon: Icon, label }) => (
            <button
              key={id}
              onClick={() => onChartChange(id)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                padding: '6px 12px',
                backgroundColor: activeChart === id ? accentColor : 'transparent',
                color: activeChart === id ? FINCEPT_COLORS.BLACK : FINCEPT_COLORS.GRAY,
                border: 'none',
                borderRadius: '2px',
                fontSize: TYPOGRAPHY.LABEL_SIZE,
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: TYPOGRAPHY.FONT_FAMILY,
                transition: 'all 0.2s',
              }}
            >
              <Icon size={12} />
              {label}
            </button>
          ))}
        </div>
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        {/* API Key Warning */}
        {!hasApiKey && (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            padding: '2px 6px',
            backgroundColor: `${FINCEPT_COLORS.RED}20`,
            color: FINCEPT_COLORS.RED,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
          }}>
            <AlertCircle size={10} />
            <span>NO API KEY</span>
          </div>
        )}

        {/* Last Update */}
        <span style={{
          fontSize: TYPOGRAPHY.LABEL_SIZE,
          color: FINCEPT_COLORS.GRAY,
          fontFamily: TYPOGRAPHY.FONT_FAMILY,
          letterSpacing: '0.5px',
        }}>
          LAST UPDATE:{' '}
          <span style={{ color: FINCEPT_COLORS.CYAN }}>
            {lastUpdate.toLocaleTimeString()}
          </span>
        </span>

        {/* Settings Button - Secondary/Outline style */}
        <button
          onClick={onSettings}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            padding: '6px 10px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT_COLORS.BORDER}`,
            color: FINCEPT_COLORS.GRAY,
            fontSize: TYPOGRAPHY.LABEL_SIZE,
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            transition: 'all 0.2s',
          }}
        >
          <Settings size={12} />
        </button>

        {/* Refresh Button - Primary style */}
        <button
          onClick={onRefresh}
          disabled={loading}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '8px 16px',
            backgroundColor: accentColor,
            color: FINCEPT_COLORS.BLACK,
            border: 'none',
            borderRadius: '2px',
            fontSize: TYPOGRAPHY.LABEL_SIZE,
            fontWeight: 700,
            cursor: loading ? 'not-allowed' : 'pointer',
            opacity: loading ? 0.7 : 1,
            fontFamily: TYPOGRAPHY.FONT_FAMILY,
          }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
          {loading ? 'LOADING' : 'REFRESH'}
        </button>
      </div>
    </div>
  );
};
