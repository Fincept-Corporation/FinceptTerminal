import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { flushSync } from 'react-dom';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useAuth } from '@/contexts/AuthContext';
import { useBrokerContext } from '@/contexts/BrokerContext';
import { useTranslation } from 'react-i18next';
import { llmApiService, ChatMessage as APIMessage } from '@/services/chat/llmApi';
import { sqliteService, ChatSession, ChatMessage, RecordedContext, LLMConfig, LLMGlobalSettings } from '@/services/core/sqliteService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import { brokerMCPBridge } from '@/services/mcp/internal/BrokerMCPBridge';
import { notesMCPBridge } from '@/services/mcp/internal/NotesMCPBridge';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';
import { showConfirm, showError } from '@/utils/notifications';

import { QuickPrompt, DEFAULT_QUICK_PROMPTS, MCP_RETRY_INTERVAL_MS, MCP_MAX_RETRIES, CLOCK_UPDATE_INTERVAL_MS, MAX_MESSAGES_IN_MEMORY, DB_TIMEOUT_MS, MCP_SUPPORTED_PROVIDERS, STREAMING_SUPPORTED_PROVIDERS, NO_API_KEY_PROVIDERS } from './components/types';
import { ChatNavbar } from './components/ChatNavbar';
import { SessionsPanel } from './components/SessionsPanel';
import { ChatPanel } from './components/ChatPanel';
import { RightSidePanel } from './components/RightSidePanel';
import { ChatStatusBar } from './components/ChatStatusBar';

// NOTE: All colors should use theme context
// No hardcoded colors - use colors.* from useTerminalTheme()

interface ChatTabProps {
  onNavigateToSettings?: () => void;
  onNavigateToTab?: (tabName: string) => void;
}

