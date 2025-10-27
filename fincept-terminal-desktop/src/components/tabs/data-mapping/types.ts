// Data Mapping Types and Interfaces

// ========== PARSER ENGINES ==========

export enum ParserEngine {
  JSONPATH = 'jsonpath',      // Simple path queries: $.data.candles[*]
  JSONATA = 'jsonata',         // Powerful transformations: $.data.candles.{ timestamp: $[0] }
  JMESPATH = 'jmespath',       // AWS-style queries: data.candles[*].[0,1,2,3,4,5]
  CUSTOM_JS = 'javascript',    // Full JavaScript expressions
  REGEX = 'regex',             // Pattern matching
  DIRECT = 'direct'            // Simple object access
}

export interface ParserConfig {
  engine: ParserEngine;
  expression: string;
  fallback?: string;
  validation?: ValidationRule[];
}

// ========== API CONFIGURATION ==========

export interface APIAuthConfig {
  type: 'none' | 'apikey' | 'bearer' | 'basic' | 'oauth2';
  config?: {
    apiKey?: {
      location: 'header' | 'query';
      keyName: string;
      value: string;
    };
    bearer?: {
      token: string;
    };
    basic?: {
      username: string;
      password: string;
    };
    oauth2?: {
      accessToken: string;
      refreshToken?: string;
    };
  };
}

export interface APIConfig {
  id: string;
  name: string;
  description: string;
  baseUrl: string;
  endpoint: string;
  method: 'GET' | 'POST' | 'PUT' | 'DELETE' | 'PATCH';
  authentication: APIAuthConfig;
  headers: Record<string, string>;
  queryParams: Record<string, string>; // Supports {placeholder} syntax
  body?: string; // JSON string for POST/PUT/PATCH
  timeout?: number; // milliseconds
  cacheEnabled: boolean;
  cacheTTL: number; // seconds
}

export interface APIResponse {
  success: boolean;
  data?: any;
  error?: {
    type: 'network' | 'auth' | 'client' | 'server' | 'timeout' | 'parse';
    statusCode?: number;
    message: string;
    details?: string;
  };
  metadata: {
    duration: number;
    url: string;
    method: string;
    timestamp: string;
  };
}

// ========== UNIFIED SCHEMAS ==========

export type FieldType = 'string' | 'number' | 'boolean' | 'datetime' | 'array' | 'object' | 'enum';

export type UnifiedSchemaType = 'OHLCV' | 'QUOTE' | 'TICK' | 'ORDER' | 'POSITION' | 'PORTFOLIO' | 'INSTRUMENT';

export interface FieldSpec {
  type: FieldType;
  required: boolean;
  description: string;
  values?: string[];  // For enum types
  defaultValue?: any;
  validation?: {
    min?: number;
    max?: number;
    pattern?: string;
    custom?: string;  // Custom validation expression
  };
}

export interface UnifiedSchema {
  name: string;
  category: 'market-data' | 'trading' | 'portfolio' | 'reference' | 'custom';
  description: string;
  fields: Record<string, FieldSpec>;
  examples?: any[];
}

// Custom field definition for user-created schemas
export interface CustomField {
  name: string;
  type: FieldType;
  description: string;
  required: boolean;
  defaultValue?: any;
  validation?: {
    min?: number;
    max?: number;
    pattern?: string;
  };
}

// ========== FIELD MAPPING ==========

export interface FieldMapping {
  targetField: string;           // Field name in unified schema
  source: {
    type: 'path' | 'static' | 'expression';
    path?: string;               // JSONPath/JSONata/etc expression
    value?: any;                 // For static values
  };
  parser: 'jsonpath' | 'jmespath' | 'direct' | 'javascript' | 'jsonata';  // Parser engine (string literals for now)
  transform?: string[];          // Transform function names
  transformParams?: any;         // Parameters for transform function
  defaultValue?: any;            // Fallback value
  required?: boolean;
  confidence?: number;           // AI suggestion confidence (0-1)

  // Legacy support
  sourceExpression?: string;      // Deprecated: use source.path
  parserEngine?: ParserEngine;   // Deprecated: use parser
}

// ========== TRANSFORMATION FUNCTIONS ==========

export type TransformFunction = (value: any, context?: any) => any;

export interface TransformDefinition {
  name: string;
  description: string;
  category: 'type' | 'format' | 'math' | 'string' | 'date' | 'custom';
  parameters?: {
    name: string;
    type: string;
    required: boolean;
    defaultValue?: any;
  }[];
  examples: {
    input: any;
    output: any;
    params?: any;
  }[];
  implementation: TransformFunction;
}

// ========== VALIDATION ==========

export interface ValidationRule {
  field: string;
  rule: 'required' | 'type' | 'range' | 'pattern' | 'custom';
  params?: any;
  message?: string;
}

export interface ValidationResult {
  valid: boolean;
  errors: {
    field: string;
    message: string;
    rule: string;
  }[];
}

// ========== MAPPING CONFIGURATION ==========

export interface MappingSource {
  type: 'api' | 'sample';
  apiConfig?: APIConfig;        // Direct API configuration
  sampleData?: any;             // For testing with static data
}

export interface DataMappingConfig {
  id: string;
  name: string;
  description: string;

  // Source configuration (API-based)
  source: MappingSource;

