/**
 * NodeConfigPanel Component
 *
 * Right sidebar panel for configuring selected node properties
 */

import React from 'react';
import { Node } from 'reactflow';
import {
  Settings2,
  X,
  Trash2,
  Plus,
  Edit3,
  AlertCircle,
} from 'lucide-react';
import { NodeParameterInput } from '../nodes/NodeParameterInput';
import type { INodeProperties, NodeParameterValue } from '@/services/nodeSystem';

interface NodeConfigPanelProps {
  selectedNode: Node;
  onLabelChange: (nodeId: string, newLabel: string) => void;
  onParameterChange: (paramName: string, value: NodeParameterValue) => void;
  onClose: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
}

const NodeConfigPanel: React.FC<NodeConfigPanelProps> = ({
  selectedNode,
  onLabelChange,
  onParameterChange,
  onClose,
  onDelete,
  onDuplicate,
}) => {
  const registryData = selectedNode.data.registryData;
  const nodeProperties: INodeProperties[] = registryData?.properties || [];
  const nodeColor = selectedNode.data.color || '#FF8800';

  return (
    <div
      style={{
        width: '300px',
        backgroundColor: '#000000',
        borderLeft: '2px solid #FF8800',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      }}
    >
      {/* Panel Header */}
      <div
        style={{
          backgroundColor: '#1A1A1A',
          borderBottom: '1px solid #2A2A2A',
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Settings2
            size={14}
            style={{ color: '#FF8800', filter: 'drop-shadow(0 0 4px #FF8800)' }}
          />
          <span
            style={{
              color: '#FF8800',
              fontSize: '11px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
            }}
          >
            NODE CONFIG
          </span>
        </div>
        <button
          onClick={onClose}
          style={{
            backgroundColor: 'transparent',
            border: 'none',
            color: '#787878',
            cursor: 'pointer',
            padding: '2px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          <X size={14} />
        </button>
      </div>

      {/* Node Type Badge */}
      <div
        style={{
          backgroundColor: '#0F0F0F',
          padding: '10px 12px',
          borderBottom: '1px solid #2A2A2A',
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
        }}
      >
        <div
          style={{
            width: '28px',
            height: '28px',
            backgroundColor: `${nodeColor}20`,
            border: `1px solid ${nodeColor}`,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: '12px',
            fontWeight: 700,
            color: nodeColor,
          }}
        >
          {(selectedNode.data.label || selectedNode.type || 'N')[0].toUpperCase()}
        </div>
        <div>
          <div style={{ color: '#FFFFFF', fontSize: '11px', fontWeight: 600 }}>
            {selectedNode.data.label || selectedNode.type}
          </div>
          <div style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
            {selectedNode.type}
          </div>
        </div>
      </div>

      {/* Scrollable Content */}
      <div style={{ flex: 1, overflow: 'auto', backgroundColor: '#000000' }}>
        {/* Label Section */}
        <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
          <label
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              color: '#FF8800',
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: '6px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            <span>LABEL</span>
            <span style={{ color: '#4A4A4A', fontSize: '8px', fontWeight: 400 }}>REQUIRED</span>
          </label>
          <div
            style={{
              position: 'relative',
              display: 'flex',
              alignItems: 'stretch',
            }}
          >
            <div
              style={{
                backgroundColor: '#1A1A1A',
                borderTop: '1px solid #2A2A2A',
                borderLeft: '1px solid #2A2A2A',
                borderBottom: '1px solid #2A2A2A',
                padding: '0 8px',
                display: 'flex',
                alignItems: 'center',
                color: '#FF8800',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
              }}
            >
              <Edit3 size={10} />
            </div>
            <input
              type="text"
              value={selectedNode.data.label || ''}
              onChange={(e) => onLabelChange(selectedNode.id, e.target.value)}
              placeholder="Enter node label..."
              style={{
                flex: 1,
                backgroundColor: '#0A0A0A',
                border: '1px solid #2A2A2A',
                borderLeft: 'none',
                padding: '10px 12px',
                color: '#FFFFFF',
                fontSize: '11px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                outline: 'none',
                transition: 'all 0.15s ease',
              }}
            />
          </div>
          <div
            style={{
              marginTop: '4px',
              fontSize: '8px',
              color: '#4A4A4A',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            }}
          >
            Display name for this node in the workflow
          </div>
        </div>

        {/* Registry Node Parameters */}
        {nodeProperties.length > 0 && (
          <div
            style={{
              padding: '14px',
              borderBottom: '1px solid #3A3A3A',
              backgroundColor: '#111111',
            }}
          >
            {/* Parameters Header */}
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                marginBottom: '16px',
                paddingBottom: '10px',
                borderBottom: '1px solid #2A2A2A',
              }}
            >
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                }}
              >
                <div
                  style={{
                    width: '4px',
                    height: '18px',
                    backgroundColor: '#FF8800',
                  }}
                />
                <span
                  style={{
                    color: '#FF8800',
                    fontSize: '13px',
                    fontWeight: 700,
                    textTransform: 'uppercase',
                    letterSpacing: '1.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  }}
                >
                  PARAMETERS
                </span>
              </div>
              <div
                style={{
                  backgroundColor: '#FF880030',
                  border: '1px solid #FF8800',
                  padding: '4px 10px',
                  fontSize: '12px',
                  fontWeight: 700,
                  color: '#FF8800',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                }}
              >
                {nodeProperties.length}
              </div>
            </div>

            {/* Parameters List */}
            <div
              style={{
                display: 'flex',
                flexDirection: 'column',
                gap: '14px',
              }}
            >
              {nodeProperties.map((param, index) => (
                <div
                  key={param.name}
                  style={{
                    padding: '14px',
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #333333',
                    position: 'relative',
                  }}
                >
                  {/* Parameter Index Badge */}
                  <div
                    style={{
                      position: 'absolute',
                      top: '-1px',
                      left: '-1px',
                      backgroundColor: '#2A2A2A',
                      color: '#AAAAAA',
                      fontSize: '11px',
                      fontWeight: 700,
                      padding: '4px 8px',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      borderRight: '1px solid #444444',
                      borderBottom: '1px solid #444444',
                    }}
                  >
                    {String(index + 1).padStart(2, '0')}
                  </div>

                  {/* Required Indicator */}
                  {param.required && (
                    <div
                      style={{
                        position: 'absolute',
                        top: '0',
                        right: '0',
                        width: '0',
                        height: '0',
                        borderStyle: 'solid',
                        borderWidth: '0 16px 16px 0',
                        borderColor: 'transparent #FF3B3B transparent transparent',
                      }}
                    />
                  )}

                  <div style={{ paddingTop: '12px' }}>
                    <NodeParameterInput
                      parameter={param}
                      value={selectedNode.data.parameters?.[param.name] ?? param.default}
                      onChange={(value) => onParameterChange(param.name, value)}
                    />
                  </div>
                </div>
              ))}
            </div>

            {/* Parameters Footer */}
            <div
              style={{
                marginTop: '14px',
                paddingTop: '10px',
                borderTop: '1px solid #2A2A2A',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}
            >
              <div
                style={{
                  width: '8px',
                  height: '8px',
                  backgroundColor: '#00D66F',
                  borderRadius: '50%',
                  boxShadow: '0 0 6px #00D66F80',
                }}
              />
              <span
                style={{
                  color: '#AAAAAA',
                  fontSize: '11px',
                  fontWeight: 600,
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}
              >
                {nodeProperties.filter((p) => p.required).length > 0
                  ? `${nodeProperties.filter((p) => p.required).length} REQUIRED`
                  : 'ALL OPTIONAL'}
              </span>
            </div>
          </div>
        )}

        {/* Node Info Section */}
        <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
          <div
            style={{
              color: '#FF8800',
              fontSize: '9px',
              fontWeight: 700,
              marginBottom: '10px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px',
            }}
          >
            NODE INFO
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            {/* Node ID */}
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                ID
              </span>
              <code style={{ color: '#FFFFFF', fontSize: '9px' }}>{selectedNode.id}</code>
            </div>

            {/* Position */}
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                POS
              </span>
              <span style={{ color: '#FFFFFF', fontSize: '9px' }}>
                {Math.round(selectedNode.position.x)}, {Math.round(selectedNode.position.y)}
              </span>
            </div>

            {/* Registry Type */}
            {selectedNode.data.nodeTypeName && (
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                  TYPE
                </span>
                <span style={{ color: '#FFFFFF', fontSize: '9px' }}>
                  {selectedNode.data.nodeTypeName}
                </span>
              </div>
            )}

            {/* Status */}
            {selectedNode.data.status && (
              <div
                style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}
              >
                <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>
                  STATUS
                </span>
                <span
                  style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    padding: '2px 6px',
                    backgroundColor:
                      selectedNode.data.status === 'completed'
                        ? '#00D66F20'
                        : selectedNode.data.status === 'error'
                          ? '#FF3B3B20'
                          : selectedNode.data.status === 'running'
                            ? '#0088FF20'
                            : '#78787820',
                    color:
                      selectedNode.data.status === 'completed'
                        ? '#00D66F'
                        : selectedNode.data.status === 'error'
                          ? '#FF3B3B'
                          : selectedNode.data.status === 'running'
                            ? '#0088FF'
                            : '#787878',
                    textTransform: 'uppercase',
                  }}
                >
                  {selectedNode.data.status}
                </span>
              </div>
            )}
          </div>
        </div>

        {/* Description (for registry nodes) */}
        {registryData?.description && (
          <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
            <div
              style={{
                color: '#00E5FF',
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
            >
              DESCRIPTION
            </div>
            <div style={{ color: '#FFFFFF', fontSize: '10px', lineHeight: '1.5' }}>
              {registryData.description}
            </div>
          </div>
        )}

        {/* Error Display */}
        {selectedNode.data.error && (
          <div
            style={{
              margin: '12px',
              backgroundColor: '#FF3B3B15',
              border: '1px solid #FF3B3B',
              padding: '10px',
            }}
          >
            <div
              style={{
                color: '#FF3B3B',
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '4px',
                textTransform: 'uppercase',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
              }}
            >
              <AlertCircle size={10} />
              ERROR
            </div>
            <div style={{ color: '#FFFFFF', fontSize: '10px', lineHeight: '1.4' }}>
              {selectedNode.data.error}
            </div>
          </div>
        )}

        {/* Result Preview */}
        {selectedNode.data.result && selectedNode.data.status === 'completed' && (
          <div
            style={{
              margin: '12px',
              backgroundColor: '#00D66F15',
              border: '1px solid #00D66F',
              padding: '10px',
            }}
          >
            <div
              style={{
                color: '#00D66F',
                fontSize: '9px',
                fontWeight: 700,
                marginBottom: '6px',
                textTransform: 'uppercase',
              }}
            >
              OUTPUT PREVIEW
            </div>
            <pre
              style={{
                color: '#FFFFFF',
                fontSize: '9px',
                lineHeight: '1.4',
                overflow: 'auto',
                maxHeight: '80px',
                margin: 0,
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
              }}
            >
              {typeof selectedNode.data.result === 'string'
                ? selectedNode.data.result.substring(0, 200)
                : JSON.stringify(selectedNode.data.result, null, 2).substring(0, 200)}
              {(typeof selectedNode.data.result === 'string'
                ? selectedNode.data.result.length
                : JSON.stringify(selectedNode.data.result).length) > 200 && '...'}
            </pre>
          </div>
        )}
      </div>

      {/* Panel Footer with Actions */}
      <div
        style={{
          padding: '10px 12px',
          backgroundColor: '#0F0F0F',
          borderTop: '1px solid #2A2A2A',
          display: 'flex',
          gap: '8px',
        }}
      >
        <button
          onClick={onDelete}
          style={{
            flex: 1,
            backgroundColor: 'transparent',
            color: '#FF3B3B',
            border: '1px solid #FF3B3B',
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            textTransform: 'uppercase',
            letterSpacing: '0.3px',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#FF3B3B';
            e.currentTarget.style.color = '#000000';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = '#FF3B3B';
          }}
        >
          <Trash2 size={10} />
          DELETE
        </button>
        <button
          onClick={onDuplicate}
          style={{
            flex: 1,
            backgroundColor: '#FF8800',
            color: '#000000',
            border: '1px solid #FF8800',
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            textTransform: 'uppercase',
            letterSpacing: '0.3px',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
            transition: 'all 0.15s ease',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#FFA033';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = '#FF8800';
          }}
        >
          <Plus size={10} />
          DUPLICATE
        </button>
      </div>
    </div>
  );
};

export default NodeConfigPanel;
