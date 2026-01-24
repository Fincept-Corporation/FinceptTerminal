import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT } from '../../constants';

interface Props { data: GraphNodeData; selected?: boolean; }

const TYPE_LABELS: Record<string, string> = {
  financials: 'FINANCIALS',
  valuation: 'VALUATION',
  ownership: 'OWNERSHIP',
  balance_sheet: 'BALANCE SHEET',
};

const formatValue = (val: number | string): string => {
  if (typeof val === 'string') return val;
  if (Math.abs(val) >= 1e12) return `$${(val / 1e12).toFixed(2)}T`;
  if (Math.abs(val) >= 1e9) return `$${(val / 1e9).toFixed(1)}B`;
  if (Math.abs(val) >= 1e6) return `$${(val / 1e6).toFixed(0)}M`;
  if (Math.abs(val) < 1 && val !== 0) return `${(val * 100).toFixed(1)}%`;
  return val.toFixed(2);
};

const MetricsClusterNode: React.FC<Props> = ({ data, selected }) => {
  const metrics = data.metrics;
  const metricsType = data.metricsType || 'financials';
  if (!metrics) return null;

  const title = TYPE_LABELS[metricsType] || 'METRICS';
  const entries = Object.entries(metrics).filter(([, v]) => v !== 0 && v !== '' && v !== 'N/A');

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.GRAY}`, borderRadius: '2px', padding: '10px 12px', width: '240px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="target" position={Position.Left} style={{ background: FINCEPT.GRAY }} />

      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px', paddingBottom: '6px', borderBottom: `1px solid ${FINCEPT.BORDER}`, letterSpacing: '0.5px' }}>
        {title}
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
        {entries.slice(0, 8).map(([key, value]) => (
          <div key={key} style={{ padding: '2px 0' }}>
            <div style={{ fontSize: '7px', color: FINCEPT.MUTED, textTransform: 'uppercase', letterSpacing: '0.3px' }}>
              {key.replace(/_/g, ' ')}
            </div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: typeof value === 'number' && value < 0 ? FINCEPT.RED : FINCEPT.CYAN }}>
              {formatValue(value)}
            </div>
          </div>
        ))}
      </div>

      {entries.length > 8 && (
        <div style={{ marginTop: '6px', fontSize: '7px', color: FINCEPT.MUTED, textAlign: 'center' }}>
          +{entries.length - 8} MORE
        </div>
      )}
    </div>
  );
};

export default memo(MetricsClusterNode);
