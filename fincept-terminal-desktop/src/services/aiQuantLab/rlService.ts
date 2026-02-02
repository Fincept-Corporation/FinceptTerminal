/**
 * RL Service - Reinforcement Learning API for trading agents
 * Handles PPO, DQN, A2C, SAC, TD3 agent training and evaluation
 */

import { invoke } from '@tauri-apps/api/core';

export interface RLAlgorithm {
  id: string;
  name: string;
  description: string;
  action_space: 'continuous' | 'discrete' | 'both';
}

export interface CreateEnvironmentRequest {
  tickers: string[];
  start_date: string;
  end_date: string;
  initial_cash: number;
  commission: number;
  action_space_type: 'continuous' | 'discrete';
}

export interface CreateEnvironmentResponse {
  success: boolean;
  message?: string;
  data_points?: number;
  action_space?: string;
  initial_cash?: number;
  error?: string;
}

export interface TrainAgentRequest {
  algorithm: string;
  total_timesteps: number;
  learning_rate: number;
  [key: string]: any;
}

export interface TrainAgentResponse {
  success: boolean;
  algorithm?: string;
  timesteps?: number;
  message?: string;
  error?: string;
}

export interface EvaluateAgentRequest {
  n_episodes: number;
}

export interface EvaluateAgentResponse {
  success: boolean;
  n_episodes?: number;
  mean_reward?: number;
  std_reward?: number;
  mean_length?: number;
  mean_portfolio_value?: number;
  portfolio_return?: number;
  all_rewards?: number[];
  error?: string;
}

export interface SaveModelRequest {
  path: string;
}

export interface SaveModelResponse {
  success: boolean;
  path?: string;
  error?: string;
}

export interface LoadModelRequest {
  path: string;
  algorithm: string;
}

export interface LoadModelResponse {
  success: boolean;
  path?: string;
  algorithm?: string;
  error?: string;
}

export interface AlgorithmsResponse {
  success: boolean;
  algorithms?: { [key: string]: string };
  stable_baselines_available?: boolean;
  qlib_rl_available?: boolean;
  error?: string;
}

class RLService {
  /**
   * Get list of available RL algorithms
   */
  async getAvailableAlgorithms(): Promise<AlgorithmsResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['list_algorithms']
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get algorithms'
      };
    }
  }

  /**
   * Initialize RL environment
   */
  async initialize(providerUri: string = '~/.qlib/qlib_data/cn_data', region: string = 'cn'): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['initialize', providerUri, region]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to initialize RL'
      };
    }
  }

  /**
   * Create trading environment
   */
  async createEnvironment(request: CreateEnvironmentRequest): Promise<CreateEnvironmentResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['create_env', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to create environment'
      };
    }
  }

  /**
   * Train RL agent
   */
  async trainAgent(request: TrainAgentRequest): Promise<TrainAgentResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['train', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to train agent'
      };
    }
  }

  /**
   * Evaluate trained RL agent
   */
  async evaluateAgent(nEpisodes: number = 10): Promise<EvaluateAgentResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['evaluate', nEpisodes.toString()]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to evaluate agent'
      };
    }
  }

  /**
   * Save trained model
   */
  async saveModel(path: string): Promise<SaveModelResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['save_model', path]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to save model'
      };
    }
  }

  /**
   * Load trained model
   */
  async loadModel(path: string, algorithm: string): Promise<LoadModelResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rl',
        args: ['load_model', path, algorithm]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to load model'
      };
    }
  }
}

export const rlService = new RLService();
