/**
 * Node Editor MCP Bridge
 *
 * Connects the MCP tool contexts to the actual Node Editor system.
 * Provides the implementation for all node editor MCP operations.
 */

import { terminalMCPProvider } from './TerminalMCPProvider';
import {
  TerminalContexts,
  NodeTypeInfo,
  NodeTypeDetails,
  NodeCategoryInfo,
  CreateWorkflowParams,
  AddNodeParams,
  ConnectNodesParams,
  ConfigureNodeParams,
  UpdateWorkflowParams,
  WorkflowInfo,
  WorkflowExecutionResult,
  WorkflowValidation,
  NodePropertyInfo,
  DisconnectNodesParams,
  MoveNodeParams,
  AutoLayoutParams,
  WorkflowStats,
} from './types';
import { NodeRegistry } from '@/services/nodeSystem/NodeRegistry';
import { NodeLoader, initializeNodeSystem } from '@/services/nodeSystem/NodeLoader';
import { workflowService } from '@/services/core/workflowService';
import type {
  WorkflowNode,
  WorkflowEdge,
  Workflow,
  WorkflowNodeData,
  BaseNodeData,
  DataSourceNodeData,
  TechnicalIndicatorNodeData,
  PythonAgentNodeData,
  MCPToolNodeData,
  ResultsDisplayNodeData,
  AgentMediatorNodeData,
} from '@/types/workflow';

/**
 * Category metadata for better descriptions
 */
const CATEGORY_METADATA: Record<string, { displayName: string; description: string; icon: string; color: string }> = {
  trigger: {
    displayName: 'Triggers',
    description: 'Workflow entry points - manual, scheduled, price alerts, market events',
    icon: 'Zap',
    color: '#a855f7',
  },
  marketData: {
    displayName: 'Market Data',
    description: 'Fetch real-time and historical market data from various sources',
    icon: 'Activity',
    color: '#22c55e',
  },
  trading: {
    displayName: 'Trading',
    description: 'Execute and manage trades - orders, positions, balance',
    icon: 'TrendingUp',
    color: '#14b8a6',
  },
  analytics: {
    displayName: 'Analytics',
    description: 'Financial analysis - technical indicators, portfolio optimization, backtesting',
    icon: 'BarChart2',
    color: '#06b6d4',
  },
  controlFlow: {
    displayName: 'Control Flow',
    description: 'Workflow logic - conditions, loops, branching, merging',
    icon: 'GitBranch',
    color: '#eab308',
  },
  transform: {
    displayName: 'Transform',
    description: 'Data manipulation - filter, map, aggregate, sort, join',
    icon: 'Filter',
    color: '#ec4899',
  },
  safety: {
    displayName: 'Safety',
    description: 'Risk management - position limits, loss limits, trading hours',
    icon: 'Shield',
    color: '#ef4444',
  },
  notifications: {
    displayName: 'Notifications',
    description: 'Send alerts - email, Telegram, Discord, Slack, SMS, webhooks',
    icon: 'Bell',
    color: '#f97316',
  },
  agents: {
    displayName: 'AI Agents',
    description: 'AI-powered analysis - investor personas, geopolitics, hedge fund strategies',
    icon: 'Brain',
    color: '#8b5cf6',
  },
  core: {
    displayName: 'Core',
    description: 'Basic workflow operations - filter, merge, set, switch, code',
    icon: 'Workflow',
    color: '#6b7280',
  },
};

/**
 * Map node group names to our categories
 */
function mapGroupToCategory(groups: string[] | undefined): string {
  if (!groups || !Array.isArray(groups) || groups.length === 0) {
    return 'core';
  }
  const groupLower = groups.map(g => (g || '').toLowerCase());

  if (groupLower.some(g => g.includes('trigger'))) return 'trigger';
  if (groupLower.some(g => g.includes('market') || g.includes('data') || g.includes('yfinance'))) return 'marketData';
  if (groupLower.some(g => g.includes('trading') || g.includes('order') || g.includes('position'))) return 'trading';
  if (groupLower.some(g => g.includes('analytic') || g.includes('indicator') || g.includes('backtest'))) return 'analytics';
  if (groupLower.some(g => g.includes('control') || g.includes('flow') || g.includes('logic'))) return 'controlFlow';
  if (groupLower.some(g => g.includes('transform') || g.includes('filter') || g.includes('map'))) return 'transform';
  if (groupLower.some(g => g.includes('safety') || g.includes('risk'))) return 'safety';
  if (groupLower.some(g => g.includes('notification') || g.includes('alert') || g.includes('message'))) return 'notifications';
  if (groupLower.some(g => g.includes('agent') || g.includes('ai'))) return 'agents';

  return 'core';
}

/**
 * NodeEditorMCPBridge Class
 * Singleton that provides the bridge between MCP and the Node Editor
 */
