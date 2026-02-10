/**
 * Execution Context System
 * Provides runtime context and utilities for node execution
 * Based on n8n's multi-context architecture, adapted for Fincept Terminal
 *
 * Context Types:
 * - ExecuteContext: Standard data processing nodes
 * - PollContext: Polling trigger nodes (scheduled checks)
 * - TriggerContext: Event-based trigger nodes (webhooks, market events)
 * - WebhookContext: HTTP webhook handling
 * - LoadOptionsContext: Dynamic parameter loading
 * - TradingContext: Trading execution nodes (orders, positions)
 * - BacktestContext: Backtesting and strategy testing
 * - MarketDataContext: Real-time market data streaming
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

// ================================
// BASE EXECUTION CONTEXT
// ================================

/**
 * Base execution context with common functionality
 * All specialized contexts inherit from this
 */
export abstract class BaseExecuteContext {
  protected workflow: IWorkflowDataProxy;
  protected node: INode;
  protected inputData: ITaskDataConnections;
  protected runData: IRunData;
  protected runIndex: number;
  protected executionId: string;
  protected workflowId: string;
  protected executionHints: NodeExecutionHint[] = [];

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

  // ================================
  // BASIC ACCESSORS
  // ================================

  getNode(): INode {
    return this.node;
  }

  getWorkflow(): IWorkflowDataProxy {
    return this.workflow;
  }

  getExecutionId(): string {
    return this.executionId;
  }

  getWorkflowId(): string {
    return this.workflowId;
  }

  continueOnFail(): boolean {
    return this.node.continueOnFail ?? false;
  }

