/**
 * Workflow Executor - OPTIMIZED VERSION
 * Executes workflows efficiently using DirectedGraph and partial execution
 *
 * Performance Features (from n8n):
 * - DirectedGraph for efficient workflow analysis
 * - Partial execution (only run dirty nodes)
 * - Parallel execution opportunities
 * - Cycle detection and handling
 * - Smart start node finding
 * - Subgraph extraction
 * - Run data cleaning
 *
 * Based on n8n's execution-engine architecture
 */

import {
  IWorkflow,
  INode,
  IRunExecutionData,
  IRunData,
  ITaskData,
  IExecuteData,
  ITaskDataConnections,
  NodeConnectionType,
  INodeExecutionData,
  NodeError,
  IPinData,
  IExecutionState,
} from './types';
import { NodeRegistry } from './NodeRegistry';
import { ExecutionContext } from './ExecutionContext';
import { WorkflowDataProxy } from './WorkflowDataProxy';
import { DirectedGraph } from './DirectedGraph';
import {
  isDirty,
  findStartNodes,
  findSubgraph,
  cleanRunData,
  handleCycles,
  filterDisabledNodes,
  recreateNodeExecutionStack,
  findTriggerForPartialExecution,
  calculateExecutionOrder,
} from './PartialExecutionUtils';
import {
  globalHooks,
  HookEventType,
  type IWorkflowExecuteStartEvent,
  type IWorkflowExecuteEndEvent,
  type IWorkflowExecuteErrorEvent,
  type INodeExecuteStartEvent,
  type INodeExecuteEndEvent,
  type INodeExecuteErrorEvent,
} from './ExecutionHooks';
import { globalWorkflowCache, hashData } from './WorkflowCache';

export class WorkflowExecutor {
  private workflow: IWorkflow;
  private executionId: string;
  private graph: DirectedGraph;
  private enablePartialExecution: boolean = true;
  private enableCaching: boolean = true;

  constructor(workflow: IWorkflow, executionId?: string) {
    this.workflow = workflow;
    this.executionId = executionId || this.generateExecutionId();

    // Try to get cached DirectedGraph
    const workflowId = this.workflow.id || 'default';
    const cachedGraph = this.enableCaching ? globalWorkflowCache.getCachedGraph(workflowId) : undefined;

    if (cachedGraph) {
      this.graph = cachedGraph;
      console.log('[WorkflowExecutor] Using cached DirectedGraph');
    } else {
      // Create DirectedGraph from workflow for efficient operations
      this.graph = DirectedGraph.fromWorkflow(workflow);

      // Cache the graph
      if (this.enableCaching && workflowId) {
        globalWorkflowCache.cacheGraph(workflowId, this.graph);
      }

      console.log('[WorkflowExecutor] Initialized with DirectedGraph');
    }
  }

