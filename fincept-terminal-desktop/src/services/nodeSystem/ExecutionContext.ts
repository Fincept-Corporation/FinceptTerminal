/**
 * Execution Context
 * Provides runtime context and utilities for node execution
 *
 * This is what nodes access via 'this' during execution
 */

import {
  IExecuteFunctions,
  INode,
  INodeExecutionData,
  IWorkflowDataProxy,
  ITaskDataConnections,
  NodeConnectionType,
  NodeParameterValue,
  NodeParameterValueType,
  IGetNodeParameterOptions,
  IExecuteContextHelpers,
  IDataObject,
  IHttpRequestOptions,
  IPairedItemData,
  AINodeConnectionType,
  NodeExecutionHint,
  IRunData,
  INodeProperties,
} from './types';
import { ExpressionEngine } from './ExpressionEngine';
import { extractParameterValue } from './ParameterProcessor';

/**
 * Base execution context class
 */
export class BaseExecuteContext {
  protected workflow: IWorkflowDataProxy;
  protected node: INode;
  protected inputData: ITaskDataConnections;
  protected runData: IRunData;
  protected runIndex: number;
  protected executionId: string;
  protected workflowId: string;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    runIndex: number = 0,
    executionId: string = '',
    workflowId: string = ''
  ) {
    this.workflow = workflow;
    this.node = node;
    this.inputData = inputData;
    this.runData = runData;
    this.runIndex = runIndex;
    this.executionId = executionId;
    this.workflowId = workflowId;
  }

  getNode(): INode {
    return this.node;
  }

  getWorkflow(): IWorkflowDataProxy {
    return this.workflow;
  }

  continueOnFail(): boolean {
    return this.node.continueOnFail ?? false;
  }

  /**
   * Get node parameter value with support for dot notation and expressions
   */
  getNodeParameter(
    parameterName: string,
    itemIndex: number,
    fallbackValue?: any,
    options?: IGetNodeParameterOptions
  ): NodeParameterValueType | object {
    const value = getNodeParameter(
      this.node,
      this.workflow,
      this.runData,
      this.runIndex,
      parameterName,
      itemIndex,
      fallbackValue,
      options
    );

    // Resolve expressions automatically
    const inputData = this.getInputItems() || [];
    const item = inputData[itemIndex] || { json: {} };

    const context = ExpressionEngine.createContext(
      item,
      inputData,
      itemIndex,
      this.runIndex,
      this.node.parameters,
      this.node.name,
      this.node.type,
      'Workflow' // Workflow name not available in IWorkflowDataProxy
    );

    return ExpressionEngine.resolveValue(value as NodeParameterValue, context);
  }

  /**
   * Evaluate expression with full expression engine support
   */
  evaluateExpression(expression: string, itemIndex: number): NodeParameterValueType {
    const inputData = this.getInputItems() || [];
    const item = inputData[itemIndex] || { json: {} };

    const context = ExpressionEngine.createContext(
      item,
      inputData,
      itemIndex,
      this.runIndex,
      this.node.parameters,
      this.node.name,
      this.node.type,
      'Workflow' // Workflow name not available in IWorkflowDataProxy
    );

    return ExpressionEngine.resolveValue(expression, context);
  }

  /**
   * Get input items from connected nodes
   */
  protected getInputItems(
    inputIndex: number = 0,
    connectionType: NodeConnectionType = NodeConnectionType.Main
  ): INodeExecutionData[] | undefined {
    if (!this.inputData[connectionType]) {
      return undefined;
    }

    if (inputIndex >= this.inputData[connectionType].length) {
      return undefined;
    }

    return this.inputData[connectionType][inputIndex] ?? undefined;
  }
}

/**
 * Main execution context - used by most nodes
 */