class NodeEditorMCPBridgeClass {
  private initialized = false;
  private navigateToTab?: (tab: string) => void;
  private loadWorkflowInEditor?: (workflowId: string) => void;

  /**
   * Initialize the bridge and inject contexts
   */
  async initialize(): Promise<void> {
    if (this.initialized) {
      return;
    }

    // Ensure node system is loaded
    if (!NodeLoader.isLoaded()) {
      await initializeNodeSystem();
    }

    // Build and inject contexts
    this.injectContexts();
    this.initialized = true;
    console.log('[NodeEditorMCPBridge] Initialized with node editor contexts');
  }

  /**
   * Set navigation callback (called from React component)
   */
  setNavigationCallback(navigate: (tab: string) => void): void {
    this.navigateToTab = navigate;
    // Re-inject contexts with updated navigation
    if (this.initialized) {
      this.injectContexts();
    }
  }

  /**
   * Set workflow loader callback (called from NodeEditorTab)
   */
  setWorkflowLoaderCallback(loader: (workflowId: string) => void): void {
    this.loadWorkflowInEditor = loader;
    if (this.initialized) {
      this.injectContexts();
    }
  }

  /**
   * Inject all node editor contexts into the MCP provider
   */
  private injectContexts(): void {
    const contexts: Partial<TerminalContexts> = {
      // Node Discovery
      listAvailableNodes: this.listAvailableNodes.bind(this),
      getNodeDetails: this.getNodeDetails.bind(this),
      getNodeCategories: this.getNodeCategories.bind(this),

      // Workflow Creation
      createNodeWorkflow: this.createNodeWorkflow.bind(this),
      addNodeToWorkflow: this.addNodeToWorkflow.bind(this),
      connectNodes: this.connectNodes.bind(this),
      configureNode: this.configureNode.bind(this),

      // Workflow Management
      listNodeWorkflows: this.listNodeWorkflows.bind(this),
      getNodeWorkflow: this.getNodeWorkflow.bind(this),
      updateNodeWorkflow: this.updateNodeWorkflow.bind(this),
      deleteNodeWorkflow: this.deleteNodeWorkflow.bind(this),
      duplicateNodeWorkflow: this.duplicateNodeWorkflow.bind(this),

      // Workflow Execution
      executeNodeWorkflow: this.executeNodeWorkflow.bind(this),
      stopNodeWorkflow: this.stopNodeWorkflow.bind(this),
      getWorkflowResults: this.getWorkflowResults.bind(this),
      validateNodeWorkflow: this.validateNodeWorkflow.bind(this),

      // Helpers
      openWorkflowInEditor: this.openWorkflowInEditor.bind(this),
      exportNodeWorkflow: this.exportNodeWorkflow.bind(this),
      importNodeWorkflow: this.importNodeWorkflow.bind(this),

      // Quick Edit Operations
      removeNodeFromWorkflow: this.removeNodeFromWorkflow.bind(this),
      disconnectNodes: this.disconnectNodes.bind(this),
      renameNode: this.renameNode.bind(this),
      clearWorkflow: this.clearWorkflow.bind(this),
      cloneNode: this.cloneNode.bind(this),
      moveNode: this.moveNode.bind(this),
      getWorkflowStats: this.getWorkflowStats.bind(this),
      autoLayoutWorkflow: this.autoLayoutWorkflow.bind(this),
    };

    terminalMCPProvider.setContexts(contexts);
  }

  // ============================================================================
  // NODE DISCOVERY METHODS
  // ============================================================================

  async listAvailableNodes(category?: string, search?: string): Promise<NodeTypeInfo[]> {
    let nodes = NodeRegistry.getAllNodes() || [];

    // Filter by category
    if (category && nodes.length > 0) {
      nodes = nodes.filter(node => {
        const nodeCategory = mapGroupToCategory(node.group);
        return nodeCategory === category;
      });
    }

    // Filter by search
    if (search && nodes.length > 0) {
      const searchLower = search.toLowerCase();
      nodes = nodes.filter(node =>
        node.name?.toLowerCase().includes(searchLower) ||
        node.displayName?.toLowerCase().includes(searchLower) ||
        node.description?.toLowerCase().includes(searchLower)
      );
    }

    return nodes.map(node => ({
      name: node.name,
      displayName: node.displayName,
      description: node.description,
      category: mapGroupToCategory(node.group),
      icon: node.icon,
      color: node.defaults?.color,
      inputs: Array.isArray(node.inputs) ? node.inputs.map(i => typeof i === 'string' ? i : 'main') : ['main'],
      outputs: Array.isArray(node.outputs)
        ? node.outputs.map(o => typeof o === 'string' ? o : (o as any).type || 'main')
        : ['main'],
    }));
  }

