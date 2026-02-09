import React from 'react';
import { Check } from 'lucide-react';
import type { FinceptConfigPanelProps } from '../types';
import { createStyles } from '../styles';

export function FinceptConfigPanel({ colors, sessionApiKey }: FinceptConfigPanelProps) {
  const styles = createStyles(colors);

  return (
    <div style={styles.successBox}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '10px',
        marginBottom: '12px'
      }}>
        <Check size={20} color={colors.success} />
        <span style={{ color: colors.success, fontSize: '14px', fontWeight: 700 }}>
          FINCEPT LLM - AUTO CONFIGURED
        </span>
      </div>
      <div style={{ color: '#AAA', fontSize: '12px', lineHeight: 1.6 }}>
        <div style={{ marginBottom: '8px' }}>
          Your Fincept API is automatically configured and ready to use.
        </div>
        <div style={styles.dataRow}>
          <div>
            <div style={styles.dataLabel}>API KEY</div>
            <div style={{ color: colors.primary, fontSize: '12px', fontFamily: 'monospace' }}>
              {sessionApiKey?.substring(0, 12)}...
            </div>
          </div>
          <div>
            <div style={styles.dataLabel}>COST</div>
            <div style={styles.dataValue}>5 credits/response</div>
          </div>
          <div>
            <div style={styles.dataLabel}>MODEL</div>
            <div style={styles.dataValue}>fincept-llm</div>
          </div>
        </div>
      </div>
    </div>
  );
}
