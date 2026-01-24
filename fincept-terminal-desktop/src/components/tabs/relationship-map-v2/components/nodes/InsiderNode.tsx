import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';

interface InsiderNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const InsiderNode: React.FC<InsiderNodeProps> = ({ data, selected }) => {
  const insider = data.insider;
  if (!insider) return null;

  const colors = NODE_COLORS.insider;
  const activityColor = insider.last_transaction_type === 'buy' ? 'var(--ft-color-success)' :
                        insider.last_transaction_type === 'sell' ? 'var(--ft-color-alert)' : 'var(--ft-color-text-muted)';

  return (
    <div
      style={{
        background: colors.bg,
        border: `1.5px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '6px',
        padding: '10px 14px',
        width: '240px',
        minHeight: '100px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
        <div style={{ width: '28px', height: '28px', borderRadius: '50%', background: 'rgba(0,255,255,0.15)', border: `1px solid ${colors.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '12px' }}>
          P
        </div>
        <div>
          <div style={{ fontSize: '11px', fontWeight: 'bold', color: colors.text, maxWidth: '160px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {insider.name}
          </div>
          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{insider.title}</div>
        </div>
      </div>

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        {insider.percentage > 0 && (
          <div>
            <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>Ownership</div>
            <div style={{ fontSize: '11px', fontWeight: 'bold', color: colors.border }}>{insider.percentage.toFixed(2)}%</div>
          </div>
        )}
        {insider.last_transaction_type && (
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>Last Activity</div>
            <div style={{ fontSize: '10px', fontWeight: 'bold', color: activityColor }}>
              {insider.last_transaction_type.toUpperCase()}
            </div>
          </div>
        )}
      </div>

      {insider.last_transaction_date && (
        <div style={{ marginTop: '4px', fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>
          Last: {insider.last_transaction_date}
        </div>
      )}
    </div>
  );
};

export default memo(InsiderNode);
