// Data Mapping Types and Interfaces

import { DataSourceConnection } from '../data-sources/types';

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

// ========== UNIFIED SCHEMAS ==========

export type FieldType = 'string' | 'number' | 'boolean' | 'datetime' | 'array' | 'object' | 'enum';

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

// ========== FIELD MAPPING ==========

export interface FieldMapping {
  targetField: string;           // Field name in unified schema
  sourceExpression: string;      // JSONPath/JSONata/etc expression
  parserEngine?: ParserEngine;   // Override default parser
  transform?: string;            // Transform function name
  transformParams?: any;         // Parameters for transform function
  defaultValue?: any;            // Fallback value
  required: boolean;
  confidence?: number;           // AI suggestion confidence (0-1)
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

export interface DataMappingConfig {
  id: string;
  name: string;
  description: string;

  // Source configuration
  source: {
    connectionId: string;        // Reference to DataSourceConnection
    connection?: DataSourceConnection; // Populated at runtime
    endpoint?: string;           // API endpoint or query
    method?: 'GET' | 'POST' | 'PUT' | 'DELETE';
    headers?: Record<string, string>;
    body?: any;
    sampleData?: any;            // Sample response for testing
  };

  // Target schema
  target: {
    schema: string;              // "OHLCV", "QUOTE", "ORDER", etc.
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

export interface MappingUIState {
  view: 'gallery' | 'create' | 'edit' | 'test';
  selectedMapping?: DataMappingConfig;
  selectedTemplate?: MappingTemplate;
  selectedConnection?: DataSourceConnection;

  // For creation/editing
  currentStep: 'source' | 'schema' | 'mapping' | 'testing' | 'review';

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
