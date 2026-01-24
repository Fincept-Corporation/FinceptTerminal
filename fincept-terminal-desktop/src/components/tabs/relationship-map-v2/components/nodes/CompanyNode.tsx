import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';
import { getSignalColor, getSignalLabel } from '../../utils/valuationSignals';

interface CompanyNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const CompanyNode: React.FC<CompanyNodeProps> = ({ data, selected }) => {
  const company = data.company;
  const valuation = data.valuation;
  if (!company) return null;

  const signalColor = valuation ? getSignalColor(valuation.action) : null;
  const colors = NODE_COLORS.company;

  const formatNumber = (num: number): string => {
    if (num >= 1e12) return `$${(num / 1e12).toFixed(2)}T`;
    if (num >= 1e9) return `$${(num / 1e9).toFixed(2)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(1)}M`;
    return `$${num.toLocaleString()}`;
  };

  const formatPercent = (val: number): string => {
    if (Math.abs(val) < 1) return `${(val * 100).toFixed(1)}%`;
    return `${val.toFixed(1)}%`;
  };

  return (
    <div
      style={{
        background: colors.bg,
        border: `2px solid ${selected ? '#fff' : colors.border}`,
        borderRadius: '8px',
        padding: '16px',
        width: '420px',
        minHeight: '240px',
        position: 'relative',
        boxShadow: selected ? '0 0 20px rgba(255,165,0,0.4)' : '0 4px 12px rgba(0,0,0,0.3)',
      }}
    >
      <Handle type="source" position={Position.Right} style={{ background: colors.border }} />
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />
      <Handle type="source" position={Position.Bottom} id="bottom" style={{ background: colors.border }} />
      <Handle type="target" position={Position.Top} id="top" style={{ background: colors.border }} />

      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
        <div>
          <div style={{ fontSize: '18px', fontWeight: 'bold', color: colors.border }}>{company.ticker}</div>
          <div style={{ fontSize: '11px', color: 'var(--ft-color-text-muted)', maxWidth: '240px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {company.name}
          </div>
          <div style={{ fontSize: '10px', color: 'var(--ft-color-text-muted)', marginTop: '2px' }}>
            {company.sector} | {company.industry}
          </div>
        </div>
        {/* Valuation Badge */}
        {valuation && signalColor && (
          <div style={{
            background: signalColor.bg,
            border: `1px solid ${signalColor.border}`,
            borderRadius: '4px',
            padding: '4px 8px',
            textAlign: 'center',
          }}>
            <div style={{ fontSize: '9px', fontWeight: 'bold', color: signalColor.text }}>
              {getSignalLabel(valuation.status)}
            </div>
            <div style={{ fontSize: '11px', fontWeight: 'bold', color: signalColor.text }}>
              {valuation.action}
            </div>
            <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>
              {valuation.confidence} conf.
            </div>
          </div>
        )}
      </div>

      {/* Key Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '10px' }}>
        <MetricBox label="Market Cap" value={formatNumber(company.market_cap)} />
        <MetricBox label="Price" value={`$${company.current_price?.toFixed(2) || '0'}`} />
        <MetricBox label="P/E" value={company.pe_ratio ? company.pe_ratio.toFixed(1) : 'N/A'} />
        <MetricBox label="Revenue Growth" value={formatPercent(company.revenue_growth)} positive={company.revenue_growth > 0} />
        <MetricBox label="Profit Margin" value={formatPercent(company.profit_margins)} positive={company.profit_margins > 0} />
        <MetricBox label="ROE" value={formatPercent(company.roe)} positive={company.roe > 0} />
      </div>

      {/* Ownership Bar */}
      <div style={{ marginTop: '8px' }}>
        <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>Ownership Distribution</div>
        <div style={{ display: 'flex', height: '8px', borderRadius: '4px', overflow: 'hidden', background: 'rgba(255,255,255,0.05)' }}>
          <div style={{ width: `${company.insider_percent}%`, background: 'var(--ft-color-accent)', minWidth: company.insider_percent > 0 ? '2px' : '0' }} />
          <div style={{ width: `${company.institutional_percent}%`, background: 'var(--ft-color-success)', minWidth: company.institutional_percent > 0 ? '2px' : '0' }} />
          <div style={{ flex: 1, background: 'rgba(120,120,120,0.3)' }} />
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '3px', fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>
          <span>Insiders: {company.insider_percent.toFixed(1)}%</span>
          <span>Institutional: {company.institutional_percent.toFixed(1)}%</span>
        </div>
      </div>

      {/* Analyst Recommendation */}
      {company.recommendation && company.recommendation !== 'N/A' && (
        <div style={{ marginTop: '8px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
            Analyst: {company.recommendation.toUpperCase()} ({company.analyst_count})
          </span>
          {company.target_price > 0 && (
            <span style={{ fontSize: '9px', color: 'var(--ft-color-info)' }}>
              Target: ${company.target_price.toFixed(0)}
            </span>
          )}
        </div>
      )}
    </div>
  );
};

function MetricBox({ label, value, positive }: { label: string; value: string; positive?: boolean }) {
  return (
    <div style={{ textAlign: 'center' }}>
      <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', marginBottom: '2px' }}>{label}</div>
      <div style={{
        fontSize: '11px',
        fontWeight: 'bold',
        color: positive !== undefined ? (positive ? 'var(--ft-color-success)' : 'var(--ft-color-alert)') : 'var(--ft-color-text)',
      }}>
        {value}
      </div>
    </div>
  );
}

export default memo(CompanyNode);
