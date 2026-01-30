// LLM API Integration Service
// Unified interface for different LLM providers using native SDKs

import { fetch } from '@tauri-apps/plugin-http';
import { invoke } from '@tauri-apps/api/core';
import { llmLogger } from '../core/loggerService';

// Native SDK imports
import { ChatOpenAI } from '@langchain/openai';
import { ChatAnthropic } from '@langchain/anthropic';
import { ChatGoogleGenerativeAI } from '@langchain/google-genai';
import { ChatOllama } from '@langchain/ollama';
import Groq from 'groq-sdk';
import { HumanMessage, SystemMessage, AIMessage } from '@langchain/core/messages';

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
  // Execute MCP tool (internal or external)
  private async executeMCPTool(serverId: string, toolName: string, args: any): Promise<any> {
    if (serverId === 'fincept-terminal') {
      // Internal tool - use terminalMCPProvider directly
      const { terminalMCPProvider, isInternalMCPInitialized } = await import('../mcp/internal');

      // Check if MCP system is initialized
      if (!isInternalMCPInitialized()) {
        throw new Error('Internal MCP system not initialized. Please restart the application.');
      }

      const result = await terminalMCPProvider.callTool(toolName, args);

      if (!result.success) {
        throw new Error(result.error || 'Tool execution failed');
      }

      // Return full result object (includes chart_data, ticker, company, etc.)
      return result;
    } else {
      // External MCP server - use mcpManager
      const { mcpManager } = await import('../mcp/mcpManager');
      return await mcpManager.callTool(serverId, toolName, args);
    }
  }

  // Normalize model ID for native provider APIs
  // OpenRouter uses "provider/model" format, but native APIs need just the model name
  private normalizeModelId(provider: string, modelId: string): string {
    if (!modelId) return modelId;

    // If model ID contains "/", extract the model name part for native APIs
    if (modelId.includes('/')) {
      const parts = modelId.split('/');
      const providerPrefix = parts[0].toLowerCase();
      const modelName = parts.slice(1).join('/');

      // Map of provider prefixes to their native provider names
      const providerMap: Record<string, string[]> = {
        'openai': ['openai', 'gpt'],
        'anthropic': ['anthropic', 'claude'],
        'google': ['google', 'gemini'],
        'groq': ['groq', 'llama', 'mixtral'],
        'deepseek': ['deepseek'],
        'meta': ['meta-llama', 'llama'],
        'mistral': ['mistralai', 'mistral'],
      };

      // Check if this is a native provider call (not openrouter)
      if (provider !== 'openrouter') {
        // For native APIs, return just the model name without provider prefix
        const providerPrefixes = providerMap[provider] || [];
        if (providerPrefixes.some(p => providerPrefix.includes(p))) {
          return modelName;
        }
        // If prefix doesn't match provider, return as-is (might be custom)
        return modelName;
      }
    }

    return modelId;
  }

  // Convert ChatMessage array to LangChain message format
  private toLangChainMessages(messages: ChatMessage[]) {
    return messages.map(msg => {
      switch (msg.role) {
        case 'system':
          return new SystemMessage(msg.content);
        case 'user':
          return new HumanMessage(msg.content);
        case 'assistant':
          return new AIMessage(msg.content);
        default:
          return new HumanMessage(msg.content);
      }
    });
  }

  // OpenAI API using @langchain/openai
  private async callOpenAI(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    try {
      const normalizedModel = this.normalizeModelId('openai', config.model);
      const model = new ChatOpenAI({
        openAIApiKey: config.apiKey,
        modelName: normalizedModel,
        temperature: config.temperature,
        maxTokens: config.maxTokens,
        streaming: !!onStream,
        configuration: config.baseUrl ? {
          baseURL: config.baseUrl
        } : undefined
      });

      const langChainMessages = this.toLangChainMessages(messages);

      if (onStream) {
        let fullContent = '';
        const stream = await model.stream(langChainMessages);

        for await (const chunk of stream) {
          const content = typeof chunk.content === 'string' ? chunk.content : '';
          fullContent += content;
          onStream(content, false);
        }

        onStream('', true);
        return { content: fullContent };
      } else {
        const response = await model.invoke(langChainMessages);
        const content = typeof response.content === 'string' ? response.content : '';
        return {
          content,
          usage: {
            promptTokens: response.usage_metadata?.input_tokens || 0,
            completionTokens: response.usage_metadata?.output_tokens || 0,
            totalTokens: response.usage_metadata?.total_tokens || 0
          }
        };
      }
    } catch (error) {
      llmLogger.error('OpenAI API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'OpenAI API error'
      };
    }
  }

  // Anthropic API using @langchain/anthropic
  private async callAnthropic(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    try {
      const normalizedModel = this.normalizeModelId('anthropic', config.model);
      const model = new ChatAnthropic({
        anthropicApiKey: config.apiKey,
        modelName: normalizedModel,
        temperature: config.temperature,
        maxTokens: config.maxTokens,
        streaming: !!onStream
      });

      const langChainMessages = this.toLangChainMessages(messages);

      if (onStream) {
        let fullContent = '';
        const stream = await model.stream(langChainMessages);

        for await (const chunk of stream) {
          const content = typeof chunk.content === 'string' ? chunk.content : '';
          fullContent += content;
          onStream(content, false);
        }

        onStream('', true);
        return { content: fullContent };
      } else {
        const response = await model.invoke(langChainMessages);
        const content = typeof response.content === 'string' ? response.content : '';
        return {
          content,
          usage: {
            promptTokens: response.usage_metadata?.input_tokens || 0,
            completionTokens: response.usage_metadata?.output_tokens || 0,
            totalTokens: response.usage_metadata?.total_tokens || 0
          }
        };
      }
    } catch (error) {
      llmLogger.error('Anthropic API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Anthropic API error'
      };
    }
  }

  // Google Gemini API using @langchain/google-genai
  private async callGemini(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    try {
      const normalizedModel = this.normalizeModelId('google', config.model);
      const model = new ChatGoogleGenerativeAI({
        apiKey: config.apiKey,
        model: normalizedModel,
        temperature: config.temperature,
        maxOutputTokens: config.maxTokens,
        streaming: !!onStream
      });

      const langChainMessages = this.toLangChainMessages(messages);

      if (onStream) {
        let fullContent = '';
        const stream = await model.stream(langChainMessages);

        for await (const chunk of stream) {
          const content = typeof chunk.content === 'string' ? chunk.content : '';
          fullContent += content;
          onStream(content, false);
        }

        onStream('', true);
        return { content: fullContent };
      } else {
        const response = await model.invoke(langChainMessages);
        const content = typeof response.content === 'string' ? response.content : '';
        return {
          content,
          usage: {
            promptTokens: response.usage_metadata?.input_tokens || 0,
            completionTokens: response.usage_metadata?.output_tokens || 0,
            totalTokens: response.usage_metadata?.total_tokens || 0
          }
        };
      }
    } catch (error) {
      llmLogger.error('Gemini API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Gemini API error'
      };
    }
  }

  // Groq API using groq-sdk
  private async callGroq(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    try {
      const normalizedModel = this.normalizeModelId('groq', config.model);
      const groq = new Groq({
        apiKey: config.apiKey,
        dangerouslyAllowBrowser: true
      });

      const groqMessages = messages.map(msg => ({
        role: msg.role as 'system' | 'user' | 'assistant',
        content: msg.content
      }));

      if (onStream) {
        let fullContent = '';
        const stream = await groq.chat.completions.create({
          model: normalizedModel,
          messages: groqMessages,
          temperature: config.temperature,
          max_tokens: config.maxTokens,
          stream: true
        });

        for await (const chunk of stream) {
          const content = chunk.choices[0]?.delta?.content || '';
          fullContent += content;
          onStream(content, false);
        }

        onStream('', true);
        return { content: fullContent };
      } else {
        const response = await groq.chat.completions.create({
          model: normalizedModel,
          messages: groqMessages,
          temperature: config.temperature,
          max_tokens: config.maxTokens
        });

        return {
          content: response.choices[0]?.message?.content || '',
          usage: {
            promptTokens: response.usage?.prompt_tokens || 0,
            completionTokens: response.usage?.completion_tokens || 0,
            totalTokens: response.usage?.total_tokens || 0
          }
        };
      }
    } catch (error) {
      llmLogger.error('Groq API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Groq API error'
      };
    }
  }

  // DeepSeek API (OpenAI-compatible, uses ChatOpenAI with custom baseUrl)
  private async callDeepSeek(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const deepseekConfig = {
      ...config,
      baseUrl: config.baseUrl || 'https://api.deepseek.com/v1'
    };
    return this.callOpenAI(messages, deepseekConfig, onStream);
  }

  // Ollama API using @langchain/ollama
  private async callOllama(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    try {
      const model = new ChatOllama({
        baseUrl: config.baseUrl || 'http://localhost:11434',
        model: config.model,
        temperature: config.temperature,
        numPredict: config.maxTokens
      });

      const langChainMessages = this.toLangChainMessages(messages);

      if (onStream) {
        let fullContent = '';
        const stream = await model.stream(langChainMessages);

        for await (const chunk of stream) {
          const content = typeof chunk.content === 'string' ? chunk.content : '';
          fullContent += content;
          onStream(content, false);
        }

        onStream('', true);
        return { content: fullContent };
      } else {
        const response = await model.invoke(langChainMessages);
        const content = typeof response.content === 'string' ? response.content : '';
        return { content };
      }
    } catch (error) {
      llmLogger.error('Ollama API Error:', error);
      const errorMsg = error instanceof Error ? error.message : 'Ollama API error';
      if (errorMsg.includes('ECONNREFUSED') || errorMsg.includes('fetch')) {
        return {
          content: '',
          error: 'Ollama server not found. Please ensure Ollama is running at http://localhost:11434'
        };
      }
      return {
        content: '',
        error: errorMsg
      };
    }
  }

  // OpenRouter API (for custom models from Model Library)
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
          'HTTP-Referer': typeof window !== 'undefined' ? window.location.origin : 'https://fincept.ai',
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
        const errorData = await response.json();
        llmLogger.debug('OpenRouter full error response:', JSON.stringify(errorData, null, 2));
        const errorMessage = errorData.error?.message || errorData.message || JSON.stringify(errorData);
        throw new Error(`OpenRouter error (${response.status}): ${errorMessage}`);
      }

      if (onStream) {
        return await this.handleOpenRouterStream(response, onStream);
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
      llmLogger.error('OpenRouter API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'OpenRouter API error'
      };
    }
  }

  private async handleOpenRouterStream(response: Response, onStream: StreamCallback): Promise<LLMResponse> {
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

  // Fincept LLM API (custom)
  private async callFincept(
    messages: ChatMessage[],
    config: LLMConfig,
    onStream?: StreamCallback
  ): Promise<LLMResponse> {
    const url = config.baseUrl || 'https://finceptbackend.share.zrok.io/research/llm';

    try {
      // Build full prompt including system message and conversation history
      const systemPrompt = messages.find(m => m.role === 'system')?.content || '';
      const conversationParts: string[] = [];

      if (systemPrompt) {
        conversationParts.push(`System: ${systemPrompt}`);
      }

      // Add conversation history
      for (const msg of messages) {
        if (msg.role === 'user') {
          conversationParts.push(`User: ${msg.content}`);
        } else if (msg.role === 'assistant') {
          conversationParts.push(`Assistant: ${msg.content}`);
        }
      }

      const fullPrompt = conversationParts.join('\n\n');

      const requestBody = {
        prompt: fullPrompt,
        temperature: config.temperature,
        max_tokens: config.maxTokens
      };

      llmLogger.debug('Fincept API request:', { url, bodyKeys: Object.keys(requestBody) });

      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'X-API-Key': config.apiKey || ''
        },
        body: JSON.stringify(requestBody)
      });

      if (!response.ok) {
        const error = await response.json().catch(() => ({}));
        llmLogger.error('Fincept API error response:', error);
        throw new Error(error.detail || error.message || `Fincept API error: ${response.status}`);
      }

      const data = await response.json();
      llmLogger.debug('Fincept API response:', data);

      // Response can be nested in data.data.response or directly in data.response
      const responseData = data.data || data;
      const content = responseData.response || responseData.content || responseData.answer || responseData.text || responseData.result || '';

      if (onStream && content) {
        onStream(content, false);
        onStream('', true);
      }

      // Usage can also be nested in data.data
      const usage = responseData.usage || data.usage || {};

      return {
        content,
        usage: {
          promptTokens: usage.input_tokens || usage.prompt_tokens || 0,
          completionTokens: usage.output_tokens || usage.completion_tokens || 0,
          totalTokens: usage.total_tokens || 0
        }
      };
    } catch (error) {
      llmLogger.error('Fincept API Error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Fincept API error'
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
      // Get active provider config from Rust SQLite
      const configs = await invoke<any[]>('db_get_llm_configs');
      const activeConfig = configs.find(c => c.is_active);
      if (!activeConfig) {
        return {
          content: '',
          error: 'No active LLM provider configured. Please check settings.'
        };
      }

      // Get global settings
      const globalSettings = await invoke<any>('db_get_llm_global_settings');

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
      if (config.provider !== 'ollama' && config.provider !== 'fincept' && !config.apiKey) {
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
        case 'anthropic':
          return this.callAnthropic(messages, config, onStream);
        case 'gemini':
        case 'google':
          return this.callGemini(messages, config, onStream);
        case 'groq':
          return this.callGroq(messages, config, onStream);
        case 'deepseek':
          return this.callDeepSeek(messages, config, onStream);
        case 'ollama':
          return this.callOllama(messages, config, onStream);
        case 'openrouter':
          return this.callOpenRouter(messages, config, onStream);
        case 'fincept':
          return this.callFincept(messages, config, onStream);
        default:
          // For custom models from Model Library, check if it's an OpenRouter model
          if (config.model && config.model.includes('/')) {
            return this.callOpenRouter(messages, config, onStream);
          }
          return {
            content: '',
            error: `Unknown provider: ${config.provider}. Please configure it in settings.`
          };
      }
    } catch (error) {
      llmLogger.error('Error in chat:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  // Chat with MCP tools support
  async chatWithTools(
    userMessage: string,
    conversationHistory: ChatMessage[] = [],
    tools: Array<any> = [],
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    try {
      // Get active provider config from Rust SQLite
      const configs = await invoke<any[]>('db_get_llm_configs');
      const activeConfig = configs.find(c => c.is_active);
      if (!activeConfig) {
        return {
          content: '',
          error: 'No active LLM provider configured. Please check settings.'
        };
      }

      // Providers that support function calling (MCP tools)
      const supportsTools = ['openai', 'anthropic', 'gemini', 'google', 'groq', 'openrouter', 'fincept'].includes(activeConfig.provider);

      if (!supportsTools && tools.length > 0) {
        llmLogger.warn(`Provider '${activeConfig.provider}' does not support function calling. Available tools: ${tools.length}`);

        const warningMessage = `[WARN] MCP Tools Not Supported\n\n` +
          `Your current LLM provider (${activeConfig.provider.toUpperCase()}) does not support function calling with MCP tools.\n\n` +
          `**Available MCP Tools:** ${tools.length} tools detected\n` +
          `**Supported Providers:** OpenAI, Anthropic, Gemini, Groq, OpenRouter\n\n` +
          `To use MCP tools, please switch to a supported provider in Settings.`;

        return {
          content: warningMessage,
          error: undefined
        };
      }

      // If no tools provided, fall back to regular chat
      if (tools.length === 0) {
        return this.chat(userMessage, conversationHistory, onStream);
      }

      // Get global settings
      const globalSettings = await invoke<any>('db_get_llm_global_settings');

      // Build config
      const config: LLMConfig = {
        provider: activeConfig.provider,
        apiKey: activeConfig.api_key,
        baseUrl: activeConfig.base_url,
        model: activeConfig.model,
        temperature: globalSettings.temperature,
        maxTokens: globalSettings.max_tokens,
        systemPrompt: globalSettings.system_prompt
      };

      // Format tools for OpenAI-style function calling
      const formattedTools = tools.map((tool: any) => ({
        type: 'function' as const,
        function: {
          name: `${tool.serverId}__${tool.name}`,
          description: tool.description || `Tool: ${tool.name} from ${tool.serverId}`,
          parameters: tool.inputSchema || {
            type: 'object',
            properties: {},
            required: []
          }
        }
      }));

      llmLogger.debug(`[chatWithTools] Formatted ${formattedTools.length} tools for LLM`,
        formattedTools.map(t => t.function.name));

      // Build messages array
      const defaultSystemPrompt = `You are a helpful assistant with access to tools. Use them when appropriate to help the user.

When you use the edgar_get_financials tool, it returns financial data with chart_data containing YoY comparisons. The chart_data includes:
- revenue, net_income, total_assets, total_equity with historical periods
- growth metrics showing YoY percentage changes

Mention the year-over-year growth percentages when discussing financial performance to provide meaningful context.`;

      const messages: ChatMessage[] = [
        { role: 'system', content: config.systemPrompt || defaultSystemPrompt },
        ...conversationHistory,
        { role: 'user', content: userMessage }
      ];

      // Use OpenAI-compatible tool calling for supported providers
      return await this.callWithTools(messages, config, formattedTools, onStream, onToolCall);

    } catch (error) {
      llmLogger.error('Error in chatWithTools:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  // Call LLM with tool support (OpenAI-compatible format)
  private async callWithTools(
    messages: ChatMessage[],
    config: LLMConfig,
    tools: any[],
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    try {
      // For OpenAI, Groq, DeepSeek - use ChatOpenAI with tools
      if (['openai', 'groq', 'deepseek'].includes(config.provider)) {
        const baseUrl = config.provider === 'groq'
          ? 'https://api.groq.com/openai/v1'
          : config.provider === 'deepseek'
            ? 'https://api.deepseek.com/v1'
            : config.baseUrl;

        const normalizedModel = this.normalizeModelId(config.provider, config.model);
        const model = new ChatOpenAI({
          openAIApiKey: config.apiKey,
          modelName: normalizedModel,
          temperature: config.temperature,
          maxTokens: config.maxTokens,
          configuration: baseUrl ? { baseURL: baseUrl } : undefined
        });

        const modelWithTools = model.bindTools(tools);
        const langChainMessages = this.toLangChainMessages(messages);
        const response = await modelWithTools.invoke(langChainMessages);

        // Check if tool calls were made
        if (response.tool_calls && response.tool_calls.length > 0) {
          let toolResults = '';

          for (const toolCall of response.tool_calls) {
            const functionName = toolCall.name;
            const args = toolCall.args;

            // Parse function name to get serverId and toolName
            const parts = functionName.split('__');
            if (parts.length === 2) {
              const [serverId, toolName] = parts;

              onToolCall?.(toolName, args);

              try {
                const result = await this.executeMCPTool(serverId, toolName, args);
                onToolCall?.(toolName, args, result);
                toolResults += `\n**Tool: ${toolName}**\nResult: ${JSON.stringify(result, null, 2)}\n`;
              } catch (toolError) {
                toolResults += `\n**Tool: ${toolName}**\nError: ${toolError instanceof Error ? toolError.message : 'Unknown error'}\n`;
              }
            }
          }

          // Return tool results directly for speed
          if (toolResults) {
            const content = toolResults.trim();
            if (onStream) {
              onStream(content, false);
              onStream('', true);
            }
            return { content };
          }
        }

        const content = typeof response.content === 'string' ? response.content : '';

        if (onStream && content) {
          onStream(content, false);
          onStream('', true);
        }

        return { content };
      }

      // For Anthropic - use ChatAnthropic with tools
      if (config.provider === 'anthropic') {
        const normalizedModel = this.normalizeModelId('anthropic', config.model);
        const model = new ChatAnthropic({
          anthropicApiKey: config.apiKey,
          modelName: normalizedModel,
          temperature: config.temperature,
          maxTokens: config.maxTokens
        });

        const modelWithTools = model.bindTools(tools);
        const langChainMessages = this.toLangChainMessages(messages);
        const response = await modelWithTools.invoke(langChainMessages);

        // Handle tool calls similar to OpenAI
        if (response.tool_calls && response.tool_calls.length > 0) {
          let toolResults = '';

          for (const toolCall of response.tool_calls) {
            const functionName = toolCall.name;
            const args = toolCall.args;

            const parts = functionName.split('__');
            if (parts.length === 2) {
              const [serverId, toolName] = parts;
              onToolCall?.(toolName, args);

              try {
                const result = await this.executeMCPTool(serverId, toolName, args);
                onToolCall?.(toolName, args, result);
                toolResults += `\n**Tool: ${toolName}**\nResult: ${JSON.stringify(result, null, 2)}\n`;
              } catch (toolError) {
                toolResults += `\n**Tool: ${toolName}**\nError: ${toolError instanceof Error ? toolError.message : 'Unknown error'}\n`;
              }
            }
          }

          if (toolResults) {
            const followUpMessages: ChatMessage[] = [
              ...messages,
              { role: 'assistant', content: `I used the following tools:\n${toolResults}` },
              { role: 'user', content: 'Please provide a summary based on the tool results above.' }
            ];

            return this.chat(followUpMessages[followUpMessages.length - 1].content, followUpMessages.slice(0, -1), onStream);
          }
        }

        const content = typeof response.content === 'string' ? response.content : '';

        if (onStream && content) {
          onStream(content, false);
          onStream('', true);
        }

        return { content };
      }

      // For Google/Gemini - use ChatGoogleGenerativeAI with tools
      if (['gemini', 'google'].includes(config.provider)) {
        const normalizedModel = this.normalizeModelId('google', config.model);
        const model = new ChatGoogleGenerativeAI({
          apiKey: config.apiKey,
          model: normalizedModel,
          temperature: config.temperature,
          maxOutputTokens: config.maxTokens
        });

        const modelWithTools = model.bindTools(tools);
        const langChainMessages = this.toLangChainMessages(messages);
        const response = await modelWithTools.invoke(langChainMessages);

        // Handle tool calls
        if (response.tool_calls && response.tool_calls.length > 0) {
          let toolResults = '';

          for (const toolCall of response.tool_calls) {
            const functionName = toolCall.name;
            const args = toolCall.args;

            const parts = functionName.split('__');
            if (parts.length === 2) {
              const [serverId, toolName] = parts;
              onToolCall?.(toolName, args);

              try {
                const result = await this.executeMCPTool(serverId, toolName, args);
                onToolCall?.(toolName, args, result);
                toolResults += `\n**Tool: ${toolName}**\nResult: ${JSON.stringify(result, null, 2)}\n`;
              } catch (toolError) {
                toolResults += `\n**Tool: ${toolName}**\nError: ${toolError instanceof Error ? toolError.message : 'Unknown error'}\n`;
              }
            }
          }

          if (toolResults) {
            const followUpMessages: ChatMessage[] = [
              ...messages,
              { role: 'assistant', content: `I used the following tools:\n${toolResults}` },
              { role: 'user', content: 'Please provide a summary based on the tool results above.' }
            ];

            return this.chat(followUpMessages[followUpMessages.length - 1].content, followUpMessages.slice(0, -1), onStream);
          }
        }

        const content = typeof response.content === 'string' ? response.content : '';

        if (onStream && content) {
          onStream(content, false);
          onStream('', true);
        }

        return { content };
      }

      // For OpenRouter - use HTTP API with tools
      if (config.provider === 'openrouter') {
        return await this.callOpenRouterWithTools(messages, config, tools, onStream, onToolCall);
      }

      // For Fincept - use HTTP API with tools (OpenAI-compatible)
      if (config.provider === 'fincept') {
        return await this.callFinceptWithTools(messages, config, tools, onStream, onToolCall);
      }

      // Fallback to regular chat if tools not supported
      llmLogger.warn(`Tool calling not implemented for provider: ${config.provider}, falling back to regular chat`);
      return this.chat(messages[messages.length - 1].content, messages.slice(0, -1), onStream);

    } catch (error) {
      llmLogger.error('Error in callWithTools:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }
  }

  // Fincept with tools support
  private async callFinceptWithTools(
    messages: ChatMessage[],
    config: LLMConfig,
    tools: any[],
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    const url = config.baseUrl || 'https://finceptbackend.share.zrok.io/research/llm';

    try {
      // Build full prompt
      const systemPrompt = messages.find(m => m.role === 'system')?.content || '';
      const conversationParts: string[] = [];

      if (systemPrompt) {
        conversationParts.push(`System: ${systemPrompt}`);
      }

      for (const msg of messages) {
        if (msg.role === 'user') {
          conversationParts.push(`User: ${msg.content}`);
        } else if (msg.role === 'assistant') {
          conversationParts.push(`Assistant: ${msg.content}`);
        }
      }

      const fullPrompt = conversationParts.join('\n\n');

      const requestBody: any = {
        prompt: fullPrompt,
        temperature: config.temperature,
        max_tokens: config.maxTokens,
        tools: tools.map(t => t.function) // Send tools in OpenAI format
      };

      // Only add tool_choice if tools are present
      if (tools.length > 0) {
        requestBody.tool_choice = { type: 'auto' };
      }

      console.log('[Fincept Tools] Request body:', JSON.stringify(requestBody, null, 2));
      llmLogger.debug('Fincept API with tools request:', { url, tools: tools.length });

      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'X-API-Key': config.apiKey || ''
        },
        body: JSON.stringify(requestBody)
      });

      console.log('[Fincept Tools] Response status:', response.status);

      if (!response.ok) {
        const error = await response.json().catch(() => ({}));
        console.error('[Fincept Tools] Error response:', error);
        llmLogger.error('Fincept API error response:', error);
        throw new Error(error.detail || error.message || `Fincept API error: ${response.status}`);
      }

      const data = await response.json();
      console.log('[Fincept Tools] Response data:', JSON.stringify(data, null, 2));
      llmLogger.debug('Fincept API response:', data);

      // Response structure: { success: true, data: { response, tool_calls, ... } }
      if (!data.success) {
        throw new Error(data.error || 'API request failed');
      }

      const responseData = data.data;

      // Check for tool calls in response
      console.log('[Fincept Tools] Checking for tool_calls:', responseData?.tool_calls);

      if (responseData.tool_calls && Array.isArray(responseData.tool_calls) && responseData.tool_calls.length > 0) {
        console.log('[Fincept Tools] Found tool calls:', responseData.tool_calls.length);
        let toolResults = '';

        for (const toolCall of responseData.tool_calls) {
          const functionName = toolCall.function?.name || toolCall.name;
          const argsRaw = toolCall.function?.arguments || toolCall.arguments;

          // Parse arguments if it's a string
          const args = typeof argsRaw === 'string' ? JSON.parse(argsRaw) : argsRaw;

          console.log('[Fincept Tools] Executing tool:', functionName, 'with args:', args);

          // Parse function name (format: serverId__toolName)
          const parts = functionName.split('__');
          if (parts.length === 2) {
            const [serverId, toolName] = parts;
            onToolCall?.(toolName, args);

            try {
              const result = await this.executeMCPTool(serverId, toolName, args);
              console.log('[Fincept Tools] Tool result:', result);
              onToolCall?.(toolName, args, result);

              // Extract the most relevant content from result
              let resultContent = '';
              if (result.content) {
                resultContent = result.content;
              } else if (result.data && typeof result.data === 'string') {
                resultContent = result.data;
              } else if (result.data?.markdown) {
                resultContent = result.data.markdown;
              } else if (result.markdown) {
                resultContent = result.markdown;
              } else if (result.message) {
                resultContent = result.message;
              } else if (result.data) {
                // Fallback: stringify data but limit size
                resultContent = JSON.stringify(result.data, null, 2).slice(0, 2000);
              } else {
                resultContent = JSON.stringify(result, null, 2).slice(0, 2000);
              }

              toolResults += `\n**Tool: ${toolName}**\n${resultContent}\n`;
            } catch (toolError) {
              console.error('[Fincept Tools] Tool error:', toolError);
              toolResults += `\n**Tool: ${toolName}**\nError: ${toolError instanceof Error ? toolError.message : 'Unknown error'}\n`;
            }
          }
        }

        // If we have tool results, send them back to LLM for final formatting
        if (toolResults) {
          console.log('[Fincept Tools] Sending tool results back to LLM for formatting');

          // Build SIMPLE prompt with ONLY the user's last question and tool results
          // No full history needed - just format the data
          const userQuestion = messages[messages.length - 1]?.content || 'Please summarize the data';
          const newPrompt = `User asked: "${userQuestion}"\n\nI retrieved this data:\n${toolResults}\n\nPlease provide a clear, concise summary of this data in natural language.`;

          const followUpBody = {
            prompt: newPrompt,
            temperature: config.temperature,
            max_tokens: config.maxTokens
          };

          const followUpResponse = await fetch(url, {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
              'X-API-Key': config.apiKey || ''
            },
            body: JSON.stringify(followUpBody)
          });

          if (!followUpResponse.ok) {
            throw new Error(`Follow-up call failed: ${followUpResponse.status}`);
          }

          const followUpData = await followUpResponse.json();
          const followUpContent = followUpData.data?.response || followUpData.data?.content || '';

          if (onStream && followUpContent) {
            onStream(followUpContent, false);
            onStream('', true);
          }

          return {
            content: followUpContent,
            usage: followUpData.data?.usage ? {
              promptTokens: (responseData.usage?.input_tokens || 0) + (followUpData.data.usage.input_tokens || 0),
              completionTokens: (responseData.usage?.output_tokens || 0) + (followUpData.data.usage.output_tokens || 0),
              totalTokens: (responseData.usage?.total_tokens || 0) + (followUpData.data.usage.total_tokens || 0)
            } : undefined
          };
        }
      }

      // No tool calls, return the response content
      const content = responseData.response || responseData.content || responseData.answer || responseData.text || '';
      console.log('[Fincept Tools] No tool calls, returning content:', content?.substring(0, 100));

      if (onStream && content) {
        onStream(content, false);
        onStream('', true);
      }

      const usage = responseData.usage || data.usage || {};

      return {
        content,
        usage: {
          promptTokens: usage.input_tokens || usage.prompt_tokens || 0,
          completionTokens: usage.output_tokens || usage.completion_tokens || 0,
          totalTokens: usage.total_tokens || 0
        }
      };
    } catch (error) {
      llmLogger.error('Fincept API with tools error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Fincept API error'
      };
    }
  }

  // OpenRouter with tools support
  private async callOpenRouterWithTools(
    messages: ChatMessage[],
    config: LLMConfig,
    tools: any[],
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    const url = `${config.baseUrl || 'https://openrouter.ai/api/v1'}/chat/completions`;

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${config.apiKey}`,
          'HTTP-Referer': typeof window !== 'undefined' ? window.location.origin : 'https://fincept.ai',
          'X-Title': 'Fincept Terminal'
        },
        body: JSON.stringify({
          model: config.model,
          messages: messages,
          tools: tools,
          temperature: config.temperature,
          max_tokens: config.maxTokens
        })
      });

      if (!response.ok) {
        const errorData = await response.json();
        throw new Error(errorData.error?.message || `OpenRouter error: ${response.status}`);
      }

      const data = await response.json();
      const choice = data.choices[0];

      // Check for tool calls
      if (choice.message?.tool_calls && choice.message.tool_calls.length > 0) {
        let toolResults = '';

        for (const toolCall of choice.message.tool_calls) {
          const functionName = toolCall.function.name;
          const args = JSON.parse(toolCall.function.arguments || '{}');

          const parts = functionName.split('__');
          if (parts.length === 2) {
            const [serverId, toolName] = parts;
            onToolCall?.(toolName, args);

            try {
              const { mcpManager } = await import('../mcp/mcpManager');
              const result = await mcpManager.callTool(serverId, toolName, args);
              onToolCall?.(toolName, args, result);
              toolResults += `\n**Tool: ${toolName}**\nResult: ${JSON.stringify(result, null, 2)}\n`;
            } catch (toolError) {
              toolResults += `\n**Tool: ${toolName}**\nError: ${toolError instanceof Error ? toolError.message : 'Unknown error'}\n`;
            }
          }
        }

        if (toolResults) {
          const content = toolResults.trim();
          if (onStream) {
            onStream(content, false);
            onStream('', true);
          }
          return { content };
        }
      }

      const content = choice.message?.content || '';

      if (onStream && content) {
        onStream(content, false);
        onStream('', true);
      }

      return {
        content,
        usage: {
          promptTokens: data.usage?.prompt_tokens || 0,
          completionTokens: data.usage?.completion_tokens || 0,
          totalTokens: data.usage?.total_tokens || 0
        }
      };
    } catch (error) {
      llmLogger.error('OpenRouter with tools error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'OpenRouter API error'
      };
    }
  }

  // Test connection
  async testConnection(): Promise<{ success: boolean; error?: string; model?: string }> {
    try {
      const configs = await invoke<any[]>('db_get_llm_configs');
      const activeConfig = configs.find(c => c.is_active);
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
