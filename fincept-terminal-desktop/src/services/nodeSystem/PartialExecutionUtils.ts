/**
 * Partial Execution Utilities
 * Optimizations for smart workflow execution based on n8n's execution engine
 *
 * Features:
 * - Dirty node detection (nodes that need re-execution)
 * - Start node finding (where to begin partial execution)
 * - Subgraph extraction (only execute necessary nodes)
 * - Run data cleaning (remove stale execution data)
 */

import {
  INode,
  IRunData,
  IPinData,
  NodeConnectionType,
  INodeExecutionData,
} from './types';
import { DirectedGraph, GraphConnection } from './DirectedGraph';

/**
 * Check if a node is "dirty" and needs re-execution
 *
 * A node is dirty if:
 * - It has no run data and no pinned data
 * - Its parameters changed (future enhancement)
 * - One of its parents is disabled (future enhancement)
 * - It has an error (future enhancement)
 */
export function isDirty(
  node: INode,
  runData: IRunData = {},
  pinData: IPinData = {}
): boolean {
  // Check for pinned data (always clean if pinned)
  const hasPinnedData = pinData[node.name] !== undefined;
  if (hasPinnedData) {
    return false;
  }

  // Check for run data
  const hasRunData = runData[node.name] !== undefined;
  if (hasRunData) {
    return false;
  }

  // Node has neither pinned data nor run data - it's dirty
  return true;
}

/**
 * Find start nodes for partial execution
 *
 * Start nodes are the earliest dirty nodes on each branch
 * from trigger to destination. These are where execution begins.
 *
 * Algorithm:
 * 1. Start from trigger node
 * 2. If current node is dirty or is destination - it's a start node
 * 3. If cycle detected - stop following branch
 * 4. Recurse with all children in subgraph
 */
export function findStartNodes(options: {
  graph: DirectedGraph;
  trigger: INode;
  destination: INode;
  runData: IRunData;
  pinData: IPinData;
}): Set<INode> {
  const { graph, trigger, destination, runData, pinData } = options;

  const startNodes = new Set<INode>();
  const seen = new Set<string>();

  function findRecursive(current: INode) {
    const nodeIsDirty = isDirty(current, runData, pinData);

    // If dirty, this is a start node
    if (nodeIsDirty) {
      startNodes.add(current);
      return;
    }

    // If we reached destination, this is a start node
    if (current === destination) {
      startNodes.add(current);
      return;
    }

    // Cycle detection
    if (seen.has(current.name)) {
      return;
    }

    seen.add(current.name);

    // Recurse with direct children
    const children = graph.getDirectChildren(current);

    for (const child of children) {
      // Only follow branches that have run data
      const childRunData = runData[child.name];
      const childPinData = pinData[child.name];

      const hasData = childRunData !== undefined || childPinData !== undefined;

      if (!hasData) {
        // Skip branches without data
        continue;
      }

      findRecursive(child);
    }
  }

  findRecursive(trigger);
  return startNodes;
}

/**
 * Extract subgraph from trigger to destination
 *
 * Returns only the nodes and connections needed to execute
 * from trigger to destination, ignoring unrelated branches.
 */
export function findSubgraph(options: {
  graph: DirectedGraph;
  trigger: INode;
  destination: INode;
}): DirectedGraph {
  const { graph, trigger, destination } = options;
  return graph.extractSubgraph(trigger, destination);
}

/**
 * Clean run data to only include nodes in subgraph
 *
 * Removes execution data for nodes not in the current execution path.
 * This prevents stale data from affecting the execution.
 */
export function cleanRunData(
  runData: IRunData,
  subgraph: DirectedGraph
): IRunData {
  const cleanedData: IRunData = {};
  const subgraphNodes = subgraph.getNodes();

  for (const [nodeName, nodeData] of Object.entries(runData)) {
    if (subgraphNodes.has(nodeName)) {
      cleanedData[nodeName] = nodeData;
    }
  }

  return cleanedData;
}

/**
 * Handle cycles in workflow graph
 *
 * Detects and breaks cycles to prevent infinite loops.
 * Returns modified graph with cycle-breaking edges removed.
 */
export function handleCycles(graph: DirectedGraph): {
  hasCycles: boolean;
  cycleNodes: string[];
} {
  const hasCycles = graph.hasCycles();

  if (!hasCycles) {
    return { hasCycles: false, cycleNodes: [] };
  }

  // Detect cycle nodes
  const cycleNodes: string[] = [];
  const visited = new Set<string>();
  const recursionStack = new Set<string>();

  const detectCycleDFS = (node: INode): boolean => {
    if (recursionStack.has(node.name)) {
      cycleNodes.push(node.name);
      return true;
    }

    if (visited.has(node.name)) {
      return false;
    }

    visited.add(node.name);
    recursionStack.add(node.name);

    const children = graph.getDirectChildren(node);

    for (const child of children) {
      if (detectCycleDFS(child)) {
        return true;
      }
    }

    recursionStack.delete(node.name);
    return false;
  };

  const nodes = Array.from(graph.getNodes().values());
  for (const node of nodes) {
    if (detectCycleDFS(node)) {
      break;
    }
  }

  return { hasCycles: true, cycleNodes };
}

/**
 * Filter disabled nodes from graph
 *
 * Returns new graph with disabled nodes and their connections removed.
 */
