/**
 * Meta Learning Service - Model selection, ensemble, AutoML
 */

import { invoke } from '@tauri-apps/api/core';

export interface ModelSelectionRequest {
  models: string[];
  data?: any;
  metric?: string;
}

export interface CreateEnsembleRequest {
  models: string[];
  weights?: number[];
}

export interface AutoTuneRequest {
  model_type: string;
  param_grid: { [key: string]: any[] };
}

class MetaLearningService {
  async modelSelection(request: ModelSelectionRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_meta_learning',
        args: ['select', JSON.stringify(request.models)]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to select models' };
    }
  }

  async createEnsemble(request: CreateEnsembleRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_meta_learning',
        args: ['ensemble', JSON.stringify(request.models)]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to create ensemble' };
    }
  }

  async autoTune(request: AutoTuneRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_meta_learning',
        args: ['tune', JSON.stringify(request.param_grid)]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to tune' };
    }
  }
}

export const metaLearningService = new MetaLearningService();
