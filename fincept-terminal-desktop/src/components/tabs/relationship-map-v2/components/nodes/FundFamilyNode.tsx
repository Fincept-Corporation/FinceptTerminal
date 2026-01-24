import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';

interface FundFamilyNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const FundFamilyNode: React.FC<FundFamilyNodeProps> = ({ data, selected }) => {
  const family = data.fundFamily;
  if (!family) return null;

  const colors = NODE_COLORS.fund_family;

  const formatValue = (val: number): string => {
    if (val >= 1e9) return `$${(val / 1e9).toFixed(1)}B`;
    if (val >= 1e6) return `$${(val / 1e6).toFixed(0)}M`;
    return `$${val.toLocaleString()}`;
  };

  const trendIcon = family.change_trend === 'increasing' ? '\u2191' :
                    family.change_trend === 'decreasing' ? '\u2193' : '\u2194';
  const trendColor = family.change_trend === 'increasing' ? 'var(--ft-color-success)' :
                     family.change_trend === 'decreasing' ? 'var(--ft-color-alert)' : 'var(--ft-color-text-muted)';

  return (
    <div
      style={{
        background: colors.bg,
        border: `1.5px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '8px',
        padding: '12px',
        width: '280px',
        minHeight: '160px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />
      <Handle type="source" position={Position.Right} style={{ background: colors.border }} />

      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px' }}>
        <div style={{ fontSize: '12px', fontWeight: 'bold', color: colors.border, maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
          {family.name}
        </div>
        <span style={{ fontSize: '14px', color: trendColor }}>{trendIcon}</span>
      </div>

      {/* Aggregate Stats */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '10px' }}>
        <div>
          <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>Total Ownership</div>
          <div style={{ fontSize: '14px', fontWeight: 'bold', color: colors.border }}>{family.total_percentage.toFixed(2)}%</div>
        </div>
        <div>
          <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>Total Value</div>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{formatValue(family.total_value)}</div>
        </div>
      </div>

      {/* Top Funds */}
      <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>
        Funds ({family.funds.length})
      </div>
      {family.funds.slice(0, 3).map((fund, i) => (
        <div key={i} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', color: 'var(--ft-color-text)', marginBottom: '2px', padding: '2px 4px', background: 'rgba(255,255,255,0.03)', borderRadius: '2px' }}>
          <span style={{ maxWidth: '160px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{fund.name}</span>
          <span style={{ color: colors.border }}>{fund.percentage.toFixed(2)}%</span>
        </div>
      ))}
    </div>
  );
};

export default memo(FundFamilyNode);
