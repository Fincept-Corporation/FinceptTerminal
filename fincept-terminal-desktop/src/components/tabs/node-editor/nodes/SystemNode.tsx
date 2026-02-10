/**
 * System Node Component for ReactFlow
 *
 * React component for rendering nodes from the n8n-inspired node system.
 * Works with the NodeRegistry and displays nodes with their properties.
 */

import React, { useMemo } from 'react';
import { Position, NodeProps } from 'reactflow';
import type { INode, INodeType } from '@/services/nodeSystem';
import { NodeRegistry } from '@/services/nodeSystem';
import {
  Settings,
  Play,
  CheckCircle,
  XCircle,
  Clock,
  AlertCircle,
  Zap,
} from 'lucide-react';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  BaseNode,
  NodeHeader,
  Button,
  StatusPanel,
  Tag,
} from './shared';

export interface SystemNodeData {
  node: INode;
  onConfigure?: (node: INode) => void;
  onExecute?: (node: INode) => void;
  executionStatus?: 'idle' | 'running' | 'success' | 'error';
  executionTime?: number;
  error?: string;
}

export const SystemNode: React.FC<NodeProps<SystemNodeData>> = ({
  data,
  selected,
}) => {
  const node = data?.node;

  // Get node type definition - handle missing node gracefully
  const nodeType = useMemo<INodeType | null>(() => {
    if (!node?.type) {
      return null;
    }
    try {
      return NodeRegistry.getNodeType(node.type, node.typeVersion);
    } catch (error) {
      console.error('Failed to get node type:', error);
      return null;
    }
  }, [node?.type, node?.typeVersion]);

  // Handle missing node data
  if (!node) {
    return (
      <div style={{
        padding: `${SPACING.LG} ${SPACING.XL}`,
        backgroundColor: `${FINCEPT.ORANGE}20`,
        border: `2px solid ${FINCEPT.ORANGE}`,
        borderRadius: BORDER_RADIUS.SM,
        minWidth: '200px',
        fontFamily: FONT_FAMILY,
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.MD,
          color: FINCEPT.ORANGE,
        }}>
          <AlertCircle size={16} />
          <span style={{ fontSize: '12px', fontWeight: 600 }}>Invalid Node Data</span>
        </div>
        <div style={{
          fontSize: '10px',
          color: `${FINCEPT.ORANGE}B0`,
          marginTop: SPACING.XS,
        }}>
          Node data is missing or corrupted
        </div>
      </div>
    );
  }

  // Node color
  const nodeColor = nodeType?.description?.defaults?.color || FINCEPT.GRAY;

  // Execution status icon
  const getStatusIcon = () => {
    switch (data.executionStatus) {
      case 'running':
        return <Clock size={14} color={FINCEPT.BLUE} style={{ animation: 'pulse 2s infinite' }} />;
      case 'success':
        return <CheckCircle size={14} color={FINCEPT.GREEN} />;
      case 'error':
        return <XCircle size={14} color={FINCEPT.RED} />;
      default:
        return null;
    }
  };

  // Node icon or first letter
  const getNodeIcon = () => {
    if (nodeType?.description?.icon) {
      return <Zap size={16} style={{ color: nodeColor }} />;
    }

    const displayName = nodeType?.description?.displayName || node.type;
    return (
      <div style={{
        width: '24px',
        height: '24px',
        borderRadius: BORDER_RADIUS.SM,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        fontSize: '11px',
        fontWeight: 700,
        background: `${nodeColor}30`,
        color: nodeColor,
      }}>
        {displayName[0].toUpperCase()}
      </div>
    );
  };

  if (!nodeType) {
    return (
      <div style={{
        padding: `${SPACING.LG} ${SPACING.XL}`,
        backgroundColor: `${FINCEPT.RED}20`,
        border: `2px solid ${FINCEPT.RED}`,
        borderRadius: BORDER_RADIUS.SM,
        minWidth: '200px',
        fontFamily: FONT_FAMILY,
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.MD,
          color: FINCEPT.RED,
        }}>
          <AlertCircle size={16} />
          <span style={{ fontSize: '12px', fontWeight: 600 }}>Unknown Node Type</span>
        </div>
        <div style={{
          fontSize: '10px',
          color: `${FINCEPT.RED}B0`,
          marginTop: SPACING.XS,
        }}>
          {node.type}
        </div>
      </div>
    );
  }

  return (
    <BaseNode
      selected={selected}
      minWidth="220px"
      maxWidth="350px"
      borderColor={nodeColor}
      handles={[
        { type: 'target', position: Position.Left, color: FINCEPT.GRAY },
        { type: 'source', position: Position.Right, color: FINCEPT.GRAY },
      ]}
    >
      <div style={{ opacity: node.disabled ? 0.5 : 1 }}>
        {/* Header */}
        <NodeHeader
          icon={getNodeIcon()}
          title={nodeType.description.displayName}
          subtitle={node.name !== nodeType.description.displayName ? node.name : undefined}
          color={FINCEPT.WHITE}
          rightActions={getStatusIcon()}
        />

        {/* Body */}
        <div style={{ padding: `${SPACING.MD} ${SPACING.LG}` }}>
          {/* Execution Time */}
          {data.executionTime !== undefined && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.GRAY,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.XS,
              marginBottom: SPACING.XS,
            }}>
              <Clock size={12} />
              <span>{data.executionTime}ms</span>
            </div>
          )}

          {/* Error Message */}
          {data.error && (
            <StatusPanel
              type="error"
              icon={<XCircle size={12} />}
              message={data.error}
            />
          )}

          {/* Node Type */}
          <div style={{
            fontSize: '9px',
            color: FINCEPT.GRAY,
            opacity: 0.7,
            marginBottom: SPACING.XS,
          }}>
            {node.type} v{node.typeVersion || 1}
          </div>

          {/* Disabled Badge */}
          {node.disabled && (
            <Tag label="Disabled" color={FINCEPT.ORANGE} />
          )}
        </div>

        {/* Footer - Actions */}
        <div style={{
          padding: `${SPACING.MD} ${SPACING.LG}`,
          borderTop: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.MD,
        }}>
          <button
            onClick={() => data.onConfigure?.(node)}
            style={{
              flex: 1,
              padding: `${SPACING.XS} ${SPACING.MD}`,
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: BORDER_RADIUS.SM,
              fontSize: '10px',
              color: FINCEPT.GRAY,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: SPACING.XS,
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            title="Configure node"
          >
            <Settings size={12} />
            Configure
          </button>

          {data.onExecute && (
            <button
              onClick={() => data.onExecute?.(node)}
              disabled={node.disabled || data.executionStatus === 'running'}
              style={{
                padding: `${SPACING.XS} ${SPACING.MD}`,
                backgroundColor: node.disabled || data.executionStatus === 'running'
                  ? FINCEPT.HEADER_BG
                  : FINCEPT.BLUE,
                border: 'none',
                borderRadius: BORDER_RADIUS.SM,
                fontSize: '10px',
                color: node.disabled || data.executionStatus === 'running'
                  ? FINCEPT.GRAY
                  : FINCEPT.WHITE,
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.XS,
                cursor: node.disabled || data.executionStatus === 'running'
                  ? 'not-allowed'
                  : 'pointer',
                transition: 'all 0.2s',
              }}
              title="Execute node"
            >
              <Play size={12} />
            </button>
          )}
        </div>
      </div>

      {/* Execution Indicator Overlay */}
      {data.executionStatus === 'running' && (
        <div style={{
          position: 'absolute',
          inset: 0,
          borderRadius: BORDER_RADIUS.SM,
          pointerEvents: 'none',
          background: `linear-gradient(90deg, transparent 0%, ${nodeColor}20 50%, transparent 100%)`,
          backgroundSize: '200% 100%',
          animation: 'shimmer 2s infinite',
        }} />
      )}

      <style>{`
        @keyframes shimmer {
          0% { background-position: 200% 0; }
          100% { background-position: -200% 0; }
        }
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>
    </BaseNode>
  );
};

export default SystemNode;
