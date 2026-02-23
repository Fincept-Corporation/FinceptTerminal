// QuantLib MCP Tools - Barrel Export
// Exports all QuantLib module tools for registration with the MCP provider

// Phase 1 modules
import { quantlibCoreTools } from './core';
import { quantlibPricingTools } from './pricing';
import { quantlibRiskTools } from './risk';
import { quantlibPortfolioTools } from './portfolio';

// Phase 2 modules
import { quantlibCurvesTools } from './curves';
import { quantlibVolatilityTools } from './volatility';
import { quantlibModelsTools } from './models';
import { quantlibStatisticsTools } from './statistics';
import { quantlibInstrumentsTools } from './instruments';

// Re-export individual modules for selective imports
// Phase 1
export { quantlibCoreTools } from './core';
export { quantlibPricingTools } from './pricing';
export { quantlibRiskTools } from './risk';
export { quantlibPortfolioTools } from './portfolio';

// Phase 2
export { quantlibCurvesTools } from './curves';
export { quantlibVolatilityTools } from './volatility';
export { quantlibModelsTools } from './models';
export { quantlibStatisticsTools } from './statistics';
export { quantlibInstrumentsTools } from './instruments';

// Combined export of all QuantLib tools
// Phase 1: Core, Pricing, Risk, Portfolio (120 tools)
// Phase 2: Curves, Volatility, Models, Statistics, Instruments (137 tools)
// Total: 257 tools
export const allQuantLibTools = [
  // Phase 1 (120 tools)
  ...quantlibCoreTools,        // 51 tools
  ...quantlibPricingTools,     // 29 tools
  ...quantlibRiskTools,        // 25 tools
  ...quantlibPortfolioTools,   // 15 tools
  // Phase 2 (137 tools)
  ...quantlibCurvesTools,      // 31 tools
  ...quantlibVolatilityTools,  // 14 tools
  ...quantlibModelsTools,      // 14 tools
  ...quantlibStatisticsTools,  // 52 tools
  ...quantlibInstrumentsTools, // 26 tools
];

// Tool counts for reference
export const QUANTLIB_TOOL_COUNTS = {
  // Phase 1
  core: quantlibCoreTools.length,
  pricing: quantlibPricingTools.length,
  risk: quantlibRiskTools.length,
  portfolio: quantlibPortfolioTools.length,
  // Phase 2
  curves: quantlibCurvesTools.length,
  volatility: quantlibVolatilityTools.length,
  models: quantlibModelsTools.length,
  statistics: quantlibStatisticsTools.length,
  instruments: quantlibInstrumentsTools.length,
  // Totals
  phase1Total: quantlibCoreTools.length + quantlibPricingTools.length + quantlibRiskTools.length + quantlibPortfolioTools.length,
  phase2Total: quantlibCurvesTools.length + quantlibVolatilityTools.length + quantlibModelsTools.length + quantlibStatisticsTools.length + quantlibInstrumentsTools.length,
  total: allQuantLibTools.length,
};

// Future modules (to be implemented):
// - stochastic.ts (36 tools) - GBM, Ornstein-Uhlenbeck, Monte Carlo
// - ml.ts (48 tools) - Credit scoring, regression, clustering
// - numerical.ts (28 tools) - Differentiation, FFT, interpolation
// - analysis.ts (122 tools) - Fundamentals, ratios, valuation
// - economics.ts (25 tools) - Equilibrium, game theory
// - solver.ts (25 tools) - Bond yields, implied vol
// - physics.ts (24 tools) - Entropy, thermodynamics
// - scheduling.ts (14 tools) - Calendars, day counts
// - regulatory.ts (11 tools) - Basel III, IFRS 9