  /**
   * Execute the workflow with optimizations
   *
   * Supports:
   * - Full execution (all nodes)
   * - Partial execution (only dirty nodes)
   * - Pinned data support
   * - Cycle detection
   */
  async execute(
    startNodeName?: string,
    pinData?: IPinData,
    existingRunData?: IRunData
  ): Promise<IRunExecutionData> {
    const startTime = Date.now();
    console.log(`[WorkflowExecutor] Starting execution: ${this.executionId}`);

    // Emit workflow start hook
    await globalHooks.emit({
      type: HookEventType.WorkflowExecuteStart,
      timestamp: new Date(),
      executionId: this.executionId,
      workflowId: this.workflow.id,
      workflow: this.workflow,
      startNode: startNodeName,
    } as IWorkflowExecuteStartEvent);

    // Detect cycles
    const cycleCheck = handleCycles(this.graph);
    if (cycleCheck.hasCycles) {
      console.warn('[WorkflowExecutor] Detected cycles in workflow:', cycleCheck.cycleNodes);
    }

    // Merge existing run data with new execution
    const baseRunData = existingRunData || {};
    const mergedPinData = { ...this.workflow.pinData, ...pinData };

    // Initialize execution state
    const executionState: IExecutionState = {
      runData: { ...baseRunData },
      pinData: mergedPinData,
      nodeExecutionStack: [],
      waitingExecution: {},
    };

    // Create workflow data proxy
    const workflowDataProxy = new WorkflowDataProxy(
      this.workflow,
      executionState.runData,
      this.workflow.staticData || {}
    );

    // Determine execution strategy
    let nodesToExecute: INode[];

    if (startNodeName) {
      // Execute from specific start node
      const startNode = this.workflow.nodes.find((n) => n.name === startNodeName);
      if (!startNode) {
        throw new Error(`Start node not found: ${startNodeName}`);
      }

      nodesToExecute = [startNode];
      console.log('[WorkflowExecutor] Executing from specific start node:', startNodeName);
    } else if (this.enablePartialExecution && Object.keys(baseRunData).length > 0) {
      // Partial execution - only run dirty nodes
      nodesToExecute = this.findNodesToExecutePartially(executionState);
      console.log('[WorkflowExecutor] Partial execution:', nodesToExecute.map((n) => n.name));
    } else {
      // Full execution - run all nodes
      nodesToExecute = this.findNodesToExecuteFully();
      console.log('[WorkflowExecutor] Full execution:', nodesToExecute.map((n) => n.name));
    }

    // Filter out disabled nodes
    const filteredGraph = filterDisabledNodes(this.graph);

    // Build execution stack
    for (const node of nodesToExecute) {
      executionState.nodeExecutionStack.push({
        node,
        data: { main: [[{ json: {} }]] },
        source: null,
      });
    }

    // Execute nodes in order
    const executedNodes = new Set<string>();

    while (executionState.nodeExecutionStack.length > 0) {
      const executeData = executionState.nodeExecutionStack.shift()!;

      try {
        // Skip if already executed
        if (executedNodes.has(executeData.node.name)) {
          continue;
        }

        // Check if node is disabled
        if (executeData.node.disabled) {
          console.log(`[WorkflowExecutor] Skipping disabled node: ${executeData.node.name}`);
          continue;
        }

        // Check if pinned
        if (executionState.pinData[executeData.node.name]) {
          console.log(`[WorkflowExecutor] Using pinned data for: ${executeData.node.name}`);

          executionState.runData[executeData.node.name] = [
            {
              data: { main: [executionState.pinData[executeData.node.name]] },
              executionTime: 0,
              executionStatus: 'success',
              startTime: Date.now(),
              source: [],
            },
          ];

          executedNodes.add(executeData.node.name);

          // Add downstream nodes
          await this.addDownstreamNodesOptimized(
            executeData.node,
            executionState,
            workflowDataProxy,
            filteredGraph
          );
          continue;
        }

        // Check if all parent nodes have executed
        const parents = filteredGraph.getDirectParents(executeData.node);
        const allParentsExecuted = parents.every(
          (p) => executedNodes.has(p.name) || executionState.pinData[p.name]
        );

        if (!allParentsExecuted) {
          // Re-add to end of stack and continue
          executionState.nodeExecutionStack.push(executeData);
          continue;
        }

        // Execute node
        console.log(`[WorkflowExecutor] Executing node: ${executeData.node.name}`);

        // Emit node start hook
        await globalHooks.emit({
          type: HookEventType.NodeExecuteStart,
          timestamp: new Date(),
          executionId: this.executionId,
          workflowId: this.workflow.id,
          nodeName: executeData.node.name,
          node: executeData.node,
          inputData: executeData.data.main[0] || [],
        } as INodeExecuteStartEvent);

        const nodeStartTime = Date.now();

        // Try to get cached node output
        const workflowId = this.workflow.id || 'default';
        const inputHash = hashData(executeData.data);
        let taskData: ITaskData;

        const cachedOutput = this.enableCaching
          ? globalWorkflowCache.getCachedNodeOutput(workflowId, executeData.node.name, inputHash)
          : undefined;

        if (cachedOutput) {
          console.log(`[WorkflowExecutor] Using cached output for: ${executeData.node.name}`);
          taskData = {
            data: { main: cachedOutput },
            executionTime: 0,
            executionStatus: 'success',
            startTime: nodeStartTime,
            source: [],
          };
        } else {
          // Execute node
          taskData = await this.executeNode(
            executeData.node,
            executeData.data,
            workflowDataProxy,
            executionState.runData
          );

          // Cache the output
          if (this.enableCaching && taskData.executionStatus === 'success' && taskData.data.main) {
            globalWorkflowCache.cacheNodeOutput(
              workflowId,
              executeData.node.name,
              inputHash,
              taskData.data.main as INodeExecutionData[][]
            );
          }
        }

        // Store result
        if (!executionState.runData[executeData.node.name]) {
          executionState.runData[executeData.node.name] = [];
        }
        executionState.runData[executeData.node.name].push(taskData);

        executedNodes.add(executeData.node.name);

        // Emit node end hook
        const nodeDuration = Date.now() - nodeStartTime;
        await globalHooks.emit({
          type: HookEventType.NodeExecuteEnd,
          timestamp: new Date(),
          executionId: this.executionId,
          workflowId: this.workflow.id,
          nodeName: executeData.node.name,
          node: executeData.node,
          outputData: taskData.data.main || [],
          duration: nodeDuration,
        } as INodeExecuteEndEvent);

        // Update proxy with new run data
        workflowDataProxy.updateRunData(executionState.runData);

        // Add downstream nodes
        await this.addDownstreamNodesOptimized(
          executeData.node,
          executionState,
          workflowDataProxy,
          filteredGraph
        );
      } catch (error: any) {
        console.error(`[WorkflowExecutor] Error executing ${executeData.node.name}:`, error);

        // Emit node error hook
        await globalHooks.emit({
          type: HookEventType.NodeExecuteError,
          timestamp: new Date(),
          executionId: this.executionId,
          workflowId: this.workflow.id,
          nodeName: executeData.node.name,
          node: executeData.node,
          error: error as NodeError,
          continueOnFail: executeData.node.continueOnFail || false,
        } as INodeExecuteErrorEvent);

        // Store error
        if (!executionState.runData[executeData.node.name]) {
          executionState.runData[executeData.node.name] = [];
        }

        executionState.runData[executeData.node.name].push({
          data: { main: [] },
          executionTime: 0,
          executionStatus: 'error',
          startTime: Date.now(),
          error: error as NodeError,
          source: [],
        });

        executedNodes.add(executeData.node.name);

        // Check if should continue on fail
        if (!executeData.node.continueOnFail) {
          console.log('[WorkflowExecutor] Stopping execution due to error');
          break;
        }

        console.log('[WorkflowExecutor] Continuing execution despite error');
      }
    }

    console.log('[WorkflowExecutor] Execution complete');

    const executionResult = {
      resultData: {
        runData: executionState.runData,
        pinData: executionState.pinData,
      },
      executionData: {
        contextData: {},
        nodeExecutionStack: [],
        waitingExecution: {},
        metadata: {},
      },
    };

    // Emit workflow end hook
    const duration = Date.now() - startTime;
    await globalHooks.emit({
      type: HookEventType.WorkflowExecuteEnd,
      timestamp: new Date(),
      executionId: this.executionId,
      workflowId: this.workflow.id,
      workflow: this.workflow,
      executionData: executionResult,
      success: true,
      duration,
    } as IWorkflowExecuteEndEvent);

    return executionResult;
  }

