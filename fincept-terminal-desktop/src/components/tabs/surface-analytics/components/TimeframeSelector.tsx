/**
 * Surface Analytics - Timeframe Selector Component
 * Select OHLCV timeframes (1D, 1H, 1M, 1S)
 */

import React from 'react';
import { Clock, AlertTriangle, TrendingUp } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import type { OHLCVTimeframe, TimeframeInfo } from '../types';

interface TimeframeSelectorProps {
  value: OHLCVTimeframe;
  onChange: (timeframe: OHLCVTimeframe) => void;
  accentColor: string;
  textColor: string;
  disabled?: boolean;
}

// Timeframe definitions with metadata
const TIMEFRAMES: TimeframeInfo[] = [
  {
    value: 'ohlcv-1d',
    label: 'Daily',
    description: 'End-of-day OHLCV bars',
    costLevel: 'low',
    recordsPerDay: '1',
  },
  {
    value: 'ohlcv-eod',
    label: 'EOD',
    description: 'End-of-day with adjustments',
    costLevel: 'low',
    recordsPerDay: '1',
  },
  {
    value: 'ohlcv-1h',
    label: 'Hourly',
    description: '60-minute OHLCV bars',
    costLevel: 'low',
    recordsPerDay: '~7',
  },
  {
    value: 'ohlcv-1m',
    label: 'Minute',
    description: '1-minute OHLCV bars',
    costLevel: 'medium',
    recordsPerDay: '~390',
  },
  {
    value: 'ohlcv-1s',
    label: 'Second',
    description: '1-second OHLCV bars',
    costLevel: 'high',
    recordsPerDay: '~23K',
  },
];

export const TimeframeSelector: React.FC<TimeframeSelectorProps> = ({
  value,
  onChange,
  accentColor,
  textColor,
  disabled = false,
}) => {
  const { t } = useTranslation('surfaceAnalytics');
  const { colors } = useTerminalTheme();

  const getCostLevelColor = (level: 'low' | 'medium' | 'high') => {
    switch (level) {
      case 'low': return colors.success;
      case 'medium': return colors.warning;
      case 'high': return colors.alert;
    }
  };

  const getCostLevelLabel = (level: 'low' | 'medium' | 'high') => {
    switch (level) {
      case 'low': return '$';
      case 'medium': return '$$';
      case 'high': return '$$$';
    }
  };

  const selectedTimeframe = TIMEFRAMES.find(tf => tf.value === value) || TIMEFRAMES[0];

  return (
    <div
      style={{
        backgroundColor: colors.background,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: 'var(--ft-border-radius)',
        padding: '12px',
        opacity: disabled ? 0.6 : 1,
      }}
    >
      {/* Header */}
      <div className="flex items-center gap-2 mb-3">
        <Clock size={14} style={{ color: accentColor }} />
        <span className="text-xs font-bold" style={{ color: accentColor, letterSpacing: '0.5px' }}>
          {t('timeframe.dataTimeframe')}
        </span>
      </div>

      {/* Timeframe Buttons */}
      <div className="grid grid-cols-5 gap-1 mb-3">
        {TIMEFRAMES.map(tf => {
          const isSelected = value === tf.value;
          return (
            <button
              key={tf.value}
              onClick={() => !disabled && onChange(tf.value)}
              disabled={disabled}
              className="flex flex-col items-center p-2 transition-all"
              style={{
                backgroundColor: isSelected ? accentColor : colors.background,
                border: `1px solid ${isSelected ? accentColor : colors.textMuted}`,
                borderRadius: 'var(--ft-border-radius)',
                cursor: disabled ? 'not-allowed' : 'pointer',
              }}
            >
              <span
                className="text-xs font-bold"
                style={{ color: isSelected ? colors.background : textColor }}
              >
                {tf.label}
              </span>
              <span
                className="text-[9px] mt-0.5"
                style={{ color: isSelected ? colors.background : getCostLevelColor(tf.costLevel) }}
              >
                {getCostLevelLabel(tf.costLevel)}
              </span>
            </button>
          );
        })}
      </div>

      {/* Selected Timeframe Details */}
      <div
        className="p-2 text-xs"
        style={{
          backgroundColor: colors.background,
          border: `1px solid ${colors.textMuted}`,
          borderRadius: 'var(--ft-border-radius)',
        }}
      >
        <div className="flex items-center justify-between mb-1">
          <span style={{ color: colors.textMuted }}>{t('timeframe.schema')}:</span>
          <span className="font-mono" style={{ color: textColor }}>{selectedTimeframe.value}</span>
        </div>
        <div className="flex items-center justify-between mb-1">
          <span style={{ color: colors.textMuted }}>{t('timeframe.description')}:</span>
          <span style={{ color: textColor }}>{selectedTimeframe.description}</span>
        </div>
        <div className="flex items-center justify-between">
          <span style={{ color: colors.textMuted }}>{t('timeframe.recordsPerDay')}:</span>
          <span style={{ color: textColor }}>{selectedTimeframe.recordsPerDay}</span>
        </div>
      </div>

      {/* Warning for high-frequency data */}
      {selectedTimeframe.costLevel === 'high' && (
        <div
          className="flex items-center gap-2 mt-2 p-2 text-xs"
          style={{
            backgroundColor: `${colors.warning}15`,
            border: `1px solid ${colors.warning}50`,
            borderRadius: 'var(--ft-border-radius)',
            color: colors.warning,
          }}
        >
          <AlertTriangle size={12} />
          <span>{t('timeframe.highFrequencyWarning')}</span>
        </div>
      )}

      {/* Data Volume Indicator */}
      <div className="mt-3">
        <div className="flex items-center justify-between mb-1">
          <span className="text-xs" style={{ color: colors.textMuted }}>
            {t('timeframe.dataVolume')}
          </span>
          <div className="flex items-center gap-1">
            <TrendingUp size={10} style={{ color: getCostLevelColor(selectedTimeframe.costLevel) }} />
            <span className="text-xs" style={{ color: getCostLevelColor(selectedTimeframe.costLevel) }}>
              {selectedTimeframe.costLevel.toUpperCase()}
            </span>
          </div>
        </div>
        <div
          className="h-1 rounded"
          style={{ backgroundColor: colors.textMuted }}
        >
          <div
            className="h-full rounded transition-all"
            style={{
              width: selectedTimeframe.costLevel === 'low' ? '20%' :
                     selectedTimeframe.costLevel === 'medium' ? '60%' : '100%',
              backgroundColor: getCostLevelColor(selectedTimeframe.costLevel),
            }}
          />
        </div>
      </div>
    </div>
  );
};
