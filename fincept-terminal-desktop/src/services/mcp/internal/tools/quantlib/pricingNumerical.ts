// QuantLib Pricing - Binomial, Monte Carlo, and PDE tools (7)

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber } from '../../QuantLibMCPBridge';

export const quantlibPricingNumericalTools: InternalTool[] = [
  // ==================== BINOMIAL (3) ====================
  {
    name: 'quantlib_pricing_binomial_price',
    description: 'Price option using binomial tree (Cox-Ross-Rubinstein) - supports American exercise',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        steps: { type: 'number', description: 'Number of tree steps' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        exercise: { type: 'string', description: 'Exercise style', enum: ['european', 'american'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity', 'steps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/binomial/price', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          steps: args.steps,
          option_type: args.option_type || 'call',
          exercise: args.exercise || 'european',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Binomial ${args.exercise || 'european'} ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)} (${args.steps} steps)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_binomial_greeks',
    description: 'Calculate Greeks using binomial tree',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        steps: { type: 'number', description: 'Number of tree steps' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        exercise: { type: 'string', description: 'Exercise style', enum: ['european', 'american'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity', 'steps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/binomial/greeks', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          steps: args.steps,
          option_type: args.option_type || 'call',
          exercise: args.exercise || 'european',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Binomial Greeks: Δ=${formatNumber(result.delta, 4)}, Γ=${formatNumber(result.gamma, 6)}, Θ=${formatNumber(result.theta, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_binomial_convergence',
    description: 'Analyze binomial tree convergence as number of steps increases',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        max_steps: { type: 'number', description: 'Maximum number of steps to analyze' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        exercise: { type: 'string', description: 'Exercise style', enum: ['european', 'american'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity', 'max_steps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/binomial/convergence', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          max_steps: args.max_steps,
          option_type: args.option_type || 'call',
          exercise: args.exercise || 'european',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Binomial convergence analysis: ${args.max_steps} max steps`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== MONTE CARLO (2) ====================
  {
    name: 'quantlib_pricing_monte_carlo_price',
    description: 'Price option using Monte Carlo simulation',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        n_simulations: { type: 'number', description: 'Number of Monte Carlo simulations' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity', 'n_simulations'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/monte-carlo/price', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          n_simulations: args.n_simulations,
          option_type: args.option_type || 'call',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `MC ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)} ± ${formatNumber(result.std_error || result.se, 4)} (${args.n_simulations} sims)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_monte_carlo_barrier',
    description: 'Price barrier option using Monte Carlo simulation',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        barrier: { type: 'number', description: 'Barrier level' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        n_simulations: { type: 'number', description: 'Number of Monte Carlo simulations' },
        n_steps: { type: 'number', description: 'Number of time steps per path' },
        barrier_type: { type: 'string', description: 'Barrier type', enum: ['down-and-out', 'down-and-in', 'up-and-out', 'up-and-in'] },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'barrier', 'risk_free_rate', 'volatility', 'time_to_maturity', 'n_simulations', 'n_steps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/monte-carlo/barrier', {
          spot: args.spot,
          strike: args.strike,
          barrier: args.barrier,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          n_simulations: args.n_simulations,
          n_steps: args.n_steps,
          barrier_type: args.barrier_type || 'down-and-out',
          option_type: args.option_type || 'call',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `MC Barrier (${args.barrier_type || 'down-and-out'}) ${args.option_type || 'call'}: ${formatNumber(result.price, 4)} (B=${args.barrier})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== PDE / FINITE DIFFERENCE (2) ====================
  {
    name: 'quantlib_pricing_pde_price',
    description: 'Price option using PDE/finite difference method - supports American exercise',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        n_spot: { type: 'number', description: 'Number of spot grid points' },
        n_time: { type: 'number', description: 'Number of time steps' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        exercise: { type: 'string', description: 'Exercise style', enum: ['european', 'american'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity', 'n_spot', 'n_time'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/pde/price', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          n_spot: args.n_spot,
          n_time: args.n_time,
          option_type: args.option_type || 'call',
          exercise: args.exercise || 'european',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `PDE ${args.exercise || 'european'} ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)} (${args.n_spot}x${args.n_time} grid)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_pde_greeks',
    description: 'Calculate Greeks using PDE/finite difference method',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        n_spot: { type: 'number', description: 'Number of spot grid points' },
        n_time: { type: 'number', description: 'Number of time steps' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        exercise: { type: 'string', description: 'Exercise style', enum: ['european', 'american'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity', 'n_spot', 'n_time'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/pde/greeks', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          n_spot: args.n_spot,
          n_time: args.n_time,
          option_type: args.option_type || 'call',
          exercise: args.exercise || 'european',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `PDE Greeks: Δ=${formatNumber(result.delta, 4)}, Γ=${formatNumber(result.gamma, 6)}, Θ=${formatNumber(result.theta, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
];
