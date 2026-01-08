/**
 * Context Factory
 * Creates appropriate execution context based on node type
 *
 * This factory determines which context to use based on:
 * - Node type (trigger, poll, webhook, standard)
 * - Node category (trading, analytics, etc.)
 * - Node configuration
 */

import {
  INode,
  IWorkflowDataProxy,
  ITaskDataConnections,
  IRunData,
  INodeTypeDescription,
} from './types';
import {
  BaseExecuteContext,
  ExecutionContext,
  PollContext,
  TriggerContext,
  WebhookContext,
  LoadOptionsContext,
  TradingContext,
  BacktestContext,
  MarketDataContext,
} from './ExecutionContext';
import { NodeRegistry } from './NodeRegistry';

/**
 * Context type enum for easy identification
 */
export enum ContextType {
  Execute = 'execute',
  Poll = 'poll',
  Trigger = 'trigger',
  Webhook = 'webhook',
  LoadOptions = 'loadOptions',
  Trading = 'trading',
  Backtest = 'backtest',
  MarketData = 'marketData',
}

/**
 * Context factory options
 */
export interface IContextFactoryOptions {
  workflow: IWorkflowDataProxy;
  node: INode;
  inputData: ITaskDataConnections;
  runData: IRunData;
  runIndex?: number;
  executionId?: string;
  workflowId?: string;

  // For specific contexts
  emitCallback?: (data: any[][]) => void;
  emitErrorCallback?: (error: Error) => void;
  requestData?: any;
  currentNodeParameters?: Record<string, any>;
  initialCash?: number;
}

/**
 * Context Factory Class
 * Determines and creates the appropriate execution context for a node
 */
export class ContextFactory {
  /**
   * Create execution context for a node
   */
  static createContext(options: IContextFactoryOptions): BaseExecuteContext {
    const contextType = this.determineContextType(options.node);

    switch (contextType) {
      case ContextType.Poll:
        return this.createPollContext(options);

      case ContextType.Trigger:
        return this.createTriggerContext(options);

      case ContextType.Webhook:
        return this.createWebhookContext(options);

      case ContextType.LoadOptions:
        return this.createLoadOptionsContext(options);

      case ContextType.Trading:
        return this.createTradingContext(options);

      case ContextType.Backtest:
        return this.createBacktestContext(options);

      case ContextType.MarketData:
        return this.createMarketDataContext(options);

      case ContextType.Execute:
      default:
        return this.createExecuteContext(options);
    }
  }

  /**
   * Determine context type from node
   */
  private static determineContextType(node: INode): ContextType {
    // Get node type description
    const nodeType = NodeRegistry.getNodeType(node.type, node.typeVersion);
    const description = nodeType.description;

    // Check node name patterns for finance-specific contexts
    const nodeName = node.type.toLowerCase();

    // Trading nodes
    if (
      nodeName.includes('order') ||
      nodeName.includes('trade') ||
      nodeName.includes('position') ||
      description.group.includes('trading')
    ) {
      return ContextType.Trading;
    }

    // Backtest nodes
    if (
      nodeName.includes('backtest') ||
      nodeName.includes('strategy') ||
      description.group.includes('backtest')
    ) {
      return ContextType.Backtest;
    }

    // Market data streaming nodes
    if (
      nodeName.includes('stream') ||
      nodeName.includes('quote') ||
      nodeName.includes('marketdata') ||
      description.group.includes('marketData')
    ) {
      return ContextType.MarketData;
    }

    // Standard n8n context types
    if (nodeName.includes('poll') || description.group.includes('trigger-poll')) {
      return ContextType.Poll;
    }

    if (
      nodeName.includes('trigger') ||
      nodeName.includes('webhook') ||
      description.group.includes('trigger')
    ) {
      // Check if webhook specifically
      if (nodeName.includes('webhook')) {
        return ContextType.Webhook;
      }
      return ContextType.Trigger;
    }

    // Default to execute context
    return ContextType.Execute;
  }

  /**
   * Create execute context
   */
  private static createExecuteContext(options: IContextFactoryOptions): ExecutionContext {
    return new ExecutionContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || ''
    );
  }

  /**
   * Create poll context
   */
  private static createPollContext(options: IContextFactoryOptions): PollContext {
    return new PollContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || '',
      options.emitCallback,
      options.emitErrorCallback
    );
  }

  /**
   * Create trigger context
   */
  private static createTriggerContext(options: IContextFactoryOptions): TriggerContext {
    return new TriggerContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || '',
      options.emitCallback,
      options.emitErrorCallback
    );
  }

  /**
   * Create webhook context
   */
  private static createWebhookContext(options: IContextFactoryOptions): WebhookContext {
    return new WebhookContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.requestData || {},
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || ''
    );
  }

  /**
   * Create load options context
   */
  private static createLoadOptionsContext(options: IContextFactoryOptions): LoadOptionsContext {
    return new LoadOptionsContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.currentNodeParameters
    );
  }

  /**
   * Create trading context
   */
  private static createTradingContext(options: IContextFactoryOptions): TradingContext {
    return new TradingContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || ''
    );
  }

  /**
   * Create backtest context
   */
  private static createBacktestContext(options: IContextFactoryOptions): BacktestContext {
    return new BacktestContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.initialCash || 100000,
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || ''
    );
  }

  /**
   * Create market data context
   */
  private static createMarketDataContext(options: IContextFactoryOptions): MarketDataContext {
    return new MarketDataContext(
      options.workflow,
      options.node,
      options.inputData,
      options.runData,
      options.runIndex || 0,
      options.executionId || '',
      options.workflowId || ''
    );
  }

  /**
   * Get context type for a node (useful for debugging/logging)
   */
  static getContextType(node: INode): ContextType {
    return this.determineContextType(node);
  }

  /**
   * Check if node is a trigger node
   */
  static isTriggerNode(node: INode): boolean {
    const contextType = this.determineContextType(node);
    return contextType === ContextType.Poll || contextType === ContextType.Trigger;
  }

  /**
   * Check if node is a trading node
   */
  static isTradingNode(node: INode): boolean {
    return this.determineContextType(node) === ContextType.Trading;
  }

  /**
   * Check if node is a backtest node
   */
  static isBacktestNode(node: INode): boolean {
    return this.determineContextType(node) === ContextType.Backtest;
  }
}

/**
 * Helper function to create context (shorthand)
 */
export function createExecutionContext(options: IContextFactoryOptions): BaseExecuteContext {
  return ContextFactory.createContext(options);
}
