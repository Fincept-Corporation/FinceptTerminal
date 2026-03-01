/**
 * AdvancedModelsPanel - Types
 */

export interface ModelInfo {
  id: string;
  name: string;
  category: string;
  description: string;
  features: string[];
  use_cases: string[];
  available: boolean;
}

export interface ModelConfig {
  input_size?: number;
  hidden_size?: number;
  num_layers?: number;
  dropout?: number;
  nhead?: number;
}

export interface CreatedModel {
  model_id: string;
  type: string;
  created_at: string;
  trained: boolean;
  epochs_trained: number;
}

export interface TrainingResult {
  success: boolean;
  model_id?: string;
  epochs_trained?: number;
  total_epochs?: number;
  final_loss?: number;
  loss_history?: number[];
  error?: string;
}
