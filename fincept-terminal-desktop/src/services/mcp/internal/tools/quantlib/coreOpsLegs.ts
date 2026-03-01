// QuantLib Core MCP Tools - Operations, Legs, Periods (24 tools)

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber } from '../../QuantLibMCPBridge';

export const quantlibCoreOpsLegsTools: InternalTool[] = [
  // ==================== OPERATIONS ====================
  {
    name: 'quantlib_ops_discount_cashflows',
    description: 'Calculate present value of a series of cashflows using discount factors',
    inputSchema: {
      type: 'object',
      properties: {
        cashflows: { type: 'string', description: 'JSON array of cashflow amounts (e.g., "[100, 100, 1100]")' },
        times: { type: 'string', description: 'JSON array of times in years (e.g., "[1, 2, 3]")' },
        discount_factors: { type: 'string', description: 'JSON array of discount factors (e.g., "[0.95, 0.90, 0.85]")' },
      },
      required: ['cashflows', 'times', 'discount_factors'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/discount-cashflows', {
          cashflows: JSON.parse(args.cashflows),
          times: JSON.parse(args.times),
          discount_factors: JSON.parse(args.discount_factors),
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Present Value: ${formatNumber(result.pv || result.present_value, 2)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_black_scholes',
    description: 'Calculate Black-Scholes option price and Greeks',
    inputSchema: {
      type: 'object',
      properties: {
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        time: { type: 'number', description: 'Time to maturity in years' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
      },
      required: ['option_type', 'spot', 'strike', 'rate', 'time', 'volatility'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/black-scholes', args, apiKey);
        return {
          success: true,
          data: result,
          message: `BS ${args.option_type}: price=${formatNumber(result.price, 4)}, delta=${formatNumber(result.delta, 4)}, gamma=${formatNumber(result.gamma, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_black76',
    description: 'Calculate Black76 option price for futures/forwards',
    inputSchema: {
      type: 'object',
      properties: {
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time: { type: 'number', description: 'Time to maturity in years' },
        discount_factor: { type: 'number', description: 'Discount factor' },
      },
      required: ['option_type', 'forward', 'strike', 'volatility', 'time', 'discount_factor'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/black76', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Black76 ${args.option_type}: price=${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_var',
    description: 'Calculate Value at Risk using various methods',
    inputSchema: {
      type: 'object',
      properties: {
        method: { type: 'string', description: 'VaR method', enum: ['parametric', 'historical', 'monte_carlo'] },
        returns: { type: 'string', description: 'JSON array of historical returns (for historical method)' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
        portfolio_std: { type: 'number', description: 'Portfolio standard deviation (for parametric)' },
        confidence: { type: 'number', description: 'Confidence level (e.g., 0.95, 0.99)' },
        holding_period: { type: 'number', description: 'Holding period in days' },
      },
      required: ['method'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const body: any = { method: args.method };
        if (args.returns) body.returns = JSON.parse(args.returns);
        if (args.portfolio_value !== undefined) body.portfolio_value = args.portfolio_value;
        if (args.portfolio_std !== undefined) body.portfolio_std = args.portfolio_std;
        if (args.confidence !== undefined) body.confidence = args.confidence;
        if (args.holding_period !== undefined) body.holding_period = args.holding_period;

        const result = await quantlibApiCall('/quantlib/core/ops/var', body, apiKey);
        return {
          success: true,
          data: result,
          message: `VaR (${args.method}): ${formatNumber(result.var || result.VaR, 2)} at ${(args.confidence || 0.95) * 100}% confidence`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_interpolate',
    description: 'Interpolate a value from data points using various methods',
    inputSchema: {
      type: 'object',
      properties: {
        method: { type: 'string', description: 'Interpolation method', enum: ['linear', 'cubic', 'log_linear', 'flat_forward'] },
        x: { type: 'number', description: 'Point at which to interpolate' },
        x_data: { type: 'string', description: 'JSON array of x data points' },
        y_data: { type: 'string', description: 'JSON array of y data points' },
      },
      required: ['method', 'x', 'x_data', 'y_data'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/interpolate', {
          method: args.method,
          x: args.x,
          x_data: JSON.parse(args.x_data),
          y_data: JSON.parse(args.y_data),
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Interpolated value at x=${args.x}: ${formatNumber(result.y || result.result, 6)} (${args.method})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_forward_rate',
    description: 'Calculate forward rate from two discount factors',
    inputSchema: {
      type: 'object',
      properties: {
        df1: { type: 'number', description: 'First discount factor' },
        df2: { type: 'number', description: 'Second discount factor' },
        t1: { type: 'number', description: 'First time point (years)' },
        t2: { type: 'number', description: 'Second time point (years)' },
      },
      required: ['df1', 'df2', 't1', 't2'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/forward-rate', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Forward rate [${args.t1}, ${args.t2}]: ${formatNumber(result.forward_rate || result.rate, 6)} (${formatNumber((result.forward_rate || result.rate) * 100, 4)}%)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_zero_rate_convert',
    description: 'Convert between zero rate and discount factor',
    inputSchema: {
      type: 'object',
      properties: {
        direction: { type: 'string', description: 'Conversion direction', enum: ['rate_to_df', 'df_to_rate'] },
        value: { type: 'number', description: 'Input value (rate or discount factor)' },
        t: { type: 'number', description: 'Time in years' },
      },
      required: ['direction', 'value', 't'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/zero-rate-convert', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Conversion (${args.direction}): ${args.value} → ${formatNumber(result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_gbm_paths',
    description: 'Generate Geometric Brownian Motion simulation paths',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Initial spot price' },
        drift: { type: 'number', description: 'Drift rate (mu)' },
        volatility: { type: 'number', description: 'Volatility (sigma)' },
        time: { type: 'number', description: 'Time horizon in years' },
        n_paths: { type: 'number', description: 'Number of simulation paths' },
        n_steps: { type: 'number', description: 'Number of time steps per path' },
        seed: { type: 'number', description: 'Random seed for reproducibility' },
        antithetic: { type: 'boolean', description: 'Use antithetic variates' },
      },
      required: ['spot', 'drift', 'volatility', 'time'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/gbm-paths', args, apiKey);
        const nPaths = args.n_paths || 1000;
        const nSteps = args.n_steps || 100;
        return {
          success: true,
          data: result,
          message: `Generated ${nPaths} GBM paths with ${nSteps} steps (S0=${args.spot}, μ=${args.drift}, σ=${args.volatility})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_cholesky',
    description: 'Compute Cholesky decomposition of a positive-definite matrix',
    inputSchema: {
      type: 'object',
      properties: {
        matrix: { type: 'string', description: 'JSON 2D array representing the matrix (e.g., "[[1,0.5],[0.5,1]]")' },
      },
      required: ['matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/cholesky', {
          matrix: JSON.parse(args.matrix),
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Cholesky decomposition computed successfully`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_covariance_matrix',
    description: 'Compute covariance matrix from a matrix of returns',
    inputSchema: {
      type: 'object',
      properties: {
        returns: { type: 'string', description: 'JSON 2D array of returns (each row is an asset, each column is a time period)' },
      },
      required: ['returns'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/covariance-matrix', {
          returns: JSON.parse(args.returns),
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Covariance matrix computed (${result.covariance_matrix?.length || 'N/A'} x ${result.covariance_matrix?.[0]?.length || 'N/A'})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_statistics',
    description: 'Compute descriptive statistics for a series of values',
    inputSchema: {
      type: 'object',
      properties: {
        values: { type: 'string', description: 'JSON array of numeric values' },
        ddof: { type: 'number', description: 'Delta degrees of freedom for std dev calculation' },
      },
      required: ['values'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/statistics', {
          values: JSON.parse(args.values),
          ddof: args.ddof,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Statistics: mean=${formatNumber(result.mean, 4)}, std=${formatNumber(result.std, 4)}, min=${formatNumber(result.min, 4)}, max=${formatNumber(result.max, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_ops_percentile',
    description: 'Calculate percentile of a data series',
    inputSchema: {
      type: 'object',
      properties: {
        values: { type: 'string', description: 'JSON array of numeric values' },
        p: { type: 'number', description: 'Percentile to calculate (0 to 100)' },
      },
      required: ['values', 'p'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/ops/percentile', {
          values: JSON.parse(args.values),
          p: args.p,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.p}th percentile: ${formatNumber(result.percentile || result.result, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== LEGS ====================
  {
    name: 'quantlib_legs_fixed',
    description: 'Generate a fixed-rate leg (series of fixed coupon payments)',
    inputSchema: {
      type: 'object',
      properties: {
        notional: { type: 'number', description: 'Notional amount' },
        rate: { type: 'number', description: 'Fixed coupon rate (decimal)' },
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        frequency: { type: 'string', description: 'Payment frequency', enum: ['annual', 'semiannual', 'quarterly', 'monthly'] },
        day_count: { type: 'string', description: 'Day count convention', enum: ['ACT/360', 'ACT/365', '30/360', 'ACT/ACT'] },
        currency: { type: 'string', description: 'Currency code' },
      },
      required: ['notional', 'rate', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/legs/fixed', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Fixed leg: ${formatNumber(args.notional, 0)} @ ${formatNumber(args.rate * 100, 2)}% (${args.frequency || 'annual'})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_legs_float',
    description: 'Generate a floating-rate leg (series of floating coupon payments)',
    inputSchema: {
      type: 'object',
      properties: {
        notional: { type: 'number', description: 'Notional amount' },
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        spread: { type: 'number', description: 'Spread over index (decimal)' },
        frequency: { type: 'string', description: 'Payment frequency' },
        day_count: { type: 'string', description: 'Day count convention' },
        index_name: { type: 'string', description: 'Reference index (e.g., "SOFR", "EURIBOR")' },
      },
      required: ['notional', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/legs/float', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Float leg: ${formatNumber(args.notional, 0)} + ${formatNumber((args.spread || 0) * 10000, 0)}bps spread`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_legs_zero_coupon',
    description: 'Generate a zero-coupon leg (single payment at maturity)',
    inputSchema: {
      type: 'object',
      properties: {
        notional: { type: 'number', description: 'Notional amount' },
        rate: { type: 'number', description: 'Zero rate (decimal)' },
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        day_count: { type: 'string', description: 'Day count convention' },
      },
      required: ['notional', 'rate', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/legs/zero-coupon', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Zero-coupon leg: ${formatNumber(args.notional, 0)} @ ${formatNumber(args.rate * 100, 2)}%`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== PERIODS ====================
  {
    name: 'quantlib_periods_fixed_coupon',
    description: 'Calculate a single fixed coupon period',
    inputSchema: {
      type: 'object',
      properties: {
        notional: { type: 'number', description: 'Notional amount' },
        rate: { type: 'number', description: 'Coupon rate (decimal)' },
        start_date: { type: 'string', description: 'Period start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'Period end date (YYYY-MM-DD)' },
        day_count: { type: 'string', description: 'Day count convention' },
      },
      required: ['notional', 'rate', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/periods/fixed-coupon', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Fixed coupon: ${formatNumber(result.coupon || result.payment, 2)} (${args.start_date} to ${args.end_date})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_periods_float_coupon',
    description: 'Calculate a single floating coupon period',
    inputSchema: {
      type: 'object',
      properties: {
        notional: { type: 'number', description: 'Notional amount' },
        start_date: { type: 'string', description: 'Period start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'Period end date (YYYY-MM-DD)' },
        spread: { type: 'number', description: 'Spread over index (decimal)' },
        day_count: { type: 'string', description: 'Day count convention' },
        fixing_rate: { type: 'number', description: 'Fixed rate for the period (if known)' },
      },
      required: ['notional', 'start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/periods/float-coupon', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Float coupon: ${formatNumber(result.coupon || result.payment, 2)} (${args.start_date} to ${args.end_date})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_periods_day_count_fraction',
    description: 'Calculate the day count fraction between two dates',
    inputSchema: {
      type: 'object',
      properties: {
        start_date: { type: 'string', description: 'Start date (YYYY-MM-DD)' },
        end_date: { type: 'string', description: 'End date (YYYY-MM-DD)' },
        convention: { type: 'string', description: 'Day count convention', enum: ['ACT/360', 'ACT/365', '30/360', 'ACT/ACT'] },
      },
      required: ['start_date', 'end_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/periods/day-count-fraction', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Day count fraction (${args.convention || 'ACT/365'}): ${formatNumber(result.fraction || result.dcf, 6)} years`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
];
