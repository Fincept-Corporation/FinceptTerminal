// QuantLib Portfolio MCP Tools - 15 endpoints
// Portfolio Optimization, Risk Metrics, Black-Litterman, Risk Parity

import { InternalTool } from '../../types';
import { quantlibApiCall, formatNumber, formatPercent } from '../../QuantLibMCPBridge';

export const quantlibPortfolioTools: InternalTool[] = [
  // ==================== OPTIMIZE (4) ====================
  {
    name: 'quantlib_portfolio_optimize_min_variance',
    description: 'Find minimum variance portfolio weights given expected returns and covariance matrix',
    inputSchema: {
      type: 'object',
      properties: {
        expected_returns: { type: 'string', description: 'JSON array of expected returns for each asset' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        rf_rate: { type: 'number', description: 'Risk-free rate (optional)' },
      },
      required: ['expected_returns', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/optimize/min-variance', {
          expected_returns: JSON.parse(args.expected_returns),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          rf_rate: args.rf_rate,
        }, apiKey);
        const weights = result.weights || result.optimal_weights;
        return {
          success: true,
          data: result,
          message: `Min Variance: σ=${formatPercent(result.volatility || result.portfolio_volatility)}, E[R]=${formatPercent(result.expected_return || result.portfolio_return)}, weights=[${weights?.map((w: number) => formatPercent(w)).join(', ') || 'N/A'}]`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_optimize_max_sharpe',
    description: 'Find maximum Sharpe ratio portfolio weights (tangency portfolio)',
    inputSchema: {
      type: 'object',
      properties: {
        expected_returns: { type: 'string', description: 'JSON array of expected returns' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        rf_rate: { type: 'number', description: 'Risk-free rate' },
      },
      required: ['expected_returns', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/optimize/max-sharpe', {
          expected_returns: JSON.parse(args.expected_returns),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          rf_rate: args.rf_rate,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Max Sharpe: SR=${formatNumber(result.sharpe_ratio, 4)}, E[R]=${formatPercent(result.expected_return || result.portfolio_return)}, σ=${formatPercent(result.volatility || result.portfolio_volatility)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_optimize_target_return',
    description: 'Find minimum variance portfolio for a target return',
    inputSchema: {
      type: 'object',
      properties: {
        expected_returns: { type: 'string', description: 'JSON array of expected returns' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        target_return: { type: 'number', description: 'Target portfolio return (decimal)' },
      },
      required: ['expected_returns', 'covariance_matrix', 'target_return'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/optimize/target-return', {
          expected_returns: JSON.parse(args.expected_returns),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          target_return: args.target_return,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Target Return ${formatPercent(args.target_return)}: achieved σ=${formatPercent(result.volatility || result.portfolio_volatility)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_optimize_efficient_frontier',
    description: 'Generate the efficient frontier - set of optimal portfolios',
    inputSchema: {
      type: 'object',
      properties: {
        expected_returns: { type: 'string', description: 'JSON array of expected returns' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        n_points: { type: 'number', description: 'Number of points on the frontier' },
      },
      required: ['expected_returns', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/optimize/efficient-frontier', {
          expected_returns: JSON.parse(args.expected_returns),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          n_points: args.n_points || 20,
        }, apiKey);
        const nPoints = result.frontier?.length || args.n_points || 20;
        return {
          success: true,
          data: result,
          message: `Efficient frontier: ${nPoints} portfolios from σ=${formatPercent(result.min_volatility || 0)} to σ=${formatPercent(result.max_volatility || 0)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== RISK (8) ====================
  {
    name: 'quantlib_portfolio_risk_comprehensive',
    description: 'Comprehensive portfolio risk analysis including multiple metrics',
    inputSchema: {
      type: 'object',
      properties: {
        weights: { type: 'string', description: 'JSON array of portfolio weights' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        returns: { type: 'string', description: 'JSON array of historical returns (optional)' },
        benchmark_returns: { type: 'string', description: 'JSON array of benchmark returns (optional)' },
        rf_rate: { type: 'number', description: 'Risk-free rate' },
      },
      required: ['weights', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/comprehensive', {
          weights: JSON.parse(args.weights),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          returns: args.returns ? JSON.parse(args.returns) : undefined,
          benchmark_returns: args.benchmark_returns ? JSON.parse(args.benchmark_returns) : undefined,
          rf_rate: args.rf_rate,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Portfolio Risk: σ=${formatPercent(result.volatility)}, Sharpe=${formatNumber(result.sharpe_ratio, 4)}, Sortino=${formatNumber(result.sortino_ratio, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_var',
    description: 'Calculate portfolio Value at Risk',
    inputSchema: {
      type: 'object',
      properties: {
        weights: { type: 'string', description: 'JSON array of portfolio weights' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        confidence: { type: 'number', description: 'Confidence level (e.g., 0.95, 0.99)' },
        horizon: { type: 'number', description: 'Holding period in days' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
        method: { type: 'string', description: 'VaR method', enum: ['parametric', 'historical', 'monte_carlo'] },
      },
      required: ['weights', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/var', {
          weights: JSON.parse(args.weights),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          confidence: args.confidence,
          horizon: args.horizon,
          portfolio_value: args.portfolio_value,
          method: args.method,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Portfolio VaR (${(args.confidence || 0.95) * 100}%): ${formatNumber(result.var || result.VaR, 2)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_cvar',
    description: 'Calculate portfolio Conditional VaR (Expected Shortfall)',
    inputSchema: {
      type: 'object',
      properties: {
        weights: { type: 'string', description: 'JSON array of portfolio weights' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        confidence: { type: 'number', description: 'Confidence level' },
        horizon: { type: 'number', description: 'Holding period' },
        portfolio_value: { type: 'number', description: 'Portfolio value' },
        method: { type: 'string', description: 'Method', enum: ['parametric', 'historical'] },
      },
      required: ['weights', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/cvar', {
          weights: JSON.parse(args.weights),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          confidence: args.confidence,
          horizon: args.horizon,
          portfolio_value: args.portfolio_value,
          method: args.method,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Portfolio CVaR/ES (${(args.confidence || 0.95) * 100}%): ${formatNumber(result.cvar || result.expected_shortfall, 2)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_contribution',
    description: 'Calculate risk contribution of each asset to portfolio variance',
    inputSchema: {
      type: 'object',
      properties: {
        weights: { type: 'string', description: 'JSON array of portfolio weights' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
      },
      required: ['weights', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/contribution', {
          weights: JSON.parse(args.weights),
          covariance_matrix: JSON.parse(args.covariance_matrix),
        }, apiKey);
        const contributions = result.risk_contributions || result.contributions;
        return {
          success: true,
          data: result,
          message: `Risk contributions: [${contributions?.map((c: number) => formatPercent(c)).join(', ') || 'N/A'}]`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_ratios',
    description: 'Calculate portfolio performance ratios (Sharpe, Sortino, Calmar, etc.)',
    inputSchema: {
      type: 'object',
      properties: {
        returns: { type: 'string', description: 'JSON array of portfolio returns' },
        rf_rate: { type: 'number', description: 'Risk-free rate' },
        benchmark_returns: { type: 'string', description: 'JSON array of benchmark returns' },
        target_return: { type: 'number', description: 'Target return for Sortino ratio' },
      },
      required: ['returns'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/ratios', {
          returns: JSON.parse(args.returns),
          rf_rate: args.rf_rate,
          benchmark_returns: args.benchmark_returns ? JSON.parse(args.benchmark_returns) : undefined,
          target_return: args.target_return,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Ratios: Sharpe=${formatNumber(result.sharpe, 4)}, Sortino=${formatNumber(result.sortino, 4)}, Calmar=${formatNumber(result.calmar, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_incremental_var',
    description: 'Calculate incremental VaR from adding/changing a position',
    inputSchema: {
      type: 'object',
      properties: {
        weights: { type: 'string', description: 'JSON array of current weights' },
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        new_weight: { type: 'number', description: 'New weight for the asset' },
        asset_index: { type: 'number', description: 'Index of asset being changed' },
        confidence: { type: 'number', description: 'Confidence level' },
      },
      required: ['weights', 'covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/incremental-var', {
          weights: JSON.parse(args.weights),
          covariance_matrix: JSON.parse(args.covariance_matrix),
          new_weight: args.new_weight,
          asset_index: args.asset_index,
          confidence: args.confidence,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Incremental VaR: ${formatNumber(result.incremental_var, 4)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_inverse_volatility',
    description: 'Calculate inverse volatility (1/σ) weighted portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
      },
      required: ['covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/inverse-volatility', {
          covariance_matrix: JSON.parse(args.covariance_matrix),
        }, apiKey);
        const weights = result.weights || result.inverse_vol_weights;
        return {
          success: true,
          data: result,
          message: `Inverse Vol Weights: [${weights?.map((w: number) => formatPercent(w)).join(', ') || 'N/A'}]`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_risk_comprehensive_analysis',
    description: 'Full portfolio risk analysis with returns data',
    inputSchema: {
      type: 'object',
      properties: {
        returns: { type: 'string', description: 'JSON 2D array of asset returns (each row is an asset)' },
        weights: { type: 'string', description: 'JSON array of portfolio weights' },
        confidence: { type: 'number', description: 'Confidence level for VaR/ES' },
        risk_free_rate: { type: 'number', description: 'Risk-free rate' },
      },
      required: ['returns', 'weights'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk/portfolio-comprehensive', {
          returns: JSON.parse(args.returns),
          weights: JSON.parse(args.weights),
          confidence: args.confidence,
          risk_free_rate: args.risk_free_rate,
        }, apiKey);
        return {
          success: true,
          data: result,
          message: `Comprehensive Analysis: σ=${formatPercent(result.volatility)}, VaR=${formatNumber(result.var, 4)}, MaxDD=${formatPercent(result.max_drawdown)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== BLACK-LITTERMAN (2) ====================
  {
    name: 'quantlib_portfolio_black_litterman_equilibrium',
    description: 'Calculate Black-Litterman equilibrium (implied) returns from market cap weights',
    inputSchema: {
      type: 'object',
      properties: {
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        market_caps: { type: 'string', description: 'JSON array of market capitalizations' },
        risk_aversion: { type: 'number', description: 'Risk aversion parameter (default ~2.5)' },
      },
      required: ['covariance_matrix', 'market_caps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/black-litterman/equilibrium', {
          covariance_matrix: JSON.parse(args.covariance_matrix),
          market_caps: JSON.parse(args.market_caps),
          risk_aversion: args.risk_aversion,
        }, apiKey);
        const returns = result.equilibrium_returns || result.implied_returns;
        return {
          success: true,
          data: result,
          message: `BL Equilibrium Returns: [${returns?.map((r: number) => formatPercent(r)).join(', ') || 'N/A'}]`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
  {
    name: 'quantlib_portfolio_black_litterman_posterior',
    description: 'Calculate Black-Litterman posterior returns and optimal weights',
    inputSchema: {
      type: 'object',
      properties: {
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        market_caps: { type: 'string', description: 'JSON array of market caps' },
        views: { type: 'string', description: 'JSON array of view objects with P (picking matrix row), Q (view return), confidence' },
        risk_aversion: { type: 'number', description: 'Risk aversion parameter' },
      },
      required: ['covariance_matrix', 'market_caps'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const body: any = {
          covariance_matrix: JSON.parse(args.covariance_matrix),
          market_caps: JSON.parse(args.market_caps),
          risk_aversion: args.risk_aversion,
        };
        if (args.views) body.views = JSON.parse(args.views);

        const result = await quantlibApiCall('/quantlib/portfolio/black-litterman/posterior', body, apiKey);
        return {
          success: true,
          data: result,
          message: `BL Posterior: ${result.posterior_returns?.length || 'N/A'} assets, optimal weights calculated`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },

  // ==================== RISK PARITY (1) ====================
  {
    name: 'quantlib_portfolio_risk_parity',
    description: 'Calculate risk parity portfolio - equal risk contribution from each asset',
    inputSchema: {
      type: 'object',
      properties: {
        covariance_matrix: { type: 'string', description: 'JSON 2D array covariance matrix' },
        method: { type: 'string', description: 'Optimization method', enum: ['slsqp', 'ccd', 'newton'] },
      },
      required: ['covariance_matrix'],
    },
    handler: async (args, contexts) => {
      try {
        const apiKey = contexts.getQuantLibApiKey?.() || null;
        const result = await quantlibApiCall('/quantlib/portfolio/risk-parity', {
          covariance_matrix: JSON.parse(args.covariance_matrix),
          method: args.method,
        }, apiKey);
        const weights = result.weights || result.risk_parity_weights;
        return {
          success: true,
          data: result,
          message: `Risk Parity Weights: [${weights?.map((w: number) => formatPercent(w)).join(', ') || 'N/A'}], σ=${formatPercent(result.portfolio_volatility)}`,
        };
      } catch (error) {
        return { success: false, error: (error as Error).message };
      }
    },
  },
];
