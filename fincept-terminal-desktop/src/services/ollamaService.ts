// Ollama Service - Interact with local Ollama API
// Provides model listing and connection testing

import { fetch } from '@tauri-apps/plugin-http';

export interface OllamaModel {
  name: string;
  model: string;
  modified_at: string;
  size: number;
  digest: string;
  details: {
    parent_model: string;
    format: string;
    family: string;
    families: string[];
    parameter_size: string;
    quantization_level: string;
  };
}

export interface OllamaListResponse {
  models: OllamaModel[];
}

export interface OllamaVersionResponse {
  version: string;
}

class OllamaService {
  private defaultBaseUrl = 'http://localhost:11434';

  /**
   * Get list of locally available Ollama models
   */
  async listModels(baseUrl?: string): Promise<{ success: boolean; models?: string[]; error?: string }> {
    const url = baseUrl || this.defaultBaseUrl;

    try {
      const response = await fetch(`${url}/api/tags`, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
        },
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data: OllamaListResponse = await response.json();

      // Extract model names from the response
      const modelNames = data.models.map(m => m.name);

      return {
        success: true,
        models: modelNames,
      };
    } catch (error) {
      console.error('Failed to fetch Ollama models:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to connect to Ollama',
      };
    }
  }

  /**
   * Get detailed information about available models
   */
  async listModelsDetailed(baseUrl?: string): Promise<{ success: boolean; models?: OllamaModel[]; error?: string }> {
    const url = baseUrl || this.defaultBaseUrl;

    try {
      const response = await fetch(`${url}/api/tags`, {
        method: 'GET',
        headers: {
          'Content-Type': 'application/json',
        },
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data: OllamaListResponse = await response.json();

      return {
        success: true,
        models: data.models,
      };
    } catch (error) {
      console.error('Failed to fetch Ollama models:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to connect to Ollama',
      };
    }
  }

  /**
   * Check if Ollama is running and accessible
   */
  async checkConnection(baseUrl?: string): Promise<{ success: boolean; version?: string; error?: string }> {
    const url = baseUrl || this.defaultBaseUrl;

    try {
      const response = await fetch(`${url}/api/version`, {
        method: 'GET',
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data: OllamaVersionResponse = await response.json();

      return {
        success: true,
        version: data.version,
      };
    } catch (error) {
      console.error('Failed to connect to Ollama:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to connect to Ollama',
      };
    }
  }

  /**
   * Format model size for display
   */
  formatModelSize(bytes: number): string {
    const gb = bytes / (1024 * 1024 * 1024);
    if (gb >= 1) {
      return `${gb.toFixed(2)} GB`;
    }
    const mb = bytes / (1024 * 1024);
    return `${mb.toFixed(2)} MB`;
  }

  /**
   * Get model display name with details
   */
  formatModelDisplay(model: OllamaModel): string {
    const size = this.formatModelSize(model.size);
    const params = model.details.parameter_size;
    return `${model.name} (${params}, ${size})`;
  }
}

// Singleton instance
export const ollamaService = new OllamaService();
