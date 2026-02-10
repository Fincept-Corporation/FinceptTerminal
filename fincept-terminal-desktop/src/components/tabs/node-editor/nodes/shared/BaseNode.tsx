// Base Node Wrapper
// Provides consistent container and handle setup for all nodes

import React, { ReactNode } from 'react';
import { Handle, Position } from 'reactflow';
import { getContainerStyle, getHandleStyle, FINCEPT } from './FinceptNodeDesign';

interface HandleConfig {
  id?: string;
  position: Position;
  type: 'source' | 'target';
  color?: string;
  top?: string;
  label?: string;
}

interface BaseNodeProps {
  children: ReactNode;
  selected: boolean;
  minWidth?: string;
  maxWidth?: string;
  handles?: HandleConfig[];
  borderColor?: string;
}

/**
 * BaseNode - Wrapper component that provides consistent styling and handles
 *
 * Usage:
 * <BaseNode
 *   selected={selected}
 *   minWidth="280px"
 *   handles={[
 *     { type: 'target', position: Position.Left, color: FINCEPT.GREEN },
 *     { type: 'source', position: Position.Right, color: FINCEPT.ORANGE }
 *   ]}
 * >
 *   <NodeHeader ... />
 *   <div>Your content</div>
 * </BaseNode>
 */
export const BaseNode: React.FC<BaseNodeProps> = ({
  children,
  selected,
  minWidth = '280px',
  maxWidth = '400px',
  handles = [],
  borderColor,
}) => {
  return (
    <div
      style={{
        ...getContainerStyle(selected, borderColor),
        minWidth,
        maxWidth,
      }}
    >
      {/* Render handles */}
      {handles.map((handle, index) => (
        <React.Fragment key={handle.id || `handle-${index}`}>
          <Handle
            type={handle.type}
            position={handle.position}
            id={handle.id}
            style={{
              ...getHandleStyle(handle.color || FINCEPT.GREEN),
              ...(handle.top && { top: handle.top }),
              ...(handle.position === Position.Left && { left: '-6px' }),
              ...(handle.position === Position.Right && { right: '-6px' }),
            }}
          />
          {/* Handle label */}
          {handle.label && (
            <div
              style={{
                position: 'absolute',
                ...(handle.position === Position.Left && {
                  left: '-60px',
                  textAlign: 'right',
                }),
                ...(handle.position === Position.Right && {
                  right: '-60px',
                  textAlign: 'left',
                }),
                ...(handle.top ? { top: handle.top } : { top: '50%' }),
                transform: 'translateY(-50%)',
                fontSize: '9px',
                color: FINCEPT.GRAY,
                whiteSpace: 'nowrap',
              }}
            >
              {handle.label}
            </div>
          )}
        </React.Fragment>
      ))}

      {/* Node content */}
      {children}
    </div>
  );
};

// Specialized base node for nodes with dynamic input handles
interface DynamicHandleBaseNodeProps extends Omit<BaseNodeProps, 'handles'> {
  inputHandles?: Array<{ id: string; label?: string; top?: string }>;
  outputHandles?: Array<{ id: string; label?: string; top?: string; color?: string }>;
  inputColor?: string;
  outputColor?: string;
}

export const DynamicHandleBaseNode: React.FC<DynamicHandleBaseNodeProps> = ({
  children,
  selected,
  minWidth = '280px',
  maxWidth = '400px',
  inputHandles = [],
  outputHandles = [],
  inputColor = FINCEPT.ORANGE,
  outputColor = FINCEPT.GREEN,
  borderColor,
}) => {
  const handles: HandleConfig[] = [
    ...inputHandles.map((input, index) => ({
      id: input.id,
      type: 'target' as const,
      position: Position.Left,
      color: inputColor,
      top: input.top || `${30 + index * 20}px`,
      label: input.label,
    })),
    ...outputHandles.map((output, index) => ({
      id: output.id,
      type: 'source' as const,
      position: Position.Right,
      color: output.color || outputColor,
      top: output.top || `${30 + index * 20}px`,
      label: output.label,
    })),
  ];

  return (
    <BaseNode
      selected={selected}
      minWidth={minWidth}
      maxWidth={maxWidth}
      handles={handles}
      borderColor={borderColor}
    >
      {children}
    </BaseNode>
  );
};
