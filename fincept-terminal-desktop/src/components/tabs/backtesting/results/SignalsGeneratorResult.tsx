/**
 * SignalsGeneratorResult - Random signal generator results display
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface SignalsGeneratorResultProps {
  result: any;
}

export const SignalsGeneratorResult: React.FC<SignalsGeneratorResultProps> = ({ result }) => {
  const data = result?.data;

  if (!data) {
    return (
      <div style={{ padding: '16px', color: FINCEPT.MUTED, fontSize: '9px', fontFamily: FONT_FAMILY, textAlign: 'center' }}>
        No signal data available.
      </div>
    );
  }

  const entries = data.entries || data.entrySignals || [];
  const exits = data.exits || data.exitSignals || [];
  const totalEntries = data.totalEntries || entries.length;
  const totalExits = data.totalExits || exits.length;
  const density = data.signalDensity;
  const signals = data.signals || [];

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
          SIGNAL SUMMARY
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN, fontFamily: FONT_FAMILY }}>
              {totalEntries}
            </div>
            <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>ENTRIES</div>
          </div>
          <div style={{ textAlign: 'center' }}>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.RED, fontFamily: FONT_FAMILY }}>
              {totalExits}
            </div>
            <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>EXITS</div>
          </div>
          {density !== undefined && (
            <div style={{ textAlign: 'center' }}>
              <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: FONT_FAMILY }}>
                {(density * 100).toFixed(1)}%
              </div>
              <div style={{ fontSize: '7px', color: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}>DENSITY</div>
            </div>
          )}
        </div>
      </div>

      {/* Signal Timeline Preview */}
      {signals.length > 0 && (
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
            SIGNAL TIMELINE (FIRST 20)
          </div>

          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
            {signals.slice(0, 20).map((signal: any, idx: number) => {
              const isEntry = signal.type === 'entry' || signal === 1 || signal === true;
              const color = isEntry ? FINCEPT.GREEN : FINCEPT.RED;
              const label = isEntry ? 'E' : 'X';
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
                  {typeof signal === 'object' ? (signal.date || `#${idx + 1}`) : ''} {label}
                </span>
              );
            })}
          </div>
        </div>
      )}

      {/* Entry timestamps */}
      {entries.length > 0 && signals.length === 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: FINCEPT.GREEN,
            marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
          }}>
            ENTRY SIGNALS (FIRST 20)
          </div>

          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
            {entries.slice(0, 20).map((entry: any, idx: number) => (
              <span key={idx} style={{
                padding: '2px 6px',
                backgroundColor: `${FINCEPT.GREEN}20`,
                border: `1px solid ${FINCEPT.GREEN}40`,
                borderRadius: '2px',
                fontSize: '8px',
                color: FINCEPT.GREEN,
                fontFamily: FONT_FAMILY,
              }}>
                {typeof entry === 'string' ? entry : `#${idx + 1}`}
              </span>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};