  /**
   * Find nodes to execute for partial execution
   *
   * Uses DirectedGraph to find dirty nodes and rebuild execution path
   */
  private findNodesToExecutePartially(executionState: IExecutionState): INode[] {
    // Find all dirty nodes
    const dirtyNodes: INode[] = [];
    const allNodes = Array.from(this.graph.getNodes().values());

    for (const node of allNodes) {
      if (isDirty(node, executionState.runData, executionState.pinData)) {
        dirtyNodes.push(node);
      }
    }

    if (dirtyNodes.length === 0) {
      // No dirty nodes, nothing to execute
      return [];
    }

    // For each dirty node, find path from trigger
    const nodesToExecute = new Set<INode>();

    for (const dirtyNode of dirtyNodes) {
      // Find trigger for this node
      const trigger = findTriggerForPartialExecution(this.graph, dirtyNode);

      if (!trigger) {
        // No trigger found, use the dirty node itself
        nodesToExecute.add(dirtyNode);
        continue;
      }

      // Find start nodes from trigger to dirty node
      const startNodes = findStartNodes({
        graph: this.graph,
        trigger,
        destination: dirtyNode,
        runData: executionState.runData,
        pinData: executionState.pinData,
      });

      // Add all nodes in execution path
      for (const startNode of startNodes) {
        const subgraph = findSubgraph({
          graph: this.graph,
          trigger: startNode,
          destination: dirtyNode,
        });

        const subgraphNodes = Array.from(subgraph.getNodes().values());
        subgraphNodes.forEach((n) => nodesToExecute.add(n));
      }
    }

    return Array.from(nodesToExecute);
  }

  /**
   * Find nodes to execute for full execution
   *
   * Returns all nodes in topological order
   */
  private findNodesToExecuteFully(): INode[] {
    // Get execution order (nodes grouped by level)
    const levels = calculateExecutionOrder(this.graph);

    // Flatten levels into single array
    const allNodes: INode[] = [];
    for (const level of levels) {
      allNodes.push(...level);
    }

    return allNodes;
  }

  /**
   * Execute a single node
   */
  private async executeNode(
    node: INode,
    inputData: ITaskDataConnections,
    workflowDataProxy: WorkflowDataProxy,
    runData: IRunData
  ): Promise<ITaskData> {
    const startTime = Date.now();

    try {
      // Get node type
      const nodeType = NodeRegistry.getNodeType(node.type, node.typeVersion);

      // Create execution context
      const context = new ExecutionContext(
        workflowDataProxy,
        node,
        inputData,
        runData,
        0, // runIndex
        this.executionId,
        this.workflow.id || ''
      );

      // Execute node
      if (!nodeType.execute) {
        throw new Error(`Node type ${node.type} does not have an execute function`);
      }

      const result = await nodeType.execute.call(context);

      const executionTime = Date.now() - startTime;

      return {
        data: { main: result },
        executionTime,
        executionStatus: 'success',
        startTime,
        source: [],
        hints: context.getExecutionHints(),
      };
    } catch (error: any) {
      const executionTime = Date.now() - startTime;

      throw {
        ...error,
        node,
        executionTime,
      };
    }
  }

