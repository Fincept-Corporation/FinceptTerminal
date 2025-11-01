import { useTerminalTheme } from '@/contexts/ThemeContext';
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
  X,
  Monitor
} from 'lucide-react';
import MCPToolNode from './node-editor/MCPToolNode';
import { generateMCPNodeConfigs, MCPNodeConfig } from './node-editor/mcpNodeConfigs';
import PythonAgentNode from './node-editor/PythonAgentNode';
import ResultsDisplayNode from './node-editor/ResultsDisplayNode';
import AgentMediatorNode from './node-editor/AgentMediatorNode';
import { pythonAgentService, AgentMetadata } from '@/services/pythonAgentService';
import { nodeExecutionManager } from '@/services/nodeExecutionManager';

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
                background: 'colors.panel',
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
    type: 'agent-mediator',
    label: 'Agent Mediator',
    color: '#3b82f6',
    category: 'Processing',
    hasInput: true,
    hasOutput: true,
  },
  {
    type: 'results-display',
    label: 'Results Display',
    color: '#f59e0b',
    category: 'Output',
    hasInput: true,
    hasOutput: false,
  },
];

export default function NodeEditorTab() {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [mcpNodeConfigs, setMcpNodeConfigs] = useState<MCPNodeConfig[]>([]);
  const [agentConfigs, setAgentConfigs] = useState<AgentMetadata[]>([]);

  // Load saved workflow from localStorage or start with blank slate
  const loadSavedWorkflow = useCallback(() => {
    try {
      const saved = localStorage.getItem('nodeEditorWorkflow');
      if (saved) {
        const workflow = JSON.parse(saved);
        console.log('[NodeEditor] Loaded saved workflow:', workflow.nodes.length, 'nodes');
        return workflow;
      }
    } catch (error) {
      console.error('[NodeEditor] Failed to load saved workflow:', error);
    }
    return { nodes: [], edges: [] };
  }, []);

  const [nodes, setNodes, onNodesChange] = useNodesState(loadSavedWorkflow().nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(loadSavedWorkflow().edges);

  // Auto-save workflow to localStorage whenever nodes or edges change
  useEffect(() => {
    const saveWorkflow = () => {
      try {
        const workflow = { nodes, edges };
        localStorage.setItem('nodeEditorWorkflow', JSON.stringify(workflow));
        console.log('[NodeEditor] Auto-saved workflow:', nodes.length, 'nodes,', edges.length, 'edges');
      } catch (error) {
        console.error('[NodeEditor] Failed to save workflow:', error);
      }
    };

    // Debounce saves to avoid too frequent writes
    const timeoutId = setTimeout(saveWorkflow, 500);
    return () => clearTimeout(timeoutId);
  }, [nodes, edges]);

  // Load MCP node configurations and Python agents on mount
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

    const loadAgents = async () => {
      try {
        const agents = await pythonAgentService.getAvailableAgents();
        setAgentConfigs(agents);
        console.log('[NodeEditor] Loaded Python agents:', agents.length);
      } catch (error) {
        console.error('[NodeEditor] Failed to load agents:', error);
      }
    };

    loadMCPNodes();
    loadAgents();
  }, []);

  const [selectedNodes, setSelectedNodes] = useState<string[]>([]);
  const [showNodeMenu, setShowNodeMenu] = useState(false);

  const nodeTypes: NodeTypes = useMemo(() => ({
    custom: CustomNode,
    'mcp-tool': MCPToolNode,
    'python-agent': PythonAgentNode,
    'results-display': ResultsDisplayNode,
    'agent-mediator': AgentMediatorNode
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

  // Add new node (handles regular nodes, MCP tool nodes, Python agent nodes, and agent mediator)
  const addNode = (config: any) => {
    const isMCPNode = config.type === 'mcp-tool';
    const isPythonAgent = config.type === 'python-agent';
    const isResultsDisplay = config.type === 'results-display';
    const isAgentMediator = config.type === 'agent-mediator';

    const newNode: Node = {
      id: `node_${Date.now()}`,
      type: isPythonAgent ? 'python-agent' : (isMCPNode ? 'mcp-tool' : (isResultsDisplay ? 'results-display' : (isAgentMediator ? 'agent-mediator' : 'custom'))),
      position: { x: Math.random() * 400 + 100, y: Math.random() * 300 + 100 },
      data: isPythonAgent ? {
        agentType: config.id,  // Use 'id' field which contains agent_type like 'warren_buffett'
        agentCategory: config.category,
        label: config.name,
        icon: config.icon,
        color: config.color,
        parameters: config.parameters.reduce((acc: any, param: any) => {
          if (param.default_value !== null && param.default_value !== undefined) {
            acc[param.name] = param.default_value;
          }
          return acc;
        }, {}),
        selectedLLM: 'active', // Default to active provider
        status: 'idle',
        onExecute: async (nodeId: string) => {
          console.log('[NodeEditor] Executing single Python agent node:', config.id);
          // Execute just this node
          try {
            setNodes((nds) =>
              nds.map((n) => n.id === nodeId ? { ...n, data: { ...n.data, status: 'running' } } : n)
            );

            const result = await pythonAgentService.executeAgent(
              config.id,
              config.parameters.reduce((acc: any, param: any) => {
                if (param.default_value !== null && param.default_value !== undefined) {
                  acc[param.name] = param.default_value;
                }
                return acc;
              }, {}),
              {}
            );

            setNodes((nds) =>
              nds.map((n) =>
                n.id === nodeId
                  ? {
                      ...n,
                      data: {
                        ...n.data,
                        status: result.success ? 'completed' : 'error',
                        result: result.success ? result.data : undefined,
                        error: result.success ? undefined : result.error,
                      },
                    }
                  : n
              )
            );
          } catch (error: any) {
            setNodes((nds) =>
              nds.map((n) =>
                n.id === nodeId ? { ...n, data: { ...n.data, status: 'error', error: error.message } } : n
              )
            );
          }
        },
        onParameterChange: (params: Record<string, any>) => {
          console.log('[NodeEditor] Agent parameters changed:', params);
        },
        onLLMChange: (provider: string) => {
          setNodes((nds) =>
            nds.map((n) =>
              n.id === newNode.id
                ? {
                    ...n,
                    data: {
                      ...n.data,
                      selectedLLM: provider,
                    },
                  }
                : n
            )
          );
        },
      } : (isMCPNode ? {
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
      } : (isResultsDisplay ? {
        label: config.label,
        color: config.color,
        inputData: undefined,
      } : (isAgentMediator ? {
        label: config.label,
        selectedProvider: undefined, // Will use active provider by default
        customPrompt: undefined,
        status: 'idle',
        result: undefined,
        error: undefined,
        inputData: undefined,
        onExecute: (nodeId: string) => {
          console.log('[NodeEditor] Agent mediator execute clicked:', nodeId);
          // Workflow execution will handle this
        },
        onConfigChange: (newConfig: any) => {
          setNodes((nds) =>
            nds.map((n) =>
              n.id === newNode.id
                ? {
                    ...n,
                    data: {
                      ...n.data,
                      selectedProvider: newConfig.selectedProvider,
                      customPrompt: newConfig.customPrompt,
                    },
                  }
                : n
            )
          );
        },
      } : {
        label: config.label,
        nodeType: config.type,
        color: config.color,
        hasInput: config.hasInput,
        hasOutput: config.hasOutput,
        onLabelChange: handleLabelChange,
      }))),
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

  // Save workflow manually
  const saveWorkflow = useCallback(() => {
    try {
      const workflow = { nodes, edges };
      localStorage.setItem('nodeEditorWorkflow', JSON.stringify(workflow));
      console.log('[NodeEditor] Manually saved workflow:', nodes.length, 'nodes,', edges.length, 'edges');
      // Could add visual feedback here
    } catch (error) {
      console.error('[NodeEditor] Failed to save workflow:', error);
    }
  }, [nodes, edges]);

  // Export workflow to JSON file
  const exportWorkflow = useCallback(() => {
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
  }, [nodes, edges]);

  // Import workflow from JSON file
  const importWorkflow = useCallback(() => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = (e: any) => {
      const file = e.target.files[0];
      if (file) {
        const reader = new FileReader();
        reader.onload = (event: any) => {
          try {
            const workflow = JSON.parse(event.target.result);
            setNodes(workflow.nodes || []);
            setEdges(workflow.edges || []);
            console.log('[NodeEditor] Imported workflow:', workflow.nodes?.length || 0, 'nodes');
          } catch (error) {
            console.error('[NodeEditor] Failed to import workflow:', error);
          }
        };
        reader.readAsText(file);
      }
    };
    input.click();
  }, [setNodes, setEdges]);

  // Clear workflow (start fresh)
  const clearWorkflow = useCallback(() => {
    setNodes([]);
    setEdges([]);
    localStorage.removeItem('nodeEditorWorkflow');
    console.log('[NodeEditor] Cleared workflow');
  }, [setNodes, setEdges]);

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        backgroundColor: 'colors.panel',
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
          onClick={async () => {
            try {
              console.log('[NodeEditor] Starting workflow execution...');
              await nodeExecutionManager.executeWorkflow(
                nodes,
                edges,
                (nodeId, status, result) => {
                  console.log(`[NodeEditor] Node ${nodeId} status: ${status}`, result);
                  // Update node status
                  setNodes((nds) =>
                    nds.map((node) => {
                      if (node.id === nodeId) {
                        // For results-display nodes, update inputData
                        if (node.type === 'results-display') {
                          console.log('[NodeEditor] Updating results-display node:', nodeId, 'with data:', result);
                          console.log('[NodeEditor] Status:', status);
                          // Force update to trigger re-render
                          const currentData = node.data as any;
                          return {
                            ...node,
                            data: {
                              ...node.data,
                              inputData: status === 'completed' ? result : (status === 'error' ? undefined : currentData.inputData),
                              status,
                            },
                          };
                        }
                        // For other nodes, update status and result
                        return {
                          ...node,
                          data: {
                            ...node.data,
                            status,
                            result: status === 'completed' ? result : undefined,
                            error: status === 'error' ? result : undefined,
                          },
                        };
                      }
                      return node;
                    })
                  );
                }
              );
              console.log('[NodeEditor] Workflow execution completed');
            } catch (error: any) {
              console.error('[NodeEditor] Workflow execution failed:', error);
              // Don't use alert - just log the error
            }
          }}
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
          onClick={saveWorkflow}
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
          title="Save workflow to browser storage"
        >
          <Save size={14} />
          SAVE
        </button>

        <button
          onClick={importWorkflow}
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
          title="Import workflow from file"
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

        <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>

        <button
          onClick={() => {
            if (window.confirm('Are you sure you want to clear all nodes? This cannot be undone.')) {
              clearWorkflow();
            }
          }}
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
          title="Clear all nodes and connections"
        >
          <Trash2 size={14} />
          CLEAR ALL
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
                    backgroundColor: 'colors.panel',
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
                    e.currentTarget.style.backgroundColor = 'colors.panel';
                    e.currentTarget.style.borderColor = `${config.color}40`;
                  }}
                >
                  {config.type === 'data-source' && <Database size={16} />}
                  {config.type === 'transformation' && <Zap size={16} />}
                  {config.type === 'analysis' && <TrendingUp size={16} />}
                  {config.type === 'results-display' && <Monitor size={16} />}
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
                  color: 'colors.primary',
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
                    backgroundColor: 'colors.panel',
                    color: 'colors.primary',
                    border: '1px solid colors.primary40',
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
                    e.currentTarget.style.backgroundColor = 'colors.primary20';
                    e.currentTarget.style.borderColor = 'colors.primary';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'colors.panel';
                    e.currentTarget.style.borderColor = 'colors.primary40';
                  }}
                >
                  <span style={{ fontSize: '14px' }}>{config.icon}</span>
                  <div style={{ flex: 1, textAlign: 'left' }}>
                    <div>{config.label}</div>
                    <div style={{ fontSize: '9px', color: 'colors.textMuted' }}>
                      {config.serverId}
                    </div>
                  </div>
                </button>
              ))}
            </div>
          )}

          {/* Python Agents Section */}
          {agentConfigs.length > 0 && (
            <div style={{ marginBottom: '12px' }}>
              <div
                style={{
                  color: '#22c55e',
                  fontSize: '10px',
                  marginBottom: '6px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px',
                  fontWeight: 'bold',
                }}
              >
                🤖 PYTHON AGENTS ({agentConfigs.length})
              </div>
              {['trader', 'hedge-fund', 'economic', 'geopolitics'].map((category) => {
                const categoryAgents = agentConfigs.filter(a => a.category === category);
                if (categoryAgents.length === 0) return null;

                return (
                  <div key={category} style={{ marginBottom: '8px' }}>
                    <div style={{ color: '#737373', fontSize: '9px', marginBottom: '4px', paddingLeft: '4px' }}>
                      {category.toUpperCase()}
                    </div>
                    {categoryAgents.map((agent) => (
                      <button
                        key={agent.id}
                        onClick={() => addNode({ ...agent, type: 'python-agent' })}
                        style={{
                          backgroundColor: 'colors.panel',
                          color: agent.color,
                          border: `1px solid ${agent.color}40`,
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
                          e.currentTarget.style.backgroundColor = `${agent.color}20`;
                          e.currentTarget.style.borderColor = agent.color;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = 'colors.panel';
                          e.currentTarget.style.borderColor = `${agent.color}40`;
                        }}
                      >
                        <span style={{ fontSize: '14px' }}>{agent.icon}</span>
                        <div style={{ flex: 1, textAlign: 'left' }}>
                          <div>{agent.name}</div>
                          <div style={{ fontSize: '9px', color: '#737373' }}>
                            {agent.description.substring(0, 40)}...
                          </div>
                        </div>
                      </button>
                    ))}
                  </div>
                );
              })}
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
          style={{ background: 'colors.panel' }}
          defaultEdgeOptions={{
            animated: true,
            style: { stroke: '#ea580c', strokeWidth: 2 },
          }}
        >
          <Background
            color="#2d2d2d"
            gap={20}
            size={1}
            style={{ backgroundColor: 'colors.panel' }}
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