  async getNodeDetails(nodeType: string): Promise<NodeTypeDetails | null> {
    try {
      const description = NodeRegistry.getNodeTypeDescription(nodeType);
      if (!description) return null;

      const properties: NodePropertyInfo[] = (description.properties || []).map(prop => ({
        name: prop.name,
        displayName: prop.displayName,
        type: prop.type,
        default: prop.default,
        description: prop.description,
        required: prop.required,
        options: prop.options && Array.isArray(prop.options)
          ? prop.options.map((opt: any) => ({
              name: opt.name || opt.displayName || String(opt.value),
              value: opt.value,
              description: opt.description,
            }))
          : undefined,
      }));

      return {
        name: description.name,
        displayName: description.displayName,
        description: description.description,
        category: mapGroupToCategory(description.group),
        icon: description.icon,
        color: description.defaults?.color,
        inputs: Array.isArray(description.inputs) ? description.inputs.map(i => typeof i === 'string' ? i : 'main') : ['main'],
        outputs: Array.isArray(description.outputs)
          ? description.outputs.map(o => typeof o === 'string' ? o : (o as any).type || 'main')
          : ['main'],
        version: Array.isArray(description.version) ? description.version[0] : description.version,
        properties,
        hints: description.hints?.map(h => h.message),
        usableAsTool: description.usableAsTool,
      };
    } catch {
      return null;
    }
  }

  async getNodeCategories(): Promise<NodeCategoryInfo[]> {
    const allNodes = NodeRegistry.getAllNodes() || [];
    const categoryCounts: Record<string, number> = {};

    // Count nodes per category
    for (const node of allNodes) {
      const category = mapGroupToCategory(node.group);
      categoryCounts[category] = (categoryCounts[category] || 0) + 1;
    }

    // Build category info with metadata
    return Object.entries(CATEGORY_METADATA).map(([name, meta]) => ({
      name,
      displayName: meta.displayName,
      description: meta.description,
      nodeCount: categoryCounts[name] || 0,
      icon: meta.icon,
      color: meta.color,
    }));
  }

  // ============================================================================
  // WORKFLOW CREATION METHODS
  // ============================================================================

