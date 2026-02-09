/**
 * Node Loader
 * Automatically discovers and registers all available nodes
 *
 * This module handles:
 * - Loading node definitions from the nodes directory
 * - Registering nodes with the Node Registry
 * - Providing a centralized initialization point
 */

import { NodeRegistry } from './NodeRegistry';
import { INodeType } from './types';

// Import core nodes
import { SetNode, CodeNode } from './nodes/core';

// Import analytics nodes
import {
  TechnicalIndicatorsNode,
  // TechnicalIndicatorsPythonNode, // Deleted
  PortfolioOptimizationNode,
  BacktestEngineNode,
  RiskAnalysisNode,
  PerformanceMetricsNode,
  CorrelationMatrixNode,
} from './nodes/analytics';

// Import market data nodes
import {
  GetQuoteNode,
  GetMarketDepthNode,
  StreamQuotesNode,
  GetFundamentalsNode,
  GetTickerStatsNode,
} from './nodes/marketData';

// Import trading nodes
import {
  PlaceOrderNode,
  ModifyOrderNode,
  CancelOrderNode,
  GetPositionsNode,
  GetHoldingsNode,
  GetOrdersNode,
  GetBalanceNode,
  ClosePositionNode,
} from './nodes/trading';

// Import trigger nodes
import {
  ManualTriggerNode,
  ScheduleTriggerNode,
  PriceAlertTriggerNode,
  NewsEventTriggerNode,
  MarketEventTriggerNode,
  WebhookTriggerNode,
} from './nodes/triggers';

// Import agent nodes
import { AgentMediatorNode } from './nodes/agents/AgentMediatorNode';
import { AgentNode } from './nodes/agents/AgentNode';
import { MultiAgentNode } from './nodes/agents/MultiAgentNode';
import { GeopoliticsAgentNode } from './nodes/agents/GeopoliticsAgentNode';
import { HedgeFundAgentNode } from './nodes/agents/HedgeFundAgentNode';
import { InvestorAgentNode } from './nodes/agents/InvestorAgentNode';

// Import agent node generator for dynamic Python agent nodes
import { generateAllAgentNodes, getAgentNodeStatistics } from './utils/AgentNodeGenerator';

// Import control flow nodes
import {
  IfElseNode,
  SwitchNode,
  LoopNode,
  WaitNode,
  MergeNode,
  SplitNode,
  ErrorHandlerNode,
  // CodeNode, // Deleted
  // StopAndErrorNode, // Deleted
  // CompareDatasetsNode, // Deleted
  // NoOpNode, // Deleted
} from './nodes/controlFlow';

// Import safety nodes
import {
  RiskCheckNode,
  PositionSizeLimitNode,
  LossLimitNode,
  TradingHoursCheckNode,
} from './nodes/safety';

// Import notification nodes
import {
  WebhookNotificationNode,
  EmailNode,
  TelegramNode,
  DiscordNode,
  SlackNode,
  SMSNode,
} from './nodes/notifications';

// Import transform nodes
import {
  FilterTransformNode,
  MapNode,
  AggregateNode,
  SortNode,
  GroupByNode,
  JoinNode,
  DeduplicateNode,
  ReshapeNode,
} from './nodes/transform';

// TODO: Import agent node generator when ready
// import { generateAllAgentNodes, getAgentNodeStatistics } from './utils/AgentNodeGenerator';

/**
 * Node Loader Class
 */
class NodeLoaderClass {
  private loaded: boolean = false;
  private loadedNodes: Map<string, INodeType> = new Map();

  /**
   * Load all available nodes
   */
  async loadAll(): Promise<void> {
    if (this.loaded) {
      return;
    }

    // Load core nodes
    await this.loadCoreNodes();

    // Load finance nodes
    await this.loadFinanceNodes();

    // Load analytics nodes
    await this.loadAnalyticsNodes();

    // Load market data nodes
    await this.loadMarketDataNodes();

    // Load control flow nodes
    await this.loadControlFlowNodes();

    // Load safety nodes
    await this.loadSafetyNodes();

    // Load notification nodes
    await this.loadNotificationNodes();

    // Load transform nodes
    await this.loadTransformNodes();

    // Load trading nodes
    await this.loadTradingNodes();

    // Load trigger nodes
    await this.loadTriggerNodes();

    // Load AI agent nodes
    await this.loadAgentNodes();

    this.loaded = true;
  }

