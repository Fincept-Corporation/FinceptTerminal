/**
 * Core Node System Types
 * Based on n8n architecture, adapted for Fincept Terminal
 *
 * This file contains all fundamental types for the node-based workflow system.
 * Finance-specific extensions are added while maintaining n8n's proven architecture.
 */

// ================================
// BASIC DATA TYPES
// ================================

// Base parameter value - primitives, arrays, and objects
// Extended to support Collection/FixedCollection default values (empty arrays)
export type NodeParameterValue =
  | string
  | number
  | boolean
  | undefined
  | null
  | any[]      // For Collection/FixedCollection defaults
  | IDataObject; // For object-type parameters

export type NodeParameterValueType =
  | NodeParameterValue
  | INodeParameters
  | NodeParameterValue[]
  | INodeParameters[]
  | INodeParameters[][];

export interface IDataObject {
  [key: string]: NodeParameterValueType;
}

export interface INodeParameters {
  [key: string]: NodeParameterValueType;
}

// ================================
// CONNECTION TYPES
// ================================

/**
 * Types of connections between nodes
 * Extended with finance-specific connection types
 */
export enum NodeConnectionType {
  Main = 'main',                           // Standard data flow

  // AI Connections (from n8n)
  AiLanguageModel = 'ai_languageModel',
  AiMemory = 'ai_memory',
  AiTool = 'ai_tool',
  AiDocument = 'ai_document',
  AiVectorStore = 'ai_vectorStore',
  AiTextSplitter = 'ai_textSplitter',
  AiEmbedding = 'ai_embedding',
  AiOutputParser = 'ai_outputParser',

  // Finance-specific connections
  MarketData = 'finance_marketData',       // Real-time market data
  PortfolioData = 'finance_portfolio',     // Portfolio holdings
  PriceData = 'finance_priceData',         // Historical prices
  FundamentalData = 'finance_fundamental', // Fundamental data
  TechnicalData = 'finance_technical',     // Technical indicators
  NewsData = 'finance_news',               // News and sentiment
  EconomicData = 'finance_economic',       // Economic indicators
  OptionsData = 'finance_options',         // Options chain data
  BacktestData = 'finance_backtest',       // Backtest results
}

export type MainConnectionType = NodeConnectionType.Main;
export type AINodeConnectionType = Exclude<NodeConnectionType, NodeConnectionType.Main>;
export type FinanceConnectionType =
  | NodeConnectionType.MarketData
  | NodeConnectionType.PortfolioData
  | NodeConnectionType.PriceData
  | NodeConnectionType.FundamentalData
  | NodeConnectionType.TechnicalData
  | NodeConnectionType.NewsData
  | NodeConnectionType.EconomicData
  | NodeConnectionType.OptionsData
  | NodeConnectionType.BacktestData;

// ================================
// NODE EXECUTION DATA
// ================================

/**
 * Paired item data - tracks which output item came from which input item
 * Essential for data lineage and debugging
 */
export interface IPairedItemData {
  item: number;              // Index of the item in the input
  input?: number;            // Which input the data came from (for multiple inputs)
  sourceOverwrite?: ISourceData;
}

/**
 * Source data - tracks where data originated from in the workflow
 */
export interface ISourceData {
  previousNode: string;
  previousNodeOutput?: number;
  previousNodeRun?: number;
}

/**
 * Binary data interface for handling files, images, etc.
 */
export interface IBinaryKeyData {
  [key: string]: IBinaryData;
}

export interface IBinaryData {
  data: string;              // base64 encoded data or file path
  mimeType: string;
  fileName?: string;
  fileExtension?: string;
  fileSize?: string;
  directory?: string;
  id?: string;               // Storage ID for large files
}

/**
 * Core execution data structure - the primary data carrier between nodes
 */
export interface INodeExecutionData {
  json: IDataObject;                                    // Main JSON data
  binary?: IBinaryKeyData;                              // Binary file data
  error?: NodeApiError | NodeOperationError;            // Error if execution failed
  pairedItem?: IPairedItemData | IPairedItemData[] | number;  // Data lineage
  metadata?: {
    subExecution?: {
      executionId: string;
      workflowId: string;
    };
  };
}

/**
 * Task data connections - how data flows between nodes
 * Each connection type can have multiple outputs (for branching)
 */
export interface ITaskDataConnections {
  [connectionType: string]: Array<INodeExecutionData[] | null>;
}

// ================================
// ERROR TYPES
// ================================

export class NodeError extends Error {
  constructor(
    public node: INode,
    message: string
  ) {
    super(message);
    this.name = 'NodeError';
  }
}