  // ================================
  // PARAMETER ACCESS
  // ================================

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
      'Workflow'
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
      'Workflow'
    );

    return ExpressionEngine.resolveValue(expression, context);
  }

  // ================================
  // INPUT/OUTPUT DATA
  // ================================

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

  // ================================
  // EXECUTION HINTS
  // ================================

  addExecutionHints(...hints: NodeExecutionHint[]): void {
    this.executionHints.push(...hints);
  }

  getExecutionHints(): NodeExecutionHint[] {
    return this.executionHints;
  }

  // ================================
  // CREDENTIALS (stub for implementation)
  // ================================

  async getCredentials(type: string): Promise<IDataObject> {
    // TODO: Implement credential retrieval
    console.log(`[BaseExecuteContext] Credentials requested for type: ${type}`);
    return {};
  }

  // ================================
  // COMMON HELPERS
  // ================================

  protected createBaseHelpers() {
    return {
      returnJsonArray: (items: IDataObject | IDataObject[]): INodeExecutionData[] => {
        const itemsArray = Array.isArray(items) ? items : [items];
        return itemsArray.map((json) => ({ json }));
      },

      normalizeItems: (items: INodeExecutionData[]): INodeExecutionData[] => {
        return items.map((item) => ({
          json: item.json || {},
          binary: item.binary,
          pairedItem: item.pairedItem,
          error: item.error,
          metadata: item.metadata,
        }));
      },

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

  protected createFinanceHelpers() {
    return {
      formatCurrency: (value: number, currency: string = 'USD'): string => {
        return new Intl.NumberFormat('en-US', {
          style: 'currency',
          currency,
        }).format(value);
      },

      calculatePercentageChange: (oldValue: number, newValue: number): number => {
        if (oldValue === 0) return 0;
        return ((newValue - oldValue) / oldValue) * 100;
      },

      validateSymbol: (symbol: string): boolean => {
        return /^[A-Z][A-Z0-9.]*$/.test(symbol);
      },

      parseTimestamp: (timestamp: number | string): Date => {
        if (typeof timestamp === 'number') {
          const ts = timestamp > 10000000000 ? timestamp : timestamp * 1000;
          return new Date(ts);
        }
        return new Date(timestamp);
      },
    };
  }
}

// ================================
// STANDARD EXECUTE CONTEXT
// ================================

/**
 * Standard execution context for data processing nodes
 */
export class ExecutionContext extends BaseExecuteContext implements IExecuteFunctions {
  private abortController: AbortController;
  readonly helpers: IExecuteContextHelpers;

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
    this.helpers = this.createHelpers();
  }

  private createHelpers(): IExecuteContextHelpers {
    const baseHelpers = this.createBaseHelpers();
    const financeHelpers = this.createFinanceHelpers();

    return {
      ...baseHelpers,
      ...financeHelpers,

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
            fetchOptions.body = options.body as any;
          }
        }

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
    } as IExecuteContextHelpers;
  }

  getInputData(
    inputIndex: number = 0,
    connectionType: NodeConnectionType = NodeConnectionType.Main
  ): INodeExecutionData[] {
    return this.getInputItems(inputIndex, connectionType) ?? [];
  }

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

  getExecutionCancelSignal(): AbortSignal {
    return this.abortController.signal;
  }

  async sendChunk?(type: string, itemIndex: number, content?: IDataObject | string): Promise<void> {
    console.log('[ExecutionContext] Chunk sent:', { type, itemIndex, content });
  }

  // File helpers
  async readFile(filePath: string): Promise<string> {
    const { readFile: tauriReadFile } = await import('@tauri-apps/plugin-fs');
    const data = await tauriReadFile(filePath);
    return new TextDecoder().decode(data);
  }

  async writeFile(filePath: string, content: string): Promise<void> {
    const { writeFile: tauriWriteFile } = await import('@tauri-apps/plugin-fs');
    await tauriWriteFile(filePath, new TextEncoder().encode(content));
  }

  parseCSV(content: string, delimiter: string, hasHeaders: boolean): IDataObject[] {
    const lines = content.split('\n').filter(l => l.trim());
    if (lines.length === 0) return [];
    const headers = hasHeaders
      ? lines[0].split(delimiter).map(h => h.trim())
      : lines[0].split(delimiter).map((_, i) => `col${i}`);
    const dataLines = hasHeaders ? lines.slice(1) : lines;
    return dataLines.map(line => {
      const values = line.split(delimiter);
      const obj: IDataObject = {};
      headers.forEach((h, i) => { obj[h] = values[i]?.trim() ?? ''; });
      return obj;
    });
  }

  generateCSV(data: IDataObject[], delimiter: string, includeHeaders: boolean): string {
    if (data.length === 0) return '';
    const headers = Object.keys(data[0]);
    const lines: string[] = [];
    if (includeHeaders) lines.push(headers.join(delimiter));
    for (const row of data) {
      lines.push(headers.map(h => String(row[h] ?? '')).join(delimiter));
    }
    return lines.join('\n');
  }

  // Crypto helpers
  async hash(value: string, algorithm: string, encoding: string): Promise<string> {
    const algoMap: Record<string, string> = { md5: 'SHA-1', sha256: 'SHA-256', sha512: 'SHA-512' };
    const algo = algoMap[algorithm] || 'SHA-256';
    const data = new TextEncoder().encode(value);
    const hashBuffer = await crypto.subtle.digest(algo, data);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    if (encoding === 'base64') {
      return btoa(String.fromCharCode(...hashArray));
    }
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
  }

  // Date/time helpers
  formatDate(date: Date, format: string): string {
    const pad = (n: number) => n.toString().padStart(2, '0');
    return format
      .replace('YYYY', date.getFullYear().toString())
      .replace('MM', pad(date.getMonth() + 1))
      .replace('DD', pad(date.getDate()))
      .replace('HH', pad(date.getHours()))
      .replace('mm', pad(date.getMinutes()))
      .replace('ss', pad(date.getSeconds()));
  }

  addTime(date: Date, amount: number, unit: string): Date {
    const result = new Date(date);
    switch (unit) {
      case 'seconds': result.setSeconds(result.getSeconds() + amount); break;
      case 'minutes': result.setMinutes(result.getMinutes() + amount); break;
      case 'hours': result.setHours(result.getHours() + amount); break;
      case 'days': result.setDate(result.getDate() + amount); break;
      case 'weeks': result.setDate(result.getDate() + amount * 7); break;
      case 'months': result.setMonth(result.getMonth() + amount); break;
      case 'years': result.setFullYear(result.getFullYear() + amount); break;
    }
    return result;
  }

  // JSON path helpers
  extractPath(obj: any, path: string): any {
    return path.split('.').reduce((acc, key) => acc?.[key], obj);
  }

  setPath(obj: any, path: string, value: any): any {
    const result = JSON.parse(JSON.stringify(obj));
    const keys = path.split('.');
    let current = result;
    for (let i = 0; i < keys.length - 1; i++) {
      if (current[keys[i]] === undefined) current[keys[i]] = {};
      current = current[keys[i]];
    }
    current[keys[keys.length - 1]] = value;
    return result;
  }

  deletePath(obj: any, path: string): any {
    const result = JSON.parse(JSON.stringify(obj));
    const keys = path.split('.');
    let current = result;
    for (let i = 0; i < keys.length - 1; i++) {
      if (current[keys[i]] === undefined) return result;
      current = current[keys[i]];
    }
    delete current[keys[keys.length - 1]];
    return result;
  }

  // XML helpers
  xmlToJson(element: Element): IDataObject {
    const obj: IDataObject = {};
    if (element.attributes.length > 0) {
      const attrs: IDataObject = {};
      for (let i = 0; i < element.attributes.length; i++) {
        attrs[element.attributes[i].name] = element.attributes[i].value;
      }
      obj['@attributes'] = attrs;
    }
    if (element.children.length === 0) {
      obj['#text'] = element.textContent || '';
    } else {
      for (let i = 0; i < element.children.length; i++) {
        const child = element.children[i];
        const childObj = this.xmlToJson(child);
        if (obj[child.tagName] !== undefined) {
          if (!Array.isArray(obj[child.tagName])) {
            obj[child.tagName] = [obj[child.tagName]];
          }
          (obj[child.tagName] as any[]).push(childObj);
        } else {
          obj[child.tagName] = childObj;
        }
      }
    }
    return obj;
  }

  jsonToXml(obj: any, rootName: string): string {
    const toXml = (data: any, name: string): string => {
      if (data === null || data === undefined) return `<${name}/>`;
      if (typeof data !== 'object') return `<${name}>${String(data)}</${name}>`;
      if (Array.isArray(data)) return data.map(item => toXml(item, name)).join('');
      let xml = `<${name}>`;
      for (const [key, value] of Object.entries(data)) {
        xml += toXml(value, key);
      }
      xml += `</${name}>`;
      return xml;
    };
    return `<?xml version="1.0" encoding="UTF-8"?>${toXml(obj, rootName)}`;
  }
}

