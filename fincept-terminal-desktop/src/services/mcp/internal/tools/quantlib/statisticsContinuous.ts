// QuantLib Statistics MCP Tools — Continuous Distributions
// Normal, Lognormal, Student-t, Chi-squared, Gamma, Beta, Exponential, F (26 tools)

import type { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber } from '../../QuantLibMCPBridge';

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - NORMAL (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const normalPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_normal_properties',
  description: 'Get properties of a normal distribution (mean, variance, skewness, kurtosis).',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean (default: 0)' },
      sigma: { type: 'number', description: 'Standard deviation (default: 1)' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/normal/properties', {
        mu: args.mu,
        sigma: args.sigma,
      }, apiKey);
      return { success: true, data: result, message: `Normal(μ=${args.mu || 0}, σ=${args.sigma || 1}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const normalPdfTool: InternalTool = {
  name: 'quantlib_statistics_normal_pdf',
  description: 'Calculate normal distribution PDF at point x.',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean' },
      sigma: { type: 'number', description: 'Standard deviation' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/normal/pdf', {
        mu: args.mu, sigma: args.sigma, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const normalCdfTool: InternalTool = {
  name: 'quantlib_statistics_normal_cdf',
  description: 'Calculate normal distribution CDF at point x.',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean' },
      sigma: { type: 'number', description: 'Standard deviation' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/normal/cdf', {
        mu: args.mu, sigma: args.sigma, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const normalPpfTool: InternalTool = {
  name: 'quantlib_statistics_normal_ppf',
  description: 'Calculate normal distribution quantile (inverse CDF).',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean' },
      sigma: { type: 'number', description: 'Standard deviation' },
      p: { type: 'number', description: 'Probability (0-1)' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/normal/ppf', {
        mu: args.mu, sigma: args.sigma, p: args.p,
      }, apiKey);
      return { success: true, data: result, message: `PPF(${args.p}) = ${formatNumber(result.ppf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - LOGNORMAL (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const lognormalPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_lognormal_properties',
  description: 'Get properties of a lognormal distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean of log' },
      sigma: { type: 'number', description: 'Std dev of log' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/lognormal/properties', {
        mu: args.mu, sigma: args.sigma,
      }, apiKey);
      return { success: true, data: result, message: 'Lognormal distribution properties' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const lognormalPdfTool: InternalTool = {
  name: 'quantlib_statistics_lognormal_pdf',
  description: 'Calculate lognormal distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean of log' },
      sigma: { type: 'number', description: 'Std dev of log' },
      x: { type: 'number', description: 'Point to evaluate (>0)' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/lognormal/pdf', {
        mu: args.mu, sigma: args.sigma, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Lognormal PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const lognormalCdfTool: InternalTool = {
  name: 'quantlib_statistics_lognormal_cdf',
  description: 'Calculate lognormal distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean of log' },
      sigma: { type: 'number', description: 'Std dev of log' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/lognormal/cdf', {
        mu: args.mu, sigma: args.sigma, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Lognormal CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const lognormalPpfTool: InternalTool = {
  name: 'quantlib_statistics_lognormal_ppf',
  description: 'Calculate lognormal distribution quantile.',
  inputSchema: {
    type: 'object',
    properties: {
      mu: { type: 'number', description: 'Mean of log' },
      sigma: { type: 'number', description: 'Std dev of log' },
      p: { type: 'number', description: 'Probability (0-1)' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/lognormal/ppf', {
        mu: args.mu, sigma: args.sigma, p: args.p,
      }, apiKey);
      return { success: true, data: result, message: `Lognormal PPF(${args.p}) = ${formatNumber(result.ppf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - STUDENT-T (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const studentTPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_student_t_properties',
  description: 'Get properties of Student-t distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      df: { type: 'number', description: 'Degrees of freedom' },
      mu: { type: 'number', description: 'Location parameter' },
      sigma: { type: 'number', description: 'Scale parameter' },
    },
    required: ['df'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/student-t/properties', {
        df: args.df, mu: args.mu, sigma: args.sigma,
      }, apiKey);
      return { success: true, data: result, message: `Student-t(df=${args.df}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const studentTPdfTool: InternalTool = {
  name: 'quantlib_statistics_student_t_pdf',
  description: 'Calculate Student-t distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      df: { type: 'number', description: 'Degrees of freedom' },
      mu: { type: 'number', description: 'Location' },
      sigma: { type: 'number', description: 'Scale' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: ['df'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/student-t/pdf', {
        df: args.df, mu: args.mu, sigma: args.sigma, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Student-t PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const studentTCdfTool: InternalTool = {
  name: 'quantlib_statistics_student_t_cdf',
  description: 'Calculate Student-t distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      df: { type: 'number', description: 'Degrees of freedom' },
      mu: { type: 'number', description: 'Location' },
      sigma: { type: 'number', description: 'Scale' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: ['df'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/student-t/cdf', {
        df: args.df, mu: args.mu, sigma: args.sigma, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Student-t CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - CHI-SQUARED (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const chiSquaredPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_chi_squared_properties',
  description: 'Get properties of chi-squared distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      df: { type: 'number', description: 'Degrees of freedom' },
    },
    required: ['df'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/chi-squared/properties', {
        df: args.df,
      }, apiKey);
      return { success: true, data: result, message: `Chi-squared(df=${args.df}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const chiSquaredPdfTool: InternalTool = {
  name: 'quantlib_statistics_chi_squared_pdf',
  description: 'Calculate chi-squared distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      df: { type: 'number', description: 'Degrees of freedom' },
      x: { type: 'number', description: 'Point to evaluate (>=0)' },
    },
    required: ['df', 'x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/chi-squared/pdf', {
        df: args.df, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Chi-squared PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const chiSquaredCdfTool: InternalTool = {
  name: 'quantlib_statistics_chi_squared_cdf',
  description: 'Calculate chi-squared distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      df: { type: 'number', description: 'Degrees of freedom' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: ['df', 'x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/chi-squared/cdf', {
        df: args.df, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Chi-squared CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - GAMMA (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const gammaPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_gamma_properties',
  description: 'Get properties of gamma distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      alpha: { type: 'number', description: 'Shape parameter (>0)' },
      beta: { type: 'number', description: 'Rate parameter (>0)' },
    },
    required: ['alpha'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/gamma/properties', {
        alpha: args.alpha, beta: args.beta,
      }, apiKey);
      return { success: true, data: result, message: `Gamma(α=${args.alpha}, β=${args.beta || 1}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const gammaPdfTool: InternalTool = {
  name: 'quantlib_statistics_gamma_pdf',
  description: 'Calculate gamma distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      alpha: { type: 'number', description: 'Shape parameter' },
      beta: { type: 'number', description: 'Rate parameter' },
      x: { type: 'number', description: 'Point to evaluate (>=0)' },
    },
    required: ['alpha'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/gamma/pdf', {
        alpha: args.alpha, beta: args.beta, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Gamma PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const gammaCdfTool: InternalTool = {
  name: 'quantlib_statistics_gamma_cdf',
  description: 'Calculate gamma distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      alpha: { type: 'number', description: 'Shape parameter' },
      beta: { type: 'number', description: 'Rate parameter' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: ['alpha'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/gamma/cdf', {
        alpha: args.alpha, beta: args.beta, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Gamma CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - BETA (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const betaPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_beta_properties',
  description: 'Get properties of beta distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      alpha: { type: 'number', description: 'Alpha parameter (>0)' },
      beta: { type: 'number', description: 'Beta parameter (>0)' },
    },
    required: ['alpha', 'beta'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/beta/properties', {
        alpha: args.alpha, beta: args.beta,
      }, apiKey);
      return { success: true, data: result, message: `Beta(α=${args.alpha}, β=${args.beta}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const betaPdfTool: InternalTool = {
  name: 'quantlib_statistics_beta_pdf',
  description: 'Calculate beta distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      alpha: { type: 'number', description: 'Alpha parameter' },
      beta: { type: 'number', description: 'Beta parameter' },
      x: { type: 'number', description: 'Point to evaluate (0-1)' },
    },
    required: ['alpha', 'beta'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/beta/pdf', {
        alpha: args.alpha, beta: args.beta, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Beta PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const betaCdfTool: InternalTool = {
  name: 'quantlib_statistics_beta_cdf',
  description: 'Calculate beta distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      alpha: { type: 'number', description: 'Alpha parameter' },
      beta: { type: 'number', description: 'Beta parameter' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: ['alpha', 'beta'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/beta/cdf', {
        alpha: args.alpha, beta: args.beta, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Beta CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - EXPONENTIAL (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const exponentialPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_exponential_properties',
  description: 'Get properties of exponential distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      rate: { type: 'number', description: 'Rate parameter (λ > 0)' },
    },
    required: ['rate'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/exponential/properties', {
        rate: args.rate,
      }, apiKey);
      return { success: true, data: result, message: `Exponential(λ=${args.rate}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const exponentialPdfTool: InternalTool = {
  name: 'quantlib_statistics_exponential_pdf',
  description: 'Calculate exponential distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      rate: { type: 'number', description: 'Rate parameter' },
      x: { type: 'number', description: 'Point to evaluate (>=0)' },
    },
    required: ['rate', 'x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/exponential/pdf', {
        rate: args.rate, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Exponential PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const exponentialCdfTool: InternalTool = {
  name: 'quantlib_statistics_exponential_cdf',
  description: 'Calculate exponential distribution CDF.',
  inputSchema: {
    type: 'object',
    properties: {
      rate: { type: 'number', description: 'Rate parameter' },
      x: { type: 'number', description: 'Point to evaluate' },
    },
    required: ['rate', 'x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/exponential/cdf', {
        rate: args.rate, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `Exponential CDF(${args.x}) = ${formatNumber(result.cdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const exponentialPpfTool: InternalTool = {
  name: 'quantlib_statistics_exponential_ppf',
  description: 'Calculate exponential distribution quantile.',
  inputSchema: {
    type: 'object',
    properties: {
      rate: { type: 'number', description: 'Rate parameter' },
      p: { type: 'number', description: 'Probability (0-1)' },
    },
    required: ['rate', 'p'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/exponential/ppf', {
        rate: args.rate, p: args.p,
      }, apiKey);
      return { success: true, data: result, message: `Exponential PPF(${args.p}) = ${formatNumber(result.ppf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// CONTINUOUS DISTRIBUTIONS - F (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const fPropertiesTool: InternalTool = {
  name: 'quantlib_statistics_f_properties',
  description: 'Get properties of F distribution.',
  inputSchema: {
    type: 'object',
    properties: {
      df1: { type: 'number', description: 'Numerator degrees of freedom' },
      df2: { type: 'number', description: 'Denominator degrees of freedom' },
    },
    required: ['df1', 'df2'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/f/properties', {
        df1: args.df1, df2: args.df2,
      }, apiKey);
      return { success: true, data: result, message: `F(${args.df1}, ${args.df2}) properties` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const fPdfTool: InternalTool = {
  name: 'quantlib_statistics_f_pdf',
  description: 'Calculate F distribution PDF.',
  inputSchema: {
    type: 'object',
    properties: {
      df1: { type: 'number', description: 'Numerator df' },
      df2: { type: 'number', description: 'Denominator df' },
      x: { type: 'number', description: 'Point to evaluate (>=0)' },
    },
    required: ['df1', 'df2', 'x'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/statistics/distributions/f/pdf', {
        df1: args.df1, df2: args.df2, x: args.x,
      }, apiKey);
      return { success: true, data: result, message: `F PDF(${args.x}) = ${formatNumber(result.pdf)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EXPORT
// ══════════════════════════════════════════════════════════════════════════════

export const statisticsContinuousTools: InternalTool[] = [
  // Normal (4)
  normalPropertiesTool, normalPdfTool, normalCdfTool, normalPpfTool,
  // Lognormal (4)
  lognormalPropertiesTool, lognormalPdfTool, lognormalCdfTool, lognormalPpfTool,
  // Student-t (3)
  studentTPropertiesTool, studentTPdfTool, studentTCdfTool,
  // Chi-squared (3)
  chiSquaredPropertiesTool, chiSquaredPdfTool, chiSquaredCdfTool,
  // Gamma (3)
  gammaPropertiesTool, gammaPdfTool, gammaCdfTool,
  // Beta (3)
  betaPropertiesTool, betaPdfTool, betaCdfTool,
  // Exponential (4)
  exponentialPropertiesTool, exponentialPdfTool, exponentialCdfTool, exponentialPpfTool,
  // F (2)
  fPropertiesTool, fPdfTool,
];
