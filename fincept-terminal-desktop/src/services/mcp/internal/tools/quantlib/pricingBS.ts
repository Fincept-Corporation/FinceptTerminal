// QuantLib Pricing - Black-Scholes tools (8)

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

export const quantlibPricingBSTools: InternalTool[] = [
  // ==================== BLACK-SCHOLES (8) ====================
  {
    name: 'quantlib_pricing_bs_price',
    description: 'Calculate Black-Scholes option price for European options',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price of the underlying' },
        strike: { type: 'number', description: 'Strike price of the option' },
        risk_free_rate: { type: 'number', description: 'Risk-free interest rate (decimal, e.g., 0.05 for 5%)' },
        volatility: { type: 'number', description: 'Annualized volatility (decimal, e.g., 0.2 for 20%)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        dividend_yield: { type: 'number', description: 'Continuous dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/price', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `BS ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)} (S=${args.spot}, K=${args.strike}, σ=${formatPercent(args.volatility)}, T=${args.time_to_maturity}y)`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_greeks',
    description: 'Calculate Black-Scholes Greeks (Delta, Gamma, Theta, Vega, Rho)',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/greeks', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Greeks: Δ=${formatNumber(result.delta, 4)}, Γ=${formatNumber(result.gamma, 6)}, Θ=${formatNumber(result.theta, 4)}, V=${formatNumber(result.vega, 4)}, ρ=${formatNumber(result.rho, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_implied_vol',
    description: 'Calculate implied volatility from market option price using Black-Scholes',
    inputSchema: {
      type: 'object',
      properties: {
        market_price: { type: 'number', description: 'Observed market price of the option' },
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['market_price', 'spot', 'strike', 'risk_free_rate', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/implied-vol', {
          market_price: args.market_price,
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Implied Volatility: ${formatPercent(result.implied_volatility || result.iv)} (market price: ${args.market_price})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_greeks_full',
    description: 'Calculate all Black-Scholes Greeks including higher-order (Vanna, Volga, Speed, etc.)',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/greeks-full', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Full Greeks computed: ${Object.keys(result).length} metrics`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_digital_call',
    description: 'Price a digital (binary) call option using Black-Scholes',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/digital-call', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Digital Call price: ${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_digital_put',
    description: 'Price a digital (binary) put option using Black-Scholes',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/digital-put', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Digital Put price: ${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_asset_or_nothing_call',
    description: 'Price an asset-or-nothing call option',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/asset-or-nothing-call', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Asset-or-Nothing Call price: ${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bs_asset_or_nothing_put',
    description: 'Price an asset-or-nothing put option',
    inputSchema: {
      type: 'object',
      properties: {
        spot: { type: 'number', description: 'Current spot price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        dividend_yield: { type: 'number', description: 'Dividend yield (decimal)' },
      },
      required: ['spot', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bs/asset-or-nothing-put', {
          spot: args.spot,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          dividend_yield: args.dividend_yield || 0,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Asset-or-Nothing Put price: ${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
];
