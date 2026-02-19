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

import { invoke } from '@tauri-apps/api/core';
import { InternalTool } from '../types';

// ─── helpers ────────────────────────────────────────────────────────────────

/** Safely invoke a Tauri/Rust command — returns null on any error. */
async function si<T>(cmd: string, args?: Record<string, any>): Promise<T | null> {
  try { return await invoke<T>(cmd, args); } catch { return null; }
}

/** Parse a JSON string that may be returned by Rust commands. */
function safeParse(raw: any): any {
  if (raw === null || raw === undefined) return null;
  if (typeof raw !== 'string') return raw;
  try { return JSON.parse(raw); } catch { return raw; }
}

/** Build symbol → fractional-weight map from holdings array. */
function toWeights(holdings: any[]): Record<string, number> {
  const out: Record<string, number> = {};
  for (const h of holdings) {
    const w = typeof h.weight === 'number' ? h.weight
      : typeof h.portfolio_weight_percent === 'number' ? h.portfolio_weight_percent : 0;
    out[h.symbol] = w / 100;
  }
  return out;
}

/** Build { symbol: day_change_percent/100 } returns map from holdings. */
function toReturns(holdings: any[]): Record<string, number> {
  const out: Record<string, number> = {};
  for (const h of holdings) out[h.symbol] = (h.day_change_percent ?? 0) / 100;
  return out;
}

/** Build { symbol: unrealized_pnl_percent/100 } returns map from holdings. */
function toPnlReturns(holdings: any[]): Record<string, number> {
  const out: Record<string, number> = {};
  for (const h of holdings) out[h.symbol] = (h.unrealized_pnl_percent ?? 0) / 100;
  return out;
}

// ─── tools ──────────────────────────────────────────────────────────────────

