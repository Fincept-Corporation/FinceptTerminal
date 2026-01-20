/**
 * CustomNode Component
 *
 * Generic custom node component for the workflow editor
 * Handles rendering, editing labels, and displaying status
 */

import React, { useState } from 'react';
import { Handle, Position } from 'reactflow';
import {
  Database,
  Zap,
  TrendingUp,
  BarChart3,
  Brain,
  FileText,
  Edit3,
} from 'lucide-react';
import type { CustomNodeData } from '../types';

interface CustomNodeProps {
  data: CustomNodeData;
  id: string;
  selected: boolean;
}

const CustomNode: React.FC<CustomNodeProps> = ({ data, id, selected }) => {
  const [isEditing, setIsEditing] = useState(false);
  const [label, setLabel] = useState(data.label);

  const handleLabelSave = () => {
    data.onLabelChange(id, label);
    setIsEditing(false);
  };

  const getIcon = () => {
    switch (data.nodeType) {
      case 'data-source':
        return <Database size={16} />;
      case 'transformation':
        return <Zap size={16} />;
      case 'analysis':
        return <TrendingUp size={16} />;
      case 'visualization':
        return <BarChart3 size={16} />;
      case 'agent':
        return <Brain size={16} />;
      case 'output':
        return <FileText size={16} />;
      default:
        return <Database size={16} />;
    }
  };

  return (
    <div
      style={{
        background: selected ? '#2d2d2d' : '#1a1a1a',
        border: `2px solid ${selected ? data.color : `${data.color}80`}`,
        borderRadius: '6px',
        minWidth: '180px',
        boxShadow: selected
          ? `0 0 20px ${data.color}60, 0 4px 12px rgba(0,0,0,0.5)`
          : `0 2px 8px rgba(0,0,0,0.3)`,
        transition: 'all 0.2s',
      }}
    >
      {/* Input Handle */}
      {data.hasInput && (
        <Handle
          type="target"
          position={Position.Left}
          style={{
            background: data.color,
            width: 12,
            height: 12,
            border: '2px solid #1a1a1a',
            boxShadow: `0 0 8px ${data.color}`,
          }}
        />
      )}

      {/* Node Header */}
      <div
        style={{
          background: `${data.color}20`,
          borderBottom: `1px solid ${data.color}40`,
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: '8px',
          color: data.color,
          borderTopLeftRadius: '4px',
          borderTopRightRadius: '4px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {getIcon()}
          {isEditing ? (
            <input
              type="text"
              value={label}
              onChange={(e) => setLabel(e.target.value)}
              onBlur={handleLabelSave}
              onKeyDown={(e) => {
                if (e.key === 'Enter') handleLabelSave();
                if (e.key === 'Escape') {
                  setLabel(data.label);
                  setIsEditing(false);
                }
              }}
              autoFocus
              style={{
                background: '#1a1a1a',
                border: `1px solid ${data.color}`,
                color: data.color,
                fontSize: '11px',
                padding: '2px 6px',
                borderRadius: '3px',
                outline: 'none',
                fontWeight: 'bold',
                width: '120px',
              }}
            />
          ) : (
            <span
              style={{
                fontSize: '11px',
                fontWeight: 'bold',
                letterSpacing: '0.3px',
              }}
            >
              {data.label.toUpperCase()}
            </span>
          )}
        </div>
        {!isEditing && (
          <button
            onClick={() => setIsEditing(true)}
            style={{
              background: 'transparent',
              border: 'none',
              color: data.color,
              cursor: 'pointer',
              padding: '2px',
              display: 'flex',
              alignItems: 'center',
            }}
            title="Edit label"
          >
            <Edit3 size={12} />
          </button>
        )}
      </div>

      {/* Node Body */}
      <div
        style={{
          padding: '12px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
          color: '#a3a3a3',
          fontSize: '10px',
        }}
      >
        <div>Type: {data.nodeType}</div>
        <div>Status: {data.status || 'Ready'}</div>
        <div style={{ color: '#737373' }}>ID: {id.substring(0, 8)}</div>
      </div>

      {/* Output Handle */}
      {data.hasOutput && (
        <Handle
          type="source"
          position={Position.Right}
          style={{
            background: data.color,
            width: 12,
            height: 12,
            border: '2px solid #1a1a1a',
            boxShadow: `0 0 8px ${data.color}`,
          }}
        />
      )}
    </div>
  );
};

export default CustomNode;
