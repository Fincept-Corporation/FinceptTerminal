/**
 * NodePalette Component
 *
 * Sidebar component for browsing and adding nodes to the workflow canvas
 * Supports search, categories, and drag-and-drop
 */

import React, { useState, useMemo } from 'react';
import { Database, Search, ChevronLeft, ChevronRight, Workflow } from 'lucide-react';
import { NodeRegistry, INodeTypeDescription } from '@/services/nodeSystem';
import { BUILTIN_NODE_CONFIGS, CATEGORY_CONFIG } from '../constants';
import type { NodePaletteProps, PaletteNodeItem } from '../types';

const NodePalette: React.FC<NodePaletteProps> = ({
  onNodeAdd,
  isCollapsed,
  onToggleCollapse,
  mcpNodeConfigs,
  agentConfigs,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(
    new Set(['Input', 'Processing', 'Output'])
  );

  // Get all registered nodes from NodeRegistry
  const registryNodes = useMemo(() => {
    try {
      return NodeRegistry.getAllNodes();
    } catch (e) {
      console.warn('[NodePalette] NodeRegistry not initialized yet');
      return [];
    }
  }, []);

  // Combine builtin nodes with registry nodes
  const allNodes = useMemo(() => {
    const nodes: PaletteNodeItem[] = [];

    // Add builtin UI nodes
    BUILTIN_NODE_CONFIGS.forEach((config) => {
      nodes.push({
        id: config.type,
        type: config.type,
        label: config.label,
        category: config.category,
        color: config.color,
        description: config.description,
        source: 'builtin',
        data: config,
      });
    });

    // Add registry nodes
    registryNodes.forEach((node: INodeTypeDescription) => {
      const category = node.group?.[0] || 'Core';
      const categoryConfig = CATEGORY_CONFIG[category] || CATEGORY_CONFIG['Core'];
      nodes.push({
        id: node.name,
        type: 'registry-node',
        label: node.displayName,
        category,
        color: node.defaults?.color || categoryConfig.color,
        description: node.description || '',
        source: 'registry',
        data: node,
      });
    });

    // Add MCP tool nodes
    mcpNodeConfigs.forEach((config) => {
      nodes.push({
        id: config.id,
        type: 'mcp-tool',
        label: config.label,
        category: 'MCP Tools',
        color: '#ea580c',
        description: `MCP: ${config.serverId}`,
        source: 'mcp',
        data: config,
      });
    });

    // Add Python agent nodes
    agentConfigs.forEach((agent) => {
      nodes.push({
        id: agent.id,
        type: 'python-agent',
        label: agent.name,
        category: 'Python Agents',
        color: agent.color || '#22c55e',
        description: agent.description,
        source: 'agent',
        data: agent,
      });
    });

    return nodes;
  }, [registryNodes, mcpNodeConfigs, agentConfigs]);

  // Group nodes by category
  const nodesByCategory = useMemo(() => {
    const grouped = new Map<string, PaletteNodeItem[]>();

    allNodes.forEach((node) => {
      if (!grouped.has(node.category)) {
        grouped.set(node.category, []);
      }
      grouped.get(node.category)!.push(node);
    });

    // Sort each category alphabetically
    grouped.forEach((nodes) => {
      nodes.sort((a, b) => a.label.localeCompare(b.label));
    });

    return grouped;
  }, [allNodes]);

  // Filter by search
  const filteredCategories = useMemo(() => {
    if (!searchQuery.trim()) return nodesByCategory;

    const query = searchQuery.toLowerCase();
    const filtered = new Map<string, PaletteNodeItem[]>();

    nodesByCategory.forEach((nodes, category) => {
      const matching = nodes.filter(
        (n) =>
          n.label.toLowerCase().includes(query) ||
          n.description.toLowerCase().includes(query) ||
          n.id.toLowerCase().includes(query)
      );
      if (matching.length > 0) {
        filtered.set(category, matching);
      }
    });

    return filtered;
  }, [nodesByCategory, searchQuery]);

  const toggleCategory = (category: string) => {
    const newExpanded = new Set(expandedCategories);
    if (newExpanded.has(category)) {
      newExpanded.delete(category);
    } else {
      newExpanded.add(category);
    }
    setExpandedCategories(newExpanded);
  };

  if (isCollapsed) {
    return (
      <div
        style={{
          width: '40px',
          backgroundColor: '#0a0a0a',
          borderRight: '1px solid #2d2d2d',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          paddingTop: '8px',
        }}
      >
        <button
          onClick={onToggleCollapse}
          style={{
            backgroundColor: 'transparent',
            border: 'none',
            color: '#a3a3a3',
            cursor: 'pointer',
            padding: '8px',
          }}
          title="Expand palette"
        >
          <ChevronRight size={16} />
        </button>
      </div>
    );
  }

  return (
    <div
      style={{
        width: '280px',
        backgroundColor: '#0a0a0a',
        borderRight: '1px solid #2d2d2d',
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
      }}
    >
      {/* Palette Header */}
      <div
        style={{
          padding: '12px',
          borderBottom: '1px solid #2d2d2d',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            color: '#ea580c',
            fontSize: '12px',
            fontWeight: 'bold',
          }}
        >
          <Database size={14} />
          NODE PALETTE
        </div>
        <button
          onClick={onToggleCollapse}
          style={{
            backgroundColor: 'transparent',
            border: 'none',
            color: '#6b7280',
            cursor: 'pointer',
            padding: '4px',
          }}
          title="Collapse palette"
        >
          <ChevronLeft size={14} />
        </button>
      </div>

      {/* Search */}
      <div style={{ padding: '8px 12px', borderBottom: '1px solid #2d2d2d' }}>
        <div style={{ position: 'relative' }}>
          <Search
            size={14}
            style={{
              position: 'absolute',
              left: '10px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: '#6b7280',
            }}
          />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search nodes..."
            style={{
              width: '100%',
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '4px',
              padding: '8px 8px 8px 32px',
              color: '#fff',
              fontSize: '11px',
              outline: 'none',
            }}
          />
        </div>
        <div style={{ color: '#6b7280', fontSize: '10px', marginTop: '6px' }}>
          {allNodes.length} nodes available
        </div>
      </div>

      {/* Node Categories */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {Array.from(filteredCategories.entries()).map(([category, categoryNodes]) => {
          const catConfig = CATEGORY_CONFIG[category] || {
            icon: <Workflow size={14} />,
            color: '#6b7280',
          };
          const isExpanded = expandedCategories.has(category);

          return (
            <div key={category} style={{ borderBottom: '1px solid #1a1a1a' }}>
              {/* Category Header */}
              <button
                onClick={() => toggleCategory(category)}
                style={{
                  width: '100%',
                  padding: '10px 12px',
                  backgroundColor: 'transparent',
                  border: 'none',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  cursor: 'pointer',
                  color: '#d4d4d4',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <span style={{ color: catConfig.color }}>{catConfig.icon}</span>
                  <span style={{ fontSize: '11px', fontWeight: 'bold' }}>{category}</span>
                  <span style={{ fontSize: '10px', color: '#6b7280' }}>
                    ({categoryNodes.length})
                  </span>
                </div>
                {isExpanded ? (
                  <ChevronRight size={12} style={{ transform: 'rotate(90deg)' }} />
                ) : (
                  <ChevronRight size={12} />
                )}
              </button>

              {/* Category Nodes */}
              {isExpanded && (
                <div style={{ padding: '4px 8px 8px' }}>
                  {categoryNodes.map((node) => (
                    <div
                      key={node.id}
                      onClick={() => onNodeAdd(node.type, node)}
                      draggable
                      onDragStart={(e) => {
                        e.dataTransfer.setData('application/reactflow', JSON.stringify(node));
                        e.dataTransfer.effectAllowed = 'move';
                      }}
                      style={{
                        backgroundColor: '#1a1a1a',
                        border: `1px solid ${node.color}30`,
                        borderRadius: '4px',
                        padding: '8px 10px',
                        marginBottom: '4px',
                        cursor: 'pointer',
                        transition: 'all 0.15s',
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.backgroundColor = `${node.color}15`;
                        e.currentTarget.style.borderColor = `${node.color}60`;
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = '#1a1a1a';
                        e.currentTarget.style.borderColor = `${node.color}30`;
                      }}
                    >
                      <div
                        style={{
                          display: 'flex',
                          alignItems: 'center',
                          gap: '8px',
                        }}
                      >
                        <div
                          style={{
                            width: '24px',
                            height: '24px',
                            borderRadius: '4px',
                            backgroundColor: `${node.color}20`,
                            border: `1px solid ${node.color}40`,
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            color: node.color,
                            fontSize: '10px',
                            fontWeight: 'bold',
                          }}
                        >
                          {node.label[0]}
                        </div>
                        <div style={{ flex: 1, minWidth: 0 }}>
                          <div
                            style={{
                              color: node.color,
                              fontSize: '11px',
                              fontWeight: 'bold',
                              whiteSpace: 'nowrap',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                            }}
                          >
                            {node.label}
                          </div>
                          {node.description && (
                            <div
                              style={{
                                color: '#6b7280',
                                fontSize: '9px',
                                whiteSpace: 'nowrap',
                                overflow: 'hidden',
                                textOverflow: 'ellipsis',
                              }}
                            >
                              {node.description.substring(0, 40)}
                              {node.description.length > 40 ? '...' : ''}
                            </div>
                          )}
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          );
        })}

        {filteredCategories.size === 0 && (
          <div
            style={{
              padding: '24px',
              textAlign: 'center',
              color: '#6b7280',
            }}
          >
            <Search size={24} style={{ margin: '0 auto 8px', opacity: 0.5 }} />
            <div style={{ fontSize: '11px' }}>No nodes found</div>
          </div>
        )}
      </div>

      {/* Palette Footer */}
      <div
        style={{
          padding: '8px 12px',
          borderTop: '1px solid #2d2d2d',
          backgroundColor: '#0a0a0a',
        }}
      >
        <div style={{ color: '#6b7280', fontSize: '9px', textAlign: 'center' }}>
          Drag nodes to canvas or click to add
        </div>
      </div>
    </div>
  );
};

export default NodePalette;