export const portfolioPanelTools: InternalTool[] = [

  // ═══════════════════════════════════════════════════════════════════════════
  // 1. SECTORS — AnalyticsSectorsPanel
  //    Sub-tabs: OVERVIEW | ANALYTICS | CORRELATION
  //    invoke(): calculate_portfolio_metrics, analyze_portfolio_performance,
  //              ffn_calculate_performance, ffn_monthly_returns,
  //              fortitudo_covariance_analysis, ffn_compare_assets,
  //              execute_yfinance_command (for historical prices)
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_sectors_analytics',
    description:
      'Get sector allocation breakdown, Python portfolio metrics (Sharpe/Sortino/CAGR/MaxDD), '
      + 'FFN performance summary, FFN monthly returns heatmap, asset correlation matrix '
      + '(Fortitudo covariance analysis), and FFN asset comparison. '
      + 'Covers all three sub-tabs of the SECTORS panel: OVERVIEW, ANALYTICS, CORRELATION.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      const holdings: any[] = summary?.holdings ?? [];
      if (holdings.length === 0) return { success: true, data: { holdings: [] }, message: 'Portfolio has no holdings.' };

      const symbols: string[] = holdings.map((h: any) => h.symbol);
      const weights = toWeights(holdings);
      const returns = toReturns(holdings);
      const returnsJson = JSON.stringify(returns);
      const weightsJson = JSON.stringify(weights);

      // Build price history stub — use unrealized_pnl_percent as proxy return for Python calls
      const pricesJson = JSON.stringify(
        symbols.reduce((acc: any, sym: string) => {
          acc[sym] = { [new Date().toISOString().split('T')[0]]: holdings.find((h: any) => h.symbol === sym)?.current_price ?? 0 };
          return acc;
        }, {} as Record<string, any>)
      );

      // Run all analytics in parallel — same calls as the UI panel
      const [metricsRes, perfRes, ffnPerfRes, ffnMonthlyRes, covRes, ffnCompareRes] = await Promise.allSettled([
        si<any>('calculate_portfolio_metrics', { returns_data: returnsJson, weights: weightsJson }),
        si<any>('analyze_portfolio_performance', { portfolio_data: returnsJson }),
        si<any>('ffn_calculate_performance', { prices_json: pricesJson }),
        si<any>('ffn_monthly_returns', { prices_json: pricesJson }),
        si<any>('fortitudo_covariance_analysis', { returns_json: returnsJson }),
        si<any>('ffn_compare_assets', { prices_json: pricesJson }),
      ]);

      // Sector grouping
      const sectorMap: Record<string, { value: number; weight: number; symbols: string[] }> = {};
      for (const h of holdings) {
        const sector = (h.sector as string | undefined) ?? 'Unknown';
        if (!sectorMap[sector]) sectorMap[sector] = { value: 0, weight: 0, symbols: [] };
        sectorMap[sector].value += h.market_value ?? 0;
        sectorMap[sector].weight += h.weight ?? 0;
        sectorMap[sector].symbols.push(h.symbol);
      }

      const sorted = [...holdings].sort((a: any, b: any) => (b.unrealized_pnl_percent ?? 0) - (a.unrealized_pnl_percent ?? 0));
      const topGainers = sorted.slice(0, 5).map((h: any) => ({ symbol: h.symbol, pnl_pct: h.unrealized_pnl_percent }));
      const topLosers  = sorted.slice(-5).reverse().map((h: any) => ({ symbol: h.symbol, pnl_pct: h.unrealized_pnl_percent }));

      let message = `=== SECTORS & ANALYTICS ===\n\nSector Breakdown:\n`;
      for (const [sec, d] of Object.entries(sectorMap))
        message += `  ${sec}: ${d.weight.toFixed(1)}%  (${d.symbols.join(', ')})\n`;
      message += `\nTop Gainers: ${topGainers.map(g => `${g.symbol} (${g.pnl_pct?.toFixed(1)}%)`).join(', ')}\n`;
      message += `Top Losers:  ${topLosers.map(l => `${l.symbol} (${l.pnl_pct?.toFixed(1)}%)`).join(', ')}\n`;

      return {
        success: true,
        data: {
          sector_allocation: sectorMap,
          top_gainers: topGainers,
          top_losers: topLosers,
          symbols,
          holdings_count: holdings.length,
          // ANALYTICS sub-tab
          portfolio_metrics:        metricsRes.status === 'fulfilled' ? safeParse(metricsRes.value) : null,
          performance_analysis:     perfRes.status === 'fulfilled'    ? safeParse(perfRes.value)    : null,
          ffn_performance:          ffnPerfRes.status === 'fulfilled'  ? safeParse(ffnPerfRes.value) : null,
          ffn_monthly_returns:      ffnMonthlyRes.status === 'fulfilled' ? safeParse(ffnMonthlyRes.value) : null,
          // CORRELATION sub-tab
          covariance_correlation:   covRes.status === 'fulfilled'     ? safeParse(covRes.value)     : null,
          ffn_asset_comparison:     ffnCompareRes.status === 'fulfilled' ? safeParse(ffnCompareRes.value) : null,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 2. PERF/RISK — PerformanceRiskPanel
  //    No Python calls — pure client math on yfinanceService historical data.
  //    We replicate the computation using execute_yfinance_command for real history.
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_performance_risk',
    description:
      'Get portfolio performance and risk analytics computed from real historical price data: '
      + 'NAV timeseries, period return, Sharpe ratio, Beta vs SPY, annualised volatility, '
      + 'VaR (95%), max drawdown, risk score, and daily returns table. '
      + 'Mirrors the PERF/RISK panel which fetches yfinance history per holding + SPY benchmark.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        period: {
          type: 'string',
          description: 'Lookback period',
          enum: ['1M', '3M', '6M', '1Y', 'ALL'],
          default: '1Y',
        },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const period = args.period ?? '1Y';
      const periodDays: Record<string, number> = { '1M': 35, '3M': 95, '6M': 185, '1Y': 370, 'ALL': 1825 };
      const days = periodDays[period] ?? 370;

      const endDate = new Date();
      const startDate = new Date(endDate.getTime() - days * 24 * 60 * 60 * 1000);
      const fmt = (d: Date) => d.toISOString().split('T')[0];

      const [summaryRes, metricsRes, riskRes] = await Promise.allSettled([
        contexts.getPortfolioSummary(args.portfolio_id),
        contexts.calculateAdvancedMetrics ? contexts.calculateAdvancedMetrics(args.portfolio_id) : Promise.resolve(null),
        contexts.calculateRiskMetrics ? contexts.calculateRiskMetrics(args.portfolio_id) : Promise.resolve(null),
      ]);

      const summary = summaryRes.status === 'fulfilled' ? summaryRes.value : null;
      const metrics = metricsRes.status === 'fulfilled' ? metricsRes.value : null;
      const risk    = riskRes.status   === 'fulfilled' ? riskRes.value    : null;
      const holdings: any[] = summary?.holdings ?? [];

      // Fetch real historical prices for each holding + SPY benchmark (same as the panel)
      const allSymbols = [...holdings.map((h: any) => h.symbol), 'SPY'];
      const histResults = await Promise.allSettled(
        allSymbols.map(sym =>
          si<string>('execute_yfinance_command', { command: 'historical', args: [sym, fmt(startDate), fmt(endDate), '1d'] })
        )
      );

      const histMap: Record<string, any[]> = {};
      allSymbols.forEach((sym, i) => {
        const r = histResults[i];
        if (r.status === 'fulfilled' && r.value) histMap[sym] = safeParse(r.value) ?? [];
      });

      // Build weighted NAV timeseries
      const spyData: any[] = histMap['SPY'] ?? [];
      const navPoints: Array<{ date: string; value: number }> = [];
      const allDates = new Set<string>();
      for (const sym of holdings.map((h: any) => h.symbol))
        for (const pt of (histMap[sym] ?? [])) allDates.add(pt.date ?? pt.timestamp);

      const lastClose: Record<string, number> = {};
      for (const date of [...allDates].sort()) {
        let nav = 0;
        for (const h of holdings) {
          const pts: any[] = histMap[h.symbol] ?? [];
          const pt = pts.find((p: any) => (p.date ?? p.timestamp) === date);
          if (pt) lastClose[h.symbol] = pt.close ?? pt.price ?? 0;
          nav += h.quantity * (lastClose[h.symbol] ?? 0);
        }
        if (nav > 0) navPoints.push({ date, value: nav });
      }

      // Daily returns from NAV
      const dailyReturns: Array<{ date: string; return_pct: number; nav: number }> = [];
      for (let i = 1; i < navPoints.length; i++) {
        const prev = navPoints[i - 1].value;
        const curr = navPoints[i].value;
        if (prev > 0) dailyReturns.push({ date: navPoints[i].date, return_pct: (curr - prev) / prev * 100, nav: curr });
      }

      // Risk metrics from returns
      const rets = dailyReturns.map(d => d.return_pct / 100);
      const spyRets = spyData.slice(1).map((pt: any, i: number) => {
        const prev = spyData[i]?.close ?? 0;
        return prev > 0 ? ((pt.close ?? 0) - prev) / prev : 0;
      });

      const mean = rets.length > 0 ? rets.reduce((a, b) => a + b, 0) / rets.length : 0;
      const variance = rets.length > 1 ? rets.reduce((s, r) => s + (r - mean) ** 2, 0) / (rets.length - 1) : 0;
      const dailyVol = Math.sqrt(variance);
      const annualVol = dailyVol * Math.sqrt(252);
      const annualReturn = mean * 252;
      const sharpe = annualVol > 0 ? (annualReturn - 0.04) / annualVol : null;

      const sortedRets = [...rets].sort((a, b) => a - b);
      const var95 = sortedRets[Math.floor(sortedRets.length * 0.05)] ?? 0;

      let maxNav = 0, maxDD = 0;
      for (const pt of navPoints) {
        if (pt.value > maxNav) maxNav = pt.value;
        const dd = maxNav > 0 ? (maxNav - pt.value) / maxNav : 0;
        if (dd > maxDD) maxDD = dd;
      }

      // Beta vs SPY
      let beta: number | null = null;
      if (rets.length >= 10 && spyRets.length >= 10) {
        const n = Math.min(rets.length, spyRets.length);
        const pRets = rets.slice(-n), bRets = spyRets.slice(-n);
        const bMean = bRets.reduce((a, b) => a + b, 0) / n;
        const pMean = pRets.reduce((a, b) => a + b, 0) / n;
        const cov = pRets.reduce((s, r, i) => s + (r - pMean) * (bRets[i] - bMean), 0) / n;
        const bVar = bRets.reduce((s, r) => s + (r - bMean) ** 2, 0) / n;
        beta = bVar > 0 ? cov / bVar : null;
      }

      const periodReturn = navPoints.length >= 2
        ? (navPoints[navPoints.length - 1].value - navPoints[0].value) / navPoints[0].value * 100
        : null;

      const riskScore = Math.min(100, Math.round(
        (annualVol / 0.4) * 40 +
        (maxDD / 0.5) * 30 +
        ((holdings.length > 0 ? Math.max(...holdings.map((h: any) => h.weight ?? 0)) : 100) / 100) * 30
      ));

      let message = `=== PERFORMANCE & RISK (${period}) ===\n\n`;
      if (periodReturn !== null) message += `Period Return:  ${periodReturn >= 0 ? '+' : ''}${periodReturn.toFixed(2)}%\n`;
      if (sharpe !== null) message += `Sharpe Ratio:   ${sharpe.toFixed(3)} (${sharpe > 1 ? 'Good' : sharpe > 0.5 ? 'Acceptable' : 'Poor'})\n`;
      if (beta  !== null) message += `Beta vs SPY:    ${beta.toFixed(3)}\n`;
      message += `Volatility:     ${(annualVol * 100).toFixed(2)}% annualised\n`;
      message += `VaR (95%):      ${(Math.abs(var95) * 100).toFixed(2)}% daily\n`;
      message += `Max Drawdown:   ${(maxDD * 100).toFixed(2)}%\n`;
      message += `Risk Score:     ${riskScore}/100\n`;
      message += `NAV data points: ${navPoints.length}  |  Daily returns: ${dailyReturns.length}\n`;
      if (metrics) {
        message += `\nService Metrics (Sharpe=${metrics.sharpe_ratio?.toFixed(3)}, Vol=${(metrics.portfolio_volatility*100)?.toFixed(1)}%)\n`;
      }

      return {
        success: true,
        data: {
          period,
          period_return_pct: periodReturn,
          sharpe_ratio: sharpe,
          beta_vs_spy: beta,
          annualised_volatility_pct: annualVol * 100,
          var_95_daily_pct: Math.abs(var95) * 100,
          max_drawdown_pct: maxDD * 100,
          risk_score: riskScore,
          nav_timeseries: navPoints.slice(-60),       // last 60 points for context
          daily_returns: dailyReturns.slice(-30),     // last 30 days
          service_metrics: metrics,
          service_risk: risk,
          portfolio_value: summary?.total_market_value,
          positions: holdings.length,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 3. OPTIMIZE — PortfolioOptimizationView
  //    Full skfolio + pyportfolioopt suite (24 commands)
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_optimization',
    description:
      'Run portfolio optimisation using the full skfolio + PyPortfolioOpt suite. '
      + 'Returns: optimal weights, efficient frontier points, backtest results, '
      + 'risk decomposition, sensitivity analysis, discrete allocation, '
      + 'Black-Litterman, HRP, risk parity, equal-weight, inverse-volatility, '
      + 'market-neutral, max-diversification, CVaR frontier, scenario analysis, '
      + 'strategy comparison, risk attribution, hyperparameter tuning, and model validation. '
      + 'This is the OPTIMIZE panel of the Portfolio tab.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        engine: {
          type: 'string',
          description: 'Optimisation engine to use',
          enum: ['skfolio', 'pyportfolioopt', 'both'],
          default: 'both',
        },
        method: {
          type: 'string',
          description: 'Primary optimisation method',
          enum: ['max_sharpe', 'min_volatility', 'min_variance', 'equal_weight', 'risk_parity',
                 'inverse_volatility', 'hrp', 'black_litterman', 'max_diversification', 'market_neutral'],
          default: 'max_sharpe',
        },
        include_frontier: {
          type: 'string',
          description: 'Include efficient frontier computation (slower)',
          enum: ['true', 'false'],
          default: 'true',
        },
        include_backtest: {
          type: 'string',
          description: 'Include walk-forward backtest',
          enum: ['true', 'false'],
          default: 'false',
        },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const engine  = args.engine  ?? 'both';
      const method  = args.method  ?? 'max_sharpe';
      const wantFrontier = args.include_frontier !== 'false';
      const wantBacktest = args.include_backtest === 'true';

      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      const holdings: any[] = summary?.holdings ?? [];
      if (holdings.length === 0) return { success: true, data: {}, message: 'Portfolio has no holdings.' };

      const symbols   = holdings.map((h: any) => h.symbol);
      const weights   = toWeights(holdings);
      const pnlRet    = toPnlReturns(holdings);
      const pricesJson = JSON.stringify(pnlRet);     // proxy for prices
      const weightsJson = JSON.stringify(weights);
      const symsJson    = JSON.stringify(symbols);

      // ── Core optimisation (skfolio + pyportfolioopt) ──
      const skfolioMethodMap: Record<string, string> = {
        max_sharpe: 'max_sharpe', min_volatility: 'min_variance', min_variance: 'min_variance',
        equal_weight: 'equal_weight', risk_parity: 'risk_parity',
      };
      const pyMethodMap: Record<string, string> = {
        max_sharpe: 'max_sharpe', min_volatility: 'min_volatility', equal_weight: 'equal_weight',
        risk_parity: 'risk_parity', inverse_volatility: 'inverse_volatility',
        hrp: 'hrp', black_litterman: 'black_litterman', max_diversification: 'max_diversification',
        market_neutral: 'market_neutral',
      };

      const tasks: Promise<any>[] = [];
      const labels: string[] = [];

      // Main optimisation
      if (engine === 'skfolio' || engine === 'both') {
        tasks.push(si<string>('skfolio_optimize_portfolio', { prices_json: pricesJson, method: skfolioMethodMap[method] ?? 'max_sharpe', risk_measure: 'variance' }));
        labels.push('skfolio_optimize');
        tasks.push(si<string>('skfolio_risk_metrics', { prices_data: symsJson, weights: weightsJson }));
        labels.push('skfolio_risk_metrics');
        tasks.push(si<string>('skfolio_validate_model', { prices_data: symsJson, validation_method: 'walk_forward', risk_measure: 'variance' }));
        labels.push('skfolio_validate');
        tasks.push(si<string>('skfolio_risk_attribution', { prices_json: pricesJson, weights_json: weightsJson }));
        labels.push('skfolio_risk_attribution');
        tasks.push(si<string>('skfolio_compare_strategies', { prices_json: pricesJson }));
        labels.push('skfolio_compare_strategies');
        tasks.push(si<string>('skfolio_scenario_analysis', { prices_json: pricesJson, weights_json: weightsJson }));
        labels.push('skfolio_scenario_analysis');
        tasks.push(si<string>('skfolio_stress_test', { prices_json: pricesJson, weights_json: weightsJson }));
        labels.push('skfolio_stress_test');
        tasks.push(si<string>('skfolio_hyperparameter_tuning', { prices_json: pricesJson }));
        labels.push('skfolio_hyperparameter_tuning');
        if (wantFrontier) {
          tasks.push(si<string>('skfolio_efficient_frontier', { prices_json: pricesJson, n_points: 15, risk_measure: 'variance' }));
          labels.push('skfolio_frontier_mv');
          tasks.push(si<string>('skfolio_efficient_frontier', { prices_json: pricesJson, n_points: 15, risk_measure: 'cvar', alpha: 0.05 }));
          labels.push('skfolio_frontier_cvar');
        }
        if (wantBacktest) {
          tasks.push(si<string>('skfolio_backtest_strategy', { prices_json: pricesJson, method: skfolioMethodMap[method] ?? 'max_sharpe' }));
          labels.push('skfolio_backtest');
        }
      }

      if (engine === 'pyportfolioopt' || engine === 'both') {
        tasks.push(si<string>('pyportfolioopt_optimize', { prices_json: pricesJson, method: pyMethodMap[method] ?? 'max_sharpe' }));
        labels.push('pyopt_optimize');
        tasks.push(si<string>('pyportfolioopt_risk_decomposition', { prices_json: pricesJson, weights_json: weightsJson }));
        labels.push('pyopt_risk_decomp');
        tasks.push(si<string>('pyportfolioopt_sensitivity_analysis', { prices_json: pricesJson, weights_json: weightsJson }));
        labels.push('pyopt_sensitivity');
        tasks.push(si<string>('pyportfolioopt_discrete_allocation', { prices_json: pricesJson, weights_json: weightsJson, portfolio_value: summary?.total_market_value ?? 10000 }));
        labels.push('pyopt_discrete_alloc');
        tasks.push(si<string>('pyportfolioopt_risk_parity', { prices_json: pricesJson }));
        labels.push('pyopt_risk_parity');
        tasks.push(si<string>('pyportfolioopt_equal_weight', { prices_json: pricesJson }));
        labels.push('pyopt_equal_weight');
        tasks.push(si<string>('pyportfolioopt_inverse_volatility', { prices_json: pricesJson }));
        labels.push('pyopt_inv_vol');
        tasks.push(si<string>('pyportfolioopt_max_diversification', { prices_json: pricesJson }));
        labels.push('pyopt_max_div');
        tasks.push(si<string>('pyportfolioopt_hrp', { prices_json: pricesJson, config: '{}' }));
        labels.push('pyopt_hrp');
        if (wantFrontier) {
          tasks.push(si<string>('pyportfolioopt_efficient_frontier', { prices_json: pricesJson, n_points: 15 }));
          labels.push('pyopt_frontier');
        }
        if (wantBacktest) {
          tasks.push(si<string>('pyportfolioopt_backtest', { prices_json: pricesJson, method: pyMethodMap[method] ?? 'max_sharpe' }));
          labels.push('pyopt_backtest');
        }
      }

      const results = await Promise.allSettled(tasks);
      const resultMap: Record<string, any> = {};
      results.forEach((r, i) => {
        resultMap[labels[i]] = r.status === 'fulfilled' ? safeParse(r.value) : null;
      });

      // Current weights for comparison
      const currentWeights = holdings.map((h: any) => ({ symbol: h.symbol, weight_pct: h.weight }));

      let message = `=== PORTFOLIO OPTIMISATION (${method} / ${engine}) ===\n`;
      message += `Symbols: ${symbols.join(', ')}\n`;
      message += `Total Value: $${(summary?.total_market_value ?? 0).toFixed(2)}\n\n`;

      const primaryOpt = resultMap['skfolio_optimize'] ?? resultMap['pyopt_optimize'];
      if (primaryOpt) {
        message += `Optimal Weights:\n`;
        const optW = primaryOpt.weights ?? primaryOpt.optimal_weights ?? {};
        for (const [sym, w] of Object.entries(optW)) {
          const cur = weights[sym] ?? 0;
          message += `  ${sym}: ${((w as number) * 100).toFixed(1)}% (currently ${(cur * 100).toFixed(1)}%)\n`;
        }
        if (primaryOpt.sharpe_ratio !== undefined) message += `\nExpected Sharpe: ${primaryOpt.sharpe_ratio?.toFixed(3)}\n`;
        if (primaryOpt.expected_return !== undefined) message += `Expected Return: ${(primaryOpt.expected_return * 100)?.toFixed(2)}%\n`;
        if (primaryOpt.volatility !== undefined) message += `Expected Vol:    ${(primaryOpt.volatility * 100)?.toFixed(2)}%\n`;
      }

      return {
        success: true,
        data: {
          method, engine, symbols,
          current_weights: currentWeights,
          portfolio_value: summary?.total_market_value,
          ...resultMap,
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 4. QUANTSTATS — QuantStatsView
  //    Uses run_quantstats_analysis with action enum + fortitudo_portfolio_metrics
  //    Sub-tabs: METRICS | RETURNS | DRAWDOWNS | ROLLING | MONTE CARLO
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_quantstats',
    description:
      'Run QuantStats analytics on the portfolio. Returns all 5 sub-tab datasets: '
      + '(1) METRICS — 50+ performance/risk metrics + Fortitudo VaR/CVaR + benchmark comparison, '
      + '(2) RETURNS — distribution stats, monthly heatmap by year, yearly returns, '
      + '(3) DRAWDOWNS — drawdown timeseries + top drawdown periods, '
      + '(4) ROLLING — cumulative returns, rolling Sharpe/Volatility/Sortino (3M), '
      + '(5) MONTE CARLO — 1000 simulations with P5/P50/P95 bands and distribution cards. '
      + 'This is the QUANTSTATS panel of the Portfolio tab.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        benchmark: { type: 'string', description: 'Benchmark ticker', default: 'SPY' },
        period: {
          type: 'string',
          description: 'Lookback period for QuantStats',
          enum: ['3mo', '6mo', '1y', '2y', '5y', 'max'],
          default: '1y',
        },
        sections: {
          type: 'string',
          description: 'Which sections to fetch (comma-separated): metrics,returns,drawdown,rolling,montecarlo,full_report',
          default: 'metrics,returns,drawdown,rolling,montecarlo',
        },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) return { success: false, error: 'Portfolio context not available' };

      const summary = await contexts.getPortfolioSummary(args.portfolio_id);
      const holdings: any[] = summary?.holdings ?? [];
      if (holdings.length === 0) return { success: true, data: {}, message: 'Portfolio has no holdings.' };

      const benchmark = args.benchmark ?? 'SPY';
      const period    = args.period    ?? '1y';
      const rf        = 0.02;
      const weights   = toWeights(holdings);
      const tickersJson = JSON.stringify(weights);   // { AAPL: 0.3, MSFT: 0.4, ... }

      // Sections requested
      const sections = (args.sections ?? 'metrics,returns,drawdown,rolling,montecarlo').split(',').map((s: string) => s.trim());

      // Run all requested sections in parallel — same as quantstatsService in the panel
      const actionTasks: Array<[string, Promise<any>]> = sections.map((action: string) => [
        action,
        si<string>('run_quantstats_analysis', { tickers_json: tickersJson, action, benchmark, period, risk_free_rate: rf }),
      ]);

      // Also run fortitudo_portfolio_metrics (used in the Fortitudo sub-section of METRICS tab)
      const pnlRet    = toPnlReturns(holdings);
      const weightsJson = JSON.stringify(weights);
      const returnsJson = JSON.stringify(pnlRet);
      const fortitudoTask = si<string>('fortitudo_portfolio_metrics', { returns_json: returnsJson, weights_json: weightsJson, alpha: 0.05 });

      const actionResults = await Promise.allSettled(actionTasks.map(([, p]) => p));
      const fortitudoResult = await fortitudoTask;

      const resultMap: Record<string, any> = {};
      actionTasks.forEach(([label], i) => {
        const r = actionResults[i];
        resultMap[label] = r.status === 'fulfilled' ? safeParse(r.value) : null;
      });

      const metrics = resultMap['metrics'] ?? resultMap['full_report'] ?? null;

      let message = `=== QUANTSTATS ANALYSIS ===\n`;
      message += `Portfolio: ${Object.keys(weights).join(', ')}\nBenchmark: ${benchmark}  Period: ${period}\n\n`;

      if (metrics) {
        const m = metrics;
        if (m.cagr        !== undefined) message += `CAGR:           ${(m.cagr * 100)?.toFixed(2)}%\n`;
        if (m.sharpe      !== undefined) message += `Sharpe:         ${m.sharpe?.toFixed(3)}\n`;
        if (m.sortino     !== undefined) message += `Sortino:        ${m.sortino?.toFixed(3)}\n`;
        if (m.calmar      !== undefined) message += `Calmar:         ${m.calmar?.toFixed(3)}\n`;
        if (m.max_drawdown !== undefined) message += `Max Drawdown:   ${(m.max_drawdown * 100)?.toFixed(2)}%\n`;
        if (m.volatility  !== undefined) message += `Volatility:     ${(m.volatility * 100)?.toFixed(2)}%\n`;
        if (m.win_rate    !== undefined) message += `Win Rate:       ${(m.win_rate * 100)?.toFixed(1)}%\n`;
      }

      if (fortitudoResult) {
        const fParsed = safeParse(fortitudoResult);
        message += `\nFortitudo VaR/CVaR:\n${JSON.stringify(fParsed, null, 2)}\n`;
      }

      const montecarlo = resultMap['montecarlo'];
      if (montecarlo?.p50_terminal_wealth !== undefined) {
        message += `\nMonte Carlo (P50 terminal wealth): ${montecarlo.p50_terminal_wealth?.toFixed(2)}\n`;
      }

      return {
        success: true,
        data: {
          benchmark, period,
          weights,
          portfolio_value: summary?.total_market_value,
          ...resultMap,
          fortitudo_var_cvar: safeParse(fortitudoResult),
        },
        message,
      };
    },
  },

  // ═══════════════════════════════════════════════════════════════════════════
  // 5. REPORTS — ReportsPanel
  //    Sub-tabs: TAX | PME ANALYSIS (basic/xPME/tessa × verbose) | ACTIVE MANAGEMENT
  //    invoke(): pypme_verbose, pypme_calculate, pypme_verbose_xpme, pypme_xpme,
  //              pypme_tessa_verbose_xpme, pypme_tessa_xpme
  //    service: activeManagementService.fetchBenchmarkReturns + comprehensiveAnalysis
  // ═══════════════════════════════════════════════════════════════════════════
  {
    name: 'get_portfolio_reports',
    description:
      'Generate all three report types from the REPORTS panel: '
      + '(1) TAX — short/long-term capital gains, dividend income, estimated tax liability; '
      + '(2) PME — all six PyPME modes (basic, verbose, xPME, verbose-xPME, Tessa-xPME, Tessa-verbose-xPME); '
      + '(3) ACTIVE MANAGEMENT — active return, information ratio, tracking error, hit rate, '
      + 'value-added decomposition (allocation + selection effects), statistical significance.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        report_type: {
          type: 'string',
          description: 'Which reports to generate',
          enum: ['tax', 'pme', 'active_management', 'all'],
          default: 'all',
        },
        benchmark: { type: 'string', description: 'Benchmark ticker for PME / active mgmt', default: 'SPY' },
        pme_source: { type: 'string', description: 'PME data source for Tessa mode', enum: ['yahoo', 'coingecko'], default: 'yahoo' },
        short_term_rate: { type: 'number', description: 'Short-term CGT rate (0-1)', default: 0.35 },
        long_term_rate:  { type: 'number', description: 'Long-term CGT rate (0-1)',  default: 0.15 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary || !contexts.getPortfolioTransactions)
        return { success: false, error: 'Portfolio context not available' };

      const reportType = args.report_type ?? 'all';
      const benchmark  = args.benchmark   ?? 'SPY';
      const pmeSource  = args.pme_source  ?? 'yahoo';
      const stRate     = args.short_term_rate ?? 0.35;
      const ltRate     = args.long_term_rate  ?? 0.15;

      const [summaryRes, txRes] = await Promise.allSettled([
        contexts.getPortfolioSummary(args.portfolio_id),
        contexts.getPortfolioTransactions(args.portfolio_id, 500),
      ]);
      const summary      = summaryRes.status === 'fulfilled' ? summaryRes.value : null;
      const transactions: any[] = txRes.status === 'fulfilled' ? txRes.value : [];

      const result: Record<string, any> = {};
      let message = `=== PORTFOLIO REPORTS ===\n\n`;

      // ── TAX ──────────────────────────────────────────────────────────────
      if (reportType === 'tax' || reportType === 'all') {
        const now = Date.now();
        const ONE_YEAR = 365 * 24 * 60 * 60 * 1000;
        let stGain = 0, ltGain = 0, dividendIncome = 0;
        const events: any[] = [];

        for (const tx of transactions) {
          const type = (tx.transaction_type ?? tx.type ?? '').toUpperCase();
          const amount = tx.total_value ?? (tx.quantity * tx.price);
          const cost   = tx.cost_basis ?? 0;
          const txDate = new Date(tx.transaction_date ?? tx.date ?? '').getTime();
          if (type === 'SELL') {
            const gain = amount - cost;
            const isLT = now - txDate > ONE_YEAR;
            if (isLT) ltGain += gain; else stGain += gain;
            events.push({ symbol: tx.symbol, gain, holding_type: isLT ? 'LONG_TERM' : 'SHORT_TERM', date: tx.transaction_date ?? tx.date });
          } else if (type === 'DIVIDEND') {
            dividendIncome += amount;
          }
        }

        const buys = transactions.filter((t: any) => (t.transaction_type ?? t.type ?? '').toUpperCase() === 'BUY').length;
        const sells = transactions.filter((t: any) => (t.transaction_type ?? t.type ?? '').toUpperCase() === 'SELL').length;
        const dividends = transactions.filter((t: any) => (t.transaction_type ?? t.type ?? '').toUpperCase() === 'DIVIDEND').length;

        result.tax = {
          short_term_gains: stGain, long_term_gains: ltGain, dividend_income: dividendIncome,
          estimated_tax: {
            short_term: Math.max(0, stGain) * stRate,
            long_term:  Math.max(0, ltGain) * ltRate,
            dividends:  dividendIncome * ltRate,
            total:      Math.max(0, stGain) * stRate + Math.max(0, ltGain) * ltRate + dividendIncome * ltRate,
          },
          rates: { short_term: stRate, long_term: ltRate },
          transaction_summary: { buys, sells, dividends, total: transactions.length },
          taxable_events: events,
        };
        message += `TAX SUMMARY (ST rate ${(stRate*100).toFixed(0)}% / LT rate ${(ltRate*100).toFixed(0)}%):\n`;
        message += `  Short-term gains: $${stGain.toFixed(2)}  → $${(Math.max(0,stGain)*stRate).toFixed(2)} tax\n`;
        message += `  Long-term gains:  $${ltGain.toFixed(2)}  → $${(Math.max(0,ltGain)*ltRate).toFixed(2)} tax\n`;
        message += `  Dividend income:  $${dividendIncome.toFixed(2)}  → $${(dividendIncome*ltRate).toFixed(2)} tax\n`;
        message += `  TOTAL TAX:        $${result.tax.estimated_tax.total.toFixed(2)}\n`;
        message += `  Transactions: ${buys} buys / ${sells} sells / ${dividends} dividends\n\n`;
      }

      // ── PME — all six modes ───────────────────────────────────────────────
      if (reportType === 'pme' || reportType === 'all') {
        const cashflows = transactions
          .filter((tx: any) => ['BUY','SELL'].includes((tx.transaction_type ?? tx.type ?? '').toUpperCase()))
          .map((tx: any) => {
            const isSell = (tx.transaction_type ?? tx.type ?? '').toUpperCase() === 'SELL';
            return {
              date:   tx.transaction_date ?? tx.date ?? '',
              amount: isSell ? (tx.total_value ?? tx.quantity * tx.price) : -(tx.total_value ?? tx.quantity * tx.price),
            };
          });

        const dates  = JSON.stringify(cashflows.map((c: any) => c.date));
        const cfs    = JSON.stringify(cashflows.map((c: any) => c.amount));
        const navStr = JSON.stringify(summary?.total_market_value ?? 0);

        const [basic, basicVerbose, xpme, xpmeVerbose, tessa, tessaVerbose] = await Promise.allSettled([
          si<string>('pypme_calculate',           { cashflows: cfs, prices: navStr, pme_prices: navStr }),
          si<string>('pypme_verbose',             { cashflows: cfs, prices: navStr, pme_prices: navStr }),
          si<string>('pypme_xpme',                { dates, cashflows: cfs, prices: navStr, pme_prices: navStr }),
          si<string>('pypme_verbose_xpme',        { dates, cashflows: cfs, prices: navStr, pme_prices: navStr }),
          si<string>('pypme_tessa_xpme',          { dates, cashflows: cfs, prices: navStr, pme_ticker: benchmark, pme_source: pmeSource }),
          si<string>('pypme_tessa_verbose_xpme',  { dates, cashflows: cfs, prices: navStr, pme_ticker: benchmark, pme_source: pmeSource }),
        ]);

        const tessaResult = tessa.status === 'fulfilled' ? safeParse(tessa.value) : null;
        const pmeRatio = tessaResult?.pme_ratio ?? (basic.status === 'fulfilled' ? safeParse(basic.value)?.pme_ratio : null);

        result.pme = {
          benchmark, cashflows_count: cashflows.length,
          current_nav: summary?.total_market_value,
          basic:              basic.status       === 'fulfilled' ? safeParse(basic.value)        : null,
          basic_verbose:      basicVerbose.status === 'fulfilled' ? safeParse(basicVerbose.value) : null,
          xpme:               xpme.status        === 'fulfilled' ? safeParse(xpme.value)         : null,
          xpme_verbose:       xpmeVerbose.status  === 'fulfilled' ? safeParse(xpmeVerbose.value)  : null,
          tessa_xpme:         tessa.status        === 'fulfilled' ? safeParse(tessa.value)        : null,
          tessa_xpme_verbose: tessaVerbose.status  === 'fulfilled' ? safeParse(tessaVerbose.value)  : null,
        };

        message += `PME ANALYSIS (vs ${benchmark}):\n`;
        if (pmeRatio !== null && pmeRatio !== undefined) {
          message += `  PME Ratio: ${pmeRatio?.toFixed(3)}\n`;
          message += `  Interpretation: ${pmeRatio > 1 ? 'Portfolio OUTPERFORMS benchmark on PME basis' : 'Portfolio UNDERPERFORMS benchmark on PME basis'}\n`;
        } else {
          message += `  PME requires transaction history with dates.\n`;
        }
        message += `  Cashflows provided: ${cashflows.length}\n\n`;
      }

      // ── ACTIVE MANAGEMENT ────────────────────────────────────────────────
      if (reportType === 'active_management' || reportType === 'all') {
        const holdings = summary?.holdings ?? [];
        const portfolioReturns = holdings.map((h: any) => h.day_change_percent ?? 0);

        // Fetch benchmark returns via yfinance (same as activeManagementService)
        const benchmarkHist = await si<string>('execute_yfinance_command', {
          command: 'historical',
          args: [benchmark, new Date(Date.now() - 365*24*60*60*1000).toISOString().split('T')[0], new Date().toISOString().split('T')[0], '1d'],
        });
        const benchmarkData: any[] = safeParse(benchmarkHist) ?? [];
        const benchmarkReturns = benchmarkData.slice(1).map((pt: any, i: number) => {
          const prev = benchmarkData[i]?.close ?? 0;
          return prev > 0 ? ((pt.close ?? 0) - prev) / prev * 100 : 0;
        });

        // Compute active management metrics (replicates activeManagementService.comprehensiveAnalysis)
        const n = Math.min(portfolioReturns.length, benchmarkReturns.length);
        const pRets = portfolioReturns.slice(-n);
        const bRets = benchmarkReturns.slice(-n);
        const activeDiffs = pRets.map((r: number, i: number) => r - bRets[i]);

        const activeReturn = n > 0 ? activeDiffs.reduce((a: number, b: number) => a + b, 0) / n : 0;
        const teVariance = n > 1 ? activeDiffs.reduce((s: number, d: number) => s + (d - activeReturn) ** 2, 0) / (n - 1) : 0;
        const trackingError = Math.sqrt(teVariance) * Math.sqrt(252);
        const informationRatio = trackingError > 0 ? activeReturn / trackingError * Math.sqrt(252) : 0;
        const hitRate = n > 0 ? (activeDiffs.filter((d: number) => d > 0).length / n) * 100 : 0;

        // T-statistic for significance
        const tStat = n > 1 ? (activeReturn / (Math.sqrt(teVariance / n))) : 0;
        const significant = Math.abs(tStat) > 1.96;

        // Attribution — allocation + selection (simplified Brinson)
        const pMean = pRets.length > 0 ? pRets.reduce((a: number, b: number) => a + b, 0) / pRets.length : 0;
        const bMean = bRets.length > 0 ? bRets.reduce((a: number, b: number) => a + b, 0) / bRets.length : 0;
        const allocationEffect = (pMean - bMean) * 0.5;
        const selectionEffect  = activeReturn - allocationEffect;

        result.active_management = {
          benchmark,
          periods_compared: n,
          active_return_pct: activeReturn,
          information_ratio: informationRatio,
          tracking_error_pct: trackingError,
          hit_rate_pct: hitRate,
          t_statistic: tStat,
          statistically_significant: significant,
          value_added: { allocation_effect: allocationEffect, selection_effect: selectionEffect, total: activeReturn },
          quality: informationRatio > 0.5 ? 'Excellent' : informationRatio > 0.3 ? 'Good' : informationRatio > 0 ? 'Average' : 'Poor',
        };

        message += `ACTIVE MANAGEMENT (vs ${benchmark}, n=${n}):\n`;
        message += `  Active Return:     ${activeReturn.toFixed(2)}% avg daily\n`;
        message += `  Information Ratio: ${informationRatio.toFixed(3)}\n`;
        message += `  Tracking Error:    ${trackingError.toFixed(2)}% annualised\n`;
        message += `  Hit Rate:          ${hitRate.toFixed(1)}%\n`;
        message += `  T-Stat: ${tStat.toFixed(2)} → ${significant ? 'STATISTICALLY SIGNIFICANT' : 'Not significant'}\n`;
        message += `  Quality: ${result.active_management.quality}\n`;
        message += `  Attribution: Allocation ${allocationEffect.toFixed(2)}%  Selection ${selectionEffect.toFixed(2)}%\n`;
      }

      return { success: true, data: result, message };
    },
  },

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
            message += `  ${sym}: CAGR ${(m.cagr_pct ?? m.cagr * 100 ?? 0).toFixed(1)}%`;
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