export class NodeOperationError extends NodeError {
  constructor(
    node: INode,
    message: string,
    public description?: string,
    public itemIndex?: number
  ) {
    super(node, message);
    this.name = 'NodeOperationError';
  }
}

export class NodeApiError extends NodeError {
  constructor(
    node: INode,
    public httpCode: number,
    message: string,
    public description?: string
  ) {
    super(node, message);
    this.name = 'NodeApiError';
  }
}

// ================================
// NODE PROPERTY TYPES
// ================================

/**
 * All supported parameter types
 * Extended from n8n's 30+ types
 */
export enum NodePropertyType {
  // Basic types
  String = 'string',
  Number = 'number',
  Boolean = 'boolean',

  // Selection types
  Options = 'options',
  MultiOptions = 'multiOptions',

  // Complex types
  Collection = 'collection',
  FixedCollection = 'fixedCollection',
  Json = 'json',

  // UI types
  Notice = 'notice',
  Button = 'button',
  Color = 'color',
  DateTime = 'dateTime',
  Hidden = 'hidden',

  // Advanced types
  Filter = 'filter',
  ResourceLocator = 'resourceLocator',
  ResourceMapper = 'resourceMapper',
  CredentialsSelect = 'credentialsSelect',
  CurlImport = 'curlImport',

  // Finance-specific types
  StockSymbol = 'stockSymbol',           // Stock symbol selector
  PortfolioSelector = 'portfolioSelector', // Portfolio picker
  DateRange = 'dateRange',                // Date range picker
  TimeInterval = 'timeInterval',          // Time interval (1d, 1h, etc.)
  TechnicalIndicator = 'technicalIndicator', // Technical indicator selector
  AssetClass = 'assetClass',              // Asset class picker
}

/**
 * Field type for validation
 */
export enum FieldType {
  String = 'string',
  Number = 'number',
  Boolean = 'boolean',
  DateTime = 'dateTime',
  Time = 'time',
  Object = 'object',
  Array = 'array',
  Url = 'url',
  Email = 'email',
}

/**
 * Options for a parameter
 * Simple options for Options/MultiOptions types
 */
export interface INodePropertyOptions {
  name: string;
  value: string | number | boolean;
  description?: string;
  action?: string;
  routing?: INodePropertyRouting;
}

/**
 * Collection field option - for FixedCollection types
 * Contains nested parameter structures via 'values' array
 */
export interface INodePropertyCollectionValue {
  name: string;
  description?: string;
  displayName?: string;
  values: INodeProperties[];
}

/**
 * Type options for parameters
 */
export interface INodePropertyTypeOptions {
  multipleValues?: boolean;
  multipleValueButtonText?: string;
  sortable?: boolean;
  minValue?: number;
  maxValue?: number;
  numberPrecision?: number;
  password?: boolean;
  rows?: number;
  alwaysOpenEditWindow?: boolean;
  editor?: 'code' | 'json' | 'html';
  loadOptionsMethod?: string;
  loadOptionsDependsOn?: string[];

  // Filter specific
  filter?: {
    caseSensitive?: boolean | string;
    typeValidation?: 'strict' | 'loose';
    version?: number;
  };

  // Resource locator specific
  resourceMapper?: {
    resourceMapperMethod: string;
    mode: 'add' | 'update';
    fieldWords?: {
      singular: string;
      plural: string;
    };
  };
}

/**
 * Display options - control when parameters are shown/hidden
 */
export interface IDisplayOptions {
  show?: {
    [key: string]: NodeParameterValue[];
  };
  hide?: {
    [key: string]: NodeParameterValue[];
  };
}

/**
 * Routing configuration for parameters
 */
export interface INodePropertyRouting {
  request?: {
    method?: string;
    url?: string;
    headers?: IDataObject;
    body?: IDataObject;
  };
  send?: {
    type?: string;
    property?: string;
    value?: string;
  };
}

/**
 * Node property definition
 */
export interface INodeProperties {
  displayName: string;
  name: string;
  type: NodePropertyType;
  typeOptions?: INodePropertyTypeOptions;
  default: NodeParameterValue;
  description?: string;
  hint?: string;
  placeholder?: string;
  required?: boolean;
  displayOptions?: IDisplayOptions;

  // For Options/MultiOptions: simple key-value options
  // For Collection: nested INodeProperties (parameters themselves)
  // For FixedCollection: INodePropertyCollectionValue (with 'values' array)
  options?: INodePropertyOptions[] | INodeProperties[] | INodePropertyCollectionValue[];

