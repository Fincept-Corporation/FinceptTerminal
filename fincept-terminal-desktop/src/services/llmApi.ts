// LLM API Integration Service
// Unified interface for different LLM providers with streaming support

import { fetch } from '@tauri-apps/plugin-http';
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
    // Filter out system messages AND any placeholder/empty responses
    const conversationMessages = messages.filter(m =>
      m.role !== 'system' &&
      m.content.trim() !== '' &&
      m.content !== '(No response generated)'
    );

    // Gemini requires alternating roles - merge consecutive messages from same role
    const contents: any[] = [];
    let lastRole: string | null = null;

    for (const msg of conversationMessages) {
      const geminiRole = msg.role === 'assistant' ? 'model' : 'user';

      if (geminiRole === lastRole && contents.length > 0) {
        // Merge with previous message
        contents[contents.length - 1].parts.push({ text: '\n\n' + msg.content });
      } else {
        // New message
        contents.push({
          role: geminiRole,
          parts: [{ text: msg.content }]
        });
        lastRole = geminiRole;
      }
    }

    // Ensure we have at least one message
    if (contents.length === 0) {
      return {
        content: '',
        error: 'No messages to send to Gemini'
      };
    }

    const requestBody = {
      contents,
      systemInstruction: systemPrompt ? { parts: [{ text: systemPrompt }] } : undefined,
      generationConfig: {
        temperature: config.temperature,
        maxOutputTokens: config.maxTokens
      }
    };

    console.log('[Gemini] 📨 Request details:');
    console.log('[Gemini] - Contents count:', contents.length);
    console.log('[Gemini] - System instruction length:', systemPrompt?.length || 0, 'characters');
    console.log('[Gemini] - First content role:', contents[0]?.role);
    console.log('[Gemini] - First content preview:', JSON.stringify(contents[0]).substring(0, 200));

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(requestBody)
      });

      if (!response.ok) {
        const errorText = await response.text();
        let errorMessage = `Gemini API error: ${response.status}`;
        try {
          const error = JSON.parse(errorText);
          errorMessage = error.error?.message || errorMessage;
          console.error('[Gemini] API Error Response:', error);
        } catch {
          console.error('[Gemini] API Error Text:', errorText);
        }
        throw new Error(errorMessage);
      }

      if (onStream) {
        return await this.handleGeminiStream(response, onStream);
      } else {
        const data = await response.json();

        // Extract text content safely
        const candidate = data.candidates?.[0];
        if (!candidate) {
          throw new Error('No candidates in Gemini response');
        }

        const parts = candidate.content?.parts;
        if (!parts || parts.length === 0) {
          throw new Error('No content parts in Gemini response');
        }

        const text = parts[0].text;
        if (!text) {
          throw new Error('No text in Gemini response parts');
        }

        return {
          content: text,
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
    let accumulatedJson = '';

    if (!reader) {
      throw new Error('No response body');
    }

    try {
      // Accumulate all chunks into complete JSON string
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        const chunk = decoder.decode(value);
        accumulatedJson += chunk;
      }

      // Parse the complete JSON response (Gemini returns an array)
      try {
        const parsed = JSON.parse(accumulatedJson);

        // Gemini returns an array of response objects
        if (Array.isArray(parsed)) {
          for (const item of parsed) {
            const content = item.candidates?.[0]?.content?.parts?.[0]?.text || '';
            if (content) {
              fullContent += content;
              onStream(content, false);
            }
          }
        } else {
          // Single object response
          const content = parsed.candidates?.[0]?.content?.parts?.[0]?.text || '';
          if (content) {
            fullContent = content;
            onStream(content, false);
          }
        }
      } catch (e) {
        console.error('[Gemini Stream] Failed to parse JSON:', e);
        console.error('[Gemini Stream] Accumulated JSON:', accumulatedJson);
        throw new Error('Failed to parse Gemini response');
      }

      onStream('', true);
      return { content: fullContent };
    } finally {
      reader.releaseLock();
    }
  }

  // Gemini with function calling
  private async callGeminiWithTools(
    userMessage: string,
    conversationHistory: ChatMessage[],
    tools: Array<any>,
    config: LLMConfig,
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    const url = `https://generativelanguage.googleapis.com/v1beta/models/${config.model}:generateContent?key=${config.apiKey}`;

    // Convert MCP tools to Gemini function format
    const functionDeclarations = tools.map(tool => {
      // Clean schema for Gemini - remove $schema and additionalProperties
      const cleanSchema = tool.inputSchema ? { ...tool.inputSchema } : { type: 'object', properties: {} };
      delete cleanSchema.$schema;
      delete cleanSchema.additionalProperties;

      return {
        name: `${tool.serverId}__${tool.name}`,
        description: tool.description || '',
        parameters: cleanSchema
      };
    });

    console.log('[MCP] Sending tools to Gemini:', functionDeclarations.length, 'tools');

    // Convert messages to Gemini format
    const systemPrompt = conversationHistory.find(m => m.role === 'system')?.content || config.systemPrompt || '';
    const conversationMessages = conversationHistory.filter(m => m.role !== 'system');

    const contents = [
      ...conversationMessages.map(msg => ({
        role: msg.role === 'assistant' ? 'model' : 'user',
        parts: [{ text: msg.content }]
      })),
      {
        role: 'user',
        parts: [{ text: userMessage }]
      }
    ];

    try {
      // First call - with tools
      let response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          contents,
          systemInstruction: systemPrompt ? { parts: [{ text: systemPrompt }] } : undefined,
          tools: [{ functionDeclarations }],
          generationConfig: {
            temperature: config.temperature,
            maxOutputTokens: config.maxTokens
          }
        })
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error?.message || `Gemini API error: ${response.status}`);
      }

      let data = await response.json();
      const candidate = data.candidates?.[0];

      // Track tool result content for fallback
      let resultContent = '';

      // Check if Gemini wants to call functions
      const functionCall = candidate?.content?.parts?.find((part: any) => part.functionCall);

      if (functionCall) {
        console.log('[MCP] Gemini requested function call:', functionCall.functionCall.name);

        const toolName = functionCall.functionCall.name;
        const toolArgs = functionCall.functionCall.args || {};

        // Notify UI
        if (onToolCall) {
          onToolCall(toolName, toolArgs);
        }

        // Execute tool
        const { mcpManager } = await import('./mcpManager');
        const [serverId, actualToolName] = toolName.includes('__')
          ? toolName.split('__')
          : [toolName, toolName];

        let toolResult: any;
        try {
          toolResult = await mcpManager.callTool(serverId, actualToolName, toolArgs);

          // Format result
          if (toolResult.content && Array.isArray(toolResult.content)) {
            resultContent = toolResult.content
              .map((c: any) => {
                if (c.text) return c.text;
                if (c.data) return typeof c.data === 'string' ? c.data : JSON.stringify(c.data, null, 2);
                return JSON.stringify(c, null, 2);
              })
              .join('\n\n');
          } else if (toolResult.content) {
            resultContent = typeof toolResult.content === 'string'
              ? toolResult.content
              : JSON.stringify(toolResult.content, null, 2);
          } else {
            resultContent = JSON.stringify(toolResult, null, 2);
          }

          // Notify UI of result
          if (onToolCall) {
            onToolCall(toolName, toolArgs, toolResult);
          }

          console.log('[MCP] Tool result:', resultContent);

          // Second call with function response
          contents.push({
            role: 'model',
            parts: [{ functionCall: functionCall.functionCall } as any]
          });

          contents.push({
            role: 'user',
            parts: [{
              functionResponse: {
                name: toolName,
                response: { result: resultContent }
              }
            } as any]
          });

        } catch (error) {
          console.error('[MCP] Tool execution error:', error);
          contents.push({
            role: 'user',
            parts: [{
              functionResponse: {
                name: toolName,
                response: { error: error instanceof Error ? error.message : 'Unknown error' }
              }
            } as any]
          });
        }

        // Get final response
        response = await fetch(url, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            contents,
            systemInstruction: systemPrompt ? { parts: [{ text: systemPrompt }] } : undefined,
            generationConfig: {
              temperature: config.temperature,
              maxOutputTokens: config.maxTokens
            }
          })
        });

        if (!response.ok) {
          const error = await response.json();
          throw new Error(error.error?.message || `Gemini API error: ${response.status}`);
        }

        data = await response.json();
      }

      // Extract final content
      let finalContent = data.candidates?.[0]?.content?.parts?.[0]?.text || '';

      // If Gemini didn't generate text after tool call, return the tool result
      if (!finalContent && resultContent) {
        finalContent = resultContent;
      }

      console.log('[MCP] Gemini final response length:', finalContent.length);

      // Signal completion to UI
      if (onStream && finalContent) {
        onStream(finalContent, false);
        onStream('', true);
      }

      return {
        content: finalContent,
        usage: {
          promptTokens: data.usageMetadata?.promptTokenCount || 0,
          completionTokens: data.usageMetadata?.candidatesTokenCount || 0,
          totalTokens: data.usageMetadata?.totalTokenCount || 0
        }
      };

    } catch (error) {
      console.error('Gemini function calling error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
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
    const url = `${config.baseUrl || 'http://localhost:11434'}/api/chat`;

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
        if (response.status === 404) {
          let errorMsg = '';
          try {
            const errorData = await response.json();
            if (errorData.error && errorData.error.includes('model')) {
              errorMsg = `Ollama model '${config.model}' not found. Please pull the model first using: ollama pull ${config.model}`;
            } else {
              errorMsg = errorData.error || 'Ollama server not found. Please ensure Ollama is running at http://localhost:11434';
            }
          } catch {
            errorMsg = 'Ollama server not found. Please ensure Ollama is running at http://localhost:11434';
          }
          throw new Error(errorMsg);
        }
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

  // Ollama with function calling
  private async callOllamaWithTools(
    userMessage: string,
    conversationHistory: ChatMessage[],
    tools: Array<any>,
    config: LLMConfig,
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    const url = `${config.baseUrl || 'http://localhost:11434'}/api/chat`;

    // Convert MCP tools to Ollama function format
    const ollamaTools = tools.map(tool => {
      // Clean schema for Ollama - remove $schema and additionalProperties
      const cleanSchema = tool.inputSchema ? { ...tool.inputSchema } : { type: 'object', properties: {} };
      delete cleanSchema.$schema;
      delete cleanSchema.additionalProperties;

      return {
        type: 'function',
        function: {
          name: `${tool.serverId}__${tool.name}`,
          description: tool.description || '',
          parameters: cleanSchema
        }
      };
    });

    console.log('[MCP] Sending tools to Ollama:', ollamaTools.length, 'tools');

    // Build messages
    const systemPrompt = conversationHistory.find(m => m.role === 'system')?.content || config.systemPrompt || '';
    const conversationMessages = conversationHistory.filter(m => m.role !== 'system');

    let messages: any[] = [
      { role: 'system', content: systemPrompt + '\n\nYou have access to tools. Use them when the user asks for specific data or actions.' },
      ...conversationMessages.map(msg => ({ role: msg.role, content: msg.content })),
      { role: 'user', content: userMessage }
    ];

    try {
      // First call - with tools
      let response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          model: config.model,
          messages: messages,
          tools: ollamaTools,
          stream: false,
          options: {
            temperature: config.temperature,
            num_predict: config.maxTokens
          }
        })
      });

      if (!response.ok) {
        if (response.status === 404) {
          throw new Error(`Ollama model '${config.model}' not found. Please pull the model first using: ollama pull ${config.model}`);
        }
        throw new Error(`Ollama API error: ${response.status}`);
      }

      let data = await response.json();
      let assistantMessage = data.message;

      // Track tool result content for fallback
      let resultContent = '';

      // Check if Ollama wants to call tools
      if (assistantMessage.tool_calls && assistantMessage.tool_calls.length > 0) {
        console.log('[MCP] Ollama requested tool calls:', assistantMessage.tool_calls.length);

        // Add assistant's tool call message to conversation
        messages.push(assistantMessage);

        // Execute each tool call
        for (const toolCall of assistantMessage.tool_calls) {
          const toolName = toolCall.function.name;
          const toolArgs = toolCall.function.arguments;

          console.log(`[MCP] Calling tool: ${toolName}`, toolArgs);

          // Notify UI that tool is being called
          if (onToolCall) {
            onToolCall(toolName, toolArgs);
          }

          // Import mcpManager dynamically to avoid circular dependency
          const { mcpManager } = await import('./mcpManager');

          // Execute the tool
          try {
            // Parse server ID from tool name (format: serverId__toolName)
            const [serverId, actualToolName] = toolName.includes('__')
              ? toolName.split('__')
              : [toolName, toolName];

            const toolResult = await mcpManager.callTool(serverId, actualToolName, toolArgs);

            // Format result for Ollama
            if (toolResult.content && Array.isArray(toolResult.content)) {
              resultContent = toolResult.content
                .map((c: any) => {
                  if (c.text) return c.text;
                  if (c.data) return typeof c.data === 'string' ? c.data : JSON.stringify(c.data, null, 2);
                  return JSON.stringify(c, null, 2);
                })
                .join('\n\n');
            } else if (toolResult.content) {
              resultContent = typeof toolResult.content === 'string'
                ? toolResult.content
                : JSON.stringify(toolResult.content, null, 2);
            } else {
              resultContent = JSON.stringify(toolResult, null, 2);
            }

            // Add tool result to conversation
            messages.push({
              role: 'tool',
              content: resultContent
            });

            // Notify UI of result
            if (onToolCall) {
              onToolCall(toolName, toolArgs, toolResult);
            }

            console.log(`[MCP] Tool result:`, resultContent);
          } catch (error) {
            console.error(`[MCP] Tool execution error:`, error);
            messages.push({
              role: 'tool',
              content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
            });
          }
        }

        // Second call - get final response with tool results
        response = await fetch(url, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            model: config.model,
            messages: messages,
            stream: false,
            options: {
              temperature: config.temperature,
              num_predict: config.maxTokens
            }
          })
        });

        if (!response.ok) {
          throw new Error(`Ollama API error: ${response.status}`);
        }

        data = await response.json();
        assistantMessage = data.message;
      }

      // Extract final content
      let finalContent = assistantMessage.content || '';

      // If Ollama didn't generate text after tool call, return the tool result
      if (!finalContent && resultContent) {
        finalContent = resultContent;
      }

      console.log('[MCP] Ollama final response length:', finalContent.length);

      // Signal completion to UI
      if (onStream && finalContent) {
        onStream(finalContent, false);
        onStream('', true);
      }

      return {
        content: finalContent,
        usage: {
          promptTokens: data.prompt_eval_count || 0,
          completionTokens: data.eval_count || 0,
          totalTokens: (data.prompt_eval_count || 0) + (data.eval_count || 0)
        }
      };

    } catch (error) {
      console.error('Ollama function calling error:', error);
      return {
        content: '',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
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
        const errorData = await response.json();
        console.error('OpenRouter full error response:', JSON.stringify(errorData, null, 2));
        const errorMessage = errorData.error?.message || errorData.message || JSON.stringify(errorData);
        throw new Error(`OpenRouter error (${response.status}): ${errorMessage}`);
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

  // Chat with MCP tools support (Full Implementation)
  async chatWithTools(
    userMessage: string,
    conversationHistory: ChatMessage[] = [],
    tools: Array<any> = [],
    onStream?: StreamCallback,
    onToolCall?: (toolName: string, args: any, result?: any) => void
  ): Promise<LLMResponse> {
    try {
      // Get active provider config
      const activeConfig = await sqliteService.getActiveLLMConfig();
      if (!activeConfig) {
        return {
          content: '',
          error: 'No active LLM provider configured. Please check settings.'
        };
      }

      // OpenAI, OpenRouter, Gemini, DeepSeek, and Ollama support function calling
      const supportsTools = ['openai', 'openrouter', 'gemini', 'deepseek', 'ollama'].includes(activeConfig.provider);

      if (!supportsTools && tools.length > 0) {
        // Provider doesn't support tools - show warning message
        console.warn(`[MCP] Provider '${activeConfig.provider}' does not support function calling. Available tools: ${tools.length}`);

        const warningMessage = `⚠️ MCP Tools Not Supported\n\n` +
          `Your current LLM provider (${activeConfig.provider.toUpperCase()}) does not support function calling with MCP tools.\n\n` +
          `**Available MCP Tools:** ${tools.length} tools detected\n` +
          `**Supported Providers:** OpenAI, Gemini, DeepSeek, OpenRouter, Ollama\n\n` +
          `To use MCP tools, please switch to a supported provider in Settings (CONFIG button).`;

        // Return warning as response
        return {
          content: warningMessage,
          error: undefined
        };
      }

      // If no tools provided, fall back to regular chat
      if (tools.length === 0) {
        return this.chat(userMessage, conversationHistory, onStream);
      }

      const globalSettings = await sqliteService.getLLMGlobalSettings();

      const config: LLMConfig = {
        provider: activeConfig.provider,
        apiKey: activeConfig.api_key,
        baseUrl: activeConfig.base_url,
        model: activeConfig.model,
        temperature: globalSettings.temperature,
        maxTokens: globalSettings.max_tokens,
        systemPrompt: globalSettings.system_prompt
      };

      // Handle Gemini separately since it has different API format
      if (activeConfig.provider === 'gemini') {
        return this.callGeminiWithTools(userMessage, conversationHistory, tools, config, onStream, onToolCall);
      }

      // Handle Ollama separately since it has different API format
      if (activeConfig.provider === 'ollama') {
        return this.callOllamaWithTools(userMessage, conversationHistory, tools, config, onStream, onToolCall);
      }

      // Build messages for OpenAI/OpenRouter/DeepSeek (all use OpenAI format)
      let messages: any[] = [
        { role: 'system', content: config.systemPrompt || 'You are a helpful assistant with access to tools. Use them when needed.' },
        ...conversationHistory.map(msg => ({ role: msg.role, content: msg.content })),
        { role: 'user', content: userMessage }
      ];

      // Convert MCP tools to OpenAI function format
      // Prefix tool name with serverId for proper routing
      const functions = tools.map(tool => ({
        type: 'function',
        function: {
          name: `${tool.serverId}__${tool.name}`, // Format: serverId__toolName
          description: tool.description || '',
          parameters: tool.inputSchema || { type: 'object', properties: {} }
        }
      }));

      console.log('[MCP] Sending tools to API:', functions.length, 'tools');
      console.log('[MCP] Tool names:', functions.map(f => f.function.name));

      // Determine base URL based on provider
      let baseUrl = config.baseUrl;
      if (!baseUrl) {
        if (activeConfig.provider === 'deepseek') {
          baseUrl = 'https://api.deepseek.com';
        } else if (activeConfig.provider === 'openrouter') {
          baseUrl = 'https://openrouter.ai/api/v1';
        } else {
          baseUrl = 'https://api.openai.com/v1';
        }
      }

      const url = `${baseUrl}/chat/completions`;

      // First call - with tools
      let response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${config.apiKey}`,
          ...(activeConfig.provider === 'openrouter' ? {
            'HTTP-Referer': window.location.origin,
            'X-Title': 'Fincept Terminal'
          } : {})
        },
        body: JSON.stringify({
          model: config.model,
          messages: messages,
          tools: functions,
          tool_choice: 'auto',
          temperature: config.temperature,
          max_tokens: config.maxTokens
        })
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error?.message || `API error: ${response.status}`);
      }

      let data = await response.json();
      let assistantMessage = data.choices[0].message;

      // Check if AI wants to call tools
      if (assistantMessage.tool_calls && assistantMessage.tool_calls.length > 0) {
        console.log('[MCP] AI requested tool calls:', assistantMessage.tool_calls.length);

        // Add assistant's tool call message to conversation
        messages.push(assistantMessage);

        // Execute each tool call
        for (const toolCall of assistantMessage.tool_calls) {
          const toolName = toolCall.function.name;
          const toolArgs = JSON.parse(toolCall.function.arguments);

          console.log(`[MCP] Calling tool: ${toolName}`, toolArgs);

          // Notify UI that tool is being called
          if (onToolCall) {
            onToolCall(toolName, toolArgs);
          }

          // Import mcpManager dynamically to avoid circular dependency
          const { mcpManager } = await import('./mcpManager');

          // Execute the tool
          try {
            // Parse server ID from tool name (format: serverId__toolName)
            const [serverId, actualToolName] = toolName.includes('__')
              ? toolName.split('__')
              : [toolName, toolName];

            const toolResult = await mcpManager.callTool(serverId, actualToolName, toolArgs);

            // Format result for OpenAI - Better formatting
            let resultContent = '';
            if (toolResult.content && Array.isArray(toolResult.content)) {
              resultContent = toolResult.content
                .map((c: any) => {
                  if (c.text) return c.text;
                  if (c.data) return typeof c.data === 'string' ? c.data : JSON.stringify(c.data, null, 2);
                  return JSON.stringify(c, null, 2);
                })
                .join('\n\n');
            } else if (toolResult.content) {
              resultContent = typeof toolResult.content === 'string'
                ? toolResult.content
                : JSON.stringify(toolResult.content, null, 2);
            } else {
              resultContent = JSON.stringify(toolResult, null, 2);
            }

            // Add tool result to conversation
            messages.push({
              role: 'tool',
              tool_call_id: toolCall.id,
              content: resultContent
            });

            // Notify UI of result
            if (onToolCall) {
              onToolCall(toolName, toolArgs, toolResult);
            }

            console.log(`[MCP] Tool result:`, resultContent);
          } catch (error) {
            console.error(`[MCP] Tool execution error:`, error);
            messages.push({
              role: 'tool',
              tool_call_id: toolCall.id,
              content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
            });
          }
        }

        // Second call - get final response with tool results
        response = await fetch(url, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${config.apiKey}`,
            ...(activeConfig.provider === 'openrouter' ? {
              'HTTP-Referer': window.location.origin,
              'X-Title': 'Fincept Terminal'
            } : {})
          },
          body: JSON.stringify({
            model: config.model,
            messages: messages,
            temperature: config.temperature,
            max_tokens: config.maxTokens
          })
        });

        if (!response.ok) {
          const error = await response.json();
          throw new Error(error.error?.message || `API error: ${response.status}`);
        }

        data = await response.json();
        assistantMessage = data.choices[0].message;
      }

      // Ensure content exists
      const finalContent = assistantMessage.content || '';
      console.log('[MCP] Final response length:', finalContent.length);

      return {
        content: finalContent,
        usage: {
          promptTokens: data.usage?.prompt_tokens || 0,
          completionTokens: data.usage?.completion_tokens || 0,
          totalTokens: data.usage?.total_tokens || 0
        }
      };

    } catch (error) {
      console.error('Error in chatWithTools:', error);
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
