// LLM API Integration Service
// Unified interface for different LLM providers with streaming support

import { sqliteService } from './sqliteService';

export interface LLMConfig {
  provider: string;
  apiKey?: string;
  baseUrl?: string;
  model: string;
  temperature: number;
  maxTokens: number;
  systemPrompt: string;
}

export interface ChatMessage {
  role: 'system' | 'user' | 'assistant';
  content: string;
}

export interface LLMResponse {
  content: string;
  error?: string;
  usage?: {
    promptTokens: number;
    completionTokens: number;
    totalTokens: number;
  };
}

export type StreamCallback = (chunk: string, done: boolean) => void;

class LLMApiService {
  // OpenAI API
  private async callOpenAI(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const url = `${config.baseUrl || 'https://api.openai.com/v1'}/chat/completions`;

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${config.apiKey}`
        },
        body: JSON.stringify({
          model: config.model,
          messages: messages,
          temperature: config.temperature,
          max_tokens: config.maxTokens,
          stream: !!onStream
        })
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error?.message || `OpenAI API error: ${response.status}`);
      }

      if (onStream) {
        return await this.handleOpenAIStream(response, onStream);
      } else {
        const data = await response.json();
        return {
          content: data.choices[0].message.content,
          usage: {
            promptTokens: data.usage?.prompt_tokens || 0,
            completionTokens: data.usage?.completion_tokens || 0,
            totalTokens: data.usage?.total_tokens || 0
          }
        };
      }
    } catch (error) {
      console.error('OpenAI API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  private async handleOpenAIStream(response: Response, onStream: StreamCallback): Promise<LLMResponse> {
    const reader = response.body?.getReader();
    const decoder = new TextDecoder();
    let fullContent = '';

    if (!reader) {
      throw new Error('No response body');
    }

    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        const chunk = decoder.decode(value);
        const lines = chunk.split('\n').filter(line => line.trim().startsWith('data: '));

        for (const line of lines) {
          const data = line.replace('data: ', '').trim();
          if (data === '[DONE]') continue;

          try {
            const parsed = JSON.parse(data);
            const content = parsed.choices[0]?.delta?.content || '';
            if (content) {
              fullContent += content;
              onStream(content, false);
            }
          } catch (e) {
            // Skip invalid JSON
          }
        }
      }

      onStream('', true);
      return { content: fullContent };
    } finally {
      reader.releaseLock();
    }
  }

  // Gemini API
  private async callGemini(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const url = `https://generativelanguage.googleapis.com/v1beta/models/${config.model}:${onStream ? 'streamGenerateContent' : 'generateContent'}?key=${config.apiKey}`;

    // Convert messages to Gemini format
    const systemPrompt = messages.find(m => m.role === 'system')?.content || '';
    const conversationMessages = messages.filter(m => m.role !== 'system');

    const contents = conversationMessages.map(msg => ({
      role: msg.role === 'assistant' ? 'model' : 'user',
      parts: [{ text: msg.content }]
    }));

    const requestBody = {
      contents,
      systemInstruction: systemPrompt ? { parts: [{ text: systemPrompt }] } : undefined,
      generationConfig: {
        temperature: config.temperature,
        maxOutputTokens: config.maxTokens
      }
    };

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(requestBody)
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error?.message || `Gemini API error: ${response.status}`);
      }

      if (onStream) {
        return await this.handleGeminiStream(response, onStream);
      } else {
        const data = await response.json();
        return {
          content: data.candidates[0].content.parts[0].text,
          usage: {
            promptTokens: data.usageMetadata?.promptTokenCount || 0,
            completionTokens: data.usageMetadata?.candidatesTokenCount || 0,
            totalTokens: data.usageMetadata?.totalTokenCount || 0
          }
        };
      }
    } catch (error) {
      console.error('Gemini API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  private async handleGeminiStream(response: Response, onStream: StreamCallback): Promise<LLMResponse> {
    const reader = response.body?.getReader();
    const decoder = new TextDecoder();
    let fullContent = '';

    if (!reader) {
      throw new Error('No response body');
    }

    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        const chunk = decoder.decode(value);
        const lines = chunk.split('\n').filter(line => line.trim());

        for (const line of lines) {
          try {
            const parsed = JSON.parse(line);
            const content = parsed.candidates?.[0]?.content?.parts?.[0]?.text || '';
            if (content) {
              fullContent += content;
              onStream(content, false);
            }
          } catch (e) {
            // Skip invalid JSON
          }
        }
      }

      onStream('', true);
      return { content: fullContent };
    } finally {
      reader.releaseLock();
    }
  }

  // DeepSeek API (OpenAI-compatible)
  private async callDeepSeek(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const deepseekConfig = {
      ...config,
      baseUrl: config.baseUrl || 'https://api.deepseek.com'
    };
    return this.callOpenAI(messages, deepseekConfig, onStream);
  }

  // Ollama API
  private async callOllama(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const url = `${config.baseUrl}/api/chat`;

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          model: config.model,
          messages: messages,
          stream: !!onStream,
          options: {
            temperature: config.temperature,
            num_predict: config.maxTokens
          }
        })
      });

      if (!response.ok) {
        throw new Error(`Ollama API error: ${response.status}`);
      }

      if (onStream) {
        return await this.handleOllamaStream(response, onStream);
      } else {
        const data = await response.json();
        return {
          content: data.message.content,
          usage: {
            promptTokens: data.prompt_eval_count || 0,
            completionTokens: data.eval_count || 0,
            totalTokens: (data.prompt_eval_count || 0) + (data.eval_count || 0)
          }
        };
      }
    } catch (error) {
      console.error('Ollama API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error. Is Ollama running?'
      };
    }
  }

  private async handleOllamaStream(response: Response, onStream: StreamCallback): Promise<LLMResponse> {
    const reader = response.body?.getReader();
    const decoder = new TextDecoder();
    let fullContent = '';

    if (!reader) {
      throw new Error('No response body');
    }

    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        const chunk = decoder.decode(value);
        const lines = chunk.split('\n').filter(line => line.trim());

        for (const line of lines) {
          try {
            const parsed = JSON.parse(line);
            const content = parsed.message?.content || '';
            if (content) {
              fullContent += content;
              onStream(content, false);
            }
            if (parsed.done) {
              break;
            }
          } catch (e) {
            // Skip invalid JSON
          }
        }
      }

      onStream('', true);
      return { content: fullContent };
    } finally {
      reader.releaseLock();
    }
  }

  // OpenRouter API (OpenAI-compatible)
  private async callOpenRouter(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const url = `${config.baseUrl || 'https://openrouter.ai/api/v1'}/chat/completions`;

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${config.apiKey}`,
          'HTTP-Referer': window.location.origin,
          'X-Title': 'Fincept Terminal'
        },
        body: JSON.stringify({
          model: config.model,
          messages: messages,
          temperature: config.temperature,
          max_tokens: config.maxTokens,
          stream: !!onStream
        })
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error?.message || `OpenRouter API error: ${response.status}`);
      }

      if (onStream) {
        return await this.handleOpenAIStream(response, onStream);
      } else {
        const data = await response.json();
        return {
          content: data.choices[0].message.content,
          usage: {
            promptTokens: data.usage?.prompt_tokens || 0,
            completionTokens: data.usage?.completion_tokens || 0,
            totalTokens: data.usage?.total_tokens || 0
          }
        };
      }
    } catch (error) {
      console.error('OpenRouter API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  // Main chat method
  async chat(
    userMessage: string,
    conversationHistory: ChatMessage[] = [],
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    try {
      // Get active provider config from SQLite
      const activeConfig = await sqliteService.getActiveLLMConfig();
      if (!activeConfig) {
        return {
          content: '',
          error: 'No active LLM provider configured. Please check settings.'
        };
      }

      // Get global settings
      const globalSettings = await sqliteService.getLLMGlobalSettings();

      // Build complete config
      const config: LLMConfig = {
        provider: activeConfig.provider,
        apiKey: activeConfig.api_key,
        baseUrl: activeConfig.base_url,
        model: activeConfig.model,
        temperature: globalSettings.temperature,
        maxTokens: globalSettings.max_tokens,
        systemPrompt: globalSettings.system_prompt
      };

      // Validate configuration
      if (config.provider !== 'ollama' && !config.apiKey) {
        return {
          content: '',
          error: `${config.provider} requires an API key. Please configure it in settings.`
        };
      }

      // Build messages array
      const messages: ChatMessage[] = [
        { role: 'system', content: config.systemPrompt || 'You are a helpful assistant.' },
        ...conversationHistory,
        { role: 'user', content: userMessage }
      ];

      // Call appropriate API based on provider
      switch (config.provider) {
        case 'openai':
          return this.callOpenAI(messages, config, onStream);
        case 'gemini':
          return this.callGemini(messages, config, onStream);
        case 'deepseek':
          return this.callDeepSeek(messages, config, onStream);
        case 'ollama':
          return this.callOllama(messages, config, onStream);
        case 'openrouter':
          return this.callOpenRouter(messages, config, onStream);
        default:
          return {
            content: '',
            error: `Unknown provider: ${config.provider}`
          };
      }
    } catch (error) {
      console.error('Error in chat:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  // Test connection
  async testConnection(): Promise<{ success: boolean; error?: string; model?: string }> {
    try {
      const activeConfig = await sqliteService.getActiveLLMConfig();
      if (!activeConfig) {
        return { success: false, error: 'No active provider configured' };
      }

      const response = await this.chat('Hello', [], undefined);
      if (response.error) {
        return { success: false, error: response.error };
      }
      return { success: true, model: activeConfig.model };
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Connection test failed'
      };
    }
  }
}

export const llmApiService = new LLMApiService();