  routing?: INodePropertyRouting;
  validateType?: FieldType;
  ignoreValidationDuringExecution?: boolean;
  noDataExpression?: boolean;
  extractValue?: {
    type: string;
    property: string;
  };
}

// ================================
// NODE DESCRIPTION
// ================================

/**
 * Node hints - provide guidance to users
 */
export interface NodeHint {
  message: string;
  type?: 'info' | 'warning' | 'danger';
  location?: 'outputPane' | 'inputPane' | 'ndv';
  displayCondition?: string;
  whenToDisplay?: 'always' | 'beforeExecution' | 'afterExecution';
}

/**
 * Node output definition
 */
export interface INodeOutputConfiguration {
  type: NodeConnectionType | string;
  displayName?: string;
}

/**
 * Credential description for node
 */
export interface INodeCredentialDescription {
  name: string;
  required?: boolean;
  displayOptions?: IDisplayOptions;
  testedBy?: string | ICredentialTestRequest;
}

export interface ICredentialTestRequest {
  request: {
    method: string;
    url: string;
    headers?: IDataObject;
    body?: IDataObject;
  };
  rules?: Array<{
    type: 'responseCode' | 'responseSuccessBody' | 'responseErrorBody';
    properties: IDataObject;
  }>;
}

/**
 * Base node type description
 */
export interface INodeTypeBaseDescription {
  displayName: string;
  name: string;
  icon?: string;
  iconUrl?: string;
  iconColor?: string;
  badgeIconUrl?: string;
  group: string[];
  description: string;
  defaultVersion?: number;
}

/**
 * Full node type description
 */
export interface INodeTypeDescription extends INodeTypeBaseDescription {
  version: number | number[];
  subtitle?: string;
  defaults: {
    name: string;
    color?: string;
  };
  inputs: NodeConnectionType[] | string;
  outputs: NodeConnectionType[] | INodeOutputConfiguration[] | string;
  properties: INodeProperties[];
  credentials?: INodeCredentialDescription[];
  hints?: NodeHint[];
  outputNames?: string[];
  usableAsTool?: boolean;
  maxNodes?: number;
  keepOnlySet?: boolean;
}

// ================================
// NODE INTERFACE
// ================================

/**
 * Core node instance
 */
export interface INode {
  id: string;
  name: string;
  type: string;
  typeVersion: number;
  position: [number, number];
  parameters: INodeParameters;
  credentials?: {
    [key: string]: {
      id: string;
      name: string;
    };
  };
  disabled?: boolean;
  notes?: string;
  notesInFlow?: boolean;
  color?: string;
  continueOnFail?: boolean;
  alwaysOutputData?: boolean;
  executeOnce?: boolean;
  retryOnFail?: boolean;
  maxTries?: number;
  waitBetweenTries?: number;
}

// ================================
// EXECUTION CONTEXT INTERFACES
// ================================

/**
 * Options for getting node parameters
 */
export interface IGetNodeParameterOptions {
  extractValue?: boolean;
  returnObjectType?: 'json' | 'object';
  fallbackValue?: any;
}

/**
 * Workflow data proxy - provides access to workflow data
 */
export interface IWorkflowDataProxy {
  getNode(nodeName: string): INode | null;
  getNodeData(nodeName: string, runIndex?: number): INodeExecutionData[] | null;
  getWorkflowStaticData(type: string): IDataObject;
}

/**
 * Execute context helpers
 */
export interface IExecuteContextHelpers {
  httpRequest(options: IHttpRequestOptions): Promise<any>;
  returnJsonArray(items: IDataObject | IDataObject[]): INodeExecutionData[];
  normalizeItems(items: INodeExecutionData[]): INodeExecutionData[];
  constructExecutionMetaData(
    inputData: INodeExecutionData[],
    options: { itemData: IPairedItemData | IPairedItemData[] }
  ): INodeExecutionData[];
}

/**
 * HTTP request options
 */
export interface IHttpRequestOptions {
  url: string;
  method?: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH' | 'HEAD' | 'OPTIONS';
  headers?: IDataObject;
  body?: any;
  qs?: IDataObject;
  auth?: {
    username: string;
    password: string;
  } | {
    bearer: string;
  };
  json?: boolean;
  timeout?: number;
  returnFullResponse?: boolean;
  ignoreHttpStatusErrors?: boolean;
}

/**
 * Base execution functions - available in all contexts
 */
export interface IExecuteFunctionsBase {
  getNode(): INode;
  getWorkflow(): IWorkflowDataProxy;
  getNodeParameter(
    parameterName: string,
    itemIndex: number,
    fallbackValue?: any,
    options?: IGetNodeParameterOptions
  ): NodeParameterValueType | object;
  continueOnFail(): boolean;
  evaluateExpression(expression: string, itemIndex: number): NodeParameterValueType;
}

