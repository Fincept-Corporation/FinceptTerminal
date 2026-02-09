/**
 * Surface Analytics - Control Bar Component
 * Top navigation with chart type selection and controls
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
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import type { ChartType } from '../types';

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
}) => {
  const { t } = useTranslation('surfaceAnalytics');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  const chartButtons = [
    { id: 'volatility' as ChartType, icon: TrendingUp, label: t('charts.volatility') },
    { id: 'correlation' as ChartType, icon: Network, label: t('charts.correlation') },
    { id: 'yield-curve' as ChartType, icon: Activity, label: t('charts.yieldCurve') },
    { id: 'pca' as ChartType, icon: BarChart3, label: t('charts.pca') },
  ];

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 16px',
        backgroundColor: colors.panel,
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
            fontSize: fontSize.body,
            fontWeight: 700,
            color: accentColor,
            letterSpacing: '0.5px',
            fontFamily,
          }}>
            {t('title')}
          </span>
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: colors.textMuted }} />

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
                color: activeChart === id ? colors.background : colors.textMuted,
                border: 'none',
                borderRadius: 'var(--ft-border-radius)',
                fontSize: fontSize.tiny,
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily,
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
            backgroundColor: `${colors.alert}20`,
            color: colors.alert,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            borderRadius: 'var(--ft-border-radius)',
          }}>
            <AlertCircle size={10} />
            <span>{t('noApiKey')}</span>
          </div>
        )}

        {/* Last Update */}
        <span style={{
          fontSize: fontSize.tiny,
          color: colors.textMuted,
          fontFamily,
          letterSpacing: '0.5px',
        }}>
          {t('lastUpdate')}:{' '}
          <span style={{ color: colors.info }}>
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
            border: `1px solid ${colors.textMuted}`,
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            borderRadius: 'var(--ft-border-radius)',
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
            color: colors.background,
            border: 'none',
            borderRadius: 'var(--ft-border-radius)',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            cursor: loading ? 'not-allowed' : 'pointer',
            opacity: loading ? 0.7 : 1,
            fontFamily,
          }}
        >
          <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
          {loading ? t('common:loading') : t('refresh')}
        </button>
      </div>
    </div>
  );
};
