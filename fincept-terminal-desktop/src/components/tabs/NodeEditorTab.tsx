import { useTerminalTheme } from '@/contexts/ThemeContext';
import React, { useState, useCallback, useMemo, useEffect, useRef } from 'react';
import { saveSetting, getSetting } from '@/services/core/sqliteService';
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
  ReactFlowProvider,
  useReactFlow,
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
  Monitor,
  Rocket,
  ChevronLeft,
  ChevronRight,
  Search,
  Settings2,
  Workflow,
  Filter,
  GitBranch,
  Code,
  Bell,
  Shield,
  BarChart2,
  Activity,
  RefreshCw,
  AlertCircle,
} from 'lucide-react';
import MCPToolNode from './node-editor/MCPToolNode';
import { generateMCPNodeConfigs, MCPNodeConfig } from './node-editor/mcpNodeConfigs';
import PythonAgentNode from './node-editor/PythonAgentNode';
import ResultsDisplayNode from './node-editor/ResultsDisplayNode';
import AgentMediatorNode from './node-editor/AgentMediatorNode';
import DataSourceNode from './node-editor/DataSourceNode';
import TechnicalIndicatorNode from './node-editor/TechnicalIndicatorNode';
import BacktestNode from './node-editor/BacktestNode';
import OptimizationNode from './node-editor/OptimizationNode';
import { SystemNode } from './node-editor/SystemNode';
import WorkflowManager from './node-editor/WorkflowManager';
import ResultsModal from './node-editor/ResultsModal';
import { NodeParameterInput } from './node-editor/NodeParameterInput';
import type { INodeProperties, NodeParameterValue } from '@/services/nodeSystem';
import { pythonAgentService, AgentMetadata } from '@/services/chat/pythonAgentService';
import { workflowExecutor } from './node-editor/WorkflowExecutor';
import { Workflow as WorkflowType, workflowService } from '@/services/core/workflowService';
import { nodeExecutionManager } from '@/services/core/nodeExecutionManager';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
import { NodeRegistry, INodeTypeDescription } from '@/services/nodeSystem';

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

// Built-in UI node configs (these have custom React components)
const BUILTIN_NODE_CONFIGS = [
  {
    type: 'data-source',
    label: 'Data Source',
    color: '#3b82f6',
    category: 'Input',
    hasInput: false,
    hasOutput: true,
    description: 'Connect to databases and fetch data',
  },
  {
    type: 'technical-indicator',
    label: 'Technical Indicators',
    color: '#10b981',
    category: 'Processing',
    hasInput: true,
    hasOutput: true,
    description: 'Calculate technical indicators on market data',
  },
  {
    type: 'agent-mediator',
    label: 'Agent Mediator',
    color: '#8b5cf6',
    category: 'AI',
    hasInput: true,
    hasOutput: true,
    description: 'AI-powered data analysis and decision making',
  },
  {
    type: 'results-display',
    label: 'Results Display',
    color: '#f59e0b',
    category: 'Output',
    hasInput: true,
    hasOutput: false,
    description: 'Display workflow results with formatting',
  },
  {
    type: 'backtest',
    label: 'Backtest',
    color: '#06b6d4',
    category: 'Analytics',
    hasInput: true,
    hasOutput: true,
    description: 'Run backtests on trading strategies',
  },
  {
    type: 'optimization',
    label: 'Optimization',
    color: '#a855f7',
    category: 'Analytics',
    hasInput: true,
    hasOutput: true,
    description: 'Optimize strategy parameters using grid/genetic algorithms',
  },
];

// Category icons and colors for the palette
const CATEGORY_CONFIG: Record<string, { icon: React.ReactNode; color: string }> = {
  'Core': { icon: <Workflow size={14} />, color: '#6b7280' },
  'Input': { icon: <Database size={14} />, color: '#3b82f6' },
  'Processing': { icon: <Zap size={14} />, color: '#10b981' },
  'Output': { icon: <Monitor size={14} />, color: '#f59e0b' },
  'AI': { icon: <Brain size={14} />, color: '#8b5cf6' },
  'Analytics': { icon: <BarChart2 size={14} />, color: '#06b6d4' },
  'Market Data': { icon: <Activity size={14} />, color: '#22c55e' },
  'Control Flow': { icon: <GitBranch size={14} />, color: '#eab308' },
  'Transform': { icon: <Filter size={14} />, color: '#ec4899' },
  'Safety': { icon: <Shield size={14} />, color: '#ef4444' },
  'Notifications': { icon: <Bell size={14} />, color: '#f97316' },
  'Triggers': { icon: <Zap size={14} />, color: '#a855f7' },
  'Trading': { icon: <TrendingUp size={14} />, color: '#14b8a6' },
  'agents': { icon: <Brain size={14} />, color: '#8b5cf6' },
  'marketData': { icon: <Activity size={14} />, color: '#22c55e' },
  'trading': { icon: <TrendingUp size={14} />, color: '#14b8a6' },
  'triggers': { icon: <Zap size={14} />, color: '#a855f7' },
  'controlFlow': { icon: <GitBranch size={14} />, color: '#eab308' },
  'transform': { icon: <Filter size={14} />, color: '#ec4899' },
  'safety': { icon: <Shield size={14} />, color: '#ef4444' },
  'notifications': { icon: <Bell size={14} />, color: '#f97316' },
  'analytics': { icon: <BarChart2 size={14} />, color: '#06b6d4' },
};

// Integrated Node Palette Component
interface NodePaletteProps {
  onNodeAdd: (nodeType: string, nodeData: any) => void;
  isCollapsed: boolean;
  onToggleCollapse: () => void;
  mcpNodeConfigs: MCPNodeConfig[];
  agentConfigs: AgentMetadata[];
}

