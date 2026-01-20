import React, { memo, useMemo } from 'react';
import { Handle, Position } from 'reactflow';
import { Building2 } from 'lucide-react';
import { formatMarketCap } from '../utils';
import type { CompanyNodeProps } from '../types';

const CompanyNode = memo(({ data }: CompanyNodeProps) => {
  const { colors } = data;

  // Memoize styles to prevent recreation on each render
  const containerStyle = useMemo(
    () => ({
      background: 'linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 100%)',
      border: `3px solid ${colors.accent}`,
      borderRadius: '12px',
      padding: '24px',
      minWidth: '320px',
      maxWidth: '400px',
      boxShadow: `0 0 40px ${colors.accent}80, 0 10px 30px rgba(0,0,0,0.7)`,
    }),
    [colors.accent]
  );

  const handleStyle = useMemo(() => ({ opacity: 0 }), []);

  return (
    <div style={containerStyle}>
      <Handle type="target" position={Position.Top} style={handleStyle} />
      <div className="flex flex-col items-center text-center">
        <Building2 className="w-12 h-12 mb-3" style={{ color: colors.accent }} />
        <h2 className="text-2xl font-bold mb-1" style={{ color: colors.text }}>
          {data.name}
        </h2>
        <div className="text-xl mb-2 font-mono" style={{ color: colors.accent }}>
          {data.ticker}
        </div>
        <div className="text-sm mb-1" style={{ color: colors.textMuted }}>
          {data.sector} • {data.industry}
        </div>
        <div className="text-sm font-semibold mt-2" style={{ color: colors.success }}>
          Market Cap: {formatMarketCap(data.market_cap || 0)}
        </div>
        <div className="text-xs mt-2 opacity-60">
          CIK: {data.cik} | {data.exchange}
        </div>
        {data.employees && (
          <div className="text-xs mt-1" style={{ color: colors.textMuted }}>
            {data.employees.toLocaleString()} employees
          </div>
        )}
      </div>
      <Handle type="source" position={Position.Bottom} style={handleStyle} />
      <Handle type="source" position={Position.Left} style={handleStyle} />
      <Handle type="source" position={Position.Right} style={handleStyle} />
    </div>
  );
});

CompanyNode.displayName = 'CompanyNode';

export default CompanyNode;
