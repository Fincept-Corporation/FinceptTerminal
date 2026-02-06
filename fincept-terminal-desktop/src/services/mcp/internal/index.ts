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
import { newsTools } from './tools/news';
import { reportBuilderMCPBridge } from './ReportBuilderMCPBridge';
import { brokerMCPBridge } from './BrokerMCPBridge';
import { stockBrokerMCPBridge } from './StockBrokerMCPBridge';
import { notesMCPBridge } from './NotesMCPBridge';
import { nodeEditorMCPBridge } from './NodeEditorMCPBridge';
import { portfolioMCPBridge } from './PortfolioMCPBridge';
import { newsMCPBridge } from './NewsMCPBridge';

// Flag to ensure initialization only runs once
let isInitialized = false;

/**
 * Initialize all internal MCP tools and bridges
 * This must be called once during app startup
 */
export function initializeInternalMCP(): void {
  if (isInitialized) {
    return;
  }


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
    terminalMCPProvider.registerTools(newsTools);


    // Initialize global bridge contexts
    reportBuilderMCPBridge.initializeGlobalContexts();

    // Initialize Node Editor bridge contexts
    nodeEditorMCPBridge.initialize();

    // Initialize Portfolio bridge contexts
    portfolioMCPBridge.connect();

    // Initialize News bridge contexts
    newsMCPBridge.connect();

    // Mark as initialized
    isInitialized = true;
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
export { portfolioMCPBridge } from './PortfolioMCPBridge';
export { newsMCPBridge } from './NewsMCPBridge';
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
  // News types
  NewsArticleInfo,
  RSSFeedInfo,
  UserRSSFeedInfo,
  AddRSSFeedParams,
  UpdateRSSFeedParams,
  NewsAnalysisResult,
  NewsAnalysisData,
  NewsSentimentStats,
  NewsCacheStats,
} from './types';
