import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT } from '../../constants';

interface Props { data: GraphNodeData; selected?: boolean; }

const FundFamilyNode: React.FC<Props> = ({ data, selected }) => {
  const f = data.fundFamily;
  if (!f) return null;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.PURPLE}`, borderRadius: '2px', padding: '10px 12px', width: '240px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="source" position={Position.Right} style={{ background: FINCEPT.PURPLE }} />

      <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.PURPLE, marginBottom: '6px' }}>{f.name}</div>

      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
        <div>
          <div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>TOTAL %</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN }}>{f.total_percentage.toFixed(2)}%</div>
        </div>
        <div style={{ textAlign: 'right' }}>
          <div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>VALUE</div>
          <div style={{ fontSize: '9px', color: FINCEPT.WHITE }}>{f.total_value >= 1e9 ? `$${(f.total_value / 1e9).toFixed(1)}B` : `$${(f.total_value / 1e6).toFixed(0)}M`}</div>
        </div>
      </div>

      {f.funds.length > 1 && (
        <div style={{ borderTop: `1px solid ${FINCEPT.BORDER}`, paddingTop: '6px' }}>
          <div style={{ fontSize: '7px', color: FINCEPT.GRAY, marginBottom: '3px' }}>FUNDS ({f.funds.length})</div>
          {f.funds.slice(0, 3).map((fund, i) => (
            <div key={i} style={{ fontSize: '8px', color: FINCEPT.MUTED, marginBottom: '1px' }}>{fund.name} ({fund.percentage.toFixed(1)}%)</div>
          ))}
        </div>
      )}
    </div>
  );
};

export default memo(FundFamilyNode);
