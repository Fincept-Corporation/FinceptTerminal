/**
 * Rolling Retraining Service - Automated model retraining
 */

import { invoke } from '@tauri-apps/api/core';

export interface CreateScheduleRequest {
  model_id: string;
  frequency?: 'hourly' | 'daily' | 'weekly';
  window?: number;
}

export interface ExecuteRetrainRequest {
  model_id: string;
}

class RollingRetrainingService {
  async createSchedule(request: CreateScheduleRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rolling_retraining',
        args: ['create', request.model_id]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to create schedule' };
    }
  }

  async executeRetrain(request: ExecuteRetrainRequest): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rolling_retraining',
        args: ['retrain', request.model_id]
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to retrain' };
    }
  }

  async getSchedules(): Promise<any> {
    try {
      const result = await invoke<string>('execute_python_command', {
        command: 'qlib_rolling_retraining',
        args: ['list']
      });
      return JSON.parse(result);
    } catch (error) {
      return { success: false, error: error instanceof Error ? error.message : 'Failed to get schedules' };
    }
  }
}

export const rollingRetrainingService = new RollingRetrainingService();
