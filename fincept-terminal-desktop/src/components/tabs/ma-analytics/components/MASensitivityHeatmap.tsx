import React from 'react';
import { FINCEPT, TYPOGRAPHY } from '../../portfolio-tab/finceptStyles';
import { HEATMAP_SCALE } from '../constants';

interface MASensitivityHeatmapProps {
  /** 2D array of values [row][col] */
  data: number[][];
  rowLabels: string[];
  colLabels: string[];
  rowHeader: string;
  colHeader: string;
  /** Format function for cell values */
  formatValue?: (val: number) => string;
  /** Highlight row/col index for the base case */
  highlightRow?: number;
  highlightCol?: number;
  accentColor?: string;
}

export const MASensitivityHeatmap: React.FC<MASensitivityHeatmapProps> = ({
  data,
  rowLabels,
  colLabels,
  rowHeader,
  colHeader,
  formatValue = (v) => `$${v.toFixed(1)}`,
  highlightRow,
  highlightCol,
  accentColor = FINCEPT.ORANGE,
}) => {
  // Find min/max for color scaling
  const allValues = data.flat();
  const min = Math.min(...allValues);
  const max = Math.max(...allValues);
  const range = max - min || 1;

  const getColor = (val: number): string => {
    const normalized = (val - min) / range;
    const idx = Math.min(Math.floor(normalized * (HEATMAP_SCALE.length - 1)), HEATMAP_SCALE.length - 1);
    return HEATMAP_SCALE[idx];
  };

  const cellSize = '52px';
  const headerCellSize = '60px';

  return (
    <div style={{ overflow: 'auto' }}>
      {/* Column header label */}
      <div style={{
        textAlign: 'center',
        fontSize: '9px',
        fontFamily: TYPOGRAPHY.MONO,
        fontWeight: TYPOGRAPHY.BOLD,
        color: accentColor,
        letterSpacing: TYPOGRAPHY.WIDE,
        textTransform: 'uppercase' as const,
        marginBottom: '6px',
      }}>
        {colHeader}
      </div>

      <div style={{ display: 'inline-block' }}>
        {/* Column labels row */}
        <div style={{ display: 'flex' }}>
          {/* Corner cell with row header */}
          <div style={{
            width: headerCellSize,
            height: '28px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '8px',
            fontFamily: TYPOGRAPHY.MONO,
            fontWeight: TYPOGRAPHY.BOLD,
            color: accentColor,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase' as const,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            borderRight: `1px solid ${FINCEPT.BORDER}`,
          }}>
            {rowHeader}
          </div>
          {colLabels.map((label, ci) => (
            <div
              key={ci}
              style={{
                width: cellSize,
                height: '28px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontSize: '8px',
                fontFamily: TYPOGRAPHY.MONO,
                fontWeight: TYPOGRAPHY.BOLD,
                color: highlightCol === ci ? accentColor : FINCEPT.GRAY,
                backgroundColor: highlightCol === ci ? `${accentColor}15` : 'transparent',
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                borderRight: `1px solid ${FINCEPT.BORDER}`,
              }}
            >
              {label}
            </div>
          ))}
        </div>

        {/* Data rows */}
        {data.map((row, ri) => (
          <div key={ri} style={{ display: 'flex' }}>
            {/* Row label */}
            <div style={{
              width: headerCellSize,
              height: '32px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: '8px',
              fontFamily: TYPOGRAPHY.MONO,
              fontWeight: TYPOGRAPHY.BOLD,
              color: highlightRow === ri ? accentColor : FINCEPT.GRAY,
              backgroundColor: highlightRow === ri ? `${accentColor}15` : 'transparent',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              borderRight: `1px solid ${FINCEPT.BORDER}`,
            }}>
              {rowLabels[ri]}
            </div>
            {/* Data cells */}
            {row.map((val, ci) => {
              const cellColor = getColor(val);
              const isHighlight = highlightRow === ri && highlightCol === ci;
              return (
                <div
                  key={ci}
                  title={`${rowHeader}: ${rowLabels[ri]}, ${colHeader}: ${colLabels[ci]} → ${formatValue(val)}`}
                  style={{
                    width: cellSize,
                    height: '32px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontSize: '9px',
                    fontFamily: TYPOGRAPHY.MONO,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                    backgroundColor: `${cellColor}30`,
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    borderRight: `1px solid ${FINCEPT.BORDER}`,
                    outline: isHighlight ? `2px solid ${accentColor}` : 'none',
                    outlineOffset: '-2px',
                    position: 'relative' as const,
                  }}
                >
                  {formatValue(val)}
                </div>
              );
            })}
          </div>
        ))}
      </div>

      {/* Color scale legend */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        marginTop: '10px',
      }}>
        <span style={{ fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>
          LOW
        </span>
        <div style={{ display: 'flex', gap: '1px' }}>
          {HEATMAP_SCALE.map((c, i) => (
            <div key={i} style={{
              width: '16px', height: '8px',
              backgroundColor: `${c}50`,
              borderRadius: '1px',
            }} />
          ))}
        </div>
        <span style={{ fontSize: '8px', fontFamily: TYPOGRAPHY.MONO, color: FINCEPT.MUTED }}>
          HIGH
        </span>
      </div>
    </div>
  );
};
