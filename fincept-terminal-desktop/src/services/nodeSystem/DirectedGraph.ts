/**
 * Directed Graph Implementation
 * Based on n8n's DirectedGraph for efficient workflow graph operations
 *
 * This provides:
 * - Graph representation as adjacency list
 * - Efficient node/connection queries
 * - Graph manipulation (add/remove nodes, connections)
 * - Subgraph extraction
 * - Parent/child traversal
 */

import {
  INode,
  IWorkflow,
  IConnection,
  NodeConnectionType,
  IConnections,
} from './types';

export interface GraphConnection {
  from: INode;
  to: INode;
  type: NodeConnectionType;
  outputIndex: number;
  inputIndex: number;
}

// Key format: fromName-outputType-outputIndex-inputIndex-toName
type GraphKey = `${string}-${NodeConnectionType}-${number}-${number}-${string}`;

/**
 * Directed Graph for efficient workflow operations
 *
 * Provides a more flexible graph representation than the nested Workflow format.
 * Allows for:
 * - Incremental graph building
 * - Easy graph manipulation
 * - Efficient queries
 * - Subgraph extraction
 */
export class DirectedGraph {
  private nodes: Map<string, INode> = new Map();
  private connections: Map<GraphKey, GraphConnection> = new Map();

  /**
   * Create graph from workflow
   */
  static fromWorkflow(workflow: IWorkflow): DirectedGraph {
    const graph = new DirectedGraph();

    // Add all nodes
    for (const node of workflow.nodes) {
      graph.addNode(node);
    }

    // Add all connections
    for (const [sourceNodeName, nodeConnections] of Object.entries(workflow.connections)) {
      const sourceNode = workflow.nodes.find((n) => n.name === sourceNodeName);
      if (!sourceNode) continue;

      for (const [connectionType, outputs] of Object.entries(nodeConnections)) {
        for (let outputIndex = 0; outputIndex < outputs.length; outputIndex++) {
          const connections = outputs[outputIndex];

          for (const connection of connections) {
            const targetNode = workflow.nodes.find((n) => n.name === connection.node);
            if (!targetNode) continue;

            graph.addConnection({
              from: sourceNode,
              to: targetNode,
              type: connectionType as NodeConnectionType,
              outputIndex,
              inputIndex: connection.index,
            });
          }
        }
      }
    }

    return graph;
  }

  /**
   * Convert graph back to workflow
   */
  toWorkflow(baseWorkflow: IWorkflow): IWorkflow {
    const connections: IConnections = {};

    // Rebuild connections object
    for (const connection of this.connections.values()) {
      const sourceName = connection.from.name;

      if (!connections[sourceName]) {
        connections[sourceName] = {};
      }

      if (!connections[sourceName][connection.type]) {
        connections[sourceName][connection.type] = [];
      }

      // Ensure output array exists
      while (connections[sourceName][connection.type].length <= connection.outputIndex) {
        connections[sourceName][connection.type].push([]);
      }

      connections[sourceName][connection.type][connection.outputIndex].push({
        node: connection.to.name,
        type: connection.type,
        index: connection.inputIndex,
      });
    }

    return {
      ...baseWorkflow,
      nodes: Array.from(this.nodes.values()),
      connections,
    };
  }

  /**
   * Check if node exists
   */
  hasNode(nodeName: string): boolean {
    return this.nodes.has(nodeName);
  }

  /**
   * Get all nodes
   */
  getNodes(): Map<string, INode> {
    return new Map(this.nodes.entries());
  }

  /**
   * Get node by name
   */
  getNode(nodeName: string): INode | undefined {
    return this.nodes.get(nodeName);
  }

  /**
   * Get nodes by names
   */
  getNodesByNames(names: string[]): Set<INode> {
    const nodes = new Set<INode>();

    for (const name of names) {
      const node = this.nodes.get(name);
      if (node) {
        nodes.add(node);
      }
    }

    return nodes;
  }

  /**
   * Add node to graph
   */
  addNode(node: INode): this {
    this.nodes.set(node.name, node);
    return this;
  }

  /**
   * Add multiple nodes
   */
  addNodes(...nodes: INode[]): this {
    for (const node of nodes) {
      this.addNode(node);
    }
    return this;
  }

  /**
   * Remove node from graph
   */
  removeNode(nodeName: string): this {
    this.nodes.delete(nodeName);

    // Remove all connections to/from this node
    const keysToDelete: GraphKey[] = [];

    for (const [key, connection] of this.connections.entries()) {
      if (connection.from.name === nodeName || connection.to.name === nodeName) {
        keysToDelete.push(key);
      }
    }

    for (const key of keysToDelete) {
      this.connections.delete(key);
    }

    return this;
  }

  /**
   * Get all connections (optionally filtered)
   */
  getConnections(filter: { to?: INode; from?: INode } = {}): GraphConnection[] {
    const filtered: GraphConnection[] = [];

    for (const connection of this.connections.values()) {
      const toMatches = filter.to ? connection.to === filter.to : true;
      const fromMatches = filter.from ? connection.from === filter.from : true;

      if (toMatches && fromMatches) {
        filtered.push(connection);
      }
    }

    return filtered;
  }

  /**
   * Add connection between nodes
   */
  addConnection(connection: GraphConnection): this {
    const key = this.makeConnectionKey(connection);
    this.connections.set(key, connection);
    return this;
  }

  /**
   * Get direct parent nodes (nodes that output to this node)
   */
  getDirectParents(node: INode): INode[] {
    const parents: INode[] = [];
    const seen = new Set<string>();

    for (const connection of this.connections.values()) {
      if (connection.to === node && !seen.has(connection.from.name)) {
        parents.push(connection.from);
        seen.add(connection.from.name);
      }
    }

    return parents;
  }

