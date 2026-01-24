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

export { terminalMCPProvider } from './TerminalMCPProvider';
export { INTERNAL_SERVER_ID, INTERNAL_SERVER_NAME } from './types';
export type { TerminalContexts, InternalTool, InternalToolResult } from './types';
