// Internal Terminal MCP Provider - Initialization & Barrel Export

import { terminalMCPProvider } from './TerminalMCPProvider';
import { navigationTools } from './tools/navigation';
import { tradingTools } from './tools/trading';
import { portfolioTools } from './tools/portfolio';
import { dashboardTools } from './tools/dashboard';
import { settingsTools } from './tools/settings';
import { workspaceTools } from './tools/workspace';
import { backtestingTools } from './tools/backtesting';
import { marketDataTools } from './tools/market-data';
import { workflowTools } from './tools/workflow';
import { notesTools } from './tools/notes';
import { cryptoTradingTools } from './tools/crypto-trading';
import { stockTradingTools } from './tools/stock-trading';
import { reportBuilderTools } from './tools/report-builder';
import { edgarTools } from './tools/edgar';
import { nodeEditorTools } from './tools/node-editor';
import { reportBuilderMCPBridge } from './ReportBuilderMCPBridge';
import { brokerMCPBridge } from './BrokerMCPBridge';
import { stockBrokerMCPBridge } from './StockBrokerMCPBridge';
import { notesMCPBridge } from './NotesMCPBridge';
import { nodeEditorMCPBridge } from './NodeEditorMCPBridge';

// Flag to ensure initialization only runs once
let isInitialized = false;

/**
 * Initialize all internal MCP tools and bridges
 * This must be called once during app startup
 */
export function initializeInternalMCP(): void {
  if (isInitialized) {
    console.warn('[Internal MCP] Already initialized, skipping...');
    return;
  }

  console.log('[Internal MCP] Initializing...');

  try {
    // Register all tool modules
    terminalMCPProvider.registerTools(navigationTools);
    terminalMCPProvider.registerTools(tradingTools);
    terminalMCPProvider.registerTools(portfolioTools);
    terminalMCPProvider.registerTools(dashboardTools);
    terminalMCPProvider.registerTools(settingsTools);
    terminalMCPProvider.registerTools(workspaceTools);
    terminalMCPProvider.registerTools(backtestingTools);
    terminalMCPProvider.registerTools(marketDataTools);
    terminalMCPProvider.registerTools(workflowTools);
    terminalMCPProvider.registerTools(notesTools);
    terminalMCPProvider.registerTools(cryptoTradingTools);
    terminalMCPProvider.registerTools(stockTradingTools);
    terminalMCPProvider.registerTools(reportBuilderTools);
    terminalMCPProvider.registerTools(edgarTools);
    terminalMCPProvider.registerTools(nodeEditorTools);

    console.log(`[Internal MCP] Registered ${terminalMCPProvider.getToolCount()} tools`);

    // Initialize global bridge contexts
    reportBuilderMCPBridge.initializeGlobalContexts();
    console.log('[Internal MCP] Report Builder bridge initialized');

    // Initialize Node Editor bridge contexts
    nodeEditorMCPBridge.initialize();
    console.log('[Internal MCP] Node Editor bridge initialized');

    // Mark as initialized
    isInitialized = true;
    console.log('[Internal MCP] Initialization complete');
  } catch (error) {
    console.error('[Internal MCP] Initialization failed:', error);
    throw error;
  }
}

/**
 * Get initialization status
 */
export function isInternalMCPInitialized(): boolean {
  return isInitialized;
}

export { terminalMCPProvider } from './TerminalMCPProvider';
export { brokerMCPBridge } from './BrokerMCPBridge';
export { stockBrokerMCPBridge } from './StockBrokerMCPBridge';
export { notesMCPBridge } from './NotesMCPBridge';
export { reportBuilderMCPBridge } from './ReportBuilderMCPBridge';
export { nodeEditorMCPBridge } from './NodeEditorMCPBridge';
export { INTERNAL_SERVER_ID, INTERNAL_SERVER_NAME } from './types';
export type { TerminalContexts, InternalTool, InternalToolResult } from './types';
export type {
  NodeTypeInfo,
  NodeTypeDetails,
  NodeCategoryInfo,
  CreateWorkflowParams,
  WorkflowNodeInput,
  WorkflowEdgeInput,
  WorkflowInfo,
  WorkflowExecutionResult,
  WorkflowValidation,
} from './types';
