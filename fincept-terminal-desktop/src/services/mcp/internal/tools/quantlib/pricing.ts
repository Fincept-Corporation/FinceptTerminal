// QuantLib Pricing MCP Tools - 29 endpoints
// Black-Scholes, Black76, Bachelier, Kirk Spread, Binomial, Monte Carlo, PDE

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

export const quantlibPricingTools: InternalTool[] = [
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

  // ==================== BLACK76 (7) ====================
  {
    name: 'quantlib_pricing_black76_price',
    description: 'Calculate Black76 option price for futures/forward options',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/price', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Black76 ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)} (F=${args.forward}, K=${args.strike})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_black76_greeks',
    description: 'Calculate Black76 Greeks for futures/forward options',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/greeks', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Black76 Greeks: Δ=${formatNumber(result.delta, 4)}, Γ=${formatNumber(result.gamma, 6)}, V=${formatNumber(result.vega, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_black76_greeks_full',
    description: 'Calculate all Black76 Greeks including higher-order',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/greeks-full', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Black76 Full Greeks computed: ${Object.keys(result).length} metrics`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_black76_implied_vol',
    description: 'Calculate implied volatility from market price using Black76',
    inputSchema: {
      type: 'object',
      properties: {
        market_price: { type: 'number', description: 'Market price of the option' },
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['market_price', 'forward', 'strike', 'risk_free_rate', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/implied-vol', {
          market_price: args.market_price,
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Black76 Implied Vol: ${formatPercent(result.implied_volatility || result.iv)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_black76_caplet',
    description: 'Price a caplet using Black76 model',
    inputSchema: {
      type: 'object',
      properties: {
        forward_rate: { type: 'number', description: 'Forward rate' },
        strike: { type: 'number', description: 'Strike rate' },
        discount_factor: { type: 'number', description: 'Discount factor to payment date' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        t_start: { type: 'number', description: 'Caplet start time in years' },
        t_end: { type: 'number', description: 'Caplet end time in years' },
        notional: { type: 'number', description: 'Notional amount' },
      },
      required: ['forward_rate', 'strike', 'discount_factor', 'volatility', 't_start', 't_end'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/caplet', {
          forward_rate: args.forward_rate,
          strike: args.strike,
          discount_factor: args.discount_factor,
          volatility: args.volatility,
          t_start: args.t_start,
          t_end: args.t_end,
          notional: args.notional || 1,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Caplet price: ${formatNumber(result.price, 4)} (K=${formatPercent(args.strike)}, σ=${formatPercent(args.volatility)})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_black76_floorlet',
    description: 'Price a floorlet using Black76 model',
    inputSchema: {
      type: 'object',
      properties: {
        forward_rate: { type: 'number', description: 'Forward rate' },
        strike: { type: 'number', description: 'Strike rate' },
        discount_factor: { type: 'number', description: 'Discount factor to payment date' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        t_start: { type: 'number', description: 'Floorlet start time in years' },
        t_end: { type: 'number', description: 'Floorlet end time in years' },
        notional: { type: 'number', description: 'Notional amount' },
      },
      required: ['forward_rate', 'strike', 'discount_factor', 'volatility', 't_start', 't_end'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/floorlet', {
          forward_rate: args.forward_rate,
          strike: args.strike,
          discount_factor: args.discount_factor,
          volatility: args.volatility,
          t_start: args.t_start,
          t_end: args.t_end,
          notional: args.notional || 1,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Floorlet price: ${formatNumber(result.price, 4)} (K=${formatPercent(args.strike)}, σ=${formatPercent(args.volatility)})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_black76_swaption',
    description: 'Price a swaption using Black76 model',
    inputSchema: {
      type: 'object',
      properties: {
        forward_swap_rate: { type: 'number', description: 'Forward swap rate' },
        strike: { type: 'number', description: 'Strike rate' },
        annuity: { type: 'number', description: 'Swap annuity (PV01)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        t_expiry: { type: 'number', description: 'Time to expiry in years' },
        is_payer: { type: 'boolean', description: 'True for payer swaption, false for receiver' },
        notional: { type: 'number', description: 'Notional amount' },
      },
      required: ['forward_swap_rate', 'strike', 'annuity', 'volatility', 't_expiry'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/black76/swaption', {
          forward_swap_rate: args.forward_swap_rate,
          strike: args.strike,
          annuity: args.annuity,
          volatility: args.volatility,
          t_expiry: args.t_expiry,
          is_payer: args.is_payer !== false,
          notional: args.notional || 1,
        }, apiKey);
        const swaptionType = args.is_payer !== false ? 'Payer' : 'Receiver';
        return {
          success: true,
          data: result,
          message: `${swaptionType} Swaption price: ${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== BACHELIER (6) ====================
  {
    name: 'quantlib_pricing_bachelier_price',
    description: 'Calculate option price using Bachelier (normal) model',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        normal_volatility: { type: 'number', description: 'Normal volatility (absolute, not percentage)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'normal_volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bachelier/price', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          normal_volatility: args.normal_volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Bachelier ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bachelier_greeks',
    description: 'Calculate Bachelier model Greeks',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        normal_volatility: { type: 'number', description: 'Normal volatility' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'normal_volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bachelier/greeks', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          normal_volatility: args.normal_volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Bachelier Greeks: Δ=${formatNumber(result.delta, 4)}, Γ=${formatNumber(result.gamma, 6)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bachelier_greeks_full',
    description: 'Calculate all Bachelier model Greeks',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        normal_volatility: { type: 'number', description: 'Normal volatility' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'normal_volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bachelier/greeks-full', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          normal_volatility: args.normal_volatility,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Bachelier Full Greeks computed`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bachelier_implied_vol',
    description: 'Calculate implied normal volatility from market price',
    inputSchema: {
      type: 'object',
      properties: {
        market_price: { type: 'number', description: 'Market price of the option' },
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['market_price', 'forward', 'strike', 'risk_free_rate', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bachelier/implied-vol', {
          market_price: args.market_price,
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Bachelier Implied Normal Vol: ${formatNumber(result.implied_volatility || result.iv, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bachelier_vol_conversion',
    description: 'Convert between lognormal and normal volatility',
    inputSchema: {
      type: 'object',
      properties: {
        volatility: { type: 'number', description: 'Input volatility' },
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        direction: { type: 'string', description: 'Conversion direction', enum: ['lognormal_to_normal', 'normal_to_lognormal'] },
      },
      required: ['volatility', 'forward', 'strike', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bachelier/vol-conversion', {
          volatility: args.volatility,
          forward: args.forward,
          strike: args.strike,
          time_to_maturity: args.time_to_maturity,
          direction: args.direction || 'lognormal_to_normal',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Vol conversion (${args.direction || 'lognormal_to_normal'}): ${formatNumber(args.volatility, 4)} → ${formatNumber(result.converted_volatility || result.result, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_pricing_bachelier_shifted_lognormal',
    description: 'Price option using shifted lognormal model',
    inputSchema: {
      type: 'object',
      properties: {
        forward: { type: 'number', description: 'Forward price' },
        strike: { type: 'number', description: 'Strike price' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        volatility: { type: 'number', description: 'Volatility (decimal)' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        shift: { type: 'number', description: 'Shift parameter' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['forward', 'strike', 'risk_free_rate', 'volatility', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/bachelier/shifted-lognormal', {
          forward: args.forward,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          volatility: args.volatility,
          time_to_maturity: args.time_to_maturity,
          shift: args.shift || 0,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Shifted Lognormal price: ${formatNumber(result.price, 4)} (shift=${args.shift || 0})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== KIRK SPREAD (1) ====================
  {
    name: 'quantlib_pricing_kirk_spread',
    description: 'Price a spread option using Kirk approximation',
    inputSchema: {
      type: 'object',
      properties: {
        F1: { type: 'number', description: 'First forward price' },
        F2: { type: 'number', description: 'Second forward price' },
        strike: { type: 'number', description: 'Strike (spread level)' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate (decimal)' },
        sigma1: { type: 'number', description: 'Volatility of first asset (decimal)' },
        sigma2: { type: 'number', description: 'Volatility of second asset (decimal)' },
        rho: { type: 'number', description: 'Correlation between assets' },
        time_to_maturity: { type: 'number', description: 'Time to maturity in years' },
        option_type: { type: 'string', description: 'Option type', enum: ['call', 'put'] },
      },
      required: ['F1', 'F2', 'strike', 'risk_free_rate', 'sigma1', 'sigma2', 'rho', 'time_to_maturity'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/pricing/kirk/spread-price', {
          F1: args.F1,
          F2: args.F2,
          strike: args.strike,
          risk_free_rate: args.risk_free_rate,
          sigma1: args.sigma1,
          sigma2: args.sigma2,
          rho: args.rho,
          time_to_maturity: args.time_to_maturity,
          option_type: args.option_type || 'call',
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Kirk Spread ${args.option_type || 'call'} price: ${formatNumber(result.price, 4)} (F1=${args.F1}, F2=${args.F2}, K=${args.strike}, ρ=${args.rho})`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

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
