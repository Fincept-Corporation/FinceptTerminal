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

// Import finance nodes
import { StockDataNode } from '../../nodes/finance/StockData.node';

// Import core nodes
import { FilterNode as CoreFilterNode } from '../../nodes/core/Filter.node';
import { MergeNode as CoreMergeNode } from '../../nodes/core/Merge.node';
import { SetNode } from '../../nodes/core/Set.node';
import { SwitchNode as CoreSwitchNode } from '../../nodes/core/Switch.node';
import { CodeNode as CoreCodeNode } from '../../nodes/core/Code.node';

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
  YFinanceNode,
} from './nodes/marketData';

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
      console.log('[NodeLoader] Nodes already loaded, skipping...');
      return;
    }

    console.log('[NodeLoader] Starting node loading...');

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

    // TODO: Load AI agent nodes when ready
    // await this.loadAIAgentNodes();

    this.loaded = true;

    // Print statistics
    const stats = NodeRegistry.getStatistics();
    console.log('[NodeLoader] Loading complete!');
    console.log(`  - Total node types: ${stats.totalNodeTypes}`);
    console.log(`  - Total versions: ${stats.totalVersions}`);
    console.log('  - Node types:');
    stats.nodeTypes.forEach((nt) => {
      console.log(`    • ${nt.name} (${nt.versions} version${nt.versions > 1 ? 's' : ''})`);
    });
  }

  /**
   * Load core nodes (basic workflow nodes)
   */
  private async loadCoreNodes(): Promise<void> {
    console.log('[NodeLoader] Loading core nodes...');

    try {
      // Filter Node - Remove items based on conditions
      const filterNode = new CoreFilterNode();
      NodeRegistry.registerNodeType(filterNode, 'CoreFilterNode');
      this.loadedNodes.set('filter', filterNode);

      // Merge Node - Combine data from multiple inputs
      const mergeNode = new CoreMergeNode();
      NodeRegistry.registerNodeType(mergeNode, 'CoreMergeNode');
      this.loadedNodes.set('coreMerge', mergeNode);

      // Set Node - Add/edit/remove fields
      const setNode = new SetNode();
      NodeRegistry.registerNodeType(setNode, 'SetNode');
      this.loadedNodes.set('set', setNode);

      // Switch Node - Route items based on conditions
      const switchNode = new CoreSwitchNode();
      NodeRegistry.registerNodeType(switchNode, 'CoreSwitchNode');
      this.loadedNodes.set('coreSwitch', switchNode);

      // Code Node - Execute custom JavaScript
      const codeNode = new CoreCodeNode();
      NodeRegistry.registerNodeType(codeNode, 'CoreCodeNode');
      this.loadedNodes.set('coreCode', codeNode);

      console.log('[NodeLoader] Core nodes loaded (5 nodes)');
    } catch (error) {
      console.error('[NodeLoader] Error loading core nodes:', error);
    }
  }

  /**
   * Load finance-specific nodes
   */
  private async loadFinanceNodes(): Promise<void> {
    console.log('[NodeLoader] Loading finance nodes...');

    try {
      // Stock Data Node
      const stockDataNode = new StockDataNode();
      NodeRegistry.registerNodeType(stockDataNode, 'StockDataNode');
      this.loadedNodes.set('stockData', stockDataNode);

      // More finance nodes will be added:
      // - TechnicalIndicators Node
      // - PortfolioOptimizer Node
      // - RiskCalculator Node
      // - BacktestStrategy Node
      // - OptionsAnalysis Node
      // - FundamentalAnalysis Node
      // - SentimentAnalysis Node
      // - EconomicData Node

      console.log('[NodeLoader] Finance nodes loaded (1 node)');
    } catch (error) {
      console.error('[NodeLoader] Error loading finance nodes:', error);
    }
  }

  /**
   * Load AI agent nodes (auto-generated from Python agents)
   * TODO: Implement agent node generator
   */
  private async loadAIAgentNodes(): Promise<void> {
    console.log('[NodeLoader] AI agent nodes not yet implemented, skipping...');

    // TODO: Uncomment when agent generator is ready
    /*
    try {
      const stats = await getAgentNodeStatistics();
      console.log(`[NodeLoader] Found ${stats.totalAgents} Python agents across ${stats.categories} categories:`);
      for (const [category, count] of Object.entries(stats.byCategory)) {
        console.log(`  - ${category}: ${count} agents`);
      }

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
    console.log('[NodeLoader] Loading analytics nodes...');

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

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }

      console.log(`[NodeLoader] Analytics nodes loaded (${nodes.length} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading analytics nodes:', error);
    }
  }

  /**
   * Load market data nodes
   */
  private async loadMarketDataNodes(): Promise<void> {
    console.log('[NodeLoader] Loading market data nodes...');

    try {
      const nodes = [
        { instance: new YFinanceNode(), name: 'YFinanceNode' },
      ];

      for (const { instance, name } of nodes) {
        NodeRegistry.registerNodeType(instance, name);
        this.loadedNodes.set(instance.description.name, instance);
      }

      console.log(`[NodeLoader] Market data nodes loaded (${nodes.length} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading market data nodes:', error);
    }
  }

  /**
   * Load control flow nodes (Phase 7)
   */
  private async loadControlFlowNodes(): Promise<void> {
    console.log('[NodeLoader] Loading control flow nodes...');

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

      console.log(`[NodeLoader] Control flow nodes loaded (${nodes.length} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading control flow nodes:', error);
    }
  }

  /**
   * Load safety nodes (Phase 8)
   */
  private async loadSafetyNodes(): Promise<void> {
    console.log('[NodeLoader] Loading safety nodes...');

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

      console.log(`[NodeLoader] Safety nodes loaded (${nodes.length} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading safety nodes:', error);
    }
  }

  /**
   * Load notification nodes (Phase 9)
   */
  private async loadNotificationNodes(): Promise<void> {
    console.log('[NodeLoader] Loading notification nodes...');

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

      console.log(`[NodeLoader] Notification nodes loaded (${nodes.length} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading notification nodes:', error);
    }
  }

  /**
   * Load transform nodes (Phase 10)
   */
  private async loadTransformNodes(): Promise<void> {
    console.log('[NodeLoader] Loading transform nodes...');

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

      console.log(`[NodeLoader] Transform nodes loaded (${nodes.length} nodes)`);
    } catch (error) {
      console.error('[NodeLoader] Error loading transform nodes:', error);
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
    console.log('[NodeLoader] Reloading all nodes...');
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
  console.log('[NodeSystem] Initializing...');

  try {
    await NodeLoader.loadAll();
    console.log('[NodeSystem] Initialization complete!');
  } catch (error) {
    console.error('[NodeSystem] Initialization failed:', error);
    throw error;
  }
}
