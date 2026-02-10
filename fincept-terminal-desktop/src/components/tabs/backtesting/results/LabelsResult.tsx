/**
 * LabelsResult - ML label generation results display
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface LabelsResultProps {
  result: any;
}

export const LabelsResult: React.FC<LabelsResultProps> = ({ result }) => {
  const data = result?.data;

  if (!data) {
    return (
      <div style={{ padding: '16px', color: FINCEPT.MUTED, fontSize: '9px', fontFamily: FONT_FAMILY, textAlign: 'center' }}>
        No label data available.
      </div>
    );
  }

  const distribution = data.distribution || {};
  const labels = data.labels || [];
  const totalLabels = data.totalLabels || labels.length;
  const signalDensity = data.signalDensity;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Summary */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
      }}>
        <div style={{
          fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE,
          marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
        }}>
          LABEL SUMMARY
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
          {/* Total */}
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
              {totalLabels}
            </div>
            <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>TOTAL</div>
          </div>

          {/* Signal Density */}
          {signalDensity !== undefined && (
            <div style={{ textAlign: 'center' }}>
              <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: FONT_FAMILY }}>
                {(signalDensity * 100).toFixed(1)}%
              </div>
              <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>DENSITY</div>
            </div>
          )}
        </div>
      </div>

      {/* Distribution */}
      {Object.keys(distribution).length > 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE,
            marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
          }}>
            LABEL DISTRIBUTION
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {Object.entries(distribution).map(([label, count]) => {
              const color = label === '1' || label === 'buy' ? FINCEPT.GREEN
                : label === '-1' || label === 'sell' ? FINCEPT.RED
                : FINCEPT.GRAY;
              const pct = totalLabels > 0 ? ((count as number) / totalLabels * 100).toFixed(1) : '0';

              return (
                <div key={label} style={{
                  display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                }}>
                  <span style={{ fontSize: '9px', fontWeight: 700, color, fontFamily: FONT_FAMILY }}>
                    {String(label).toUpperCase()}
                  </span>
                  <span style={{ fontSize: '9px', color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
                    {String(count)} ({pct}%)
                  </span>
                </div>
              );
            })}
          </div>
        </div>
      )}

      {/* Label Preview */}
      {labels.length > 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE,
            marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
          }}>
            LABEL PREVIEW (FIRST 20)
          </div>

          <div style={{
            display: 'flex', flexWrap: 'wrap', gap: '4px',
          }}>
            {labels.slice(0, 20).map((label: any, idx: number) => {
              const val = typeof label === 'object' ? label.label : label;
              const color = val === 1 || val === '1' ? FINCEPT.GREEN
                : val === -1 || val === '-1' ? FINCEPT.RED
                : FINCEPT.GRAY;
              return (
                <span key={idx} style={{
                  padding: '2px 6px',
                  backgroundColor: `${color}20`,
                  border: `1px solid ${color}40`,
                  borderRadius: '2px',
                  fontSize: '8px',
                  fontWeight: 700,
                  color,
                  fontFamily: FONT_FAMILY,
                }}>
                  {String(val)}
                </span>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
};
