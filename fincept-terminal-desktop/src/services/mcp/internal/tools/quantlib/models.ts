// QuantLib Models MCP Tools
// 14 tools for short rate models, Hull-White, Heston, Merton, Kou, Variance Gamma, Dupire, SVI

import type { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

// ══════════════════════════════════════════════════════════════════════════════
// SHORT RATE MODELS (4 tools)
// ══════════════════════════════════════════════════════════════════════════════

const shortRateBondPriceTool: InternalTool = {
  name: 'quantlib_models_short_rate_bond_price',
  description: 'Price zero-coupon bonds using short rate models (Vasicek, CIR, Hull-White).',
  inputSchema: {
    type: 'object',
    properties: {
      model: { type: 'string', description: 'Model type: vasicek, cir, or hull_white (default: vasicek)' },
      kappa: { type: 'number', description: 'Mean reversion speed' },
      theta: { type: 'number', description: 'Long-term mean rate' },
      sigma: { type: 'number', description: 'Volatility' },
      r0: { type: 'number', description: 'Initial short rate' },
      maturities: { type: 'string', description: 'JSON array of bond maturities in years' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/short-rate/bond-price', {
        model: args.model,
        kappa: args.kappa,
        theta: args.theta,
        sigma: args.sigma,
        r0: args.r0,
        maturities: args.maturities ? JSON.parse(args.maturities as string) : undefined,
      }, apiKey);
      return { success: true, data: result, message: `Priced bonds using ${args.model || 'vasicek'} model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const shortRateYieldCurveTool: InternalTool = {
  name: 'quantlib_models_short_rate_yield_curve',
  description: 'Generate yield curve from short rate model parameters.',
  inputSchema: {
    type: 'object',
    properties: {
      model: { type: 'string', description: 'Model type: vasicek, cir, or hull_white' },
      kappa: { type: 'number', description: 'Mean reversion speed' },
      theta: { type: 'number', description: 'Long-term mean rate' },
      sigma: { type: 'number', description: 'Volatility' },
      r0: { type: 'number', description: 'Initial short rate' },
      maturities: { type: 'string', description: 'JSON array of maturities in years' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/short-rate/yield-curve', {
        model: args.model,
        kappa: args.kappa,
        theta: args.theta,
        sigma: args.sigma,
        r0: args.r0,
        maturities: args.maturities ? JSON.parse(args.maturities as string) : undefined,
      }, apiKey);
      return { success: true, data: result, message: `Generated yield curve from ${args.model || 'vasicek'} model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const shortRateMonteCarloTool: InternalTool = {
  name: 'quantlib_models_short_rate_monte_carlo',
  description: 'Simulate short rate paths using Monte Carlo.',
  inputSchema: {
    type: 'object',
    properties: {
      model: { type: 'string', description: 'Model type: vasicek, cir, or hull_white' },
      kappa: { type: 'number', description: 'Mean reversion speed' },
      theta: { type: 'number', description: 'Long-term mean rate' },
      sigma: { type: 'number', description: 'Volatility' },
      r0: { type: 'number', description: 'Initial short rate' },
      T: { type: 'number', description: 'Time horizon in years' },
      n_steps: { type: 'number', description: 'Number of time steps' },
      n_paths: { type: 'number', description: 'Number of simulation paths' },
      seed: { type: 'number', description: 'Random seed for reproducibility' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/short-rate/monte-carlo', {
        model: args.model,
        kappa: args.kappa,
        theta: args.theta,
        sigma: args.sigma,
        r0: args.r0,
        T: args.T,
        n_steps: args.n_steps,
        n_paths: args.n_paths,
        seed: args.seed,
      }, apiKey);
      return { success: true, data: result, message: `Simulated ${args.n_paths || 1000} paths over ${args.T || 1}Y` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const shortRateBondOptionTool: InternalTool = {
  name: 'quantlib_models_short_rate_bond_option',
  description: 'Price bond options using short rate models.',
  inputSchema: {
    type: 'object',
    properties: {
      model: { type: 'string', description: 'Model type: vasicek, cir, or hull_white' },
      kappa: { type: 'number', description: 'Mean reversion speed' },
      theta: { type: 'number', description: 'Long-term mean rate' },
      sigma: { type: 'number', description: 'Volatility' },
      r0: { type: 'number', description: 'Initial short rate' },
      T: { type: 'number', description: 'Option maturity in years' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/short-rate/bond-option', {
        model: args.model,
        kappa: args.kappa,
        theta: args.theta,
        sigma: args.sigma,
        r0: args.r0,
        T: args.T,
      }, apiKey);
      return { success: true, data: result, message: `Priced bond option using ${args.model || 'vasicek'} model` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// HULL-WHITE (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const hullWhiteCalibrateTool: InternalTool = {
  name: 'quantlib_models_hull_white_calibrate',
  description: 'Calibrate Hull-White model parameters to market data.',
  inputSchema: {
    type: 'object',
    properties: {
      kappa: { type: 'number', description: 'Initial guess for mean reversion' },
      sigma: { type: 'number', description: 'Initial guess for volatility' },
      r0: { type: 'number', description: 'Initial short rate' },
      market_tenors: { type: 'string', description: 'JSON array of market tenors' },
      market_rates: { type: 'string', description: 'JSON array of market rates' },
    },
    required: [],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/hull-white/calibrate', {
        kappa: args.kappa,
        sigma: args.sigma,
        r0: args.r0,
        market_tenors: args.market_tenors ? JSON.parse(args.market_tenors as string) : undefined,
        market_rates: args.market_rates ? JSON.parse(args.market_rates as string) : undefined,
      }, apiKey);
      return { success: true, data: result, message: `Calibrated Hull-White: κ=${formatNumber(result.kappa)}, σ=${formatNumber(result.sigma)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// HESTON MODEL (3 tools)
// ══════════════════════════════════════════════════════════════════════════════

const hestonPriceTool: InternalTool = {
  name: 'quantlib_models_heston_price',
  description: 'Price European options using the Heston stochastic volatility model.',
  inputSchema: {
    type: 'object',
    properties: {
      S0: { type: 'number', description: 'Initial spot price' },
      v0: { type: 'number', description: 'Initial variance' },
      r: { type: 'number', description: 'Risk-free rate' },
      kappa: { type: 'number', description: 'Mean reversion speed of variance' },
      theta: { type: 'number', description: 'Long-term variance' },
      sigma_v: { type: 'number', description: 'Volatility of variance (vol of vol)' },
      rho: { type: 'number', description: 'Correlation between spot and variance' },
      strike: { type: 'number', description: 'Option strike' },
      T: { type: 'number', description: 'Time to maturity in years' },
      option_type: { type: 'string', description: 'Option type: call or put (default: call)' },
    },
    required: ['S0', 'v0', 'r', 'kappa', 'theta', 'sigma_v', 'rho', 'strike', 'T'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/heston/price', {
        S0: args.S0,
        v0: args.v0,
        r: args.r,
        kappa: args.kappa,
        theta: args.theta,
        sigma_v: args.sigma_v,
        rho: args.rho,
        strike: args.strike,
        T: args.T,
        option_type: args.option_type,
      }, apiKey);
      return { success: true, data: result, message: `Heston ${args.option_type || 'call'} price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const hestonImpliedVolTool: InternalTool = {
  name: 'quantlib_models_heston_implied_vol',
  description: 'Calculate implied volatility from Heston model price.',
  inputSchema: {
    type: 'object',
    properties: {
      S0: { type: 'number', description: 'Initial spot price' },
      v0: { type: 'number', description: 'Initial variance' },
      r: { type: 'number', description: 'Risk-free rate' },
      kappa: { type: 'number', description: 'Mean reversion speed' },
      theta: { type: 'number', description: 'Long-term variance' },
      sigma_v: { type: 'number', description: 'Vol of vol' },
      rho: { type: 'number', description: 'Correlation' },
      strike: { type: 'number', description: 'Option strike' },
      T: { type: 'number', description: 'Time to maturity' },
      option_type: { type: 'string', description: 'Option type: call or put' },
    },
    required: ['S0', 'v0', 'r', 'kappa', 'theta', 'sigma_v', 'rho', 'strike', 'T'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/heston/implied-vol', {
        S0: args.S0,
        v0: args.v0,
        r: args.r,
        kappa: args.kappa,
        theta: args.theta,
        sigma_v: args.sigma_v,
        rho: args.rho,
        strike: args.strike,
        T: args.T,
        option_type: args.option_type,
      }, apiKey);
      return { success: true, data: result, message: `Heston implied vol: ${formatPercent(result.implied_vol)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const hestonMonteCarloTool: InternalTool = {
  name: 'quantlib_models_heston_monte_carlo',
  description: 'Price options using Heston Monte Carlo simulation.',
  inputSchema: {
    type: 'object',
    properties: {
      S0: { type: 'number', description: 'Initial spot price' },
      v0: { type: 'number', description: 'Initial variance' },
      r: { type: 'number', description: 'Risk-free rate' },
      kappa: { type: 'number', description: 'Mean reversion speed' },
      theta: { type: 'number', description: 'Long-term variance' },
      sigma_v: { type: 'number', description: 'Vol of vol' },
      rho: { type: 'number', description: 'Correlation' },
      strike: { type: 'number', description: 'Option strike' },
      T: { type: 'number', description: 'Time to maturity' },
      option_type: { type: 'string', description: 'Option type: call or put' },
      n_paths: { type: 'number', description: 'Number of Monte Carlo paths' },
      n_steps: { type: 'number', description: 'Number of time steps' },
      seed: { type: 'number', description: 'Random seed' },
    },
    required: ['S0', 'v0', 'r', 'kappa', 'theta', 'sigma_v', 'rho', 'strike', 'T'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/heston/monte-carlo', {
        S0: args.S0,
        v0: args.v0,
        r: args.r,
        kappa: args.kappa,
        theta: args.theta,
        sigma_v: args.sigma_v,
        rho: args.rho,
        strike: args.strike,
        T: args.T,
        option_type: args.option_type,
        n_paths: args.n_paths,
        n_steps: args.n_steps,
        seed: args.seed,
      }, apiKey);
      return { success: true, data: result, message: `Heston MC price: ${formatNumber(result.price)} (SE: ${formatNumber(result.std_error)})` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// MERTON JUMP DIFFUSION (2 tools)
// ══════════════════════════════════════════════════════════════════════════════

const mertonPriceTool: InternalTool = {
  name: 'quantlib_models_merton_price',
  description: 'Price options using Merton jump-diffusion model (analytical).',
  inputSchema: {
    type: 'object',
    properties: {
      S: { type: 'number', description: 'Spot price' },
      K: { type: 'number', description: 'Strike price' },
      T: { type: 'number', description: 'Time to maturity in years' },
      r: { type: 'number', description: 'Risk-free rate' },
      sigma: { type: 'number', description: 'Diffusion volatility' },
      lambda_jump: { type: 'number', description: 'Jump intensity (jumps per year)' },
      mu_jump: { type: 'number', description: 'Mean log jump size' },
      sigma_jump: { type: 'number', description: 'Jump size volatility' },
      q: { type: 'number', description: 'Dividend yield (optional)' },
    },
    required: ['S', 'K', 'T', 'r', 'sigma', 'lambda_jump', 'mu_jump', 'sigma_jump'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/merton/price', {
        S: args.S,
        K: args.K,
        T: args.T,
        r: args.r,
        sigma: args.sigma,
        lambda_jump: args.lambda_jump,
        mu_jump: args.mu_jump,
        sigma_jump: args.sigma_jump,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Merton price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

const mertonFFTTool: InternalTool = {
  name: 'quantlib_models_merton_fft',
  description: 'Price options using Merton model with FFT method.',
  inputSchema: {
    type: 'object',
    properties: {
      S: { type: 'number', description: 'Spot price' },
      K: { type: 'number', description: 'Strike price' },
      T: { type: 'number', description: 'Time to maturity' },
      r: { type: 'number', description: 'Risk-free rate' },
      sigma: { type: 'number', description: 'Diffusion volatility' },
      lambda_jump: { type: 'number', description: 'Jump intensity' },
      mu_jump: { type: 'number', description: 'Mean log jump size' },
      sigma_jump: { type: 'number', description: 'Jump size volatility' },
      q: { type: 'number', description: 'Dividend yield (optional)' },
    },
    required: ['S', 'K', 'T', 'r', 'sigma', 'lambda_jump', 'mu_jump', 'sigma_jump'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/merton/fft', {
        S: args.S,
        K: args.K,
        T: args.T,
        r: args.r,
        sigma: args.sigma,
        lambda_jump: args.lambda_jump,
        mu_jump: args.mu_jump,
        sigma_jump: args.sigma_jump,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Merton FFT price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// KOU DOUBLE EXPONENTIAL JUMP (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const kouPriceTool: InternalTool = {
  name: 'quantlib_models_kou_price',
  description: 'Price options using Kou double-exponential jump-diffusion model.',
  inputSchema: {
    type: 'object',
    properties: {
      S: { type: 'number', description: 'Spot price' },
      K: { type: 'number', description: 'Strike price' },
      T: { type: 'number', description: 'Time to maturity' },
      r: { type: 'number', description: 'Risk-free rate' },
      sigma: { type: 'number', description: 'Diffusion volatility' },
      lambda_jump: { type: 'number', description: 'Jump intensity' },
      p: { type: 'number', description: 'Probability of upward jump (0-1)' },
      eta1: { type: 'number', description: 'Rate of upward jump (>1)' },
      eta2: { type: 'number', description: 'Rate of downward jump (>0)' },
      q: { type: 'number', description: 'Dividend yield (optional)' },
    },
    required: ['S', 'K', 'T', 'r', 'sigma', 'lambda_jump', 'p', 'eta1', 'eta2'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/kou/price', {
        S: args.S,
        K: args.K,
        T: args.T,
        r: args.r,
        sigma: args.sigma,
        lambda_jump: args.lambda_jump,
        p: args.p,
        eta1: args.eta1,
        eta2: args.eta2,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Kou price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// VARIANCE GAMMA (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const varianceGammaPriceTool: InternalTool = {
  name: 'quantlib_models_variance_gamma_price',
  description: 'Price options using Variance Gamma model.',
  inputSchema: {
    type: 'object',
    properties: {
      S: { type: 'number', description: 'Spot price' },
      K: { type: 'number', description: 'Strike price' },
      T: { type: 'number', description: 'Time to maturity' },
      r: { type: 'number', description: 'Risk-free rate' },
      sigma: { type: 'number', description: 'Volatility parameter' },
      theta_vg: { type: 'number', description: 'Drift/skewness parameter' },
      nu: { type: 'number', description: 'Variance rate (kurtosis parameter)' },
      q: { type: 'number', description: 'Dividend yield (optional)' },
    },
    required: ['S', 'K', 'T', 'r', 'sigma', 'theta_vg', 'nu'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/variance-gamma/price', {
        S: args.S,
        K: args.K,
        T: args.T,
        r: args.r,
        sigma: args.sigma,
        theta_vg: args.theta_vg,
        nu: args.nu,
        q: args.q,
      }, apiKey);
      return { success: true, data: result, message: `Variance Gamma price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// DUPIRE LOCAL VOL (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const dupirePriceTool: InternalTool = {
  name: 'quantlib_models_dupire_price',
  description: 'Price options using Dupire local volatility model.',
  inputSchema: {
    type: 'object',
    properties: {
      spot: { type: 'number', description: 'Spot price' },
      r: { type: 'number', description: 'Risk-free rate' },
      q: { type: 'number', description: 'Dividend yield' },
      sigma: { type: 'number', description: 'Local volatility' },
      strike: { type: 'number', description: 'Strike price' },
      T: { type: 'number', description: 'Time to maturity' },
      option_type: { type: 'string', description: 'Option type: call or put' },
    },
    required: ['spot'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/dupire/price', {
        spot: args.spot,
        r: args.r,
        q: args.q,
        sigma: args.sigma,
        strike: args.strike,
        T: args.T,
        option_type: args.option_type,
      }, apiKey);
      return { success: true, data: result, message: `Dupire ${args.option_type || 'call'} price: ${formatNumber(result.price)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// SVI (STOCHASTIC VOLATILITY INSPIRED) (1 tool)
// ══════════════════════════════════════════════════════════════════════════════

const sviCalibrateTool: InternalTool = {
  name: 'quantlib_models_svi_calibrate',
  description: 'Calibrate SVI (Stochastic Volatility Inspired) parameters to market smile.',
  inputSchema: {
    type: 'object',
    properties: {
      strikes: { type: 'string', description: 'JSON array of strikes' },
      market_vols: { type: 'string', description: 'JSON array of market implied vols' },
      forward: { type: 'number', description: 'Forward price' },
      T: { type: 'number', description: 'Time to expiry' },
    },
    required: ['strikes', 'market_vols', 'forward', 'T'],
  },
  handler: async (args, contexts) => {
    try {
      const apiKey = contexts.getQuantLibApiKey?.() || null;
      const result = await quantlibApiCall('/quantlib/models/svi/calibrate', {
        strikes: JSON.parse(args.strikes as string),
        market_vols: JSON.parse(args.market_vols as string),
        forward: args.forward,
        T: args.T,
      }, apiKey);
      return { success: true, data: result, message: `Calibrated SVI: a=${formatNumber(result.a)}, b=${formatNumber(result.b)}, ρ=${formatNumber(result.rho)}, m=${formatNumber(result.m)}, σ=${formatNumber(result.sigma)}` };
    } catch (error) {
      return { success: false, error: (error as Error).message };
    }
  },
};

// ══════════════════════════════════════════════════════════════════════════════
// EXPORT ALL TOOLS
// ══════════════════════════════════════════════════════════════════════════════

export const quantlibModelsTools: InternalTool[] = [
  // Short Rate (4)
  shortRateBondPriceTool,
  shortRateYieldCurveTool,
  shortRateMonteCarloTool,
  shortRateBondOptionTool,
  // Hull-White (1)
  hullWhiteCalibrateTool,
  // Heston (3)
  hestonPriceTool,
  hestonImpliedVolTool,
  hestonMonteCarloTool,
  // Merton (2)
  mertonPriceTool,
  mertonFFTTool,
  // Kou (1)
  kouPriceTool,
  // Variance Gamma (1)
  varianceGammaPriceTool,
  // Dupire (1)
  dupirePriceTool,
  // SVI (1)
  sviCalibrateTool,
];
