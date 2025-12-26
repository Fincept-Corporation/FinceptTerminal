import React, { useState, useEffect } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface RelationshipNode {
  id: string;
  label: string;
  type: 'country' | 'conflict' | 'organization' | 'crisis';
  severity?: 'low' | 'medium' | 'high' | 'critical';
  x: number;
  y: number;
  connections: string[];
  datasetCount?: number;
}

interface GeopoliticsRelationshipMapProps {
  centerNode?: string;
  width?: number;
  height?: number;
}

const GeopoliticsRelationshipMap: React.FC<GeopoliticsRelationshipMapProps> = ({
  centerNode = 'Global Conflicts',
  width = 800,
  height = 600
}) => {
  const { colors } = useTerminalTheme();
  const [hoveredNode, setHoveredNode] = useState<string | null>(null);
  const [selectedNode, setSelectedNode] = useState<string | null>(null);

  // Define relationship nodes (inspired by Bloomberg's network graph)
  const nodes: RelationshipNode[] = [
    // Center node
    { id: 'center', label: centerNode, type: 'crisis', x: 400, y: 300, connections: ['ukraine', 'gaza', 'sudan', 'yemen', 'syria', 'myanmar'], datasetCount: 245 },

    // Primary conflicts (first ring)
    { id: 'ukraine', label: 'Ukraine', type: 'conflict', severity: 'critical', x: 300, y: 150, connections: ['center', 'refugees', 'food'], datasetCount: 89 },
    { id: 'gaza', label: 'Gaza', type: 'conflict', severity: 'critical', x: 500, y: 150, connections: ['center', 'humanitarian', 'health'], datasetCount: 67 },
    { id: 'sudan', label: 'Sudan', type: 'conflict', severity: 'critical', x: 600, y: 300, connections: ['center', 'refugees', 'food'], datasetCount: 54 },
    { id: 'yemen', label: 'Yemen', type: 'conflict', severity: 'high', x: 500, y: 450, connections: ['center', 'food', 'health'], datasetCount: 43 },
    { id: 'syria', label: 'Syria', type: 'conflict', severity: 'high', x: 300, y: 450, connections: ['center', 'refugees'], datasetCount: 38 },
    { id: 'myanmar', label: 'Myanmar', type: 'conflict', severity: 'high', x: 200, y: 300, connections: ['center', 'humanitarian'], datasetCount: 29 },

    // Crisis types (second ring)
    { id: 'refugees', label: 'Displacement', type: 'crisis', severity: 'critical', x: 150, y: 150, connections: ['ukraine', 'sudan', 'syria'], datasetCount: 156 },
    { id: 'food', label: 'Food Security', type: 'crisis', severity: 'critical', x: 650, y: 150, connections: ['ukraine', 'sudan', 'yemen'], datasetCount: 134 },
    { id: 'humanitarian', label: 'Humanitarian', type: 'crisis', severity: 'high', x: 650, y: 450, connections: ['gaza', 'myanmar'], datasetCount: 98 },
    { id: 'health', label: 'Health Crisis', type: 'crisis', severity: 'medium', x: 150, y: 450, connections: ['gaza', 'yemen'], datasetCount: 76 },

    // Organizations (outer ring)
    { id: 'unhcr', label: 'UNHCR', type: 'organization', x: 100, y: 100, connections: ['refugees'], datasetCount: 234 },
    { id: 'wfp', label: 'WFP', type: 'organization', x: 700, y: 100, connections: ['food'], datasetCount: 187 },
    { id: 'who', label: 'WHO', type: 'organization', x: 700, y: 500, connections: ['health'], datasetCount: 145 },
    { id: 'fao', label: 'FAO', type: 'organization', x: 100, y: 500, connections: ['food'], datasetCount: 123 },
  ];

  const getNodeColor = (node: RelationshipNode): string => {
    if (node.type === 'organization') return colors.info;

    switch (node.severity) {
      case 'critical': return colors.alert;
      case 'high': return colors.warning;
      case 'medium': return '#FF9800'; // Orange
      case 'low': return colors.success;
      default: return colors.secondary;
    }
  };

  const getNodeSize = (node: RelationshipNode): number => {
    if (node.id === 'center') return 80;
    if (node.type === 'organization') return 45;
    if (node.type === 'conflict') return 55;
    return 50;
  };

  const drawConnections = () => {
    return nodes.map(node => {
      return node.connections.map(targetId => {
        const target = nodes.find(n => n.id === targetId);
        if (!target) return null;

        const isHovered = hoveredNode === node.id || hoveredNode === targetId;
        const strokeWidth = isHovered ? 2 : 1;
        const opacity = hoveredNode && !isHovered ? 0.2 : 0.6;

        return (
          <line
            key={`${node.id}-${targetId}`}
            x1={node.x}
            y1={node.y}
            x2={target.x}
            y2={target.y}
            stroke={colors.textMuted}
            strokeWidth={strokeWidth}
            opacity={opacity}
            strokeDasharray={node.type === 'organization' ? '4,4' : '0'}
          />
        );
      });
    });
  };

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: colors.background,
      position: 'relative',
      overflow: 'hidden'
    }}>
      {/* Header */}
      <div style={{
        padding: '8px 12px',
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        fontSize: '12px',
        fontWeight: 'bold',
        color: colors.primary
      }}>
        GEOPOLITICAL RELATIONSHIP MAP
      </div>

      {/* SVG Canvas */}
      <svg width={width} height={height} style={{ display: 'block' }}>
        {/* Draw connections first (behind nodes) */}
        <g>{drawConnections()}</g>

        {/* Draw nodes */}
        {nodes.map(node => {
          const size = getNodeSize(node);
          const color = getNodeColor(node);
          const isHovered = hoveredNode === node.id;
          const isSelected = selectedNode === node.id;

          return (
            <g
              key={node.id}
              onMouseEnter={() => setHoveredNode(node.id)}
              onMouseLeave={() => setHoveredNode(null)}
              onClick={() => setSelectedNode(node.id)}
              style={{ cursor: 'pointer' }}
            >
              {/* Node glow effect when hovered */}
              {isHovered && (
                <circle
                  cx={node.x}
                  cy={node.y}
                  r={size / 2 + 5}
                  fill={color}
                  opacity={0.3}
                />
              )}

              {/* Main node circle */}
              <circle
                cx={node.x}
                cy={node.y}
                r={size / 2}
                fill={node.id === 'center' ? color : colors.background}
                stroke={color}
                strokeWidth={isHovered || isSelected ? 3 : 2}
                opacity={hoveredNode && !isHovered ? 0.5 : 1}
              />

              {/* Node label */}
              <text
                x={node.x}
                y={node.y - size / 2 - 8}
                textAnchor="middle"
                fill={isHovered ? color : colors.text}
                fontSize={node.id === 'center' ? '12px' : '10px'}
                fontWeight={isHovered || node.id === 'center' ? 'bold' : 'normal'}
                opacity={hoveredNode && !isHovered ? 0.5 : 1}
              >
                {node.label}
              </text>

              {/* Dataset count badge */}
              {node.datasetCount && (
                <text
                  x={node.x}
                  y={node.y + 4}
                  textAnchor="middle"
                  fill={node.id === 'center' ? colors.background : color}
                  fontSize="9px"
                  fontWeight="bold"
                >
                  {node.datasetCount}
                </text>
              )}

              {/* Severity indicator for conflicts */}
              {node.type === 'conflict' && node.severity && (
                <circle
                  cx={node.x + size / 2 - 5}
                  cy={node.y - size / 2 + 5}
                  r={4}
                  fill={color}
                  stroke={colors.background}
                  strokeWidth={1}
                />
              )}
            </g>
          );
        })}
      </svg>

      {/* Node Details Panel */}
      {selectedNode && (
        <div style={{
          position: 'absolute',
          right: '12px',
          top: '60px',
          width: '200px',
          backgroundColor: colors.panel,
          border: `2px solid ${colors.primary}`,
          borderRadius: '4px',
          padding: '12px',
          fontSize: '10px'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            marginBottom: '8px'
          }}>
            <span style={{ color: colors.primary, fontWeight: 'bold' }}>
              {nodes.find(n => n.id === selectedNode)?.label}
            </span>
            <button
              onClick={() => setSelectedNode(null)}
              style={{
                backgroundColor: 'transparent',
                border: 'none',
                color: colors.textMuted,
                cursor: 'pointer',
                fontSize: '12px',
                padding: 0
              }}
            >
              ✕
            </button>
          </div>

          {(() => {
            const node = nodes.find(n => n.id === selectedNode);
            if (!node) return null;

            return (
              <>
                <div style={{ marginBottom: '6px' }}>
                  <span style={{ color: colors.textMuted }}>Type: </span>
                  <span style={{ color: colors.text, textTransform: 'capitalize' }}>
                    {node.type}
                  </span>
                </div>

                {node.severity && (
                  <div style={{ marginBottom: '6px' }}>
                    <span style={{ color: colors.textMuted }}>Severity: </span>
                    <span style={{
                      color: getNodeColor(node),
                      fontWeight: 'bold',
                      textTransform: 'uppercase'
                    }}>
                      {node.severity}
                    </span>
                  </div>
                )}

                <div style={{ marginBottom: '6px' }}>
                  <span style={{ color: colors.textMuted }}>Datasets: </span>
                  <span style={{ color: colors.secondary, fontWeight: 'bold' }}>
                    {node.datasetCount || 0}
                  </span>
                </div>

                <div style={{ marginBottom: '6px' }}>
                  <span style={{ color: colors.textMuted }}>Connections: </span>
                  <span style={{ color: colors.info }}>
                    {node.connections.length}
                  </span>
                </div>

                <div style={{ marginTop: '8px', paddingTop: '8px', borderTop: `1px solid ${colors.textMuted}` }}>
                  <button
                    style={{
                      width: '100%',
                      backgroundColor: colors.primary,
                      color: colors.background,
                      border: 'none',
                      padding: '6px',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      borderRadius: '2px'
                    }}
                  >
                    VIEW DATASETS →
                  </button>
                </div>
              </>
            );
          })()}
        </div>
      )}

      {/* Legend */}
      <div style={{
        position: 'absolute',
        left: '12px',
        bottom: '12px',
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: '4px',
        padding: '8px',
        fontSize: '9px'
      }}>
        <div style={{ color: colors.primary, fontWeight: 'bold', marginBottom: '6px' }}>
          LEGEND
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
          <div style={{ width: '12px', height: '12px', borderRadius: '50%', backgroundColor: colors.alert }} />
          <span style={{ color: colors.text }}>Critical</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
          <div style={{ width: '12px', height: '12px', borderRadius: '50%', backgroundColor: colors.warning }} />
          <span style={{ color: colors.text }}>High</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
          <div style={{ width: '12px', height: '12px', borderRadius: '50%', backgroundColor: '#FF9800' }} />
          <span style={{ color: colors.text }}>Medium</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <div style={{ width: '12px', height: '12px', borderRadius: '50%', backgroundColor: colors.info }} />
          <span style={{ color: colors.text }}>Organization</span>
        </div>
      </div>

      {/* Stats overlay */}
      <div style={{
        position: 'absolute',
        right: '12px',
        bottom: '12px',
        backgroundColor: colors.panel,
        border: `1px solid ${colors.textMuted}`,
        borderRadius: '4px',
        padding: '8px',
        fontSize: '9px',
        minWidth: '120px'
      }}>
        <div style={{ color: colors.primary, fontWeight: 'bold', marginBottom: '6px' }}>
          NETWORK STATS
        </div>
        <div style={{ marginBottom: '3px' }}>
          <span style={{ color: colors.textMuted }}>Nodes: </span>
          <span style={{ color: colors.text }}>{nodes.length}</span>
        </div>
        <div style={{ marginBottom: '3px' }}>
          <span style={{ color: colors.textMuted }}>Conflicts: </span>
          <span style={{ color: colors.alert }}>{nodes.filter(n => n.type === 'conflict').length}</span>
        </div>
        <div style={{ marginBottom: '3px' }}>
          <span style={{ color: colors.textMuted }}>Organizations: </span>
          <span style={{ color: colors.info }}>{nodes.filter(n => n.type === 'organization').length}</span>
        </div>
        <div>
          <span style={{ color: colors.textMuted }}>Total Datasets: </span>
          <span style={{ color: colors.secondary, fontWeight: 'bold' }}>
            {nodes.reduce((sum, n) => sum + (n.datasetCount || 0), 0)}
          </span>
        </div>
      </div>
    </div>
  );
};

export default GeopoliticsRelationshipMap;