// ================================
// POLL CONTEXT
// ================================

/**
 * Poll context for polling trigger nodes
 */
export class PollContext extends BaseExecuteContext {
  readonly helpers: any;
  private emitCallback?: (data: INodeExecutionData[][]) => void;
  private emitErrorCallback?: (error: Error) => void;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    runIndex: number = 0,
    executionId: string = '',
    workflowId: string = '',
    emitCallback?: (data: INodeExecutionData[][]) => void,
    emitErrorCallback?: (error: Error) => void
  ) {
    super(workflow, node, inputData, runData, runIndex, executionId, workflowId);
    this.emitCallback = emitCallback;
    this.emitErrorCallback = emitErrorCallback;
    this.helpers = {
      ...this.createBaseHelpers(),
      ...this.createFinanceHelpers(),
      sleep: (ms: number) => new Promise((resolve) => setTimeout(resolve, ms)),
      getCronExpression: (interval: string) => {
        const match = interval.match(/^(\d+)([smhd])$/);
        if (!match) throw new Error(`Invalid interval: ${interval}`);
        const [, value, unit] = match;
        const num = parseInt(value, 10);
        switch (unit) {
          case 's': return '* * * * *';
          case 'm': return `*/${num} * * * *`;
          case 'h': return `0 */${num} * * *`;
          case 'd': return `0 0 */${num} * *`;
          default: throw new Error(`Unsupported unit: ${unit}`);
        }
      },
    };
  }

  async __emit(data: INodeExecutionData[][]): Promise<void> {
    if (this.emitCallback) {
      this.emitCallback(data);
    } else {
      console.log('[PollContext] Data emitted:', data.length, 'items');
    }
  }

  async __emitError(error: Error): Promise<void> {
    if (this.emitErrorCallback) {
      this.emitErrorCallback(error);
    } else {
      console.error('[PollContext] Error emitted:', error);
    }
  }
}