/**
 * Main execution functions - for standard nodes
 */
export interface IExecuteFunctions extends IExecuteFunctionsBase {
  getInputData(inputIndex?: number, connectionType?: NodeConnectionType): INodeExecutionData[];

  getExecutionId(): string;
  getWorkflowId(): string;

  helpers: IExecuteContextHelpers;

  // Get credentials
  getCredentials(type: string): Promise<IDataObject>;

  // Execution metadata
  getExecutionCancelSignal(): AbortSignal;

  // AI connection helpers
  getInputConnectionData(
    connectionType: AINodeConnectionType,
    itemIndex: number
  ): Promise<unknown>;

  // Add execution hints
  addExecutionHints(...hints: NodeExecutionHint[]): void;

  // Send chunks (for streaming)
  sendChunk?(type: string, itemIndex: number, content?: IDataObject | string): Promise<void>;

  // File helpers (used by file-related nodes)
  readFile(filePath: string): Promise<string>;
  writeFile(filePath: string, content: string): Promise<void>;
  parseCSV(content: string, delimiter: string, hasHeaders: boolean): IDataObject[];
  generateCSV(data: IDataObject[], delimiter: string, includeHeaders: boolean): string;

  // Crypto helpers (used by CryptoNode)
  hash(value: string, algorithm: string, encoding: string): Promise<string>;

  // Date/time helpers (used by DateTimeNode)
  formatDate(date: Date, format: string): string;
  addTime(date: Date, amount: number, unit: string): Date;

  // JSON path helpers (used by JSONNode)
  extractPath(obj: any, path: string): any;
  setPath(obj: any, path: string, value: any): any;
  deletePath(obj: any, path: string): any;

  // XML helpers (used by XMLNode)
  xmlToJson(element: Element): IDataObject;
  jsonToXml(obj: any, rootName: string): string;
}

/**
 * Load options functions - for dynamic parameter options
 */
export interface ILoadOptionsFunctions extends IExecuteFunctionsBase {
  getCurrentNodeParameter(parameterName: string): NodeParameterValueType | object | undefined;
  getCurrentNodeParameters(): INodeParameters | undefined;
}

/**
 * Webhook functions - for webhook nodes
 */
export interface IWebhookFunctions extends IExecuteFunctionsBase {
  getRequestObject(): any;
  getResponseObject(): any;
  getBodyData(): IDataObject;
  getHeaderData(): IDataObject;
  getParamsData(): IDataObject;
  getQueryData(): IDataObject;
  getWebhookName(): string;
}

// ================================
// NODE TYPE INTERFACE
// ================================

/**
 * Node execution hint
 */
export interface NodeExecutionHint {
  message: string;
  type?: 'info' | 'warning' | 'error';
  location?: 'outputPane' | 'inputPane';
  whenToDisplay?: 'always' | 'beforeExecution' | 'afterExecution';
}

/**
 * Node output - result of node execution
 */
export type NodeOutput = INodeExecutionData[][];

/**
 * Core node type interface
 */
export interface INodeType {
  description: INodeTypeDescription;

  // Main execution function
  execute?(this: IExecuteFunctions): Promise<NodeOutput>;

  // Webhook handler
  webhook?(this: IWebhookFunctions): Promise<any>;

  // Methods for dynamic behavior
  methods?: {
    loadOptions?: {
      [key: string]: (this: ILoadOptionsFunctions) => Promise<INodePropertyOptions[]>;
    };
    credentialTest?: {
      [key: string]: (
        this: IExecuteFunctions,
        credential: IDataObject
      ) => Promise<{ status: 'OK' | 'Error'; message?: string }>;
    };
    listSearch?: {
      [key: string]: (
        this: ILoadOptionsFunctions,
        filter?: string,
        paginationToken?: string
      ) => Promise<INodeListSearchResult>;
    };
  };
}

export interface INodeListSearchResult {
  results: INodePropertyOptions[];
  paginationToken?: string;
}

// ================================
// VERSIONED NODE TYPE
// ================================

/**
 * Interface for versioned node types
 */
export interface IVersionedNodeType {
  nodeVersions: {
    [version: number]: INodeType;
  };
  currentVersion: number;
  getNodeType(version?: number): INodeType;
}

// ================================
// WORKFLOW EXECUTION
// ================================

/**
 * Connection definition
 */
export interface IConnection {
  node: string;
  type: NodeConnectionType;
  index: number;
}

/**
 * Node connections
 */
export interface INodeConnections {
  [outputType: string]: Array<IConnection[]>;
}