const ChatTab: React.FC<ChatTabProps> = ({ onNavigateToSettings, onNavigateToTab }) => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const auth = useAuth();
  const { t } = useTranslation('chat');
  const { activeAdapter, tradingMode, activeBroker } = useBrokerContext();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [currentSessionUuid, setCurrentSessionUuid] = useState<string | null>(null);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [messageInput, setMessageInput] = useState('');
  const [isTyping, setIsTyping] = useState(false);
  const [sessions, setSessions] = useState<ChatSession[]>([]);
  const [systemStatus, setSystemStatus] = useState('STATUS: INITIALIZING...');
  const [streamingContent, setStreamingContent] = useState('');
  const [statistics, setStatistics] = useState({ totalSessions: 0, totalMessages: 0, totalTokens: 0 });

  // Workspace tab state - persist active session across workspace switches
  const getWorkspaceState = useCallback(() => ({
    currentSessionUuid: currentSessionUuid || null,
  }), [currentSessionUuid]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.currentSessionUuid === 'string') setCurrentSessionUuid(state.currentSessionUuid);
  }, []);

  useWorkspaceTabState('chat', getWorkspaceState, setWorkspaceState);
  const [mcpToolsCount, setMcpToolsCount] = useState(0);
  const [linkedContexts, setLinkedContexts] = useState<RecordedContext[]>([]);
  const [activeToolCalls, setActiveToolCalls] = useState<Array<{ name: string; args: any; status: 'calling' | 'success' | 'error' }>>([]);
  const [pendingChartData, setPendingChartData] = useState<any>(null);

  // LLM Configuration state
  const [activeLLMConfig, setActiveLLMConfig] = useState<LLMConfig | null>(null);
  const [allLLMConfigs, setAllLLMConfigs] = useState<LLMConfig[]>([]);
  const [llmGlobalSettings, setLLMGlobalSettings] = useState<LLMGlobalSettings>({
    temperature: 0.7,
    max_tokens: 4096,
    system_prompt: ''
  });
  const [showLLMSelector, setShowLLMSelector] = useState(false);

  // Quick Prompts state
  const [quickPrompts, setQuickPrompts] = useState<QuickPrompt[]>(DEFAULT_QUICK_PROMPTS);
  const [isEditingPrompts, setIsEditingPrompts] = useState(false);
  const [editingPromptIndex, setEditingPromptIndex] = useState<number | null>(null);
  const [editPromptCmd, setEditPromptCmd] = useState('');
  const [editPromptText, setEditPromptText] = useState('');

  const handleContextsChange = (contexts: RecordedContext[]) => {
    setLinkedContexts(contexts);
  };

  // Rename and search functionality
  const [renamingSessionId, setRenamingSessionId] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const [searchQuery, setSearchQuery] = useState('');

  const messagesEndRef = useRef<HTMLDivElement>(null);
  const sessionsListRef = useRef<HTMLDivElement>(null);
  const streamedContentRef = useRef<string>(''); // Track accumulated streaming content
  const retryTimeoutsRef = useRef<Set<NodeJS.Timeout>>(new Set()); // Track pending retry timeouts
  const llmSelectorRef = useRef<HTMLDivElement>(null);
  const mountedRef = useRef(true); // Track component mount state for cleanup

  // Check if current provider supports MCP tools
  const providerSupportsMCP = () => {
    if (!activeLLMConfig) return false;
    return MCP_SUPPORTED_PROVIDERS.includes(activeLLMConfig.provider.toLowerCase());
  };

  // Check if current provider supports streaming
  const providerSupportsStreaming = () => {
    if (!activeLLMConfig) return false;
    return STREAMING_SUPPORTED_PROVIDERS.includes(activeLLMConfig.provider.toLowerCase());
  };

  // Check if current provider requires API key and if it's configured
  const getApiKeyStatus = (): { required: boolean; configured: boolean } => {
    if (!activeLLMConfig) return { required: false, configured: false };
    const provider = activeLLMConfig.provider.toLowerCase();
    const required = !NO_API_KEY_PROVIDERS.includes(provider);
    const configured = !required || (activeLLMConfig.api_key && activeLLMConfig.api_key.trim().length > 0);
    return { required, configured: !!configured };
  };

  // Check if a custom system prompt is configured
  const hasCustomSystemPrompt = (): boolean => {
    return !!(llmGlobalSettings.system_prompt && llmGlobalSettings.system_prompt.trim().length > 0);
  };

  // Check if base URL should be shown (for Ollama or custom endpoints)
  const shouldShowBaseUrl = (): boolean => {
    if (!activeLLMConfig) return false;
    const provider = activeLLMConfig.provider.toLowerCase();
    // Show for Ollama, or if a custom base_url is set
    return provider === 'ollama' || !!(activeLLMConfig.base_url && activeLLMConfig.base_url.trim().length > 0);
  };

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Connect broker to MCP bridge
  useEffect(() => {
    if (activeAdapter) {
      brokerMCPBridge.connect({
        activeAdapter,
        tradingMode,
        activeBroker,
      });
      console.log(`[Chat MCP] Connected to ${activeBroker} in ${tradingMode} mode`);
    }

    return () => {
      brokerMCPBridge.disconnect();
    };
  }, [activeAdapter, tradingMode, activeBroker]);

  // Connect notes to MCP bridge
  useEffect(() => {
    notesMCPBridge.connect();
    return () => {
      notesMCPBridge.disconnect();
    };
  }, []);

  // Initialize database on mount
  useEffect(() => {
    const initDatabase = async () => {
      try {
        setSystemStatus('STATUS: INITIALIZING DATABASE...');

        // Initialize database
        await sqliteService.initialize();

        // Verify database health
        const healthCheck = await sqliteService.healthCheck();
        if (!healthCheck.healthy) {
          throw new Error(healthCheck.message);
        }

        // Ensure default LLM configs exist
        await sqliteService.ensureDefaultLLMConfigs();

        // Load initial data
        await loadSessions();
        await loadStatistics();
        await loadLLMConfiguration();
        await loadMCPToolsCount();

        setSystemStatus('STATUS: READY');
      } catch (error) {
        console.error('Database initialization error:', error);
        const errorMsg = error instanceof Error ? error.message : 'Unknown error';
        setSystemStatus(`ERROR: ${errorMsg}`);
      }
    };

    initDatabase();

    // Note: Don't close database on unmount as it's shared across tabs
    // The database connection is managed as a singleton
  }, []);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, CLOCK_UPDATE_INTERVAL_MS);
    return () => clearInterval(timer);
  }, []);

  // Periodically check for MCP tools updates
  useEffect(() => {
    const mcpCheckInterval = setInterval(() => {
      loadMCPToolsCount();
    }, MCP_RETRY_INTERVAL_MS);

    return () => {
      clearInterval(mcpCheckInterval);
      // Clear all pending retry timeouts on unmount
      retryTimeoutsRef.current.forEach(clearTimeout);
      retryTimeoutsRef.current.clear();
    };
  }, []);

  // Auto-scroll to bottom
  useEffect(() => {
    scrollToBottom();
  }, [messages, streamingContent]);

  // Click outside to close LLM selector
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (llmSelectorRef.current && !llmSelectorRef.current.contains(event.target as Node)) {
        setShowLLMSelector(false);
      }
    };

    if (showLLMSelector) {
      document.addEventListener('mousedown', handleClickOutside);
    }

    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
    };
  }, [showLLMSelector]);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const loadSessions = useCallback(async () => {
    try {
      const loadedSessions = await withTimeout(
        sqliteService.getChatSessions(),
        DB_TIMEOUT_MS,
        'Load sessions timeout'
      );
      if (!mountedRef.current) return;
      setSessions(loadedSessions);
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('Failed to load sessions:', error);
    }
  }, []);

  const loadStatistics = useCallback(async () => {
    try {
      // Optimized: Use SQL aggregation instead of loading all messages
      const stats = await withTimeout(
        sqliteService.getChatStatistics(),
        DB_TIMEOUT_MS,
        'Load statistics timeout'
      );
      if (!mountedRef.current) return;
      setStatistics(stats);
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('[ChatTab] Failed to load statistics:', error);
      // Fallback to basic count
      try {
        const sessions = await sqliteService.getChatSessions(1000);
        if (!mountedRef.current) return;
        setStatistics({
          totalSessions: sessions.length,
          totalMessages: 0,
          totalTokens: 0
        });
      } catch (fallbackError) {
        console.error('[ChatTab] Fallback statistics also failed:', fallbackError);
      }
    }
  }, []);

  const loadLLMConfiguration = useCallback(async () => {
    try {
      const [activeConfig, globalSettings, allConfigs] = await withTimeout(
        Promise.all([
          sqliteService.getActiveLLMConfig(),
          sqliteService.getLLMGlobalSettings(),
          sqliteService.getLLMConfigs()
        ]),
        DB_TIMEOUT_MS,
        'Load LLM configuration timeout'
      );

      if (!mountedRef.current) return;

      setActiveLLMConfig(activeConfig);
      setAllLLMConfigs(allConfigs);
      setLLMGlobalSettings(globalSettings);

      if (!activeConfig) {
        setSystemStatus('WARNING: No active LLM provider configured');
      }
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('[ChatTab] Failed to load LLM configuration:', error);
      setSystemStatus('ERROR: Failed to load LLM configuration');
    }
  }, []);

  // Listen for LLM configuration changes from Settings tab
  useEffect(() => {
    const handleLLMConfigChange = () => {
      console.log('[ChatTab] LLM configuration changed, reloading...');
      loadLLMConfiguration();
    };

    window.addEventListener('llm-config-changed', handleLLMConfigChange);
    return () => {
      window.removeEventListener('llm-config-changed', handleLLMConfigChange);
    };
  }, [loadLLMConfiguration]);

  // Load quick prompts from database
  const loadQuickPrompts = useCallback(async () => {
    try {
      const savedPrompts = await sqliteService.getSetting('chat_quick_prompts');
      if (savedPrompts) {
        const parsed = JSON.parse(savedPrompts);
        if (Array.isArray(parsed) && parsed.length > 0) {
          setQuickPrompts(parsed);
        }
      }
    } catch (error) {
      console.error('[ChatTab] Failed to load quick prompts:', error);
      // Keep default prompts on error
    }
  }, []);

  // Save quick prompts to database
  const saveQuickPrompts = useCallback(async (prompts: QuickPrompt[]) => {
    try {
      await sqliteService.saveSetting('chat_quick_prompts', JSON.stringify(prompts), 'chat');
      setQuickPrompts(prompts);
    } catch (error) {
      console.error('[ChatTab] Failed to save quick prompts:', error);
    }
  }, []);

  // Quick prompt editing handlers
  const startEditingPrompt = (index: number) => {
    setEditingPromptIndex(index);
    setEditPromptCmd(quickPrompts[index].cmd);
    setEditPromptText(quickPrompts[index].prompt);
    setIsEditingPrompts(true);
  };

  const saveEditedPrompt = async () => {
    if (editingPromptIndex === null || !editPromptCmd.trim() || !editPromptText.trim()) return;

    const newPrompts = [...quickPrompts];
    newPrompts[editingPromptIndex] = {
      cmd: editPromptCmd.trim().toUpperCase(),
      prompt: editPromptText.trim()
    };
    await saveQuickPrompts(newPrompts);
    setEditingPromptIndex(null);
    setIsEditingPrompts(false);
  };

  const cancelEditingPrompt = () => {
    setEditingPromptIndex(null);
    setIsEditingPrompts(false);
    setEditPromptCmd('');
    setEditPromptText('');
  };

  const resetQuickPrompts = async () => {
    await saveQuickPrompts(DEFAULT_QUICK_PROMPTS);
  };

  // Load quick prompts on mount
  useEffect(() => {
    loadQuickPrompts();
  }, [loadQuickPrompts]);

  const switchLLMProvider = useCallback(async (provider: string) => {
    try {
      setSystemStatus('STATUS: SWITCHING LLM PROVIDER...');
      await withTimeout(
        sqliteService.setActiveLLMProvider(provider),
        DB_TIMEOUT_MS,
        'Switch provider timeout'
      );
      if (!mountedRef.current) return;
      await loadLLMConfiguration();
      if (!mountedRef.current) return;
      setShowLLMSelector(false);
      setSystemStatus('STATUS: READY');
      console.log(`[ChatTab] Switched to LLM provider: ${provider}`);
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('[ChatTab] Failed to switch LLM provider:', error);
      setSystemStatus('ERROR: Failed to switch provider');
    }
  }, [loadLLMConfiguration]);

  const loadMCPToolsCount = async (retryCount = 0, maxRetries = MCP_MAX_RETRIES) => {
    try {
      const { mcpToolService } = await import('@/services/mcp/mcpToolService');
      const tools = await mcpToolService.getAllTools();
      setMcpToolsCount(tools.length);

      if (tools.length === 0 && retryCount < maxRetries) {
        const delay = Math.min(1000 * Math.pow(2, retryCount), MCP_RETRY_INTERVAL_MS);
        const timeoutId = setTimeout(() => {
          retryTimeoutsRef.current.delete(timeoutId);
          loadMCPToolsCount(retryCount + 1, maxRetries);
        }, delay);
        retryTimeoutsRef.current.add(timeoutId);
      }
    } catch (error) {
      setMcpToolsCount(0);
    }
  };

  const loadSessionMessages = async (sessionUuid: string) => {
    try {
      setSystemStatus('STATUS: LOADING MESSAGES...');
      const loadedMessages = await sqliteService.getChatMessages(sessionUuid);
      setMessages(loadedMessages.slice(-MAX_MESSAGES_IN_MEMORY));
      setSystemStatus('STATUS: READY');
    } catch (error) {
      console.error('[ChatTab] Failed to load messages:', error);
      setSystemStatus('ERROR: Failed to load messages');
    }
  };

  const generateSmartTitle = (message: string): string => {
    // Clean the message but keep important characters
    const trimmed = message.trim();

    if (!trimmed) {
      const now = new Date();
      return `Chat ${now.getHours()}:${now.getMinutes().toString().padStart(2, '0')}`;
    }

    // Extract keywords from common patterns
    const lowerMsg = trimmed.toLowerCase();

    // Check for common financial terms
    const financialKeywords = [
      'market', 'stock', 'portfolio', 'risk', 'analysis',
      'trend', 'trading', 'investment', 'price', 'strategy',
      'forecast', 'data', 'chart', 'indicator', 'technical'
    ];

    const foundKeyword = financialKeywords.find(kw => lowerMsg.includes(kw));
    if (foundKeyword) {
      return foundKeyword.charAt(0).toUpperCase() + foundKeyword.slice(1) + ' Chat';
    }

    // Check for question words
    if (lowerMsg.startsWith('what')) return 'What Query';
    if (lowerMsg.startsWith('how')) return 'How To';
    if (lowerMsg.startsWith('why')) return 'Why Question';
    if (lowerMsg.startsWith('when')) return 'When Query';
    if (lowerMsg.startsWith('where')) return 'Where Query';
    if (lowerMsg.startsWith('analyze')) return 'Analysis';
    if (lowerMsg.startsWith('explain')) return 'Explanation';
    if (lowerMsg.startsWith('compare')) return 'Comparison';
    if (lowerMsg.startsWith('calculate')) return 'Calculation';
    if (lowerMsg.startsWith('show') || lowerMsg.startsWith('get')) return 'Data Request';

    // Extract first 2-3 meaningful words (max 20 chars)
    const words = trimmed.split(/\s+/).filter(w => w.length > 2);
    if (words.length === 0) {
      const now = new Date();
      return `Chat ${now.getHours()}:${now.getMinutes().toString().padStart(2, '0')}`;
    }

    let title = words.slice(0, 2).join(' ');
    if (title.length > 20) {
      title = title.substring(0, 20);
    }

    return title;
  };

  const createNewSession = useCallback(async (): Promise<string | null> => {
    try {
      const title = messageInput.trim() ?
        generateSmartTitle(messageInput) :
        `Chat ${new Date().toLocaleString()}`;

      const newSession = await withTimeout(
        sqliteService.createChatSession(title),
        DB_TIMEOUT_MS,
        'Create session timeout'
      );
      if (!mountedRef.current) return null;
      setSessions(prev => [newSession, ...prev]);
      setCurrentSessionUuid(newSession.session_uuid);
      setMessages([]);
      await loadStatistics();
      if (!mountedRef.current) return null;
      setSystemStatus('STATUS: NEW SESSION CREATED');
      return newSession.session_uuid;
    } catch (error) {
      if (!mountedRef.current) return null;
      console.error('Failed to create session:', error);
      setSystemStatus('ERROR: Failed to create session');
      return null;
    }
  }, [messageInput, loadStatistics]);

  const selectSession = useCallback(async (sessionUuid: string) => {
    setCurrentSessionUuid(sessionUuid);
    await loadSessionMessages(sessionUuid);
  }, []);

  const deleteSession = useCallback(async (sessionUuid: string) => {
    try {
      await withTimeout(
        sqliteService.deleteChatSession(sessionUuid),
        DB_TIMEOUT_MS,
        'Delete session timeout'
      );
      if (!mountedRef.current) return;
      setSessions(prev => prev.filter(s => s.session_uuid !== sessionUuid));
      if (currentSessionUuid === sessionUuid) {
        setCurrentSessionUuid(null);
        setMessages([]);
      }
      await loadStatistics();
      if (!mountedRef.current) return;
      setSystemStatus('STATUS: SESSION DELETED');
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('Failed to delete session:', error);
      setSystemStatus('ERROR: Failed to delete session');
    }
  }, [currentSessionUuid, loadStatistics]);

  const startRenaming = useCallback((sessionUuid: string, currentTitle: string) => {
    setRenamingSessionId(sessionUuid);
    setRenameValue(currentTitle);
  }, []);

  const saveRename = useCallback(async (sessionUuid: string) => {
    const sanitizedName = sanitizeInput(renameValue);
    if (!sanitizedName.trim()) {
      setRenamingSessionId(null);
      return;
    }

    try {
      await withTimeout(
        sqliteService.updateChatSessionTitle(sessionUuid, sanitizedName.trim()),
        DB_TIMEOUT_MS,
        'Rename session timeout'
      );
      if (!mountedRef.current) return;
      setSessions(prev => prev.map(s =>
        s.session_uuid === sessionUuid ? { ...s, title: sanitizedName.trim() } : s
      ));
      setRenamingSessionId(null);
      setSystemStatus('STATUS: SESSION RENAMED');
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('Failed to rename session:', error);
      setSystemStatus('ERROR: Failed to rename session');
    }
  }, [renameValue]);

  const cancelRename = useCallback(() => {
    setRenamingSessionId(null);
    setRenameValue('');
  }, []);

  const clearCurrentChat = useCallback(async () => {
    if (!currentSessionUuid) return;

    try {
      await withTimeout(
        sqliteService.clearChatSessionMessages(currentSessionUuid),
        DB_TIMEOUT_MS,
        'Clear chat timeout'
      );
      if (!mountedRef.current) return;
      setMessages([]);
      await loadSessions();
      await loadStatistics();
      if (!mountedRef.current) return;
      setSystemStatus('STATUS: CHAT CLEARED');
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('Failed to clear chat:', error);
      setSystemStatus('ERROR: Failed to clear chat');
    }
  }, [currentSessionUuid, loadSessions, loadStatistics]);

  const deleteAllSessions = useCallback(async () => {
    const confirmed = await showConfirm('This action cannot be undone.', {
      title: 'Delete ALL sessions and messages?',
      type: 'danger'
    });
    if (!confirmed) {
      return;
    }

    try {
      for (const session of sessions) {
        await sqliteService.deleteChatSession(session.session_uuid);
        if (!mountedRef.current) return;
      }
      setSessions([]);
      setMessages([]);
      setCurrentSessionUuid(null);
      await loadStatistics();
      if (!mountedRef.current) return;
      setSystemStatus('STATUS: ALL SESSIONS DELETED');
    } catch (error) {
      if (!mountedRef.current) return;
      console.error('Failed to delete all sessions:', error);
      setSystemStatus('ERROR: Failed to delete all sessions');
    }
  }, [sessions, loadStatistics]);

  // Helper functions for handleSendMessage
  const validateLLMConfiguration = (): { valid: boolean; error?: string } => {
    if (!activeLLMConfig) {
      return {
        valid: false,
        error: 'No LLM provider is configured.\n\nPlease go to Settings → LLM Configuration to set up a provider.'
      };
    }

    // Providers that don't require API key
    const noApiKeyProviders = ['ollama', 'fincept'];
    const requiresApiKey = !noApiKeyProviders.includes(activeLLMConfig.provider.toLowerCase());

    if (requiresApiKey && !activeLLMConfig.api_key) {
      const providerName = activeLLMConfig.provider.charAt(0).toUpperCase() + activeLLMConfig.provider.slice(1);
      return {
        valid: false,
        error: `${providerName} requires an API key.\n\nPlease go to Settings → LLM Configuration to add your ${providerName} API key.`
      };
    }

    return { valid: true };
  };

  const ensureSessionExists = async (): Promise<string | null> => {
    if (currentSessionUuid) {
      return currentSessionUuid;
    }

    // createNewSession now returns the UUID directly, no need to wait for state update
    const newSessionUuid = await createNewSession();
    return newSessionUuid;
  };

  const buildConversationHistory = (): APIMessage[] => {
    return messages.map(msg => ({
      role: msg.role as 'system' | 'user' | 'assistant',
      content: msg.content
    }));
  };

  const addContextToConversation = async (conversationHistory: APIMessage[]): Promise<void> => {
    if (linkedContexts.length === 0) {
      console.log('[ChatTab] No active contexts linked');
      return;
    }

    console.log('[ChatTab] Active contexts linked:', linkedContexts.length);
    const contextIds = linkedContexts.map(ctx => ctx.id);
    const contextData = await contextRecorderService.formatMultipleContexts(contextIds, 'markdown');

    const systemMessage: APIMessage = {
      role: 'system' as const,
      content: `The following recorded data contexts are provided for your reference:\n\n${contextData}\n\n---\n\nPlease use this information to help answer the user's questions.`
    };

    conversationHistory.unshift(systemMessage);
    console.log('[ChatTab] System message with context added to conversation');
  };

  const getMCPTools = async (): Promise<any[]> => {
    try {
      const { mcpToolService } = await import('@/services/mcp/mcpToolService');
      return await mcpToolService.getAllTools();
    } catch (error) {
      return [];
    }
  };

  const handleStreamingCallback = (chunk: string, done: boolean) => {
    if (!done) {
      streamedContentRef.current += chunk;
      flushSync(() => {
        setStreamingContent(prev => prev + chunk);
      });
    } else {
      flushSync(() => {
        setIsTyping(false);
        setStreamingContent('');
        setSystemStatus('STATUS: READY');
      });
    }
  };

  const handleToolCallback = (toolName: string, args: any, result?: any) => {
    if (!result) {
      // Tool call started
      console.log(`[Tool Call] ${toolName}`, args);
      setSystemStatus(`STATUS: Calling ${toolName}...`);
      setActiveToolCalls(prev => [...prev, { name: toolName, args, status: 'calling' }]);
    } else {
      // Tool call completed
      console.log(`[Tool Result] ${toolName}`, result);
      setSystemStatus('STATUS: Processing response...');
      setActiveToolCalls(prev =>
        prev.map(call =>
          call.name === toolName && call.status === 'calling'
            ? { ...call, status: result.error ? 'error' : 'success' }
            : call
        )
      );

      // Store chart data for financial tools
      if (result.chart_data) {
        console.log('[ChatTab] Chart data received:', result.chart_data);
        setPendingChartData({
          data: result.chart_data,
          ticker: result.ticker,
          company: result.company
        });
      }

      // Check if a report was created and schedule navigation after completion
      if (toolName === 'create_report_template' && result?.id) {
        (window as any).__navigateToReportBuilder = true;
      }
    }
  };

  const saveAssistantMessage = async (sessionUuid: string, content: string, usage?: any): Promise<void> => {
    if (!activeLLMConfig) return;

    // Save message with chart data if available
    const chartData = pendingChartData;

    const aiMessage = await sqliteService.addChatMessage({
      session_uuid: sessionUuid,
      role: 'assistant',
      content,
      provider: activeLLMConfig.provider,
      model: activeLLMConfig.model,
      tokens_used: usage?.totalTokens,
      tool_calls: activeToolCalls.length > 0 ? activeToolCalls.map(call => ({
        name: call.name,
        args: call.args,
        timestamp: new Date().toISOString(),
        status: call.status
      })) : undefined,
      metadata: chartData ? { chart_data: chartData.data, ticker: chartData.ticker, company: chartData.company } : undefined
    });

    setMessages(prev => [...prev, aiMessage]);
    setActiveToolCalls([]);
    setPendingChartData(null);
  };

  const saveErrorMessage = async (sessionUuid: string, error: string): Promise<void> => {
    const errorMessage = await sqliteService.addChatMessage({
      session_uuid: sessionUuid,
      role: 'assistant',
      content: `[ERROR] ${error}\n\nPlease check your LLM configuration and try again.`
    });

    setMessages(prev => [...prev, errorMessage]);
  };

  const handleSendMessage = useCallback(async () => {
    // Sanitize user input
    const sanitizedInput = sanitizeInput(messageInput);
    if (!sanitizedInput.trim()) return;

    // Validate LLM configuration
    const validation = validateLLMConfiguration();
    if (!validation.valid) {
      setSystemStatus('ERROR: Invalid LLM configuration');
      showError('Invalid LLM configuration', [
        { label: 'ERROR', value: validation.error || 'Unknown error' }
      ]);
      onNavigateToSettings?.();
      return;
    }

    // Ensure session exists
    const sessionUuid = await ensureSessionExists();
    if (!sessionUuid) {
      setSystemStatus('ERROR: Failed to create session');
      return;
    }

    const userContent = sanitizedInput.trim();
    setMessageInput('');
    setIsTyping(true);
    setStreamingContent('');
    setSystemStatus(`STATUS: CALLING ${activeLLMConfig!.provider.toUpperCase()} API...`);

    try {
      // Save user message
      const userMessage = await sqliteService.addChatMessage({
        session_uuid: sessionUuid,
        role: 'user',
        content: userContent
      });
      setMessages(prev => [...prev, userMessage]);

      // Build conversation history
      const conversationHistory = buildConversationHistory();

      // Add linked contexts if any
      await addContextToConversation(conversationHistory);

      // Get MCP tools
      const mcpTools = await getMCPTools();
      console.log(`[Chat] MCP Tools available: ${mcpTools.length}`, mcpTools.map(t => t.name));

      // Reset streaming reference
      streamedContentRef.current = '';

      // Call LLM API
      const response = mcpTools.length > 0
        ? await llmApiService.chatWithTools(
          userContent,
          conversationHistory,
          mcpTools,
          handleStreamingCallback,
          handleToolCallback
        )
        : await llmApiService.chat(
          userContent,
          conversationHistory,
          handleStreamingCallback
        );

      // Handle response
      if (response.error) {
        await saveErrorMessage(sessionUuid, response.error);
        setSystemStatus(`ERROR: ${response.error}`);
      } else {
        const contentToSave = response.content || streamedContentRef.current || '(No response generated)';
        await saveAssistantMessage(sessionUuid, contentToSave, response.usage);
        streamedContentRef.current = '';
      }

      // Update UI state
      setIsTyping(false);
      setStreamingContent('');

      // Reload data
      await Promise.all([
        loadSessions(),
        loadStatistics(),
        auth.refreshUserData()
      ]);

      // Check if we should navigate to report builder (after everything is done)
      if ((window as any).__navigateToReportBuilder && onNavigateToTab) {
        (window as any).__navigateToReportBuilder = false;
        // Small delay to ensure message is saved and UI is stable
        setTimeout(() => {
          onNavigateToTab('report-builder');
        }, 500);
      }

    } catch (error) {
      if (!mountedRef.current) return;
      console.error('[ChatTab] Send message error:', error);
      await saveErrorMessage(
        sessionUuid,
        `Unexpected error: ${error instanceof Error ? error.message : 'Unknown error'}`
      );
      setIsTyping(false);
      setStreamingContent('');
      setSystemStatus('ERROR: Request failed');
    }
  }, [messageInput, activeLLMConfig, onNavigateToSettings, onNavigateToTab, loadSessions, loadStatistics, auth]);

  // Filter sessions by search query (searches in both title and message content)
  const filteredSessions = searchQuery
    ? sessions.filter(session =>
      session.title.toLowerCase().includes(searchQuery.toLowerCase())
    )
    : sessions;

  const formatTime = (date: Date | string | undefined) => {
    if (!date) return '00:00:00';
    try {
      const d = typeof date === 'string' ? new Date(date) : date;
      if (isNaN(d.getTime())) return '00:00:00';
      return d.toLocaleTimeString('en-US', {
        hour12: false,
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
      });
    } catch (error) {
      console.error('Error formatting time:', error, date);
      return '00:00:00';
    }
  };

  const formatSessionTime = (dateString: string | undefined) => {
    if (!dateString) return '00:00';
    try {
      const date = new Date(dateString);
      if (isNaN(date.getTime())) return '00:00';

      const now = new Date();
      const diffHours = Math.floor((now.getTime() - date.getTime()) / (1000 * 60 * 60));

      if (diffHours < 24) {
        return date.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit' });
      } else if (diffHours < 168) { // Less than a week
        return date.toLocaleDateString('en-US', { weekday: 'short', hour: '2-digit', minute: '2-digit' });
      } else {
        return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
      }
    } catch (error) {
      console.error('Error formatting session time:', error, dateString);
      return '00:00';
    }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        /* Scrollbar styling per design system */
        *::-webkit-scrollbar {
          width: 6px;
          height: 6px;
        }
        *::-webkit-scrollbar-track {
          background: ${colors.background};
        }
        *::-webkit-scrollbar-thumb {
          background: ${'rgba(255,255,255,0.1)'};
          border-radius: 3px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: ${colors.textMuted};
        }
        /* For Firefox */
        * {
          scrollbar-width: thin;
          scrollbar-color: ${'rgba(255,255,255,0.1)'} ${colors.background};
        }
        /* Input focus states */
        input:focus, textarea:focus {
          outline: none;
          border-color: ${colors.primary} !important;
        }
        /* Blink animation for cursor */
        @keyframes blink {
          0%, 50% { opacity: 1; }
          51%, 100% { opacity: 0; }
        }
        @keyframes pulse {
          0%, 100% { opacity: 0.6; transform: scale(1); }
          50% { opacity: 1; transform: scale(1.02); }
        }
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        @keyframes fadeInOut {
          0%, 100% { opacity: 0.5; }
          50% { opacity: 1; }
        }
        @keyframes dots {
          0%, 20% { content: '.'; }
          40% { content: '..'; }
          60%, 100% { content: '...'; }
        }
      `}</style>

      {/* Top Navigation Bar */}
      <ChatNavbar
        colors={colors}
        fontSize={fontSize}
        currentTime={currentTime}
        activeLLMConfig={activeLLMConfig}
        allLLMConfigs={allLLMConfigs}
        showLLMSelector={showLLMSelector}
        setShowLLMSelector={setShowLLMSelector}
        llmSelectorRef={llmSelectorRef}
        t={t}
        onSwitchLLMProvider={switchLLMProvider}
        onNavigateToSettings={onNavigateToSettings}
        onCreateNewSession={createNewSession}
      />

      {/* Main Content */}
      <ErrorBoundary name="ChatMainContent" variant="minimal">
        <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
          {/* Left Panel - Sessions */}
          <SessionsPanel
            colors={colors}
            fontSize={fontSize}
            sessions={sessions}
            filteredSessions={filteredSessions}
            currentSessionUuid={currentSessionUuid}
            renamingSessionId={renamingSessionId}
            renameValue={renameValue}
            searchQuery={searchQuery}
            statistics={statistics}
            sessionsListRef={sessionsListRef}
            t={t}
            onSelectSession={selectSession}
            onDeleteSession={deleteSession}
            onDeleteAllSessions={deleteAllSessions}
            onStartRenaming={startRenaming}
            onSaveRename={saveRename}
            onCancelRename={cancelRename}
            onRenameValueChange={setRenameValue}
            onSearchChange={setSearchQuery}
            formatSessionTime={formatSessionTime}
          />

          {/* Center Panel - Chat */}
          <ChatPanel
            colors={colors}
            fontSize={fontSize}
            messages={messages}
            streamingContent={streamingContent}
            isTyping={isTyping}
            messageInput={messageInput}
            currentSessionUuid={currentSessionUuid}
            sessions={sessions}
            activeLLMConfig={activeLLMConfig}
            systemStatus={systemStatus}
            activeToolCalls={activeToolCalls}
            messagesEndRef={messagesEndRef}
            t={t}
            formatTime={formatTime}
            onMessageInputChange={setMessageInput}
            onSendMessage={handleSendMessage}
            onClearCurrentChat={clearCurrentChat}
            onCreateNewSession={createNewSession}
          />

          {/* Right Panel - Quick Actions & Context Selector */}
          <RightSidePanel
            colors={colors}
            fontSize={fontSize}
            activeLLMConfig={activeLLMConfig}
            llmGlobalSettings={llmGlobalSettings}
            mcpToolsCount={mcpToolsCount}
            currentSessionUuid={currentSessionUuid}
            quickPrompts={quickPrompts}
            isEditingPrompts={isEditingPrompts}
            editingPromptIndex={editingPromptIndex}
            editPromptCmd={editPromptCmd}
            editPromptText={editPromptText}
            t={t}
            providerSupportsMCP={providerSupportsMCP}
            providerSupportsStreaming={providerSupportsStreaming}
            getApiKeyStatus={getApiKeyStatus}
            hasCustomSystemPrompt={hasCustomSystemPrompt}
            shouldShowBaseUrl={shouldShowBaseUrl}
            onContextsChange={handleContextsChange}
            onSetMessageInput={setMessageInput}
            onStartEditingPrompt={startEditingPrompt}
            onSaveEditedPrompt={saveEditedPrompt}
            onCancelEditingPrompt={cancelEditingPrompt}
            onResetQuickPrompts={resetQuickPrompts}
            onEditPromptCmdChange={setEditPromptCmd}
            onEditPromptTextChange={setEditPromptText}
            onNavigateToTab={onNavigateToTab}
          />
        </div>
      </ErrorBoundary>

      {/* Status Bar (Bottom) */}
      <ChatStatusBar
        colors={colors}
        fontSize={fontSize}
        activeLLMConfig={activeLLMConfig}
        currentSessionUuid={currentSessionUuid}
        messagesCount={messages.length}
        streamingContent={streamingContent}
      />
    </div>
  );
};

export default ChatTab;