// ================================
// TRIGGER CONTEXT
// ================================

/**
 * Trigger context for event-based trigger nodes
 */
export class TriggerContext extends BaseExecuteContext {
  readonly helpers: any;
  private emitCallback?: (data: INodeExecutionData[][]) => void;
  private emitErrorCallback?: (error: Error) => void;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    runIndex: number = 0,
    executionId: string = '',
    workflowId: string = '',
    emitCallback?: (data: INodeExecutionData[][]) => void,
    emitErrorCallback?: (error: Error) => void
  ) {
    super(workflow, node, inputData, runData, runIndex, executionId, workflowId);
    this.emitCallback = emitCallback;
    this.emitErrorCallback = emitErrorCallback;
    this.helpers = {
      ...this.createBaseHelpers(),
      ...this.createFinanceHelpers(),
      sleep: (ms: number) => new Promise((resolve) => setTimeout(resolve, ms)),
      getCronExpression: (interval: string) => {
        const match = interval.match(/^(\d+)([smhd])$/);
        if (!match) throw new Error(`Invalid interval: ${interval}`);
        const [, value, unit] = match;
        const num = parseInt(value, 10);
        switch (unit) {
          case 's': return '* * * * *';
          case 'm': return `*/${num} * * * *`;
          case 'h': return `0 */${num} * * *`;
          case 'd': return `0 0 */${num} * *`;
          default: throw new Error(`Unsupported unit: ${unit}`);
        }
      },
    };
  }

  async emit(data: INodeExecutionData[][]): Promise<void> {
    if (this.emitCallback) {
      this.emitCallback(data);
    } else {
      console.log('[TriggerContext] Data emitted:', data.length, 'items');
    }
  }

  async emitError(error: Error): Promise<void> {
    if (this.emitErrorCallback) {
      this.emitErrorCallback(error);
    } else {
      console.error('[TriggerContext] Error emitted:', error);
    }
  }
}

// ================================
// WEBHOOK CONTEXT
// ================================

/**
 * Webhook context for HTTP webhook nodes
 */
export class WebhookContext extends BaseExecuteContext {
  readonly helpers: any;
  private requestData: any;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    requestData: any = {},
    runIndex: number = 0,
    executionId: string = '',
    workflowId: string = ''
  ) {
    super(workflow, node, inputData, runData, runIndex, executionId, workflowId);
    this.requestData = requestData;
    this.helpers = this.createBaseHelpers();
  }

  getBodyData(): IDataObject {
    return this.requestData.body || {};
  }

  getHeaderData(): IDataObject {
    return this.requestData.headers || {};
  }

  getParamsData(): IDataObject {
    return this.requestData.params || {};
  }

  getQueryData(): IDataObject {
    return this.requestData.query || {};
  }

  getRequestObject(): any {
    return this.requestData;
  }

  getResponseObject(): any {
    return {};
  }
}

// ================================
// LOAD OPTIONS CONTEXT
// ================================

/**
 * Load options context for dynamic parameter options
 */
export class LoadOptionsContext extends BaseExecuteContext {
  private currentNodeParameters?: IDataObject;
  readonly helpers: any;

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    currentNodeParameters?: IDataObject
  ) {
    super(workflow, node, inputData, runData);
    this.currentNodeParameters = currentNodeParameters;
    this.helpers = {
      ...this.createBaseHelpers(),
      ...this.createFinanceHelpers(),
    };
  }

  getCurrentNodeParameter(parameterName: string): NodeParameterValueType | object | undefined {
    if (!this.currentNodeParameters) {
      return undefined;
    }
    return getParameterValue(this.currentNodeParameters, parameterName.split('.'));
  }

  getCurrentNodeParameters(): IDataObject | undefined {
    return this.currentNodeParameters;
  }
}

// ================================
// FINANCE-SPECIFIC CONTEXTS
// ================================

/**
 * Trading context for trading execution nodes
 */
export class TradingContext extends ExecutionContext {
  async checkRisk(order: {
    symbol: string;
    quantity: number;
    price: number;
  }): Promise<{ allowed: boolean; reason?: string }> {
    // TODO: Implement actual risk checks
    console.log('[TradingContext] Risk check:', order);
    return { allowed: true };
  }

