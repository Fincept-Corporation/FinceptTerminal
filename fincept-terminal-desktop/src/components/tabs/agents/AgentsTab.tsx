import React, { useState, useCallback, useMemo } from 'react';
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
  Settings,
  Pause,
  X,
  Filter,
  Clock,
  Activity,
} from 'lucide-react';
import { AGENT_CONFIGS, AGENT_CATEGORIES, getAgentConfig } from './agentConfigs';
import { AgentConfig, AgentStatus, AgentCategory } from './types';
import { AgentMCPConfig } from './agentMCPExecutor';

// Custom Agent Node Component with Dynamic Handles
const AgentNode = ({ data, id, selected }: any) => {
  const [isEditing, setIsEditing] = useState(false);
  const [label, setLabel] = useState(data.label);
  const [showConfig, setShowConfig] = useState(false);
  const [isHovered, setIsHovered] = useState(false);

  const handleLabelSave = () => {
    data.onLabelChange(id, label);
    setIsEditing(false);
  };

  const getStatusColor = (status: AgentStatus) => {
    switch (status) {
      case 'running':
        return '#10b981';
      case 'completed':
        return '#3b82f6';
      case 'error':
        return '#ef4444';
      case 'paused':
        return '#f59e0b';
      default:
        return '#6b7280';
    }
  };

  const getStatusIcon = (status: AgentStatus) => {
    switch (status) {
      case 'running':
        return '⚡';
      case 'completed':
        return '✓';
      case 'error':
        return '✗';
      case 'paused':
        return '⏸️';
      default:
        return '○';
    }
  };

  // DYNAMIC HANDLE GENERATION
  // Calculate actual connections to this node
  const connectedInputs = data.connectedInputs || 0;
  const connectedOutputs = data.connectedOutputs || 0;

  // Minimum handles (always show at least this many)
  const minHandles = 2;

  // Base handle count (shown when not hovered)
  const baseInputHandles = Math.max(minHandles, connectedInputs + 1);
  const baseOutputHandles = Math.max(minHandles, connectedOutputs + 1);

  // Expanded handle count (shown when hovered or has many connections)
  const expandedInputHandles = isHovered ? Math.max(baseInputHandles, 5) : baseInputHandles;
  const expandedOutputHandles = isHovered ? Math.max(baseOutputHandles, 5) : baseOutputHandles;

  // Use expanded counts for rendering
  const inputHandleCount = expandedInputHandles;
  const outputHandleCount = expandedOutputHandles;

  return (
    <div
      onMouseEnter={() => setIsHovered(true)}
      onMouseLeave={() => setIsHovered(false)}
      style={{
        background: selected ? '#2d2d2d' : '#1a1a1a',
        border: `2px solid ${selected ? data.color : `${data.color}80`}`,
        borderRadius: '8px',
        minWidth: '200px',
        boxShadow: selected
          ? `0 0 24px ${data.color}80, 0 6px 16px rgba(0,0,0,0.6)`
          : `0 3px 10px rgba(0,0,0,0.4)`,
        transition: 'all 0.2s',
        position: 'relative',
      }}
    >
      {/* Multiple Input Handles */}
      {data.hasInput && (
        <>
          {Array.from({ length: inputHandleCount }).map((_, index) => {
            const totalHandles = inputHandleCount;
            const topOffset = ((index + 1) * 100) / (totalHandles + 1);

            return (
              <Handle
                key={`input-${index}`}
                type="target"
                position={Position.Left}
                id={`input-${index}`}
                style={{
                  background: data.color,
                  width: 12,
                  height: 12,
                  border: '2px solid #1a1a1a',
                  boxShadow: `0 0 10px ${data.color}`,
                  top: `${topOffset}%`,
                  transform: 'translateY(-50%)',
                }}
                title={`Input ${index + 1}`}
              />
            );
          })}
        </>
      )}

      {/* Status Indicator */}
      <div
        style={{
          position: 'absolute',
          top: '-8px',
          right: '-8px',
          background: getStatusColor(data.status),
          borderRadius: '50%',
          width: '24px',
          height: '24px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          fontSize: '12px',
          border: '2px solid #1a1a1a',
          boxShadow: '0 2px 8px rgba(0,0,0,0.5)',
        }}
      >
        {getStatusIcon(data.status)}
      </div>

      {/* Node Header */}
      <div
        style={{
          background: `${data.color}25`,
          borderBottom: `1px solid ${data.color}50`,
          padding: '10px 14px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: '8px',
          color: data.color,
          borderTopLeftRadius: '6px',
          borderTopRightRadius: '6px',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flex: 1 }}>
          <span style={{ fontSize: '18px' }}>{data.icon || '🤖'}</span>
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
                fontSize: '12px',
                padding: '3px 8px',
                borderRadius: '4px',
                outline: 'none',
                fontWeight: 'bold',
                width: '140px',
              }}
            />
          ) : (
            <span
              style={{
                fontSize: '12px',
                fontWeight: 'bold',
                letterSpacing: '0.3px',
                cursor: 'pointer',
              }}
              onDoubleClick={() => setIsEditing(true)}
            >
              {data.label.toUpperCase()}
            </span>
          )}
        </div>
        <button
          onClick={() => setShowConfig(!showConfig)}
          style={{
            background: 'transparent',
            border: 'none',
            color: data.color,
            cursor: 'pointer',
            padding: '4px',
            display: 'flex',
            alignItems: 'center',
            borderRadius: '4px',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.background = `${data.color}30`)}
          onMouseLeave={(e) => (e.currentTarget.style.background = 'transparent')}
          title="Configure agent"
        >
          <Settings size={14} />
        </button>
      </div>

      {/* Node Body */}
      <div
        style={{
          padding: '12px 14px',
          display: 'flex',
          flexDirection: 'column',
          gap: '6px',
          color: '#a3a3a3',
          fontSize: '10px',
        }}
      >
        <div style={{ color: '#d4d4d4', fontSize: '11px' }}>
          Type: <span style={{ color: data.color }}>{data.agentType}</span>
        </div>
        <div>
          Status: <span style={{ color: getStatusColor(data.status) }}>{data.status}</span>
        </div>
        {data.mcpConfig && data.mcpConfig.enabled && (
          <div style={{
            backgroundColor: '#FFA50020',
            border: '1px solid #FFA500',
            borderRadius: '3px',
            padding: '4px 6px',
            fontSize: '9px',
            color: '#FFA500',
            fontWeight: 'bold',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            marginTop: '4px'
          }}>
            <span>⚡</span>
            <span>MCP: {data.mcpConfig.allowedServers.length} servers</span>
          </div>
        )}
        {data.hasInput && data.hasOutput && (
          <div style={{ display: 'flex', gap: '10px', fontSize: '9px', marginTop: '4px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={{ color: '#84cc16' }}>📥</span>
              <span style={{ color: '#737373' }}>
                <span style={{
                  backgroundColor: connectedInputs > 0 ? '#84cc16' : '#404040',
                  color: connectedInputs > 0 ? 'black' : '#737373',
                  padding: '2px 6px',
                  borderRadius: '3px',
                  fontWeight: 'bold',
                  fontSize: '9px'
                }}>
                  {connectedInputs}
                </span>
                <span style={{ marginLeft: '2px' }}>/{inputHandleCount}</span>
              </span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <span style={{ color: '#3b82f6' }}>📤</span>
              <span style={{ color: '#737373' }}>
                <span style={{
                  backgroundColor: connectedOutputs > 0 ? '#3b82f6' : '#404040',
                  color: connectedOutputs > 0 ? 'white' : '#737373',
                  padding: '2px 6px',
                  borderRadius: '3px',
                  fontWeight: 'bold',
                  fontSize: '9px'
                }}>
                  {connectedOutputs}
                </span>
                <span style={{ marginLeft: '2px' }}>/{outputHandleCount}</span>
              </span>
            </div>
          </div>
        )}
        {isHovered && (
          <div style={{
            fontSize: '8px',
            color: '#fbbf24',
            marginTop: '4px',
            fontStyle: 'italic'
          }}>
            💡 Hover to see more handles
          </div>
        )}
        {data.lastRun && (
          <div style={{ fontSize: '9px', color: '#737373' }}>
            Last run: {new Date(data.lastRun).toLocaleTimeString()}
          </div>
        )}
        {data.error && (
          <div style={{ color: '#ef4444', fontSize: '9px', marginTop: '4px' }}>
            Error: {data.error}
          </div>
        )}
      </div>

      {/* Multiple Output Handles */}
      {data.hasOutput && (
        <>
          {Array.from({ length: outputHandleCount }).map((_, index) => {
            const totalHandles = outputHandleCount;
            const topOffset = ((index + 1) * 100) / (totalHandles + 1);

            return (
              <Handle
                key={`output-${index}`}
                type="source"
                position={Position.Right}
                id={`output-${index}`}
                style={{
                  background: data.color,
                  width: 12,
                  height: 12,
                  border: '2px solid #1a1a1a',
                  boxShadow: `0 0 10px ${data.color}`,
                  top: `${topOffset}%`,
                  transform: 'translateY(-50%)',
                }}
                title={`Output ${index + 1}`}
              />
            );
          })}
        </>
      )}
    </div>
  );
};

