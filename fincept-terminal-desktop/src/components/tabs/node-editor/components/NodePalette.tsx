/**
 * NodePalette Component
 *
 * Sidebar component for browsing and adding nodes to the workflow canvas
 * Supports search, categories, and drag-and-drop
 */

import React, { useState, useMemo } from 'react';
import { Layers, Search, ChevronLeft, ChevronRight, Workflow, ChevronDown } from 'lucide-react';
import { NodeRegistry, INodeTypeDescription } from '@/services/nodeSystem';
import { BUILTIN_NODE_CONFIGS, CATEGORY_CONFIG } from '../constants';
import type { NodePaletteProps, PaletteNodeItem } from '../types';

const NodePalette: React.FC<NodePaletteProps> = ({
  onNodeAdd,
  isCollapsed,
  onToggleCollapse,
  mcpNodeConfigs,
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

    return nodes;
  }, [registryNodes, mcpNodeConfigs]);

  // Group nodes by category
  const nodesByCategory = useMemo(() => {
    const grouped = new Map<string, PaletteNodeItem[]>();

    allNodes.forEach((node) => {
      if (!grouped.has(node.category)) {
        grouped.set(node.category, []);
      }
      grouped.get(node.category)!.push(node);
    });

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
          width: '36px',
          backgroundColor: '#000000',
          borderRight: '1px solid #2a2a2a',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          paddingTop: '10px',
          gap: '12px',
        }}
      >
        <button
          onClick={onToggleCollapse}
          title="Expand palette"
          style={{
            backgroundColor: 'transparent',
            border: 'none',
            color: '#787878',
            cursor: 'pointer',
            padding: '6px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            borderRadius: '2px',
            transition: 'color 0.15s',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.color = '#FF8800')}
          onMouseLeave={(e) => (e.currentTarget.style.color = '#787878')}
        >
          <ChevronRight size={14} />
        </button>
        <div
          style={{
            color: '#FF8800',
            fontSize: '8px',
            fontWeight: 700,
            letterSpacing: '1px',
            writingMode: 'vertical-rl',
            textOrientation: 'mixed',
            transform: 'rotate(180deg)',
            marginTop: '4px',
          }}
        >
          NODES
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        width: '260px',
        backgroundColor: '#000000',
        borderRight: '1px solid #2a2a2a',
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        fontFamily: '"IBM Plex Mono", Consolas, monospace',
      }}
    >
      {/* Header */}
      <div
        style={{
          padding: '10px 12px',
          backgroundColor: '#0f0f0f',
          borderBottom: '1px solid #2a2a2a',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '7px',
          }}
        >
          <Layers size={13} style={{ color: '#FF8800' }} />
          <span
            style={{
              color: '#FF8800',
              fontSize: '10px',
              fontWeight: 700,
              letterSpacing: '0.8px',
            }}
          >
            NODE PALETTE
          </span>
        </div>
        <button
          onClick={onToggleCollapse}
          title="Collapse palette"
          style={{
            backgroundColor: 'transparent',
            border: 'none',
            color: '#787878',
            cursor: 'pointer',
            padding: '4px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            borderRadius: '2px',
            transition: 'color 0.15s',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.color = '#FF8800')}
          onMouseLeave={(e) => (e.currentTarget.style.color = '#787878')}
        >
          <ChevronLeft size={13} />
        </button>
      </div>

      {/* Search */}
      <div
        style={{
          padding: '8px 10px',
          borderBottom: '1px solid #2a2a2a',
          backgroundColor: '#000000',
        }}
      >
        <div style={{ position: 'relative' }}>
          <Search
            size={11}
            style={{
              position: 'absolute',
              left: '8px',
              top: '50%',
              transform: 'translateY(-50%)',
              color: '#787878',
              pointerEvents: 'none',
            }}
          />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search nodes..."
            style={{
              width: '100%',
              backgroundColor: '#0f0f0f',
              border: '1px solid #2a2a2a',
              borderRadius: '2px',
              padding: '6px 8px 6px 26px',
              color: '#ffffff',
              fontSize: '10px',
              fontFamily: '"IBM Plex Mono", Consolas, monospace',
              outline: 'none',
              boxSizing: 'border-box',
              transition: 'border-color 0.15s',
            }}
            onFocus={(e) => (e.currentTarget.style.borderColor = '#FF8800')}
            onBlur={(e) => (e.currentTarget.style.borderColor = '#2a2a2a')}
          />
        </div>
        <div
          style={{
            color: '#787878',
            fontSize: '9px',
            marginTop: '5px',
            letterSpacing: '0.3px',
          }}
        >
          {allNodes.length} NODES AVAILABLE
        </div>
      </div>

      {/* Node Categories */}
      <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
        {Array.from(filteredCategories.entries()).map(([category, categoryNodes]) => {
          const catConfig = CATEGORY_CONFIG[category] || {
            icon: <Workflow size={12} />,
            color: '#787878',
          };
          const isExpanded = expandedCategories.has(category);

          return (
            <div key={category}>
              {/* Category Header */}
              <button
                onClick={() => toggleCategory(category)}
                style={{
                  width: '100%',
                  padding: '7px 10px',
                  backgroundColor: '#0f0f0f',
                  border: 'none',
                  borderBottom: '1px solid #2a2a2a',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  cursor: 'pointer',
                  transition: 'background-color 0.15s',
                }}
                onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = '#1a1a1a')}
                onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = '#0f0f0f')}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '7px' }}>
                  <span style={{ color: catConfig.color, display: 'flex', alignItems: 'center' }}>
                    {catConfig.icon}
                  </span>
                  <span
                    style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: '#d4d4d4',
                      letterSpacing: '0.5px',
                    }}
                  >
                    {category.toUpperCase()}
                  </span>
                  <span
                    style={{
                      fontSize: '8px',
                      color: '#787878',
                      backgroundColor: '#1a1a1a',
                      border: '1px solid #2a2a2a',
                      borderRadius: '2px',
                      padding: '1px 4px',
                      letterSpacing: '0.3px',
                    }}
                  >
                    {categoryNodes.length}
                  </span>
                </div>
                <ChevronDown
                  size={11}
                  style={{
                    color: '#787878',
                    transform: isExpanded ? 'rotate(0deg)' : 'rotate(-90deg)',
                    transition: 'transform 0.15s',
                  }}
                />
              </button>

              {/* Category Nodes */}
              {isExpanded && (
                <div
                  style={{
                    padding: '4px 6px 6px',
                    backgroundColor: '#000000',
                    borderBottom: '1px solid #1a1a1a',
                  }}
                >
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
                        backgroundColor: '#0f0f0f',
                        border: `1px solid #2a2a2a`,
                        borderRadius: '2px',
                        padding: '6px 8px',
                        marginBottom: '3px',
                        cursor: 'grab',
                        transition: 'all 0.15s',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                      }}
                      onMouseEnter={(e) => {
                        e.currentTarget.style.backgroundColor = `${node.color}10`;
                        e.currentTarget.style.borderColor = `${node.color}50`;
                        e.currentTarget.style.borderLeftColor = node.color;
                        e.currentTarget.style.borderLeftWidth = '2px';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.backgroundColor = '#0f0f0f';
                        e.currentTarget.style.borderColor = '#2a2a2a';
                        e.currentTarget.style.borderLeftWidth = '1px';
                      }}
                    >
                      {/* Color indicator dot */}
                      <div
                        style={{
                          width: '6px',
                          height: '6px',
                          borderRadius: '50%',
                          backgroundColor: node.color,
                          flexShrink: 0,
                          boxShadow: `0 0 4px ${node.color}60`,
                        }}
                      />
                      {/* Node info */}
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div
                          style={{
                            color: '#ffffff',
                            fontSize: '10px',
                            fontWeight: 700,
                            letterSpacing: '0.3px',
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
                              color: '#787878',
                              fontSize: '8px',
                              letterSpacing: '0.2px',
                              whiteSpace: 'nowrap',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                              marginTop: '2px',
                            }}
                          >
                            {node.description.length > 38
                              ? `${node.description.substring(0, 38)}...`
                              : node.description}
                          </div>
                        )}
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
              padding: '32px 16px',
              textAlign: 'center',
            }}
          >
            <Search size={20} style={{ color: '#2a2a2a', margin: '0 auto 10px' }} />
            <div
              style={{
                fontSize: '9px',
                fontWeight: 700,
                color: '#787878',
                letterSpacing: '0.5px',
              }}
            >
              NO NODES FOUND
            </div>
            <div style={{ fontSize: '9px', color: '#4a4a4a', marginTop: '4px' }}>
              Try a different search term
            </div>
          </div>
        )}
      </div>

      {/* Footer */}
      <div
        style={{
          padding: '6px 10px',
          borderTop: '1px solid #2a2a2a',
          backgroundColor: '#0f0f0f',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '6px',
        }}
      >
        <div
          style={{
            width: '4px',
            height: '4px',
            borderRadius: '50%',
            backgroundColor: '#FF8800',
          }}
        />
        <span
          style={{
            color: '#4a4a4a',
            fontSize: '8px',
            letterSpacing: '0.5px',
            fontWeight: 700,
          }}
        >
          DRAG OR CLICK TO ADD
        </span>
      </div>
    </div>
  );
};

export default NodePalette;
