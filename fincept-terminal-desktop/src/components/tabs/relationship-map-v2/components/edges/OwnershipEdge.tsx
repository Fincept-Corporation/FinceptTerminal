import React from 'react';
import { BaseEdge, getSmoothStepPath, type EdgeProps } from 'reactflow';
import type { GraphEdgeData } from '../../types';

const OwnershipEdge: React.FC<EdgeProps> = ({
  id,
  sourceX,
  sourceY,
  targetX,
  targetY,
  sourcePosition,
  targetPosition,
  data,
  selected,
}) => {
  const edgeData = data as GraphEdgeData | undefined;
  const percentage = edgeData?.percentage || 0;
  const category = edgeData?.category || 'internal';

  // Scale stroke width based on ownership percentage (1-8px for 0-15%)
  const strokeWidth = category === 'ownership'
    ? Math.max(1, Math.min(8, (percentage / 15) * 8))
    : category === 'peer' ? 1.5
    : 1;

  // Color based on category
  const strokeColor = category === 'ownership' ? 'var(--ft-color-success)'
    : category === 'peer' ? 'var(--ft-color-info)'
    : category === 'event' ? 'var(--ft-color-alert)'
    : category === 'supply_chain' ? 'var(--ft-color-warning)'
    : 'var(--ft-color-text-muted)';

  const strokeDasharray = category === 'peer' ? '5 5'
    : category === 'event' ? '3 3'
    : undefined;

  const [edgePath, labelX, labelY] = getSmoothStepPath({
    sourceX,
    sourceY,
    sourcePosition,
    targetX,
    targetY,
    targetPosition,
    borderRadius: 20,
  });

  return (
    <>
      <BaseEdge
        id={id}
        path={edgePath}
        style={{
          stroke: selected ? '#fff' : strokeColor,
          strokeWidth: selected ? strokeWidth + 1 : strokeWidth,
          strokeDasharray,
          opacity: selected ? 1 : 0.7,
        }}
      />
      {/* Edge Label for ownership */}
      {category === 'ownership' && percentage > 0 && (
        <foreignObject
          x={labelX - 25}
          y={labelY - 10}
          width={50}
          height={20}
          style={{ pointerEvents: 'none' }}
        >
          <div style={{
            background: 'rgba(0,0,0,0.8)',
            border: '1px solid rgba(0,200,0,0.4)',
            borderRadius: '3px',
            padding: '1px 4px',
            fontSize: '8px',
            color: 'var(--ft-color-success)',
            textAlign: 'center',
            whiteSpace: 'nowrap',
          }}>
            {percentage.toFixed(1)}%
          </div>
        </foreignObject>
      )}
      {/* Edge Label for peer */}
      {category === 'peer' && edgeData?.label && (
        <foreignObject
          x={labelX - 20}
          y={labelY - 8}
          width={40}
          height={16}
          style={{ pointerEvents: 'none' }}
        >
          <div style={{
            fontSize: '7px',
            color: 'var(--ft-color-info)',
            textAlign: 'center',
            opacity: 0.7,
          }}>
            {edgeData.label}
          </div>
        </foreignObject>
      )}
    </>
  );
};

export default OwnershipEdge;