export default function AgentsTab() {
  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);
  const [selectedNodes, setSelectedNodes] = useState<string[]>([]);
  const [showAgentMenu, setShowAgentMenu] = useState(false);
  const [selectedCategory, setSelectedCategory] = useState<AgentCategory | 'all'>('all' as const);
  const [isExecuting, setIsExecuting] = useState(false);
  const [workflowName, setWorkflowName] = useState('Untitled Workflow');

  const nodeTypes: NodeTypes = useMemo(() => ({ agent: AgentNode }), []);

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

  // Handle parameter change
  function handleParameterChange(nodeId: string, paramName: string, value: any) {
    setNodes((nds) =>
      nds.map((node) => {
        if (node.id === nodeId) {
          return {
            ...node,
            data: {
              ...node.data,
              parameters: {
                ...node.data.parameters,
                [paramName]: value,
              },
            },
          };
        }
        return node;
      })
    );
  }

  // Update connection counts for nodes
  const updateConnectionCounts = useCallback(() => {
    setNodes((nds) =>
      nds.map((node) => {
        // Count inputs connected to this node
        const connectedInputs = edges.filter((edge) => edge.target === node.id).length;
        // Count outputs from this node
        const connectedOutputs = edges.filter((edge) => edge.source === node.id).length;

        return {
          ...node,
          data: {
            ...node.data,
            connectedInputs,
            connectedOutputs,
          },
        };
      })
    );
  }, [edges, setNodes]);

  // Update counts whenever edges change
  React.useEffect(() => {
    updateConnectionCounts();
  }, [edges, updateConnectionCounts]);

  // Handle connection with validation and dynamic handle support
  const onConnect = useCallback(
    (params: Connection) => {
      // Get source and target nodes
      const sourceNode = nodes.find(n => n.id === params.source);
      const targetNode = nodes.find(n => n.id === params.target);

      if (!sourceNode || !targetNode) return;

      // Different edge colors based on agent categories
      let edgeColor = '#ea580c';
      let strokeWidth = 2;

      // Data flow edges (from data agents)
      if (sourceNode.data.category === 'data') {
        edgeColor = '#84cc16';
        strokeWidth = 2.5;
      }
      // Analysis edges (from analysis agents)
      else if (sourceNode.data.category === 'analysis') {
        edgeColor = '#8b5cf6';
        strokeWidth = 2.5;
      }
      // Trader/Hedge fund signal edges
      else if (sourceNode.data.category === 'trader' || sourceNode.data.category === 'hedge-fund') {
        edgeColor = '#3b82f6';
        strokeWidth = 2.5;
      }
      // Execution edges
      else if (sourceNode.data.category === 'execution') {
        edgeColor = '#10b981';
        strokeWidth = 3;
      }

      const newEdge = {
        ...params,
        id: `edge_${params.source}_${params.sourceHandle}_${params.target}_${params.targetHandle}_${Date.now()}`,
        animated: true,
        style: { stroke: edgeColor, strokeWidth: strokeWidth },
        markerEnd: { type: MarkerType.ArrowClosed, color: edgeColor },
        label: sourceNode.data.category === 'data' ? '📊 Data' :
               sourceNode.data.category === 'analysis' ? '📈 Analysis' :
               sourceNode.data.category === 'trader' || sourceNode.data.category === 'hedge-fund' ? '💡 Signal' :
               sourceNode.data.category === 'execution' ? '⚡ Execute' : '',
        labelStyle: { fill: edgeColor, fontWeight: 'bold', fontSize: '10px' },
        labelBgStyle: { fill: '#1a1a1a', fillOpacity: 0.8 },
        labelBgPadding: [4, 6] as [number, number],
        labelBgBorderRadius: 4,
      };
      setEdges((eds) => addEdge(newEdge, eds));
    },
    [setEdges, nodes]
  );

  // Add new agent node
  const addAgentNode = (config: AgentConfig) => {
    // Default MCP configuration
    const defaultMCPConfig: AgentMCPConfig = {
      enabled: false,
      allowedServers: [],
      autonomousMode: true,
      maxToolCalls: 10
    };

    const newNode: Node = {
      id: `agent_${Date.now()}`,
      type: 'agent',
      position: { x: Math.random() * 400 + 100, y: Math.random() * 300 + 100 },
      data: {
        label: config.name,
        agentType: config.type,
        category: config.category,
        icon: config.icon,
        color: config.color,
        hasInput: config.hasInput,
        hasOutput: config.hasOutput,
        parameters: config.parameters.reduce((acc, param) => {
          acc[param.name] = param.defaultValue;
          return acc;
        }, {} as Record<string, any>),
        status: 'idle' as AgentStatus,
        connectedInputs: 0,
        connectedOutputs: 0,
        mcpConfig: defaultMCPConfig,
        onLabelChange: handleLabelChange,
        onParameterChange: handleParameterChange,
      },
    };
    setNodes((nds) => [...nds, newNode]);
    setShowAgentMenu(false);
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
      name: workflowName,
      nodes: nodes,
      edges: edges,
      createdAt: new Date().toISOString(),
    };
    const dataStr = JSON.stringify(workflow, null, 2);
    const dataUri = `data:application/json;charset=utf-8,${encodeURIComponent(dataStr)}`;
    const exportFileDefaultName = `${workflowName.replace(/\s+/g, '_')}_${Date.now()}.json`;

    const linkElement = document.createElement('a');
    linkElement.setAttribute('href', dataUri);
    linkElement.setAttribute('download', exportFileDefaultName);
    linkElement.click();
  };

  // Execute workflow
  const executeWorkflow = async () => {
    setIsExecuting(true);

    try {
      console.log('[AgentsTab] Starting workflow execution with', nodes.length, 'agents');

      // Find starting nodes (nodes with no inputs or connected inputs)
      const startingNodes = nodes.filter(node => {
        const hasInputConnections = edges.some(edge => edge.target === node.id);
        return !node.data.hasInput || !hasInputConnections;
      });

      console.log('[AgentsTab] Starting nodes:', startingNodes.map(n => n.data.label));

      // Execute agents with MCP support if enabled
      for (const node of startingNodes) {
        if (node.data.mcpConfig && node.data.mcpConfig.enabled) {
          console.log(`[AgentsTab] Agent ${node.data.label} has MCP enabled with ${node.data.mcpConfig.allowedServers.length} servers`);

          // Update node status
          setNodes((nds) =>
            nds.map((n) =>
              n.id === node.id
                ? { ...n, data: { ...n.data, status: 'running' as AgentStatus } }
                : n
            )
          );

          // Execute agent with MCP (simplified for demonstration)
          // In production, this would call executeAgentWithMCP
          await new Promise(resolve => setTimeout(resolve, 1000));

          // Update node status to completed
          setNodes((nds) =>
            nds.map((n) =>
              n.id === node.id
                ? { ...n, data: { ...n.data, status: 'completed' as AgentStatus, lastRun: Date.now() } }
                : n
            )
          );
        }
      }

      setIsExecuting(false);
      console.log('[AgentsTab] Workflow execution completed');
      alert('Workflow execution completed!\n\nAgents with MCP enabled can now use external tools autonomously.');
    } catch (error) {
      console.error('[AgentsTab] Workflow execution failed:', error);
      setIsExecuting(false);
      alert('Workflow execution failed: ' + (error instanceof Error ? error.message : 'Unknown error'));
    }
  };

  // Filtered agent configs
  const filteredAgents = selectedCategory === 'all'
    ? AGENT_CONFIGS
    : AGENT_CONFIGS.filter(config => config.category === selectedCategory);

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
          padding: '10px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          flexWrap: 'wrap',
          flexShrink: 0,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Activity size={18} style={{ color: '#ea580c' }} />
          <input
            type="text"
            value={workflowName}
            onChange={(e) => setWorkflowName(e.target.value)}
            style={{
              background: '#0a0a0a',
              border: '1px solid #404040',
              color: '#ea580c',
              fontSize: '13px',
              fontWeight: 'bold',
              padding: '6px 10px',
              borderRadius: '4px',
              outline: 'none',
              minWidth: '200px',
            }}
          />
          <div style={{ width: '1px', height: '24px', backgroundColor: '#404040' }}></div>
        </div>

        <button
          onClick={() => setShowAgentMenu(!showAgentMenu)}
          style={{
            backgroundColor: '#ea580c',
            color: 'white',
            border: 'none',
            padding: '8px 14px',
            fontSize: '12px',
            cursor: 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontWeight: 'bold',
          }}
          onMouseEnter={(e) => (e.currentTarget.style.backgroundColor = '#dc2626')}
          onMouseLeave={(e) => (e.currentTarget.style.backgroundColor = '#ea580c')}
        >
          <Plus size={16} />
          ADD AGENT
        </button>

        <button
          onClick={executeWorkflow}
          disabled={isExecuting || nodes.length === 0}
          style={{
            backgroundColor: isExecuting ? '#6b7280' : '#10b981',
            color: 'white',
            border: 'none',
            padding: '8px 14px',
            fontSize: '12px',
            cursor: isExecuting || nodes.length === 0 ? 'not-allowed' : 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontWeight: 'bold',
          }}
        >
          {isExecuting ? <Pause size={16} /> : <Play size={16} />}
          {isExecuting ? 'RUNNING...' : 'EXECUTE'}
        </button>

        <div style={{ width: '1px', height: '24px', backgroundColor: '#404040' }}></div>

        <button
          style={{
            backgroundColor: 'transparent',
            color: '#a3a3a3',
            border: '1px solid #404040',
            padding: '8px 14px',
            fontSize: '12px',
            cursor: 'pointer',
            borderRadius: '4px',
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
          <Save size={16} />
          SAVE
        </button>

        <button
          style={{
            backgroundColor: 'transparent',
            color: '#a3a3a3',
            border: '1px solid #404040',
            padding: '8px 14px',
            fontSize: '12px',
            cursor: 'pointer',
            borderRadius: '4px',
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
          <Upload size={16} />
          IMPORT
        </button>

        <button
          onClick={exportWorkflow}
          disabled={nodes.length === 0}
          style={{
            backgroundColor: 'transparent',
            color: nodes.length === 0 ? '#6b7280' : '#a3a3a3',
            border: '1px solid #404040',
            padding: '8px 14px',
            fontSize: '12px',
            cursor: nodes.length === 0 ? 'not-allowed' : 'pointer',
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}
          onMouseEnter={(e) => {
            if (nodes.length > 0) {
              e.currentTarget.style.backgroundColor = '#2d2d2d';
              e.currentTarget.style.color = 'white';
            }
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.color = nodes.length === 0 ? '#6b7280' : '#a3a3a3';
          }}
        >
          <Download size={16} />
          EXPORT
        </button>

        {selectedNodes.length > 0 && (
          <>
            <div style={{ width: '1px', height: '24px', backgroundColor: '#404040' }}></div>
            <button
              onClick={deleteSelectedNodes}
              style={{
                backgroundColor: 'transparent',
                color: '#ef4444',
                border: '1px solid #ef4444',
                padding: '8px 14px',
                fontSize: '12px',
                cursor: 'pointer',
                borderRadius: '4px',
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
              <Trash2 size={16} />
              DELETE ({selectedNodes.length})
            </button>
          </>
        )}

        <div
          style={{
            marginLeft: 'auto',
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Clock size={14} style={{ color: '#737373' }} />
            <span style={{ color: '#737373', fontSize: '11px' }}>
              {new Date().toLocaleTimeString()}
            </span>
          </div>
          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
          <span style={{ color: '#737373', fontSize: '11px' }}>
            AGENTS: {nodes.length}
          </span>
          <span style={{ color: '#737373', fontSize: '11px' }}>
            CONNECTIONS: {edges.length}
          </span>
        </div>
      </div>

      {/* Agent Menu */}
      {showAgentMenu && (
        <div
          style={{
            position: 'absolute',
            top: '70px',
            left: '16px',
            backgroundColor: '#1a1a1a',
            border: '1px solid #2d2d2d',
            borderRadius: '6px',
            padding: '16px',
            zIndex: 1000,
            minWidth: '320px',
            maxHeight: '600px',
            overflow: 'auto',
            boxShadow: '0 8px 24px rgba(0,0,0,0.7)',
          }}
        >
          <div
            style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '16px',
            }}
          >
            <div
              style={{
                color: '#a3a3a3',
                fontSize: '13px',
                fontWeight: 'bold',
                letterSpacing: '0.5px',
              }}
            >
              SELECT AGENT TYPE
            </div>
            <button
              onClick={() => setShowAgentMenu(false)}
              style={{
                background: 'transparent',
                border: 'none',
                color: '#a3a3a3',
                cursor: 'pointer',
                padding: '4px',
              }}
            >
              <X size={18} />
            </button>
          </div>

          {/* Category Filter */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              <button
                onClick={() => setSelectedCategory('all')}
                style={{
                  background: selectedCategory === 'all' ? '#ea580c' : '#0a0a0a',
                  color: selectedCategory === 'all' ? 'white' : '#a3a3a3',
                  border: '1px solid #404040',
                  padding: '6px 12px',
                  fontSize: '10px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                }}
              >
                ALL ({AGENT_CONFIGS.length})
              </button>
              {AGENT_CATEGORIES.map((cat) => (
                <button
                  key={cat.id}
                  onClick={() => setSelectedCategory(cat.id as AgentCategory)}
                  style={{
                    background: selectedCategory === cat.id ? cat.color : '#0a0a0a',
                    color: selectedCategory === cat.id ? 'white' : '#a3a3a3',
                    border: `1px solid ${selectedCategory === cat.id ? cat.color : '#404040'}`,
                    padding: '6px 12px',
                    fontSize: '10px',
                    cursor: 'pointer',
                    borderRadius: '4px',
                    fontWeight: 'bold',
                  }}
                >
                  {cat.label.toUpperCase()}
                </button>
              ))}
            </div>
          </div>

          {/* Agent List */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {filteredAgents.map((config) => (
              <button
                key={config.id}
                onClick={() => addAgentNode(config)}
                style={{
                  backgroundColor: '#0a0a0a',
                  color: config.color,
                  border: `1px solid ${config.color}40`,
                  padding: '12px',
                  fontSize: '12px',
                  cursor: 'pointer',
                  borderRadius: '6px',
                  display: 'flex',
                  alignItems: 'flex-start',
                  gap: '12px',
                  textAlign: 'left',
                  width: '100%',
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
                <span style={{ fontSize: '20px', flexShrink: 0 }}>{config.icon}</span>
                <div style={{ flex: 1 }}>
                  <div style={{ fontWeight: 'bold', marginBottom: '4px' }}>
                    {config.name}
                  </div>
                  <div style={{ fontSize: '10px', color: '#737373', lineHeight: '1.4' }}>
                    {config.description}
                  </div>
                </div>
              </button>
            ))}
          </div>
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
            gap={24}
            size={1}
            style={{ backgroundColor: '#0a0a0a' }}
          />
          <Controls
            style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '6px',
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
          padding: '8px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '20px',
          fontSize: '11px',
          color: '#737373',
          flexShrink: 0,
        }}
      >
        <span>💡 Unlimited connections: Handles auto-expand as you add more connections (hover to see extra handles)</span>
        {selectedNodes.length > 0 && (
          <>
            <div style={{ width: '1px', height: '16px', backgroundColor: '#404040' }}></div>
            <span style={{ color: '#ea580c' }}>
              Selected: {selectedNodes.length} agent(s)
            </span>
          </>
        )}
        <div
          style={{
            marginLeft: 'auto',
            display: 'flex',
            alignItems: 'center',
            gap: '16px',
          }}
        >
          <span>📊 Data (green) | 📈 Analysis (purple) | 💡 Signal (blue) | ⚡ Execution (green)</span>
        </div>
      </div>
    </div>
  );
}
