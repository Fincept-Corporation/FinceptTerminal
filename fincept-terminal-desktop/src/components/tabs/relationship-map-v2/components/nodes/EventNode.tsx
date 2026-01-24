import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';

interface EventNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const categoryIcons: Record<string, string> = {
  earnings: 'E',
  management: 'M',
  merger: 'A',
  restructuring: 'R',
  governance: 'G',
  other: '8K',
};

const categoryLabels: Record<string, string> = {
  earnings: 'Earnings',
  management: 'Management',
  merger: 'M&A',
  restructuring: 'Restructuring',
  governance: 'Governance',
  other: 'Corporate Event',
};

const EventNode: React.FC<EventNodeProps> = ({ data, selected }) => {
  const event = data.event;
  if (!event) return null;

  const colors = NODE_COLORS.event;
  const icon = categoryIcons[event.category] || '8K';
  const categoryLabel = categoryLabels[event.category] || 'Event';

  return (
    <div
      style={{
        background: colors.bg,
        border: `1.5px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '6px',
        padding: '8px 12px',
        width: '240px',
        minHeight: '80px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
        <div style={{ width: '22px', height: '22px', borderRadius: '4px', background: 'rgba(255,0,0,0.2)', border: `1px solid ${colors.border}`, display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '8px', fontWeight: 'bold', color: colors.border }}>
          {icon}
        </div>
        <div>
          <div style={{ fontSize: '10px', fontWeight: 'bold', color: colors.text }}>{categoryLabel}</div>
          <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>{event.filing_date}</div>
        </div>
      </div>

      <div style={{ fontSize: '9px', color: 'var(--ft-color-text)', maxHeight: '28px', overflow: 'hidden', textOverflow: 'ellipsis', lineHeight: '14px' }}>
        {event.description || event.items?.[0] || 'Corporate event'}
      </div>

      {event.form && (
        <div style={{ marginTop: '4px', fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>
          Form: {event.form}
        </div>
      )}
    </div>
  );
};

export default memo(EventNode);
