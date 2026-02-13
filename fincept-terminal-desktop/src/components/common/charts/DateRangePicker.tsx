// DateRangePicker — Reusable preset buttons + custom date inputs

import React from 'react';
import { Calendar } from 'lucide-react';
import { FINCEPT_COLORS } from './types';
import type { DateRangePreset } from './types';

interface DateRangePickerProps {
  dateRangePreset: DateRangePreset;
  setDateRangePreset: (preset: DateRangePreset) => void;
  startDate: string;
  endDate: string;
  setStartDate: (date: string) => void;
  setEndDate: (date: string) => void;
  sourceColor: string;
}

const PRESETS: { value: DateRangePreset; label: string }[] = [
  { value: '1M', label: '1M' },
  { value: '3M', label: '3M' },
  { value: '6M', label: '6M' },
  { value: '1Y', label: '1Y' },
  { value: '2Y', label: '2Y' },
  { value: '5Y', label: '5Y' },
  { value: '10Y', label: '10Y' },
  { value: 'ALL', label: 'ALL' },
];

export function DateRangePicker({
  dateRangePreset,
  setDateRangePreset,
  startDate,
  endDate,
  setStartDate,
  setEndDate,
  sourceColor,
}: DateRangePickerProps) {
  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        padding: '6px 12px',
        backgroundColor: FINCEPT_COLORS.PANEL_BG,
        borderRadius: '2px',
        border: `1px solid ${FINCEPT_COLORS.BORDER}`,
      }}
    >
      <Calendar size={14} color={FINCEPT_COLORS.GRAY} />

      {/* Preset buttons */}
      <div style={{ display: 'flex', gap: '2px' }}>
        {PRESETS.map((preset) => (
          <button
            key={preset.value}
            onClick={() => setDateRangePreset(preset.value)}
            style={{
              padding: '3px 6px',
              backgroundColor: dateRangePreset === preset.value ? sourceColor : 'transparent',
              color: dateRangePreset === preset.value ? FINCEPT_COLORS.DARK_BG : FINCEPT_COLORS.GRAY,
              border: `1px solid ${dateRangePreset === preset.value ? sourceColor : FINCEPT_COLORS.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              transition: 'all 0.15s',
            }}
          >
            {preset.label}
          </button>
        ))}
      </div>

      {/* Separator */}
      <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT_COLORS.BORDER }} />

      {/* Custom date inputs */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
        <input
          type="date"
          value={startDate}
          onChange={(e) => setStartDate(e.target.value)}
          style={{
            padding: '3px 6px',
            backgroundColor: FINCEPT_COLORS.DARK_BG,
            color: FINCEPT_COLORS.WHITE,
            border: `1px solid ${FINCEPT_COLORS.BORDER}`,
            borderRadius: '2px',
            fontSize: '9px',
            fontFamily: 'inherit',
            cursor: 'pointer',
          }}
        />
        <span style={{ color: FINCEPT_COLORS.GRAY, fontSize: '9px' }}>to</span>
        <input
          type="date"
          value={endDate}
          onChange={(e) => setEndDate(e.target.value)}
          style={{
            padding: '3px 6px',
            backgroundColor: FINCEPT_COLORS.DARK_BG,
            color: FINCEPT_COLORS.WHITE,
            border: `1px solid ${FINCEPT_COLORS.BORDER}`,
            borderRadius: '2px',
            fontSize: '9px',
            fontFamily: 'inherit',
            cursor: 'pointer',
          }}
        />
      </div>
    </div>
  );
}

export default DateRangePicker;