  /**
   * Add downstream nodes to execution stack (OPTIMIZED)
   *
   * Uses DirectedGraph for efficient child node lookups
   */
  private async addDownstreamNodesOptimized(
    node: INode,
    executionState: IExecutionState,
    workflowDataProxy: WorkflowDataProxy,
    graph: DirectedGraph
  ): Promise<void> {
    // Get direct children from graph (more efficient than proxy)
    const children = graph.getDirectChildren(node);

    for (const childNode of children) {
      // Check if already in stack or executed
      const alreadyInStack = executionState.nodeExecutionStack.some(
        (item) => item.node.name === childNode.name
      );

      if (alreadyInStack) {
        continue;
      }

      // Skip if disabled
      if (childNode.disabled) {
        continue;
      }

      // Build input data for child node
      const inputData = await this.buildInputDataOptimized(
        childNode,
        executionState,
        graph
      );

      // Add to execution stack
      executionState.nodeExecutionStack.push({
        node: childNode,
        data: inputData,
        source: {
          main: [
            {
              node: node.name,
              data: executionState.runData[node.name][0].data,
            },
          ],
        },
      });
    }
  }

  /**
   * DEPRECATED: Old method - kept for backward compatibility
   * Use addDownstreamNodesOptimized instead
   */
  private async addDownstreamNodes(
    node: INode,
    executionState: IExecutionState,
    workflowDataProxy: WorkflowDataProxy
  ): Promise<void> {
    return this.addDownstreamNodesOptimized(
      node,
      executionState,
      workflowDataProxy,
      this.graph
    );
  }

  /**
   * Build input data for a node from its parent nodes (OPTIMIZED)
   *
   * Uses DirectedGraph for parent node lookups
   */
  private async buildInputDataOptimized(
    node: INode,
    executionState: IExecutionState,
    graph: DirectedGraph
  ): Promise<ITaskDataConnections> {
    const inputData: ITaskDataConnections = {
      main: [],
    };

    // Get parent connections from graph
    const parentConnections = graph.getDirectParentConnections(node);

    // Group connections by input index
    const connectionsByInput = new Map<number, typeof parentConnections>();

    for (const connection of parentConnections) {
      if (!connectionsByInput.has(connection.inputIndex)) {
        connectionsByInput.set(connection.inputIndex, []);
      }
      connectionsByInput.get(connection.inputIndex)!.push(connection);
    }

    // Build input data for each input
    for (const [inputIndex, connections] of connectionsByInput) {
      if (!inputData.main[inputIndex]) {
        inputData.main[inputIndex] = [];
      }

      for (const connection of connections) {
        // Get parent's execution data
        const parentRunData = executionState.runData[connection.from.name];

        if (!parentRunData || parentRunData.length === 0) {
          // Check for pinned data
          const pinnedData = executionState.pinData[connection.from.name];
          if (pinnedData) {
            inputData.main[inputIndex].push(...pinnedData);
          }
          continue;
        }

        // Get output data from parent
        const parentOutputData =
          parentRunData[0].data[connection.type]?.[connection.outputIndex] || [];

        inputData.main[inputIndex].push(...parentOutputData);
      }
    }

    return inputData;
  }

  /**
   * DEPRECATED: Old method - kept for backward compatibility
   * Use buildInputDataOptimized instead
   */
  private async buildInputData(
    node: INode,
    executionState: IExecutionState,
    workflowDataProxy: WorkflowDataProxy
  ): Promise<ITaskDataConnections> {
    return this.buildInputDataOptimized(node, executionState, this.graph);
  }

  /**
   * Find the start node (OPTIMIZED)
   *
   * Uses DirectedGraph.getStartNodes() for O(1) lookup
   */
  private findStartNode(): INode | undefined {
    const startNodes = this.graph.getStartNodes();

    if (startNodes.length === 0) {
      // Fallback to first node if no start nodes found
      return this.workflow.nodes[0];
    }

    // Prefer manual trigger nodes
    const manualTrigger = startNodes.find(
      (node) => node.type === 'manualTrigger' || node.type.includes('trigger')
    );

    if (manualTrigger) {
      return manualTrigger;
    }

    // Return first start node
    return startNodes[0];
  }

  /**
   * Generate execution ID
   */
  private generateExecutionId(): string {
    return `exec_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }
}
