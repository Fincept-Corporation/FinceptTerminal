import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { FINCEPT, SIGNAL_COLORS } from '../../constants';

interface CompanyNodeProps { data: GraphNodeData; selected?: boolean; }

function fmtMoney(val: number): string {
  if (!val) return 'N/A';
  if (val >= 1e12) return `$${(val / 1e12).toFixed(2)}T`;
  if (val >= 1e9) return `$${(val / 1e9).toFixed(1)}B`;
  if (val >= 1e6) return `$${(val / 1e6).toFixed(0)}M`;
  return `$${val.toLocaleString()}`;
}

const CompanyNode: React.FC<CompanyNodeProps> = ({ data, selected }) => {
  const c = data.company;
  const v = data.valuation;
  if (!c) return null;

  const sigColor = v ? SIGNAL_COLORS[v.action] : null;

  return (
    <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: `2px solid ${selected ? FINCEPT.WHITE : FINCEPT.ORANGE}`, borderRadius: '2px', padding: '12px 16px', width: '380px', minHeight: '200px', fontFamily: '"IBM Plex Mono", monospace', boxShadow: `0 0 12px ${FINCEPT.ORANGE}30` }}>
      <Handle type="target" position={Position.Top} style={{ background: FINCEPT.ORANGE }} />
      <Handle type="source" position={Position.Bottom} style={{ background: FINCEPT.ORANGE }} />
      <Handle type="source" position={Position.Left} style={{ background: FINCEPT.ORANGE }} />
      <Handle type="source" position={Position.Right} style={{ background: FINCEPT.ORANGE }} />

      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
        <div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.ORANGE }}>{c.ticker}</div>
          <div style={{ fontSize: '9px', color: FINCEPT.WHITE }}>{c.name}</div>
        </div>
        {sigColor && (
          <div style={{ padding: '3px 8px', backgroundColor: sigColor.bg, border: `1px solid ${sigColor.border}`, borderRadius: '2px' }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: sigColor.text }}>{v!.action}</div>
          </div>
        )}
      </div>

      <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '10px' }}>{c.sector} | {c.industry}</div>

      {/* Key Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '6px', marginBottom: '10px' }}>
        <Metric label="MKT CAP" value={fmtMoney(c.market_cap)} />
        <Metric label="PRICE" value={c.current_price ? `$${c.current_price.toFixed(2)}` : 'N/A'} />
        <Metric label="P/E" value={c.pe_ratio ? c.pe_ratio.toFixed(1) : 'N/A'} />
        <Metric label="GROWTH" value={c.revenue_growth ? `${(c.revenue_growth * 100).toFixed(1)}%` : 'N/A'} color={c.revenue_growth > 0 ? FINCEPT.GREEN : FINCEPT.RED} />
        <Metric label="MARGIN" value={c.profit_margins ? `${(c.profit_margins * 100).toFixed(1)}%` : 'N/A'} />
        <Metric label="ROE" value={c.roe ? `${(c.roe * 100).toFixed(1)}%` : 'N/A'} />
      </div>

      {/* Ownership Bar */}
      {(c.insider_percent > 0 || c.institutional_percent > 0) && (
        <div style={{ marginBottom: '8px' }}>
          <div style={{ fontSize: '7px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '3px' }}>OWNERSHIP</div>
          <div style={{ display: 'flex', height: '4px', borderRadius: '2px', overflow: 'hidden', backgroundColor: FINCEPT.BORDER }}>
            <div style={{ width: `${c.insider_percent}%`, backgroundColor: FINCEPT.CYAN }} />
            <div style={{ width: `${c.institutional_percent}%`, backgroundColor: FINCEPT.GREEN }} />
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px' }}>
            <span style={{ fontSize: '7px', color: FINCEPT.CYAN }}>INS {c.insider_percent.toFixed(1)}%</span>
            <span style={{ fontSize: '7px', color: FINCEPT.GREEN }}>INST {c.institutional_percent.toFixed(1)}%</span>
          </div>
        </div>
      )}

      {/* Analyst */}
      {c.recommendation && c.recommendation !== 'N/A' && (
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: '8px', color: FINCEPT.GRAY }}>ANALYST: <span style={{ color: FINCEPT.YELLOW, fontWeight: 700 }}>{c.recommendation.toUpperCase()}</span></span>
          {c.target_price > 0 && <span style={{ fontSize: '8px', color: FINCEPT.CYAN }}>TARGET ${c.target_price.toFixed(0)}</span>}
        </div>
      )}
    </div>
  );
};

const Metric: React.FC<{ label: string; value: string; color?: string }> = ({ label, value, color }) => (
  <div>
    <div style={{ fontSize: '7px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.3px' }}>{label}</div>
    <div style={{ fontSize: '10px', fontWeight: 700, color: color || FINCEPT.CYAN, fontFamily: '"IBM Plex Mono", monospace' }}>{value}</div>
  </div>
);

export default memo(CompanyNode);