/**
 * Workflow connections
 */
export interface IConnections {
  [nodeName: string]: INodeConnections;
}

/**
 * Workflow definition
 */
export interface IWorkflow {
  id?: string;
  name: string;
  nodes: INode[];
  connections: IConnections;
  active: boolean;
  settings?: IWorkflowSettings;
  staticData?: IDataObject;
  pinData?: IPinData;
}

export interface IWorkflowSettings {
  executionOrder?: 'v0' | 'v1';
  saveManualExecutions?: boolean;
  callerPolicy?: string;
  errorWorkflow?: string;
  timezone?: string;
  saveExecutionProgress?: boolean;
  saveDataErrorExecution?: 'all' | 'none';
  saveDataSuccessExecution?: 'all' | 'none';
}

/**
 * Pin data for testing
 */
export interface IPinData {
  [nodeName: string]: INodeExecutionData[];
}

/**
 * Task data - result of node execution
 */
export interface ITaskData {
  data: ITaskDataConnections;
  executionTime: number;
  executionStatus?: 'success' | 'error' | 'unknown';
  executionIndex?: number;
  startTime: number;
  error?: NodeError;
  hints?: NodeExecutionHint[];
  source: Array<ISourceData | null>;
}

/**
 * Run data - all execution results
 */
export interface IRunData {
  [nodeName: string]: ITaskData[];
}

/**
 * Execution state
 */
export interface IExecutionState {
  runData: IRunData;
  pinData: IPinData;
  nodeExecutionStack: IExecuteData[];
  waitingExecution: IWaitingForExecution;
}

/**
 * Execute data - node to be executed
 */
export interface IExecuteData {
  node: INode;
  data: ITaskDataConnections;
  source: ITaskDataConnectionsSource | null;
  runIndex?: number;
}

export interface ITaskDataConnectionsSource {
  main?: Array<{
    node: string;
    data: ITaskDataConnections;
  }>;
}

export interface IWaitingForExecution {
  [nodeName: string]: {
    [outputIndex: string]: {
      [connectionIndex: string]: ITaskDataConnections;
    };
  };
}

/**
 * Run execution data - complete execution state
 */
export interface IRunExecutionData {
  startData?: {
    destinationNode?: string;
    runNodeFilter?: string[];
  };
  resultData: {
    runData: IRunData;
    pinData?: IPinData;
  };
  executionData?: {
    contextData: IDataObject;
    nodeExecutionStack: IExecuteData[];
    waitingExecution: IWaitingForExecution;
    metadata: { [nodeName: string]: ITaskMetadata[] };
  };
}

export interface ITaskMetadata {
  subExecution?: {
    executionId: string;
    workflowId: string;
  };
}

// ================================
// CREDENTIALS
// ================================

export interface ICredentialType {
  name: string;
  displayName: string;
  documentationUrl?: string;
  properties: INodeProperties[];
  authenticate?: {
    type: string;
    properties: IDataObject;
  };
  test?: ICredentialTestRequest;
}

export interface ICredentialsDecrypted {
  id: string;
  name: string;
  type: string;
  data: IDataObject;
}

export interface ICredentialDataDecryptedObject {
  [key: string]: string | number | boolean;
}

// ================================
// FINANCE-SPECIFIC TYPES
// ================================

/**
 * Market data structure
 */
export interface IMarketData {
  symbol: string;
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  vwap?: number;
}

/**
 * Portfolio holding
 */
export interface IPortfolioHolding {
  symbol: string;
  quantity: number;
  averageCost: number;
  currentPrice: number;
  marketValue: number;
  unrealizedPL: number;
  unrealizedPLPercent: number;
  weight: number;
}

/**
 * Technical indicator result
 */
export interface ITechnicalIndicatorResult {
  indicator: string;
  symbol: string;
  timestamp: number;
  values: {
    [key: string]: number;
  };
}

/**
 * Backtest result
 */
export interface IBacktestResult {
  strategy: string;
  period: {
    start: string;
    end: string;
  };
  metrics: {
    totalReturn: number;
    annualizedReturn: number;
    sharpeRatio: number;
    maxDrawdown: number;
    winRate: number;
    profitFactor: number;
  };
  trades: IBacktestTrade[];
}

export interface IBacktestTrade {
  symbol: string;
  action: 'BUY' | 'SELL';
  quantity: number;
  price: number;
  timestamp: number;
  pnl?: number;
}

// ================================
// EXPORTS
// ================================

export type {
  INodeExecutionData as NodeExecutionData,
  INodeType as NodeType,
  INode as Node,
  IWorkflow as Workflow,
};
