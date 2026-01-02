/**
 * Hedge Fund Agent Node
 * Execute hedge fund strategy agents
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { AgentBridge } from '../../adapters/AgentBridge';

export class HedgeFundAgentNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Hedge Fund Agent',
    name: 'hedgeFundAgent',
    icon: 'file:hedge-fund.svg',
    group: ['agents'],
    version: 1,
    subtitle: '={{$parameter["strategy"]}}',
    description: 'Get analysis from hedge fund strategy agents',
    defaults: {
      name: 'Hedge Fund Agent',
      color: '#6366f1',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Strategy',
        name: 'strategy',
        type: NodePropertyType.Options,
        default: 'long_short_equity',
        options: [
          { name: 'Long/Short Equity', value: 'long_short_equity', description: 'Traditional long/short stock picking' },
          { name: 'Global Macro', value: 'global_macro', description: 'Trade on macro economic themes' },
          { name: 'Event Driven', value: 'event_driven', description: 'M&A, spinoffs, restructuring' },
          { name: 'Quantitative', value: 'quantitative', description: 'Systematic, data-driven strategies' },
          { name: 'Distressed', value: 'distressed', description: 'Distressed debt and special situations' },
          { name: 'Activist', value: 'activist', description: 'Activist investing approach' },
          { name: 'Multi-Strategy', value: 'multi_strategy', description: 'Diversified approach' },
          { name: 'Merger Arbitrage', value: 'merger_arb', description: 'M&A arbitrage opportunities' },
          { name: 'Credit', value: 'credit', description: 'Credit and fixed income strategies' },
          { name: 'Volatility', value: 'volatility', description: 'Volatility trading strategies' },
        ],
        description: 'Hedge fund strategy to apply',
      },
      {
        displayName: 'Analysis Mode',
        name: 'analysisMode',
        type: NodePropertyType.Options,
        default: 'opportunity',
        options: [
          { name: 'Find Opportunities', value: 'opportunity', description: 'Identify trading opportunities' },
          { name: 'Analyze Position', value: 'position', description: 'Analyze a specific position' },
          { name: 'Portfolio Construction', value: 'portfolio', description: 'Build/optimize portfolio' },
          { name: 'Risk Management', value: 'risk', description: 'Risk analysis and hedging' },
          { name: 'Market Regime', value: 'regime', description: 'Identify market regime' },
        ],
        description: 'Type of analysis to perform',
      },
      {
        displayName: 'Market/Sector Focus',
        name: 'focus',
        type: NodePropertyType.Options,
        default: 'us_equities',
        options: [
          { name: 'US Equities', value: 'us_equities' },
          { name: 'International Equities', value: 'intl_equities' },
          { name: 'Emerging Markets', value: 'em' },
          { name: 'Fixed Income', value: 'fixed_income' },
          { name: 'Currencies/FX', value: 'fx' },
          { name: 'Commodities', value: 'commodities' },
          { name: 'Crypto', value: 'crypto' },
          { name: 'Multi-Asset', value: 'multi_asset' },
        ],
        description: 'Market or asset class focus',
      },
      {
        displayName: 'Symbol(s)',
        name: 'symbols',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'AAPL, MSFT, GOOGL',
        description: 'Symbols to analyze (for position analysis)',
        displayOptions: {
          show: { analysisMode: ['position'] },
        },
      },
      {
        displayName: 'Event Type',
        name: 'eventType',
        type: NodePropertyType.Options,
        default: 'merger',
        options: [
          { name: 'Merger/Acquisition', value: 'merger' },
          { name: 'Spinoff', value: 'spinoff' },
          { name: 'Restructuring', value: 'restructuring' },
          { name: 'Earnings', value: 'earnings' },
          { name: 'Activist Campaign', value: 'activist' },
          { name: 'Regulatory', value: 'regulatory' },
        ],
        description: 'Type of event to analyze',
        displayOptions: {
          show: { strategy: ['event_driven', 'merger_arb'] },
        },
      },
      {
        displayName: 'Risk Tolerance',
        name: 'riskTolerance',
        type: NodePropertyType.Options,
        default: 'moderate',
        options: [
          { name: 'Conservative', value: 'conservative' },
          { name: 'Moderate', value: 'moderate' },
          { name: 'Aggressive', value: 'aggressive' },
        ],
        description: 'Risk tolerance for recommendations',
      },
      {
        displayName: 'Time Horizon',
        name: 'timeHorizon',
        type: NodePropertyType.Options,
        default: 'medium',
        options: [
          { name: 'Intraday', value: 'intraday' },
          { name: 'Short-term (Days)', value: 'short' },
          { name: 'Medium-term (Weeks)', value: 'medium' },
          { name: 'Long-term (Months)', value: 'long' },
        ],
        description: 'Investment time horizon',
      },
      {
        displayName: 'Capital Allocation',
        name: 'capitalAllocation',
        type: NodePropertyType.Number,
        default: 100000,
        description: 'Capital available for allocation',
      },
      {
        displayName: 'Include Hedges',
        name: 'includeHedges',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include hedging recommendations',
      },
      {
        displayName: 'Max Positions',
        name: 'maxPositions',
        type: NodePropertyType.Number,
        default: 10,
        description: 'Maximum number of positions',
        displayOptions: {
          show: { analysisMode: ['opportunity', 'portfolio'] },
        },
      },
      {
        displayName: 'LLM Provider',
        name: 'llmProvider',
        type: NodePropertyType.Options,
        default: 'ollama',
        options: [
          { name: 'Ollama (Local)', value: 'ollama' },
          { name: 'OpenAI', value: 'openai' },
          { name: 'Anthropic', value: 'anthropic' },
        ],
        description: 'LLM provider',
      },
      {
        displayName: 'Model',
        name: 'model',
        type: NodePropertyType.String,
        default: 'llama3',
        description: 'Model to use',
      },
    ],
  };

  private static readonly strategyDescriptions: Record<string, { description: string; characteristics: string[] }> = {
    long_short_equity: {
      description: 'Combines long positions in undervalued stocks with short positions in overvalued stocks',
      characteristics: ['Stock selection', 'Sector analysis', 'Relative value', 'Factor exposure'],
    },
    global_macro: {
      description: 'Trades based on macroeconomic and geopolitical analysis across asset classes',
      characteristics: ['Interest rates', 'Currency movements', 'Political risk', 'Economic cycles'],
    },
    event_driven: {
      description: 'Profits from corporate events like mergers, restructurings, and spinoffs',
      characteristics: ['Deal spreads', 'Timeline analysis', 'Regulatory risk', 'Deal probability'],
    },
    quantitative: {
      description: 'Uses mathematical models and algorithms to identify trading opportunities',
      characteristics: ['Statistical arbitrage', 'Factor models', 'Machine learning', 'Backtesting'],
    },
    distressed: {
      description: 'Invests in distressed companies or debt',
      characteristics: ['Credit analysis', 'Bankruptcy law', 'Recovery rates', 'Reorganization'],
    },
    activist: {
      description: 'Takes significant positions to influence corporate decisions',
      characteristics: ['Governance', 'Strategic changes', 'Capital allocation', 'Board engagement'],
    },
    multi_strategy: {
      description: 'Diversified approach across multiple hedge fund strategies',
      characteristics: ['Diversification', 'Risk allocation', 'Correlation', 'Capital efficiency'],
    },
    merger_arb: {
      description: 'Profits from spreads in announced M&A transactions',
      characteristics: ['Deal certainty', 'Regulatory approval', 'Financing risk', 'Timing'],
    },
    credit: {
      description: 'Focus on credit instruments and fixed income opportunities',
      characteristics: ['Credit spreads', 'Default risk', 'Yield analysis', 'Duration'],
    },
    volatility: {
      description: 'Trades volatility as an asset class',
      characteristics: ['Implied vs realized', 'Vol surface', 'Greeks', 'Term structure'],
    },
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const strategy = this.getNodeParameter('strategy', 0) as string;
    const analysisMode = this.getNodeParameter('analysisMode', 0) as string;
    const focus = this.getNodeParameter('focus', 0) as string;
    const riskTolerance = this.getNodeParameter('riskTolerance', 0) as string;
    const timeHorizon = this.getNodeParameter('timeHorizon', 0) as string;
    const capitalAllocation = this.getNodeParameter('capitalAllocation', 0) as number;
    const includeHedges = this.getNodeParameter('includeHedges', 0) as boolean;
    const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
    const model = this.getNodeParameter('model', 0) as string;

    const inputData = this.getInputData();
    const strategyInfo = HedgeFundAgentNode.strategyDescriptions[strategy];

    // Build context based on analysis mode
    const context: Record<string, any> = {
      strategy,
      strategyInfo,
      focus,
      riskTolerance,
      timeHorizon,
      capitalAllocation,
      includeHedges,
    };

    let query = '';

    switch (analysisMode) {
      case 'opportunity': {
        const maxPositions = this.getNodeParameter('maxPositions', 0) as number;
        context.maxPositions = maxPositions;
        query = `Identify ${maxPositions} ${strategy} opportunities in ${focus} market with ${riskTolerance} risk tolerance and ${timeHorizon} time horizon.`;
        break;
      }

      case 'position': {
        const symbols = this.getNodeParameter('symbols', 0) as string;
        context.symbols = symbols.split(',').map(s => s.trim());
        query = `Analyze ${symbols} from a ${strategy} perspective. Provide entry/exit levels and risk metrics.`;
        break;
      }

      case 'portfolio': {
        const maxPositions = this.getNodeParameter('maxPositions', 0) as number;
        context.maxPositions = maxPositions;
        context.currentPortfolio = inputData[0]?.json?.portfolio || [];
        query = `Construct a ${strategy} portfolio with $${capitalAllocation} capital. Include position sizing and hedges.`;
        break;
      }

      case 'risk': {
        context.currentPortfolio = inputData[0]?.json?.portfolio || [];
        context.positions = inputData[0]?.json?.positions || [];
        query = `Analyze portfolio risk from a ${strategy} perspective. Identify exposures and suggest hedges.`;
        break;
      }

      case 'regime': {
        context.marketData = inputData[0]?.json?.marketData || {};
        query = `Identify current market regime for ${strategy} strategy in ${focus}. What adjustments are needed?`;
        break;
      }
    }

    // Add event-specific context
    if (strategy === 'event_driven' || strategy === 'merger_arb') {
      context.eventType = this.getNodeParameter('eventType', 0) as string;
    }

    try {
      const result = await AgentBridge.executeAgent({
        agentId: `hedgefund_${strategy}`,
        category: 'hedgeFund',
        parameters: {
          query,
          ...context,
        },
        llmProvider: llmProvider as any,
        llmModel: model,
      });

      const responseData = result.data || {};
      const output: Record<string, any> = {
        success: true,
        strategy,
        strategyDescription: strategyInfo?.description,
        strategyCharacteristics: strategyInfo?.characteristics,
        analysisMode,
        focus,
        riskTolerance,
        timeHorizon,
        capitalAllocation,
        analysis: responseData.response || responseData.analysis || '',
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
      };

      // Parse structured output if available
      if (responseData.opportunities) {
        output.opportunities = responseData.opportunities;
      }
      if (responseData.positions) {
        output.recommendedPositions = responseData.positions;
      }
      if (responseData.hedges && includeHedges) {
        output.hedges = responseData.hedges;
      }
      if (responseData.riskMetrics) {
        output.riskMetrics = responseData.riskMetrics;
      }
      if (responseData.marketRegime) {
        output.marketRegime = responseData.marketRegime;
      }

      return [[{ json: output }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          strategy,
          analysisMode,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }
}
