// QuantLib Pricing - Black76, Bachelier, and Kirk Spread tools (14)

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

export const quantlibPricingBlack76BachelierTools: InternalTool[] = [
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
];
