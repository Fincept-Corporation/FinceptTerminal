/**
 * Node Configuration Panel
 *
 * Displays and allows editing of all parameters for a selected node.
 * Renders the parameter input UI using NodeParameterInput component.
 *
 * Features:
 * - Dynamic parameter rendering based on node type
 * - Expression support for all parameters
 * - Parameter validation
 * - Conditional parameters (displayOptions)
 * - Collection and FixedCollection support
 * - Real-time parameter updates
 */

import React, { useCallback, useMemo } from 'react';
import type {
  INode,
  INodeProperties,
  NodeParameterValue,
  INodeType,
} from '@/services/nodeSystem';
import { NodeRegistry } from '@/services/nodeSystem';
import NodeParameterInput from './NodeParameterInput';
import { Settings2, X, Info, AlertCircle } from 'lucide-react';

interface NodeConfigPanelProps {
  node: INode | null;
  onNodeUpdate: (nodeId: string, parameters: Record<string, any>) => void;
  onClose: () => void;
}

export const NodeConfigPanel: React.FC<NodeConfigPanelProps> = ({
  node,
  onNodeUpdate,
  onClose,
}) => {
  // Get node type definition
  const nodeType = useMemo<INodeType | null>(() => {
    if (!node) return null;
    try {
      return NodeRegistry.getNodeType(node.type, node.typeVersion);
    } catch (error) {
      console.error('Failed to get node type:', error);
      return null;
    }
  }, [node]);

  // Filter parameters based on displayOptions
  const visibleParameters = useMemo(() => {
    if (!nodeType?.description?.properties) return [];

    return nodeType.description.properties.filter((param) => {
      if (!param.displayOptions) return true;

      // Check show conditions
      if (param.displayOptions.show) {
        for (const [paramName, allowedValues] of Object.entries(param.displayOptions.show)) {
          const currentValue = node?.parameters[paramName];
          const values = Array.isArray(allowedValues) ? allowedValues : [allowedValues];

          if (!values.includes(currentValue as any)) {
            return false;
          }
        }
      }

      // Check hide conditions
      if (param.displayOptions.hide) {
        for (const [paramName, hiddenValues] of Object.entries(param.displayOptions.hide)) {
          const currentValue = node?.parameters[paramName];
          const values = Array.isArray(hiddenValues) ? hiddenValues : [hiddenValues];

          if (values.includes(currentValue as any)) {
            return false;
          }
        }
      }

      return true;
    });
  }, [nodeType, node?.parameters]);

  // Handle parameter change
  const handleParameterChange = useCallback(
    (paramName: string, value: NodeParameterValue) => {
      if (!node) return;

      const updatedParameters = {
        ...node.parameters,
        [paramName]: value,
      };

      onNodeUpdate(node.name, updatedParameters);
    },
    [node, onNodeUpdate]
  );

  if (!node) {
    return (
      <div className="w-96 bg-gray-900 border-l border-gray-800 flex items-center justify-center">
        <div className="text-center text-gray-500">
          <Settings2 size={48} className="mx-auto mb-4 opacity-30" />
          <p>Select a node to configure</p>
        </div>
      </div>
    );
  }

  if (!nodeType) {
    return (
      <div className="w-96 bg-gray-900 border-l border-gray-800 flex items-center justify-center">
        <div className="text-center text-red-400">
          <AlertCircle size={48} className="mx-auto mb-4" />
          <p>Node type not found</p>
          <p className="text-sm text-gray-500 mt-2">{node.type}</p>
        </div>
      </div>
    );
  }

  return (
    <div className="w-96 bg-gray-900 border-l border-gray-800 flex flex-col h-full">
      {/* Panel Header */}
      <div className="border-b border-gray-800 p-4 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div
            className="w-10 h-10 rounded flex items-center justify-center"
            style={{
              background: `${nodeType.description.defaults.color}20`,
              border: `2px solid ${nodeType.description.defaults.color}40`,
            }}
          >
            <Settings2 size={20} style={{ color: nodeType.description.defaults.color }} />
          </div>
          <div>
            <h3 className="text-sm font-semibold text-gray-200">
              {nodeType.description.displayName}
            </h3>
            <p className="text-xs text-gray-500">{node.name}</p>
          </div>
        </div>
        <button
          onClick={onClose}
          className="p-1.5 hover:bg-gray-800 rounded transition-colors"
          title="Close"
        >
          <X size={18} className="text-gray-400" />
        </button>
      </div>

      {/* Node Description */}
      {nodeType.description.description && (
        <div className="p-4 bg-blue-500/10 border-b border-gray-800">
          <div className="flex items-start gap-2">
            <Info size={16} className="text-blue-400 mt-0.5 flex-shrink-0" />
            <p className="text-xs text-blue-300">{nodeType.description.description}</p>
          </div>
        </div>
      )}

      {/* Parameters List */}
      <div className="flex-1 overflow-y-auto p-4 space-y-4">
        {visibleParameters.length === 0 ? (
          <div className="text-center text-gray-500 py-8">
            <Settings2 size={32} className="mx-auto mb-2 opacity-30" />
            <p className="text-sm">No parameters to configure</p>
          </div>
        ) : (
          visibleParameters.map((param) => (
            <div key={param.name} className="pb-4 border-b border-gray-800 last:border-0">
              <NodeParameterInput
                parameter={param}
                value={node.parameters[param.name] ?? param.default}
                onChange={(value) => handleParameterChange(param.name, value)}
                disabled={node.disabled}
              />
            </div>
          ))
        )}
      </div>

      {/* Panel Footer */}
      <div className="border-t border-gray-800 p-4 bg-gray-900">
        <div className="flex items-center justify-between text-xs text-gray-500">
          <span>Node ID: {node.name}</span>
          <span>
            {node.disabled ? (
              <span className="text-yellow-500">Disabled</span>
            ) : (
              <span className="text-green-500">Active</span>
            )}
          </span>
        </div>
      </div>
    </div>
  );
};

export default NodeConfigPanel;
