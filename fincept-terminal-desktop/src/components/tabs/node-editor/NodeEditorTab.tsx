/**
 * NodeEditorTab - Main Component
 *
 * Visual workflow editor for creating and executing financial analysis workflows.
 * This is the main orchestrator component that composes all sub-components.
 */

import React, { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import ReactFlow, {
  Node,
  Edge,
  addEdge,
  Background,
  Controls,
  MiniMap,
  Connection,
  NodeTypes,
  MarkerType,
} from 'reactflow';
import 'reactflow/dist/style.css';
import {
  Database,
  Monitor,
  Zap,
  TrendingUp,
  BarChart3,
  Brain,
  FileText,
  X,
  Workflow,
} from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { showWarning, showSuccess, showError } from '@/utils/notifications';

// Local components
import {
  CustomNode,
  NodePalette,
  NodeEditorToolbar,
  DeployDialog,
  NodeConfigPanel,
  TemplateGallery,
} from './components';

// Local hooks
import { useWorkflowManagement } from './hooks';

// Node components
import {
  MCPToolNode,
  PythonAgentNode,
  ResultsDisplayNode,
  AgentMediatorNode,
  DataSourceNode,
  TechnicalIndicatorNode,
  BacktestNode,
  OptimizationNode,
  SystemNode,
  WorkflowManager,
  ResultsModal,
  generateMCPNodeConfigs,
  MCPNodeConfig,
} from './nodes';

// Constants
import { BUILTIN_NODE_CONFIGS, DEFAULT_EDGE_OPTIONS, FLOW_BACKGROUND_CONFIG } from './constants';

// Types
import type { INodeTypeDescription, NodeParameterValue } from './types';
import type { WorkflowTemplate } from './workflowTemplates';

// Services
import { workflowService } from '@/services/core/workflowService';
import { nodeExecutionManager } from '@/services/core/nodeExecutionManager';
import { NodeRegistry, NodeLoader } from '@/services/nodeSystem';
import { TabFooter } from '@/components/common/TabFooter';

export default function NodeEditorTab() {
  const { t } = useTranslation('nodeEditor');

  // MCP configurations
  const [mcpNodeConfigs, setMcpNodeConfigs] = useState<MCPNodeConfig[]>([]);

  // Workflow management hook
  const {
    nodes,
    edges,
    setNodes,
    setEdges,
    onNodesChange,
    onEdgesChange,
    selectedNodes,
    setSelectedNodes,
    isExecuting,
    setIsExecuting,
    workflowMode,
    setWorkflowMode,
    editingDraftId,
    setEditingDraftId,
    workflowName,
    setWorkflowName,
    workflowDescription,
    setWorkflowDescription,
    saveWorkflow,
    exportWorkflow,
    importWorkflow,
    clearWorkflow,
    deployWorkflow,
    saveDraft,
    handleLoadWorkflow,
    handleViewResults,
    handleEditDraft,
    deleteSelectedNodes,
    deleteSelectedEdges,
    onSelectionChange,
  } = useWorkflowManagement();

  // UI state
  const [showNodeMenu, setShowNodeMenu] = useState(false);
  const [activeView, setActiveView] = useState<'editor' | 'workflows'>('editor');
  const [isPaletteCollapsed, setIsPaletteCollapsed] = useState(false);
  const [showDeployDialog, setShowDeployDialog] = useState(false);
  const [showResultsModal, setShowResultsModal] = useState(false);
  const [resultsData, setResultsData] = useState<any>(null);
  const [resultsWorkflowName, setResultsWorkflowName] = useState<string>('');
  const [showTemplateGallery, setShowTemplateGallery] = useState(false);

  const reactFlowWrapper = useRef<HTMLDivElement>(null);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeView,
    isPaletteCollapsed,
    workflowName,
  }), [activeView, isPaletteCollapsed, workflowName]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (state.activeView === 'editor' || state.activeView === 'workflows') setActiveView(state.activeView);
    if (typeof state.isPaletteCollapsed === 'boolean') setIsPaletteCollapsed(state.isPaletteCollapsed);
    if (typeof state.workflowName === 'string') setWorkflowName(state.workflowName);
  }, [setWorkflowName]);

  useWorkspaceTabState('node-editor', getWorkspaceState, setWorkspaceState);

  // Load MCP node configurations, Python agents, and NodeSystem on mount
  useEffect(() => {
    const initializeNodeSystem = async () => {
      try {
        // Initialize the node system with all backend nodes
        if (!NodeLoader.isLoaded()) {
          console.log('[NodeEditor] Initializing Node System...');
          await NodeLoader.loadAll();
          console.log('[NodeEditor] Node System initialized:', NodeLoader.getLoadedNodeNames());
        }
      } catch (error) {
        console.error('[NodeEditor] Failed to initialize Node System:', error);
      }
    };

    const loadMCPNodes = async () => {
      try {
        const configs = await generateMCPNodeConfigs();
        setMcpNodeConfigs(configs);
      } catch (error) {
        console.error('[NodeEditor] Failed to load MCP configs:', error);
      }
    };

    initializeNodeSystem();
    loadMCPNodes();
  }, []);

  // Node types configuration
  const nodeTypes: NodeTypes = useMemo(
    () => ({
      custom: CustomNode,
      'mcp-tool': MCPToolNode,
      'python-agent': PythonAgentNode,
      'results-display': ResultsDisplayNode,
      'agent-mediator': AgentMediatorNode,
      'data-source': DataSourceNode,
      'technical-indicator': TechnicalIndicatorNode,
      backtest: BacktestNode,
      optimization: OptimizationNode,
      system: SystemNode,
    }),
    []
  );

  // Handle label change
  const handleLabelChange = useCallback(
    (nodeId: string, newLabel: string) => {
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
    },
    [setNodes]
  );

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

  // Add new node
  const addNode = useCallback(
    (config: any) => {
      const isMCPNode = config.type === 'mcp-tool';
      const isResultsDisplay = config.type === 'results-display';
      const isAgentMediator = config.type === 'agent-mediator';
      const isDataSource = config.type === 'data-source';
      const isTechnicalIndicator = config.type === 'technical-indicator';

      const nodeId = `node_${Date.now()}`;

      const newNode: Node = {
        id: nodeId,
        type: isMCPNode
          ? 'mcp-tool'
          : isResultsDisplay
            ? 'results-display'
            : isAgentMediator
              ? 'agent-mediator'
              : isDataSource
                ? 'data-source'
                : isTechnicalIndicator
                  ? 'technical-indicator'
                  : 'custom',
        position: { x: Math.random() * 400 + 100, y: Math.random() * 300 + 100 },
        data: isMCPNode
            ? {
                serverId: config.serverId,
                toolName: config.toolName,
                label: config.label,
                parameters: {},
                onParameterChange: () => {},
                onExecute: () => {},
              }
            : isResultsDisplay
              ? {
                  label: config.label,
                  color: config.color,
                  inputData: undefined,
                }
              : isAgentMediator
                ? {
                    label: config.label,
                    selectedProvider: undefined,
                    customPrompt: undefined,
                    status: 'idle',
                    result: undefined,
                    error: undefined,
                    inputData: undefined,
                    onExecute: () => {},
                    onConfigChange: (newConfig: any) => {
                      setNodes((nds) =>
                        nds.map((n) =>
                          n.id === nodeId
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
                  }
                : isDataSource
                  ? {
                      label: config.label,
                      selectedConnectionId: undefined,
                      query: '',
                      status: 'idle',
                      result: undefined,
                      error: undefined,
                      onConnectionChange: (connectionId: string) => {
                        setNodes((nds) =>
                          nds.map((n) =>
                            n.id === nodeId
                              ? { ...n, data: { ...n.data, selectedConnectionId: connectionId } }
                              : n
                          )
                        );
                      },
                      onQueryChange: (query: string) => {
                        setNodes((nds) =>
                          nds.map((n) =>
                            n.id === nodeId ? { ...n, data: { ...n.data, query: query } } : n
                          )
                        );
                      },
                      onExecute: async () => {},
                    }
                  : isTechnicalIndicator
                    ? {
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
                        onExecute: () => {},
                        onParameterChange: (params: Record<string, any>) => {
                          setNodes((nds) =>
                            nds.map((n) =>
                              n.id === nodeId ? { ...n, data: { ...n.data, ...params } } : n
                            )
                          );
                        },
                      }
                    : {
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
    },
    [setNodes, handleLabelChange]
  );

  // Handle node from palette
  const handlePaletteNodeAdd = useCallback(
    (nodeType: string, nodeData: any) => {
      if (nodeData.source === 'builtin') {
        addNode(nodeData.data);
      } else if (nodeData.source === 'mcp') {
        addNode(nodeData.data);
      } else if (nodeData.source === 'agent') {
        addNode({ ...nodeData.data, type: 'python-agent' });
      } else if (nodeData.source === 'registry') {
        const registryNode = nodeData.data as INodeTypeDescription;
        const newNode: Node = {
          id: `node_${Date.now()}`,
          type: 'custom',
          position: { x: Math.random() * 400 + 100, y: Math.random() * 300 + 100 },
          data: {
            label: registryNode.displayName,
            nodeTypeName: registryNode.name,
            color: nodeData.color || '#6b7280',
            hasInput:
              (registryNode.inputs?.length || 0) > 0 || registryNode.name !== 'manualTrigger',
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
    },
    [addNode, setNodes, handleLabelChange]
  );

  // Handle drop from palette
  const onDrop = useCallback(
    (event: React.DragEvent) => {
      event.preventDefault();

      const data = event.dataTransfer.getData('application/reactflow');
      if (!data) return;

      const nodeData = JSON.parse(data);
      const reactFlowBounds = reactFlowWrapper.current?.getBoundingClientRect();
      if (!reactFlowBounds) return;

      const position = {
        x: event.clientX - reactFlowBounds.left - 150,
        y: event.clientY - reactFlowBounds.top - 25,
      };

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
            hasInput:
              (registryNode.inputs?.length || 0) > 0 || registryNode.name !== 'manualTrigger',
            hasOutput: (registryNode.outputs?.length || 0) > 0,
            status: 'idle',
            parameters: {},
            registryData: registryNode,
            onLabelChange: handleLabelChange,
          },
        };
        setNodes((nds) => [...nds, newNode]);
      } else {
        handlePaletteNodeAdd(nodeData.type, nodeData);
      }
    },
    [setNodes, handlePaletteNodeAdd, handleLabelChange]
  );

  const onDragOver = useCallback((event: React.DragEvent) => {
    event.preventDefault();
    event.dataTransfer.dropEffect = 'move';
  }, []);

  // Execute workflow
  const handleExecute = useCallback(async () => {
    if (isExecuting) {
      showWarning('A workflow is already executing. Please wait for it to complete.');
      return;
    }

    try {
      setIsExecuting(true);
      await nodeExecutionManager.executeWorkflow(nodes, edges, (nodeId, status, result) => {
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
      });
    } catch (error: any) {
      console.error('[NodeEditor] Workflow execution failed:', error);
    } finally {
      setIsExecuting(false);
    }
  }, [isExecuting, nodes, edges, setNodes, setIsExecuting]);

  // Handle keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      // Don't trigger if user is typing in an input field
      const target = event.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable) {
        return;
      }

      if (event.key === 'Delete' || event.key === 'Backspace') {
        deleteSelectedNodes();
        deleteSelectedEdges();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [deleteSelectedNodes, deleteSelectedEdges]);

  // Quick save draft handler
  const handleQuickSaveDraft = useCallback(async () => {
    if (editingDraftId) {
      try {
        await workflowService.updateDraft(
          editingDraftId,
          workflowName || 'Untitled Draft',
          workflowDescription,
          nodes,
          edges
        );
        showSuccess('Draft saved successfully');
      } catch (error: any) {
        showError('Save failed', [
          { label: 'ERROR', value: error.message }
        ]);
      }
    }
  }, [editingDraftId, workflowName, workflowDescription, nodes, edges]);

  // Handle deploy with status callback
  const handleDeploy = useCallback(() => {
    setShowDeployDialog(false);
    setWorkflowName('');
    setWorkflowDescription('');
    setEditingDraftId(null);
    setActiveView('editor');

    deployWorkflow((nodeId, status, result) => {
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
    });
  }, [deployWorkflow, setNodes, setWorkflowName, setWorkflowDescription, setEditingDraftId]);

  // Handle save draft
  const handleSaveDraft = useCallback(() => {
    saveDraft();
    setShowDeployDialog(false);
    setWorkflowName('');
    setWorkflowDescription('');
    setActiveView('workflows');
  }, [saveDraft]);

  // Handle view results wrapper
  const handleViewResultsWrapper = useCallback(
    async (workflow: any) => {
      const result = await handleViewResults(workflow);
      if ('showModal' in result && result.showModal) {
        setResultsData(result.data);
        setResultsWorkflowName(result.name);
        setShowResultsModal(true);
      } else if ('navigate' in result && result.navigate) {
        setActiveView('editor');
      }
    },
    [handleViewResults]
  );

  // Handle edit draft wrapper
  const handleEditDraftWrapper = useCallback(
    (workflow: any) => {
      handleEditDraft(workflow);
      // Don't show deploy dialog - user should edit first
      setActiveView('editor');
    },
    [handleEditDraft]
  );

  // Handle load workflow wrapper with execution support
  const handleLoadWorkflowWrapper = useCallback(
    async (loadedNodes: Node[], loadedEdges: Edge[], workflowId: string, workflow: any, shouldExecute: boolean = false) => {
      // Navigate to editor view
      setActiveView('editor');

      // Create status callback for node updates
      const statusCallback = (nodeId: string, status: string, result?: any) => {
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
      };

      // Call the original handleLoadWorkflow with execution support
      await handleLoadWorkflow(loadedNodes, loadedEdges, workflowId, workflow, shouldExecute, statusCallback);
    },
    [handleLoadWorkflow, setNodes]
  );

  // Handle parameter change for config panel
  const handleParameterChange = useCallback(
    (paramName: string, value: NodeParameterValue) => {
      if (selectedNodes.length === 1) {
        setNodes((nds) =>
          nds.map((node) => {
            if (node.id === selectedNodes[0]) {
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
      }
    },
    [selectedNodes, setNodes]
  );

  // Handle duplicate node
  const handleDuplicateNode = useCallback(() => {
    if (selectedNodes.length === 1) {
      const selectedNode = nodes.find((n) => n.id === selectedNodes[0]);
      if (selectedNode) {
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
      }
    }
  }, [selectedNodes, nodes, setNodes]);

  // Import workflow template
  const handleImportTemplate = useCallback(
    async (template: WorkflowTemplate) => {
      // Clear existing workflow first to avoid stale auto-saved data
      setNodes([]);
      setEdges([]);

      // Deep clone nodes and re-attach callbacks that can't be serialized
      const templateNodes = template.nodes.map((node) => {
        const cloned = JSON.parse(JSON.stringify(node));
        // Re-attach callback functions based on node type
        if (cloned.type === 'data-source') {
          cloned.data.onConnectionChange = () => {};
          cloned.data.onQueryChange = () => {};
          cloned.data.onExecute = () => {};
        } else if (cloned.type === 'technical-indicator') {
          cloned.data.onExecute = () => {};
          cloned.data.onParameterChange = () => {};
        } else if (cloned.type === 'agent-mediator') {
          cloned.data.onExecute = () => {};
          cloned.data.onConfigChange = () => {};
        } else if (cloned.type === 'python-agent') {
          // Ensure parameters exist
          if (!cloned.data.parameters) {
            cloned.data.parameters = {};
          }
          cloned.data.onExecute = () => {};
          cloned.data.onParameterChange = () => {};
          cloned.data.onLLMChange = () => {};
        } else if (cloned.type === 'backtest') {
          // Ensure parameters exist
          if (!cloned.data.parameters) {
            cloned.data.parameters = {};
          }
        } else if (cloned.type === 'optimization') {
          // Ensure parameters exist
          if (!cloned.data.parameters) {
            cloned.data.parameters = {};
          }
        } else if (cloned.type === 'mcp-tool') {
          // Ensure parameters exist
          if (!cloned.data.parameters) {
            cloned.data.parameters = {};
          }
          cloned.data.onExecute = () => {};
          cloned.data.onParameterChange = () => {};
        } else if (cloned.type === 'custom') {
          cloned.data.onLabelChange = handleLabelChange;
          // Ensure parameters exist for custom nodes
          if (!cloned.data.parameters) {
            cloned.data.parameters = {};
          }
        } else if (cloned.type === 'results-display') {
          // Results display nodes don't need callbacks
        }
        return cloned;
      });
      const templateEdges = JSON.parse(JSON.stringify(template.edges));

      // Use requestAnimationFrame to ensure the clear has taken effect before setting new nodes
      requestAnimationFrame(() => {
        setNodes(templateNodes);
        setEdges(templateEdges);
      });

      setWorkflowMode('new');
      setWorkflowName(template.name);
      setWorkflowDescription(template.description);
    },
    [setNodes, setEdges, setWorkflowMode, setWorkflowName, setWorkflowDescription, handleLabelChange]
  );

  // Get selected node for config panel
  const selectedNode = selectedNodes.length === 1 ? nodes.find((n) => n.id === selectedNodes[0]) : null;

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
      <NodeEditorToolbar
        activeView={activeView}
        setActiveView={setActiveView}
        showNodeMenu={showNodeMenu}
        setShowNodeMenu={setShowNodeMenu}
        isExecuting={isExecuting}
        nodes={nodes}
        edges={edges}
        selectedNodes={selectedNodes}
        workflowMode={workflowMode}
        editingDraftId={editingDraftId}
        workflowName={workflowName}
        workflowDescription={workflowDescription}
        onExecute={handleExecute}
        onClearWorkflow={clearWorkflow}
        onSaveWorkflow={saveWorkflow}
        onImportWorkflow={importWorkflow}
        onExportWorkflow={exportWorkflow}
        onDeleteSelectedNodes={deleteSelectedNodes}
        onShowDeployDialog={() => setShowDeployDialog(true)}
        onQuickSaveDraft={handleQuickSaveDraft}
        onShowTemplates={() => setShowTemplateGallery(true)}
      />

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

          {['Input', 'Processing', 'Output', 'AI', 'Analytics'].map((category) => (
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
                  {config.type === 'technical-indicator' && <TrendingUp size={16} />}
                  {config.type === 'results-display' && <Monitor size={16} />}
                  {config.type === 'agent-mediator' && <Brain size={16} />}
                  {config.type === 'backtest' && <BarChart3 size={16} />}
                  {config.type === 'optimization' && <Zap size={16} />}
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
                  color: '#ea580c',
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
                    backgroundColor: '#0a0a0a',
                    color: '#ea580c',
                    border: '1px solid #ea580c40',
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
                    e.currentTarget.style.backgroundColor = '#ea580c20';
                    e.currentTarget.style.borderColor = '#ea580c';
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = '#0a0a0a';
                    e.currentTarget.style.borderColor = '#ea580c40';
                  }}
                >
                  <span style={{ fontSize: '14px' }}>{config.icon}</span>
                  <div style={{ flex: 1, textAlign: 'left' }}>
                    <div>{config.label}</div>
                    <div style={{ fontSize: '9px', color: '#6b7280' }}>{config.serverId}</div>
                  </div>
                </button>
              ))}
            </div>
          )}
        </div>
      )}

      {/* Main Content */}
      {activeView === 'editor' ? (
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left Sidebar - Node Palette */}
          <NodePalette
            onNodeAdd={handlePaletteNodeAdd}
            isCollapsed={isPaletteCollapsed}
            onToggleCollapse={() => setIsPaletteCollapsed(!isPaletteCollapsed)}
            mcpNodeConfigs={mcpNodeConfigs}
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
              defaultEdgeOptions={DEFAULT_EDGE_OPTIONS}
            >
              <Background {...FLOW_BACKGROUND_CONFIG} />
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
                nodeColor={(node) => node.data.color || '#3b82f6'}
              />
            </ReactFlow>

            {/* Empty State Overlay */}
            {nodes.length === 0 && (
              <div
                style={{
                  position: 'absolute',
                  top: '50%',
                  left: '50%',
                  transform: 'translate(-50%, -50%)',
                  textAlign: 'center',
                  color: '#6b7280',
                  pointerEvents: 'none',
                }}
              >
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

          {/* Right Sidebar - Config Panel */}
          {selectedNode && (
            <NodeConfigPanel
              selectedNode={selectedNode}
              onLabelChange={handleLabelChange}
              onParameterChange={handleParameterChange}
              onClose={() => setSelectedNodes([])}
              onDelete={deleteSelectedNodes}
              onDuplicate={handleDuplicateNode}
            />
          )}
        </div>
      ) : (
        /* Workflow Manager View */
        <div style={{ flex: 1, overflow: 'hidden' }}>
          <WorkflowManager
            onLoadWorkflow={handleLoadWorkflowWrapper}
            onViewResults={handleViewResultsWrapper}
            onEditDraft={handleEditDraftWrapper}
          />
        </div>
      )}

      {/* Bottom Info Bar */}
      {activeView === 'editor' && (
        <TabFooter
          tabName="NODE EDITOR"
          leftInfo={[
            {
              label: 'Drag nodes to move | Drag from handles to connect | Select & Delete to remove',
              color: '#737373',
            },
            ...(selectedNodes.length > 0
              ? [{ label: `Selected: ${selectedNodes.length} node(s)`, color: '#ea580c' }]
              : []),
          ]}
          statusInfo="Scroll to zoom | Right-click drag to pan"
          backgroundColor="#1a1a1a"
          borderColor="#2d2d2d"
        />
      )}

      {/* Deploy Dialog */}
      <DeployDialog
        isOpen={showDeployDialog}
        onClose={() => setShowDeployDialog(false)}
        workflowName={workflowName}
        setWorkflowName={setWorkflowName}
        workflowDescription={workflowDescription}
        setWorkflowDescription={setWorkflowDescription}
        onDeploy={handleDeploy}
        onSaveDraft={handleSaveDraft}
      />

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

      {/* Template Gallery */}
      <TemplateGallery
        isOpen={showTemplateGallery}
        onClose={() => setShowTemplateGallery(false)}
        onImportTemplate={handleImportTemplate}
      />
    </div>
  );
}
