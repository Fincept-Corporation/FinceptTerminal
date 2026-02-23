// QuantLib Risk MCP Tools - 25 endpoints
// VaR, Stress Testing, EVT, XVA, Sensitivities, Tail Risk, Portfolio Risk

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

export const quantlibRiskTools: InternalTool[] = [
  // ==================== VaR (6) ====================
  {
    name: 'quantlib_risk_var_parametric',
    description: 'Calculate parametric (variance-covariance) Value at Risk',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_value: { type: 'number', description: 'Total portfolio value' },
        volatility: { type: 'number', description: 'Portfolio volatility (decimal)' },
        confidence: { type: 'number', description: 'Confidence level (e.g., 0.95, 0.99)' },
        horizon: { type: 'number', description: 'Holding period in days' },
      },
      required: ['portfolio_value', 'volatility'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/var/parametric', {
          portfolio_value: args.portfolio_value,
          volatility: args.volatility,
          confidence: args.confidence || 0.99,
          horizon: args.horizon || 1,
        }, apiKey);
        const conf = (args.confidence || 0.99) * 100;
        return {
          success: true,
          data: result,
          message: `Parametric VaR (${conf}%, ${args.horizon || 1}d): ${formatNumber(result.var || result.VaR, 2)} (${formatPercent((result.var || result.VaR) / args.portfolio_value)} of portfolio)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_var_historical',
    description: 'Calculate historical simulation Value at Risk',
    inputSchema: {
      type: 'object',
      properties: {
        returns: { type: 'string', description: 'JSON array of historical returns' },
        confidence: { type: 'number', description: 'Confidence level (e.g., 0.95, 0.99)' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
      },
      required: ['returns'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/var/historical', {
          returns: JSON.parse(args.returns),
          confidence: args.confidence || 0.99,
          portfolio_value: args.portfolio_value || 1000000,
        }, apiKey);
        const conf = (args.confidence || 0.99) * 100;
        return {
          success: true,
          data: result,
          message: `Historical VaR (${conf}%): ${formatNumber(result.var || result.VaR, 2)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_var_component',
    description: 'Calculate component VaR - contribution of each factor to total VaR',
    inputSchema: {
      type: 'object',
      properties: {
        returns_by_factor: { type: 'string', description: 'JSON object with factor names as keys and return arrays as values' },
        weights: { type: 'string', description: 'JSON object with factor names as keys and weights as values' },
        confidence: { type: 'number', description: 'Confidence level' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
      },
      required: ['returns_by_factor', 'weights'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/var/component', {
          returns_by_factor: JSON.parse(args.returns_by_factor),
          weights: JSON.parse(args.weights),
          confidence: args.confidence || 0.99,
          portfolio_value: args.portfolio_value || 1000000,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Component VaR calculated for ${Object.keys(JSON.parse(args.weights)).length} factors`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_var_incremental',
    description: 'Calculate incremental VaR - change in VaR from adding a new position',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_returns: { type: 'string', description: 'JSON array of portfolio returns' },
        new_position_returns: { type: 'string', description: 'JSON array of new position returns' },
        position_weight: { type: 'number', description: 'Weight of new position (0 to 1)' },
        confidence: { type: 'number', description: 'Confidence level' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
      },
      required: ['portfolio_returns', 'new_position_returns', 'position_weight'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/var/incremental', {
          portfolio_returns: JSON.parse(args.portfolio_returns),
          new_position_returns: JSON.parse(args.new_position_returns),
          position_weight: args.position_weight,
          confidence: args.confidence || 0.99,
          portfolio_value: args.portfolio_value || 1000000,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Incremental VaR: ${formatNumber(result.incremental_var, 2)} (${formatPercent(args.position_weight)} weight)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_var_marginal',
    description: 'Calculate marginal VaR - sensitivity of VaR to position size changes',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_returns: { type: 'string', description: 'JSON array of portfolio returns' },
        new_position_returns: { type: 'string', description: 'JSON array of position returns' },
        position_weight: { type: 'number', description: 'Current position weight' },
        confidence: { type: 'number', description: 'Confidence level' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
      },
      required: ['portfolio_returns', 'new_position_returns', 'position_weight'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/var/marginal', {
          portfolio_returns: JSON.parse(args.portfolio_returns),
          new_position_returns: JSON.parse(args.new_position_returns),
          position_weight: args.position_weight,
          confidence: args.confidence || 0.99,
          portfolio_value: args.portfolio_value || 1000000,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Marginal VaR: ${formatNumber(result.marginal_var, 4)} per unit weight change`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_var_es_optimization',
    description: 'Optimize portfolio to minimize Expected Shortfall (CVaR)',
    inputSchema: {
      type: 'object',
      properties: {
        returns: { type: 'string', description: 'JSON 2D array of asset returns (each row is an asset)' },
        confidence: { type: 'number', description: 'Confidence level for ES' },
        target_return: { type: 'number', description: 'Target portfolio return (optional)' },
      },
      required: ['returns'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/var/es-optimization', {
          returns: JSON.parse(args.returns),
          confidence: args.confidence || 0.95,
          target_return: args.target_return,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `ES-optimized weights: min ES = ${formatNumber(result.min_es || result.expected_shortfall, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== STRESS & BACKTEST (2) ====================
  {
    name: 'quantlib_risk_stress_scenario',
    description: 'Define and calculate impact of a stress scenario',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Scenario name (e.g., "2008 Crisis", "Rate Shock")' },
        factor_shocks: { type: 'string', description: 'JSON object with risk factors and their shock magnitudes' },
        description: { type: 'string', description: 'Scenario description' },
      },
      required: ['name', 'factor_shocks'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/stress/scenario', {
          name: args.name,
          factor_shocks: JSON.parse(args.factor_shocks),
          description: args.description || '',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Stress scenario "${args.name}" applied: ${Object.keys(JSON.parse(args.factor_shocks)).length} factors shocked`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_backtest',
    description: 'Backtest VaR model against actual returns',
    inputSchema: {
      type: 'object',
      properties: {
        actual_returns: { type: 'string', description: 'JSON array of actual realized returns' },
        var_estimates: { type: 'string', description: 'JSON array of VaR estimates for same periods' },
        confidence: { type: 'number', description: 'VaR confidence level used' },
      },
      required: ['actual_returns', 'var_estimates'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/backtest', {
          actual_returns: JSON.parse(args.actual_returns),
          var_estimates: JSON.parse(args.var_estimates),
          confidence: args.confidence || 0.99,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `VaR Backtest: ${result.exceptions || result.breaches} exceptions in ${JSON.parse(args.actual_returns).length} periods (expected: ${formatNumber((1 - (args.confidence || 0.99)) * JSON.parse(args.actual_returns).length, 1)})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== EVT (3) ====================
  {
    name: 'quantlib_risk_evt_gpd',
    description: 'Fit Generalized Pareto Distribution (GPD) to tail losses',
    inputSchema: {
      type: 'object',
      properties: {
        data: { type: 'string', description: 'JSON array of loss data' },
        threshold: { type: 'number', description: 'Threshold for tail definition (optional)' },
        quantile: { type: 'number', description: 'Quantile to use for threshold if not specified' },
      },
      required: ['data'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/evt/gpd', {
          data: JSON.parse(args.data),
          threshold: args.threshold,
          quantile: args.quantile || 0.95,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `GPD fit: ξ=${formatNumber(result.xi || result.shape, 4)}, β=${formatNumber(result.beta || result.scale, 4)}, threshold=${formatNumber(result.threshold, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_evt_gev',
    description: 'Fit Generalized Extreme Value (GEV) distribution to block maxima',
    inputSchema: {
      type: 'object',
      properties: {
        data: { type: 'string', description: 'JSON array of block maxima data' },
        fit_method: { type: 'string', description: 'Fitting method', enum: ['mle', 'pwm', 'lmom'] },
        return_periods: { type: 'string', description: 'JSON array of return periods to calculate' },
      },
      required: ['data'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/evt/gev', {
          data: JSON.parse(args.data),
          fit_method: args.fit_method || 'mle',
          return_periods: args.return_periods ? JSON.parse(args.return_periods) : undefined,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `GEV fit: μ=${formatNumber(result.mu || result.location, 4)}, σ=${formatNumber(result.sigma || result.scale, 4)}, ξ=${formatNumber(result.xi || result.shape, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_evt_hill',
    description: 'Estimate tail index using Hill estimator',
    inputSchema: {
      type: 'object',
      properties: {
        data: { type: 'string', description: 'JSON array of data' },
        threshold: { type: 'number', description: 'Threshold for tail' },
        quantile: { type: 'number', description: 'Quantile for threshold selection' },
      },
      required: ['data'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/evt/hill', {
          data: JSON.parse(args.data),
          threshold: args.threshold,
          quantile: args.quantile || 0.95,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Hill estimator: α=${formatNumber(result.alpha || result.hill_index, 4)} (tail index)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== TAIL RISK (2) ====================
  {
    name: 'quantlib_risk_tail_comprehensive',
    description: 'Comprehensive tail risk analysis including VaR, ES, and EVT metrics',
    inputSchema: {
      type: 'object',
      properties: {
        returns: { type: 'string', description: 'JSON array of returns' },
        confidence: { type: 'number', description: 'Confidence level' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
      },
      required: ['returns'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/tail-risk/comprehensive', {
          returns: JSON.parse(args.returns),
          confidence: args.confidence || 0.99,
          portfolio_value: args.portfolio_value || 1000000,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Tail Risk: VaR=${formatNumber(result.var, 2)}, ES=${formatNumber(result.es || result.expected_shortfall, 2)}, Skew=${formatNumber(result.skewness, 4)}, Kurt=${formatNumber(result.kurtosis, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_tail_dependence',
    description: 'Calculate tail dependence between assets',
    inputSchema: {
      type: 'object',
      properties: {
        returns_x: { type: 'string', description: 'JSON array of first asset returns' },
        returns_y: { type: 'string', description: 'JSON array of second asset returns' },
        quantile: { type: 'number', description: 'Tail quantile (e.g., 0.05 for 5% tail)' },
      },
      required: ['returns_x', 'returns_y'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/tail-risk/tail-dependence', {
          returns_x: JSON.parse(args.returns_x),
          returns_y: JSON.parse(args.returns_y),
          quantile: args.quantile || 0.05,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Tail dependence: lower=${formatNumber(result.lower_tail, 4)}, upper=${formatNumber(result.upper_tail, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== XVA (2) ====================
  {
    name: 'quantlib_risk_xva_cva',
    description: 'Calculate Credit Valuation Adjustment (CVA) and related XVA metrics',
    inputSchema: {
      type: 'object',
      properties: {
        exposure_paths: { type: 'string', description: 'JSON 2D array of exposure simulation paths' },
        counterparty_spread: { type: 'number', description: 'Counterparty credit spread (decimal)' },
        recovery_rate: { type: 'number', description: 'Expected recovery rate (decimal)' },
        own_spread: { type: 'number', description: 'Own credit spread for DVA' },
        funding_spread: { type: 'number', description: 'Funding spread for FVA' },
      },
      required: ['exposure_paths', 'counterparty_spread'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/xva/cva', {
          exposure_paths: JSON.parse(args.exposure_paths),
          counterparty_spread: args.counterparty_spread,
          recovery_rate: args.recovery_rate || 0.4,
          own_spread: args.own_spread,
          funding_spread: args.funding_spread,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `CVA: ${formatNumber(result.cva, 4)}, DVA: ${formatNumber(result.dva || 0, 4)}, FVA: ${formatNumber(result.fva || 0, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_xva_pfe',
    description: 'Calculate Potential Future Exposure (PFE) profile',
    inputSchema: {
      type: 'object',
      properties: {
        exposure_paths: { type: 'string', description: 'JSON 2D array of exposure simulation paths' },
        counterparty_spread: { type: 'number', description: 'Counterparty credit spread' },
        recovery_rate: { type: 'number', description: 'Recovery rate' },
      },
      required: ['exposure_paths', 'counterparty_spread'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/xva/pfe', {
          exposure_paths: JSON.parse(args.exposure_paths),
          counterparty_spread: args.counterparty_spread,
          recovery_rate: args.recovery_rate || 0.4,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `PFE profile: max PFE = ${formatNumber(result.max_pfe || Math.max(...(result.pfe_profile || [0])), 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== CORRELATION STRESS (1) ====================
  {
    name: 'quantlib_risk_correlation_stress',
    description: 'Apply stress scenarios to correlation matrix',
    inputSchema: {
      type: 'object',
      properties: {
        correlation_matrix: { type: 'string', description: 'JSON 2D array correlation matrix' },
        stress_type: { type: 'string', description: 'Stress type', enum: ['crisis', 'decorrelation', 'custom'] },
        volatilities: { type: 'string', description: 'JSON array of asset volatilities' },
        weights: { type: 'string', description: 'JSON array of portfolio weights' },
      },
      required: ['correlation_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/correlation-stress', {
          correlation_matrix: JSON.parse(args.correlation_matrix),
          stress_type: args.stress_type || 'crisis',
          volatilities: args.volatilities ? JSON.parse(args.volatilities) : undefined,
          weights: args.weights ? JSON.parse(args.weights) : undefined,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Correlation stress (${args.stress_type || 'crisis'}): portfolio vol change = ${formatPercent(result.vol_change || 0)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== SENSITIVITIES (6) ====================
  {
    name: 'quantlib_risk_sensitivities_greeks',
    description: 'Calculate option Greeks for risk management',
    inputSchema: {
      type: 'object',
      properties: {
        S: { type: 'number', description: 'Spot price' },
        K: { type: 'number', description: 'Strike price' },
        T: { type: 'number', description: 'Time to maturity (years)' },
        r: { type: 'number', description: 'Risk-free rate' },
        sigma: { type: 'number', description: 'Volatility' },
        q: { type: 'number', description: 'Dividend yield' },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/sensitivities/greeks', {
          S: args.S || 100,
          K: args.K || 100,
          T: args.T || 1,
          r: args.r || 0.05,
          sigma: args.sigma || 0.2,
          q: args.q || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Greeks: Δ=${formatNumber(result.delta, 4)}, Γ=${formatNumber(result.gamma, 6)}, V=${formatNumber(result.vega, 4)}, Θ=${formatNumber(result.theta, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_sensitivities_bucket_delta',
    description: 'Calculate bucket delta sensitivities for rate curve',
    inputSchema: {
      type: 'object',
      properties: {
        positions: { type: 'string', description: 'JSON array of position objects with tenor and notional' },
        bump_size: { type: 'number', description: 'Bump size in decimal (e.g., 0.0001 for 1bp)' },
      },
      required: ['positions'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/sensitivities/bucket-delta', {
          positions: JSON.parse(args.positions),
          bump_size: args.bump_size || 0.0001,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Bucket deltas calculated for ${JSON.parse(args.positions).length} positions`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_sensitivities_cross_gamma',
    description: 'Calculate cross-gamma (spot-vol correlation risk)',
    inputSchema: {
      type: 'object',
      properties: {
        S: { type: 'number', description: 'Spot price' },
        K: { type: 'number', description: 'Strike price' },
        T: { type: 'number', description: 'Time to maturity' },
        r: { type: 'number', description: 'Risk-free rate' },
        sigma: { type: 'number', description: 'Volatility' },
        bump: { type: 'number', description: 'Bump size for finite difference' },
      },
      required: [],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/sensitivities/cross-gamma', {
          S: args.S || 100,
          K: args.K || 100,
          T: args.T || 1,
          r: args.r || 0.05,
          sigma: args.sigma || 0.2,
          bump: args.bump || 0.01,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Cross-Gamma (Vanna): ${formatNumber(result.cross_gamma || result.vanna, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_sensitivities_key_rate_duration',
    description: 'Calculate key rate durations for a cashflow stream',
    inputSchema: {
      type: 'object',
      properties: {
        cashflows: { type: 'string', description: 'JSON array of cashflow amounts' },
        times: { type: 'string', description: 'JSON array of cashflow times (years)' },
        curve_rates: { type: 'string', description: 'JSON array of zero rates at each tenor' },
        bump_size: { type: 'number', description: 'Rate bump size' },
      },
      required: ['cashflows', 'times', 'curve_rates'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/sensitivities/key-rate-duration', {
          cashflows: JSON.parse(args.cashflows),
          times: JSON.parse(args.times),
          curve_rates: JSON.parse(args.curve_rates),
          bump_size: args.bump_size || 0.0001,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Key rate durations calculated for ${JSON.parse(args.times).length} tenors`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_sensitivities_parallel_shift',
    description: 'Calculate PV01/DV01 - sensitivity to parallel rate shift',
    inputSchema: {
      type: 'object',
      properties: {
        cashflows: { type: 'string', description: 'JSON array of cashflow amounts' },
        times: { type: 'string', description: 'JSON array of cashflow times' },
        curve_rates: { type: 'string', description: 'JSON array of zero rates' },
        bump_size: { type: 'number', description: 'Parallel shift size' },
      },
      required: ['cashflows', 'times', 'curve_rates'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/sensitivities/parallel-shift', {
          cashflows: JSON.parse(args.cashflows),
          times: JSON.parse(args.times),
          curve_rates: JSON.parse(args.curve_rates),
          bump_size: args.bump_size || 0.0001,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `DV01: ${formatNumber(result.dv01 || result.pv01, 4)} per 1bp parallel shift`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_sensitivities_twist',
    description: 'Calculate sensitivity to curve twist (steepening/flattening)',
    inputSchema: {
      type: 'object',
      properties: {
        cashflows: { type: 'string', description: 'JSON array of cashflow amounts' },
        times: { type: 'string', description: 'JSON array of cashflow times' },
        curve_rates: { type: 'string', description: 'JSON array of zero rates' },
        bump_size: { type: 'number', description: 'Twist magnitude' },
      },
      required: ['cashflows', 'times', 'curve_rates'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/sensitivities/twist', {
          cashflows: JSON.parse(args.cashflows),
          times: JSON.parse(args.times),
          curve_rates: JSON.parse(args.curve_rates),
          bump_size: args.bump_size || 0.0001,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Twist sensitivity: ${formatNumber(result.twist_sensitivity, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== PORTFOLIO RISK (2) ====================
  {
    name: 'quantlib_risk_portfolio_optimal_hedge',
    description: 'Calculate optimal hedge ratios to neutralize delta and gamma',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_delta: { type: 'number', description: 'Current portfolio delta' },
        hedge_delta: { type: 'number', description: 'Hedge instrument delta' },
        portfolio_gamma: { type: 'number', description: 'Current portfolio gamma' },
        hedge_gamma: { type: 'number', description: 'Hedge instrument gamma' },
        target_delta: { type: 'number', description: 'Target portfolio delta' },
        target_gamma: { type: 'number', description: 'Target portfolio gamma' },
      },
      required: ['portfolio_delta', 'hedge_delta'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/portfolio-risk/optimal-hedge', {
          portfolio_delta: args.portfolio_delta,
          hedge_delta: args.hedge_delta,
          portfolio_gamma: args.portfolio_gamma || 0,
          hedge_gamma: args.hedge_gamma || 0,
          target_delta: args.target_delta || 0,
          target_gamma: args.target_gamma || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Optimal hedge ratio: ${formatNumber(result.hedge_ratio || result.ratio, 4)} units`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_risk_portfolio_exposure_profile',
    description: 'Calculate exposure profile from simulation paths',
    inputSchema: {
      type: 'object',
      properties: {
        paths: { type: 'string', description: 'JSON 2D array of simulation paths' },
        confidence: { type: 'number', description: 'Confidence level for PFE' },
      },
      required: ['paths'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/risk/portfolio-risk/exposure-profile', {
          paths: JSON.parse(args.paths),
          confidence: args.confidence || 0.95,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Exposure profile: EPE=${formatNumber(result.expected_positive_exposure || result.epe, 4)}, max PFE=${formatNumber(result.max_pfe, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
];
