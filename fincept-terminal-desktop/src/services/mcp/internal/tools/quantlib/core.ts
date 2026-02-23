// QuantLib Core MCP Tools - 51 endpoints
// Types, Conventions, AutoDiff, Distributions, Math, Operations, Legs, Periods

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber } from '../../QuantLibMCPBridge';

export const quantlibCoreTools: InternalTool[] = [
  // ==================== TYPES ====================
  {
    name: 'quantlib_types_tenor_add_to_date',
    description: 'Add a tenor (e.g., "3M", "1Y", "6M") to a date and get the resulting date',
    inputSchema: {
      type: 'object',
      properties: {
        tenor: { type: 'string', description: 'Tenor string (e.g., "3M", "1Y", "6M", "2W")' },
        start_date: { type: 'string', description: 'Start date in YYYY-MM-DD format' },
      },
      required: ['tenor', 'start_date'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/tenor/add-to-date', {
          tenor: args.tenor,
          start_date: args.start_date,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.start_date} + ${args.tenor} = ${result.result_date || result.end_date}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_rate_convert',
    description: 'Convert interest rate between different units (decimal, percent, bps)',
    inputSchema: {
      type: 'object',
      properties: {
        value: { type: 'number', description: 'Rate value to convert' },
        unit: { type: 'string', description: 'Target unit', enum: ['decimal', 'percent', 'bps'] },
      },
      required: ['value'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/rate/convert', {
          value: args.value,
          unit: args.unit,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Rate conversion: ${args.value} → ${JSON.stringify(result)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_money_create',
    description: 'Create a money object with amount and currency',
    inputSchema: {
      type: 'object',
      properties: {
        amount: { type: 'number', description: 'Monetary amount' },
        currency: { type: 'string', description: 'Currency code (e.g., USD, EUR, GBP)' },
      },
      required: ['amount'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/money/create', {
          amount: args.amount,
          currency: args.currency || 'USD',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Created: ${args.currency || 'USD'} ${formatNumber(args.amount, 2)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_money_convert',
    description: 'Convert money from one currency to another using a given exchange rate',
    inputSchema: {
      type: 'object',
      properties: {
        amount: { type: 'number', description: 'Amount to convert' },
        from_currency: { type: 'string', description: 'Source currency code' },
        to_currency: { type: 'string', description: 'Target currency code' },
        rate: { type: 'number', description: 'Exchange rate (from_currency to to_currency)' },
      },
      required: ['amount', 'from_currency', 'to_currency', 'rate'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/money/convert', args, apiKey);
        const converted = args.amount * args.rate;
        return {
          success: true,
          data: result,
          message: `${args.from_currency} ${formatNumber(args.amount, 2)} → ${args.to_currency} ${formatNumber(converted, 2)} @ rate ${args.rate}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_spread_from_bps',
    description: 'Convert basis points to a spread value',
    inputSchema: {
      type: 'object',
      properties: {
        bps: { type: 'number', description: 'Spread in basis points (e.g., 50 for 50bps)' },
      },
      required: ['bps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/spread/from-bps', { bps: args.bps }, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.bps} bps = ${formatNumber(args.bps / 10000, 6)} decimal`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_notional_schedule',
    description: 'Generate a notional amortization schedule (bullet, linear, custom)',
    inputSchema: {
      type: 'object',
      properties: {
        schedule_type: { type: 'string', description: 'Schedule type', enum: ['bullet', 'linear', 'custom'] },
        notional: { type: 'number', description: 'Initial notional amount' },
        periods: { type: 'number', description: 'Number of periods' },
        rate: { type: 'number', description: 'Amortization rate (for custom type)' },
      },
      required: ['schedule_type', 'notional', 'periods'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/notional-schedule', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Generated ${args.schedule_type} schedule: ${args.periods} periods, initial notional ${formatNumber(args.notional, 0)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_list_currencies',
    description: 'Get list of all supported currency codes',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/currencies', {}, apiKey, 'GET');
        return {
          success: true,
          data: result,
          message: `Available currencies: ${Array.isArray(result) ? result.length : 'N/A'} codes`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_types_list_frequencies',
    description: 'Get list of all supported payment frequencies',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/types/frequencies', {}, apiKey, 'GET');
        return {
          success: true,
          data: result,
          message: `Available frequencies: ${Array.isArray(result) ? result.join(', ') : JSON.stringify(result)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== CONVENTIONS ====================
  {
    name: 'quantlib_conventions_format_date',
    description: 'Format a date string according to a specified format',
    inputSchema: {
      type: 'object',
      properties: {
        date_str: { type: 'string', description: 'Date string to format' },
        format: { type: 'string', description: 'Output format (e.g., "YYYY-MM-DD", "DD/MM/YYYY")' },
      },
      required: ['date_str'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/conventions/format-date', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Formatted: ${result.formatted || result.result}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_conventions_parse_date',
    description: 'Parse a date string into a standardized format',
    inputSchema: {
      type: 'object',
      properties: {
        date_string: { type: 'string', description: 'Date string to parse' },
      },
      required: ['date_string'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/conventions/parse-date', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Parsed: ${result.parsed || result.date}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_conventions_days_to_years',
    description: 'Convert number of days to years using a day count convention',
    inputSchema: {
      type: 'object',
      properties: {
        value: { type: 'number', description: 'Number of days' },
        convention: { type: 'string', description: 'Day count convention (e.g., "ACT/360", "ACT/365", "30/360")' },
      },
      required: ['value'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/conventions/days-to-years', args, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.value} days = ${formatNumber(result.years || result.result, 6)} years (${args.convention || 'default'})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_conventions_years_to_days',
    description: 'Convert years to number of days using a day count convention',
    inputSchema: {
      type: 'object',
      properties: {
        value: { type: 'number', description: 'Number of years' },
        convention: { type: 'string', description: 'Day count convention' },
      },
      required: ['value'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/conventions/years-to-days', args, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.value} years = ${formatNumber(result.days || result.result, 0)} days`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_conventions_normalize_rate',
    description: 'Normalize an interest rate to decimal format',
    inputSchema: {
      type: 'object',
      properties: {
        value: { type: 'number', description: 'Rate value' },
        unit: { type: 'string', description: 'Input unit', enum: ['decimal', 'percent', 'bps'] },
      },
      required: ['value'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/conventions/normalize-rate', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Normalized rate: ${formatNumber(result.normalized || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_conventions_normalize_volatility',
    description: 'Normalize volatility to decimal format',
    inputSchema: {
      type: 'object',
      properties: {
        value: { type: 'number', description: 'Volatility value' },
        vol_type: { type: 'string', description: 'Volatility type', enum: ['lognormal', 'normal'] },
        is_percent: { type: 'boolean', description: 'Whether input is in percent' },
      },
      required: ['value'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/conventions/normalize-volatility', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Normalized volatility: ${formatNumber(result.normalized || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== AUTODIFF ====================
  {
    name: 'quantlib_autodiff_dual_eval',
    description: 'Evaluate a function and its derivative using automatic differentiation (dual numbers)',
    inputSchema: {
      type: 'object',
      properties: {
        func_name: { type: 'string', description: 'Function name', enum: ['exp', 'log', 'sin', 'cos', 'sqrt', 'power'] },
        x: { type: 'number', description: 'Point at which to evaluate' },
        power_n: { type: 'number', description: 'Power exponent (for power function)' },
      },
      required: ['func_name', 'x'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/autodiff/dual-eval', args, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.func_name}(${args.x}): value=${formatNumber(result.value, 6)}, derivative=${formatNumber(result.derivative, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_autodiff_gradient',
    description: 'Compute gradient of a multivariate function using automatic differentiation',
    inputSchema: {
      type: 'object',
      properties: {
        func_name: { type: 'string', description: 'Function name', enum: ['sum_squares', 'rosenbrock', 'sphere'] },
        x: { type: 'string', description: 'JSON array of input values (e.g., "[1.0, 2.0, 3.0]")' },
      },
      required: ['func_name', 'x'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const xValues = JSON.parse(args.x);
        const result = await quantlibApiCall('/quantlib/core/autodiff/gradient', {
          func_name: args.func_name,
          x: xValues,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Gradient of ${args.func_name}: [${result.gradient?.map((v: number) => formatNumber(v, 6)).join(', ') || 'N/A'}]`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_autodiff_taylor_expand',
    description: 'Compute Taylor series expansion of a function around a point',
    inputSchema: {
      type: 'object',
      properties: {
        func_name: { type: 'string', description: 'Function name', enum: ['exp', 'log', 'sin', 'cos'] },
        x0: { type: 'number', description: 'Point of expansion' },
        order: { type: 'number', description: 'Order of Taylor expansion' },
      },
      required: ['func_name', 'x0'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/autodiff/taylor-expand', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Taylor expansion of ${args.func_name} around x=${args.x0} (order ${args.order || 5})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== DISTRIBUTIONS ====================
  {
    name: 'quantlib_dist_normal_cdf',
    description: 'Calculate cumulative distribution function (CDF) of standard normal distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate CDF' },
      },
      required: ['x'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/normal/cdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `N(${args.x}) = ${formatNumber(result.cdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_normal_pdf',
    description: 'Calculate probability density function (PDF) of standard normal distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate PDF' },
      },
      required: ['x'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/normal/pdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `n(${args.x}) = ${formatNumber(result.pdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_normal_ppf',
    description: 'Calculate percent point function (inverse CDF) of standard normal distribution',
    inputSchema: {
      type: 'object',
      properties: {
        p: { type: 'number', description: 'Probability value (0 to 1)' },
      },
      required: ['p'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/normal/ppf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `N⁻¹(${args.p}) = ${formatNumber(result.ppf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_t_cdf',
    description: 'Calculate CDF of Student\'s t-distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate CDF' },
        df: { type: 'number', description: 'Degrees of freedom' },
      },
      required: ['x', 'df'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/t/cdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `t-CDF(${args.x}, df=${args.df}) = ${formatNumber(result.cdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_t_pdf',
    description: 'Calculate PDF of Student\'s t-distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate PDF' },
        df: { type: 'number', description: 'Degrees of freedom' },
      },
      required: ['x', 'df'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/t/pdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `t-PDF(${args.x}, df=${args.df}) = ${formatNumber(result.pdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_t_ppf',
    description: 'Calculate inverse CDF (quantile) of Student\'s t-distribution',
    inputSchema: {
      type: 'object',
      properties: {
        p: { type: 'number', description: 'Probability value (0 to 1)' },
        df: { type: 'number', description: 'Degrees of freedom' },
      },
      required: ['p', 'df'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/t/ppf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `t⁻¹(${args.p}, df=${args.df}) = ${formatNumber(result.ppf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_chi2_cdf',
    description: 'Calculate CDF of chi-squared distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate CDF' },
        df: { type: 'number', description: 'Degrees of freedom' },
      },
      required: ['x', 'df'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/chi2/cdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `χ²-CDF(${args.x}, df=${args.df}) = ${formatNumber(result.cdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_chi2_pdf',
    description: 'Calculate PDF of chi-squared distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate PDF' },
        df: { type: 'number', description: 'Degrees of freedom' },
      },
      required: ['x', 'df'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/chi2/pdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `χ²-PDF(${args.x}, df=${args.df}) = ${formatNumber(result.pdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_gamma_cdf',
    description: 'Calculate CDF of gamma distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate CDF' },
        alpha: { type: 'number', description: 'Shape parameter (alpha)' },
        beta: { type: 'number', description: 'Rate parameter (beta)' },
      },
      required: ['x', 'alpha'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/gamma/cdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Gamma-CDF(${args.x}, α=${args.alpha}, β=${args.beta || 1}) = ${formatNumber(result.cdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_gamma_pdf',
    description: 'Calculate PDF of gamma distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate PDF' },
        alpha: { type: 'number', description: 'Shape parameter (alpha)' },
        beta: { type: 'number', description: 'Rate parameter (beta)' },
      },
      required: ['x', 'alpha'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/gamma/pdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Gamma-PDF(${args.x}, α=${args.alpha}, β=${args.beta || 1}) = ${formatNumber(result.pdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_exp_cdf',
    description: 'Calculate CDF of exponential distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate CDF' },
        rate: { type: 'number', description: 'Rate parameter (lambda)' },
      },
      required: ['x', 'rate'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/exponential/cdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Exp-CDF(${args.x}, λ=${args.rate}) = ${formatNumber(result.cdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_exp_pdf',
    description: 'Calculate PDF of exponential distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'Value at which to evaluate PDF' },
        rate: { type: 'number', description: 'Rate parameter (lambda)' },
      },
      required: ['x', 'rate'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/exponential/pdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Exp-PDF(${args.x}, λ=${args.rate}) = ${formatNumber(result.pdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_exp_ppf',
    description: 'Calculate inverse CDF of exponential distribution',
    inputSchema: {
      type: 'object',
      properties: {
        p: { type: 'number', description: 'Probability value (0 to 1)' },
        rate: { type: 'number', description: 'Rate parameter (lambda)' },
      },
      required: ['p', 'rate'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/exponential/ppf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `Exp⁻¹(${args.p}, λ=${args.rate}) = ${formatNumber(result.ppf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_dist_bivariate_normal_cdf',
    description: 'Calculate CDF of bivariate normal distribution',
    inputSchema: {
      type: 'object',
      properties: {
        x: { type: 'number', description: 'First variable value' },
        y: { type: 'number', description: 'Second variable value' },
        rho: { type: 'number', description: 'Correlation coefficient (-1 to 1)' },
      },
      required: ['x', 'y', 'rho'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/distributions/bivariate-normal/cdf', args, apiKey);
        return {
          success: true,
          data: result,
          message: `BivarN(${args.x}, ${args.y}, ρ=${args.rho}) = ${formatNumber(result.cdf || result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== MATH ====================
  {
    name: 'quantlib_math_eval',
    description: 'Evaluate a mathematical function at a point',
    inputSchema: {
      type: 'object',
      properties: {
        func_name: { type: 'string', description: 'Function name', enum: ['exp', 'log', 'sin', 'cos', 'tan', 'sqrt', 'abs', 'floor', 'ceil'] },
        x: { type: 'number', description: 'Input value' },
      },
      required: ['func_name', 'x'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/math/eval', args, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.func_name}(${args.x}) = ${formatNumber(result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_math_two_arg',
    description: 'Evaluate a two-argument mathematical function',
    inputSchema: {
      type: 'object',
      properties: {
        func_name: { type: 'string', description: 'Function name', enum: ['pow', 'max', 'min', 'atan2', 'mod'] },
        x: { type: 'number', description: 'First argument' },
        y: { type: 'number', description: 'Second argument' },
      },
      required: ['func_name', 'x', 'y'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/core/math/two-arg', args, apiKey);
        return {
          success: true,
          data: result,
          message: `${args.func_name}(${args.x}, ${args.y}) = ${formatNumber(result.result, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

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
