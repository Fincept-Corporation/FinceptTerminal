/**
 * CustomNode Component
 *
 * Generic custom node component for the workflow editor
 * Handles rendering, editing labels, and displaying status
 */

import React, { useState } from 'react';
import { Position } from 'reactflow';
import {
  Database,
  Zap,
  TrendingUp,
  BarChart3,
  Brain,
  FileText,
  Edit3,
  AlertCircle,
} from 'lucide-react';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  FONT_SIZE,
  BaseNode,
} from '../nodes/shared';
import type { CustomNodeData } from '../types';

interface CustomNodeProps {
  data: CustomNodeData;
  id: string;
  selected: boolean;
}

const CustomNode: React.FC<CustomNodeProps> = ({ data, id, selected }) => {
  const [isEditing, setIsEditing] = useState(false);
  const [label, setLabel] = useState(data?.label || 'Node');

  const handleLabelSave = () => {
    if (data?.onLabelChange) {
      data.onLabelChange(id, label);
    }
    setIsEditing(false);
  };

  // Handle missing data gracefully
  if (!data) {
    return (
      <div style={{
        background: FINCEPT.PANEL_BG,
        border: `2px solid ${FINCEPT.ORANGE}`,
        borderRadius: BORDER_RADIUS.SM,
        padding: SPACING.LG,
        minWidth: '180px',
        fontFamily: FONT_FAMILY,
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.MD,
          color: FINCEPT.ORANGE,
        }}>
          <AlertCircle size={14} />
          <span style={{ fontSize: FONT_SIZE.XL, fontWeight: 600 }}>Invalid Node Data</span>
        </div>
      </div>
    );
  }

  const nodeColor = data.color || FINCEPT.ORANGE;

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

  const handles = [
    ...(data.hasInput ? [{ type: 'target' as const, position: Position.Left, color: nodeColor }] : []),
    ...(data.hasOutput ? [{ type: 'source' as const, position: Position.Right, color: nodeColor }] : []),
  ];

  return (
    <BaseNode
      selected={selected}
      minWidth="180px"
      maxWidth="300px"
      borderColor={nodeColor}
      handles={handles}
    >
      {/* Node Header */}
      <div
        style={{
          background: `${nodeColor}20`,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          padding: `${SPACING.MD} ${SPACING.LG}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: SPACING.MD,
          color: nodeColor,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MD }}>
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
                background: FINCEPT.DARK_BG,
                border: `1px solid ${nodeColor}`,
                color: nodeColor,
                fontSize: FONT_SIZE.LG,
                padding: '2px 6px',
                borderRadius: BORDER_RADIUS.SM,
                outline: 'none',
                fontWeight: 700,
                fontFamily: FONT_FAMILY,
                width: '120px',
              }}
            />
          ) : (
            <span
              style={{
                fontSize: FONT_SIZE.LG,
                fontWeight: 700,
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
              color: nodeColor,
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
          padding: SPACING.LG,
          display: 'flex',
          flexDirection: 'column',
          gap: SPACING.XS,
          color: FINCEPT.GRAY,
          fontSize: FONT_SIZE.MD,
        }}
      >
        <div>Type: {data.nodeType}</div>
        <div>Status: {data.status || 'Ready'}</div>
        <div style={{ color: FINCEPT.GRAY, opacity: 0.6 }}>ID: {id.substring(0, 8)}</div>
      </div>
    </BaseNode>
  );
};

export default CustomNode;
