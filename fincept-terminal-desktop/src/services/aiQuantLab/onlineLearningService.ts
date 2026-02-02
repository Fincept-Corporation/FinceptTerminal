/**
 * Online Learning Service - Real-time model updates & drift detection
 * Handles incremental learning and concept drift
 */

import { invoke } from '@tauri-apps/api/core';

export interface CreateModelRequest {
  model_type: 'linear' | 'tree' | 'adaptive_random_forest' | 'logistic';
  model_id?: string;
  [key: string]: any;
}

export interface CreateModelResponse {
  success: boolean;
  model_id?: string;
  model_type?: string;
  message?: string;
  error?: string;
}

export interface IncrementalTrainRequest {
  model_id: string;
  features: { [key: string]: number } | any;
  target: number | any;
}

export interface IncrementalTrainResponse {
  success: boolean;
  model_id?: string;
  prediction?: number;
  actual?: number;
  prediction_error?: number;
  samples_trained?: number;
  total_samples?: number;
  current_mae?: number;
  drift_detected?: boolean;
  error?: string;
}

export interface PredictOnlineRequest {
  model_id: string;
  features: { [key: string]: number } | any;
}

export interface PredictOnlineResponse {
  success: boolean;
  model_id?: string;
  prediction?: number;
  predictions?: number[];
  count?: number;
  error?: string;
}

export interface ModelPerformanceResponse {
  success: boolean;
  model_id?: string;
  model_type?: string;
  samples_trained?: number;
  current_mae?: number;
  last_updated?: string;
  created_at?: string;
  drift_detected?: boolean;
  error?: string;
}

export interface RollingUpdateRequest {
  model_id: string;
  update_frequency: 'hourly' | 'daily' | 'weekly';
  retrain_window?: number;
  min_samples?: number;
}

export interface RollingUpdateResponse {
  success: boolean;
  model_id?: string;
  update_schedule?: any;
  message?: string;
  error?: string;
}

export interface HandleDriftRequest {
  model_id: string;
  strategy: 'retrain' | 'ensemble' | 'adaptive';
}

export interface HandleDriftResponse {
  success: boolean;
  model_id?: string;
  strategy?: string;
  action?: string;
  timestamp?: string;
  error?: string;
}

export interface AllModelsResponse {
  success: boolean;
  models?: any[];
  count?: number;
  river_available?: boolean;
  qlib_online_available?: boolean;
  error?: string;
}

class OnlineLearningService {
  /**
   * Initialize online learning system
   */
  async initialize(providerUri: string = '~/.qlib/qlib_data/cn_data', region: string = 'cn'): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['initialize', providerUri, region]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to initialize online learning'
      };
    }
  }

  /**
   * Create online learning model
   */
  async createModel(request: CreateModelRequest): Promise<CreateModelResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['create_model', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to create model'
      };
    }
  }

  /**
   * Incrementally train model with new data
   */
  async incrementalTrain(request: IncrementalTrainRequest): Promise<IncrementalTrainResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['train', request.model_id, JSON.stringify(request.features), request.target.toString()]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to train incrementally'
      };
    }
  }

  /**
   * Make online prediction
   */
  async predictOnline(request: PredictOnlineRequest): Promise<PredictOnlineResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['predict', request.model_id, JSON.stringify(request.features)]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to predict'
      };
    }
  }

  /**
   * Get model performance metrics
   */
  async getModelPerformance(modelId: string): Promise<ModelPerformanceResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['performance', modelId]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get performance'
      };
    }
  }

  /**
   * Setup rolling update schedule
   */
  async setupRollingUpdate(request: RollingUpdateRequest): Promise<RollingUpdateResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['setup_rolling', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to setup rolling update'
      };
    }
  }

  /**
   * Handle concept drift
   */
  async handleConceptDrift(request: HandleDriftRequest): Promise<HandleDriftResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['handle_drift', JSON.stringify(request)]
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to handle drift'
      };
    }
  }

  /**
   * Get all online models
   */
  async getAllModels(): Promise<AllModelsResponse> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_online_learning',
        args: ['list_models']
      });
      return JSON.parse(result);
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get models'
      };
    }
  }
}

export const onlineLearningService = new OnlineLearningService();
