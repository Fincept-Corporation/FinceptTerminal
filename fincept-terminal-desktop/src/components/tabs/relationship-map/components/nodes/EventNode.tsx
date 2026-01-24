import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT } from '../../constants';

interface Props { data: GraphNodeData; selected?: boolean; }

const CATEGORY_LABELS: Record<string, string> = {
  earnings: 'EARNINGS',
  management: 'MGMT',
  merger: 'M&A',
  restructuring: 'RESTRUCTURE',
  governance: 'GOVERNANCE',
  other: '8-K',
};

const EventNode: React.FC<Props> = ({ data, selected }) => {
  const ev = data.event;
  if (!ev) return null;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.RED}`, borderRadius: '2px', padding: '8px 12px', width: '220px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="target" position={Position.Left} style={{ background: FINCEPT.RED }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
        <div style={{ padding: '2px 5px', backgroundColor: `${FINCEPT.RED}20`, borderRadius: '2px' }}>
          <span style={{ fontSize: '7px', fontWeight: 700, color: FINCEPT.RED }}>{CATEGORY_LABELS[ev.category] || ev.form}</span>
        </div>
        <span style={{ fontSize: '7px', color: FINCEPT.MUTED }}>{ev.filing_date}</span>
      </div>
      <div style={{ fontSize: '8px', color: FINCEPT.WHITE, lineHeight: 1.3 }}>
        {ev.description.length > 60 ? ev.description.slice(0, 60) + '...' : ev.description}
      </div>
    </div>
  );
};

export default memo(EventNode);
