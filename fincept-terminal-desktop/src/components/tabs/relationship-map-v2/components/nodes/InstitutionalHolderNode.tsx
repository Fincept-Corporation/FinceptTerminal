import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';

interface InstitutionalHolderNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const InstitutionalHolderNode: React.FC<InstitutionalHolderNodeProps> = ({ data, selected }) => {
  const holder = data.holder;
  if (!holder) return null;

  const colors = NODE_COLORS.institutional;

  const formatValue = (val: number): string => {
    if (val >= 1e9) return `$${(val / 1e9).toFixed(1)}B`;
    if (val >= 1e6) return `$${(val / 1e6).toFixed(0)}M`;
    return `$${val.toLocaleString()}`;
  };

  return (
    <div
      style={{
        background: colors.bg,
        border: `1.5px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '6px',
        padding: '10px 14px',
        width: '220px',
        minHeight: '90px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
        <div style={{ width: '24px', height: '24px', borderRadius: '50%', background: 'rgba(0,200,0,0.2)', border: `1px solid ${colors.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '10px', color: colors.border }}>
          I
        </div>
        <div>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: colors.text, maxWidth: '150px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {holder.name}
          </div>
        </div>
      </div>

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>Ownership</div>
          <div style={{ fontSize: '13px', fontWeight: 'bold', color: colors.border }}>{holder.percentage.toFixed(2)}%</div>
        </div>
        {holder.value > 0 && (
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>Value</div>
            <div style={{ fontSize: '10px', color: 'var(--ft-color-text)' }}>{formatValue(holder.value)}</div>
          </div>
        )}
      </div>

      {holder.change_percent !== undefined && holder.change_percent !== 0 && (
        <div style={{ marginTop: '4px', fontSize: '9px', color: holder.change_percent > 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)' }}>
          {holder.change_percent > 0 ? '+' : ''}{holder.change_percent.toFixed(1)}% QoQ
        </div>
      )}
    </div>
  );
};

export default memo(InstitutionalHolderNode);
