// QuantLib Core MCP Tools - Types, Conventions, AutoDiff, Distributions, Math (27 tools)

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber } from '../../QuantLibMCPBridge';

export const quantlibCoreTypesConventionsTools: InternalTool[] = [
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
];
