import React from 'react';
import { BaseEdge, getSmoothStepPath, type EdgeProps } from 'reactflow';
import type { GraphEdgeData } from '../../types';
import { FINCEPT } from '../../constants';

const CATEGORY_COLORS: Record<string, string> = {
  ownership: FINCEPT.GREEN,
  peer: FINCEPT.BLUE,
  event: FINCEPT.RED,
  supply_chain: FINCEPT.YELLOW,
  internal: FINCEPT.MUTED,
};

const OwnershipEdge: React.FC<EdgeProps> = ({
  id, sourceX, sourceY, targetX, targetY,
  sourcePosition, targetPosition, data, selected,
}) => {
  const edgeData = data as GraphEdgeData | undefined;
  const category = edgeData?.category || 'internal';
  const percentage = edgeData?.percentage || 0;

  const [edgePath, labelX, labelY] = getSmoothStepPath({
    sourceX, sourceY, targetX, targetY, sourcePosition, targetPosition, borderRadius: 8,
  });

  const strokeWidth = category === 'ownership' ? Math.max(1, Math.min(6, percentage / 2)) : category === 'peer' ? 1.5 : 1;
  const stroke = CATEGORY_COLORS[category] || FINCEPT.MUTED;
  const dashArray = (category === 'peer' || category === 'event') ? '5 5' : undefined;

  return (
    <>
      <BaseEdge id={id} path={edgePath} style={{ stroke, strokeWidth, opacity: selected ? 1 : 0.6, strokeDasharray: dashArray }} />
      {category === 'ownership' && percentage > 0 && (
        <foreignObject x={labelX - 20} y={labelY - 8} width={40} height={16} style={{ overflow: 'visible' }}>
          <div style={{ fontSize: '7px', fontWeight: 700, color: FINCEPT.GREEN, backgroundColor: FINCEPT.DARK_BG, padding: '1px 4px', borderRadius: '2px', border: `1px solid ${FINCEPT.BORDER}`, textAlign: 'center', fontFamily: '"IBM Plex Mono", monospace' }}>
            {percentage.toFixed(1)}%
          </div>
        </foreignObject>
      )}
    </>
  );
};

export default OwnershipEdge;