  // Target schema (predefined or custom)
  target: {
    schemaType: 'predefined' | 'custom';
    schema?: string;              // "OHLCV", "QUOTE", etc. (for predefined)
    customFields?: CustomField[]; // User-defined fields (for custom)
    schemaDefinition?: UnifiedSchema; // Populated at runtime
  };

  // Data extraction
  extraction: {
    engine: ParserEngine;        // Default parser engine
    rootPath?: string;           // Path to data array/object
    isArray: boolean;            // Is source data an array?
    isArrayOfArrays?: boolean;   // Special case for [[...], [...]]
  };

  // Field mappings
  fieldMappings: FieldMapping[];

  // Post-processing
  postProcessing?: {
    filter?: string;             // JSONata filter expression
    sort?: {
      field: string;
      order: 'asc' | 'desc';
    };
    limit?: number;
    deduplicate?: string;        // Deduplicate by field
  };

  // Validation
  validation: {
    enabled: boolean;
    strictMode: boolean;         // Fail on validation errors?
    rules: ValidationRule[];
  };

  // Metadata
  metadata: {
    createdAt: string;
    updatedAt: string;
    version: number;
    tags: string[];
    aiGenerated: boolean;
    confidence?: number;
    author?: string;
    testResults?: {
      lastTested: string;
      success: boolean;
      sampleSize: number;
      errors?: string[];
    };
  };
}

// ========== MAPPING EXECUTION ==========

export interface MappingExecutionContext {
  mapping: DataMappingConfig;
  data?: any;  // Sample data for testing, if not fetched from connection
  parameters?: Record<string, any>;  // Runtime parameters (symbol, date range, etc.)
  options?: {
    skipValidation?: boolean;
    returnRaw?: boolean;
    debug?: boolean;
  };
}

export interface MappingExecutionResult {
  success: boolean;
  data?: any;
  rawData?: any;
  errors?: string[];
  warnings?: string[];
  metadata: {
    executedAt: string;
    duration: number;
    recordsProcessed: number;
    recordsReturned: number;
    validationResults?: ValidationResult;
  };
}

// ========== API ENDPOINT METADATA ==========

export interface EndpointDefinition {
  path: string;
  method: 'GET' | 'POST' | 'PUT' | 'DELETE';
  description: string;
  parameters: {
    name: string;
    type: string;
    required: boolean;
    location: 'path' | 'query' | 'header' | 'body';
    description: string;
    example?: any;
  }[];
  authentication: {
    type: 'none' | 'apikey' | 'oauth' | 'basic' | 'bearer';
    location?: 'header' | 'query';
    keyName?: string;
  };
  sampleResponse?: any;
  rateLimit?: {
    requests: number;
    period: string;
  };
}

// ========== BROKER-SPECIFIC METADATA ==========

export interface BrokerMetadata {
  id: string;
  name: string;
  region: 'india' | 'usa' | 'global';
  type: 'broker' | 'data-provider' | 'exchange';
  website: string;
  documentationUrl: string;

  endpoints: Record<string, EndpointDefinition>;

  authentication: {
    type: 'apikey' | 'oauth2' | 'basic' | 'token';
    steps: string[];
    docsUrl: string;
  };

  dataFormats: {
    quotes?: string;
    ohlcv?: string;
    orders?: string;
    positions?: string;
  };

  commonIssues?: {
    issue: string;
    solution: string;
  }[];
}

// ========== TEMPLATE ==========

export interface MappingTemplate {
  id: string;
  name: string;
  description: string;
  category: 'broker' | 'data-provider' | 'exchange' | 'custom';
  tags: string[];

  broker?: BrokerMetadata;

  // Pre-configured mapping
  mappingConfig: Partial<DataMappingConfig>;

  // Usage instructions
  instructions?: string;

  // Popularity metrics
  usageCount?: number;
  rating?: number;

  verified: boolean;
  official: boolean;
}

// ========== AI SUGGESTIONS ==========

export interface AIFieldSuggestion {
  targetField: string;
  suggestedExpression: string;
  confidence: number;
  method: 'exact' | 'alias' | 'type' | 'semantic' | 'pattern';
  alternates?: {
    expression: string;
    confidence: number;
  }[];
}

export interface AIMappingSuggestion {
  fieldSuggestions: AIFieldSuggestion[];
  overallConfidence: number;
  warnings: string[];
  suggestions: string[];
}

// ========== FLATTENED JSON (for visualization) ==========

export interface FlattenedJSON {
  [path: string]: {
    value: any;
    type: string;
    depth: number;
    isLeaf: boolean;
    arrayIndex?: number;
  };
}

// ========== UI STATE ==========

export type CreateStep = 'api-config' | 'schema-select' | 'field-mapping' | 'cache-settings' | 'test-save';

export interface MappingUIState {
  view: 'gallery' | 'create' | 'edit' | 'list';
  selectedMapping?: DataMappingConfig;
  selectedTemplate?: MappingTemplate;

  // For creation/editing
  currentStep: CreateStep;

  // JSON Explorer state
  jsonExplorer: {
    data?: any;
    expandedPaths: Set<string>;
    selectedPath?: string[];
  };

  // Expression builder state
  expressionBuilder: {
    engine: ParserEngine;
    expression: string;
    testData: string;
    result?: any;
    error?: string;
  };

  // Testing state
  testing: {
    isRunning: boolean;
    results?: MappingExecutionResult;
  };
}