export class ExecutionContext extends BaseExecuteContext implements IExecuteFunctions {
  private executionHints: NodeExecutionHint[] = [];
  private abortController: AbortController;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    runIndex: number = 0,
    executionId: string = '',
    workflowId: string = ''
  ) {
    super(workflow, node, inputData, runData, runIndex, executionId, workflowId);
    this.abortController = new AbortController();
  }

  /**
   * Get input data from connected nodes
   */
  getInputData(
    inputIndex: number = 0,
    connectionType: NodeConnectionType = NodeConnectionType.Main
  ): INodeExecutionData[] {
    const items = this.getInputItems(inputIndex, connectionType);
    return items ?? [];
  }

  /**
   * Get execution ID
   */
  getExecutionId(): string {
    return this.executionId;
  }

  /**
   * Get workflow ID
   */
  getWorkflowId(): string {
    return this.workflowId;
  }

  /**
   * Get credentials
   */
  async getCredentials(type: string): Promise<IDataObject> {
    // TODO: Implement credential retrieval
    // This will integrate with the credential manager
    return {};
  }

  /**
   * Get execution cancel signal
   */
  getExecutionCancelSignal(): AbortSignal {
    return this.abortController.signal;
  }

  /**
   * Get input connection data (for AI nodes)
   */
  async getInputConnectionData(
    connectionType: AINodeConnectionType,
    itemIndex: number
  ): Promise<unknown> {
    const items = this.getInputItems(0, connectionType);
    if (!items || itemIndex >= items.length) {
      return undefined;
    }

    return items[itemIndex].json;
  }

  /**
   * Add execution hints
   */
  addExecutionHints(...hints: NodeExecutionHint[]): void {
    this.executionHints.push(...hints);
  }

  /**
   * Get execution hints
   */
  getExecutionHints(): NodeExecutionHint[] {
    return this.executionHints;
  }

  /**
   * Send chunk (for streaming)
   */
  async sendChunk?(type: string, itemIndex: number, content?: IDataObject | string): Promise<void> {
    // TODO: Implement chunk sending for streaming nodes
    console.log('[ExecutionContext] Chunk sent:', { type, itemIndex, content });
  }

  /**
   * Helper functions
   */
  helpers: IExecuteContextHelpers = {
    /**
     * Make HTTP request
     */
    httpRequest: async (options: IHttpRequestOptions): Promise<any> => {
      const url = options.url;
      const method = options.method || 'GET';

      const fetchOptions: RequestInit = {
        method,
        headers: options.headers as HeadersInit,
        signal: this.abortController.signal,
      };

      if (options.body) {
        if (options.json !== false) {
          fetchOptions.headers = {
            ...fetchOptions.headers,
            'Content-Type': 'application/json',
          };
          fetchOptions.body = JSON.stringify(options.body);
        } else {
          fetchOptions.body = options.body;
        }
      }

      // Add authentication
      if (options.auth) {
        if ('bearer' in options.auth) {
          fetchOptions.headers = {
            ...fetchOptions.headers,
            Authorization: `Bearer ${options.auth.bearer}`,
          };
        } else if ('username' in options.auth) {
          const credentials = btoa(`${options.auth.username}:${options.auth.password}`);
          fetchOptions.headers = {
            ...fetchOptions.headers,
            Authorization: `Basic ${credentials}`,
          };
        }
      }

      // Add query parameters
      let fullUrl = url;
      if (options.qs) {
        const params = new URLSearchParams(options.qs as any);
        fullUrl = `${url}?${params.toString()}`;
      }

      try {
        const response = await fetch(fullUrl, fetchOptions);

        if (!response.ok && !options.ignoreHttpStatusErrors) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        if (options.returnFullResponse) {
          const body = options.json !== false ? await response.json() : await response.text();

          // Convert headers to object
          const headers: Record<string, string> = {};
          response.headers.forEach((value, key) => {
            headers[key] = value;
          });

          return {
            statusCode: response.status,
            statusMessage: response.statusText,
            headers,
            body,
          };
        }

        return options.json !== false ? await response.json() : await response.text();
      } catch (error: any) {
        throw new Error(`HTTP Request failed: ${error.message}`);
      }
    },

    /**
     * Convert data to node execution format
     */
    returnJsonArray: (items: IDataObject | IDataObject[]): INodeExecutionData[] => {
      const itemsArray = Array.isArray(items) ? items : [items];
      return itemsArray.map((json) => ({ json }));
    },

    /**
     * Normalize items to ensure consistent format
     */
    normalizeItems: (items: INodeExecutionData[]): INodeExecutionData[] => {
      return items.map((item) => ({
        json: item.json || {},
        binary: item.binary,
        pairedItem: item.pairedItem,
        error: item.error,
        metadata: item.metadata,
      }));
    },

    /**
     * Construct execution metadata with paired item tracking
     */
    constructExecutionMetaData: (
      inputData: INodeExecutionData[],
      options: { itemData: IPairedItemData | IPairedItemData[] }
    ): INodeExecutionData[] => {
      return inputData.map((item, index) => {
        const pairedItem = Array.isArray(options.itemData)
          ? options.itemData[index] || options.itemData[0]
          : options.itemData;

        return {
          ...item,
          pairedItem,
        };
      });
    },
  };
}

