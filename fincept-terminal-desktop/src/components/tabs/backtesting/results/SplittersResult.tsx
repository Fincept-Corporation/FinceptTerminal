/**
 * SplittersResult - Cross-validation split results display
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface SplittersResultProps {
  result: any;
}

export const SplittersResult: React.FC<SplittersResultProps> = ({ result }) => {
  const data = result?.data;

  if (!data) {
    return (
      <div style={{ padding: '16px', color: FINCEPT.MUTED, fontSize: '9px', fontFamily: FONT_FAMILY, textAlign: 'center' }}>
        No split data available.
      </div>
    );
  }

  const splits = data.splits || [];
  const nSplits = data.nSplits || splits.length;
  const coverage = data.coverage;

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
          SPLIT SUMMARY
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: FONT_FAMILY }}>
              {nSplits}
            </div>
            <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>SPLITS</div>
          </div>
          {coverage !== undefined && (
            <div style={{ textAlign: 'center' }}>
              <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: FONT_FAMILY }}>
                {typeof coverage === 'number' ? `${(coverage * 100).toFixed(1)}%` : String(coverage)}
              </div>
              <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>COVERAGE</div>
            </div>
          )}
        </div>
      </div>

      {/* Split Ranges */}
      {splits.length > 0 && (
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
            SPLIT RANGES
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {splits.map((split: any, idx: number) => (
              <div key={idx} style={{
                padding: '8px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}>
                <span style={{
                  fontSize: '8px', fontWeight: 700, color: FINCEPT.MUTED,
                  fontFamily: FONT_FAMILY, minWidth: '30px',
                }}>
                  #{idx + 1}
                </span>

                {/* Train range */}
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>TRAIN</div>
                  <div style={{ fontSize: '8px', color: FINCEPT.BLUE, fontFamily: FONT_FAMILY, fontWeight: 600 }}>
                    {split.trainStart || split.train_start || '?'} → {split.trainEnd || split.train_end || '?'}
                  </div>
                </div>

                {/* Test range */}
                <div style={{ flex: 1 }}>
                  <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>TEST</div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GREEN, fontFamily: FONT_FAMILY, fontWeight: 600 }}>
                    {split.testStart || split.test_start || '?'} → {split.testEnd || split.test_end || '?'}
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
