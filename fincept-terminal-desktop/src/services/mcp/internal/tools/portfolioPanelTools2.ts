// Portfolio Panel MCP Tools — Part 2 (tools 6–11)
// Panel coverage:
//   6. get_portfolio_custom_indices      → CustomIndexView
//   7. get_portfolio_risk_management     → RiskManagementPanel
//   8. get_portfolio_planning            → PlanningPanel
//   9. get_portfolio_economics_overlay   → EconomicsPanel
//  10. get_portfolio_ffn_analysis        → FFNView
//  11. get_portfolio_full_context        → single-call snapshot

import { InternalTool } from '../types';
import { si, safeParse, toWeights, toReturns, toPnlReturns } from './portfolioPanelHelpers';

export const portfolioPanelTools2: InternalTool[] = [

  // ═══════════════════════════════════════════════════════════════════════════
  // 6. INDICES — CustomIndexView  (unchanged — already correct)
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_custom_indices',
    description:
      'Get all custom indices with constituents, current value, change vs base, '
      + 'calculation method (price-weighted, market-cap, equal, fundamental, custom), '
      + 'and optional historical snapshots. This is the INDICES panel.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id:      { type: 'string', description: 'Portfolio ID' },
        include_snapshots: { type: 'string', description: 'Include historical snapshots', enum: ['true','false'], default: 'false' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getAllCustomIndices || !contexts.getIndexConstituents)
        return { success: false, error: 'Custom index context not available' };

      const allIndices = await contexts.getAllCustomIndices();
      const indices = allIndices.filter((idx: any) => !idx.portfolio_id || idx.portfolio_id === args.portfolio_id);
      const wantSnaps = args.include_snapshots === 'true';

      const enriched = (await Promise.allSettled(
        indices.map(async (idx: any) => {
          const [consRes, sumRes, snapRes] = await Promise.allSettled([
            contexts.getIndexConstituents!(idx.id),
            contexts.getCustomIndexSummary ? contexts.getCustomIndexSummary(idx.id) : Promise.resolve(null),
            wantSnaps && contexts.getIndexSnapshots ? contexts.getIndexSnapshots(idx.id, 30) : Promise.resolve([]),
          ]);
          return {
            id: idx.id, name: idx.name, description: idx.description,
            base_value: idx.base_value, calculation_method: idx.calculation_method,
            created_at: idx.created_at,
            constituents: consRes.status === 'fulfilled' ? consRes.value : [],
            summary:      sumRes.status  === 'fulfilled' ? sumRes.value  : null,
            snapshots:    snapRes.status === 'fulfilled' ? snapRes.value  : [],
          };
        })
      )).filter(r => r.status === 'fulfilled').map((r: any) => r.value);

      let message = `=== CUSTOM INDICES (${enriched.length}) ===\n\n`;
      for (const idx of enriched) {
        const cur = idx.summary?.current_value ?? idx.base_value ?? 100;
        const base = idx.base_value ?? 100;
        const chg = base > 0 ? (cur - base) / base * 100 : 0;
        message += `${idx.name} [${idx.calculation_method ?? 'equal'}]:\n`;
        message += `  Base ${base} → Current ${cur?.toFixed(2)} (${chg >= 0 ? '+' : ''}${chg.toFixed(2)}%)\n`;
        message += `  Constituents: ${(idx.constituents as any[]).map((c: any) => c.symbol).join(', ') || 'none'}\n\n`;
      }

      return { success: true, data: { indices: enriched, count: enriched.length }, message };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 7. RISK — RiskManagementPanel
  //    Sub-tabs: RISK OVERVIEW | STRESS & DECAY | FORTITUDO ADVANCED | EFFICIENT FRONTIERS
  //    invoke(): comprehensive_risk_analysis, calculate_risk_metrics,
  //              fortitudo_full_analysis, fortitudo_exp_decay_weighting,
  //              fortitudo_covariance_analysis, fortitudo_entropy_pooling,
  //              fortitudo_exposure_stacking, fortitudo_efficient_frontier_mv,
  //              fortitudo_efficient_frontier_cvar, fortitudo_optimize_mean_variance,
  //              fortitudo_optimize_mean_cvar
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_risk_management',
    description:
      'Run comprehensive risk management analysis covering all four sub-tabs of the RISK panel: '
      + '(1) RISK OVERVIEW — comprehensive_risk_analysis, calculate_risk_metrics, fortitudo_full_analysis; '
      + '(2) STRESS & DECAY — fortitudo_exp_decay_weighting, fortitudo_covariance_analysis; '
      + '(3) FORTITUDO ADVANCED — fortitudo_entropy_pooling, fortitudo_exposure_stacking; '
      + '(4) EFFICIENT FRONTIERS — MV + CVaR frontiers, min-variance + min-CVaR optimal portfolios.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id:     { type: 'string', description: 'Portfolio ID' },
        alpha:            { type: 'number', description: 'CVaR/VaR significance level', default: 0.05 },
        n_frontier_points:{ type: 'number', description: 'Points on efficient frontier', default: 20 },
        n_scenarios:      { type: 'number', description: 'Entropy pooling scenarios', default: 1000 },
        half_life:        { type: 'number', description: 'Exponential decay half-life (days)', default: 252 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const alpha    = args.alpha             ?? 0.05;
      const nPoints  = args.n_frontier_points ?? 20;
      const nScenarios = args.n_scenarios     ?? 1000;
      const halfLife = args.half_life         ?? 252;

      const [summaryRes, riskSvcRes, advMetricsRes] = await Promise.allSettled([
        contexts.getPortfolioSummary(args.portfolio_id),
        contexts.calculateRiskMetrics      ? contexts.calculateRiskMetrics(args.portfolio_id)       : Promise.resolve(null),
        contexts.calculateAdvancedMetrics  ? contexts.calculateAdvancedMetrics(args.portfolio_id)   : Promise.resolve(null),
      ]);

      const summary    = summaryRes.status   === 'fulfilled' ? summaryRes.value    : null;
      const svcRisk    = riskSvcRes.status   === 'fulfilled' ? riskSvcRes.value    : null;
      const advMetrics = advMetricsRes.status === 'fulfilled' ? advMetricsRes.value : null;

      const holdings: any[] = summary?.holdings ?? [];
      if (holdings.length === 0) return { success: true, data: {}, message: 'Portfolio has no holdings.' };

      const returnsJson = JSON.stringify(toReturns(holdings));
      const weightsJson = JSON.stringify(toWeights(holdings));
      const totalValue  = summary?.total_market_value ?? 0;

      // All 9 Fortitudo commands + 2 comprehensive risk commands — exact names from the panel
      const [
        comprehensiveRes, riskMetricsRes,
        fullAnalysisRes, expDecayRes, covarianceRes,
        entropyRes, exposureRes,
        frontierMvRes, frontierCvarRes,
        optimizeMvRes, optimizeCvarRes,
      ] = await Promise.allSettled([
        si<string>('comprehensive_risk_analysis',       { returns_data: returnsJson, weights: weightsJson, portfolio_value: totalValue }),
        si<string>('calculate_risk_metrics',            { returns_data: returnsJson, weights: weightsJson }),
        si<string>('fortitudo_full_analysis',           { returns_json: returnsJson, weights_json: weightsJson, alpha }),
        si<string>('fortitudo_exp_decay_weighting',     { returns_json: returnsJson, half_life: halfLife }),
        si<string>('fortitudo_covariance_analysis',     { returns_json: returnsJson }),
        si<string>('fortitudo_entropy_pooling',         { n_scenarios: nScenarios }),
        si<string>('fortitudo_exposure_stacking',       { sample_portfolios_json: weightsJson }),
        si<string>('fortitudo_efficient_frontier_mv',   { returns_json: returnsJson, n_points: nPoints }),
        si<string>('fortitudo_efficient_frontier_cvar', { returns_json: returnsJson, n_points: nPoints, alpha }),
        si<string>('fortitudo_optimize_mean_variance',  { returns_json: returnsJson, objective: 'min_variance' }),
        si<string>('fortitudo_optimize_mean_cvar',      { returns_json: returnsJson, objective: 'min_cvar', alpha }),
      ]);

      // Concentration metrics
      const sorted3 = [...holdings].sort((a: any, b: any) => (b.weight ?? 0) - (a.weight ?? 0));
      const top3 = sorted3.slice(0, 3).reduce((s: number, h: any) => s + (h.weight ?? 0), 0);
      const hhi  = holdings.reduce((s: number, h: any) => s + ((h.weight ?? 0) / 100) ** 2, 0);

      // Parametric stress scenarios
      const stressScenarios = [
        { name: 'Market Crash (-30%)',     shock: -0.30 },
        { name: 'Correction (-15%)',       shock: -0.15 },
        { name: 'Mild Pullback (-5%)',     shock: -0.05 },
        { name: 'Moderate Rally (+10%)',   shock:  0.10 },
        { name: 'Bull Run (+20%)',         shock:  0.20 },
      ].map(s => ({ ...s, impact: totalValue * s.shock, value_after: totalValue * (1 + s.shock) }));

      let message = `=== RISK MANAGEMENT ===\nValue: $${totalValue.toFixed(2)} | Positions: ${holdings.length}\n\n`;

      if (advMetrics) {
        message += `OVERVIEW:\n`;
        message += `  VaR (95%):     ${(advMetrics.value_at_risk_95 * 100)?.toFixed(2)}% daily\n`;
        message += `  Max Drawdown:  ${(advMetrics.max_drawdown * 100)?.toFixed(2)}%\n`;
        message += `  Volatility:    ${(advMetrics.portfolio_volatility * 100)?.toFixed(2)}% ann.\n`;
        message += `  Sharpe:        ${advMetrics.sharpe_ratio?.toFixed(3)}\n\n`;
      }
      message += `CONCENTRATION:\n  Top-3: ${top3.toFixed(1)}% | HHI: ${hhi.toFixed(3)} — `;
      message += hhi > 0.18 ? 'HIGH risk\n\n' : hhi > 0.10 ? 'MODERATE\n\n' : 'Well diversified\n\n';
      message += `STRESS SCENARIOS:\n`;
      for (const s of stressScenarios)
        message += `  ${s.name}: ${s.impact >= 0 ? '+' : ''}$${s.impact.toFixed(2)}\n`;

      return {
        success: true,
        data: {
          portfolio_value: totalValue,
          concentration: { top3_weight_pct: top3, hhi },
          stress_scenarios: stressScenarios,
          service_risk_metrics: svcRisk,
          advanced_metrics: advMetrics,
          // Sub-tab: RISK OVERVIEW
          comprehensive_risk:     comprehensiveRes.status   === 'fulfilled' ? safeParse(comprehensiveRes.value)   : null,
          risk_metrics_python:    riskMetricsRes.status     === 'fulfilled' ? safeParse(riskMetricsRes.value)     : null,
          fortitudo_full:         fullAnalysisRes.status    === 'fulfilled' ? safeParse(fullAnalysisRes.value)    : null,
          // Sub-tab: STRESS & DECAY
          exp_decay_weighting:    expDecayRes.status        === 'fulfilled' ? safeParse(expDecayRes.value)        : null,
          covariance_analysis:    covarianceRes.status      === 'fulfilled' ? safeParse(covarianceRes.value)      : null,
          // Sub-tab: FORTITUDO ADVANCED
          entropy_pooling:        entropyRes.status         === 'fulfilled' ? safeParse(entropyRes.value)         : null,
          exposure_stacking:      exposureRes.status        === 'fulfilled' ? safeParse(exposureRes.value)        : null,
          // Sub-tab: EFFICIENT FRONTIERS
          frontier_mv:            frontierMvRes.status      === 'fulfilled' ? safeParse(frontierMvRes.value)      : null,
          frontier_cvar:          frontierCvarRes.status    === 'fulfilled' ? safeParse(frontierCvarRes.value)    : null,
          optimal_min_variance:   optimizeMvRes.status      === 'fulfilled' ? safeParse(optimizeMvRes.value)      : null,
          optimal_min_cvar:       optimizeCvarRes.status    === 'fulfilled' ? safeParse(optimizeCvarRes.value)    : null,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 8. PLANNING — PlanningPanel
  //    Sub-tabs: ASSET ALLOCATION | RETIREMENT | BEHAVIORAL | ETF ANALYSIS
  //    invoke(): generate_asset_allocation, strategic_asset_allocation,
  //              calculate_retirement_plan, calculate_retirement_planning,
  //              analyze_behavioral_biases, analyze_behavioral_finance,
  //              analyze_etf_costs, analyze_etf
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_planning',
    description:
      'Generate all four planning sub-tab results: '
      + '(1) ASSET ALLOCATION — generate_asset_allocation + strategic_asset_allocation by age/risk; '
      + '(2) RETIREMENT — calculate_retirement_plan + calculate_retirement_planning projections; '
      + '(3) BEHAVIORAL — analyze_behavioral_biases + analyze_behavioral_finance; '
      + '(4) ETF ANALYSIS — analyze_etf_costs + analyze_etf for current holdings. '
      + 'This is the PLANNING panel of the Portfolio tab.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id:        { type: 'string', description: 'Portfolio ID' },
        investor_age:        { type: 'number', description: 'Investor age', default: 35 },
        retirement_age:      { type: 'number', description: 'Target retirement age', default: 65 },
        risk_tolerance:      { type: 'string', description: 'Risk tolerance level', enum: ['conservative','moderate','aggressive'], default: 'moderate' },
        current_savings:     { type: 'number', description: 'Current total savings (default: portfolio value)', default: 0 },
        annual_contribution: { type: 'number', description: 'Annual contribution', default: 12000 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      const holdings: any[] = summary?.holdings ?? [];
      const totalValue = summary?.total_market_value ?? 0;

      const age          = args.investor_age        ?? 35;
      const retAge       = args.retirement_age       ?? 65;
      const riskTol      = args.risk_tolerance       ?? 'moderate';
      const savings      = args.current_savings > 0  ? args.current_savings : totalValue;
      const annualContrib= args.annual_contribution  ?? 12000;
      const yearsToRet   = Math.max(0, retAge - age);

      const holdingsJson     = JSON.stringify(holdings.map((h: any) => ({ symbol: h.symbol, weight: h.weight, sector: h.sector })));
      const portfolioDataJson = JSON.stringify({
        holdings: holdings.map((h: any) => ({
          symbol: h.symbol, weight: h.weight,
          unrealized_pnl_percent: h.unrealized_pnl_percent,
          day_change_percent: h.day_change_percent,
        })),
      });
      const investorDataJson = JSON.stringify({ age, risk_tolerance: riskTol, years_to_retirement: yearsToRet });
      const etfSymbols = holdings.map((h: any) => h.symbol);
      // Approximate expense ratios — ETFs typically 0.03%-0.5%, stocks 0%
      const expenseRatios = JSON.stringify(etfSymbols.reduce((acc: any, sym: string) => {
        acc[sym] = sym.length <= 4 ? 0.0003 : 0;   // rough heuristic
        return acc;
      }, {} as Record<string, number>));

      // All 8 planning commands in parallel — exact command names from PlanningPanel.tsx
      const [
        allocRes, strategicRes,
        retirePlanRes, retirePlanningRes,
        biasesRes, behavFinRes,
        etfCostsRes, etfAnalysisRes,
      ] = await Promise.allSettled([
        si<string>('generate_asset_allocation',   { age, risk_tolerance: riskTol, years_to_retirement: yearsToRet }),
        si<string>('strategic_asset_allocation',  { age, risk_tolerance: riskTol, time_horizon: yearsToRet }),
        si<string>('calculate_retirement_plan',   { current_age: age, retirement_age: retAge, current_savings: savings, annual_contribution: annualContrib }),
        si<string>('calculate_retirement_planning',{ current_age: age, retirement_age: retAge, current_savings: savings, annual_contribution: annualContrib }),
        si<string>('analyze_behavioral_biases',   { portfolio_data: portfolioDataJson }),
        si<string>('analyze_behavioral_finance',  { investor_data: investorDataJson }),
        si<string>('analyze_etf_costs',           { symbols: JSON.stringify(etfSymbols), expense_ratios: expenseRatios }),
        si<string>('analyze_etf',                 { etf_data: JSON.stringify({ symbols: etfSymbols, holdings: holdingsJson }) }),
      ]);

      // Client-side fallback retirement projection
      const assumedRet = riskTol === 'aggressive' ? 0.09 : riskTol === 'conservative' ? 0.05 : 0.07;
      const fvPortfolio = savings * Math.pow(1 + assumedRet, yearsToRet);
      const fvContribs  = yearsToRet > 0 ? annualContrib * ((Math.pow(1 + assumedRet, yearsToRet) - 1) / assumedRet) : 0;
      const projectedValue = fvPortfolio + fvContribs;

      let message = `=== PLANNING ===\n`;
      message += `Age ${age} → Retire at ${retAge} (${yearsToRet}y) | Risk: ${riskTol} | Savings: $${savings.toFixed(0)}\n\n`;
      message += `RETIREMENT PROJECTION (${(assumedRet*100).toFixed(0)}% assumed return):\n`;
      message += `  Current: $${savings.toFixed(2)} + $${annualContrib}/yr → Projected: $${projectedValue.toFixed(2)}\n\n`;

      const alloc = allocRes.status === 'fulfilled' ? safeParse(allocRes.value) : null;
      if (alloc) message += `ASSET ALLOCATION: ${JSON.stringify(alloc)}\n\n`;

      const biases = biasesRes.status === 'fulfilled' ? safeParse(biasesRes.value) : null;
      if (biases) message += `BEHAVIORAL BIASES: ${JSON.stringify(biases)}\n\n`;

      return {
        success: true,
        data: {
          investor_profile: { age, retirement_age: retAge, risk_tolerance: riskTol, years_to_retirement: yearsToRet },
          portfolio_value: totalValue,
          // Sub-tab: ASSET ALLOCATION
          asset_allocation:         allocRes.status    === 'fulfilled' ? safeParse(allocRes.value)         : null,
          strategic_allocation:     strategicRes.status === 'fulfilled' ? safeParse(strategicRes.value)    : null,
          // Sub-tab: RETIREMENT
          retirement_plan:          retirePlanRes.status     === 'fulfilled' ? safeParse(retirePlanRes.value)     : null,
          retirement_planning:      retirePlanningRes.status === 'fulfilled' ? safeParse(retirePlanningRes.value) : null,
          retirement_projection_fallback: { projected_value: projectedValue, assumed_return: assumedRet, annual_contribution: annualContrib },
          // Sub-tab: BEHAVIORAL
          behavioral_biases:        biasesRes.status  === 'fulfilled' ? safeParse(biasesRes.value)  : null,
          behavioral_finance:       behavFinRes.status === 'fulfilled' ? safeParse(behavFinRes.value) : null,
          // Sub-tab: ETF ANALYSIS
          etf_costs:                etfCostsRes.status   === 'fulfilled' ? safeParse(etfCostsRes.value)    : null,
          etf_analysis:             etfAnalysisRes.status === 'fulfilled' ? safeParse(etfAnalysisRes.value) : null,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 9. ECONOMICS — EconomicsPanel  (CORRECTED command names)
  //    Sub-tabs: BUSINESS CYCLE | EQUITY RISK PREMIUM | PORTFOLIO OVERVIEW | ACTIVE MANAGEMENT
  //    invoke(): analyze_business_cycle, comprehensive_economics_analysis,
  //              analyze_equity_risk_premium, get_portfolio_overview,
  //              analyze_active_management, analyze_manager_selection
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_economics_overlay',
    description:
      'Overlay macro context on the portfolio across all four sub-tabs of the ECONOMICS panel: '
      + '(1) BUSINESS CYCLE — analyze_business_cycle + comprehensive_economics_analysis; '
      + '(2) EQUITY RISK PREMIUM — analyze_equity_risk_premium with risk-free rate + market premium; '
      + '(3) PORTFOLIO OVERVIEW — get_portfolio_overview with returns/weights; '
      + '(4) ACTIVE MANAGEMENT — analyze_active_management + analyze_manager_selection. '
      + 'Also computes cyclical vs defensive sector exposure.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id:        { type: 'string', description: 'Portfolio ID' },
        cycle_phase:         { type: 'string', description: 'Business cycle phase', enum: ['expansion','peak','contraction','trough','recovery'], default: 'expansion' },
        risk_free_rate:      { type: 'number', description: 'Risk-free rate for ERP (default: 0.04)', default: 0.04 },
        market_risk_premium: { type: 'number', description: 'Market risk premium (default: 0.055)', default: 0.055 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const cyclePhase = args.cycle_phase        ?? 'expansion';
      const rfRate     = args.risk_free_rate      ?? 0.04;
      const mrp        = args.market_risk_premium ?? 0.055;

      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      const holdings: any[] = summary?.holdings ?? [];
      const returnsJson = JSON.stringify(toReturns(holdings));
      const weightsJson = JSON.stringify(toWeights(holdings));

      const fundData = JSON.stringify({ returns: toReturns(holdings), weights: toWeights(holdings) });
      const managerCandidates = JSON.stringify([{ name: 'Current Portfolio', returns: toReturns(holdings) }]);

      // All 6 commands — exact names from EconomicsPanel.tsx
      const [cycleRes, ecoRes, erpRes, overviewRes, activeMgmtRes, managerSelRes] = await Promise.allSettled([
        si<string>('analyze_business_cycle',          { cycle_phase: cyclePhase }),
        si<string>('comprehensive_economics_analysis', { cycle_phase: cyclePhase }),
        si<string>('analyze_equity_risk_premium',     { risk_free_rate: rfRate, market_risk_premium: mrp }),
        si<string>('get_portfolio_overview',          { returns_data: returnsJson, weights: weightsJson }),
        si<string>('analyze_active_management',       { fund_data: fundData }),
        si<string>('analyze_manager_selection',       { manager_candidates: managerCandidates }),
      ]);

      // Cyclical/defensive split
      const CYCLICAL  = ['Technology','Consumer Cyclical','Financial Services','Industrials','Basic Materials','Energy','Communication Services'];
      const DEFENSIVE = ['Healthcare','Consumer Defensive','Utilities','Real Estate'];
      const sectorWeights: Record<string, number> = {};
      for (const h of holdings) {
        const s = (h.sector as string | undefined) ?? 'Unknown';
        sectorWeights[s] = (sectorWeights[s] ?? 0) + (h.weight ?? 0);
      }
      const cyclical  = Object.entries(sectorWeights).filter(([s]) => CYCLICAL.some(c => s.includes(c))).reduce((n, [,w]) => n + w, 0);
      const defensive = Object.entries(sectorWeights).filter(([s]) => DEFENSIVE.some(d => s.includes(d))).reduce((n, [,w]) => n + w, 0);

      let message = `=== ECONOMICS OVERLAY ===\nPhase: ${cyclePhase.toUpperCase()} | RF: ${(rfRate*100).toFixed(1)}% | MRP: ${(mrp*100).toFixed(1)}%\n\n`;
      message += `SECTOR POSITIONING:\n  Cyclical: ${cyclical.toFixed(1)}% | Defensive: ${defensive.toFixed(1)}% | Other: ${Math.max(0,100-cyclical-defensive).toFixed(1)}%\n\n`;

      const cycle = cycleRes.status === 'fulfilled' ? safeParse(cycleRes.value) : null;
      const erp   = erpRes.status   === 'fulfilled' ? safeParse(erpRes.value)   : null;
      if (cycle) message += `BUSINESS CYCLE: ${JSON.stringify(cycle)}\n\n`;
      if (erp)   message += `EQUITY RISK PREMIUM: ${JSON.stringify(erp)}\n\n`;

      return {
        success: true,
        data: {
          cycle_phase: cyclePhase, risk_free_rate: rfRate, market_risk_premium: mrp,
          portfolio_value: summary?.total_market_value,
          sector_weights: sectorWeights,
          macro_positioning: { cyclical_pct: cyclical, defensive_pct: defensive, other_pct: Math.max(0, 100 - cyclical - defensive) },
          // Sub-tab: BUSINESS CYCLE
          business_cycle:           cycleRes.status    === 'fulfilled' ? safeParse(cycleRes.value)    : null,
          economics_analysis:       ecoRes.status      === 'fulfilled' ? safeParse(ecoRes.value)      : null,
          // Sub-tab: EQUITY RISK PREMIUM
          equity_risk_premium:      erpRes.status      === 'fulfilled' ? safeParse(erpRes.value)      : null,
          // Sub-tab: PORTFOLIO OVERVIEW
          portfolio_overview:       overviewRes.status === 'fulfilled' ? safeParse(overviewRes.value) : null,
          // Sub-tab: ACTIVE MANAGEMENT
          active_management:        activeMgmtRes.status  === 'fulfilled' ? safeParse(activeMgmtRes.value)  : null,
          manager_selection:        managerSelRes.status  === 'fulfilled' ? safeParse(managerSelRes.value)  : null,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 10. FFN VIEW — FFNView (top-level view, toggled by FFN button)
  //     invoke(): execute_yfinance_command, ffn_full_analysis,
  //               ffn_benchmark_comparison, ffn_portfolio_optimization,
  //               ffn_rebase_prices, ffn_calculate_drawdowns,
  //               ffn_calculate_rolling_metrics
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_ffn_analysis',
    description:
      'Run FFN (Financial Functions) analysis on the portfolio. Returns all six sections: '
      + '(1) FULL ANALYSIS — ffn_full_analysis: per-asset metrics (CAGR, Sharpe, Sortino, MaxDD, Vol, Calmar); '
      + '(2) BENCHMARK COMPARISON — ffn_benchmark_comparison vs configurable benchmark; '
      + '(3) OPTIMIZATION — ffn_portfolio_optimization with ERC (equal risk contribution); '
      + '(4) REBASED PRICES — ffn_rebase_prices normalised to base 100; '
      + '(5) DRAWDOWNS — ffn_calculate_drawdowns: top periods with start/end/magnitude; '
      + '(6) ROLLING METRICS — ffn_calculate_rolling_metrics (30-day Sharpe/Vol/Sortino). '
      + 'This is the FFN view triggered by the FFN button in the Portfolio tab.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id:  { type: 'string', description: 'Portfolio ID' },
        benchmark:     { type: 'string', description: 'Benchmark ticker', default: 'SPY' },
        rolling_window:{ type: 'number', description: 'Rolling metrics window (days)', default: 30 },
        lookback_days: { type: 'number', description: 'Historical price lookback (days)', default: 365 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const benchmark    = args.benchmark      ?? 'SPY';
      const rollingWin   = args.rolling_window  ?? 30;
      const lookbackDays = args.lookback_days   ?? 365;

      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      const holdings: any[] = summary?.holdings ?? [];
      if (holdings.length === 0) return { success: true, data: {}, message: 'Portfolio has no holdings.' };

      const symbols  = holdings.map((h: any) => h.symbol);
      const endDate  = new Date();
      const startDate= new Date(endDate.getTime() - lookbackDays * 24 * 60 * 60 * 1000);
      const fmt = (d: Date) => d.toISOString().split('T')[0];

      // Fetch historical prices for all holdings + benchmark — same as FFNView does
      const priceResults = await Promise.allSettled(
        [...symbols, benchmark].map(sym =>
          si<string>('execute_yfinance_command', { command: 'historical', args: [sym, fmt(startDate), fmt(endDate), '1d'] })
        )
      );

      // Build prices map: { AAPL: { '2024-01-02': 185.0, ... }, ... }
      const pricesMap: Record<string, Record<string, number>> = {};
      [...symbols, benchmark].forEach((sym, i) => {
        const r = priceResults[i];
        if (r.status === 'fulfilled' && r.value) {
          const data: any[] = safeParse(r.value) ?? [];
          pricesMap[sym] = {};
          for (const pt of data) {
            const date = pt.date ?? pt.timestamp ?? '';
            if (date) pricesMap[sym][date] = pt.close ?? pt.price ?? 0;
          }
        }
      });

      const pricesJson          = JSON.stringify(pricesMap);
      const benchmarkPricesJson = JSON.stringify({ [benchmark]: pricesMap[benchmark] ?? {} });

      // Run all 6 FFN commands in parallel — exact names from FFNView.tsx
      const [fullRes, benchRes, optRes, rebaseRes, ddRes, rollingRes] = await Promise.allSettled([
        si<string>('ffn_full_analysis',             { prices_json: pricesJson }),
        si<string>('ffn_benchmark_comparison',      { prices_json: pricesJson, benchmark_prices_json: benchmarkPricesJson, benchmark_name: benchmark }),
        si<string>('ffn_portfolio_optimization',    { prices_json: pricesJson, method: 'erc' }),
        si<string>('ffn_rebase_prices',             { prices_json: pricesJson, base_value: 100.0 }),
        si<string>('ffn_calculate_drawdowns',       { prices_json: pricesJson }),
        si<string>('ffn_calculate_rolling_metrics', { prices_json: pricesJson, window: rollingWin }),
      ]);

      const fullAnalysis = fullRes.status === 'fulfilled' ? safeParse(fullRes.value) : null;

      let message = `=== FFN ANALYSIS ===\nSymbols: ${symbols.join(', ')}  Benchmark: ${benchmark}\n`;
      message += `Period: ${fmt(startDate)} → ${fmt(endDate)}  (${lookbackDays}d)\n\n`;

      if (fullAnalysis) {
        message += `FULL ANALYSIS:\n`;
        for (const sym of symbols) {
          const m = fullAnalysis[sym] ?? fullAnalysis?.metrics?.[sym];
          if (m) {
            message += `  ${sym}: CAGR ${(m.cagr_pct ?? (m.cagr != null ? m.cagr * 100 : 0)).toFixed(1)}%`;
            if (m.sharpe_ratio !== undefined) message += `  Sharpe ${m.sharpe_ratio.toFixed(2)}`;
            if (m.max_drawdown !== undefined) message += `  MaxDD ${(m.max_drawdown * 100).toFixed(1)}%`;
            message += '\n';
          }
        }
      }

      const opt = optRes.status === 'fulfilled' ? safeParse(optRes.value) : null;
      if (opt?.weights) {
        message += `\nERC OPTIMAL WEIGHTS:\n`;
        for (const [sym, w] of Object.entries(opt.weights))
          message += `  ${sym}: ${((w as number) * 100).toFixed(1)}%\n`;
      }

      return {
        success: true,
        data: {
          benchmark, rolling_window: rollingWin, lookback_days: lookbackDays,
          symbols, portfolio_value: summary?.total_market_value,
          prices_available: Object.fromEntries(Object.entries(pricesMap).map(([k, v]) => [k, Object.keys(v).length])),
          // Section: FULL ANALYSIS
          full_analysis:     fullRes.status    === 'fulfilled' ? safeParse(fullRes.value)    : null,
          // Section: BENCHMARK
          benchmark_comparison: benchRes.status === 'fulfilled' ? safeParse(benchRes.value)  : null,
          // Section: OPTIMIZATION (ERC)
          optimization_erc:  optRes.status     === 'fulfilled' ? safeParse(optRes.value)     : null,
          // Section: REBASED PRICES
          rebased_prices:    rebaseRes.status  === 'fulfilled' ? safeParse(rebaseRes.value)  : null,
          // Section: DRAWDOWNS
          drawdowns:         ddRes.status      === 'fulfilled' ? safeParse(ddRes.value)      : null,
          // Section: ROLLING METRICS
          rolling_metrics:   rollingRes.status === 'fulfilled' ? safeParse(rollingRes.value) : null,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // BONUS: get_portfolio_full_context — single-call complete snapshot
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_full_context',
    description:
      'Single-call comprehensive snapshot: summary, advanced metrics, risk metrics, '
      + 'diversification, sector breakdown, top/worst performers. '
      + 'Use when an AI agent needs the full portfolio picture in one shot.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const [sumRes, metRes, riskRes, divRes] = await Promise.allSettled([
        contexts.getPortfolioSummary(args.portfolio_id),
        contexts.calculateAdvancedMetrics  ? contexts.calculateAdvancedMetrics(args.portfolio_id)  : Promise.resolve(null),
        contexts.calculateRiskMetrics      ? contexts.calculateRiskMetrics(args.portfolio_id)      : Promise.resolve(null),
        contexts.analyzeDiversification    ? contexts.analyzeDiversification(args.portfolio_id)    : Promise.resolve(null),
      ]);

      const summary = sumRes.status  === 'fulfilled' ? sumRes.value  : null;
      const metrics = metRes.status  === 'fulfilled' ? metRes.value  : null;
      const risk    = riskRes.status === 'fulfilled' ? riskRes.value : null;
      const div     = divRes.status  === 'fulfilled' ? divRes.value  : null;

      const holdings: any[] = summary?.holdings ?? [];
      const totalValue = summary?.total_market_value ?? 0;

      const sectorWeights: Record<string, number> = {};
      for (const h of holdings) {
        const s = (h.sector as string | undefined) ?? 'Unknown';
        sectorWeights[s] = (sectorWeights[s] ?? 0) + (h.weight ?? 0);
      }

      const sorted = [...holdings].sort((a: any, b: any) => (b.unrealized_pnl_percent ?? 0) - (a.unrealized_pnl_percent ?? 0));

      let message = `=== FULL PORTFOLIO CONTEXT ===\n\n`;
      message += `"${summary?.portfolio?.name ?? 'Unknown'}"\n`;
      message += `Value: $${totalValue.toFixed(2)} | Cost: $${(summary?.total_cost_basis ?? 0).toFixed(2)}\n`;
      message += `P&L: ${(summary?.total_unrealized_pnl ?? 0) >= 0 ? '+' : ''}$${(summary?.total_unrealized_pnl ?? 0).toFixed(2)} (${(summary?.total_unrealized_pnl_percent ?? 0).toFixed(2)}%)\n`;
      message += `Day: ${(summary?.total_day_change ?? 0) >= 0 ? '+' : ''}$${(summary?.total_day_change ?? 0).toFixed(2)} (${(summary?.total_day_change_percent ?? 0).toFixed(2)}%)\n`;
      message += `Positions: ${holdings.length}\n`;
      if (sorted.length > 0) {
        message += `Best:  ${sorted[0].symbol} (${sorted[0].unrealized_pnl_percent?.toFixed(1)}%)\n`;
        message += `Worst: ${sorted[sorted.length-1].symbol} (${sorted[sorted.length-1].unrealized_pnl_percent?.toFixed(1)}%)\n`;
      }
      message += `Sectors: ${Object.entries(sectorWeights).map(([s,w]) => `${s} ${w.toFixed(0)}%`).join(' | ')}\n`;
      if (metrics) message += `Risk: Sharpe ${metrics.sharpe_ratio?.toFixed(2)}, Vol ${(metrics.portfolio_volatility*100)?.toFixed(1)}%, MaxDD ${(metrics.max_drawdown*100)?.toFixed(1)}%\n`;

      return {
        success: true,
        data: {
          summary: {
            portfolio: summary?.portfolio,
            total_market_value: totalValue,
            total_cost_basis: summary?.total_cost_basis,
            total_unrealized_pnl: summary?.total_unrealized_pnl,
            total_unrealized_pnl_percent: summary?.total_unrealized_pnl_percent,
            total_day_change: summary?.total_day_change,
            total_day_change_percent: summary?.total_day_change_percent,
            positions_count: holdings.length,
          },
          holdings: holdings.map((h: any) => ({
            symbol: h.symbol, quantity: h.quantity,
            current_price: h.current_price, market_value: h.market_value,
            unrealized_pnl: h.unrealized_pnl, unrealized_pnl_percent: h.unrealized_pnl_percent,
            day_change_percent: h.day_change_percent, weight: h.weight, sector: h.sector,
          })),
          sector_weights: sectorWeights,
          advanced_metrics: metrics,
          risk_metrics: risk,
          diversification: div,
          fetched_at: new Date().toISOString(),
        },
        message,
      };
    },
  },
];
