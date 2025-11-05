// Node Execution Manager - Handles workflow execution with topological sorting
import { Node, Edge } from 'reactflow';
import { pythonAgentService } from './pythonAgentService';
import { mcpToolService } from './mcpToolService';
import { agentLLMService } from './agentLLMService';

export type NodeStatus = 'idle' | 'running' | 'completed' | 'error';

export interface ExecutionResult {
  nodeId: string;
  success: boolean;
  data?: any;
  error?: string;
  execution_time_ms?: number;
}

class NodeExecutionManager {
  /**
   * Topological sort of nodes based on edges
   */
  topologicalSort(nodes: Node[], edges: Edge[]): Node[] {
    const adjList = new Map<string, string[]>();
    const inDegree = new Map<string, number>();

    // Initialize
    nodes.forEach(node => {
      adjList.set(node.id, []);
      inDegree.set(node.id, 0);
    });

    // Build adjacency list and in-degree
    edges.forEach(edge => {
      adjList.get(edge.source)?.push(edge.target);
      inDegree.set(edge.target, (inDegree.get(edge.target) || 0) + 1);
    });

    // Find nodes with no dependencies
    const queue: Node[] = [];
    nodes.forEach(node => {
      if (inDegree.get(node.id) === 0) {
        queue.push(node);
      }
    });

    const sorted: Node[] = [];

    while (queue.length > 0) {
      const node = queue.shift()!;
      sorted.push(node);

      const neighbors = adjList.get(node.id) || [];
      neighbors.forEach(neighborId => {
        const newDegree = (inDegree.get(neighborId) || 0) - 1;
        inDegree.set(neighborId, newDegree);

        if (newDegree === 0) {
          const neighborNode = nodes.find(n => n.id === neighborId);
          if (neighborNode) {
            queue.push(neighborNode);
          }
        }
      });
    }

    // Check for cycles
    if (sorted.length !== nodes.length) {
      throw new Error('Workflow contains cycles - cannot execute');
    }

    return sorted;
  }

  /**
   * Get inputs for a node from connected edges
   */
  getNodeInputs(
    node: Node,
    edges: Edge[],
    results: Map<string, any>
  ): Record<string, any> {
    const inputs: Record<string, any> = {};

    // Find incoming edges
    const incomingEdges = edges.filter(e => e.target === node.id);

    // Support multiple inputs for agent-mediator nodes
    if (node.type === 'agent-mediator' && incomingEdges.length > 1) {
      // Collect all agent outputs for synthesis
      const agentOutputs: Array<{ agentName: string; output: any }> = [];

      for (const edge of incomingEdges) {
        const sourceResult = results.get(edge.source);
        const sourceNode = results.get(`${edge.source}_node`); // Store node info separately if needed

        if (sourceResult !== undefined) {
          agentOutputs.push({
            agentName: sourceResult.agent || `Agent ${edge.source}`,
            output: sourceResult
          });
        }
      }

      return { multipleAgents: agentOutputs };
    }

    // Single input handling (default)
    for (const edge of incomingEdges) {
      const sourceResult = results.get(edge.source);

      if (sourceResult !== undefined) {
        // If edge has targetHandle (specific parameter), map it
        if (edge.targetHandle) {
          inputs[edge.targetHandle] = sourceResult;
        } else {
          // Otherwise merge entire result
          if (typeof sourceResult === 'object' && !Array.isArray(sourceResult)) {
            Object.assign(inputs, sourceResult);
          } else {
            inputs.data = sourceResult;
          }
        }
      }
    }

    return inputs;
  }

  /**
   * Execute a single node
   */
  async executeNode(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    try {
      switch (node.type) {
        case 'python-agent':
          return await this.executePythonAgent(node, inputs);

        case 'mcp-tool':
          return await this.executeMCPTool(node, inputs);

        case 'agent-mediator':
          return await this.executeAgentMediator(node, inputs);

        case 'data-source':
          return await this.executeDataSource(node);

        case 'transformation':
          return await this.executeTransformation(node, inputs);

        case 'results-display':
          return await this.executeResultsDisplay(node, inputs);

        case 'custom':
          // Custom nodes (default examples) - skip or pass through
          return {
            nodeId: node.id,
            success: true,
            data: inputs
          };

        default:
          console.warn(`Unknown node type: ${node.type}, skipping execution`);
          return {
            nodeId: node.id,
            success: true,
            data: inputs
          };
      }
    } catch (error: any) {
      return {
        nodeId: node.id,
        success: false,
        error: error.message || 'Unknown error'
      };
    }
  }

  /**
   * Execute Python agent node
   */
  private async executePythonAgent(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    const { agentType, parameters } = node.data;

    const result = await pythonAgentService.executeAgent(
      agentType,
      parameters,
      inputs
    );

    return {
      nodeId: node.id,
      success: result.success,
      data: result.data,
      error: result.error,
      execution_time_ms: result.execution_time_ms
    };
  }

