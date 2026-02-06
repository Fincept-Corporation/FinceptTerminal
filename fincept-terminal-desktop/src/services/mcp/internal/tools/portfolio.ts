// Portfolio MCP Tools - Comprehensive Portfolio Management via MCP
// Exposes all portfolio functionality: CRUD, assets, transactions, analytics, optimization

import { InternalTool } from '../types';

export const portfolioTools: InternalTool[] = [
  // ==================== PORTFOLIO CRUD ====================
  {
    name: 'create_portfolio',
    description: 'Create a new investment portfolio for tracking holdings and performance',
    inputSchema: {
      type: 'object',
      properties: {
        name: { type: 'string', description: 'Portfolio name (e.g., "Tech Growth", "Dividend Income")' },
        currency: { type: 'string', description: 'Base currency (USD, EUR, INR, GBP, etc.)', default: 'USD' },
        description: { type: 'string', description: 'Optional portfolio description or investment thesis' },
        owner: { type: 'string', description: 'Portfolio owner name', default: 'User' },
      },
      required: ['name'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createPortfolio) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const result = await contexts.createPortfolio({
        name: args.name,
        owner: args.owner || 'User',
        currency: args.currency || 'USD',
        description: args.description,
      });
      return { success: true, data: result, message: `Portfolio "${args.name}" created successfully` };
    },
  },
  {
    name: 'list_portfolios',
    description: 'List all portfolios with names and basic info. NOTE: Use get_all_portfolios_with_holdings instead to see stocks/holdings inside portfolios.',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getPortfolios || !contexts.getPortfolioAssets) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const portfolios = await contexts.getPortfolios();

      if (portfolios.length === 0) {
        return {
          success: true,
          data: { portfolios: [], count: 0 },
          message: 'No portfolios found. Create one with create_portfolio.',
        };
      }

      // Get asset counts for each portfolio
      const portfoliosWithCounts = await Promise.all(
        portfolios.map(async (p: any) => {
          try {
            const assets = await contexts.getPortfolioAssets!(p.id);
            return {
              id: p.id,
              name: p.name,
              currency: p.currency || 'USD',
              description: p.description || '',
              created_at: p.created_at,
              holdings_count: assets.length,
              symbols: assets.map((a: any) => a.symbol),
            };
          } catch {
            return {
              id: p.id,
              name: p.name,
              currency: p.currency || 'USD',
              holdings_count: 0,
              symbols: [],
            };
          }
        })
      );

      // Build message
      let message = `Found ${portfolios.length} portfolio(s):\n\n`;
      portfoliosWithCounts.forEach((p: any) => {
        message += `• "${p.name}" (${p.currency})\n`;
        message += `  ID: ${p.id}\n`;
        message += `  Holdings: ${p.holdings_count} stock(s)`;
        if (p.symbols.length > 0) {
          message += ` - ${p.symbols.join(', ')}`;
        }
        message += '\n\n';
      });

      message += `TIP: Use get_all_portfolios_with_holdings or get_portfolio_summary to see detailed values and P&L.`;

      return {
        success: true,
        data: {
          portfolios: portfoliosWithCounts,
          count: portfolios.length,
        },
        message,
      };
    },
  },
  {
    name: 'get_portfolio',
    description: 'Get details of a specific portfolio by ID',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolio) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const portfolio = await contexts.getPortfolio(args.portfolio_id);
      if (!portfolio) {
        return { success: false, error: 'Portfolio not found' };
      }
      return { success: true, data: portfolio };
    },
  },
  {
    name: 'delete_portfolio',
    description: 'Delete a portfolio and all its associated data (holdings, transactions)',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID to delete' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deletePortfolio) {
        return { success: false, error: 'Portfolio context not available' };
      }
      await contexts.deletePortfolio(args.portfolio_id);
      return { success: true, message: 'Portfolio deleted successfully' };
    },
  },

  // ==================== ASSET MANAGEMENT ====================
  {
    name: 'add_asset',
    description: 'Add a new asset/holding to a portfolio (records a buy transaction)',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        symbol: { type: 'string', description: 'Asset symbol (e.g., AAPL, MSFT, RELIANCE.NS)' },
        quantity: { type: 'number', description: 'Number of shares/units to buy' },
        buy_price: { type: 'number', description: 'Purchase price per share/unit' },
      },
      required: ['portfolio_id', 'symbol', 'quantity', 'buy_price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addAsset) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const result = await contexts.addAsset(args.portfolio_id, {
        symbol: args.symbol,
        quantity: args.quantity,
        avg_buy_price: args.buy_price,
      });
      const totalCost = args.quantity * args.buy_price;
      return {
        success: true,
        data: result,
        message: `Added ${args.quantity} shares of ${args.symbol.toUpperCase()} @ $${args.buy_price} (Total: $${totalCost.toFixed(2)})`,
      };
    },
  },
  {
    name: 'sell_asset',
    description: 'Sell shares of an asset from a portfolio (partial or full)',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        symbol: { type: 'string', description: 'Asset symbol to sell' },
        quantity: { type: 'number', description: 'Number of shares/units to sell' },
        sell_price: { type: 'number', description: 'Selling price per share/unit' },
      },
      required: ['portfolio_id', 'symbol', 'quantity', 'sell_price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.sellAsset) {
        return { success: false, error: 'Portfolio context not available' };
      }
      await contexts.sellAsset(args.portfolio_id, args.symbol, args.quantity, args.sell_price);
      const totalProceeds = args.quantity * args.sell_price;
      return {
        success: true,
        message: `Sold ${args.quantity} shares of ${args.symbol.toUpperCase()} @ $${args.sell_price} (Proceeds: $${totalProceeds.toFixed(2)})`,
      };
    },
  },
  {
    name: 'remove_asset',
    description: 'Remove an asset completely from portfolio (sells all shares at current market price)',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        symbol: { type: 'string', description: 'Asset symbol to remove' },
        sell_price: { type: 'number', description: 'Optional: Selling price per share. If not provided, uses current market price' },
      },
      required: ['portfolio_id', 'symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.removeAsset) {
        return { success: false, error: 'Portfolio context not available' };
      }
      await contexts.removeAsset(args.portfolio_id, args.symbol, args.sell_price);
      return { success: true, message: `Removed all shares of ${args.symbol.toUpperCase()} from portfolio` };
    },
  },
  {
    name: 'get_portfolio_assets',
    description: 'Get all assets/holdings in a portfolio with quantities and average costs',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioAssets) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const assets = await contexts.getPortfolioAssets(args.portfolio_id);

      // Build message with asset details
      let message = `Portfolio has ${assets.length} asset(s)`;
      if (assets.length > 0) {
        message += ':\n';
        assets.forEach((a: any) => {
          message += `- ${a.symbol}: ${a.quantity} shares @ avg cost $${a.avg_buy_price?.toFixed(2)}\n`;
        });
      }

      return {
        success: true,
        data: assets,
        message,
      };
    },
  },

  // ==================== TRANSACTIONS ====================
  {
    name: 'add_transaction',
    description: 'Record a transaction (buy, sell, dividend, or split) in a portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        type: { type: 'string', description: 'Transaction type', enum: ['buy', 'sell', 'dividend', 'split'] },
        symbol: { type: 'string', description: 'Asset symbol' },
        quantity: { type: 'number', description: 'Number of shares/units' },
        price: { type: 'number', description: 'Price per unit' },
        date: { type: 'string', description: 'Transaction date (YYYY-MM-DD format)' },
        notes: { type: 'string', description: 'Optional notes about the transaction' },
      },
      required: ['portfolio_id', 'type', 'symbol', 'quantity', 'price'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addTransaction) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const { portfolio_id, ...tx } = args;
      const result = await contexts.addTransaction(portfolio_id, tx);
      return {
        success: true,
        data: result,
        message: `Recorded ${args.type.toUpperCase()} transaction: ${args.quantity} ${args.symbol} @ $${args.price}`,
      };
    },
  },
  {
    name: 'get_portfolio_transactions',
    description: 'Get transaction history for a portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        limit: { type: 'number', description: 'Maximum number of transactions to return', default: 100 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioTransactions) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const transactions = await contexts.getPortfolioTransactions(args.portfolio_id, args.limit || 100);
      return {
        success: true,
        data: transactions,
        message: `Found ${transactions.length} transaction(s)`,
      };
    },
  },
  {
    name: 'get_symbol_transactions',
    description: 'Get all transactions for a specific symbol in a portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        symbol: { type: 'string', description: 'Asset symbol to filter by' },
      },
      required: ['portfolio_id', 'symbol'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getSymbolTransactions) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const transactions = await contexts.getSymbolTransactions(args.portfolio_id, args.symbol);
      return {
        success: true,
        data: transactions,
        message: `Found ${transactions.length} transaction(s) for ${args.symbol.toUpperCase()}`,
      };
    },
  },

  // ==================== COMPREHENSIVE VIEWS ====================
  {
    name: 'get_my_stocks',
    description: 'Get ALL stocks/holdings across all portfolios with current prices, values, and P&L. USE THIS to answer: "what stocks do I have?", "show my positions", "what is my portfolio?", "what am I holding?", "my investments"',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getPortfolios || !contexts.getPortfolioSummary) {
        return { success: false, error: 'Portfolio context not available' };
      }

      const portfolios = await contexts.getPortfolios();
      if (portfolios.length === 0) {
        return {
          success: true,
          data: { portfolios: [], total_value: 0 },
          message: 'No portfolios found. Create one with create_portfolio.',
        };
      }

      // Fetch summary for each portfolio with COMPLETE data
      const portfolioDetails = await Promise.all(
        portfolios.map(async (p: any) => {
          try {
            const summary = await contexts.getPortfolioSummary!(p.id);
            return {
              portfolio_id: p.id,
              portfolio_name: p.name,
              currency: p.currency || 'USD',
              description: p.description || '',
              created_at: p.created_at,
              // Portfolio totals
              total_market_value: summary.total_market_value,
              total_cost_basis: summary.total_cost_basis,
              total_unrealized_pnl: summary.total_unrealized_pnl,
              total_unrealized_pnl_percent: summary.total_unrealized_pnl_percent,
              total_day_change: summary.total_day_change,
              total_day_change_percent: summary.total_day_change_percent,
              total_positions: summary.holdings?.length || 0,
              last_updated: summary.last_updated,
              // Complete holdings with all metrics
              holdings: summary.holdings?.map((h: any) => ({
                symbol: h.symbol,
                quantity: h.quantity,
                average_buy_price: h.avg_buy_price,
                current_price: h.current_price,
                market_value: h.market_value,
                cost_basis: h.cost_basis,
                unrealized_pnl: h.unrealized_pnl,
                unrealized_pnl_percent: h.unrealized_pnl_percent,
                day_change: h.day_change,
                day_change_percent: h.day_change_percent,
                portfolio_weight_percent: h.weight,
                first_purchase_date: h.first_purchase_date,
              })) || [],
            };
          } catch (err) {
            return {
              portfolio_id: p.id,
              portfolio_name: p.name,
              currency: p.currency || 'USD',
              error: `Failed to fetch: ${err}`,
              holdings: [],
            };
          }
        })
      );

      // Calculate grand totals
      let grandTotalValue = 0;
      let grandTotalCost = 0;
      let grandTotalPnL = 0;
      let totalStocks = 0;

      portfolioDetails.forEach((p: any) => {
        if (!p.error) {
          grandTotalValue += p.total_market_value || 0;
          grandTotalCost += p.total_cost_basis || 0;
          grandTotalPnL += p.total_unrealized_pnl || 0;
          totalStocks += p.holdings?.length || 0;
        }
      });

      const grandTotalPnLPercent = grandTotalCost > 0 ? (grandTotalPnL / grandTotalCost) * 100 : 0;

      // Build comprehensive message for AI context
      let message = `=== PORTFOLIO OVERVIEW ===\n`;
      message += `Total Portfolios: ${portfolios.length}\n`;
      message += `Total Stocks Held: ${totalStocks}\n`;
      message += `Grand Total Value: $${grandTotalValue.toFixed(2)}\n`;
      message += `Grand Total Cost: $${grandTotalCost.toFixed(2)}\n`;
      message += `Grand Total P&L: $${grandTotalPnL.toFixed(2)} (${grandTotalPnLPercent >= 0 ? '+' : ''}${grandTotalPnLPercent.toFixed(2)}%)\n\n`;

      portfolioDetails.forEach((p: any) => {
        message += `--- ${p.portfolio_name} (${p.currency}) ---\n`;
        if (p.error) {
          message += `Error: ${p.error}\n\n`;
          return;
        }

        message += `Value: $${p.total_market_value?.toFixed(2)} | Cost: $${p.total_cost_basis?.toFixed(2)} | P&L: ${p.total_unrealized_pnl >= 0 ? '+' : ''}$${p.total_unrealized_pnl?.toFixed(2)} (${p.total_unrealized_pnl_percent?.toFixed(2)}%)\n`;
        message += `Day Change: ${p.total_day_change >= 0 ? '+' : ''}$${p.total_day_change?.toFixed(2)} (${p.total_day_change_percent?.toFixed(2)}%)\n`;

        if (p.holdings && p.holdings.length > 0) {
          message += `\nHoldings (${p.holdings.length} stocks):\n`;
          p.holdings.forEach((h: any) => {
            const pnlSign = h.unrealized_pnl >= 0 ? '+' : '';
            message += `  ${h.symbol}: ${h.quantity} shares\n`;
            message += `    Buy: $${h.average_buy_price?.toFixed(2)} | Now: $${h.current_price?.toFixed(2)} | Value: $${h.market_value?.toFixed(2)}\n`;
            message += `    P&L: ${pnlSign}$${h.unrealized_pnl?.toFixed(2)} (${pnlSign}${h.unrealized_pnl_percent?.toFixed(1)}%) | Weight: ${h.portfolio_weight_percent?.toFixed(1)}%\n`;
          });
        } else {
          message += `No holdings in this portfolio.\n`;
        }
        message += `\n`;
      });

      return {
        success: true,
        data: {
          summary: {
            total_portfolios: portfolios.length,
            total_stocks: totalStocks,
            grand_total_value: grandTotalValue,
            grand_total_cost: grandTotalCost,
            grand_total_pnl: grandTotalPnL,
            grand_total_pnl_percent: grandTotalPnLPercent,
          },
          portfolios: portfolioDetails,
        },
        message,
      };
    },
  },

  // ==================== SUMMARY & ANALYTICS ====================
  {
    name: 'get_portfolio_summary',
    description: 'Get comprehensive portfolio summary with holdings, current prices, P&L, day changes, and allocation weights for a specific portfolio',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioSummary) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const summary = await contexts.getPortfolioSummary(args.portfolio_id);

      // Transform holdings to include all relevant data with clear names
      const enrichedHoldings = summary.holdings?.map((h: any) => ({
        symbol: h.symbol,
        quantity: h.quantity,
        average_buy_price: h.avg_buy_price,
        current_price: h.current_price,
        market_value: h.market_value,
        cost_basis: h.cost_basis,
        unrealized_pnl: h.unrealized_pnl,
        unrealized_pnl_percent: h.unrealized_pnl_percent,
        day_change: h.day_change,
        day_change_percent: h.day_change_percent,
        portfolio_weight_percent: h.weight,
        first_purchase_date: h.first_purchase_date,
      })) || [];

      // Build detailed message with all data
      const pName = summary.portfolio?.name || 'Portfolio';
      let message = `=== ${pName} ===\n`;
      message += `Currency: ${summary.portfolio?.currency || 'USD'}\n`;
      message += `Total Market Value: $${summary.total_market_value?.toFixed(2)}\n`;
      message += `Total Cost Basis: $${summary.total_cost_basis?.toFixed(2)}\n`;
      message += `Unrealized P&L: ${summary.total_unrealized_pnl >= 0 ? '+' : ''}$${summary.total_unrealized_pnl?.toFixed(2)} (${summary.total_unrealized_pnl_percent?.toFixed(2)}%)\n`;
      message += `Day Change: ${summary.total_day_change >= 0 ? '+' : ''}$${summary.total_day_change?.toFixed(2)} (${summary.total_day_change_percent?.toFixed(2)}%)\n`;
      message += `Total Positions: ${summary.holdings?.length || 0}\n`;
      message += `Last Updated: ${summary.last_updated}\n\n`;

      if (enrichedHoldings.length > 0) {
        message += `--- Holdings ---\n`;
        enrichedHoldings.forEach((h: any) => {
          const pnlSign = h.unrealized_pnl >= 0 ? '+' : '';
          const daySign = h.day_change >= 0 ? '+' : '';
          message += `\n${h.symbol}:\n`;
          message += `  Shares: ${h.quantity} | Avg Cost: $${h.average_buy_price?.toFixed(2)} | Current: $${h.current_price?.toFixed(2)}\n`;
          message += `  Value: $${h.market_value?.toFixed(2)} | Cost: $${h.cost_basis?.toFixed(2)}\n`;
          message += `  P&L: ${pnlSign}$${h.unrealized_pnl?.toFixed(2)} (${pnlSign}${h.unrealized_pnl_percent?.toFixed(2)}%)\n`;
          message += `  Day: ${daySign}$${h.day_change?.toFixed(2)} (${daySign}${h.day_change_percent?.toFixed(2)}%)\n`;
          message += `  Weight: ${h.portfolio_weight_percent?.toFixed(1)}%\n`;
        });
      } else {
        message += 'No holdings in this portfolio.\n';
      }

      return {
        success: true,
        data: {
          portfolio: {
            id: summary.portfolio?.id,
            name: summary.portfolio?.name,
            currency: summary.portfolio?.currency,
            description: summary.portfolio?.description,
          },
          totals: {
            market_value: summary.total_market_value,
            cost_basis: summary.total_cost_basis,
            unrealized_pnl: summary.total_unrealized_pnl,
            unrealized_pnl_percent: summary.total_unrealized_pnl_percent,
            day_change: summary.total_day_change,
            day_change_percent: summary.total_day_change_percent,
            positions_count: summary.holdings?.length || 0,
          },
          holdings: enrichedHoldings,
          last_updated: summary.last_updated,
        },
        message,
      };
    },
  },
  {
    name: 'calculate_advanced_metrics',
    description: 'Calculate advanced portfolio metrics: Sharpe ratio, volatility, VaR, max drawdown, returns. Use for risk analysis.',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.calculateAdvancedMetrics) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const metrics = await contexts.calculateAdvancedMetrics(args.portfolio_id);

      // Format metrics with clear labels and interpretations
      const sharpeInterpretation = metrics.sharpe_ratio > 1 ? 'Good' : metrics.sharpe_ratio > 0.5 ? 'Acceptable' : 'Poor';
      const volatilityLevel = metrics.portfolio_volatility > 0.3 ? 'High' : metrics.portfolio_volatility > 0.15 ? 'Moderate' : 'Low';

      let message = `=== Advanced Portfolio Metrics ===\n\n`;
      message += `Sharpe Ratio: ${metrics.sharpe_ratio?.toFixed(3)} (${sharpeInterpretation})\n`;
      message += `  - Measures risk-adjusted return. Higher is better. >1 is good, >2 is excellent.\n\n`;
      message += `Portfolio Volatility: ${(metrics.portfolio_volatility * 100)?.toFixed(2)}% annualized (${volatilityLevel})\n`;
      message += `  - Standard deviation of returns. Lower means more stable.\n\n`;
      message += `Value at Risk (95%): ${(metrics.value_at_risk_95 * 100)?.toFixed(2)}%\n`;
      message += `  - Maximum expected daily loss with 95% confidence.\n\n`;
      message += `Max Drawdown: ${(metrics.max_drawdown * 100)?.toFixed(2)}%\n`;
      message += `  - Largest peak-to-trough decline.\n\n`;
      message += `Portfolio Return: ${(metrics.portfolio_return * 100)?.toFixed(2)}%\n`;
      message += `  - Total return since inception.\n`;

      return {
        success: true,
        data: {
          sharpe_ratio: metrics.sharpe_ratio,
          sharpe_interpretation: sharpeInterpretation,
          portfolio_volatility: metrics.portfolio_volatility,
          portfolio_volatility_percent: metrics.portfolio_volatility * 100,
          volatility_level: volatilityLevel,
          value_at_risk_95: metrics.value_at_risk_95,
          value_at_risk_95_percent: metrics.value_at_risk_95 * 100,
          max_drawdown: metrics.max_drawdown,
          max_drawdown_percent: metrics.max_drawdown * 100,
          portfolio_return: metrics.portfolio_return,
          portfolio_return_percent: metrics.portfolio_return * 100,
        },
        message,
      };
    },
  },
  {
    name: 'calculate_risk_metrics',
    description: 'Calculate portfolio risk metrics including beta, Sharpe ratio, and drawdown analysis',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.calculateRiskMetrics) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const metrics = await contexts.calculateRiskMetrics(args.portfolio_id);
      return {
        success: true,
        data: metrics,
        message: 'Risk metrics calculated successfully',
      };
    },
  },

  // ==================== OPTIMIZATION ====================
  {
    name: 'optimize_portfolio',
    description: 'Optimize portfolio weights using various methods (efficient frontier, max Sharpe, min volatility)',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        method: {
          type: 'string',
          description: 'Optimization method',
          enum: ['max_sharpe', 'min_volatility', 'equal_weight', 'risk_parity'],
          default: 'max_sharpe',
        },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.optimizePortfolioWeights) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const result = await contexts.optimizePortfolioWeights(args.portfolio_id, args.method || 'max_sharpe');
      return {
        success: true,
        data: result,
        message: `Optimization complete using ${args.method || 'max_sharpe'} method. Expected Sharpe: ${result.sharpe_ratio?.toFixed(2) || 'N/A'}`,
      };
    },
  },
  {
    name: 'get_rebalancing_recommendations',
    description: 'Get recommendations to rebalance portfolio to target allocations',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        target_allocations: {
          type: 'string',
          description: 'JSON string of target allocations (e.g., {"AAPL": 0.3, "MSFT": 0.3, "GOOGL": 0.4})',
        },
      },
      required: ['portfolio_id', 'target_allocations'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getRebalancingRecommendations) {
        return { success: false, error: 'Portfolio context not available' };
      }
      let targetAllocations: Record<string, number>;
      try {
        targetAllocations = JSON.parse(args.target_allocations);
      } catch {
        return { success: false, error: 'Invalid target_allocations JSON format' };
      }
      const recommendations = await contexts.getRebalancingRecommendations(args.portfolio_id, targetAllocations);
      return {
        success: true,
        data: recommendations,
        message: `Generated ${recommendations.length} rebalancing recommendation(s)`,
      };
    },
  },

  // ==================== ANALYSIS ====================
  {
    name: 'analyze_diversification',
    description: 'Analyze portfolio diversification across sectors, asset classes, and geographies',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.analyzeDiversification) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const analysis = await contexts.analyzeDiversification(args.portfolio_id);
      return {
        success: true,
        data: analysis,
        message: 'Diversification analysis complete',
      };
    },
  },
  {
    name: 'get_portfolio_performance_history',
    description: 'Get historical portfolio performance data (value snapshots over time)',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
        days: { type: 'number', description: 'Number of days of history to retrieve', default: 30 },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.getPortfolioPerformanceHistory) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const history = await contexts.getPortfolioPerformanceHistory(args.portfolio_id, args.days || 30);
      return {
        success: true,
        data: history,
        message: `Retrieved ${history.length} historical data point(s)`,
      };
    },
  },

  // ==================== EXPORT ====================
  {
    name: 'export_portfolio_csv',
    description: 'Export portfolio data (holdings and transactions) to CSV format',
    inputSchema: {
      type: 'object',
      properties: {
        portfolio_id: { type: 'string', description: 'Portfolio ID' },
      },
      required: ['portfolio_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.exportPortfolioCSV) {
        return { success: false, error: 'Portfolio context not available' };
      }
      const csv = await contexts.exportPortfolioCSV(args.portfolio_id);
      return {
        success: true,
        data: { csv_content: csv },
        content: csv,
        message: 'Portfolio exported to CSV successfully',
      };
    },
  },
];
