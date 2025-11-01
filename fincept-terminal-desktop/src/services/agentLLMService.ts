// Agent LLM Service
// Facilitates LLM-powered communication between agents in node workflows

import { llmApiService, ChatMessage } from './llmApi';
import { sqliteService, LLMConfig } from './sqliteService';

export interface AgentMediationConfig {
  llmProvider?: string; // Specific provider to use (optional, uses active if not set)
  mediationPrompt?: string; // Custom prompt for mediation
}

export interface AgentMediationResult {
  success: boolean;
  mediatedContext: string;
  error?: string;
  llmProvider?: string;
  llmModel?: string;
}

class AgentLLMService {
  /**
   * Mediate between two agents using an LLM
   * Takes output from one agent and reformats it as context for the next agent
   */
  async mediateAgentOutput(
    previousAgentOutput: any,
    config: AgentMediationConfig = {}
  ): Promise<AgentMediationResult> {
    try {
      console.log('[AgentLLMService] Mediating agent output:', previousAgentOutput);

      // Extract meaningful content from previous agent
      const agentData = this.extractAgentData(previousAgentOutput);

      if (!agentData) {
        return {
          success: false,
          mediatedContext: '',
          error: 'No valid agent output to mediate'
        };
      }

      // Get LLM config (either specified provider or active one)
      let llmConfig: LLMConfig | null = null;

      if (config.llmProvider) {
        // Get specific provider config
        const configs = await sqliteService.getLLMConfigs();
        llmConfig = configs.find(c => c.provider === config.llmProvider) || null;
      } else {
        // Use active provider
        llmConfig = await sqliteService.getActiveLLMConfig();
      }

      if (!llmConfig) {
        return {
          success: false,
          mediatedContext: '',
          error: 'No LLM provider configured. Please configure LLM settings.'
        };
      }

      // Build mediation prompt
      const mediationPrompt = config.mediationPrompt || this.getDefaultMediationPrompt();

      const userMessage = `${mediationPrompt}\n\nPrevious Agent Analysis:\n${JSON.stringify(agentData, null, 2)}`;

      // Call LLM to mediate
      const response = await llmApiService.chat(userMessage, [], undefined);

      if (response.error) {
        return {
          success: false,
          mediatedContext: '',
          error: response.error
        };
      }

      console.log('[AgentLLMService] Mediation successful:', response.content);

      return {
        success: true,
        mediatedContext: response.content,
        llmProvider: llmConfig.provider,
        llmModel: llmConfig.model
      };

    } catch (error) {
      console.error('[AgentLLMService] Mediation error:', error);
      return {
        success: false,
        mediatedContext: '',
        error: error instanceof Error ? error.message : 'Unknown mediation error'
      };
    }
  }

  /**
   * Extract meaningful data from agent output
   */
  private extractAgentData(output: any): any {
    if (!output) return null;

    // Handle wrapped success response
    if (output.success !== undefined && output.data !== undefined) {
      return this.extractAgentData(output.data);
    }

    // Handle analysis object
    if (output.analysis) {
      return output.analysis;
    }

    // Handle direct agent output with ticker analyses
    if (typeof output === 'object') {
      // Check if it's a map of ticker -> analysis
      const firstKey = Object.keys(output)[0];
      if (firstKey && output[firstKey]?.signal && output[firstKey]?.reasoning) {
        return output;
      }
    }

    return output;
  }

  /**
   * Get default mediation prompt
   */
  private getDefaultMediationPrompt(): string {
    return `You are mediating between financial analysis agents.

Your task is to:
1. Summarize the key findings from the previous agent's analysis
2. Extract the most important signals, metrics, and reasoning
3. Format this information as clear, actionable context for the next agent to consider
4. Maintain objectivity - don't add your own investment opinions, just synthesize the data

Be concise but comprehensive. Focus on facts, signals, and metrics that would be valuable for subsequent analysis.`;
  }

  /**
   * Facilitate a multi-agent discussion
   * Takes multiple agent outputs and synthesizes them into a coherent summary
   */
  async synthesizeMultipleAgents(
    agentOutputs: Array<{ agentName: string; output: any }>,
    synthesisPrompt?: string
  ): Promise<AgentMediationResult> {
    try {
      console.log('[AgentLLMService] Synthesizing multiple agent outputs:', agentOutputs.length);

      if (agentOutputs.length === 0) {
        return {
          success: false,
          mediatedContext: '',
          error: 'No agent outputs to synthesize'
        };
      }

      // Extract data from all agents
      const synthesisData = agentOutputs.map(({ agentName, output }) => ({
        agent: agentName,
        analysis: this.extractAgentData(output)
      }));

      // Get active LLM config
      const llmConfig = await sqliteService.getActiveLLMConfig();
      if (!llmConfig) {
        return {
          success: false,
          mediatedContext: '',
          error: 'No LLM provider configured'
        };
      }

      // Build synthesis prompt
      const defaultPrompt = `You are synthesizing insights from multiple financial analysis agents.

Your task is to:
1. Compare and contrast the findings from all agents
2. Identify areas of consensus and disagreement
3. Highlight the most significant insights from each perspective
4. Provide a balanced, comprehensive summary that incorporates all viewpoints

Be objective and data-driven. Present a holistic view that respects each agent's methodology.`;

      const userMessage = `${synthesisPrompt || defaultPrompt}\n\nAgent Analyses:\n${JSON.stringify(synthesisData, null, 2)}`;

      // Call LLM to synthesize
      const response = await llmApiService.chat(userMessage, [], undefined);

      if (response.error) {
        return {
          success: false,
          mediatedContext: '',
          error: response.error
        };
      }

      return {
        success: true,
        mediatedContext: response.content,
        llmProvider: llmConfig.provider,
        llmModel: llmConfig.model
      };

    } catch (error) {
      console.error('[AgentLLMService] Synthesis error:', error);
      return {
        success: false,
        mediatedContext: '',
        error: error instanceof Error ? error.message : 'Unknown synthesis error'
      };
    }
  }
}

export const agentLLMService = new AgentLLMService();