  async createNodeWorkflow(params: CreateWorkflowParams): Promise<WorkflowInfo> {
    const id = `workflow_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;

    // Convert input nodes to WorkflowNode format with proper ReactFlow data structure
    // This handles both UI types (data-source, system, etc.) and backend types (yfinance, manualTrigger, etc.)
    const nodes: WorkflowNode[] = (params.nodes || []).map(n => {
      // Map the provided type to proper UI type
      const uiType = this.mapToUINodeType(n.type || 'custom');
      const backendType = n.parameters?.nodeType || n.type;

      return {
        id: n.id,
        type: uiType,
        position: n.position,
        data: this.createNodeData(
          uiType,
          backendType,
          n.label || n.type || 'Node',
          n.parameters || {}
        ),
      };
    });

    // Convert input edges to WorkflowEdge format with proper styling
    const edges: WorkflowEdge[] = (params.edges || []).map((e, idx) => ({
      id: `edge_${idx}_${e.source}_${e.target}`,
      source: e.source,
      target: e.target,
      sourceHandle: e.sourceHandle || null,
      targetHandle: e.targetHandle || null,
      animated: true,
      style: { stroke: '#ea580c', strokeWidth: 2 },
    }));

    const workflow: Workflow = {
      id,
      name: params.name,
      description: params.description,
      nodes,
      edges,
      status: 'draft',
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };

    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  /**
   * Get default color for a node type (both UI types and backend types)
   */
  private getNodeColor(nodeType: string): string {
    const colorMap: Record<string, string> = {
      // UI node types
      'data-source': '#3b82f6',
      'technical-indicator': '#10b981',
      'agent-mediator': '#8b5cf6',
      'results-display': '#f59e0b',
      'backtest': '#06b6d4',
      'optimization': '#a855f7',
      'system': '#10b981',
      'custom': '#6b7280',
      'mcp-tool': '#ea580c',
      'python-agent': '#22c55e',

      // Backend node types mapped to colors
      'manualTrigger': '#10b981',
      'scheduleTrigger': '#a855f7',
      'priceAlertTrigger': '#f97316',
      'newsEventTrigger': '#f97316',
      'marketEventTrigger': '#f97316',
      'webhookTrigger': '#f97316',
      'yfinance': '#3b82f6',
      'getQuote': '#3b82f6',
      'getHistoricalData': '#3b82f6',
      'getMarketDepth': '#3b82f6',
      'streamQuotes': '#3b82f6',
      'getFundamentals': '#3b82f6',
      'getTickerStats': '#3b82f6',
      'stockData': '#3b82f6',
      'technicalIndicators': '#10b981',
      'placeOrder': '#14b8a6',
      'modifyOrder': '#14b8a6',
      'cancelOrder': '#14b8a6',
      'getPositions': '#14b8a6',
      'getHoldings': '#14b8a6',
      'getOrders': '#14b8a6',
      'getBalance': '#14b8a6',
      'closePosition': '#14b8a6',
      'portfolioOptimization': '#06b6d4',
      'backtestEngine': '#06b6d4',
      'riskAnalysis': '#06b6d4',
      'performanceMetrics': '#06b6d4',
      'correlationMatrix': '#06b6d4',
      'ifElse': '#eab308',
      'switch': '#eab308',
      'loop': '#eab308',
      'wait': '#eab308',
      'merge': '#eab308',
      'split': '#eab308',
      'errorHandler': '#ef4444',
      'filterTransform': '#ec4899',
      'map': '#ec4899',
      'aggregate': '#ec4899',
      'sort': '#ec4899',
      'groupBy': '#ec4899',
      'join': '#ec4899',
      'deduplicate': '#ec4899',
      'reshape': '#ec4899',
      'riskCheck': '#ef4444',
      'positionSizeLimit': '#ef4444',
      'lossLimit': '#ef4444',
      'tradingHoursCheck': '#ef4444',
      'webhookNotification': '#f97316',
      'email': '#f97316',
      'telegram': '#f97316',
      'discord': '#f97316',
      'slack': '#f97316',
      'sms': '#f97316',
      'agentMediator': '#8b5cf6',
      'agent': '#8b5cf6',
      'multiAgent': '#8b5cf6',
      'aiAgent': '#8b5cf6',
      'filter': '#6b7280',
      'set': '#6b7280',
      'code': '#6b7280',
      'resultsDisplay': '#f59e0b',
    };
    return colorMap[nodeType] || '#6b7280';
  }

  /**
   * Map backend node types to UI component types
   * This is critical for proper rendering in ReactFlow
   *
   * NOTE: We use 'custom' for most MCP-created nodes because:
   * - 'system' type expects INode data structure from n8n node system
   * - 'data-source' type expects callback functions (onConnectionChange, etc.)
   * - 'custom' type works with simple data (label, color, nodeType, hasInput, hasOutput)
   */
  private mapToUINodeType(backendType: string): string {
    // Trigger nodes - use 'custom' instead of 'system' because SystemNode
    // expects data.node with INode structure from n8n node system
    if (backendType.includes('Trigger') || backendType === 'manual') {
      return 'custom';
    }

    // Data source nodes - use 'custom' instead of 'data-source' because
    // DataSourceNode expects callback functions (onConnectionChange, onQueryChange, etc.)
    const dataSourceNodes = [
      'yfinance', 'getQuote', 'getHistoricalData', 'getMarketDepth',
      'streamQuotes', 'getFundamentals', 'getTickerStats', 'stockData'
    ];
    if (dataSourceNodes.includes(backendType)) {
      return 'custom';
    }

    // Technical indicator nodes - use 'custom' for same reason
    if (backendType === 'technicalIndicators' || backendType.includes('indicator')) {
      return 'custom';
    }

    // Agent nodes - these can stay as agent-mediator if they work
    const agentNodes = [
      'agentMediator', 'agent', 'aiAgent', 'multiAgent'
    ];
    if (agentNodes.includes(backendType)) {
      return 'custom'; // Use custom for simplicity in MCP-created workflows
    }

    // Results display - this one works with simple data
    if (backendType === 'resultsDisplay' || backendType === 'results-display') {
      return 'results-display';
    }

    // Backtest nodes - use custom
    if (backendType === 'backtestEngine' || backendType === 'backtest') {
      return 'custom';
    }

    // Optimization nodes - use custom
    if (backendType === 'portfolioOptimization' || backendType === 'optimization') {
      return 'custom';
    }

    // If it's already a UI type that's safe to use, return as-is
    // Only results-display is safe without callbacks
    const safeUiTypes = ['results-display', 'custom'];
    if (safeUiTypes.includes(backendType)) {
      return backendType;
    }

    // Default to custom for everything else - it's the most flexible
    return 'custom';
  }

  /**
   * Create proper ReactFlow node data structure for any node type
   * Returns data compatible with CustomNode (the most flexible node type)
   *
   * CustomNode expects:
   * - label: string
   * - color: string
   * - nodeType: string
   * - hasInput: boolean
   * - hasOutput: boolean
   * - status?: string
   * - onLabelChange?: function (optional)
   */
  private createNodeData(
    nodeType: string,
    backendType: string,
    label: string,
    parameters: Record<string, any> = {}
  ): WorkflowNodeData {
    const uiType = this.mapToUINodeType(nodeType);
    const color = parameters.color || this.getNodeColor(backendType || nodeType);

    // Determine input/output based on backend type (triggers have no input)
    const isTrigger = (backendType || nodeType || '').toLowerCase().includes('trigger') ||
                      backendType === 'manual' ||
                      backendType === 'manualTrigger';
    const isResultsDisplay = uiType === 'results-display' ||
                             backendType === 'resultsDisplay';

    const hasInput = !isTrigger;
    const hasOutput = !isResultsDisplay;

    // Build node data compatible with CustomNode
    // CustomNode is the most flexible and doesn't require callbacks
    const nodeData = {
      // Required by CustomNode
      label: label || backendType || nodeType || 'Node',
      color: color,
      nodeType: backendType || nodeType || 'custom',
      hasInput: hasInput,
      hasOutput: hasOutput,
      status: parameters.status || 'idle',

      // Optional - these will be ignored if not used
      onLabelChange: undefined as ((nodeId: string, newLabel: string) => void) | undefined,

      // Spread any additional parameters (symbol, operation, etc.)
      ...parameters,
    };

    // Remove undefined onLabelChange to keep data clean
    if (nodeData.onLabelChange === undefined) {
      delete (nodeData as any).onLabelChange;
    }

    // Return as BaseNodeData which CustomNode accepts
    return nodeData as BaseNodeData;
  }

  async addNodeToWorkflow(params: AddNodeParams): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    // Map the provided node type to proper UI type
    const uiType = this.mapToUINodeType(params.nodeType);
    const backendType = params.parameters?.nodeType || params.nodeType;

    const newNode: WorkflowNode = {
      id: params.nodeId,
      type: uiType,
      position: params.position,
      data: this.createNodeData(
        uiType,
        backendType,
        params.label || params.nodeType,
        params.parameters || {}
      ),
    };

    // Ensure nodes array exists
    if (!workflow.nodes) {
      workflow.nodes = [];
    }
    workflow.nodes.push(newNode);
    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async connectNodes(params: ConnectNodesParams): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    const newEdge: WorkflowEdge = {
      id: `edge_${params.sourceNodeId}_${params.targetNodeId}_${Date.now()}`,
      source: params.sourceNodeId,
      target: params.targetNodeId,
      sourceHandle: params.sourceHandle || 'main',
      targetHandle: params.targetHandle || 'main',
    };

    // Ensure edges array exists
    if (!workflow.edges) {
      workflow.edges = [];
    }
    workflow.edges.push(newEdge);
    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async configureNode(params: ConfigureNodeParams): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    const nodes = workflow.nodes || [];
    const node = nodes.find(n => n.id === params.nodeId);
    if (!node) {
      throw new Error(`Node '${params.nodeId}' not found in workflow`);
    }

    // Merge parameters into node data
    node.data = {
      ...node.data,
      ...params.parameters,
    };

    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  // ============================================================================
  // WORKFLOW MANAGEMENT METHODS
  // ============================================================================

  async listNodeWorkflows(status?: string): Promise<WorkflowInfo[]> {
    let workflows = await workflowService.listWorkflows() || [];

    if (status && workflows.length > 0) {
      workflows = workflows.filter(w => w.status === status);
    }

    return workflows.map(w => this.workflowToInfo(w));
  }

  async getNodeWorkflow(workflowId: string): Promise<WorkflowInfo | null> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    return workflow ? this.workflowToInfo(workflow) : null;
  }

  async updateNodeWorkflow(params: UpdateWorkflowParams): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    if (params.name !== undefined) {
      workflow.name = params.name;
    }
    if (params.description !== undefined) {
      workflow.description = params.description;
    }
    if (params.nodes !== undefined) {
      // Map all nodes to proper UI types with full ReactFlow data structure
      workflow.nodes = (params.nodes || []).map(n => {
        const uiType = this.mapToUINodeType(n.type || 'custom');
        const backendType = n.parameters?.nodeType || n.type;

        return {
          id: n.id,
          type: uiType,
          position: n.position,
          data: this.createNodeData(
            uiType,
            backendType,
            n.label || n.type || 'Node',
            n.parameters || {}
          ),
        };
      });
    }
    if (params.edges !== undefined) {
      workflow.edges = (params.edges || []).map((e, idx) => ({
        id: `edge_${idx}_${e.source}_${e.target}`,
        source: e.source,
        target: e.target,
        sourceHandle: e.sourceHandle || null,
        targetHandle: e.targetHandle || null,
        animated: true,
        style: { stroke: '#ea580c', strokeWidth: 2 },
      }));
    }

    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async deleteNodeWorkflow(workflowId: string): Promise<void> {
    await workflowService.deleteWorkflow(workflowId);
  }

  async duplicateNodeWorkflow(workflowId: string, newName: string): Promise<WorkflowInfo> {
    const original = await workflowService.loadWorkflow(workflowId);
    if (!original) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    const newId = `workflow_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    const duplicate: Workflow = {
      ...original,
      id: newId,
      name: newName,
      status: 'draft',
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };

    await workflowService.saveWorkflow(duplicate);

    return this.workflowToInfo(duplicate);
  }