  /**
   * Get direct parent connections
   */
  getDirectParentConnections(node: INode): GraphConnection[] {
    return this.getConnections({ to: node });
  }

  /**
   * Get direct child nodes (nodes that receive input from this node)
   */
  getDirectChildren(node: INode): INode[] {
    const children: INode[] = [];
    const seen = new Set<string>();

    for (const connection of this.connections.values()) {
      if (connection.from === node && !seen.has(connection.to.name)) {
        children.push(connection.to);
        seen.add(connection.to.name);
      }
    }

    return children;
  }

  /**
   * Get all ancestors (all nodes upstream)
   */
  getAncestors(node: INode): Set<INode> {
    const ancestors = new Set<INode>();
    const visited = new Set<string>();

    const traverse = (current: INode) => {
      if (visited.has(current.name)) return;
      visited.add(current.name);

      const parents = this.getDirectParents(current);

      for (const parent of parents) {
        ancestors.add(parent);
        traverse(parent);
      }
    };

    traverse(node);
    return ancestors;
  }

  /**
   * Get all descendants (all nodes downstream)
   */
  getDescendants(node: INode): Set<INode> {
    const descendants = new Set<INode>();
    const visited = new Set<string>();

    const traverse = (current: INode) => {
      if (visited.has(current.name)) return;
      visited.add(current.name);

      const children = this.getDirectChildren(current);

      for (const child of children) {
        descendants.add(child);
        traverse(child);
      }
    };

    traverse(node);
    return descendants;
  }

  /**
   * Find path between two nodes (BFS)
   */
  findPath(from: INode, to: INode): INode[] | null {
    const queue: Array<{ node: INode; path: INode[] }> = [{ node: from, path: [from] }];
    const visited = new Set<string>();

    while (queue.length > 0) {
      const { node, path } = queue.shift()!;

      if (node === to) {
        return path;
      }

      if (visited.has(node.name)) continue;
      visited.add(node.name);

      const children = this.getDirectChildren(node);

      for (const child of children) {
        if (!visited.has(child.name)) {
          queue.push({
            node: child,
            path: [...path, child],
          });
        }
      }
    }

    return null; // No path found
  }

  /**
   * Check if graph has cycles
   */
  hasCycles(): boolean {
    const visited = new Set<string>();
    const recursionStack = new Set<string>();

    const hasCycleDFS = (node: INode): boolean => {
      if (recursionStack.has(node.name)) {
        return true; // Cycle detected
      }

      if (visited.has(node.name)) {
        return false;
      }

      visited.add(node.name);
      recursionStack.add(node.name);

      const children = this.getDirectChildren(node);

      for (const child of children) {
        if (hasCycleDFS(child)) {
          return true;
        }
      }

      recursionStack.delete(node.name);
      return false;
    };

    for (const node of this.nodes.values()) {
      if (hasCycleDFS(node)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Get nodes with no incoming connections (start nodes)
   */
  getStartNodes(): INode[] {
    const startNodes: INode[] = [];

    for (const node of this.nodes.values()) {
      const hasIncoming = this.getDirectParentConnections(node).length > 0;

      if (!hasIncoming) {
        startNodes.push(node);
      }
    }

    return startNodes;
  }

  /**
   * Get nodes with no outgoing connections (end nodes)
   */
  getEndNodes(): INode[] {
    const endNodes: INode[] = [];

    for (const node of this.nodes.values()) {
      const hasOutgoing = this.getConnections({ from: node }).length > 0;

      if (!hasOutgoing) {
        endNodes.push(node);
      }
    }

    return endNodes;
  }

  /**
   * Extract subgraph from trigger to destination
   */
  extractSubgraph(trigger: INode, destination: INode): DirectedGraph {
    const subgraph = new DirectedGraph();

    // Find all nodes in path from trigger to destination
    const nodesToInclude = new Set<string>();

    // Start from destination and work backwards to trigger
    const queue = [destination];
    const visited = new Set<string>();

    while (queue.length > 0) {
      const node = queue.shift()!;

      if (visited.has(node.name)) continue;
      visited.add(node.name);
      nodesToInclude.add(node.name);

      // If we reached the trigger, continue to find other paths
      if (node === trigger) continue;

      const parents = this.getDirectParents(node);
      queue.push(...parents);
    }

    // Add nodes to subgraph
    for (const nodeName of nodesToInclude) {
      const node = this.nodes.get(nodeName);
      if (node) {
        subgraph.addNode(node);
      }
    }

    // Add connections within subgraph
    for (const connection of this.connections.values()) {
      if (
        nodesToInclude.has(connection.from.name) &&
        nodesToInclude.has(connection.to.name)
      ) {
        subgraph.addConnection(connection);
      }
    }

    return subgraph;
  }

  /**
   * Clone the graph
   *
   * Remaps connection references to point to the cloned node objects
   * so that === comparisons work correctly within the cloned graph.
   */
  clone(): DirectedGraph {
    const cloned = new DirectedGraph();
    const nodeMap = new Map<string, INode>();

    for (const node of this.nodes.values()) {
      const clonedNode = { ...node };
      cloned.addNode(clonedNode);
      nodeMap.set(node.name, clonedNode);
    }

    for (const connection of this.connections.values()) {
      cloned.addConnection({
        ...connection,
        from: nodeMap.get(connection.from.name) || connection.from,
        to: nodeMap.get(connection.to.name) || connection.to,
      });
    }

    return cloned;
  }

  /**
   * Make connection key
   */
  private makeConnectionKey(connection: GraphConnection): GraphKey {
    return `${connection.from.name}-${connection.type}-${connection.outputIndex}-${connection.inputIndex}-${connection.to.name}`;
  }
}