const IntegratedNodePalette: React.FC<NodePaletteProps> = ({
  onNodeAdd,
  isCollapsed,
  onToggleCollapse,
  mcpNodeConfigs,
  agentConfigs,
}) => {
  const [searchQuery, setSearchQuery] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set(['Input', 'Processing', 'Output']));

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
    const nodes: Array<{
      id: string;
      type: string;
      label: string;
      category: string;
      color: string;
      description: string;
      source: 'builtin' | 'registry' | 'mcp' | 'agent';
      data?: any;
    }> = [];

    // Add builtin UI nodes
    BUILTIN_NODE_CONFIGS.forEach(config => {
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
    mcpNodeConfigs.forEach(config => {
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
    agentConfigs.forEach(agent => {
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
    const grouped = new Map<string, typeof allNodes>();

    allNodes.forEach(node => {
      if (!grouped.has(node.category)) {
        grouped.set(node.category, []);
      }
      grouped.get(node.category)!.push(node);
    });

    // Sort each category alphabetically
    grouped.forEach((nodes, _) => {
      nodes.sort((a, b) => a.label.localeCompare(b.label));
    });

    return grouped;
  }, [allNodes]);

  // Filter by search
  const filteredCategories = useMemo(() => {
    if (!searchQuery.trim()) return nodesByCategory;

    const query = searchQuery.toLowerCase();
    const filtered = new Map<string, typeof allNodes>();

    nodesByCategory.forEach((nodes, category) => {
      const matching = nodes.filter(
        n => n.label.toLowerCase().includes(query) ||
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
      <div style={{
        width: '40px',
        backgroundColor: '#0a0a0a',
        borderRight: '1px solid #2d2d2d',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        paddingTop: '8px',
      }}>
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
    <div style={{
      width: '280px',
      backgroundColor: '#0a0a0a',
      borderRight: '1px solid #2d2d2d',
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
    }}>
      {/* Palette Header */}
      <div style={{
        padding: '12px',
        borderBottom: '1px solid #2d2d2d',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          color: '#ea580c',
          fontSize: '12px',
          fontWeight: 'bold',
        }}>
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
          <Search size={14} style={{
            position: 'absolute',
            left: '10px',
            top: '50%',
            transform: 'translateY(-50%)',
            color: '#6b7280',
          }} />
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
          const catConfig = CATEGORY_CONFIG[category] || { icon: <Workflow size={14} />, color: '#6b7280' };
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
                  <span style={{ fontSize: '10px', color: '#6b7280' }}>({categoryNodes.length})</span>
                </div>
                {isExpanded ? <ChevronRight size={12} style={{ transform: 'rotate(90deg)' }} /> : <ChevronRight size={12} />}
              </button>

              {/* Category Nodes */}
              {isExpanded && (
                <div style={{ padding: '4px 8px 8px' }}>
                  {categoryNodes.map(node => (
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
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                      }}>
                        <div style={{
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
                        }}>
                          {node.label[0]}
                        </div>
                        <div style={{ flex: 1, minWidth: 0 }}>
                          <div style={{
                            color: node.color,
                            fontSize: '11px',
                            fontWeight: 'bold',
                            whiteSpace: 'nowrap',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                          }}>
                            {node.label}
                          </div>
                          {node.description && (
                            <div style={{
                              color: '#6b7280',
                              fontSize: '9px',
                              whiteSpace: 'nowrap',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                            }}>
                              {node.description.substring(0, 40)}{node.description.length > 40 ? '...' : ''}
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
          <div style={{
            padding: '24px',
            textAlign: 'center',
            color: '#6b7280',
          }}>
            <Search size={24} style={{ margin: '0 auto 8px', opacity: 0.5 }} />
            <div style={{ fontSize: '11px' }}>No nodes found</div>
          </div>
        )}
      </div>

      {/* Palette Footer */}
      <div style={{
        padding: '8px 12px',
        borderTop: '1px solid #2d2d2d',
        backgroundColor: '#0a0a0a',
      }}>
        <div style={{ color: '#6b7280', fontSize: '9px', textAlign: 'center' }}>
          Drag nodes to canvas or click to add
        </div>
      </div>
    </div>
  );
};

export default function NodeEditorTab() {
  const { t } = useTranslation('nodeEditor');
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [mcpNodeConfigs, setMcpNodeConfigs] = useState<MCPNodeConfig[]>([]);
  const [agentConfigs, setAgentConfigs] = useState<AgentMetadata[]>([]);

  // Load saved workflow from storage or start with blank slate
  const loadSavedWorkflow = useCallback(async () => {
    try {
      const saved = await getSetting('nodeEditorWorkflow');
      if (saved) {
        const workflow = JSON.parse(saved);
        return workflow;
      }
    } catch (error) {
      console.error('[NodeEditor] Failed to load saved workflow:', error);
    }
    return { nodes: [], edges: [] };
  }, []);

  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);

  // Load workflow on mount
  useEffect(() => {
    const initWorkflow = async () => {
      const workflow = await loadSavedWorkflow();
      setNodes(workflow.nodes);
      setEdges(workflow.edges);
    };
    initWorkflow();
  }, [loadSavedWorkflow]);

  // Auto-save workflow to storage whenever nodes or edges change
  useEffect(() => {
    const saveWorkflow = async () => {
      try {
        const workflow = { nodes, edges };
        await saveSetting('nodeEditorWorkflow', JSON.stringify(workflow), 'node_editor');
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
      } catch (error) {
        console.error('[NodeEditor] Failed to load MCP configs:', error);
      }
    };

    const loadAgents = async () => {
      try {
        const agents = await pythonAgentService.getAvailableAgents();
        setAgentConfigs(agents);
      } catch (error) {
        console.error('[NodeEditor] Failed to load agents:', error);
      }
    };

    loadMCPNodes();
    loadAgents();
  }, []);

  const [selectedNodes, setSelectedNodes] = useState<string[]>([]);
  const [showNodeMenu, setShowNodeMenu] = useState(false);
  const [activeView, setActiveView] = useState<'editor' | 'workflows'>('editor');
  const [isPaletteCollapsed, setIsPaletteCollapsed] = useState(false);
  const [showConfigPanel, setShowConfigPanel] = useState(false);
  const [selectedNodeForConfig, setSelectedNodeForConfig] = useState<Node | null>(null);
  const reactFlowWrapper = useRef<HTMLDivElement>(null);
  const [showDeployDialog, setShowDeployDialog] = useState(false);
  const [workflowName, setWorkflowName] = useState('');
  const [workflowDescription, setWorkflowDescription] = useState('');
  const [editingDraftId, setEditingDraftId] = useState<string | null>(null);
  const [isExecuting, setIsExecuting] = useState(false);
  const [workflowMode, setWorkflowMode] = useState<'new' | 'draft' | 'deployed'>('new');
  const [currentWorkflowId, setCurrentWorkflowId] = useState<string | null>(null);
  const [showResultsModal, setShowResultsModal] = useState(false);
  const [resultsData, setResultsData] = useState<any>(null);
  const [resultsWorkflowName, setResultsWorkflowName] = useState<string>('');

  const nodeTypes: NodeTypes = useMemo(() => ({
    custom: CustomNode,
    'mcp-tool': MCPToolNode,
    'python-agent': PythonAgentNode,
    'results-display': ResultsDisplayNode,
    'agent-mediator': AgentMediatorNode,
    'data-source': DataSourceNode,
    'technical-indicator': TechnicalIndicatorNode,
    'backtest': BacktestNode,
    'optimization': OptimizationNode,
    'system': SystemNode,
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

  // Handle node from palette (click or drag)
  const handlePaletteNodeAdd = useCallback((nodeType: string, nodeData: any) => {

    // Determine the actual node type for ReactFlow
    let reactFlowType = 'custom';
    if (nodeData.source === 'builtin') {
      reactFlowType = nodeData.data?.type || nodeType;
    } else if (nodeData.source === 'registry') {
      reactFlowType = 'custom'; // Registry nodes use custom component for now
    } else if (nodeData.source === 'mcp') {
      reactFlowType = 'mcp-tool';
    } else if (nodeData.source === 'agent') {
      reactFlowType = 'python-agent';
    }

    // Build the node based on type
    if (nodeData.source === 'builtin') {
      // Use existing addNode logic for builtin types
      addNode(nodeData.data);
    } else if (nodeData.source === 'mcp') {
      addNode(nodeData.data);
    } else if (nodeData.source === 'agent') {
      addNode({ ...nodeData.data, type: 'python-agent' });
    } else if (nodeData.source === 'registry') {
      // Create a new node from registry node description
      const registryNode = nodeData.data as INodeTypeDescription;
      const newNode: Node = {
        id: `node_${Date.now()}`,
        type: 'custom',
        position: { x: Math.random() * 400 + 100, y: Math.random() * 300 + 100 },
        data: {
          label: registryNode.displayName,
          nodeTypeName: registryNode.name,
          color: nodeData.color || '#6b7280',
          hasInput: (registryNode.inputs?.length || 0) > 0 || registryNode.name !== 'manualTrigger',
          hasOutput: (registryNode.outputs?.length || 0) > 0,
          status: 'idle',
          parameters: {},
          registryData: registryNode,
          onLabelChange: handleLabelChange,
        },
      };
      setNodes((nds) => [...nds, newNode]);
    }

    setShowNodeMenu(false);
  }, [setNodes]);

  // Handle drop from palette
  const onDrop = useCallback(
    (event: React.DragEvent) => {
      event.preventDefault();

      const data = event.dataTransfer.getData('application/reactflow');
      if (!data) return;

      const nodeData = JSON.parse(data);

      // Get position relative to the canvas
      const reactFlowBounds = reactFlowWrapper.current?.getBoundingClientRect();
      if (!reactFlowBounds) return;

      const position = {
        x: event.clientX - reactFlowBounds.left - 150,
        y: event.clientY - reactFlowBounds.top - 25,
      };

      // Add node at drop position
      if (nodeData.source === 'registry') {
        const registryNode = nodeData.data as INodeTypeDescription;
        const newNode: Node = {
          id: `node_${Date.now()}`,
          type: 'custom',
          position,
          data: {
            label: registryNode.displayName,
            nodeTypeName: registryNode.name,
            color: nodeData.color || '#6b7280',
            hasInput: (registryNode.inputs?.length || 0) > 0 || registryNode.name !== 'manualTrigger',
            hasOutput: (registryNode.outputs?.length || 0) > 0,
            status: 'idle',
            parameters: {},
            registryData: registryNode,
            onLabelChange: handleLabelChange,
          },
        };
        setNodes((nds) => [...nds, newNode]);
      } else {
        // Use existing logic for other node types
        handlePaletteNodeAdd(nodeData.type, nodeData);
      }
    },
    [setNodes, handlePaletteNodeAdd]
  );

  const onDragOver = useCallback((event: React.DragEvent) => {
    event.preventDefault();
    event.dataTransfer.dropEffect = 'move';
  }, []);

  // Add new node (handles regular nodes, MCP tool nodes, Python agent nodes, data source, technical indicators, and agent mediator)
  const addNode = (config: any) => {
    const isMCPNode = config.type === 'mcp-tool';
    const isPythonAgent = config.type === 'python-agent';
    const isResultsDisplay = config.type === 'results-display';
    const isAgentMediator = config.type === 'agent-mediator';
    const isDataSource = config.type === 'data-source';
    const isTechnicalIndicator = config.type === 'technical-indicator';

    const newNode: Node = {
      id: `node_${Date.now()}`,
      type: isPythonAgent ? 'python-agent' : (isMCPNode ? 'mcp-tool' : (isResultsDisplay ? 'results-display' : (isAgentMediator ? 'agent-mediator' : (isDataSource ? 'data-source' : (isTechnicalIndicator ? 'technical-indicator' : 'custom'))))),
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
        onParameterChange: (_params: Record<string, any>) => {
          // Parameters updated
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
        onParameterChange: (_params: Record<string, any>) => {
          // Parameters updated
        },
        onExecute: (_result: any) => {
          // Tool executed
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
        onExecute: (_nodeId: string) => {
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
      } : (isDataSource ? {
        label: config.label,
        selectedConnectionId: undefined,
        query: '',
        status: 'idle',
        result: undefined,
        error: undefined,
        onConnectionChange: (connectionId: string) => {
          setNodes((nds) =>
            nds.map((n) =>
              n.id === newNode.id
                ? {
                  ...n,
                  data: {
                    ...n.data,
                    selectedConnectionId: connectionId,
                  },
                }
                : n
            )
          );
        },
        onQueryChange: (query: string) => {
          setNodes((nds) =>
            nds.map((n) =>
              n.id === newNode.id
                ? {
                  ...n,
                  data: {
                    ...n.data,
                    query: query,
                  },
                }
                : n
            )
          );
        },
        onExecute: async () => {
          // Will be handled by workflow execution
        },
      } : (isTechnicalIndicator ? {
        label: config.label,
        dataSource: 'yfinance',
        symbol: 'AAPL',
        period: '1y',
        csvPath: '',
        jsonData: '',
        categories: ['momentum', 'volume', 'volatility', 'trend', 'others'],
        status: 'idle',
        result: undefined,
        error: undefined,
        onExecute: (_nodeId: string) => {
          // Workflow execution will handle this
        },
        onParameterChange: (params: Record<string, any>) => {
          setNodes((nds) =>
            nds.map((n) =>
              n.id === newNode.id
                ? {
                  ...n,
                  data: {
                    ...n.data,
                    ...params,
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
      }))))),
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
  const saveWorkflow = useCallback(async () => {
    try {
      const workflow = { nodes, edges };
      await saveSetting('nodeEditorWorkflow', JSON.stringify(workflow), 'node_editor');
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
  const clearWorkflow = useCallback(async () => {
    setNodes([]);
    setEdges([]);
    await saveSetting('nodeEditorWorkflow', '', 'node_editor');
    setWorkflowMode('new');
    setCurrentWorkflowId(null);
    setEditingDraftId(null);
    setWorkflowName('');
    setWorkflowDescription('');
  }, [setNodes, setEdges]);

  // Deploy workflow
  const deployWorkflow = useCallback(async () => {
    if (!workflowName.trim()) {
      alert('Please enter a workflow name');
      return;
    }

    if (isExecuting) {
      alert('A workflow is already executing. Please wait for it to complete.');
      return;
    }

    // Store values before closing dialog
    const name = workflowName;
    const description = workflowDescription;
    const currentNodes = nodes;
    const currentEdges = edges;

    // Close dialog IMMEDIATELY - don't wait for execution
    setShowDeployDialog(false);
    setWorkflowName('');
    setWorkflowDescription('');
    setEditingDraftId(null);
    setActiveView('editor'); // Stay in editor to see execution progress

    // Execute in background
    setIsExecuting(true);

    // Use setTimeout to ensure dialog closes before heavy execution starts
    setTimeout(async () => {
      try {
        const result = await workflowExecutor.executeAndSave(
          currentNodes,
          currentEdges,
          {
            name: name,
            description: description,
            saveAsDraft: false
          },
          (nodeId: string, status: string, result?: any) => {
            setNodes((nds) =>
              nds.map((node) => {
                if (node.id === nodeId) {
                  if (node.type === 'results-display') {
                    return {
                      ...node,
                      data: {
                        ...node.data,
                        inputData: status === 'completed' ? result : undefined,
                        status,
                      },
                    };
                  }
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

        if (result.success) {
          // Clear the canvas after successful deployment
          setNodes([]);
          setEdges([]);
          await saveSetting('nodeEditorWorkflow', '', 'node_editor');

          // Reset to new workflow mode
          setCurrentWorkflowId(null);
          setWorkflowMode('new');
          setEditingDraftId(null);
        } else {
          console.error('[NodeEditor] Deployment failed:', result.error);
          alert(`Deployment failed: ${result.error}`);
        }
      } catch (error: any) {
        console.error('[NodeEditor] Deploy failed:', error);
        alert(`Deployment failed: ${error.message}`);
      } finally {
        setIsExecuting(false);
      }
    }, 0);
  }, [nodes, edges, workflowName, workflowDescription, isExecuting, setNodes]);

  // Save as draft
  const saveDraft = useCallback(async () => {
    if (!workflowName.trim()) {
      alert('Please enter a workflow name');
      return;
    }

    try {
      let workflowId: string;
      if (editingDraftId) {
        // Update existing draft
        await workflowService.updateDraft(
          editingDraftId,
          workflowName,
          workflowDescription,
          nodes,
          edges
        );
        workflowId = editingDraftId;
        setEditingDraftId(null);
      } else {
        // Create new draft
        workflowId = await workflowExecutor.saveDraft(
          workflowName,
          workflowDescription,
          nodes,
          edges
        );
      }
      setCurrentWorkflowId(workflowId);
      setWorkflowMode('draft');
      setShowDeployDialog(false);
      setWorkflowName('');
      setWorkflowDescription('');
      setActiveView('workflows');
    } catch (error: any) {
      console.error('[NodeEditor] Save draft failed:', error);
      alert(`Save failed: ${error.message}`);
    }
  }, [nodes, edges, workflowName, workflowDescription, editingDraftId]);

  // Load workflow from manager (for play button on draft/completed workflows)
  const handleLoadWorkflow = useCallback((loadedNodes: Node[], loadedEdges: Edge[], workflowId: string, workflow: WorkflowType) => {
    setNodes(loadedNodes);
    setEdges(loadedEdges);
    setCurrentWorkflowId(workflowId);
    setWorkflowMode(workflow.status === 'draft' ? 'draft' : 'deployed');
    setActiveView('editor');
  }, [setNodes, setEdges]);

  // View workflow results
  const handleViewResults = useCallback((workflow: WorkflowType) => {
    // Ask user if they want to see in modal or navigate to workflow
    const choice = window.confirm(
      `View results for: ${workflow.name}\n\n` +
      `Click OK to view in modal window\n` +
      `Click Cancel to navigate to workflow editor with results`
    );

    if (choice) {
      // Show in modal
      setResultsData(workflow.results);
      setResultsWorkflowName(workflow.name);
      setShowResultsModal(true);
    } else {
      // Navigate to editor with results
      setNodes(workflow.nodes);
      setEdges(workflow.edges);
      setCurrentWorkflowId(workflow.id);
      setWorkflowMode('deployed');
      setActiveView('editor');
    }
  }, [setNodes, setEdges]);

  // Edit draft workflow
  const handleEditDraft = useCallback((workflow: WorkflowType) => {
    setNodes(workflow.nodes);
    setEdges(workflow.edges);
    setWorkflowName(workflow.name);
    setWorkflowDescription(workflow.description || '');
    setEditingDraftId(workflow.id);
    setCurrentWorkflowId(workflow.id);
    setWorkflowMode('draft');
    setShowDeployDialog(true);
    setActiveView('editor');
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
            {t('title')}
          </span>
          <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>

          {/* Tab Switcher */}
          <button
            onClick={() => setActiveView('editor')}
            style={{
              backgroundColor: activeView === 'editor' ? '#ea580c' : 'transparent',
              color: activeView === 'editor' ? 'white' : '#a3a3a3',
              border: activeView === 'editor' ? 'none' : '1px solid #404040',
              padding: '4px 10px',
              fontSize: '10px',
              cursor: 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
            }}
          >
            {t('tabs.editor')}
          </button>
          <button
            onClick={() => setActiveView('workflows')}
            style={{
              backgroundColor: activeView === 'workflows' ? '#ea580c' : 'transparent',
              color: activeView === 'workflows' ? 'white' : '#a3a3a3',
              border: activeView === 'workflows' ? 'none' : '1px solid #404040',
              padding: '4px 10px',
              fontSize: '10px',
              cursor: 'pointer',
              borderRadius: '3px',
              fontWeight: 'bold',
            }}
          >
            WORKFLOWS
          </button>
        </div>

        {/* Editor-specific buttons */}
        {activeView === 'editor' && (
          <>
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
                if (isExecuting) {
                  alert('A workflow is already executing. Please wait for it to complete.');
                  return;
                }

                try {
                  setIsExecuting(true);
                  await nodeExecutionManager.executeWorkflow(
                    nodes,
                    edges,
                    (nodeId, status, result) => {
                      // Update node status
                      setNodes((nds) =>
                        nds.map((node) => {
                          if (node.id === nodeId) {
                            // For results-display nodes, update inputData
                            if (node.type === 'results-display') {
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
                } catch (error: any) {
                  console.error('[NodeEditor] Workflow execution failed:', error);
                  // Don't use alert - just log the error
                } finally {
                  setIsExecuting(false);
                }
              }}
              disabled={isExecuting}
              style={{
                backgroundColor: 'transparent',
                color: isExecuting ? '#737373' : '#10b981',
                border: `1px solid ${isExecuting ? '#737373' : '#10b981'}`,
                padding: '6px 12px',
                fontSize: '11px',
                cursor: isExecuting ? 'not-allowed' : 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                opacity: isExecuting ? 0.5 : 1,
              }}
              onMouseEnter={(e) => {
                if (!isExecuting) {
                  e.currentTarget.style.backgroundColor = '#10b981';
                  e.currentTarget.style.color = 'white';
                }
              }}
              onMouseLeave={(e) => {
                if (!isExecuting) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = '#10b981';
                }
              }}
            >
              <Play size={14} />
              {isExecuting ? 'EXECUTING...' : 'EXECUTE'}
            </button>

            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>

            {/* New Workflow Button */}
            <button
              onClick={() => {
                if (window.confirm('Start a new workflow? Current work will be cleared unless saved.')) {
                  clearWorkflow();
                }
              }}
              style={{
                backgroundColor: 'transparent',
                color: '#3b82f6',
                border: '1px solid #3b82f6',
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                fontWeight: 'bold',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = '#3b82f6';
                e.currentTarget.style.color = 'white';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = '#3b82f6';
              }}
              title="Start a new workflow"
            >
              <Plus size={14} />
              NEW
            </button>

            {/* Quick Save for Drafts */}
            {workflowMode === 'draft' && editingDraftId && (
              <button
                onClick={async () => {
                  try {
                    await workflowService.updateDraft(
                      editingDraftId,
                      workflowName || 'Untitled Draft',
                      workflowDescription,
                      nodes,
                      edges
                    );
                    alert('Draft saved successfully!');
                  } catch (error: any) {
                    alert(`Save failed: ${error.message}`);
                  }
                }}
                style={{
                  backgroundColor: '#f59e0b',
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
                title="Quick save current draft"
              >
                <Save size={14} />
                SAVE DRAFT
              </button>
            )}

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

            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>

            {/* DEPLOY Button */}
            <button
              onClick={() => setShowDeployDialog(true)}
              style={{
                backgroundColor: 'transparent',
                color: '#3b82f6',
                border: '1px solid #3b82f6',
                padding: '6px 12px',
                fontSize: '11px',
                cursor: 'pointer',
                borderRadius: '3px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                fontWeight: 'bold',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = '#3b82f6';
                e.currentTarget.style.color = 'white';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = '#3b82f6';
              }}
              title="Deploy workflow to database"
            >
              <Rocket size={14} />
              DEPLOY
            </button>

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
          </>
        )}
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
              {BUILTIN_NODE_CONFIGS.filter((t) => t.category === category).map((config) => (
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
                 MCP TOOLS ({mcpNodeConfigs.length})
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

        </div>
      )}

      {/* Main Content - Conditional Rendering */}
      {activeView === 'editor' ? (
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left Sidebar - Node Palette */}
          <IntegratedNodePalette
            onNodeAdd={handlePaletteNodeAdd}
            isCollapsed={isPaletteCollapsed}
            onToggleCollapse={() => setIsPaletteCollapsed(!isPaletteCollapsed)}
            mcpNodeConfigs={mcpNodeConfigs}
            agentConfigs={agentConfigs}
          />

          {/* React Flow Canvas */}
          <div
            ref={reactFlowWrapper}
            style={{ flex: 1, position: 'relative' }}
            onDrop={onDrop}
            onDragOver={onDragOver}
          >
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

            {/* Empty State Overlay */}
            {nodes.length === 0 && (
              <div style={{
                position: 'absolute',
                top: '50%',
                left: '50%',
                transform: 'translate(-50%, -50%)',
                textAlign: 'center',
                color: '#6b7280',
                pointerEvents: 'none',
              }}>
                <Workflow size={48} style={{ margin: '0 auto 16px', opacity: 0.3 }} />
                <div style={{ fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                  No nodes yet
                </div>
                <div style={{ fontSize: '12px', opacity: 0.7 }}>
                  Drag nodes from the palette or click ADD NODE to get started
                </div>
              </div>
            )}
          </div>

          {/* Right Sidebar - Config Panel (Fincept Style) */}
          {selectedNodes.length === 1 && (() => {
            const selectedNode = nodes.find(n => n.id === selectedNodes[0]);
            if (!selectedNode) return null;

            // Get registry node type if this is a registry node
            const registryData = selectedNode.data.registryData;
            const nodeProperties: INodeProperties[] = registryData?.properties || [];

            // Handler for parameter changes
            const handleParameterChange = (paramName: string, value: NodeParameterValue) => {
              setNodes((nds) =>
                nds.map((node) => {
                  if (node.id === selectedNode.id) {
                    return {
                      ...node,
                      data: {
                        ...node.data,
                        parameters: {
                          ...(node.data.parameters || {}),
                          [paramName]: value,
                        },
                      },
                    };
                  }
                  return node;
                })
              );
            };

            const nodeColor = selectedNode.data.color || '#FF8800';

            return (
              <div style={{
                width: '300px',
                backgroundColor: '#000000',
                borderLeft: '2px solid #FF8800',
                display: 'flex',
                flexDirection: 'column',
                overflow: 'hidden',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace',
              }}>
                {/* Panel Header - Fincept Style */}
                <div style={{
                  backgroundColor: '#1A1A1A',
                  borderBottom: '1px solid #2A2A2A',
                  padding: '8px 12px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Settings2 size={14} style={{ color: '#FF8800', filter: 'drop-shadow(0 0 4px #FF8800)' }} />
                    <span style={{
                      color: '#FF8800',
                      fontSize: '11px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      textTransform: 'uppercase',
                    }}>
                      NODE CONFIG
                    </span>
                  </div>
                  <button
                    onClick={() => setSelectedNodes([])}
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
                <div style={{
                  backgroundColor: '#0F0F0F',
                  padding: '10px 12px',
                  borderBottom: '1px solid #2A2A2A',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                }}>
                  <div style={{
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
                  }}>
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
                    <label style={{
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      color: '#FF8800',
                      fontSize: '9px',
                      fontWeight: 700,
                      marginBottom: '6px',
                      textTransform: 'uppercase',
                      letterSpacing: '0.5px',
                    }}>
                      <span>LABEL</span>
                      <span style={{ color: '#4A4A4A', fontSize: '8px', fontWeight: 400 }}>REQUIRED</span>
                    </label>
                    <div style={{
                      position: 'relative',
                      display: 'flex',
                      alignItems: 'stretch',
                    }}>
                      <div style={{
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
                      }}>
                        <Edit3 size={10} />
                      </div>
                      <input
                        type="text"
                        value={selectedNode.data.label || ''}
                        onChange={(e) => handleLabelChange(selectedNode.id, e.target.value)}
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
                        onFocus={(e) => {
                          e.currentTarget.style.borderColor = '#FF8800';
                          e.currentTarget.style.backgroundColor = '#0F0F0F';
                          e.currentTarget.style.boxShadow = '0 0 8px #FF880030';
                          const iconDiv = e.currentTarget.previousElementSibling as HTMLElement;
                          if (iconDiv) iconDiv.style.borderColor = '#FF8800';
                        }}
                        onBlur={(e) => {
                          e.currentTarget.style.borderColor = '#2A2A2A';
                          e.currentTarget.style.backgroundColor = '#0A0A0A';
                          e.currentTarget.style.boxShadow = 'none';
                          const iconDiv = e.currentTarget.previousElementSibling as HTMLElement;
                          if (iconDiv) iconDiv.style.borderColor = '#2A2A2A';
                        }}
                      />
                    </div>
                    <div style={{
                      marginTop: '4px',
                      fontSize: '8px',
                      color: '#4A4A4A',
                      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    }}>
                      Display name for this node in the workflow
                    </div>
                  </div>

                  {/* Registry Node Parameters */}
                  {nodeProperties.length > 0 && (
                    <div style={{
                      padding: '14px',
                      borderBottom: '1px solid #3A3A3A',
                      backgroundColor: '#111111',
                    }}>
                      {/* Parameters Header */}
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        marginBottom: '16px',
                        paddingBottom: '10px',
                        borderBottom: '1px solid #2A2A2A',
                      }}>
                        <div style={{
                          display: 'flex',
                          alignItems: 'center',
                          gap: '10px',
                        }}>
                          <div style={{
                            width: '4px',
                            height: '18px',
                            backgroundColor: '#FF8800',
                          }} />
                          <span style={{
                            color: '#FF8800',
                            fontSize: '13px',
                            fontWeight: 700,
                            textTransform: 'uppercase',
                            letterSpacing: '1.5px',
                            fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                          }}>
                            PARAMETERS
                          </span>
                        </div>
                        <div style={{
                          backgroundColor: '#FF880030',
                          border: '1px solid #FF8800',
                          padding: '4px 10px',
                          fontSize: '12px',
                          fontWeight: 700,
                          color: '#FF8800',
                          fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                        }}>
                          {nodeProperties.length}
                        </div>
                      </div>

                      {/* Parameters List */}
                      <div style={{
                        display: 'flex',
                        flexDirection: 'column',
                        gap: '14px',
                      }}>
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
                            <div style={{
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
                            }}>
                              {String(index + 1).padStart(2, '0')}
                            </div>

                            {/* Required Indicator */}
                            {param.required && (
                              <div style={{
                                position: 'absolute',
                                top: '0',
                                right: '0',
                                width: '0',
                                height: '0',
                                borderStyle: 'solid',
                                borderWidth: '0 16px 16px 0',
                                borderColor: 'transparent #FF3B3B transparent transparent',
                              }} />
                            )}

                            <div style={{ paddingTop: '12px' }}>
                              <NodeParameterInput
                                parameter={param}
                                value={selectedNode.data.parameters?.[param.name] ?? param.default}
                                onChange={(value) => handleParameterChange(param.name, value)}
                              />
                            </div>
                          </div>
                        ))}
                      </div>

                      {/* Parameters Footer */}
                      <div style={{
                        marginTop: '14px',
                        paddingTop: '10px',
                        borderTop: '1px solid #2A2A2A',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                      }}>
                        <div style={{
                          width: '8px',
                          height: '8px',
                          backgroundColor: '#00D66F',
                          borderRadius: '50%',
                          boxShadow: '0 0 6px #00D66F80',
                        }} />
                        <span style={{
                          color: '#AAAAAA',
                          fontSize: '11px',
                          fontWeight: 600,
                          fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                          textTransform: 'uppercase',
                          letterSpacing: '0.5px',
                        }}>
                          {nodeProperties.filter(p => p.required).length > 0
                            ? `${nodeProperties.filter(p => p.required).length} REQUIRED`
                            : 'ALL OPTIONAL'}
                        </span>
                      </div>
                    </div>
                  )}

                  {/* Node Info Section */}
                  <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
                    <div style={{
                      color: '#FF8800',
                      fontSize: '9px',
                      fontWeight: 700,
                      marginBottom: '10px',
                      textTransform: 'uppercase',
                      letterSpacing: '0.5px',
                    }}>
                      NODE INFO
                    </div>

                    <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                      {/* Node ID */}
                      <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                        <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>ID</span>
                        <code style={{ color: '#FFFFFF', fontSize: '9px' }}>{selectedNode.id}</code>
                      </div>

                      {/* Position */}
                      <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                        <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>POS</span>
                        <span style={{ color: '#FFFFFF', fontSize: '9px' }}>
                          {Math.round(selectedNode.position.x)}, {Math.round(selectedNode.position.y)}
                        </span>
                      </div>

                      {/* Registry Type */}
                      {selectedNode.data.nodeTypeName && (
                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                          <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>TYPE</span>
                          <span style={{ color: '#FFFFFF', fontSize: '9px' }}>{selectedNode.data.nodeTypeName}</span>
                        </div>
                      )}

                      {/* Status */}
                      {selectedNode.data.status && (
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                          <span style={{ color: '#787878', fontSize: '9px', textTransform: 'uppercase' }}>STATUS</span>
                          <span style={{
                            fontSize: '9px',
                            fontWeight: 700,
                            padding: '2px 6px',
                            backgroundColor: selectedNode.data.status === 'completed' ? '#00D66F20' :
                              selectedNode.data.status === 'error' ? '#FF3B3B20' :
                              selectedNode.data.status === 'running' ? '#0088FF20' : '#78787820',
                            color: selectedNode.data.status === 'completed' ? '#00D66F' :
                              selectedNode.data.status === 'error' ? '#FF3B3B' :
                              selectedNode.data.status === 'running' ? '#0088FF' : '#787878',
                            textTransform: 'uppercase',
                          }}>
                            {selectedNode.data.status}
                          </span>
                        </div>
                      )}
                    </div>
                  </div>

                  {/* Description (for registry nodes) */}
                  {registryData?.description && (
                    <div style={{ padding: '12px', borderBottom: '1px solid #2A2A2A' }}>
                      <div style={{
                        color: '#00E5FF',
                        fontSize: '9px',
                        fontWeight: 700,
                        marginBottom: '6px',
                        textTransform: 'uppercase',
                        letterSpacing: '0.5px',
                      }}>
                        DESCRIPTION
                      </div>
                      <div style={{ color: '#FFFFFF', fontSize: '10px', lineHeight: '1.5' }}>
                        {registryData.description}
                      </div>
                    </div>
                  )}

                  {/* Error Display */}
                  {selectedNode.data.error && (
                    <div style={{
                      margin: '12px',
                      backgroundColor: '#FF3B3B15',
                      border: '1px solid #FF3B3B',
                      padding: '10px',
                    }}>
                      <div style={{
                        color: '#FF3B3B',
                        fontSize: '9px',
                        fontWeight: 700,
                        marginBottom: '4px',
                        textTransform: 'uppercase',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                      }}>
                        <AlertCircle size={10} />
                        ERROR
                      </div>
                      <div style={{ color: '#FFFFFF', fontSize: '10px', lineHeight: '1.4' }}>
                        {selectedNode.data.error}
                      </div>
                    </div>
                  )}

                  {/* Result Preview (for completed nodes) */}
                  {selectedNode.data.result && selectedNode.data.status === 'completed' && (
                    <div style={{
                      margin: '12px',
                      backgroundColor: '#00D66F15',
                      border: '1px solid #00D66F',
                      padding: '10px',
                    }}>
                      <div style={{
                        color: '#00D66F',
                        fontSize: '9px',
                        fontWeight: 700,
                        marginBottom: '6px',
                        textTransform: 'uppercase',
                      }}>
                        OUTPUT PREVIEW
                      </div>
                      <pre style={{
                        color: '#FFFFFF',
                        fontSize: '9px',
                        lineHeight: '1.4',
                        overflow: 'auto',
                        maxHeight: '80px',
                        margin: 0,
                        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                      }}>
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

                {/* Panel Footer with Actions - Fincept Style */}
                <div style={{
                  padding: '10px 12px',
                  backgroundColor: '#0F0F0F',
                  borderTop: '1px solid #2A2A2A',
                  display: 'flex',
                  gap: '8px',
                }}>
                  <button
                    onClick={() => deleteSelectedNodes()}
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
                    onClick={() => {
                      // Duplicate node
                      const newNode = {
                        ...selectedNode,
                        id: `node_${Date.now()}`,
                        position: {
                          x: selectedNode.position.x + 50,
                          y: selectedNode.position.y + 50,
                        },
                        data: { ...selectedNode.data },
                      };
                      setNodes((nds) => [...nds, newNode]);
                    }}
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
          })()}
        </div>
      ) : (
        /* Workflow Manager View */
        <div style={{ flex: 1, overflow: 'hidden' }}>
          <WorkflowManager
            onLoadWorkflow={handleLoadWorkflow}
            onViewResults={handleViewResults}
            onEditDraft={handleEditDraft}
          />
        </div>
      )}

      {/* Bottom Info Bar */}
      {activeView === 'editor' && (
        <TabFooter
          tabName="NODE EDITOR"
          leftInfo={[
            { label: '💡 Drag nodes to move | Drag from handles to connect | Select & Delete to remove', color: '#737373' },
            ...(selectedNodes.length > 0 ? [{ label: `Selected: ${selectedNodes.length} node(s)`, color: '#ea580c' }] : [])
          ]}
          statusInfo="Scroll to zoom | Right-click drag to pan"
          backgroundColor="#1a1a1a"
          borderColor="#2d2d2d"
        />
      )}

      {/* Deploy Dialog */}
      {showDeployDialog && (
        <div
          style={{
            position: 'fixed',
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            backgroundColor: 'rgba(0, 0, 0, 0.7)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            zIndex: 2000,
          }}
          onClick={() => setShowDeployDialog(false)}
        >
          <div
            style={{
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '6px',
              padding: '24px',
              minWidth: '400px',
              maxWidth: '500px',
            }}
            onClick={(e) => e.stopPropagation()}
          >
            <div
              style={{
                color: '#ea580c',
                fontSize: '14px',
                fontWeight: 'bold',
                marginBottom: '20px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}
            >
              <Rocket size={16} />
              DEPLOY WORKFLOW
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label
                style={{
                  display: 'block',
                  color: '#a3a3a3',
                  fontSize: '11px',
                  marginBottom: '6px',
                  fontWeight: 'bold',
                }}
              >
                WORKFLOW NAME *
              </label>
              <input
                type="text"
                value={workflowName}
                onChange={(e) => setWorkflowName(e.target.value)}
                placeholder="Enter workflow name..."
                autoFocus
                style={{
                  width: '100%',
                  backgroundColor: '#0a0a0a',
                  border: '1px solid #2d2d2d',
                  color: 'white',
                  padding: '8px 12px',
                  fontSize: '12px',
                  borderRadius: '4px',
                  outline: 'none',
                  boxSizing: 'border-box',
                }}
              />
            </div>

            <div style={{ marginBottom: '20px' }}>
              <label
                style={{
                  display: 'block',
                  color: '#a3a3a3',
                  fontSize: '11px',
                  marginBottom: '6px',
                  fontWeight: 'bold',
                }}
              >
                DESCRIPTION (OPTIONAL)
              </label>
              <textarea
                value={workflowDescription}
                onChange={(e) => setWorkflowDescription(e.target.value)}
                placeholder="Enter workflow description..."
                rows={3}
                style={{
                  width: '100%',
                  backgroundColor: '#0a0a0a',
                  border: '1px solid #2d2d2d',
                  color: 'white',
                  padding: '8px 12px',
                  fontSize: '12px',
                  borderRadius: '4px',
                  outline: 'none',
                  resize: 'vertical',
                  fontFamily: 'Consolas, monospace',
                  boxSizing: 'border-box',
                }}
              />
            </div>

            <div style={{ display: 'flex', gap: '12px', justifyContent: 'flex-end' }}>
              <button
                onClick={() => {
                  setShowDeployDialog(false);
                  setWorkflowName('');
                  setWorkflowDescription('');
                }}
                style={{
                  backgroundColor: 'transparent',
                  color: '#a3a3a3',
                  border: '1px solid #404040',
                  padding: '8px 16px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                }}
              >
                CANCEL
              </button>
              <button
                onClick={saveDraft}
                style={{
                  backgroundColor: 'transparent',
                  color: '#f59e0b',
                  border: '1px solid #f59e0b',
                  padding: '8px 16px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                }}
              >
                SAVE AS DRAFT
              </button>
              <button
                onClick={deployWorkflow}
                style={{
                  backgroundColor: '#3b82f6',
                  color: 'white',
                  border: 'none',
                  padding: '8px 16px',
                  fontSize: '11px',
                  cursor: 'pointer',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                }}
              >
                DEPLOY & EXECUTE
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Results Modal */}
      {showResultsModal && (
        <ResultsModal
          data={resultsData}
          workflowName={resultsWorkflowName}
          onClose={() => {
            setShowResultsModal(false);
            setResultsData(null);
            setResultsWorkflowName('');
          }}
        />
      )}
    </div>
  );
}