  // ============================================================================
  // WORKFLOW EXECUTION METHODS
  // ============================================================================

  async executeNodeWorkflow(workflowId: string, _inputData?: any): Promise<WorkflowExecutionResult> {
    const startTime = Date.now();

    try {
      await workflowService.executeWorkflow(workflowId);

      // Wait a bit for execution to complete (simplified - real implementation would use proper async handling)
      await new Promise(resolve => setTimeout(resolve, 200));

      const workflow = await workflowService.loadWorkflow(workflowId);

      return {
        success: workflow?.status === 'completed',
        workflowId,
        results: workflow?.results,
        duration: Date.now() - startTime,
      };
    } catch (error: any) {
      return {
        success: false,
        workflowId,
        error: error.message || 'Execution failed',
        duration: Date.now() - startTime,
      };
    }
  }

  async stopNodeWorkflow(workflowId: string): Promise<void> {
    await workflowService.stopWorkflow(workflowId);
  }

  async getWorkflowResults(workflowId: string): Promise<WorkflowExecutionResult | null> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) return null;

    return {
      success: workflow.status === 'completed',
      workflowId,
      results: workflow.results,
    };
  }

  async validateNodeWorkflow(workflowId: string): Promise<WorkflowValidation> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      return { valid: false, errors: [`Workflow '${workflowId}' not found`] };
    }

    const errors: string[] = [];
    const warnings: string[] = [];
    const nodes = workflow.nodes || [];
    const edges = workflow.edges || [];

    // Check for empty workflow
    if (nodes.length === 0) {
      errors.push('Workflow has no nodes');
    }

    // Check for trigger node
    const hasTrigger = nodes.some(n =>
      n.type?.toLowerCase().includes('trigger') || n.type?.toLowerCase().includes('manual')
    );
    if (!hasTrigger && nodes.length > 0) {
      warnings.push('Workflow has no trigger node - it will need to be started manually');
    }

    // Check for disconnected nodes
    const connectedNodes = new Set<string>();
    for (const edge of edges) {
      connectedNodes.add(edge.source);
      connectedNodes.add(edge.target);
    }
    for (const node of nodes) {
      if (nodes.length > 1 && !connectedNodes.has(node.id)) {
        warnings.push(`Node '${node.id}' is not connected to any other node`);
      }
    }

    // Check for invalid node types
    for (const node of nodes) {
      if (node.type && !NodeRegistry.hasNodeType(node.type)) {
        errors.push(`Unknown node type '${node.type}' in node '${node.id}'`);
      }
    }

    // Check for broken edges
    const nodeIds = new Set(nodes.map(n => n.id));
    for (const edge of edges) {
      if (!nodeIds.has(edge.source)) {
        errors.push(`Edge references non-existent source node '${edge.source}'`);
      }
      if (!nodeIds.has(edge.target)) {
        errors.push(`Edge references non-existent target node '${edge.target}'`);
      }
    }

    return {
      valid: errors.length === 0,
      errors,
      warnings: warnings.length > 0 ? warnings : undefined,
    };
  }

  // ============================================================================
  // HELPER METHODS
  // ============================================================================

  async openWorkflowInEditor(workflowId: string): Promise<void> {
    // Navigate to Node Editor tab (tab value is 'nodes')
    if (this.navigateToTab) {
      this.navigateToTab('nodes');
    }

    // Load the workflow in the editor
    if (this.loadWorkflowInEditor) {
      // Small delay to ensure tab is active
      await new Promise(resolve => setTimeout(resolve, 100));
      this.loadWorkflowInEditor(workflowId);
    }
  }

  async exportNodeWorkflow(workflowId: string): Promise<any> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    // Return a clean export format
    return {
      name: workflow.name,
      description: workflow.description,
      nodes: (workflow.nodes || []).map(n => ({
        id: n.id,
        type: n.type,
        position: n.position,
        parameters: n.data,
      })),
      edges: (workflow.edges || []).map(e => ({
        source: e.source,
        target: e.target,
        sourceHandle: e.sourceHandle,
        targetHandle: e.targetHandle,
      })),
      exportedAt: new Date().toISOString(),
      version: '1.0',
    };
  }

  async importNodeWorkflow(workflowData: any): Promise<WorkflowInfo> {
    // Create a new workflow from the imported data
    return this.createNodeWorkflow({
      name: workflowData.name || 'Imported Workflow',
      description: workflowData.description,
      nodes: (workflowData.nodes || []).map((n: any) => ({
        id: n.id,
        type: n.type,
        position: n.position || { x: 100, y: 100 },
        parameters: n.parameters || n.data,
        label: n.label,
      })),
      edges: (workflowData.edges || []).map((e: any) => ({
        source: e.source,
        target: e.target,
        sourceHandle: e.sourceHandle,
        targetHandle: e.targetHandle,
      })),
    });
  }

  // ============================================================================
  // QUICK EDIT OPERATIONS
  // ============================================================================

  async removeNodeFromWorkflow(workflowId: string, nodeId: string): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    // Remove the node
    workflow.nodes = (workflow.nodes || []).filter(n => n.id !== nodeId);

    // Remove all edges connected to this node
    workflow.edges = (workflow.edges || []).filter(
      e => e.source !== nodeId && e.target !== nodeId
    );

    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async disconnectNodes(params: DisconnectNodesParams): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    // Remove matching edge(s)
    workflow.edges = (workflow.edges || []).filter(e => {
      const isMatch = e.source === params.sourceNodeId && e.target === params.targetNodeId;
      // If sourceHandle is specified, also check that
      if (isMatch && params.sourceHandle) {
        return e.sourceHandle !== params.sourceHandle;
      }
      return !isMatch;
    });

    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async renameNode(workflowId: string, nodeId: string, newLabel: string): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    const node = (workflow.nodes || []).find(n => n.id === nodeId);
    if (!node) {
      throw new Error(`Node '${nodeId}' not found in workflow`);
    }

    // Update the label in node data
    if (node.data) {
      node.data.label = newLabel;
    }

    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async clearWorkflow(workflowId: string): Promise<WorkflowInfo> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    // Clear all nodes and edges
    workflow.nodes = [];
    workflow.edges = [];
    workflow.status = 'draft';
    workflow.updatedAt = new Date().toISOString();

    await workflowService.saveWorkflow(workflow);

    return this.workflowToInfo(workflow);
  }

  async cloneNode(
    workflowId: string,
    nodeId: string,
    newLabel?: string
  ): Promise<{ workflow: WorkflowInfo; newNodeId: string }> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    const originalNode = (workflow.nodes || []).find(n => n.id === nodeId);
    if (!originalNode) {
      throw new Error(`Node '${nodeId}' not found in workflow`);
    }

    // Generate new node ID
    const newNodeId = `${nodeId}_clone_${Date.now()}`;

    // Clone the node with new position (offset to the right)
    const clonedNode: WorkflowNode = {
      ...originalNode,
      id: newNodeId,
      position: {
        x: originalNode.position.x + 50,
        y: originalNode.position.y + 50,
      },
      data: {
        ...originalNode.data,
        label: newLabel || `Copy of ${originalNode.data?.label || nodeId}`,
      },
    };

    workflow.nodes.push(clonedNode);
    workflow.updatedAt = new Date().toISOString();

    await workflowService.saveWorkflow(workflow);

    return {
      workflow: this.workflowToInfo(workflow),
      newNodeId,
    };
  }

  async moveNode(params: MoveNodeParams): Promise<{ workflow: WorkflowInfo; newPosition: { x: number; y: number } }> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    const node = (workflow.nodes || []).find(n => n.id === params.nodeId);
    if (!node) {
      throw new Error(`Node '${params.nodeId}' not found in workflow`);
    }

    let newX = node.position.x;
    let newY = node.position.y;
    const moveOffset = 50;

    // Handle direction-based movement
    if (params.direction) {
      switch (params.direction) {
        case 'up': newY -= moveOffset; break;
        case 'down': newY += moveOffset; break;
        case 'left': newX -= moveOffset; break;
        case 'right': newX += moveOffset; break;
      }
    } else if (params.x !== undefined || params.y !== undefined) {
      // Handle coordinate-based movement
      if (params.relative) {
        newX += params.x || 0;
        newY += params.y || 0;
      } else {
        newX = params.x ?? newX;
        newY = params.y ?? newY;
      }
    }

    node.position = { x: newX, y: newY };
    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return {
      workflow: this.workflowToInfo(workflow),
      newPosition: { x: newX, y: newY },
    };
  }

  async getWorkflowStats(workflowId: string): Promise<WorkflowStats> {
    const workflow = await workflowService.loadWorkflow(workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${workflowId}' not found`);
    }

    // Derive stats from current workflow state
    // In a full implementation, this would query execution history from database
    const isCompleted = workflow.status === 'completed';
    const isError = workflow.status === 'error';
    const isRunning = workflow.status === 'running';
    const hasRun = isCompleted || isError;

    // Count nodes and edges as a proxy for complexity
    const nodeCount = (workflow.nodes || []).length;
    const edgeCount = (workflow.edges || []).length;

    return {
      workflowId,
      totalRuns: hasRun ? 1 : 0,
      successfulRuns: isCompleted ? 1 : 0,
      failedRuns: isError ? 1 : 0,
      successRate: isCompleted ? 100 : (isError ? 0 : 0),
      lastRunAt: hasRun || isRunning ? workflow.updatedAt : undefined,
      lastStatus: workflow.status,
      averageDuration: undefined,
    };
  }

  async autoLayoutWorkflow(params: AutoLayoutParams): Promise<{ workflow: WorkflowInfo; nodesRepositioned: number }> {
    const workflow = await workflowService.loadWorkflow(params.workflowId);
    if (!workflow) {
      throw new Error(`Workflow '${params.workflowId}' not found`);
    }

    const nodes = workflow.nodes || [];
    const edges = workflow.edges || [];
    if (nodes.length === 0) {
      return { workflow: this.workflowToInfo(workflow), nodesRepositioned: 0 };
    }

    const spacingX = params.spacingX || 250;
    const spacingY = params.spacingY || 100;
    const isHorizontal = params.direction !== 'vertical';

    // Build adjacency map
    const outgoing = new Map<string, string[]>();
    const incoming = new Map<string, string[]>();
    for (const edge of edges) {
      if (!outgoing.has(edge.source)) outgoing.set(edge.source, []);
      outgoing.get(edge.source)!.push(edge.target);
      if (!incoming.has(edge.target)) incoming.set(edge.target, []);
      incoming.get(edge.target)!.push(edge.source);
    }

    // Find root nodes (no incoming edges)
    const rootNodes = nodes.filter(n => !incoming.has(n.id) || incoming.get(n.id)!.length === 0);

    // BFS to assign levels
    const levels = new Map<string, number>();
    const queue: string[] = rootNodes.map(n => n.id);
    rootNodes.forEach(n => levels.set(n.id, 0));

    while (queue.length > 0) {
      const nodeId = queue.shift()!;
      const level = levels.get(nodeId)!;
      const children = outgoing.get(nodeId) || [];
      for (const childId of children) {
        if (!levels.has(childId) || levels.get(childId)! < level + 1) {
          levels.set(childId, level + 1);
          queue.push(childId);
        }
      }
    }

    // Assign levels to disconnected nodes
    let maxLevel = Math.max(0, ...levels.values());
    for (const node of nodes) {
      if (!levels.has(node.id)) {
        maxLevel++;
        levels.set(node.id, maxLevel);
      }
    }

    // Group nodes by level
    const levelGroups = new Map<number, typeof nodes>();
    for (const node of nodes) {
      const level = levels.get(node.id) || 0;
      if (!levelGroups.has(level)) levelGroups.set(level, []);
      levelGroups.get(level)!.push(node);
    }

    // Position nodes
    let nodesRepositioned = 0;
    const startX = 100;
    const startY = 100;

    for (const [level, group] of levelGroups) {
      group.forEach((node, idx) => {
        const oldPos = { ...node.position };
        if (isHorizontal) {
          node.position = {
            x: startX + level * spacingX,
            y: startY + idx * spacingY,
          };
        } else {
          node.position = {
            x: startX + idx * spacingX,
            y: startY + level * spacingY,
          };
        }
        if (oldPos.x !== node.position.x || oldPos.y !== node.position.y) {
          nodesRepositioned++;
        }
      });
    }

    workflow.updatedAt = new Date().toISOString();
    await workflowService.saveWorkflow(workflow);

    return {
      workflow: this.workflowToInfo(workflow),
      nodesRepositioned,
    };
  }

  // ============================================================================
  // UTILITY METHODS
  // ============================================================================

  private workflowToInfo(workflow: Workflow): WorkflowInfo {
    return {
      id: workflow.id,
      name: workflow.name,
      description: workflow.description,
      status: workflow.status,
      nodes: (workflow.nodes || []).map(n => ({
        id: n.id,
        type: n.type || 'unknown',
        position: n.position,
        parameters: n.data,
        label: n.data?.label as string,
      })),
      edges: this.edgesToEdgeInputs(workflow.edges || []),
      createdAt: workflow.createdAt,
      updatedAt: workflow.updatedAt,
    };
  }

  /**
   * Convert workflow edges to WorkflowEdgeInput format
   * Handles null -> undefined conversion for sourceHandle/targetHandle
   */
  private edgesToEdgeInputs(edges: WorkflowEdge[]): import('./types').WorkflowEdgeInput[] {
    return (edges || []).map(e => ({
      source: e.source,
      target: e.target,
      sourceHandle: e.sourceHandle ?? undefined,
      targetHandle: e.targetHandle ?? undefined,
    }));
  }
}

// Export singleton instance
export const nodeEditorMCPBridge = new NodeEditorMCPBridgeClass();
