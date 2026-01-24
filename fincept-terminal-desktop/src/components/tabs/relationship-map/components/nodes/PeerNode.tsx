import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT, SIGNAL_COLORS } from '../../constants';

interface PeerNodeProps { data: GraphNodeData; selected?: boolean; }

const PeerNode: React.FC<PeerNodeProps> = ({ data, selected }) => {
  const p = data.peer;
  const v = data.peerValuation;
  if (!p) return null;

  const sigColor = v ? SIGNAL_COLORS[v.action] : null;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `1.5px solid ${selected ? FINCEPT.WHITE : FINCEPT.BLUE}`, borderRadius: '2px', padding: '10px 12px', width: '240px', fontFamily: '"IBM Plex Mono", monospace' }}>
      <Handle type="target" position={Position.Left} style={{ background: FINCEPT.BLUE }} />

      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
        <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.BLUE }}>{p.ticker}</div>
        {sigColor && (
          <div style={{ padding: '2px 6px', backgroundColor: sigColor.bg, borderRadius: '2px' }}>
            <span style={{ fontSize: '7px', fontWeight: 700, color: sigColor.text }}>{v!.action}</span>
          </div>
        )}
      </div>
      <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '8px' }}>{p.name}</div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
        <div><div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>MKT CAP</div><div style={{ fontSize: '9px', color: FINCEPT.CYAN }}>{p.market_cap >= 1e9 ? `$${(p.market_cap / 1e9).toFixed(0)}B` : p.market_cap >= 1e6 ? `$${(p.market_cap / 1e6).toFixed(0)}M` : 'N/A'}</div></div>
        <div><div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>P/E</div><div style={{ fontSize: '9px', color: FINCEPT.CYAN }}>{p.pe_ratio ? p.pe_ratio.toFixed(1) : 'N/A'}</div></div>
        <div><div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>P/B</div><div style={{ fontSize: '9px', color: FINCEPT.CYAN }}>{p.price_to_book ? p.price_to_book.toFixed(1) : 'N/A'}</div></div>
        <div><div style={{ fontSize: '7px', color: FINCEPT.GRAY }}>GROWTH</div><div style={{ fontSize: '9px', color: p.revenue_growth > 0 ? FINCEPT.GREEN : FINCEPT.RED }}>{p.revenue_growth ? `${(p.revenue_growth * 100).toFixed(1)}%` : 'N/A'}</div></div>
      </div>
    </div>
  );
};

export default memo(PeerNode);
