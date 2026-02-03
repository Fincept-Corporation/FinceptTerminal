/**
 * RD-Agent Service - Frontend API for Microsoft RD-Agent integration
 * Handles autonomous factor mining, model optimization, and research automation
 */

import { invoke } from '@tauri-apps/api/core';

export interface RDAgentCapabilities {
  factor_mining: {
    description: string;
    features: string[];
    estimated_cost: string;
    estimated_time: string;
  };
  model_optimization: {
    description: string;
    features: string[];
    estimated_cost: string;
    estimated_time: string;
  };
  document_analysis: {
    description: string;
    features: string[];
    estimated_cost: string;
    estimated_time: string;
  };
  research_agent: {
    description: string;
    llm_models: string[];
  };
  development_agent: {
    description: string;
    llm_models: string[];
  };
}

export interface RDAgentStatusResponse {
  success: boolean;
  rdagent_available: boolean;
  error?: string;
}

export interface RDAgentCapabilitiesResponse {
  success: boolean;
  capabilities?: RDAgentCapabilities;
  rdagent_available?: boolean;
  error?: string;
}

export interface FactorMiningRequest {
  task_description: string;
  api_keys: Record<string, string>;
  target_market?: string;
  budget?: number;
}

export interface FactorMiningResponse {
  success: boolean;
  task_id?: string;
  status?: string;
  message?: string;
  estimated_time?: string;
  estimated_cost?: string;
  error?: string;
}

export interface FactorMiningStatus {
  success: boolean;
  task_id?: string;
  status?: string;
  progress?: number;
  factors_generated?: number;
  factors_tested?: number;
  best_ic?: number;
  elapsed_time?: string;
  estimated_remaining?: string;
  error?: string;
}

export interface DiscoveredFactor {
  factor_id: string;
  name: string;
  description: string;
  ic: number;
  ic_std: number;
  sharpe: number;
  code: string;
  performance_metrics: {
    annual_return: number;
    max_drawdown: number;
    win_rate: number;
  };
}

export interface DiscoveredFactorsResponse {
  success: boolean;
  task_id?: string;
  factors?: DiscoveredFactor[];
  total_factors?: number;
  best_ic?: number;
  avg_ic?: number;
  error?: string;
}

export interface ModelOptimizationRequest {
  model_type: string;
  base_config: any;
  optimization_target?: string;
}

export interface ModelOptimizationResponse {
  success: boolean;
  task_id?: string;
  status?: string;
  message?: string;
  optimization_target?: string;
  estimated_time?: string;
  error?: string;
}

export interface DocumentAnalysisRequest {
  document_type: string;
  document_path: string;
}

export interface DocumentAnalysisResponse {
  success: boolean;
  task_id?: string;
  status?: string;
  document_type?: string;
  message?: string;
  error?: string;
}

export interface AutonomousResearchRequest {
  research_goal: string;
  api_keys: Record<string, string>;
  iterations?: number;
}

export interface AutonomousResearchResponse {
  success: boolean;
  task_id?: string;
  status?: string;
  research_goal?: string;
  iterations?: number;
  message?: string;
  estimated_time?: string;
  error?: string;
}

class RDAgentService {
  /**
   * Check RD-Agent installation status and availability
   */
  async checkStatus(): Promise<RDAgentStatusResponse> {
    console.log('[RDAgentService] checkStatus() called');
    try {
      console.log('[RDAgentService] Invoking rdagent_check_status command...');
      const result = await invoke<string>('rdagent_check_status');
      console.log('[RDAgentService] Raw result:', result);
      const parsed = JSON.parse(result);
      console.log('[RDAgentService] Parsed result:', parsed);
      return parsed;
    } catch (error) {
      console.error('[RDAgentService] checkStatus error:', error);
      console.error('[RDAgentService] Full error object:', JSON.stringify(error, null, 2));
      return {
        success: false,
        rdagent_available: false,
        error: String(error)
      };
    }
  }

  /**
   * Get RD-Agent capabilities
   */
  async getCapabilities(): Promise<RDAgentCapabilitiesResponse> {
    try {
      const result = await invoke<string>('rdagent_get_capabilities');
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Start autonomous factor mining
   */
  async startFactorMining(
    request: FactorMiningRequest
  ): Promise<FactorMiningResponse> {
    try {
      const result = await invoke<string>('rdagent_start_factor_mining', {
        taskDescription: request.task_description,
        apiKeys: JSON.stringify(request.api_keys),
        targetMarket: request.target_market,
        budget: request.budget
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Get factor mining task status
   */
  async getFactorMiningStatus(taskId: string): Promise<FactorMiningStatus> {
    try {
      const result = await invoke<string>('rdagent_get_factor_mining_status', {
        taskId
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Get discovered factors from a task
   */
  async getDiscoveredFactors(
    taskId: string
  ): Promise<DiscoveredFactorsResponse> {
    try {
      const result = await invoke<string>('rdagent_get_discovered_factors', {
        taskId
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Optimize model using RD-Agent
   */
  async optimizeModel(
    request: ModelOptimizationRequest
  ): Promise<ModelOptimizationResponse> {
    try {
      const result = await invoke<string>('rdagent_optimize_model', {
        modelType: request.model_type,
        baseConfig: JSON.stringify(request.base_config),
        optimizationTarget: request.optimization_target
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Analyze financial document
   */
  async analyzeDocument(
    request: DocumentAnalysisRequest
  ): Promise<DocumentAnalysisResponse> {
    try {
      const result = await invoke<string>('rdagent_analyze_document', {
        documentType: request.document_type,
        documentPath: request.document_path
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }

  /**
   * Run autonomous research
   */
  async runAutonomousResearch(
    request: AutonomousResearchRequest
  ): Promise<AutonomousResearchResponse> {
    try {
      const result = await invoke<string>('rdagent_run_autonomous_research', {
        researchGoal: request.research_goal,
        apiKeys: JSON.stringify(request.api_keys),
        iterations: request.iterations
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error as string
      };
    }
  }
}

// Export singleton instance
export const rdAgentService = new RDAgentService();
