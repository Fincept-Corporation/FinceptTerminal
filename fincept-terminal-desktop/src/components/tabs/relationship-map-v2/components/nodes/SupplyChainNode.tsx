import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';

interface SupplyChainNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const SupplyChainNode: React.FC<SupplyChainNodeProps> = ({ data, selected }) => {
  const colors = NODE_COLORS.supply_chain;

  return (
    <div
      style={{
        background: colors.bg,
        border: `1.5px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '6px',
        padding: '10px 14px',
        width: '260px',
        minHeight: '100px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />
      <Handle type="source" position={Position.Right} style={{ background: colors.border }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
        <div style={{ width: '22px', height: '22px', borderRadius: '4px', background: 'rgba(255,255,0,0.15)', border: `1px solid ${colors.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '9px', color: colors.border }}>
          SC
        </div>
        <div>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: colors.text }}>{data.label}</div>
          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>Supply Chain</div>
        </div>
      </div>

      {data.metrics && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', marginTop: '4px' }}>
          {Object.entries(data.metrics).slice(0, 4).map(([key, val]) => (
            <div key={key}>
              <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>{key.replace(/_/g, ' ')}</div>
              <div style={{ fontSize: '10px', color: colors.text }}>{String(val)}</div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default memo(SupplyChainNode);
