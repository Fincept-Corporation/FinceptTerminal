// Node Execution Manager - Handles workflow execution with topological sorting
import { Node, Edge } from 'reactflow';
import { pythonAgentService } from '../chat/pythonAgentService';
import { mcpToolService } from '../mcp/mcpToolService';
import { agentLLMService } from '../chat/agentLLMService';
import { nodeLogger } from './loggerService';

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
          nodeLogger.warn(`Unknown node type: ${node.type}, skipping execution`);
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
   * Execute data source node - fetches data using appropriate adapter
   */
  private async executeDataSource(node: Node): Promise<ExecutionResult> {
    const { sourceType, connectionConfig, query } = node.data;
    const startTime = performance.now();

    try {
      // Dynamic import of adapter factory to avoid circular deps
      const { createAdapter, hasAdapter } = await import('../../components/tabs/data-sources/adapters');

      if (!sourceType || !hasAdapter(sourceType)) {
        return {
          nodeId: node.id,
          success: false,
          error: `No adapter available for data source type: ${sourceType || 'unknown'}`
        };
      }

      // Create connection object from node config
      const connection = {
        id: node.id,
        name: node.data.label || 'Data Source',
        type: sourceType,
        category: node.data.category || 'database',
        config: connectionConfig || {},
        status: 'connected' as const,
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
      };

      const adapter = createAdapter(connection);

      if (!adapter) {
        return {
          nodeId: node.id,
          success: false,
          error: `Failed to create adapter for type: ${sourceType}`
        };
      }

      // Execute query using the adapter
      const result = await adapter.query(query || {});
      const executionTime = Math.round(performance.now() - startTime);

      nodeLogger.info(`Data source executed: ${sourceType}`, { executionTime });

      return {
        nodeId: node.id,
        success: true,
        data: result,
        execution_time_ms: executionTime
      };
    } catch (error) {
      const executionTime = Math.round(performance.now() - startTime);
      nodeLogger.error('Data source execution failed:', error);

      return {
        nodeId: node.id,
        success: false,
        error: error instanceof Error ? error.message : 'Data source execution failed',
        execution_time_ms: executionTime
      };
    }
  }

  /**
   * Execute transformation node - applies data transformations
   */
  private async executeTransformation(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    const { transformType, transformConfig } = node.data;
    const startTime = performance.now();

    try {
      // Get input data (first input or merged inputs)
      let data = Object.values(inputs)[0];

      if (!data) {
        return {
          nodeId: node.id,
          success: false,
          error: 'No input data for transformation'
        };
      }

      // Apply transformations based on type
      let result: any;

      switch (transformType) {
        case 'filter':
          // Filter items based on condition
          if (Array.isArray(data)) {
            const { field, operator, value } = transformConfig || {};
            result = data.filter((item: any) => {
              const fieldValue = item[field];
              switch (operator) {
                case 'eq': return fieldValue === value;
                case 'ne': return fieldValue !== value;
                case 'gt': return fieldValue > value;
                case 'lt': return fieldValue < value;
                case 'gte': return fieldValue >= value;
                case 'lte': return fieldValue <= value;
                case 'contains': return String(fieldValue).includes(value);
                default: return true;
              }
            });
          } else {
            result = data;
          }
          break;

        case 'map':
          // Transform each item
          if (Array.isArray(data)) {
            const { fields } = transformConfig || {};
            if (fields && Array.isArray(fields)) {
              result = data.map((item: any) => {
                const mapped: any = {};
                fields.forEach((f: string) => {
                  if (item[f] !== undefined) mapped[f] = item[f];
                });
                return mapped;
              });
            } else {
              result = data;
            }
          } else {
            result = data;
          }
          break;

        case 'aggregate':
          // Aggregate data
          if (Array.isArray(data)) {
            const { operation, field } = transformConfig || {};
            const values = data.map((item: any) => Number(item[field]) || 0);
            switch (operation) {
              case 'sum': result = values.reduce((a, b) => a + b, 0); break;
              case 'avg': result = values.length ? values.reduce((a, b) => a + b, 0) / values.length : 0; break;
              case 'min': result = Math.min(...values); break;
              case 'max': result = Math.max(...values); break;
              case 'count': result = values.length; break;
              default: result = data;
            }
          } else {
            result = data;
          }
          break;

        case 'select':
          // Select specific fields
          const { selectFields } = transformConfig || {};
          if (selectFields && Array.isArray(selectFields)) {
            if (Array.isArray(data)) {
              result = data.map((item: any) => {
                const selected: any = {};
                selectFields.forEach((f: string) => {
                  selected[f] = item[f];
                });
                return selected;
              });
            } else if (typeof data === 'object') {
              result = {};
              selectFields.forEach((f: string) => {
                result[f] = data[f];
              });
            } else {
              result = data;
            }
          } else {
            result = data;
          }
          break;

        default:
          // Pass through if no transformation type
          result = data;
      }

      const executionTime = Math.round(performance.now() - startTime);
      nodeLogger.info(`Transformation executed: ${transformType || 'passthrough'}`, { executionTime });

      return {
        nodeId: node.id,
        success: true,
        data: result,
        execution_time_ms: executionTime
      };
    } catch (error) {
      const executionTime = Math.round(performance.now() - startTime);
      nodeLogger.error('Transformation failed:', error);

      return {
        nodeId: node.id,
        success: false,
        error: error instanceof Error ? error.message : 'Transformation failed',
        execution_time_ms: executionTime
      };
    }
  }

  /**
   * Execute agent mediator node
   */
  private async executeAgentMediator(
    node: Node,
    inputs: Record<string, any>
  ): Promise<ExecutionResult> {
    const { selectedProvider, customPrompt } = node.data;

    nodeLogger.info('Agent mediator executing with inputs:', inputs);

    // Check if we have multiple agents to synthesize
    if (inputs.multipleAgents && Array.isArray(inputs.multipleAgents)) {
      nodeLogger.info('Synthesizing multiple agents: ' + inputs.multipleAgents.length);

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
    nodeLogger.debug('========================================');
    nodeLogger.debug('Results Display Node Execution');
    nodeLogger.debug('Node ID:', node.id);
    nodeLogger.debug('Raw inputs:', inputs);
    nodeLogger.debug('Input keys:', Object.keys(inputs));
    nodeLogger.debug('Input type:', typeof inputs);
    nodeLogger.debug('========================================');

    // Extract actual data if it's wrapped in a success object
    let displayData = inputs;
    if (inputs.success !== undefined && inputs.data !== undefined) {
      displayData = inputs.data;
      nodeLogger.debug('Unwrapped data from success wrapper');
    }

    nodeLogger.debug('Final display data:', displayData);

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

    nodeLogger.info('Executing workflow with nodes:', sortedNodes.map(n => n.id));

    for (const node of sortedNodes) {
      nodeLogger.info(`Executing node: ${node.id} (${node.type})`);

      // Update status to running
      onNodeStatusChange(node.id, 'running');

      // Get inputs from previous nodes
      const inputs = this.getNodeInputs(node, edges, results);
      nodeLogger.debug(`Node ${node.id} inputs:`, inputs);

      // Execute node
      const result = await this.executeNode(node, inputs);
      nodeLogger.debug(`Node ${node.id} result:`, result);

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

    nodeLogger.info('Workflow completed successfully');
    return results;
  }
}

export const nodeExecutionManager = new NodeExecutionManager();