  async getCurrentPositions(): Promise<IDataObject[]> {
    // TODO: Implement position retrieval
    return [];
  }
}

/**
 * Backtest context for backtesting nodes
 */
export class BacktestContext extends ExecutionContext {
  private backtestState: {
    currentDate: Date;
    positions: Map<string, any>;
    cash: number;
    trades: any[];
  };

  constructor(
    workflow: IWorkflowDataProxy,
    node: INode,
    inputData: ITaskDataConnections,
    runData: IRunData,
    initialCash: number = 100000,
    runIndex: number = 0,
    executionId: string = '',
    workflowId: string = ''
  ) {
    super(workflow, node, inputData, runData, runIndex, executionId, workflowId);
    this.backtestState = {
      currentDate: new Date(),
      positions: new Map(),
      cash: initialCash,
      trades: [],
    };
  }

  async executeTrade(trade: {
    symbol: string;
    action: 'BUY' | 'SELL';
    quantity: number;
    price: number;
    timestamp: Date;
  }): Promise<void> {
    this.backtestState.trades.push(trade);
    const value = trade.quantity * trade.price;

    if (trade.action === 'BUY') {
      this.backtestState.cash -= value;
    } else {
      this.backtestState.cash += value;
    }

    console.log('[BacktestContext] Trade executed:', trade);
  }

  getPerformanceMetrics() {
    return {
      totalReturn: 0,
      sharpeRatio: 0,
      maxDrawdown: 0,
      winRate: 0,
      totalTrades: this.backtestState.trades.length,
    };
  }

  getPortfolioValue(currentPrices: Map<string, number>): number {
    let portfolioValue = this.backtestState.cash;
    for (const [symbol, position] of this.backtestState.positions) {
      const currentPrice = currentPrices.get(symbol) || position.avgPrice;
      portfolioValue += position.quantity * currentPrice;
    }
    return portfolioValue;
  }
}

/**
 * Market Data context for real-time market data streaming
 */
export class MarketDataContext extends ExecutionContext {
  private subscriptions: Map<string, { callback: (data: any) => void }> = new Map();
  private dataBuffer: Map<string, any[]> = new Map();

  async subscribe(symbol: string, callback: (data: any) => void): Promise<void> {
    this.subscriptions.set(symbol, { callback });
    console.log('[MarketDataContext] Subscribed to:', symbol);
  }

  async unsubscribe(symbol: string): Promise<void> {
    this.subscriptions.delete(symbol);
    console.log('[MarketDataContext] Unsubscribed from:', symbol);
  }

  getBufferedData(symbol: string, limit?: number): any[] {
    const buffer = this.dataBuffer.get(symbol) || [];
    return limit ? buffer.slice(-limit) : buffer;
  }

  handleMarketData(symbol: string, data: any): void {
    if (!this.dataBuffer.has(symbol)) {
      this.dataBuffer.set(symbol, []);
    }

    const buffer = this.dataBuffer.get(symbol)!;
    buffer.push(data);

    if (buffer.length > 1000) {
      buffer.shift();
    }

    const subscription = this.subscriptions.get(symbol);
    if (subscription) {
      subscription.callback(data);
    }
  }
}

// ================================
// HELPER FUNCTIONS
// ================================

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
  const parameterPath = parameterName.split('.');
  let value = getParameterValue(node.parameters, parameterPath);

  if (value === undefined) {
    value = fallbackValue;
  }

  if (options?.extractValue && value !== undefined && value !== null) {
    const paramDef = findParameterDefinition(node, parameterName);
    if (paramDef?.extractValue) {
      value = extractParameterValue(value, paramDef.extractValue);
    }
  }

  return value;
}

function getParameterValue(
  parameters: IDataObject,
  path: string[]
): NodeParameterValueType | object | undefined {
  let current: any = parameters;

  for (const segment of path) {
    if (current === undefined || current === null) {
      return undefined;
    }

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

export function setParameterValue(
  parameters: IDataObject,
  path: string[],
  value: any
): void {
  let current: any = parameters;

  for (let i = 0; i < path.length - 1; i++) {
    const segment = path[i];
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

function findParameterDefinition(node: INode, parameterName: string): INodeProperties | undefined {
  return undefined;
}