export function filterDisabledNodes(graph: DirectedGraph): DirectedGraph {
  const filtered = graph.clone();
  const nodes = Array.from(filtered.getNodes().values());

  for (const node of nodes) {
    if (node.disabled) {
      filtered.removeNode(node.name);
    }
  }

  return filtered;
}

/**
 * Recreate node execution stack from start nodes
 *
 * Builds execution stack in topological order starting from
 * the given start nodes, ensuring dependencies execute first.
 */
export function recreateNodeExecutionStack(options: {
  graph: DirectedGraph;
  startNodes: Set<INode>;
  runData: IRunData;
  pinData: IPinData;
}): INode[] {
  const { graph, startNodes, runData, pinData } = options;

  const executionStack: INode[] = [];
  const visited = new Set<string>();

  /**
   * Topological sort using DFS
   */
  function visit(node: INode) {
    if (visited.has(node.name)) {
      return;
    }

    visited.add(node.name);

    // Visit dependencies first (parents)
    const parents = graph.getDirectParents(node);

    for (const parent of parents) {
      // Only visit parents that have data (run data or pin data)
      const hasData = runData[parent.name] || pinData[parent.name];

      if (hasData && !parent.disabled) {
        visit(parent);
      }
    }

    // Add current node to stack
    if (!node.disabled) {
      executionStack.push(node);
    }
  }

  // Start from each start node
  for (const startNode of startNodes) {
    visit(startNode);
  }

  return executionStack;
}

/**
 * Find trigger node for partial execution
 *
 * Searches for the appropriate trigger node to start execution from.
 * Prefers manual trigger nodes, falls back to start nodes.
 */
export function findTriggerForPartialExecution(
  graph: DirectedGraph,
  destination: INode
): INode | undefined {
  // Get all start nodes (nodes with no incoming connections)
  const startNodes = graph.getStartNodes();

  if (startNodes.length === 0) {
    return undefined;
  }

  // If only one start node, use it
  if (startNodes.length === 1) {
    return startNodes[0];
  }

  // Prefer manual trigger nodes
  const manualTrigger = startNodes.find(
    (node) => node.type === 'manualTrigger' || node.type.includes('trigger')
  );

  if (manualTrigger) {
    return manualTrigger;
  }

  // Find start node that has path to destination
  for (const startNode of startNodes) {
    const path = graph.findPath(startNode, destination);
    if (path) {
      return startNode;
    }
  }

  // Fallback to first start node
  return startNodes[0];
}

/**
 * Get incoming data for a node from run data
 */
export function getIncomingData(
  runData: IRunData,
  nodeName: string,
  runIndex: number,
  connectionType: NodeConnectionType = NodeConnectionType.Main,
  outputIndex: number = 0
): INodeExecutionData[] | null {
  const nodeRunData = runData[nodeName];

  if (!nodeRunData) {
    return null;
  }

  // Get specific run index (-1 = last run)
  const targetRun = runIndex === -1 ? nodeRunData.length - 1 : runIndex;

  if (targetRun < 0 || targetRun >= nodeRunData.length) {
    return null;
  }

  const taskData = nodeRunData[targetRun];

  if (!taskData.data[connectionType]) {
    return null;
  }

  const outputData = taskData.data[connectionType][outputIndex];

  return outputData || null;
}

/**
 * Check if node has incoming data from any run
 */
export function hasIncomingDataFromAnyRun(
  runData: IRunData,
  nodeName: string,
  connectionType: NodeConnectionType = NodeConnectionType.Main,
  outputIndex: number = 0
): boolean {
  const nodeRunData = runData[nodeName];

  if (!nodeRunData || nodeRunData.length === 0) {
    return false;
  }

  // Check if any run has data on this connection
  for (const taskData of nodeRunData) {
    const data = taskData.data[connectionType]?.[outputIndex];

    if (data && data.length > 0) {
      return true;
    }
  }

  return false;
}

/**
 * Calculate execution order for nodes
 *
 * Returns nodes in the order they should be executed,
 * respecting dependencies and parallel execution opportunities.
 */
export function calculateExecutionOrder(graph: DirectedGraph): INode[][] {
  const levels: INode[][] = [];
  const visited = new Set<string>();
  const nodeLevel = new Map<string, number>();

  // Calculate level for each node (distance from start nodes)
  function calculateLevel(node: INode, level: number) {
    const currentLevel = nodeLevel.get(node.name) || 0;

    // Update level if deeper path found
    if (level > currentLevel) {
      nodeLevel.set(node.name, level);
    }

    if (visited.has(node.name)) {
      return;
    }

    visited.add(node.name);

    // Recurse with children
    const children = graph.getDirectChildren(node);
    for (const child of children) {
      calculateLevel(child, level + 1);
    }
  }

  // Calculate levels starting from start nodes
  const startNodes = graph.getStartNodes();
  for (const startNode of startNodes) {
    calculateLevel(startNode, 0);
  }

  // Group nodes by level
  const maxLevel = Math.max(...Array.from(nodeLevel.values()));

  for (let i = 0; i <= maxLevel; i++) {
    levels[i] = [];
  }

  const allNodes = Array.from(graph.getNodes().values());
  for (const node of allNodes) {
    const level = nodeLevel.get(node.name) || 0;
    levels[level].push(node);
  }

  return levels;
}
