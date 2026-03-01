// QuantLib Statistics MCP Tools — Discrete Distributions & Time Series
// Binomial, Poisson, Geometric, Hypergeometric, NegBinomial, PGF (16 tools) + Time Series (9 tools)

import type { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber } from '../../QuantLibMCPBridge';

// ══════════════════════════════════════════════════════════════════════════════
// DISCRETE DISTRIBUTIONS - BINOMIAL (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const binomialPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_binomial_properties',
  description: 'Get properties of binomial distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      n: { type: 'number', description: 'Number of trials' },
      p: { type: 'number', description: 'Probability of success (0-1)' },
    },
    required: ['n', 'p'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/binomial/properties', {
        n: args.n, p: args.p,
      }, apiKey);
      return { success: true, data: result, message: `Binomial(n=${args.n}, p=${args.p}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const binomialPmfTool: InternalTool = {
  name: 'quantlib_statistics_binomial_pmf',
  description: 'Calculate binomial distribution PMF.',
  inputSchema: {
    type: 'object',
    properties: {
      n: { type: 'number', description: 'Number of trials' },
      p: { type: 'number', description: 'Probability of success' },
      k: { type: 'number', description: 'Number of successes' },
    },
    required: ['n', 'p', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/binomial/pmf', {
        n: args.n, p: args.p, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Binomial PMF(${args.k}) = ${formatNumber(result.pmf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const binomialCdfTool: InternalTool = {
  name: 'quantlib_statistics_binomial_cdf',
  description: 'Calculate binomial distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      n: { type: 'number', description: 'Number of trials' },
      p: { type: 'number', description: 'Probability of success' },
      k: { type: 'number', description: 'Number of successes' },
    },
    required: ['n', 'p', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/binomial/cdf', {
        n: args.n, p: args.p, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Binomial CDF(${args.k}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// DISCRETE DISTRIBUTIONS - POISSON (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const poissonPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_poisson_properties',
  description: 'Get properties of Poisson distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      lam: { type: 'number', description: 'Rate parameter (λ > 0)' },
    },
    required: ['lam'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/poisson/properties', {
        lam: args.lam,
      }, apiKey);
      return { success: true, data: result, message: `Poisson(λ=${args.lam}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const poissonPmfTool: InternalTool = {
  name: 'quantlib_statistics_poisson_pmf',
  description: 'Calculate Poisson distribution PMF.',
  inputSchema: {
    type: 'object',
    properties: {
      lam: { type: 'number', description: 'Rate parameter' },
      k: { type: 'number', description: 'Number of events' },
    },
    required: ['lam', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/poisson/pmf', {
        lam: args.lam, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Poisson PMF(${args.k}) = ${formatNumber(result.pmf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const poissonCdfTool: InternalTool = {
  name: 'quantlib_statistics_poisson_cdf',
  description: 'Calculate Poisson distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      lam: { type: 'number', description: 'Rate parameter' },
      k: { type: 'number', description: 'Number of events' },
    },
    required: ['lam', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/poisson/cdf', {
        lam: args.lam, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Poisson CDF(${args.k}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// DISCRETE DISTRIBUTIONS - GEOMETRIC (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const geometricPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_geometric_properties',
  description: 'Get properties of geometric distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      p: { type: 'number', description: 'Probability of success (0-1)' },
    },
    required: ['p'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/geometric/properties', {
        p: args.p,
      }, apiKey);
      return { success: true, data: result, message: `Geometric(p=${args.p}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const geometricPmfTool: InternalTool = {
  name: 'quantlib_statistics_geometric_pmf',
  description: 'Calculate geometric distribution PMF.',
  inputSchema: {
    type: 'object',
    properties: {
      p: { type: 'number', description: 'Probability of success' },
      k: { type: 'number', description: 'Trial number of first success' },
    },
    required: ['p', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/geometric/pmf', {
        p: args.p, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Geometric PMF(${args.k}) = ${formatNumber(result.pmf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const geometricCdfTool: InternalTool = {
  name: 'quantlib_statistics_geometric_cdf',
  description: 'Calculate geometric distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      p: { type: 'number', description: 'Probability of success' },
      k: { type: 'number', description: 'Trial number' },
    },
    required: ['p', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/geometric/cdf', {
        p: args.p, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Geometric CDF(${args.k}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const geometricPpfTool: InternalTool = {
  name: 'quantlib_statistics_geometric_ppf',
  description: 'Calculate geometric distribution quantile.',
  inputSchema: {
    type: 'object',
    properties: {
      p: { type: 'number', description: 'Success probability' },
      q: { type: 'number', description: 'Quantile probability (0-1)' },
    },
    required: ['p', 'q'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/geometric/ppf', {
        p: args.p, q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Geometric PPF(${args.q}) = ${formatNumber(result.ppf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// DISCRETE DISTRIBUTIONS - HYPERGEOMETRIC (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const hypergeometricPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_hypergeometric_properties',
  description: 'Get properties of hypergeometric distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      N: { type: 'number', description: 'Population size' },
      K: { type: 'number', description: 'Number of success states in population' },
      n: { type: 'number', description: 'Number of draws' },
    },
    required: ['N', 'K', 'n'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/hypergeometric/properties', {
        N: args.N, K: args.K, n: args.n,
      }, apiKey);
      return { success: true, data: result, message: `Hypergeometric(N=${args.N}, K=${args.K}, n=${args.n}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const hypergeometricPmfTool: InternalTool = {
  name: 'quantlib_statistics_hypergeometric_pmf',
  description: 'Calculate hypergeometric distribution PMF.',
  inputSchema: {
    type: 'object',
    properties: {
      N: { type: 'number', description: 'Population size' },
      K: { type: 'number', description: 'Success states in population' },
      n: { type: 'number', description: 'Number of draws' },
      k: { type: 'number', description: 'Number of observed successes' },
    },
    required: ['N', 'K', 'n', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/hypergeometric/pmf', {
        N: args.N, K: args.K, n: args.n, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Hypergeometric PMF(${args.k}) = ${formatNumber(result.pmf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const hypergeometricCdfTool: InternalTool = {
  name: 'quantlib_statistics_hypergeometric_cdf',
  description: 'Calculate hypergeometric distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      N: { type: 'number', description: 'Population size' },
      K: { type: 'number', description: 'Success states in population' },
      n: { type: 'number', description: 'Number of draws' },
      k: { type: 'number', description: 'Number of observed successes' },
    },
    required: ['N', 'K', 'n', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/hypergeometric/cdf', {
        N: args.N, K: args.K, n: args.n, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `Hypergeometric CDF(${args.k}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// DISCRETE DISTRIBUTIONS - NEGATIVE BINOMIAL (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const negBinomialPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_negative_binomial_properties',
  description: 'Get properties of negative binomial distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      r: { type: 'number', description: 'Number of successes required' },
      p: { type: 'number', description: 'Probability of success (0-1)' },
    },
    required: ['r', 'p'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/negative-binomial/properties', {
        r: args.r, p: args.p,
      }, apiKey);
      return { success: true, data: result, message: `NegBinomial(r=${args.r}, p=${args.p}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const negBinomialPmfTool: InternalTool = {
  name: 'quantlib_statistics_negative_binomial_pmf',
  description: 'Calculate negative binomial distribution PMF.',
  inputSchema: {
    type: 'object',
    properties: {
      r: { type: 'number', description: 'Successes required' },
      p: { type: 'number', description: 'Success probability' },
      k: { type: 'number', description: 'Number of failures' },
    },
    required: ['r', 'p', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/negative-binomial/pmf', {
        r: args.r, p: args.p, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `NegBinomial PMF(${args.k}) = ${formatNumber(result.pmf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const negBinomialCdfTool: InternalTool = {
  name: 'quantlib_statistics_negative_binomial_cdf',
  description: 'Calculate negative binomial distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      r: { type: 'number', description: 'Successes required' },
      p: { type: 'number', description: 'Success probability' },
      k: { type: 'number', description: 'Number of failures' },
    },
    required: ['r', 'p', 'k'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/negative-binomial/cdf', {
        r: args.r, p: args.p, k: args.k,
      }, apiKey);
      return { success: true, data: result, message: `NegBinomial CDF(${args.k}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// PGF (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const pgfEvaluateTool: InternalTool = {
  name: 'quantlib_statistics_pgf_evaluate',
  description: 'Evaluate probability generating function for discrete distributions.',
  inputSchema: {
    type: 'object',
    properties: {
      distribution: { type: 'string', description: 'Distribution type: binomial, poisson' },
      z: { type: 'number', description: 'Point to evaluate PGF' },
      n: { type: 'number', description: 'n parameter (for binomial)' },
      p: { type: 'number', description: 'p parameter (for binomial)' },
      lam: { type: 'number', description: 'lambda parameter (for poisson)' },
    },
    required: ['distribution', 'z'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/pgf', {
        distribution: args.distribution,
        z: args.z,
        n: args.n,
        p: args.p,
        lam: args.lam,
      }, apiKey);
      return { success: true, data: result, message: `PGF(${args.z}) = ${formatNumber(result.pgf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// TIME SERIES (9 tools)
// ══════════════════════════════════════════════════════════════════════════════

const arFitTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_ar_fit',
  description: 'Fit an AR(p) model to time series data.',
  inputSchema: {
    type: 'object',
    properties: {
      data: { type: 'string', description: 'JSON array of time series values' },
      order: { type: 'number', description: 'AR order (p)' },
    },
    required: ['data'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/ar/fit', {
        data: JSON.parse(args.data as string),
        order: args.order,
      }, apiKey);
      return { success: true, data: result, message: `Fitted AR(${args.order || 'auto'}) model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const arForecastTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_ar_forecast',
  description: 'Forecast using AR model.',
  inputSchema: {
    type: 'object',
    properties: {
      data: { type: 'string', description: 'JSON array of time series values' },
      order: { type: 'number', description: 'AR order' },
      steps: { type: 'number', description: 'Forecast steps ahead' },
    },
    required: ['data'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/ar/forecast', {
        data: JSON.parse(args.data as string),
        order: args.order,
        steps: args.steps,
      }, apiKey);
      return { success: true, data: result, message: `AR forecast: ${args.steps || 1} steps ahead` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const maFitTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_ma_fit',
  description: 'Fit an MA(q) model to time series data.',
  inputSchema: {
    type: 'object',
    properties: {
      data: { type: 'string', description: 'JSON array of time series values' },
      order: { type: 'number', description: 'MA order (q)' },
    },
    required: ['data'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/ma/fit', {
        data: JSON.parse(args.data as string),
        order: args.order,
      }, apiKey);
      return { success: true, data: result, message: `Fitted MA(${args.order || 'auto'}) model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const arimaFitTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_arima_fit',
  description: 'Fit an ARIMA(p,d,q) model to time series data.',
  inputSchema: {
    type: 'object',
    properties: {
      data: { type: 'string', description: 'JSON array of time series values' },
      p: { type: 'number', description: 'AR order' },
      d: { type: 'number', description: 'Differencing order' },
      q: { type: 'number', description: 'MA order' },
    },
    required: ['data'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/arima/fit', {
        data: JSON.parse(args.data as string),
        p: args.p,
        d: args.d,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Fitted ARIMA(${args.p || 1},${args.d || 0},${args.q || 0}) model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const arimaForecastTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_arima_forecast',
  description: 'Forecast using ARIMA model.',
  inputSchema: {
    type: 'object',
    properties: {
      data: { type: 'string', description: 'JSON array of time series values' },
      p: { type: 'number', description: 'AR order' },
      d: { type: 'number', description: 'Differencing order' },
      q: { type: 'number', description: 'MA order' },
      steps: { type: 'number', description: 'Forecast steps ahead' },
    },
    required: ['data'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/arima/forecast', {
        data: JSON.parse(args.data as string),
        p: args.p, d: args.d, q: args.q, steps: args.steps,
      }, apiKey);
      return { success: true, data: result, message: `ARIMA forecast: ${args.steps || 1} steps ahead` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const garchFitTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_garch_fit',
  description: 'Fit a GARCH(p,q) model to return series.',
  inputSchema: {
    type: 'object',
    properties: {
      returns: { type: 'string', description: 'JSON array of return values' },
      p: { type: 'number', description: 'GARCH p order' },
      q: { type: 'number', description: 'GARCH q order' },
    },
    required: ['returns'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/garch/fit', {
        returns: JSON.parse(args.returns as string),
        p: args.p,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Fitted GARCH(${args.p || 1},${args.q || 1}) model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const garchForecastTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_garch_forecast',
  description: 'Forecast volatility using GARCH model.',
  inputSchema: {
    type: 'object',
    properties: {
      returns: { type: 'string', description: 'JSON array of return values' },
      p: { type: 'number', description: 'GARCH p order' },
      q: { type: 'number', description: 'GARCH q order' },
      steps: { type: 'number', description: 'Forecast steps ahead' },
    },
    required: ['returns'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/garch/forecast', {
        returns: JSON.parse(args.returns as string),
        p: args.p, q: args.q, steps: args.steps,
      }, apiKey);
      return { success: true, data: result, message: `GARCH volatility forecast: ${args.steps || 1} steps ahead` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const egarchFitTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_egarch_fit',
  description: 'Fit an EGARCH model (asymmetric volatility).',
  inputSchema: {
    type: 'object',
    properties: {
      returns: { type: 'string', description: 'JSON array of return values' },
      p: { type: 'number', description: 'EGARCH p order' },
      q: { type: 'number', description: 'EGARCH q order' },
    },
    required: ['returns'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/egarch/fit', {
        returns: JSON.parse(args.returns as string),
        p: args.p,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Fitted EGARCH(${args.p || 1},${args.q || 1}) model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const gjrGarchFitTool: InternalTool = {
  name: 'quantlib_statistics_timeseries_gjr_garch_fit',
  description: 'Fit a GJR-GARCH model (leverage effect).',
  inputSchema: {
    type: 'object',
    properties: {
      returns: { type: 'string', description: 'JSON array of return values' },
      p: { type: 'number', description: 'GJR-GARCH p order' },
      q: { type: 'number', description: 'GJR-GARCH q order' },
    },
    required: ['returns'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/timeseries/gjr-garch/fit', {
        returns: JSON.parse(args.returns as string),
        p: args.p,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Fitted GJR-GARCH(${args.p || 1},${args.q || 1}) model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EXPORT
// ══════════════════════════════════════════════════════════════════════════════

export const statisticsDiscreteTimeSeriesTools: InternalTool[] = [
  // Binomial (3)
  binomialPropertiesTool, binomialPmfTool, binomialCdfTool,
  // Poisson (3)
  poissonPropertiesTool, poissonPmfTool, poissonCdfTool,
  // Geometric (4)
  geometricPropertiesTool, geometricPmfTool, geometricCdfTool, geometricPpfTool,
  // Hypergeometric (3)
  hypergeometricPropertiesTool, hypergeometricPmfTool, hypergeometricCdfTool,
  // Negative Binomial (3)
  negBinomialPropertiesTool, negBinomialPmfTool, negBinomialCdfTool,
  // PGF (1)
  pgfEvaluateTool,
  // Time Series (9)
  arFitTool, arForecastTool, maFitTool,
  arimaFitTool, arimaForecastTool,
  garchFitTool, garchForecastTool,
  egarchFitTool, gjrGarchFitTool,
];
