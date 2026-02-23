/**
 * Workflow Data Proxy
 * Provides access to workflow data during node execution
 *
 * This class acts as a context provider for nodes to access:
 * - Other nodes in the workflow
 * - Execution data from previous nodes
 * - Workflow-level static data
 */

import {
  IWorkflowDataProxy,
  INode,
  INodeExecutionData,
  IDataObject,
  IWorkflow,
  IRunData,
} from './types';

export class WorkflowDataProxy implements IWorkflowDataProxy {
  private workflow: IWorkflow;
  private runData: IRunData;
  private staticData: IDataObject;

  constructor(workflow: IWorkflow, runData: IRunData = {}, staticData: IDataObject = {}) {
    this.workflow = workflow;
    this.runData = runData;
    this.staticData = staticData;
  }

  /**
   * Get a node by name
   */
  getNode(nodeName: string): INode | null {
    const node = this.workflow.nodes.find((n) => n.name === nodeName);
    return node || null;
  }

  /**
   * Get execution data from a specific node
   */
  getNodeData(nodeName: string, runIndex: number = 0): INodeExecutionData[] | null {
    const nodeData = this.runData[nodeName];

    if (!nodeData || !nodeData[runIndex]) {
      return null;
    }

    // Return main output data
    return nodeData[runIndex].data.main?.[0] || null;
  }

  /**
   * Get workflow static data
   * Static data persists across workflow executions
   */
  getWorkflowStaticData(type: string): IDataObject {
    if (!this.staticData[type]) {
      this.staticData[type] = {};
    }

    return this.staticData[type] as IDataObject;
  }

  /**
   * Get all nodes in workflow
   */
  getAllNodes(): INode[] {
    return this.workflow.nodes;
  }

  /**
   * Get workflow metadata
   */
  getWorkflowMetadata(): {
    id?: string;
    name: string;
    active: boolean;
  } {
    return {
      id: this.workflow.id,
      name: this.workflow.name,
      active: this.workflow.active,
    };
  }

  /**
   * Get nodes connected to a specific node's input (parent/upstream nodes)
   *
   * connections[sourceName] maps source -> targets, so to find parents of nodeName
   * we must search all connection entries for those that point TO nodeName.
   */
  getParentNodes(nodeName: string, inputIndex: number = 0): Array<{
    node: INode;
    outputIndex: number;
  }> {
    const parents: Array<{ node: INode; outputIndex: number }> = [];

    // Search all connections to find sources that connect TO nodeName
    for (const [sourceNodeName, nodeConnections] of Object.entries(this.workflow.connections)) {
      for (const connections of Object.values(nodeConnections)) {
        for (let outputIndex = 0; outputIndex < connections.length; outputIndex++) {
          const outputConnections = connections[outputIndex];

          for (const connection of outputConnections) {
            if (connection.node === nodeName && connection.index === inputIndex) {
              const parentNode = this.getNode(sourceNodeName);
              if (parentNode) {
                parents.push({
                  node: parentNode,
                  outputIndex,
                });
              }
            }
          }
        }
      }
    }

    return parents;
  }

  /**
   * Get nodes connected to a specific node's output (child/downstream nodes)
   *
   * connections[sourceName] maps source -> targets, so connections[nodeName]
   * directly gives us the children.
   */
  getChildNodes(nodeName: string, outputIndex: number = 0): Array<{
    node: INode;
    inputIndex: number;
  }> {
    const children: Array<{ node: INode; inputIndex: number }> = [];

    // connections[nodeName] gives outgoing connections FROM nodeName
    const nodeConnections = this.workflow.connections[nodeName];
    if (!nodeConnections) {
      return children;
    }

    // Look through all connection types at the specified output index
    for (const [connectionType, connections] of Object.entries(nodeConnections)) {
      if (!connections[outputIndex]) continue;

      for (const connection of connections[outputIndex]) {
        const childNode = this.getNode(connection.node);
        if (childNode) {
          children.push({
            node: childNode,
            inputIndex: connection.index,
          });
        }
      }
    }

    return children;
  }

  /**
   * Check if a node has been executed
   */
  hasNodeExecuted(nodeName: string): boolean {
    return this.runData[nodeName] !== undefined && this.runData[nodeName].length > 0;
  }

  /**
   * Get execution status of a node
   */
  getNodeExecutionStatus(nodeName: string, runIndex: number = 0): string | undefined {
    const nodeData = this.runData[nodeName];

    if (!nodeData || !nodeData[runIndex]) {
      return undefined;
    }

    return nodeData[runIndex].executionStatus;
  }

  /**
   * Update run data (used by executor)
   */
  updateRunData(runData: IRunData): void {
    this.runData = runData;
  }

  /**
   * Update static data (used by executor)
   */
  updateStaticData(staticData: IDataObject): void {
    this.staticData = staticData;
  }
}