  /**
   * Load core nodes (basic workflow nodes)
   * Note: Filter, Merge, Switch are now in transform/controlFlow - removed duplicates
   */
  private async loadCoreNodes(): Promise<void> {
    try {
      const nodes = [
        { instance: new SetNode(), name: 'SetNode' },
        { instance: new CodeNode(), name: 'CodeNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading core nodes:', error);
    }
  }

  /**
   * Load finance-specific nodes
   * Note: StockData removed - use GetQuote, GetFundamentals instead
   */
  private async loadFinanceNodes(): Promise<void> {
    // Finance nodes are now loaded via analytics and marketData categories
  }

  /**
   * Load AI agent nodes (auto-generated from Python agents)
   * TODO: Implement agent node generator
   */
  private async loadAIAgentNodes(): Promise<void> {
    // TODO: Uncomment when agent generator is ready
    /*
    try {
      const stats = await getAgentNodeStatistics();

      const agentNodes = await generateAllAgentNodes();

      let registered = 0;
      for (const [agentId, nodeType] of agentNodes.entries()) {
        try {
          NodeRegistry.registerNodeType(nodeType, `${agentId}AgentNode`);
          this.loadedNodes.set(agentId, nodeType);
          registered++;
        } catch (error) {
          console.error(`[NodeLoader] Failed to register agent node: ${agentId}`, error);
        }
      }

      console.log(`[NodeLoader] AI agent nodes loaded (${registered} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading AI agent nodes:', error);
    }
    */
  }

  /**
   * Load analytics nodes (Phase 6)
   */
  private async loadAnalyticsNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new TechnicalIndicatorsNode(), name: 'TechnicalIndicatorsNode' },
        // { instance: new TechnicalIndicatorsPythonNode(), name: 'TechnicalIndicatorsPythonNode' }, // Deleted
        { instance: new PortfolioOptimizationNode(), name: 'PortfolioOptimizationNode' },
        { instance: new BacktestEngineNode(), name: 'BacktestEngineNode' },
        { instance: new RiskAnalysisNode(), name: 'RiskAnalysisNode' },
        { instance: new PerformanceMetricsNode(), name: 'PerformanceMetricsNode' },
        { instance: new CorrelationMatrixNode(), name: 'CorrelationMatrixNode' },
      ];

      for (const { instance, name} of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading analytics nodes:', error);
    }
  }

  /**
   * Load market data nodes
   */
  private async loadMarketDataNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new GetQuoteNode(), name: 'GetQuoteNode' },
        { instance: new GetMarketDepthNode(), name: 'GetMarketDepthNode' },
        { instance: new StreamQuotesNode(), name: 'StreamQuotesNode' },
        { instance: new GetFundamentalsNode(), name: 'GetFundamentalsNode' },
        { instance: new GetTickerStatsNode(), name: 'GetTickerStatsNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading market data nodes:', error);
    }
  }

  /**
   * Load control flow nodes (Phase 7)
   */
  private async loadControlFlowNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new IfElseNode(), name: 'IfElseNode' },
        { instance: new SwitchNode(), name: 'SwitchNode' },
        { instance: new LoopNode(), name: 'LoopNode' },
        { instance: new WaitNode(), name: 'WaitNode' },
        { instance: new MergeNode(), name: 'MergeNode' },
        { instance: new SplitNode(), name: 'SplitNode' },
        { instance: new ErrorHandlerNode(), name: 'ErrorHandlerNode' },
        // { instance: new CodeNode(), name: 'CodeNode' }, // Deleted
        // { instance: new StopAndErrorNode(), name: 'StopAndErrorNode' }, // Deleted
        // { instance: new CompareDatasetsNode(), name: 'CompareDatasetsNode' }, // Deleted
        // { instance: new NoOpNode(), name: 'NoOpNode' }, // Deleted
      ];

      for (const { instance, name} of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading control flow nodes:', error);
    }
  }

  /**
   * Load safety nodes (Phase 8)
   */
  private async loadSafetyNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new RiskCheckNode(), name: 'RiskCheckNode' },
        { instance: new PositionSizeLimitNode(), name: 'PositionSizeLimitNode' },
        { instance: new LossLimitNode(), name: 'LossLimitNode' },
        { instance: new TradingHoursCheckNode(), name: 'TradingHoursCheckNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading safety nodes:', error);
    }
  }

  /**
   * Load notification nodes (Phase 9)
   */
  private async loadNotificationNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new WebhookNotificationNode(), name: 'WebhookNotificationNode' },
        { instance: new EmailNode(), name: 'EmailNode' },
        { instance: new TelegramNode(), name: 'TelegramNode' },
        { instance: new DiscordNode(), name: 'DiscordNode' },
        { instance: new SlackNode(), name: 'SlackNode' },
        { instance: new SMSNode(), name: 'SMSNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading notification nodes:', error);
    }
  }

  /**
   * Load transform nodes (Phase 10)
   */
  private async loadTransformNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new FilterTransformNode(), name: 'FilterTransformNode' },
        { instance: new MapNode(), name: 'MapNode' },
        { instance: new AggregateNode(), name: 'AggregateNode' },
        { instance: new SortNode(), name: 'SortNode' },
        { instance: new GroupByNode(), name: 'GroupByNode' },
        { instance: new JoinNode(), name: 'JoinNode' },
        { instance: new DeduplicateNode(), name: 'DeduplicateNode' },
        { instance: new ReshapeNode(), name: 'ReshapeNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading transform nodes:', error);
    }
  }

  /**
   * Load trading nodes
   */
  private async loadTradingNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new PlaceOrderNode(), name: 'PlaceOrderNode' },
        { instance: new ModifyOrderNode(), name: 'ModifyOrderNode' },
        { instance: new CancelOrderNode(), name: 'CancelOrderNode' },
        { instance: new GetPositionsNode(), name: 'GetPositionsNode' },
        { instance: new GetHoldingsNode(), name: 'GetHoldingsNode' },
        { instance: new GetOrdersNode(), name: 'GetOrdersNode' },
        { instance: new GetBalanceNode(), name: 'GetBalanceNode' },
        { instance: new ClosePositionNode(), name: 'ClosePositionNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading trading nodes:', error);
    }
  }

  /**
   * Load trigger nodes
   */
  private async loadTriggerNodes(): Promise<void> {

    try {
      const nodes = [
        { instance: new ManualTriggerNode(), name: 'ManualTriggerNode' },
        { instance: new ScheduleTriggerNode(), name: 'ScheduleTriggerNode' },
        { instance: new PriceAlertTriggerNode(), name: 'PriceAlertTriggerNode' },
        { instance: new NewsEventTriggerNode(), name: 'NewsEventTriggerNode' },
        { instance: new MarketEventTriggerNode(), name: 'MarketEventTriggerNode' },
        { instance: new WebhookTriggerNode(), name: 'WebhookTriggerNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading trigger nodes:', error);
    }
  }

  /**
   * Load AI agent nodes (static + dynamic from Python agents)
   */
  private async loadAgentNodes(): Promise<void> {

    let staticCount = 0;
    let dynamicCount = 0;

    // Load static agent nodes (TypeScript-defined)
    try {
      const staticNodes = [
        { instance: new AgentMediatorNode(), name: 'AgentMediatorNode' },
        { instance: new AgentNode(), name: 'AgentNode' },
        { instance: new MultiAgentNode(), name: 'MultiAgentNode' },
        { instance: new GeopoliticsAgentNode(), name: 'GeopoliticsAgentNode' },
        { instance: new HedgeFundAgentNode(), name: 'HedgeFundAgentNode' },
        { instance: new InvestorAgentNode(), name: 'InvestorAgentNode' },
      ];

      for (const { instance, name } of staticNodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
        staticCount++;
      }
    } catch (error) {
      console.error('[NodeLoader] Error loading static agent nodes:', error);
    }

    // Load dynamic agent nodes (generated from Python agent metadata)
    try {
      // Get statistics first
      const stats = await getAgentNodeStatistics();

      if (stats.totalAgents > 0) {

        // Generate and register dynamic agent nodes
        const dynamicAgentNodes = await generateAllAgentNodes();

        for (const [agentId, nodeType] of dynamicAgentNodes.entries()) {
          try {
            // Avoid duplicates - check if already registered
            if (!this.loadedNodes.has(agentId)) {
              NodeRegistry.registerNodeType(nodeType, `${agentId}AgentNode`);
              this.loadedNodes.set(agentId, nodeType);
              dynamicCount++;
            }
          } catch (regError) {
            console.warn(`[NodeLoader] Failed to register dynamic agent node: ${agentId}`, regError);
          }
        }
      }
    } catch (error) {
      // Dynamic agent loading is optional - don't fail the entire loading
      console.warn('[NodeLoader] Dynamic agent node generation skipped:', error);
    }
  }

  /**
   * Get loaded node instance
   */
  getNode(nodeTypeName: string): INodeType | undefined {
    return this.loadedNodes.get(nodeTypeName);
  }

  /**
   * Check if loader has been initialized
   */
  isLoaded(): boolean {
    return this.loaded;
  }

  /**
   * Reload all nodes (for development)
   */
  async reload(): Promise<void> {
    this.loaded = false;
    this.loadedNodes.clear();
    NodeRegistry.clear();
    await this.loadAll();
  }

  /**
   * Get all loaded node names
   */
  getLoadedNodeNames(): string[] {
    return Array.from(this.loadedNodes.keys());
  }
}

// Export singleton instance
export const NodeLoader = new NodeLoaderClass();

/**
 * Initialize the node system
 * Call this once during app startup
 */
export async function initializeNodeSystem(): Promise<void> {
  try {
    await NodeLoader.loadAll();
  } catch (error) {
    console.error('[NodeSystem] Initialization failed:', error);
    throw error;
  }
}
