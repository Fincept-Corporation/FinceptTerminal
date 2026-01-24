import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT } from '../../constants';

interface Props { data: GraphNodeData; selected?: boolean; }

const SupplyChainNode: React.FC<Props> = ({ data, selected }) => {
  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.YELLOW}`, borderRadius: '2px', padding: '10px 12px', width: '240px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="target" position={Position.Left} style={{ background: FINCEPT.YELLOW }} />
      <Handle type="source" position={Position.Right} style={{ background: FINCEPT.YELLOW }} />

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
        <div style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.YELLOW}20`, borderRadius: '2px' }}>
          <span style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.YELLOW }}>SC</span>
        </div>
        <div>
          <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE }}>{data.label}</div>
          <div style={{ fontSize: '7px', color: FINCEPT.MUTED, letterSpacing: '0.5px' }}>SUPPLY CHAIN</div>
        </div>
      </div>

      {data.metrics && (
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: '6px' }}>
          {Object.entries(data.metrics).slice(0, 4).map(([key, val]) => (
            <div key={key}>
              <div style={{ fontSize: '7px', color: FINCEPT.MUTED, textTransform: 'uppercase' }}>{key.replace(/_/g, ' ')}</div>
              <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN }}>{String(val)}</div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default memo(SupplyChainNode);
