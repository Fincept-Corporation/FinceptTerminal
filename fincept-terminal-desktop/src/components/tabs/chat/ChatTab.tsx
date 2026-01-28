import React, { useState, useEffect, useRef } from 'react';
import { flushSync } from 'react-dom';
import { Settings, Trash2, Bot, User, Clock, Send, Plus, Search, Edit2, Check, X } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useAuth } from '@/contexts/AuthContext';
import { useBrokerContext } from '@/contexts/BrokerContext';
import { useTranslation } from 'react-i18next';
import { llmApiService, ChatMessage as APIMessage } from '@/services/chat/llmApi';
import { sqliteService, ChatSession, ChatMessage, RecordedContext, LLMConfig, LLMGlobalSettings, ChatStatistics } from '@/services/core/sqliteService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import { brokerMCPBridge } from '@/services/mcp/internal/BrokerMCPBridge';
import { notesMCPBridge } from '@/services/mcp/internal/NotesMCPBridge';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import ContextSelector from '@/components/common/ContextSelector';

// Fincept Terminal Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

interface ChatTabProps {
  onNavigateToSettings?: () => void;
  onNavigateToTab?: (tabName: string) => void;
}

// Constants
const SESSION_CREATION_DELAY_MS = 100;
const MAX_MESSAGES_IN_MEMORY = 200;
const MCP_RETRY_INTERVAL_MS = 10000;
const MCP_MAX_RETRIES = 5;
const CLOCK_UPDATE_INTERVAL_MS = 1000;
const MCP_SUPPORTED_PROVIDERS = ['openai', 'anthropic', 'gemini', 'google', 'groq', 'deepseek', 'openrouter', 'fincept'];

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
  const [mcpToolsCount, setMcpToolsCount] = useState(0);
  const [linkedContexts, setLinkedContexts] = useState<RecordedContext[]>([]);

  // LLM Configuration state
  const [activeLLMConfig, setActiveLLMConfig] = useState<LLMConfig | null>(null);
  const [llmGlobalSettings, setLLMGlobalSettings] = useState<LLMGlobalSettings>({
    temperature: 0.7,
    max_tokens: 4096,
    system_prompt: ''
  });

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

  // Check if current provider supports MCP tools
  const providerSupportsMCP = () => {
    if (!activeLLMConfig) return false;
    return MCP_SUPPORTED_PROVIDERS.includes(activeLLMConfig.provider.toLowerCase());
  };

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

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const loadSessions = async () => {
    try {
      const loadedSessions = await sqliteService.getChatSessions();
      setSessions(loadedSessions);
    } catch (error) {
      console.error('Failed to load sessions:', error);
    }
  };

  const loadStatistics = async () => {
    try {
      // Optimized: Use SQL aggregation instead of loading all messages
      const stats = await sqliteService.getChatStatistics();
      setStatistics(stats);
    } catch (error) {
      console.error('[ChatTab] Failed to load statistics:', error);
      // Fallback to basic count
      const sessions = await sqliteService.getChatSessions(1000);
      setStatistics({
        totalSessions: sessions.length,
        totalMessages: 0,
        totalTokens: 0
      });
    }
  };

  const loadLLMConfiguration = async () => {
    try {
      const [activeConfig, globalSettings] = await Promise.all([
        sqliteService.getActiveLLMConfig(),
        sqliteService.getLLMGlobalSettings()
      ]);

      setActiveLLMConfig(activeConfig);
      setLLMGlobalSettings(globalSettings);

      if (!activeConfig) {
        setSystemStatus('WARNING: No active LLM provider configured');
      }
    } catch (error) {
      console.error('[ChatTab] Failed to load LLM configuration:', error);
      setSystemStatus('ERROR: Failed to load LLM configuration');
    }
  };

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

  const createNewSession = async (): Promise<string | null> => {
    try {
      const title = messageInput.trim() ?
        generateSmartTitle(messageInput) :
        `Chat ${new Date().toLocaleString()}`;

      const newSession = await sqliteService.createChatSession(title);
      setSessions(prev => [newSession, ...prev]);
      setCurrentSessionUuid(newSession.session_uuid);
      setMessages([]);
      await loadStatistics();
      setSystemStatus('STATUS: NEW SESSION CREATED');
      return newSession.session_uuid;
    } catch (error) {
      console.error('Failed to create session:', error);
      setSystemStatus('ERROR: Failed to create session');
      return null;
    }
  };

  const selectSession = async (sessionUuid: string) => {
    setCurrentSessionUuid(sessionUuid);
    await loadSessionMessages(sessionUuid);
  };

  const deleteSession = async (sessionUuid: string) => {
    try {
      await sqliteService.deleteChatSession(sessionUuid);
      setSessions(prev => prev.filter(s => s.session_uuid !== sessionUuid));
      if (currentSessionUuid === sessionUuid) {
        setCurrentSessionUuid(null);
        setMessages([]);
      }
      await loadStatistics();
      setSystemStatus('STATUS: SESSION DELETED');
    } catch (error) {
      console.error('Failed to delete session:', error);
      setSystemStatus('ERROR: Failed to delete session');
    }
  };

  const startRenaming = (sessionUuid: string, currentTitle: string) => {
    setRenamingSessionId(sessionUuid);
    setRenameValue(currentTitle);
  };

  const saveRename = async (sessionUuid: string) => {
    if (!renameValue.trim()) {
      setRenamingSessionId(null);
      return;
    }

    try {
      await sqliteService.updateChatSessionTitle(sessionUuid, renameValue.trim());
      setSessions(prev => prev.map(s =>
        s.session_uuid === sessionUuid ? { ...s, title: renameValue.trim() } : s
      ));
      setRenamingSessionId(null);
      setSystemStatus('STATUS: SESSION RENAMED');
    } catch (error) {
      console.error('Failed to rename session:', error);
      setSystemStatus('ERROR: Failed to rename session');
    }
  };

  const cancelRename = () => {
    setRenamingSessionId(null);
    setRenameValue('');
  };

  const clearCurrentChat = async () => {
    if (!currentSessionUuid) return;

    try {
      await sqliteService.clearChatSessionMessages(currentSessionUuid);
      setMessages([]);
      await loadSessions();
      await loadStatistics();
      setSystemStatus('STATUS: CHAT CLEARED');
    } catch (error) {
      console.error('Failed to clear chat:', error);
      setSystemStatus('ERROR: Failed to clear chat');
    }
  };

  const deleteAllSessions = async () => {
    if (!confirm('Delete ALL sessions and messages? This cannot be undone!')) {
      return;
    }

    try {
      for (const session of sessions) {
        await sqliteService.deleteChatSession(session.session_uuid);
      }
      setSessions([]);
      setMessages([]);
      setCurrentSessionUuid(null);
      await loadStatistics();
      setSystemStatus('STATUS: ALL SESSIONS DELETED');
    } catch (error) {
      console.error('Failed to delete all sessions:', error);
      setSystemStatus('ERROR: Failed to delete all sessions');
    }
  };

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
      console.log(`[Tool Call] ${toolName}`, args);
      setSystemStatus('STATUS: Fetching data...');
    } else {
      console.log(`[Tool Result] ${toolName}`, result);
      setSystemStatus('STATUS: Processing response...');

      // Check if a report was created and schedule navigation after completion
      if (toolName === 'create_report_template' && result?.id) {
        // Store the flag to navigate after response is complete
        (window as any).__navigateToReportBuilder = true;
      }
    }
  };

  const saveAssistantMessage = async (sessionUuid: string, content: string, usage?: any): Promise<void> => {
    if (!activeLLMConfig) return;

    const aiMessage = await sqliteService.addChatMessage({
      session_uuid: sessionUuid,
      role: 'assistant',
      content,
      provider: activeLLMConfig.provider,
      model: activeLLMConfig.model,
      tokens_used: usage?.totalTokens
    });

    setMessages(prev => [...prev, aiMessage]);
  };

  const saveErrorMessage = async (sessionUuid: string, error: string): Promise<void> => {
    const errorMessage = await sqliteService.addChatMessage({
      session_uuid: sessionUuid,
      role: 'assistant',
      content: `[ERROR] ${error}\n\nPlease check your LLM configuration and try again.`
    });

    setMessages(prev => [...prev, errorMessage]);
  };

  const handleSendMessage = async () => {
    if (!messageInput.trim()) return;

    // Validate LLM configuration
    const validation = validateLLMConfiguration();
    if (!validation.valid) {
      setSystemStatus('ERROR: Invalid LLM configuration');
      alert(validation.error);
      onNavigateToSettings?.();
      return;
    }

    // Ensure session exists
    const sessionUuid = await ensureSessionExists();
    if (!sessionUuid) {
      setSystemStatus('ERROR: Failed to create session');
      return;
    }

    const userContent = messageInput.trim();
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
      console.error('[ChatTab] Send message error:', error);
      await saveErrorMessage(
        sessionUuid,
        `Unexpected error: ${error instanceof Error ? error.message : 'Unknown error'}`
      );
      setIsTyping(false);
      setStreamingContent('');
      setSystemStatus('ERROR: Request failed');
    }
  };

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

  const renderWelcomeScreen = () => (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG
    }}>
      <div style={{
        textAlign: 'center',
        padding: '24px',
        maxWidth: '400px'
      }}>
        <Bot size={48} color={FINCEPT.ORANGE} style={{ marginBottom: '16px', opacity: 0.8 }} />
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: '16px',
          fontWeight: 700,
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          {t('title').toUpperCase()}
        </div>
        <div style={{
          color: FINCEPT.WHITE,
          fontSize: '11px',
          marginBottom: '16px',
          lineHeight: '1.5'
        }}>
          {t('subtitle')}
        </div>
        <div style={{
          padding: '6px 12px',
          backgroundColor: `${FINCEPT.YELLOW}20`,
          color: FINCEPT.YELLOW,
          fontSize: '9px',
          fontWeight: 700,
          borderRadius: '2px',
          marginBottom: '16px',
          display: 'inline-block',
          letterSpacing: '0.5px'
        }}>
          PROVIDER: {activeLLMConfig?.provider.toUpperCase() || 'NONE'} | MODEL: {activeLLMConfig?.model || 'N/A'}
        </div>
        <br />
        <button
          onClick={createNewSession}
          style={{
            padding: '8px 16px',
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            fontSize: '9px',
            fontWeight: 700,
            borderRadius: '2px',
            cursor: 'pointer',
            letterSpacing: '0.5px',
            transition: 'all 0.2s'
          }}
        >
          {t('header.newChat').toUpperCase()}
        </button>
      </div>
    </div>
  );

  const renderMessage = (message: ChatMessage) => {
    try {
      return (
        <div key={message.id} style={{ marginBottom: '12px' }}>
          <div style={{
            display: 'flex',
            justifyContent: message.role === 'user' ? 'flex-end' : 'flex-start',
            marginBottom: '4px'
          }}>
            <div style={{
              maxWidth: '85%',
              minWidth: '120px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${message.role === 'user' ? FINCEPT.YELLOW : FINCEPT.ORANGE}`,
              borderRadius: '2px',
              padding: '10px'
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                marginBottom: '8px',
                paddingBottom: '6px',
                borderBottom: `1px solid ${FINCEPT.BORDER}`
              }}>
                {message.role === 'user' ? (
                  <User size={12} color={FINCEPT.YELLOW} />
                ) : (
                  <Bot size={12} color={FINCEPT.ORANGE} />
                )}
                <span style={{
                  color: message.role === 'user' ? FINCEPT.YELLOW : FINCEPT.ORANGE,
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px'
                }}>
                  {message.role === 'user' ? 'YOU' : 'AI'}
                </span>
                <Clock size={10} color={FINCEPT.GRAY} />
                <span style={{
                  color: FINCEPT.GRAY,
                  fontSize: '9px',
                  letterSpacing: '0.5px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                }}>
                  {formatTime(message.timestamp)}
                </span>
                {message.provider && (
                  <span style={{
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    letterSpacing: '0.5px'
                  }}>
                    | {message.provider.toUpperCase()}
                  </span>
                )}
              </div>
              {message.role === 'assistant' && message.content ? (
                <MarkdownRenderer content={message.content} />
              ) : (
                <div style={{
                  color: FINCEPT.WHITE,
                  fontSize: '11px',
                  lineHeight: '1.6',
                  whiteSpace: 'pre-wrap',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                }}>
                  {message.content || '(Empty message)'}
                </div>
              )}
            </div>
          </div>
        </div>
      );
    } catch (error) {
      console.error('Error rendering message:', error, message);
      return (
        <div key={message.id} style={{
          marginBottom: '12px',
          color: FINCEPT.RED,
          fontSize: '10px',
          letterSpacing: '0.5px'
        }}>
          ERROR RENDERING MESSAGE
        </div>
      );
    }
  };

  const renderStreamingMessage = () => {
    if (!streamingContent) return null;

    return (
      <div style={{ marginBottom: '12px', display: 'flex', justifyContent: 'flex-start' }}>
        <div style={{
          maxWidth: '85%',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${FINCEPT.ORANGE}`,
          borderRadius: '2px',
          padding: '10px'
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            marginBottom: '8px',
            paddingBottom: '6px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <Bot size={12} color={FINCEPT.ORANGE} />
            <span style={{
              color: FINCEPT.ORANGE,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px'
            }}>
              {t('messages.typing').toUpperCase()}
            </span>
          </div>
          <div style={{
            color: FINCEPT.WHITE,
            fontSize: '11px',
            lineHeight: '1.6',
            whiteSpace: 'pre-wrap',
            fontFamily: '"IBM Plex Mono", "Consolas", monospace'
          }}>
            {streamingContent}
            <span style={{
              color: FINCEPT.ORANGE,
              animation: 'blink 1s infinite'
            }}>▊</span>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
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
          background: ${FINCEPT.DARK_BG};
        }
        *::-webkit-scrollbar-thumb {
          background: ${FINCEPT.BORDER};
          border-radius: 3px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: ${FINCEPT.MUTED};
        }
        /* For Firefox */
        * {
          scrollbar-width: thin;
          scrollbar-color: ${FINCEPT.BORDER} ${FINCEPT.DARK_BG};
        }
        /* Input focus states */
        input:focus, textarea:focus {
          outline: none;
          border-color: ${FINCEPT.ORANGE} !important;
        }
        /* Blink animation for cursor */
        @keyframes blink {
          0%, 50% { opacity: 1; }
          51%, 100% { opacity: 0; }
        }
      `}</style>
      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <span style={{
            color: FINCEPT.ORANGE,
            fontWeight: 700,
            fontSize: '11px',
            letterSpacing: '0.5px'
          }}>
            {t('title').toUpperCase()}
          </span>
          <span style={{ color: FINCEPT.GRAY }}>|</span>
          <span style={{
            color: FINCEPT.YELLOW,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>
            {activeLLMConfig?.provider.toUpperCase() || 'NO PROVIDER'}
          </span>
          <span style={{ color: FINCEPT.GRAY }}>|</span>
          <span style={{
            color: FINCEPT.WHITE,
            fontSize: '9px',
            letterSpacing: '0.5px'
          }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={() => onNavigateToSettings?.()}
            style={{
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              padding: '6px 10px',
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
            title="Go to Settings tab to configure LLM providers"
          >
            <Settings size={12} />
            {t('header.settings').toUpperCase()}
          </button>
          <button
            onClick={createNewSession}
            style={{
              backgroundColor: FINCEPT.ORANGE,
              border: 'none',
              color: FINCEPT.DARK_BG,
              padding: '8px 16px',
              fontSize: '9px',
              fontWeight: 700,
              borderRadius: '2px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s'
            }}
          >
            <Plus size={12} />
            {t('header.newChat').toUpperCase()}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Sessions */}
        <div style={{
          width: '280px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Section Header */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px'
              }}>
                {t('history.sessions').toUpperCase()} ({statistics.totalSessions})
              </span>
              {sessions.length > 0 && (
                <button
                  onClick={deleteAllSessions}
                  style={{
                    padding: '2px 6px',
                    backgroundColor: `${FINCEPT.RED}20`,
                    color: FINCEPT.RED,
                    border: 'none',
                    fontSize: '8px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    letterSpacing: '0.5px',
                    transition: 'all 0.2s'
                  }}
                  title={t('history.deleteAll')}
                >
                  {t('history.clearAll').toUpperCase()}
                </button>
              )}
            </div>

            {/* Search */}
            <div style={{ position: 'relative', marginBottom: '8px' }}>
              <Search size={12} color={FINCEPT.GRAY} style={{ position: 'absolute', left: '8px', top: '9px' }} />
              <input
                id="session-search"
                type="text"
                placeholder={t('input.placeholder')}
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{
                  width: '100%',
                  padding: '8px 10px 8px 32px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                }}
              />
            </div>

            {/* Stats */}
            <div style={{
              fontSize: '9px',
              color: FINCEPT.CYAN,
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}>
              MESSAGES: {statistics.totalMessages} | TOKENS: {statistics.totalTokens.toLocaleString()}
            </div>
          </div>

          {/* Session List with scrollbar */}
          <div
            ref={sessionsListRef}
            style={{
              flex: 1,
              overflow: 'auto',
              padding: '8px'
            }}
          >
            {filteredSessions.map(session => (
              <div
                key={session.session_uuid}
                style={{
                  padding: '10px 12px',
                  backgroundColor: currentSessionUuid === session.session_uuid ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: currentSessionUuid === session.session_uuid ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: renamingSessionId === session.session_uuid ? 'default' : 'pointer',
                  transition: 'all 0.2s',
                  marginBottom: '4px'
                }}
                onClick={() => renamingSessionId !== session.session_uuid && selectSession(session.session_uuid)}
                onMouseEnter={(e) => {
                  if (currentSessionUuid !== session.session_uuid && renamingSessionId !== session.session_uuid) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }
                }}
                onMouseLeave={(e) => {
                  if (currentSessionUuid !== session.session_uuid) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                {/* Title or Rename Input */}
                {renamingSessionId === session.session_uuid ? (
                  <div style={{ marginBottom: '6px', display: 'flex', gap: '4px', alignItems: 'center' }}>
                    <input
                      type="text"
                      value={renameValue}
                      onChange={(e) => setRenameValue(e.target.value)}
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') saveRename(session.session_uuid);
                        if (e.key === 'Escape') cancelRename();
                      }}
                      autoFocus
                      style={{
                        flex: 1,
                        padding: '4px 6px',
                        backgroundColor: FINCEPT.DARK_BG,
                        border: `1px solid ${FINCEPT.ORANGE}`,
                        color: FINCEPT.WHITE,
                        fontSize: '10px',
                        borderRadius: '2px',
                        fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                      }}
                    />
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        saveRename(session.session_uuid);
                      }}
                      style={{
                        backgroundColor: 'transparent',
                        color: FINCEPT.GREEN,
                        border: 'none',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex',
                        transition: 'all 0.2s'
                      }}
                    >
                      <Check size={12} />
                    </button>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        cancelRename();
                      }}
                      style={{
                        backgroundColor: 'transparent',
                        color: FINCEPT.RED,
                        border: 'none',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex',
                        transition: 'all 0.2s'
                      }}
                    >
                      <X size={12} />
                    </button>
                  </div>
                ) : (
                  <div style={{
                    color: FINCEPT.WHITE,
                    fontSize: '10px',
                    marginBottom: '6px',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                    fontWeight: 700
                  }}>
                    {session.title}
                  </div>
                )}

                {/* Actions Row */}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                  }}>
                    {session.message_count} MSGS • {formatSessionTime(session.updated_at)}
                  </span>
                  <div style={{ display: 'flex', gap: '6px' }}>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        startRenaming(session.session_uuid, session.title);
                      }}
                      style={{
                        backgroundColor: 'transparent',
                        color: FINCEPT.YELLOW,
                        border: 'none',
                        fontSize: '9px',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex',
                        transition: 'all 0.2s'
                      }}
                      title="Rename session"
                    >
                      <Edit2 size={10} />
                    </button>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        deleteSession(session.session_uuid);
                      }}
                      style={{
                        backgroundColor: 'transparent',
                        color: FINCEPT.RED,
                        border: 'none',
                        fontSize: '9px',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex',
                        transition: 'all 0.2s'
                      }}
                      title="Delete session"
                    >
                      <Trash2 size={10} />
                    </button>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Center Panel - Chat */}
        <div style={{
          flex: 1,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Chat Header */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px'
            }}>
              {currentSessionUuid ? (sessions.find(s => s.session_uuid === currentSessionUuid)?.title || 'CHAT').toUpperCase() : 'NO ACTIVE SESSION'}
            </span>
            {currentSessionUuid && (
              <button
                onClick={clearCurrentChat}
                style={{
                  padding: '2px 6px',
                  backgroundColor: `${FINCEPT.RED}20`,
                  color: FINCEPT.RED,
                  border: 'none',
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s'
                }}
              >
                CLEAR
              </button>
            )}
          </div>

          {/* Messages Area with scrollbar */}
          <div style={{
            flex: 1,
            padding: '16px',
            overflow: 'auto',
            backgroundColor: FINCEPT.DARK_BG
          }}>
            {messages.length === 0 && !streamingContent ? renderWelcomeScreen() : (
              <div>
                {messages.map(renderMessage)}
                {streamingContent && renderStreamingMessage()}
                {isTyping && !streamingContent && (
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    letterSpacing: '0.5px'
                  }}>
                    <Bot size={12} color={FINCEPT.GRAY} />
                    <span>{t('messages.thinking').toUpperCase()}</span>
                  </div>
                )}
                <div ref={messagesEndRef} />
              </div>
            )}
          </div>

          {/* Input Area */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderTop: `1px solid ${FINCEPT.BORDER}`
          }}>
            <div style={{ display: 'flex', gap: '8px' }}>
              <textarea
                value={messageInput}
                onChange={(e) => setMessageInput(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    handleSendMessage();
                  }
                }}
                placeholder={t('input.placeholder')}
                style={{
                  flex: 1,
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  resize: 'none',
                  height: '60px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                }}
              />
              <button
                onClick={handleSendMessage}
                disabled={!messageInput.trim() || isTyping}
                style={{
                  padding: '8px 16px',
                  backgroundColor: messageInput.trim() && !isTyping ? FINCEPT.ORANGE : FINCEPT.MUTED,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: messageInput.trim() && !isTyping ? 'pointer' : 'not-allowed',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s',
                  opacity: messageInput.trim() && !isTyping ? 1 : 0.5
                }}
              >
                <Send size={12} />
                {t('input.send').toUpperCase()}
              </button>
            </div>
            <div style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              marginTop: '6px',
              letterSpacing: '0.5px',
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}>
              {messageInput.length > 0 ? `${messageInput.length} CHARS` : systemStatus}
            </div>
          </div>
        </div>

        {/* Right Panel - Quick Actions & Context Selector */}
        <div style={{
          width: '300px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Context Selector Section */}
          <div style={{
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px'
              }}>
                {t('panel.dataContexts').toUpperCase()}
              </span>
            </div>
            <div style={{ padding: '12px' }}>
              <ContextSelector
                chatSessionUuid={currentSessionUuid}
                onContextsChange={handleContextsChange}
              />
            </div>
          </div>

          {/* Quick Prompts Section */}
          <div style={{
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px'
              }}>
                {t('panel.quickPrompts').toUpperCase()}
              </span>
            </div>
            <div style={{ padding: '12px' }}>
              {[
                { cmd: 'MARKET TRENDS', prompt: 'Analyze current market trends and key insights' },
                { cmd: 'PORTFOLIO', prompt: 'Portfolio diversification recommendations' },
                { cmd: 'RISK', prompt: 'Investment risk assessment strategies' },
                { cmd: 'TECHNICAL', prompt: 'Key technical analysis indicators' }
              ].map(item => (
                <button
                  key={item.cmd}
                  onClick={() => setMessageInput(item.prompt)}
                  style={{
                    width: '100%',
                    padding: '6px 10px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    textAlign: 'left',
                    cursor: 'pointer',
                    marginBottom: '6px',
                    letterSpacing: '0.5px',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.color = FINCEPT.GRAY;
                  }}
                >
                  {item.cmd}
                </button>
              ))}
            </div>
          </div>

          {/* System Info Section */}
          <div style={{ flex: 1, overflow: 'auto' }}>
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px'
              }}>
                {t('panel.systemInfo').toUpperCase()}
              </span>
            </div>
            <div style={{ padding: '12px' }}>
              {activeLLMConfig ? (
                <>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: '9px',
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                  }}>
                    PROVIDER: {activeLLMConfig.provider.toUpperCase()}
                  </div>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: '9px',
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                  }}>
                    MODEL: {activeLLMConfig.model}
                  </div>
                  <div style={{
                    color: FINCEPT.CYAN,
                    fontSize: '9px',
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                  }}>
                    TEMP: {llmGlobalSettings.temperature}
                  </div>
                  <div style={{
                    padding: '2px 6px',
                    backgroundColor: `${FINCEPT.GREEN}20`,
                    color: FINCEPT.GREEN,
                    fontSize: '8px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    display: 'inline-block',
                    letterSpacing: '0.5px'
                  }}>
                    STREAMING: ENABLED
                  </div>
                </>
              ) : (
                <div style={{
                  padding: '2px 6px',
                  backgroundColor: `${FINCEPT.RED}20`,
                  color: FINCEPT.RED,
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  letterSpacing: '0.5px'
                }}>
                  NO LLM PROVIDER CONFIGURED
                </div>
              )}
              <div style={{
                marginTop: '12px',
                paddingTop: '12px',
                borderTop: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div style={{
                  padding: '2px 6px',
                  backgroundColor: mcpToolsCount > 0 ? `${FINCEPT.CYAN}20` : `${FINCEPT.MUTED}20`,
                  color: mcpToolsCount > 0 ? FINCEPT.CYAN : FINCEPT.MUTED,
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  marginBottom: '8px',
                  letterSpacing: '0.5px',
                  display: 'inline-block'
                }}>
                  MCP TOOLS: {mcpToolsCount > 0 ? `${mcpToolsCount} AVAILABLE [OK]` : 'NONE'}
                </div>
                <button
                  onClick={() => onNavigateToTab?.('mcp')}
                  style={{
                    width: '100%',
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    fontSize: '9px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    marginBottom: '8px',
                    letterSpacing: '0.5px',
                    transition: 'all 0.2s'
                  }}
                  title="Configure MCP servers and tools"
                >
                  🔧 MCP INTEGRATION
                </button>
                {mcpToolsCount > 0 && !providerSupportsMCP() && activeLLMConfig && (
                  <div style={{
                    padding: '6px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.RED}`,
                    borderRadius: '2px',
                    color: FINCEPT.RED,
                    fontSize: '8px',
                    fontWeight: 700,
                    letterSpacing: '0.5px'
                  }}>
                    [WARN] {activeLLMConfig.provider.toUpperCase()} DOESN'T SUPPORT MCP TOOLS
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar (Bottom) */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        letterSpacing: '0.5px',
        fontFamily: '"IBM Plex Mono", "Consolas", monospace'
      }}>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>TAB: AI CHAT</span>
          <span>|</span>
          <span>PROVIDER: {activeLLMConfig?.provider.toUpperCase() || 'NONE'}</span>
          <span>|</span>
          <span>SESSION: {currentSessionUuid ? 'ACTIVE' : 'NONE'}</span>
        </div>
        <div>
          MESSAGES: {messages.length} | STATUS: {streamingContent ? 'STREAMING...' : 'READY'}
        </div>
      </div>
    </div>
  );
};

export default ChatTab;
