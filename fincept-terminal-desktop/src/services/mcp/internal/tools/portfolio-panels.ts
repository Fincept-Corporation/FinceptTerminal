// Portfolio Panel MCP Tools — COMPLETE COVERAGE
// Matches every invoke() call and service method used by the 9 panel views + FFN view.
//
// Panel → MCP tool mapping:
//   AnalyticsSectorsPanel    → get_portfolio_sectors_analytics
//   PerformanceRiskPanel     → get_portfolio_performance_risk
//   PortfolioOptimizationView→ get_portfolio_optimization  (skfolio + pyportfolioopt full suite)
//   QuantStatsView           → get_portfolio_quantstats    (run_quantstats_analysis + fortitudo)
//   ReportsPanel             → get_portfolio_reports       (pypme * 6 + active management)
//   CustomIndexView          → get_portfolio_custom_indices
//   RiskManagementPanel      → get_portfolio_risk_management (all 11 fortitudo commands)
//   PlanningPanel            → get_portfolio_planning      (all 8 commands)
//   EconomicsPanel           → get_portfolio_economics_overlay (correct command names)
//   FFNView                  → get_portfolio_ffn_analysis  (all 7 commands)
//   Bonus                    → get_portfolio_full_context

import { InternalTool } from '../types';
import { portfolioPanelTools1 } from './portfolioPanelTools1';
import { portfolioPanelTools2 } from './portfolioPanelTools2';

export const portfolioPanelTools: InternalTool[] = [
  ...portfolioPanelTools1,
  ...portfolioPanelTools2,
];
