/**
 * Investor Agent Node
 * Execute famous investor persona agents (Buffett, Graham, Lynch, etc.)
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

export class InvestorAgentNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Investor Agent',
    name: 'investorAgent',
    icon: 'file:investor.svg',
    group: ['agents'],
    version: 1,
    subtitle: '={{$parameter["investor"]}}',
    description: 'Get investment analysis from famous investor personas',
    defaults: {
      name: 'Investor Agent',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Investor',
        name: 'investor',
        type: NodePropertyType.Options,
        default: 'warren_buffett',
        options: [
          { name: 'Warren Buffett', value: 'warren_buffett', description: 'Value investing, economic moats, long-term' },
          { name: 'Benjamin Graham', value: 'benjamin_graham', description: 'Father of value investing, margin of safety' },
          { name: 'Charlie Munger', value: 'charlie_munger', description: 'Mental models, multidisciplinary approach' },
          { name: 'Peter Lynch', value: 'peter_lynch', description: 'Growth at reasonable price, invest in what you know' },
          { name: 'Philip Fisher', value: 'philip_fisher', description: 'Growth investing, scuttlebutt method' },
          { name: 'George Soros', value: 'george_soros', description: 'Reflexivity, macro trading' },
          { name: 'Ray Dalio', value: 'ray_dalio', description: 'All-weather portfolio, principles' },
          { name: 'Howard Marks', value: 'howard_marks', description: 'Second-level thinking, market cycles' },
          { name: 'Seth Klarman', value: 'seth_klarman', description: 'Deep value, distressed investing' },
          { name: 'Joel Greenblatt', value: 'joel_greenblatt', description: 'Magic formula, special situations' },
          { name: 'John Templeton', value: 'john_templeton', description: 'Global contrarian, buy at maximum pessimism' },
          { name: 'Carl Icahn', value: 'carl_icahn', description: 'Activist investing, corporate governance' },
        ],
        description: 'Famous investor perspective to use',
      },
      {
        displayName: 'Analysis Type',
        name: 'analysisType',
        type: NodePropertyType.Options,
        default: 'stockAnalysis',
        options: [
          { name: 'Stock Analysis', value: 'stockAnalysis', description: 'Analyze a specific stock' },
          { name: 'Portfolio Review', value: 'portfolioReview', description: 'Review a portfolio' },
          { name: 'Market Outlook', value: 'marketOutlook', description: 'General market outlook' },
          { name: 'Investment Thesis', value: 'thesis', description: 'Build investment thesis' },
          { name: 'Risk Assessment', value: 'riskAssessment', description: 'Assess investment risks' },
          { name: 'Valuation', value: 'valuation', description: 'Company valuation analysis' },
          { name: 'Sector Analysis', value: 'sectorAnalysis', description: 'Analyze an industry/sector' },
        ],
        description: 'Type of analysis to perform',
      },
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'AAPL',
        description: 'Stock symbol to analyze',
        displayOptions: {
          show: { analysisType: ['stockAnalysis', 'thesis', 'riskAssessment', 'valuation'] },
        },
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data',
        displayOptions: {
          show: { analysisType: ['stockAnalysis', 'thesis', 'riskAssessment', 'valuation'] },
        },
      },
      {
        displayName: 'Sector',
        name: 'sector',
        type: NodePropertyType.Options,
        default: 'technology',
        options: [
          { name: 'Technology', value: 'technology' },
          { name: 'Healthcare', value: 'healthcare' },
          { name: 'Financial Services', value: 'financials' },
          { name: 'Consumer Discretionary', value: 'consumer_discretionary' },
          { name: 'Consumer Staples', value: 'consumer_staples' },
          { name: 'Industrials', value: 'industrials' },
          { name: 'Energy', value: 'energy' },
          { name: 'Materials', value: 'materials' },
          { name: 'Real Estate', value: 'real_estate' },
          { name: 'Utilities', value: 'utilities' },
          { name: 'Communication Services', value: 'communication' },
        ],
        description: 'Sector to analyze',
        displayOptions: {
          show: { analysisType: ['sectorAnalysis'] },
        },
      },
      {
        displayName: 'Portfolio Data',
        name: 'portfolioData',
        type: NodePropertyType.Json,
        default: '[]',
        description: 'Portfolio holdings as JSON array',
        displayOptions: {
          show: { analysisType: ['portfolioReview'] },
        },
      },
      {
        displayName: 'Use Input Portfolio',
        name: 'useInputPortfolio',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use portfolio data from input',
        displayOptions: {
          show: { analysisType: ['portfolioReview'] },
        },
      },
      {
        displayName: 'Additional Context',
        name: 'context',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'Any specific concerns or focus areas',
        description: 'Additional context or questions',
        typeOptions: {
          rows: 3,
        },
      },
      {
        displayName: 'Include Financials',
        name: 'includeFinancials',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include financial data in analysis',
        displayOptions: {
          show: { analysisType: ['stockAnalysis', 'valuation'] },
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

  private readonly investorStyles: Record<string, { principles: string[]; focusAreas: string[] }> = {
    warren_buffett: {
      principles: ['Economic moats', 'Long-term holding', 'Circle of competence', 'Margin of safety', 'Quality management'],
      focusAreas: ['Return on equity', 'Debt levels', 'Consistent earnings', 'Brand value', 'Pricing power'],
    },
    benjamin_graham: {
      principles: ['Margin of safety', 'Mr. Market allegory', 'Intrinsic value', 'Defensive investing'],
      focusAreas: ['P/E ratio', 'P/B ratio', 'Dividend history', 'Current ratio', 'Net current assets'],
    },
    charlie_munger: {
      principles: ['Mental models', 'Invert always invert', 'Circle of competence', 'Patience'],
      focusAreas: ['Business quality', 'Management integrity', 'Competitive advantage', 'Learning ability'],
    },
    peter_lynch: {
      principles: ['Invest in what you know', 'PEG ratio', 'Ten-baggers', 'Niche companies'],
      focusAreas: ['Growth rate', 'Industry position', 'Institutional ownership', 'Insider buying'],
    },
    philip_fisher: {
      principles: ['Scuttlebutt method', 'Long-term growth', 'Quality of management', 'R&D investment'],
      focusAreas: ['Sales growth', 'Profit margins', 'Research capabilities', 'Employee relations'],
    },
    george_soros: {
      principles: ['Reflexivity', 'Boom-bust cycles', 'Fallibility', 'Global macro'],
      focusAreas: ['Market sentiment', 'Policy changes', 'Currency movements', 'Leverage levels'],
    },
    ray_dalio: {
      principles: ['Radical transparency', 'Diversification', 'Risk parity', 'Debt cycles'],
      focusAreas: ['Asset allocation', 'Correlation', 'Risk-adjusted returns', 'Economic indicators'],
    },
    howard_marks: {
      principles: ['Second-level thinking', 'Market cycles', 'Risk control', 'Contrarian investing'],
      focusAreas: ['Market psychology', 'Credit cycles', 'Valuation spreads', 'Investor behavior'],
    },
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const investor = this.getNodeParameter('investor', 0) as string;
    const analysisType = this.getNodeParameter('analysisType', 0) as string;
    const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
    const model = this.getNodeParameter('model', 0) as string;
    const context = this.getNodeParameter('context', 0) as string;

    // Get input data based on analysis type
    let analysisTarget: string = '';
    let additionalData: Record<string, unknown> = {};

    const inputData = this.getInputData();

    switch (analysisType) {
      case 'stockAnalysis':
      case 'thesis':
      case 'riskAssessment':
      case 'valuation': {
        const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
        if (useInputSymbol) {
          analysisTarget = inputData[0]?.json?.symbol as string || '';
        } else {
          analysisTarget = this.getNodeParameter('symbol', 0) as string;
        }

        if (this.getNodeParameter('includeFinancials', 0)) {
          additionalData.includeFinancials = true;
          // Would fetch real financial data here
          additionalData.financials = inputData[0]?.json?.financials || {};
        }
        break;
      }

      case 'portfolioReview': {
        const useInputPortfolio = this.getNodeParameter('useInputPortfolio', 0) as boolean;
        if (useInputPortfolio) {
          additionalData.portfolio = inputData[0]?.json?.portfolio || inputData[0]?.json?.holdings || [];
        } else {
          const portfolioStr = this.getNodeParameter('portfolioData', 0) as string;
          try {
            additionalData.portfolio = JSON.parse(portfolioStr);
          } catch {
            additionalData.portfolio = [];
          }
        }
        analysisTarget = 'portfolio';
        break;
      }

      case 'sectorAnalysis': {
        analysisTarget = this.getNodeParameter('sector', 0) as string;
        break;
      }

      case 'marketOutlook': {
        analysisTarget = 'market';
        additionalData.marketData = inputData[0]?.json?.marketData || {};
        break;
      }
    }

    if (!analysisTarget && analysisType !== 'marketOutlook') {
      return [[{
        json: {
          success: false,
          error: 'No analysis target provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    // Get investor style
    const investorStyle = this.investorStyles[investor] || { principles: [], focusAreas: [] };

    const bridge = new AgentBridge();

    try {
      const result = await bridge.executeAgent(`investor_${investor}`, {
        query: this.buildQuery(analysisType, analysisTarget, context),
        context: {
          investor,
          investorStyle,
          analysisType,
          analysisTarget,
          ...additionalData,
        },
        llmProvider,
        model,
      });

      return [[{
        json: {
          success: true,
          investor,
          investorStyle,
          analysisType,
          analysisTarget,
          analysis: result.response,
          keyInsights: result.keyInsights || [],
          recommendation: result.recommendation,
          riskFactors: result.riskFactors || [],
          confidence: result.confidence,
          principles: investorStyle.principles,
          timestamp: new Date().toISOString(),
          executionId: this.getExecutionId(),
        },
      }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          investor,
          analysisType,
          analysisTarget,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }

  private buildQuery(analysisType: string, target: string, context: string): string {
    const queries: Record<string, string> = {
      stockAnalysis: `Analyze ${target} as an investment opportunity. ${context}`,
      portfolioReview: `Review this portfolio and provide recommendations. ${context}`,
      marketOutlook: `What is your outlook on the current market conditions? ${context}`,
      thesis: `Build an investment thesis for ${target}. ${context}`,
      riskAssessment: `Assess the investment risks for ${target}. ${context}`,
      valuation: `Provide a valuation analysis for ${target}. ${context}`,
      sectorAnalysis: `Analyze the ${target} sector for investment opportunities. ${context}`,
    };

    return queries[analysisType] || `Analyze ${target}. ${context}`;
  }
}
