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
import { finscriptDocsTools } from './tools/finscript-docs';
import { dataMappingTools } from './tools/data-mapping';
import { customIndexTools } from './tools/custom-index';
import { economicsTools } from './tools/economics';
import { portfolioPanelTools } from './tools/portfolio-panels';
import { dataMappingMCPBridge } from './DataMappingMCPBridge';
import { customIndexMCPBridge } from './CustomIndexMCPBridge';
import { economicsMCPBridge } from './EconomicsMCPBridge';
import { reportBuilderMCPBridge } from './ReportBuilderMCPBridge';
import { brokerMCPBridge } from './BrokerMCPBridge';
import { stockBrokerMCPBridge } from './StockBrokerMCPBridge';
import { notesMCPBridge } from './NotesMCPBridge';
import { nodeEditorMCPBridge } from './NodeEditorMCPBridge';
import { portfolioMCPBridge } from './PortfolioMCPBridge';
import { newsMCPBridge } from './NewsMCPBridge';
import { allQuantLibTools, QUANTLIB_TOOL_COUNTS } from './tools/quantlib';
import { quantLibMCPBridge } from './QuantLibMCPBridge';

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
    terminalMCPProvider.registerTools(finscriptDocsTools);
    terminalMCPProvider.registerTools(dataMappingTools);
    terminalMCPProvider.registerTools(customIndexTools);
    terminalMCPProvider.registerTools(economicsTools);
    terminalMCPProvider.registerTools(portfolioPanelTools);

    // Register QuantLib tools (Phase 1: 120 tools)
    terminalMCPProvider.registerTools(allQuantLibTools);

    // Initialize global bridge contexts
    reportBuilderMCPBridge.initializeGlobalContexts();

    // Initialize Node Editor bridge contexts
    nodeEditorMCPBridge.initialize();

    // Initialize Portfolio bridge contexts
    portfolioMCPBridge.connect();

    // Initialize News bridge contexts
    newsMCPBridge.connect();

    // Initialize Data Mapping bridge contexts
    dataMappingMCPBridge.connect();

    // Initialize Custom Index bridge contexts
    customIndexMCPBridge.connect();

    // Initialize Economics bridge contexts (Fincept Macro API)
    economicsMCPBridge.connect();

    // Initialize QuantLib bridge contexts
    quantLibMCPBridge.connect();

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
export { dataMappingMCPBridge } from './DataMappingMCPBridge';
export { customIndexMCPBridge } from './CustomIndexMCPBridge';
export { economicsMCPBridge } from './EconomicsMCPBridge';
export { quantLibMCPBridge } from './QuantLibMCPBridge';
export { allQuantLibTools, QUANTLIB_TOOL_COUNTS } from './tools/quantlib';
export { INTERNAL_SERVER_ID, INTERNAL_SERVER_NAME } from './types';
export type { TerminalContexts, InternalTool, InternalToolResult } from './types';
export type {
  DataMappingSummary,
  DataMappingDetail,
  CreateDataMappingParams,
  DataMappingExecutionResult,
  DataMappingTemplateSummary,
  DataMappingSchemaSummary,
  // Node Editor types
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
