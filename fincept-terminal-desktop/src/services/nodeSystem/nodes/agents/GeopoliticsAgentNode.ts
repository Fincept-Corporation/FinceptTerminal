/**
 * Geopolitics Agent Node
 * Execute geopolitical analysis agents (Grand Chessboard, World Order, etc.)
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

export class GeopoliticsAgentNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Geopolitics AI',
    name: 'geopoliticsAgent',
    icon: 'file:geopolitics.svg',
    group: ['agents'],
    version: 1,
    subtitle: '={{$parameter["framework"]}}',
    description: 'Get geopolitical analysis using various frameworks and perspectives',
    defaults: {
      name: 'Geopolitics AI',
      color: '#dc2626',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Framework',
        name: 'framework',
        type: NodePropertyType.Options,
        default: 'grand_chessboard',
        options: [
          // Grand Chessboard Framework
          { name: 'Grand Chessboard', value: 'grand_chessboard', description: 'Brzezinski\'s geostrategy' },
          { name: 'Eurasian Balkans', value: 'eurasian_balkans', description: 'Central Asia power vacuum' },
          { name: 'Democratic Bridgehead', value: 'democratic_bridgehead', description: 'Europe as strategic anchor' },
          { name: 'Far Eastern Anchor', value: 'far_eastern_anchor', description: 'Japan and Pacific strategy' },
          { name: 'Black Hole', value: 'black_hole', description: 'Post-Soviet space analysis' },

          // Prisoners of Geography
          { name: 'Prisoners of Geography', value: 'prisoners_geography', description: 'Marshall\'s geographic analysis' },
          { name: 'Russia Geography', value: 'russia_geography', description: 'Russian geographic constraints' },
          { name: 'China Geography', value: 'china_geography', description: 'Chinese geographic factors' },
          { name: 'USA Geography', value: 'usa_geography', description: 'American geographic advantages' },
          { name: 'Europe Geography', value: 'europe_geography', description: 'European geographic divisions' },
          { name: 'Middle East Geography', value: 'middle_east_geography', description: 'Middle East geographic complexities' },
          { name: 'Africa Geography', value: 'africa_geography', description: 'African geographic challenges' },
          { name: 'India-Pakistan Geography', value: 'india_pakistan_geography', description: 'Subcontinent rivalries' },
          { name: 'Japan-Korea Geography', value: 'japan_korea_geography', description: 'East Asian dynamics' },
          { name: 'Latin America Geography', value: 'latin_america_geography', description: 'Latin American factors' },
          { name: 'Arctic Geography', value: 'arctic_geography', description: 'Arctic strategic importance' },

          // World Order Framework
          { name: 'World Order', value: 'world_order', description: 'Kissinger\'s order analysis' },
          { name: 'American Order', value: 'american_order', description: 'US-led international order' },
          { name: 'Chinese Order', value: 'chinese_order', description: 'Chinese vision of order' },
          { name: 'European Order', value: 'european_order', description: 'European concept of order' },
          { name: 'Islamic Order', value: 'islamic_order', description: 'Islamic world order concepts' },
          { name: 'Multipolar Order', value: 'multipolar_order', description: 'Emerging multipolar system' },

          // Other Frameworks
          { name: 'Rise and Fall of Powers', value: 'rise_fall_powers', description: 'Kennedy\'s power cycles' },
          { name: 'Clash of Civilizations', value: 'clash_civilizations', description: 'Huntington\'s thesis' },
          { name: 'Thucydides Trap', value: 'thucydides_trap', description: 'Rising power dynamics' },
        ],
        description: 'Geopolitical framework/perspective to use',
      },
      {
        displayName: 'Analysis Focus',
        name: 'analysisFocus',
        type: NodePropertyType.Options,
        default: 'general',
        options: [
          { name: 'General Analysis', value: 'general' },
          { name: 'Market Implications', value: 'market' },
          { name: 'Risk Assessment', value: 'risk' },
          { name: 'Scenario Planning', value: 'scenarios' },
          { name: 'Historical Parallels', value: 'historical' },
          { name: 'Policy Analysis', value: 'policy' },
        ],
        description: 'Focus area for the analysis',
      },
      {
        displayName: 'Topic/Region',
        name: 'topic',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Taiwan Strait tensions, BRICS expansion, etc.',
        description: 'Specific topic or region to analyze',
        typeOptions: {
          rows: 2,
        },
      },
      {
        displayName: 'Use Input Topic',
        name: 'useInputTopic',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use topic from input data',
      },
      {
        displayName: 'Time Horizon',
        name: 'timeHorizon',
        type: NodePropertyType.Options,
        default: 'medium',
        options: [
          { name: 'Immediate (Weeks)', value: 'immediate' },
          { name: 'Short-term (Months)', value: 'short' },
          { name: 'Medium-term (1-2 Years)', value: 'medium' },
          { name: 'Long-term (5+ Years)', value: 'long' },
          { name: 'Secular (Decades)', value: 'secular' },
        ],
        description: 'Time horizon for analysis',
      },
      {
        displayName: 'Include Market Impact',
        name: 'includeMarketImpact',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include analysis of market implications',
      },
      {
        displayName: 'Affected Markets',
        name: 'affectedMarkets',
        type: NodePropertyType.MultiOptions,
        default: ['equities', 'commodities'],
        options: [
          { name: 'Equities', value: 'equities' },
          { name: 'Fixed Income', value: 'bonds' },
          { name: 'Currencies', value: 'forex' },
          { name: 'Commodities', value: 'commodities' },
          { name: 'Energy', value: 'energy' },
          { name: 'Defense Sector', value: 'defense' },
          { name: 'Technology', value: 'tech' },
          { name: 'Emerging Markets', value: 'em' },
        ],
        description: 'Markets to analyze for impact',
        displayOptions: {
          show: { includeMarketImpact: [true] },
        },
      },
      {
        displayName: 'Scenario Count',
        name: 'scenarioCount',
        type: NodePropertyType.Number,
        default: 3,
        description: 'Number of scenarios to develop',
        displayOptions: {
          show: { analysisFocus: ['scenarios'] },
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

  private static readonly frameworkDescriptions: Record<string, string> = {
    grand_chessboard: 'Brzezinski\'s analysis of Eurasia as the center of world power and American grand strategy',
    prisoners_geography: 'How physical geography shapes nations\' destinies and limits their options',
    world_order: 'Kissinger\'s analysis of different concepts of international order throughout history',
    rise_fall_powers: 'Kennedy\'s study of how great powers rise and fall over centuries',
    clash_civilizations: 'Huntington\'s thesis that future conflicts will be along civilizational lines',
    thucydides_trap: 'The danger of conflict when a rising power threatens an established one',
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const framework = this.getNodeParameter('framework', 0) as string;
    const analysisFocus = this.getNodeParameter('analysisFocus', 0) as string;
    const useInputTopic = this.getNodeParameter('useInputTopic', 0) as boolean;
    const timeHorizon = this.getNodeParameter('timeHorizon', 0) as string;
    const includeMarketImpact = this.getNodeParameter('includeMarketImpact', 0) as boolean;
    const llmProvider = this.getNodeParameter('llmProvider', 0) as string;
    const model = this.getNodeParameter('model', 0) as string;

    // Get topic
    let topic: string;
    if (useInputTopic) {
      const inputData = this.getInputData();
      topic = inputData[0]?.json?.topic as string || inputData[0]?.json?.query as string || '';
    } else {
      topic = this.getNodeParameter('topic', 0) as string;
    }

    if (!topic) {
      return [[{
        json: {
          success: false,
          error: 'No topic provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    const context: Record<string, any> = {
      framework,
      frameworkDescription: GeopoliticsAgentNode.frameworkDescriptions[framework.split('_')[0]] || '',
      analysisFocus,
      timeHorizon,
      includeMarketImpact,
    };

    if (includeMarketImpact) {
      context.affectedMarkets = this.getNodeParameter('affectedMarkets', 0) as string[];
    }

    if (analysisFocus === 'scenarios') {
      context.scenarioCount = this.getNodeParameter('scenarioCount', 0) as number;
    }

    // Build query based on focus
    let query = `Using the ${framework.replace(/_/g, ' ')} framework, analyze: ${topic}.`;

    switch (analysisFocus) {
      case 'market':
        query += ' Focus on market and investment implications.';
        break;
      case 'risk':
        query += ' Provide a comprehensive risk assessment.';
        break;
      case 'scenarios':
        query += ` Develop ${context.scenarioCount} distinct scenarios with probabilities.`;
        break;
      case 'historical':
        query += ' Draw historical parallels and lessons.';
        break;
      case 'policy':
        query += ' Analyze policy implications and options.';
        break;
    }

    query += ` Time horizon: ${timeHorizon}.`;

    try {
      const result = await AgentBridge.executeAgent({
        agentId: `geopolitics_${framework}`,
        category: 'geopolitics',
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
        framework,
        frameworkDescription: context.frameworkDescription,
        topic,
        analysisFocus,
        timeHorizon,
        analysis: responseData.response || responseData.analysis || '',
        keyInsights: responseData.keyInsights || [],
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
      };

      // Add focus-specific outputs
      if (analysisFocus === 'risk' && responseData.risks) {
        output.risks = responseData.risks;
        output.riskLevel = responseData.riskLevel;
      }

      if (analysisFocus === 'scenarios' && responseData.scenarios) {
        output.scenarios = responseData.scenarios;
      }

      if (includeMarketImpact && responseData.marketImpact) {
        output.marketImpact = responseData.marketImpact;
        output.tradingIdeas = responseData.tradingIdeas || [];
        output.hedgingStrategies = responseData.hedgingStrategies || [];
      }

      if (responseData.historicalParallels) {
        output.historicalParallels = responseData.historicalParallels;
      }

      return [[{ json: output }]];
    } catch (error) {
      return [[{
        json: {
          success: false,
          framework,
          topic,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }
}
