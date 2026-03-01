// Portfolio Panel MCP Tools — Part 1 (tools 1–5)
// Panel coverage:
//   1. get_portfolio_sectors_analytics  → AnalyticsSectorsPanel
//   2. get_portfolio_performance_risk   → PerformanceRiskPanel
//   3. get_portfolio_optimization       → PortfolioOptimizationView
//   4. get_portfolio_quantstats         → QuantStatsView
//   5. get_portfolio_reports            → ReportsPanel

import { InternalTool } from '../types';
import { si, safeParse, toWeights, toReturns, toPnlReturns } from './portfolioPanelHelpers';

export const portfolioPanelTools1: InternalTool[] = [

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
          performance_analysis:     perfRes.status    === 'fulfilled'    ? safeParse(perfRes.value)    : null,
          ffn_performance:          ffnPerfRes.status === 'fulfilled'  ? safeParse(ffnPerfRes.value) : null,
          ffn_monthly_returns:      ffnMonthlyRes.status === 'fulfilled' ? safeParse(ffnMonthlyRes.value) : null,
          // CORRELATION sub-tab
          covariance_correlation:   covRes.status     === 'fulfilled'     ? safeParse(covRes.value)     : null,
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
];
