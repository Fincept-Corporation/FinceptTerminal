import React, { useState, useCallback, useMemo, useEffect } from 'react';
import ReactFlow, {
  Node,
  Edge,
  addEdge,
  Background,
  Controls,
  MiniMap,
  Connection,
  useNodesState,
  useEdgesState,
  MarkerType,
  NodeTypes,
  Handle,
  Position,
} from 'reactflow';
import 'reactflow/dist/style.css';
import {
  Plus,
  Play,
  Save,
  Download,
  Upload,
  Trash2,
  Database,
  Zap,
  TrendingUp,
  BarChart3,
  Brain,
  FileText,
  Edit3,
  X
} from 'lucide-react';
import MCPToolNode from './node-editor/MCPToolNode';
import { generateMCPNodeConfigs, MCPNodeConfig } from './node-editor/mcpNodeConfigs';

// Custom Node Component
const CustomNode = ({ data, id, selected }: any) => {
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
                background: '#0a0a0a',
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

const NODE_CONFIGS = [
  {
    type: 'data-source',
    label: 'Data Source',
    color: '#3b82f6',
    category: 'Input',
    hasInput: false,
    hasOutput: true,
  },
  {
    type: 'transformation',
    label: 'Transform',
    color: '#8b5cf6',
    category: 'Processing',
    hasInput: true,
    hasOutput: true,
  },
  {
    type: 'analysis',
    label: 'Analysis',
    color: '#10b981',
    category: 'Processing',
    hasInput: true,
    hasOutput: true,
  },
  {
    type: 'visualization',
    label: 'Visualization',
    color: '#f59e0b',
    category: 'Output',
    hasInput: true,
    hasOutput: false,
  },
  {
    type: 'agent',
    label: 'AI Agent',
    color: '#ec4899',
    category: 'Processing',
    hasInput: true,
    hasOutput: true,
  },
  {
    type: 'output',
    label: 'Output',
    color: '#06b6d4',
    category: 'Output',
    hasInput: true,
    hasOutput: false,
  },
];

export default function NodeEditorTab() {
  const [mcpNodeConfigs, setMcpNodeConfigs] = useState<MCPNodeConfig[]>([]);

  // Load MCP node configurations on mount
  useEffect(() => {
    const loadMCPNodes = async () => {
      try {
        const configs = await generateMCPNodeConfigs();
        setMcpNodeConfigs(configs);
        console.log('[NodeEditor] Loaded MCP node configs:', configs.length);
      } catch (error) {
        console.error('[NodeEditor] Failed to load MCP configs:', error);
      }
    };
    loadMCPNodes();
  }, []);

  const [nodes, setNodes, onNodesChange] = useNodesState([
    {
      id: '1',
      type: 'custom',
      position: { x: 100, y: 150 },
      data: {
        label: 'Market Data',
        nodeType: 'data-source',
        color: '#3b82f6',
        hasInput: false,
        hasOutput: true,
        onLabelChange: handleLabelChange,
      },
    },
    {
      id: '2',
      type: 'custom',
      position: { x: 400, y: 150 },
      data: {
        label: 'Normalize Data',
        nodeType: 'transformation',
        color: '#8b5cf6',
        hasInput: true,
        hasOutput: true,
        onLabelChange: handleLabelChange,
      },
    },
    {
      id: '3',
      type: 'custom',
      position: { x: 400, y: 280 },
      data: {
        label: 'Technical Analysis',
        nodeType: 'analysis',
        color: '#10b981',
        hasInput: true,
        hasOutput: true,
        onLabelChange: handleLabelChange,
      },
    },
    {
      id: '4',
      type: 'custom',
      position: { x: 700, y: 150 },
      data: {
        label: 'Chart Output',
        nodeType: 'visualization',
        color: '#f59e0b',
        hasInput: true,
        hasOutput: false,
        onLabelChange: handleLabelChange,
      },
    },
    {
      id: '5',
      type: 'custom',
      position: { x: 700, y: 280 },
      data: {
        label: 'Trading Agent',
        nodeType: 'agent',
        color: '#ec4899',
        hasInput: true,
        hasOutput: true,
        onLabelChange: handleLabelChange,
      },
    },
  ]);

  const [edges, setEdges, onEdgesChange] = useEdgesState([
    {
      id: 'e1-2',
      source: '1',
      target: '2',
      animated: true,
      style: { stroke: '#ea580c', strokeWidth: 2 },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#ea580c' },
    },
    {
      id: 'e1-3',
      source: '1',
      target: '3',
      animated: true,
      style: { stroke: '#ea580c', strokeWidth: 2 },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#ea580c' },
    },
    {
      id: 'e2-4',
      source: '2',
      target: '4',
      animated: true,
      style: { stroke: '#ea580c', strokeWidth: 2 },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#ea580c' },
    },
    {
      id: 'e3-5',
      source: '3',
      target: '5',
      animated: true,
      style: { stroke: '#ea580c', strokeWidth: 2 },
      markerEnd: { type: MarkerType.ArrowClosed, color: '#ea580c' },
    },
  ]);

  const [selectedNodes, setSelectedNodes] = useState<string[]>([]);
  const [showNodeMenu, setShowNodeMenu] = useState(false);

  const nodeTypes: NodeTypes = useMemo(() => ({
    custom: CustomNode,
    'mcp-tool': MCPToolNode
  }), []);

  // Handle label change
  function handleLabelChange(nodeId: string, newLabel: string) {
    setNodes((nds) =>
      nds.map((node) => {
        if (node.id === nodeId) {
          return {
            ...node,
            data: {
              ...node.data,
              label: newLabel,
            },
          };
        }
        return node;
      })
    );
  }

  // Handle connection
  const onConnect = useCallback(
    (params: Connection) => {
      const newEdge = {
        ...params,
        animated: true,
        style: { stroke: '#ea580c', strokeWidth: 2 },
        markerEnd: { type: MarkerType.ArrowClosed, color: '#ea580c' },
      };
      setEdges((eds) => addEdge(newEdge, eds));
    },
    [setEdges]
  );

  // Add new node (handles both regular nodes and MCP tool nodes)
  const addNode = (config: any) => {
    const isMCPNode = config.type === 'mcp-tool';

    const newNode: Node = {
      id: `node_${Date.now()}`,
      type: isMCPNode ? 'mcp-tool' : 'custom',
      position: { x: Math.random() * 400 + 100, y: Math.random() * 300 + 100 },
      data: isMCPNode ? {
        serverId: config.serverId,
        toolName: config.toolName,
        label: config.label,
        parameters: {},
        onParameterChange: (params: Record<string, any>) => {
          console.log('[NodeEditor] MCP tool parameters changed:', params);
        },
        onExecute: (result: any) => {
          console.log('[NodeEditor] MCP tool executed with result:', result);
        },
      } : {
        label: config.label,
        nodeType: config.type,
        color: config.color,
        hasInput: config.hasInput,
        hasOutput: config.hasOutput,
        onLabelChange: handleLabelChange,
      },
    };
    setNodes((nds) => [...nds, newNode]);
    setShowNodeMenu(false);
  };

  // Delete selected nodes
  const deleteSelectedNodes = useCallback(() => {
    if (selectedNodes.length > 0) {
      setNodes((nds) => nds.filter((node) => !selectedNodes.includes(node.id)));
      setEdges((eds) =>
        eds.filter(
          (edge) =>
            !selectedNodes.includes(edge.source) &&
            !selectedNodes.includes(edge.target)
        )
      );
      setSelectedNodes([]);
    }
  }, [selectedNodes, setNodes, setEdges]);

  // Handle node selection
  const onSelectionChange = useCallback((params: any) => {
    setSelectedNodes(params.nodes.map((node: Node) => node.id));
  }, []);

  // Delete selected edges
  const deleteSelectedEdges = useCallback(() => {
    setEdges((eds) => eds.filter((edge) => !edge.selected));
  }, [setEdges]);

  // Handle keyboard shortcuts
  const handleKeyDown = useCallback(
    (event: KeyboardEvent) => {
      if (event.key === 'Delete' || event.key === 'Backspace') {
        deleteSelectedNodes();
        deleteSelectedEdges();
      }
    },
    [deleteSelectedNodes, deleteSelectedEdges]
  );

  React.useEffect(() => {
    window.addEventListener('keydown', handleKeyDown);
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
    };
  }, [handleKeyDown]);

  // Export workflow
  const exportWorkflow = () => {
    const workflow = {
      nodes: nodes,
      edges: edges,
    };
    const dataStr = JSON.stringify(workflow, null, 2);
    const dataUri = `data:application/json;charset=utf-8,${encodeURIComponent(dataStr)}`;
    const exportFileDefaultName = `workflow_${Date.now()}.json`;

    const linkElement = document.createElement('a');
    linkElement.setAttribute('href', dataUri);
    linkElement.setAttribute('download', exportFileDefaultName);
    linkElement.click();
  };

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: '#0a0a0a',
        display: 'flex',
        flexDirection: 'column',
      }}
    >
      {/* Toolbar */}
      <div
        style={{
          backgroundColor: '#1a1a1a',
          borderBottom: '1px solid #2d2d2d',
          padding: '8px 12px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          flexWrap: 'wrap',
          flexShrink: 0,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span
            style={{
              color: '#ea580c',
              fontSize: '12px',
              fontWeight: 'bold',
              letterSpacing: '0.5px',
            }}
          >
            NODE EDITOR
          </span>
          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
        </div>

        <button
          onClick={() => setShowNodeMenu(!showNodeMenu)}
          style={{
            backgroundColor: '#ea580c',
            color: 'white',
            border: 'none',
            padding: '6px 12px',
            fontSize: '11px',
            cursor: 'pointer',
            borderRadius: '3px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontWeight: 'bold',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = '#dc2626')}
          onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = '#ea580c')}
        >
          <Plus size={14} />
          ADD NODE
        </button>

        <button
          style={{
            backgroundColor: 'transparent',
            color: '#10b981',
            border: '1px solid #10b981',
            padding: '6px 12px',
            fontSize: '11px',
            cursor: 'pointer',
            borderRadius: '3px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#10b981';
            e.currentTarget.style.color = 'white';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = '#10b981';
          }}
        >
          <Play size={14} />
          EXECUTE
        </button>

        <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>

        <button
          style={{
            backgroundColor: 'transparent',
            color: '#a3a3a3',
            border: '1px solid #404040',
            padding: '6px 12px',
            fontSize: '11px',
            cursor: 'pointer',
            borderRadius: '3px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#2d2d2d';
            e.currentTarget.style.color = 'white';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = '#a3a3a3';
          }}
        >
          <Save size={14} />
          SAVE
        </button>

        <button
          style={{
            backgroundColor: 'transparent',
            color: '#a3a3a3',
            border: '1px solid #404040',
            padding: '6px 12px',
            fontSize: '11px',
            cursor: 'pointer',
            borderRadius: '3px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#2d2d2d';
            e.currentTarget.style.color = 'white';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = '#a3a3a3';
          }}
        >
          <Upload size={14} />
          IMPORT
        </button>

        <button
          onClick={exportWorkflow}
          style={{
            backgroundColor: 'transparent',
            color: '#a3a3a3',
            border: '1px solid #404040',
            padding: '6px 12px',
            fontSize: '11px',
            cursor: 'pointer',
            borderRadius: '3px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = '#2d2d2d';
            e.currentTarget.style.color = 'white';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = '#a3a3a3';
          }}
        >
          <Download size={14} />
          EXPORT
        </button>

        {selectedNodes.length > 0 && (
          <>
            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
            <button
              onClick={deleteSelectedNodes}
              style={{
                backgroundColor: 'transparent',
                color: '#ef4444',
                border: '1px solid #ef4444',
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = '#ef4444';
                e.currentTarget.style.color = 'white';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = '#ef4444';
              }}
            >
              <Trash2 size={14} />
              DELETE ({selectedNodes.length})
            </button>
          </>
        )}

        <div
          style={{
            marginLeft: 'auto',
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
          }}
        >
          <span style={{ color: '#737373', fontSize: '10px' }}>
            NODES: {nodes.length}
          </span>
          <span style={{ color: '#737373', fontSize: '10px' }}>
            CONNECTIONS: {edges.length}
          </span>
        </div>
      </div>

      {/* Node Menu */}
      {showNodeMenu && (
        <div
          style={{
            position: 'absolute',
            top: '60px',
            left: '12px',
            backgroundColor: '#1a1a1a',
            border: '1px solid #2d2d2d',
            borderRadius: '4px',
            padding: '12px',
            zIndex: 1000,
            minWidth: '250px',
            maxHeight: '400px',
            overflow: 'auto',
            boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
          }}
        >
          <div
            style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '12px',
            }}
          >
            <div
              style={{
                color: '#a3a3a3',
                fontSize: '11px',
                fontWeight: 'bold',
                letterSpacing: '0.5px',
              }}
            >
              SELECT NODE TYPE
            </div>
            <button
              onClick={() => setShowNodeMenu(false)}
              style={{
                background: 'transparent',
                border: 'none',
                color: '#a3a3a3',
                cursor: 'pointer',
                padding: '2px',
              }}
            >
              <X size={16} />
            </button>
          </div>

          {['Input', 'Processing', 'Output'].map((category) => (
            <div key={category} style={{ marginBottom: '12px' }}>
              <div
                style={{
                  color: '#737373',
                  fontSize: '10px',
                  marginBottom: '6px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                }}
              >
                {category}
              </div>
              {NODE_CONFIGS.filter((t) => t.category === category).map((config) => (
                <button
                  key={config.type}
                  onClick={() => addNode(config)}
                  style={{
                    backgroundColor: '#0a0a0a',
                    color: config.color,
                    border: `1px solid ${config.color}40`,
                    padding: '8px 12px',
                    fontSize: '11px',
                    cursor: 'pointer',
                    borderRadius: '3px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    width: '100%',
                    marginBottom: '6px',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = `${config.color}20`;
                    e.currentTarget.style.borderColor = config.color;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = '#0a0a0a';
                    e.currentTarget.style.borderColor = `${config.color}40`;
                  }}
                >
                  {config.type === 'data-source' && <Database size={16} />}
                  {config.type === 'transformation' && <Zap size={16} />}
                  {config.type === 'analysis' && <TrendingUp size={16} />}
                  {config.type === 'visualization' && <BarChart3 size={16} />}
                  {config.type === 'agent' && <Brain size={16} />}
                  {config.type === 'output' && <FileText size={16} />}
                  {config.label}
                </button>
              ))}
            </div>
          ))}

          {/* MCP Tools Section */}
          {mcpNodeConfigs.length > 0 && (
            <div style={{ marginBottom: '12px' }}>
              <div
                style={{
                  color: '#FFA500',
                  fontSize: '10px',
                  marginBottom: '6px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  fontWeight: 'bold',
                }}
              >
                ⚡ MCP TOOLS ({mcpNodeConfigs.length})
              </div>
              {mcpNodeConfigs.map((config) => (
                <button
                  key={config.id}
                  onClick={() => addNode(config)}
                  style={{
                    backgroundColor: '#0a0a0a',
                    color: '#FFA500',
                    border: '1px solid #FFA50040',
                    padding: '8px 12px',
                    fontSize: '11px',
                    cursor: 'pointer',
                    borderRadius: '3px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    width: '100%',
                    marginBottom: '6px',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#FFA50020';
                    e.currentTarget.style.borderColor = '#FFA500';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = '#0a0a0a';
                    e.currentTarget.style.borderColor = '#FFA50040';
                  }}
                >
                  <span style={{ fontSize: '14px' }}>{config.icon}</span>
                  <div style={{ flex: 1, textAlign: 'left' }}>
                    <div>{config.label}</div>
                    <div style={{ fontSize: '9px', color: '#787878' }}>
                      {config.serverId}
                    </div>
                  </div>
                </button>
              ))}
            </div>
          )}
        </div>
      )}

      {/* React Flow Canvas */}
      <div style={{ flex: 1, position: 'relative' }}>
        <ReactFlow
          nodes={nodes}
          edges={edges}
          onNodesChange={onNodesChange}
          onEdgesChange={onEdgesChange}
          onConnect={onConnect}
          onSelectionChange={onSelectionChange}
          nodeTypes={nodeTypes}
          fitView
          style={{ background: '#0a0a0a' }}
          defaultEdgeOptions={{
            animated: true,
            style: { stroke: '#ea580c', strokeWidth: 2 },
          }}
        >
          <Background
            color="#2d2d2d"
            gap={20}
            size={1}
            style={{ backgroundColor: '#0a0a0a' }}
          />
          <Controls
            style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '4px',
            }}
          />
          <MiniMap
            style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
            }}
            nodeColor={(node) => {
              return node.data.color || '#3b82f6';
            }}
          />
        </ReactFlow>
      </div>

      {/* Bottom Info Bar */}
      <div
        style={{
          backgroundColor: '#1a1a1a',
          borderTop: '1px solid #2d2d2d',
          padding: '6px 12px',
          display: 'flex',
          alignItems: 'center',
          gap: '16px',
          fontSize: '10px',
          color: '#737373',
          flexShrink: 0,
        }}
      >
        <span>💡 Drag nodes to move | Drag from handles to connect | Select & Delete to remove</span>
        {selectedNodes.length > 0 && (
          <>
            <div style={{ width: '1px', height: '14px', backgroundColor: '#404040' }}></div>
            <span style={{ color: '#ea580c' }}>
              Selected: {selectedNodes.length} node(s)
            </span>
          </>
        )}
        <div
          style={{
            marginLeft: 'auto',
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
          }}
        >
          <span>Scroll to zoom | Right-click drag to pan</span>
        </div>
      </div>
    </div>
  );
}
