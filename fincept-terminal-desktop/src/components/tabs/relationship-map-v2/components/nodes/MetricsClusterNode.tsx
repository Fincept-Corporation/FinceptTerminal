import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';

interface MetricsClusterNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const typeLabels: Record<string, string> = {
  financials: 'Financial Metrics',
  valuation: 'Valuation Metrics',
  ownership: 'Ownership Structure',
  balance_sheet: 'Balance Sheet',
};

const MetricsClusterNode: React.FC<MetricsClusterNodeProps> = ({ data, selected }) => {
  const metrics = data.metrics;
  const metricsType = data.metricsType || 'financials';
  if (!metrics) return null;

  const colors = NODE_COLORS.metrics;
  const title = typeLabels[metricsType] || 'Metrics';

  const formatValue = (val: number | string): string => {
    if (typeof val === 'string') return val;
    if (Math.abs(val) >= 1e12) return `$${(val / 1e12).toFixed(2)}T`;
    if (Math.abs(val) >= 1e9) return `$${(val / 1e9).toFixed(2)}B`;
    if (Math.abs(val) >= 1e6) return `$${(val / 1e6).toFixed(1)}M`;
    if (Math.abs(val) < 1 && val !== 0) return `${(val * 100).toFixed(1)}%`;
    return val.toFixed(2);
  };

  const entries = Object.entries(metrics).filter(([, v]) => v !== 0 && v !== '' && v !== 'N/A');

  return (
    <div
      style={{
        background: colors.bg,
        border: `1.5px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '6px',
        padding: '10px 14px',
        width: '260px',
        minHeight: '140px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />

      <div style={{ fontSize: '11px', fontWeight: 'bold', color: colors.border, marginBottom: '8px', borderBottom: '1px solid rgba(120,120,120,0.3)', paddingBottom: '6px' }}>
        {title}
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
        {entries.slice(0, 8).map(([key, value]) => (
          <div key={key} style={{ padding: '2px 0' }}>
            <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', textTransform: 'capitalize' }}>
              {key.replace(/_/g, ' ')}
            </div>
            <div style={{
              fontSize: '10px', fontWeight: 'bold',
              color: typeof value === 'number' && value < 0 ? 'var(--ft-color-alert)' : 'var(--ft-color-text)',
            }}>
              {formatValue(value)}
            </div>
          </div>
        ))}
      </div>

      {entries.length > 8 && (
        <div style={{ marginTop: '4px', fontSize: '8px', color: 'var(--ft-color-text-muted)', textAlign: 'center' }}>
          +{entries.length - 8} more metrics
        </div>
      )}
    </div>
  );
};

export default memo(MetricsClusterNode);
