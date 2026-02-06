/**
 * System Node Component for ReactFlow
 *
 * React component for rendering nodes from the n8n-inspired node system.
 * Works with the NodeRegistry and displays nodes with their properties.
 *
 * Features:
 * - Dynamic styling based on node type
 * - Execution status indicators
 * - Error display
 * - Disabled state visualization
 * - Input/output handles
 * - Node configuration on click
 */

import React, { useMemo } from 'react';
import { Handle, Position, NodeProps } from 'reactflow';
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
      <div className="px-4 py-3 bg-yellow-900/20 border-2 border-yellow-500 rounded-lg min-w-[200px]">
        <div className="flex items-center gap-2 text-yellow-400">
          <AlertCircle size={16} />
          <span className="text-sm font-medium">Invalid Node Data</span>
        </div>
        <div className="text-xs text-yellow-400/70 mt-1">Node data is missing or corrupted</div>
      </div>
    );
  }

  // Node color
  const nodeColor = nodeType?.description?.defaults?.color || '#888888';

  // Execution status icon
  const getStatusIcon = () => {
    switch (data.executionStatus) {
      case 'running':
        return <Clock size={14} className="text-blue-400 animate-pulse" />;
      case 'success':
        return <CheckCircle size={14} className="text-green-400" />;
      case 'error':
        return <XCircle size={14} className="text-red-400" />;
      default:
        return null;
    }
  };

  // Node icon or first letter
  const getNodeIcon = () => {
    if (nodeType?.description?.icon) {
      // In a real implementation, you'd load the actual icon
      return <Zap size={16} style={{ color: nodeColor }} />;
    }

    const displayName = nodeType?.description?.displayName || node.type;
    return (
      <div
        className="w-6 h-6 rounded flex items-center justify-center text-xs font-bold"
        style={{
          background: `${nodeColor}30`,
          color: nodeColor,
        }}
      >
        {displayName[0].toUpperCase()}
      </div>
    );
  };

  if (!nodeType) {
    return (
      <div className="px-4 py-3 bg-red-900/20 border-2 border-red-500 rounded-lg min-w-[200px]">
        <div className="flex items-center gap-2 text-red-400">
          <AlertCircle size={16} />
          <span className="text-sm font-medium">Unknown Node Type</span>
        </div>
        <div className="text-xs text-red-400/70 mt-1">{node.type}</div>
      </div>
    );
  }

  return (
    <div
      className={`relative bg-gray-900 rounded-lg min-w-[220px] transition-all ${
        selected ? 'ring-2 ring-blue-500 shadow-lg shadow-blue-500/20' : 'shadow-md'
      } ${node.disabled ? 'opacity-50' : ''}`}
      style={{
        borderLeft: `4px solid ${nodeColor}`,
      }}
    >
      {/* Input Handle */}
      <Handle
        type="target"
        position={Position.Left}
        className="w-3 h-3 !bg-gray-700 !border-2 !border-gray-900"
        style={{
          left: -6,
        }}
      />

      {/* Node Header */}
      <div
        className="px-3 py-2 border-b border-gray-800 flex items-center justify-between gap-2"
        style={{
          background: `${nodeColor}10`,
        }}
      >
        <div className="flex items-center gap-2 flex-1 min-w-0">
          {getNodeIcon()}
          <div className="flex-1 min-w-0">
            <div className="text-sm font-medium text-gray-200 truncate">
              {nodeType.description.displayName}
            </div>
            {node.name !== nodeType.description.displayName && (
              <div className="text-xs text-gray-500 truncate">{node.name}</div>
            )}
          </div>
        </div>

        {/* Status Icon */}
        {getStatusIcon()}
      </div>

      {/* Node Body */}
      <div className="px-3 py-2 space-y-1">
        {/* Execution Time */}
        {data.executionTime !== undefined && (
          <div className="text-xs text-gray-500 flex items-center gap-1">
            <Clock size={12} />
            <span>{data.executionTime}ms</span>
          </div>
        )}

        {/* Error Message */}
        {data.error && (
          <div className="text-xs text-red-400 flex items-start gap-1 bg-red-900/20 px-2 py-1 rounded">
            <XCircle size={12} className="mt-0.5 flex-shrink-0" />
            <span className="break-words">{data.error}</span>
          </div>
        )}

        {/* Node Type */}
        <div className="text-xs text-gray-600">
          {node.type} v{node.typeVersion || 1}
        </div>

        {/* Disabled Badge */}
        {node.disabled && (
          <div className="inline-flex items-center gap-1 px-2 py-0.5 bg-yellow-900/30 border border-yellow-700/30 rounded text-xs text-yellow-500">
            <AlertCircle size={10} />
            Disabled
          </div>
        )}
      </div>

      {/* Node Footer - Actions */}
      <div className="px-3 py-2 border-t border-gray-800 flex items-center gap-2">
        <button
          onClick={() => data.onConfigure?.(node)}
          className="flex-1 px-2 py-1 bg-gray-800 hover:bg-gray-700 rounded text-xs text-gray-300 flex items-center justify-center gap-1 transition-colors"
          title="Configure node"
        >
          <Settings size={12} />
          Configure
        </button>

        {data.onExecute && (
          <button
            onClick={() => data.onExecute?.(node)}
            disabled={node.disabled || data.executionStatus === 'running'}
            className="px-2 py-1 bg-blue-600 hover:bg-blue-500 disabled:bg-gray-700 disabled:text-gray-500 rounded text-xs text-white flex items-center gap-1 transition-colors"
            title="Execute node"
          >
            <Play size={12} />
          </button>
        )}
      </div>

      {/* Output Handle */}
      <Handle
        type="source"
        position={Position.Right}
        className="w-3 h-3 !bg-gray-700 !border-2 !border-gray-900"
        style={{
          right: -6,
        }}
      />

      {/* Execution Indicator Overlay */}
      {data.executionStatus === 'running' && (
        <div
          className="absolute inset-0 rounded-lg pointer-events-none"
          style={{
            background: `linear-gradient(90deg, transparent 0%, ${nodeColor}20 50%, transparent 100%)`,
            backgroundSize: '200% 100%',
            animation: 'shimmer 2s infinite',
          }}
        />
      )}
    </div>
  );
};

export default SystemNode;