  /**
   * Execute MCP tool node
   */
  private async executeMCPTool(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    const { serverId, toolName, parameters } = node.data;

    // Merge manual parameters with inputs from connected nodes
    const mergedParams = {
      ...parameters,
      ...inputs
    };

    const result = await mcpToolService.executeToolDirect(
      serverId,
      toolName,
      mergedParams
    );

    return {
      nodeId: node.id,
      success: result.success,
      data: result.data,
      error: result.error,
      execution_time_ms: result.executionTime
    };
  }

  /**
   * Execute data source node (placeholder)
   */
  private async executeDataSource(node: Node): Promise<ExecutionResult> {
    // TODO: Integrate with dataSourceRegistry
    return {
      nodeId: node.id,
      success: true,
      data: { message: 'Data source execution not yet implemented' }
    };
  }

  /**
   * Execute transformation node (placeholder)
   */
  private async executeTransformation(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    // TODO: Implement transformation logic
    return {
      nodeId: node.id,
      success: true,
      data: inputs // Pass through for now
    };
  }

  /**
   * Execute agent mediator node
   */
  private async executeAgentMediator(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    const { selectedProvider, customPrompt } = node.data;

    console.log('[NodeExecutionManager] Agent mediator executing with inputs:', inputs);

    // Check if we have multiple agents to synthesize
    if (inputs.multipleAgents && Array.isArray(inputs.multipleAgents)) {
      console.log('[NodeExecutionManager] Synthesizing multiple agents:', inputs.multipleAgents.length);

      const result = await agentLLMService.synthesizeMultipleAgents(
        inputs.multipleAgents,
        customPrompt
      );

      if (result.success) {
        return {
          nodeId: node.id,
          success: true,
          data: {
            mediated_analysis: result.mediatedContext,
            source_agents: inputs.multipleAgents.map((a: any) => a.agentName),
            llm_provider: result.llmProvider,
            llm_model: result.llmModel
          }
        };
      } else {
        return {
          nodeId: node.id,
          success: false,
          error: result.error || 'Multi-agent synthesis failed'
        };
      }
    }

    // Single agent mediation
    const result = await agentLLMService.mediateAgentOutput(inputs, {
      llmProvider: selectedProvider, // Will use active if undefined
      mediationPrompt: customPrompt  // Will use default if undefined
    });

    if (result.success) {
      return {
        nodeId: node.id,
        success: true,
        data: {
          mediated_analysis: result.mediatedContext,
          source_agent: inputs.agent || 'unknown',
          llm_provider: result.llmProvider,
          llm_model: result.llmModel
        }
      };
    } else {
      return {
        nodeId: node.id,
        success: false,
        error: result.error || 'Agent mediation failed'
      };
    }
  }

  /**
   * Execute results display node
   */
  private async executeResultsDisplay(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    // Results display just receives and displays data
    console.log('[NodeExecutionManager] ========================================');
    console.log('[NodeExecutionManager] Results Display Node Execution');
    console.log('[NodeExecutionManager] Node ID:', node.id);
    console.log('[NodeExecutionManager] Raw inputs:', inputs);
    console.log('[NodeExecutionManager] Input keys:', Object.keys(inputs));
    console.log('[NodeExecutionManager] Input type:', typeof inputs);
    console.log('[NodeExecutionManager] ========================================');

    // Extract actual data if it's wrapped in a success object
    let displayData = inputs;
    if (inputs.success !== undefined && inputs.data !== undefined) {
      displayData = inputs.data;
      console.log('[NodeExecutionManager] Unwrapped data from success wrapper');
    }

    console.log('[NodeExecutionManager] Final display data:', displayData);

    return {
      nodeId: node.id,
      success: true,
      data: displayData // Pass the unwrapped data
    };
  }

  /**
   * Execute entire workflow
   */
  async executeWorkflow(
    nodes: Node[],
    edges: Edge[],
    onNodeStatusChange: (nodeId: string, status: NodeStatus, result?: any) => void
  ): Promise<Map<string, any>> {
    // Topological sort
    const sortedNodes = this.topologicalSort(nodes, edges);
    const results = new Map<string, any>();

    console.log('[NodeExecutionManager] Executing workflow with nodes:', sortedNodes.map(n => n.id));

    for (const node of sortedNodes) {
      console.log(`[NodeExecutionManager] Executing node: ${node.id} (${node.type})`);

      // Update status to running
      onNodeStatusChange(node.id, 'running');

      // Get inputs from previous nodes
      const inputs = this.getNodeInputs(node, edges, results);
      console.log(`[NodeExecutionManager] Node ${node.id} inputs:`, inputs);

      // Execute node
      const result = await this.executeNode(node, inputs);
      console.log(`[NodeExecutionManager] Node ${node.id} result:`, result);

      // Store result
      results.set(node.id, result.data);

      // Update status
      if (result.success) {
        onNodeStatusChange(node.id, 'completed', result.data);
      } else {
        onNodeStatusChange(node.id, 'error', result.error);
        throw new Error(`Node ${node.id} failed: ${result.error}`);
      }
    }

    console.log('[NodeExecutionManager] Workflow completed successfully');
    return results;
  }
}

export const nodeExecutionManager = new NodeExecutionManager();
