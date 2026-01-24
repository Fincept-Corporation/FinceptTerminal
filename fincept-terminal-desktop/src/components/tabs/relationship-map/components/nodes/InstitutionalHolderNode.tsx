import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT } from '../../constants';

interface Props { data: GraphNodeData; selected?: boolean; }

const InstitutionalHolderNode: React.FC<Props> = ({ data, selected }) => {
  const h = data.holder;
  if (!h) return null;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.GREEN}`, borderRadius: '2px', padding: '8px 12px', width: '200px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="source" position={Position.Right} style={{ background: FINCEPT.GREEN }} />

      <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GREEN, marginBottom: '4px' }}>{h.name}</div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <div>
          <div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>OWNERSHIP</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN }}>{h.percentage.toFixed(2)}%</div>
        </div>
        <div style={{ textAlign: 'right' }}>
          <div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>VALUE</div>
          <div style={{ fontSize: '9px', color: FINCEPT.WHITE }}>{h.value >= 1e9 ? `$${(h.value / 1e9).toFixed(1)}B` : h.value >= 1e6 ? `$${(h.value / 1e6).toFixed(0)}M` : 'N/A'}</div>
        </div>
      </div>
    </div>
  );
};

export default memo(InstitutionalHolderNode);
