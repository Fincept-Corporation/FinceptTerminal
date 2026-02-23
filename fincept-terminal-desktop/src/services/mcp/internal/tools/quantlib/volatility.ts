// QuantLib Volatility MCP Tools
// 14 tools for volatility surfaces, SABR model, and local volatility

import type { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

// ══════════════════════════════════════════════════════════════════════════════
// SURFACE (6 tools)
// ══════════════════════════════════════════════════════════════════════════════

const surfaceFlatTool: InternalTool = {
  name: 'quantlib_volatility_surface_flat',
  description: 'Create a flat volatility surface with constant vol across all strikes and expiries.',
  inputSchema: {
    type: 'object',
    properties: {
      volatility: { type: 'number', description: 'Flat volatility level (e.g., 0.20 for 20%)' },
      query_expiry: { type: 'number', description: 'Expiry to query in years' },
      query_strike: { type: 'number', description: 'Strike to query' },
    },
    required: ['volatility', 'query_expiry', 'query_strike'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/surface/flat', {
        volatility: args.volatility,
        query_expiry: args.query_expiry,
        query_strike: args.query_strike,
      }, apiKey);
      return { success: true, data: result, message: `Flat surface vol: ${formatPercent(args.volatility as number)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const surfaceTermStructureTool: InternalTool = {
  name: 'quantlib_volatility_surface_term_structure',
  description: 'Create a volatility term structure (vol varies by expiry only).',
  inputSchema: {
    type: 'object',
    properties: {
      expiries: { type: 'string', description: 'JSON array of expiry times in years' },
      volatilities: { type: 'string', description: 'JSON array of volatilities corresponding to expiries' },
      query_expiry: { type: 'number', description: 'Expiry to query in years' },
    },
    required: ['expiries', 'volatilities', 'query_expiry'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/surface/term-structure', {
        expiries: JSON.parse(args.expiries as string),
        volatilities: JSON.parse(args.volatilities as string),
        query_expiry: args.query_expiry,
      }, apiKey);
      return { success: true, data: result, message: `Vol at ${args.query_expiry}Y: ${formatPercent(result.volatility)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const surfaceGridTool: InternalTool = {
  name: 'quantlib_volatility_surface_grid',
  description: 'Create a full volatility surface from a grid of expiries, strikes, and vol matrix.',
  inputSchema: {
    type: 'object',
    properties: {
      expiries: { type: 'string', description: 'JSON array of expiry times in years' },
      strikes: { type: 'string', description: 'JSON array of strikes' },
      vol_matrix: { type: 'string', description: 'JSON 2D array of volatilities [expiries x strikes]' },
      query_expiry: { type: 'number', description: 'Expiry to query in years' },
      query_strike: { type: 'number', description: 'Strike to query' },
    },
    required: ['expiries', 'strikes', 'vol_matrix', 'query_expiry', 'query_strike'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/surface/grid', {
        expiries: JSON.parse(args.expiries as string),
        strikes: JSON.parse(args.strikes as string),
        vol_matrix: JSON.parse(args.vol_matrix as string),
        query_expiry: args.query_expiry,
        query_strike: args.query_strike,
      }, apiKey);
      return { success: true, data: result, message: `Surface vol at T=${args.query_expiry}, K=${args.query_strike}: ${formatPercent(result.volatility)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const surfaceSmileTool: InternalTool = {
  name: 'quantlib_volatility_surface_smile',
  description: 'Extract the volatility smile at a given expiry across multiple strikes.',
  inputSchema: {
    type: 'object',
    properties: {
      expiries: { type: 'string', description: 'JSON array of expiry times in years' },
      strikes: { type: 'string', description: 'JSON array of strikes' },
      vol_matrix: { type: 'string', description: 'JSON 2D array of volatilities' },
      query_expiry: { type: 'number', description: 'Expiry to extract smile at' },
      query_strikes: { type: 'string', description: 'JSON array of strikes to query' },
    },
    required: ['expiries', 'strikes', 'vol_matrix', 'query_expiry', 'query_strikes'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const queryStrikes = JSON.parse(args.query_strikes as string);
      const result = await quantlibApiCall('/quantlib/volatility/surface/smile', {
        expiries: JSON.parse(args.expiries as string),
        strikes: JSON.parse(args.strikes as string),
        vol_matrix: JSON.parse(args.vol_matrix as string),
        query_expiry: args.query_expiry,
        query_strikes: queryStrikes,
      }, apiKey);
      return { success: true, data: result, message: `Extracted smile at T=${args.query_expiry} for ${queryStrikes.length} strikes` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const surfaceTotalVarianceTool: InternalTool = {
  name: 'quantlib_volatility_surface_total_variance',
  description: 'Calculate total variance (sigma^2 * T) at a given point on the surface.',
  inputSchema: {
    type: 'object',
    properties: {
      expiries: { type: 'string', description: 'JSON array of expiry times in years' },
      strikes: { type: 'string', description: 'JSON array of strikes' },
      vol_matrix: { type: 'string', description: 'JSON 2D array of volatilities' },
      query_expiry: { type: 'number', description: 'Expiry to query in years' },
      query_strike: { type: 'number', description: 'Strike to query' },
    },
    required: ['expiries', 'strikes', 'vol_matrix', 'query_expiry', 'query_strike'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/surface/total-variance', {
        expiries: JSON.parse(args.expiries as string),
        strikes: JSON.parse(args.strikes as string),
        vol_matrix: JSON.parse(args.vol_matrix as string),
        query_expiry: args.query_expiry,
        query_strike: args.query_strike,
      }, apiKey);
      return { success: true, data: result, message: `Total variance: ${formatNumber(result.total_variance)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const surfaceFromPointsTool: InternalTool = {
  name: 'quantlib_volatility_surface_from_points',
  description: 'Build a volatility surface from scattered points (tenors, strikes, vols).',
  inputSchema: {
    type: 'object',
    properties: {
      tenors: { type: 'string', description: 'JSON array of tenor values' },
      strikes: { type: 'string', description: 'JSON array of strike values' },
      vols: { type: 'string', description: 'JSON array of volatility values' },
      forwards: { type: 'string', description: 'Optional JSON array of forward prices' },
      expiry_grid: { type: 'string', description: 'Optional JSON array of expiry grid points' },
      strike_grid: { type: 'string', description: 'Optional JSON array of strike grid points' },
    },
    required: ['tenors', 'strikes', 'vols'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/surface/from-points', {
        tenors: JSON.parse(args.tenors as string),
        strikes: JSON.parse(args.strikes as string),
        vols: JSON.parse(args.vols as string),
        forwards: args.forwards ? JSON.parse(args.forwards as string) : undefined,
        expiry_grid: args.expiry_grid ? JSON.parse(args.expiry_grid as string) : undefined,
        strike_grid: args.strike_grid ? JSON.parse(args.strike_grid as string) : undefined,
      }, apiKey);
      return { success: true, data: result, message: 'Built volatility surface from scattered points' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// SABR MODEL (6 tools)
// ══════════════════════════════════════════════════════════════════════════════

const sabrImpliedVolTool: InternalTool = {
  name: 'quantlib_volatility_sabr_implied_vol',
  description: 'Calculate SABR implied volatility (lognormal) for given parameters and strike.',
  inputSchema: {
    type: 'object',
    properties: {
      forward: { type: 'number', description: 'Forward price' },
      strike: { type: 'number', description: 'Strike price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      alpha: { type: 'number', description: 'SABR alpha (initial vol)' },
      beta: { type: 'number', description: 'SABR beta (elasticity, 0-1)' },
      rho: { type: 'number', description: 'SABR rho (correlation, -1 to 1)' },
      nu: { type: 'number', description: 'SABR nu (vol of vol)' },
    },
    required: ['forward', 'strike', 'expiry', 'alpha', 'beta', 'rho', 'nu'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/sabr/implied-vol', {
        forward: args.forward,
        strike: args.strike,
        expiry: args.expiry,
        alpha: args.alpha,
        beta: args.beta,
        rho: args.rho,
        nu: args.nu,
      }, apiKey);
      return { success: true, data: result, message: `SABR implied vol: ${formatPercent(result.implied_vol)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const sabrNormalVolTool: InternalTool = {
  name: 'quantlib_volatility_sabr_normal_vol',
  description: 'Calculate SABR normal (Bachelier) volatility for given parameters.',
  inputSchema: {
    type: 'object',
    properties: {
      forward: { type: 'number', description: 'Forward price' },
      strike: { type: 'number', description: 'Strike price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      alpha: { type: 'number', description: 'SABR alpha' },
      beta: { type: 'number', description: 'SABR beta' },
      rho: { type: 'number', description: 'SABR rho' },
      nu: { type: 'number', description: 'SABR nu' },
    },
    required: ['forward', 'strike', 'expiry', 'alpha', 'beta', 'rho', 'nu'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/sabr/normal-vol', {
        forward: args.forward,
        strike: args.strike,
        expiry: args.expiry,
        alpha: args.alpha,
        beta: args.beta,
        rho: args.rho,
        nu: args.nu,
      }, apiKey);
      return { success: true, data: result, message: `SABR normal vol: ${formatNumber(result.normal_vol)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const sabrSmileTool: InternalTool = {
  name: 'quantlib_volatility_sabr_smile',
  description: 'Generate the full SABR smile across multiple strikes.',
  inputSchema: {
    type: 'object',
    properties: {
      forward: { type: 'number', description: 'Forward price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      alpha: { type: 'number', description: 'SABR alpha' },
      beta: { type: 'number', description: 'SABR beta' },
      rho: { type: 'number', description: 'SABR rho' },
      nu: { type: 'number', description: 'SABR nu' },
      strikes: { type: 'string', description: 'JSON array of strikes to evaluate' },
    },
    required: ['forward', 'expiry', 'alpha', 'beta', 'rho', 'nu', 'strikes'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const strikes = JSON.parse(args.strikes as string);
      const result = await quantlibApiCall('/quantlib/volatility/sabr/smile', {
        forward: args.forward,
        expiry: args.expiry,
        alpha: args.alpha,
        beta: args.beta,
        rho: args.rho,
        nu: args.nu,
        strikes,
      }, apiKey);
      return { success: true, data: result, message: `Generated SABR smile for ${strikes.length} strikes` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const sabrCalibrateTool: InternalTool = {
  name: 'quantlib_volatility_sabr_calibrate',
  description: 'Calibrate SABR parameters to market volatility data.',
  inputSchema: {
    type: 'object',
    properties: {
      forward: { type: 'number', description: 'Forward price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      strikes: { type: 'string', description: 'JSON array of market strikes' },
      market_vols: { type: 'string', description: 'JSON array of market implied vols' },
      beta: { type: 'number', description: 'Optional fixed beta (if not calibrated)' },
      weights: { type: 'string', description: 'Optional JSON array of calibration weights' },
      method: { type: 'string', description: 'Calibration method (default: least_squares)' },
    },
    required: ['forward', 'expiry', 'strikes', 'market_vols'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/sabr/calibrate', {
        forward: args.forward,
        expiry: args.expiry,
        strikes: JSON.parse(args.strikes as string),
        market_vols: JSON.parse(args.market_vols as string),
        beta: args.beta,
        weights: args.weights ? JSON.parse(args.weights as string) : undefined,
        method: args.method,
      }, apiKey);
      return { success: true, data: result, message: `Calibrated SABR: α=${formatNumber(result.alpha)}, β=${formatNumber(result.beta)}, ρ=${formatNumber(result.rho)}, ν=${formatNumber(result.nu)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const sabrDensityTool: InternalTool = {
  name: 'quantlib_volatility_sabr_density',
  description: 'Calculate the risk-neutral probability density implied by SABR.',
  inputSchema: {
    type: 'object',
    properties: {
      forward: { type: 'number', description: 'Forward price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      alpha: { type: 'number', description: 'SABR alpha' },
      beta: { type: 'number', description: 'SABR beta' },
      rho: { type: 'number', description: 'SABR rho' },
      nu: { type: 'number', description: 'SABR nu' },
      strikes: { type: 'string', description: 'JSON array of strikes to evaluate density' },
    },
    required: ['forward', 'expiry', 'alpha', 'beta', 'rho', 'nu', 'strikes'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const strikes = JSON.parse(args.strikes as string);
      const result = await quantlibApiCall('/quantlib/volatility/sabr/density', {
        forward: args.forward,
        expiry: args.expiry,
        alpha: args.alpha,
        beta: args.beta,
        rho: args.rho,
        nu: args.nu,
        strikes,
      }, apiKey);
      return { success: true, data: result, message: `Computed SABR density at ${strikes.length} points` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const sabrDynamicsTool: InternalTool = {
  name: 'quantlib_volatility_sabr_dynamics',
  description: 'Analyze SABR smile dynamics (how smile changes with forward movement).',
  inputSchema: {
    type: 'object',
    properties: {
      forward: { type: 'number', description: 'Current forward price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      alpha: { type: 'number', description: 'SABR alpha' },
      beta: { type: 'number', description: 'SABR beta' },
      rho: { type: 'number', description: 'SABR rho' },
      nu: { type: 'number', description: 'SABR nu' },
      strike: { type: 'number', description: 'Strike to analyze' },
      forward_shift: { type: 'number', description: 'Optional forward shift amount (default: 1%)' },
    },
    required: ['forward', 'expiry', 'alpha', 'beta', 'rho', 'nu', 'strike'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/sabr/dynamics', {
        forward: args.forward,
        expiry: args.expiry,
        alpha: args.alpha,
        beta: args.beta,
        rho: args.rho,
        nu: args.nu,
        strike: args.strike,
        forward_shift: args.forward_shift,
      }, apiKey);
      return { success: true, data: result, message: 'Analyzed SABR smile dynamics' };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// LOCAL VOL (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const localVolConstantTool: InternalTool = {
  name: 'quantlib_volatility_local_vol_constant',
  description: 'Create a constant local volatility surface.',
  inputSchema: {
    type: 'object',
    properties: {
      volatility: { type: 'number', description: 'Constant local vol level' },
      spot: { type: 'number', description: 'Spot price to query' },
      time: { type: 'number', description: 'Time point to query' },
    },
    required: ['volatility', 'spot', 'time'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/local-vol/constant', {
        volatility: args.volatility,
        spot: args.spot,
        time: args.time,
      }, apiKey);
      return { success: true, data: result, message: `Constant local vol: ${formatPercent(args.volatility as number)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const impliedToLocalTool: InternalTool = {
  name: 'quantlib_volatility_local_vol_implied_to_local',
  description: 'Convert implied volatility to local volatility using Dupire formula.',
  inputSchema: {
    type: 'object',
    properties: {
      spot: { type: 'number', description: 'Current spot price' },
      rate: { type: 'number', description: 'Risk-free rate' },
      strike: { type: 'number', description: 'Strike price' },
      expiry: { type: 'number', description: 'Time to expiry in years' },
      implied_vol: { type: 'number', description: 'Implied volatility' },
      dk: { type: 'number', description: 'Optional strike bump for numerical derivative' },
      dt: { type: 'number', description: 'Optional time bump for numerical derivative' },
    },
    required: ['spot', 'rate', 'strike', 'expiry', 'implied_vol'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/volatility/local-vol/implied-to-local', {
        spot: args.spot,
        rate: args.rate,
        strike: args.strike,
        expiry: args.expiry,
        implied_vol: args.implied_vol,
        dk: args.dk,
        dt: args.dt,
      }, apiKey);
      return { success: true, data: result, message: `Local vol: ${formatPercent(result.local_vol)} (from implied ${formatPercent(args.implied_vol as number)})` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EXPORT ALL TOOLS
// ══════════════════════════════════════════════════════════════════════════════

export const quantlibVolatilityTools: InternalTool[] = [
  // Surface (6)
  surfaceFlatTool,
  surfaceTermStructureTool,
  surfaceGridTool,
  surfaceSmileTool,
  surfaceTotalVarianceTool,
  surfaceFromPointsTool,
  // SABR (6)
  sabrImpliedVolTool,
  sabrNormalVolTool,
  sabrSmileTool,
  sabrCalibrateTool,
  sabrDensityTool,
  sabrDynamicsTool,
  // Local Vol (2)
  localVolConstantTool,
  impliedToLocalTool,
];
