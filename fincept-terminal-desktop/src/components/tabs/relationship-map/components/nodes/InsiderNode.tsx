import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT } from '../../constants';

interface Props { data: GraphNodeData; selected?: boolean; }

const InsiderNode: React.FC<Props> = ({ data, selected }) => {
  const ins = data.insider;
  if (!ins) return null;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.CYAN}`, borderRadius: '2px', padding: '8px 12px', width: '220px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="source" position={Position.Right} style={{ background: FINCEPT.CYAN }} />

      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '2px' }}>{ins.name}</div>
      <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '6px' }}>{ins.title}</div>

      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        {ins.last_transaction_type && (
          <div style={{ padding: '2px 6px', backgroundColor: ins.last_transaction_type === 'buy' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`, borderRadius: '2px' }}>
            <span style={{ fontSize: '7px', fontWeight: 700, color: ins.last_transaction_type === 'buy' ? FINCEPT.GREEN : FINCEPT.RED }}>{ins.last_transaction_type.toUpperCase()}</span>
          </div>
        )}
        {ins.last_transaction_date && (
          <span style={{ fontSize: '7px', color: FINCEPT.MUTED }}>{ins.last_transaction_date}</span>
        )}
      </div>
    </div>
  );
};

export default memo(InsiderNode);
