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
import { FilterNode } from '../../nodes/core/Filter.node';
import { MergeNode } from '../../nodes/core/Merge.node';
import { SetNode } from '../../nodes/core/Set.node';
import { SwitchNode } from '../../nodes/core/Switch.node';
import { CodeNode } from '../../nodes/core/Code.node';

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

    // Load integration nodes (will be added later)
    // await this.loadIntegrationNodes();

    // Load AI nodes (will be added later)
    // await this.loadAINodes();

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
      const filterNode = new FilterNode();
      NodeRegistry.registerNodeType(filterNode, 'FilterNode');
      this.loadedNodes.set('filter', filterNode);

      // Merge Node - Combine data from multiple inputs
      const mergeNode = new MergeNode();
      NodeRegistry.registerNodeType(mergeNode, 'MergeNode');
      this.loadedNodes.set('merge', mergeNode);

      // Set Node - Add/edit/remove fields
      const setNode = new SetNode();
      NodeRegistry.registerNodeType(setNode, 'SetNode');
      this.loadedNodes.set('set', setNode);

      // Switch Node - Route items based on conditions
      const switchNode = new SwitchNode();
      NodeRegistry.registerNodeType(switchNode, 'SwitchNode');
      this.loadedNodes.set('switch', switchNode);

      // Code Node - Execute custom JavaScript
      const codeNode = new CodeNode();
      NodeRegistry.registerNodeType(codeNode, 'CodeNode');
      this.loadedNodes.set('code', codeNode);

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
