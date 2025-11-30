/**
 * Strategy Definition Interface
 * Platform-independent strategy representation that works across all providers
 */

import { Node, Edge } from 'reactflow';
import { StrategyParameter } from './types';

// ============================================================================
// Main Strategy Definition
// ============================================================================

export interface StrategyDefinition {
  // Metadata
  id?: string;
  name: string;
  version: string;
  description: string;
  author: string;
  createdAt?: string;
  updatedAt?: string;
  tags?: string[];

  // Strategy type determines how it's executed
  type: 'code' | 'visual' | 'template';

  // Code-based strategy (Python only)
  code?: CodeStrategy;

  // Visual strategy (from node editor)
  visual?: VisualStrategy;

  // Template strategy (pre-built)
  template?: TemplateStrategy;

  // Parameters that can be optimized
  parameters: StrategyParameter[];

  // Requirements
  requires: StrategyRequirements;

  // Provider-specific metadata (optional, stored but not used by other providers)
  providerMetadata?: Record<string, any>;
}

// ============================================================================
// Code Strategy
// ============================================================================

export interface CodeStrategy {
  language: 'python'; // Python only
  source: string; // Actual Python code
  entryPoint?: string; // Main class or function name
  dependencies?: string[]; // Required Python packages
  version?: string; // Python version requirement
}

// ============================================================================
// Visual Strategy
// ============================================================================

export interface VisualStrategy {
  nodes: Node[]; // ReactFlow nodes
  edges: Edge[]; // ReactFlow edges
  metadata: VisualStrategyMetadata;
  generatedPython?: string; // Auto-generated Python code from visual
  lastGenerated?: string; // Timestamp of last code generation
}

export interface VisualStrategyMetadata {
  canvasWidth?: number;
  canvasHeight?: number;
  zoom?: number;
  version: string;
  nodeTypes: string[];
}

// ============================================================================
// Template Strategy
// ============================================================================

export interface TemplateStrategy {
  templateId: string;
  templateName: string;
  parameters: Record<string, any>;
  customizations?: TemplateCustomization[];
}

export interface TemplateCustomization {
  section: string;
  field: string;
  value: any;
}

// ============================================================================
// Strategy Requirements
// ============================================================================

export interface StrategyRequirements {
  dataFeeds: DataFeed[];
  indicators: string[];
  minCapital?: number;
  assetClasses?: string[];
  assets?: Array<{ symbol: string; assetClass: string }>;
  timeframes?: string[];
  brokerages?: string[];
  startDate?: string;
  endDate?: string;
  capital?: number;
}

export interface DataFeed {
  type: 'price' | 'fundamental' | 'alternative' | 'news' | 'sentiment';
  resolution: 'tick' | 'second' | 'minute' | 'hour' | 'daily';
  provider?: string;
  required: boolean;
}

// ============================================================================
// Strategy Templates Catalog
// ============================================================================

export interface StrategyTemplate {
  id: string;
  name: string;
  category: StrategyCategory;
  description: string;
  difficulty: 'beginner' | 'intermediate' | 'advanced';
  parameters: StrategyParameter[];
  previewImage?: string;
  tags: string[];
  author: string;
  rating?: number;
  downloads?: number;
}

export type StrategyCategory =
  | 'momentum'
  | 'mean_reversion'
  | 'trend_following'
  | 'arbitrage'
  | 'market_making'
  | 'statistical_arbitrage'
  | 'machine_learning'
  | 'options'
  | 'portfolio_rebalancing'
  | 'risk_parity';

// ============================================================================
// Strategy Builder Helpers
// ============================================================================

export interface StrategyValidationResult {
  valid: boolean;
  errors: string[];
  warnings: string[];
}

export interface StrategyComponents {
  entryRules: Rule[];
  exitRules: Rule[];
  positionSizing: PositionSizingRule;
  riskManagement: RiskManagementRule[];
  filters?: Filter[];
}

export interface Rule {
  id: string;
  type: string;
  condition: Condition;
  action: Action;
  enabled: boolean;
}

export interface Condition {
  indicator?: string;
  operator: 'gt' | 'lt' | 'eq' | 'gte' | 'lte' | 'cross_above' | 'cross_below';
  value: number | string;
  lookback?: number;
}

export interface Action {
  type: 'buy' | 'sell' | 'close_long' | 'close_short' | 'hold';
  quantity?: number;
  price?: 'market' | 'limit' | 'stop';
  offset?: number;
}

export interface PositionSizingRule {
  method: 'fixed' | 'percentage' | 'volatility' | 'kelly' | 'risk_parity';
  value: number;
  maxPosition?: number;
}

export interface RiskManagementRule {
  type: 'stop_loss' | 'take_profit' | 'trailing_stop' | 'time_limit';
  value: number;
  unit: 'percentage' | 'points' | 'atr' | 'days';
}

export interface Filter {
  type: 'time' | 'volatility' | 'volume' | 'trend' | 'correlation';
  condition: Condition;
}

// ============================================================================
// Strategy Conversion
// ============================================================================

export interface ConversionResult {
  success: boolean;
  convertedStrategy: StrategyDefinition;
  warnings: string[];
  errors: string[];
  unsupportedFeatures?: string[];
}

// ============================================================================
// Helper Functions Types
// ============================================================================

export type StrategyType = StrategyDefinition['type'];
export type StrategyLanguage = CodeStrategy['language'];