/**
 * Load options context - for loading dynamic parameter options
 */
export class LoadOptionsContext extends BaseExecuteContext {
  private currentNodeParameters?: IDataObject;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    currentNodeParameters?: IDataObject
  ) {
    super(workflow, node, inputData, runData);
    this.currentNodeParameters = currentNodeParameters;
  }

  /**
   * Get current node parameter (partial parameters during configuration)
   */
  getCurrentNodeParameter(parameterName: string): NodeParameterValueType | object | undefined {
    if (!this.currentNodeParameters) {
      return undefined;
    }

    return getParameterValue(this.currentNodeParameters, parameterName.split('.'));
  }

  /**
   * Get all current node parameters
   */
  getCurrentNodeParameters(): IDataObject | undefined {
    return this.currentNodeParameters;
  }
}

// ================================
// HELPER FUNCTIONS
// ================================

/**
 * Get node parameter value
 */
export function getNodeParameter(
  node: INode,
  workflow: IWorkflowDataProxy,
  runData: IRunData,
  runIndex: number,
  parameterName: string,
  itemIndex: number,
  fallbackValue?: any,
  options?: IGetNodeParameterOptions
): NodeParameterValueType | object {
  // Parse parameter path (support dot notation)
  const parameterPath = parameterName.split('.');

  // Get from node parameters
  let value = getParameterValue(node.parameters, parameterPath);

  // Apply fallback if undefined
  if (value === undefined) {
    value = fallbackValue;
  }

  // Note: Expression evaluation is handled in BaseExecuteContext.getNodeParameter
  // which calls this function and then applies ExpressionEngine.resolveValue

  // Extract value if configured (for complex objects)
  if (options?.extractValue && value !== undefined && value !== null) {
    // Find the parameter definition to get extractValue config
    const paramDef = findParameterDefinition(node, parameterName);
    if (paramDef?.extractValue) {
      value = extractParameterValue(value, paramDef.extractValue);
    }
  }

  return value;
}

/**
 * Get parameter value from object using path array
 */
function getParameterValue(
  parameters: IDataObject,
  path: string[]
): NodeParameterValueType | object | undefined {
  let current: any = parameters;

  for (const segment of path) {
    if (current === undefined || current === null) {
      return undefined;
    }

    // Handle array indices: values[0]
    const arrayMatch = segment.match(/^(.+)\[(\d+)\]$/);
    if (arrayMatch) {
      const [, key, index] = arrayMatch;
      current = current[key];

      if (!Array.isArray(current)) {
        return undefined;
      }

      current = current[parseInt(index, 10)];
    } else {
      current = current[segment];
    }
  }

  return current;
}

/**
 * Set parameter value in object using path array
 */
export function setParameterValue(
  parameters: IDataObject,
  path: string[],
  value: any
): void {
  let current: any = parameters;

  for (let i = 0; i < path.length - 1; i++) {
    const segment = path[i];

    // Handle array indices
    const arrayMatch = segment.match(/^(.+)\[(\d+)\]$/);
    if (arrayMatch) {
      const [, key, index] = arrayMatch;

      if (!current[key]) {
        current[key] = [];
      }

      const idx = parseInt(index, 10);
      if (!current[key][idx]) {
        current[key][idx] = {};
      }

      current = current[key][idx];
    } else {
      if (!current[segment]) {
        current[segment] = {};
      }
      current = current[segment];
    }
  }

  // Set final value
  const lastSegment = path[path.length - 1];
  const arrayMatch = lastSegment.match(/^(.+)\[(\d+)\]$/);

  if (arrayMatch) {
    const [, key, index] = arrayMatch;
    if (!current[key]) {
      current[key] = [];
    }
    current[key][parseInt(index, 10)] = value;
  } else {
    current[lastSegment] = value;
  }
}


/**
 * Find parameter definition in node type
 */
function findParameterDefinition(node: INode, parameterName: string): INodeProperties | undefined {
  // This would require access to the node type definition
  // For now, return undefined - nodes can use extractValue option explicitly
  // In the future, this could query NodeRegistry.getNodeType(node.type)
  return undefined;
}
