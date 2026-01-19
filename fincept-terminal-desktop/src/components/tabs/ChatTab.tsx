import React, { useState, useEffect, useRef } from 'react';
import { flushSync } from 'react-dom';
import { Settings, Trash2, Bot, User, Clock, Send, Plus, Search, Edit2, Check, X } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useAuth } from '@/contexts/AuthContext';
import { useTranslation } from 'react-i18next';
import { llmApiService, ChatMessage as APIMessage } from '../../services/chat/llmApi';
import { sqliteService, ChatSession, ChatMessage, RecordedContext, LLMConfig, LLMGlobalSettings, ChatStatistics } from '../../services/core/sqliteService';
import { contextRecorderService } from '../../services/data-sources/contextRecorderService';
import MarkdownRenderer from '../common/MarkdownRenderer';
import ContextSelector from '../common/ContextSelector';
import { TabFooter } from '@/components/common/TabFooter';

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
      const { mcpToolService } = await import('../../services/mcp/mcpToolService');
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
    const cleanMsg = message.replace(/[^\w\s]/g, '').trim();
    const words = cleanMsg.split(' ');

    if (words.length === 0) {
      return `Chat ${new Date().toLocaleString()}`;
    } else if (words.length <= 4) {
      return cleanMsg.substring(0, 40);
    } else {
      return words.slice(0, 4).join(' ') + '...';
    }
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
      const { mcpToolService } = await import('../../services/mcp/mcpToolService');
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
      backgroundColor: colors.panel
    }}>
      <div style={{ textAlign: 'center', padding: '24px' }}>
        <Bot size={48} color={colors.primary} style={{ marginBottom: '16px' }} />
        <div style={{ color: colors.primary, fontSize: '20px', fontWeight: 'bold', marginBottom: '8px' }}>
          {t('title')}
        </div>
        <div style={{ color: colors.text, fontSize: '14px', marginBottom: '16px' }}>
          {t('subtitle')}
        </div>
        <div style={{ color: colors.warning, fontSize: '12px', marginBottom: '16px' }}>
          Provider: {activeLLMConfig?.provider.toUpperCase() || 'NONE'} | Model: {activeLLMConfig?.model || 'N/A'}
        </div>
        <button
          onClick={createNewSession}
          style={{
            backgroundColor: colors.primary,
            color: 'black',
            border: 'none',
            padding: '10px 20px',
            fontSize: '12px',
            fontWeight: 'bold',
            borderRadius: '4px',
            cursor: 'pointer'
          }}
        >
          {t('header.newChat')}
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
              backgroundColor: colors.panel,
              border: `1px solid ${message.role === 'user' ? colors.warning : colors.primary}`,
              borderRadius: '4px',
              padding: '10px'
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
                {message.role === 'user' ? (
                  <User size={12} color={colors.warning} />
                ) : (
                  <Bot size={12} color={colors.primary} />
                )}
                <span style={{
                  color: message.role === 'user' ? colors.warning : colors.primary,
                  fontSize: '11px',
                  fontWeight: 'bold'
                }}>
                  {message.role === 'user' ? 'YOU' : 'AI'}
                </span>
                <Clock size={10} color={colors.textMuted} />
                <span style={{ color: colors.textMuted, fontSize: '10px' }}>
                  {formatTime(message.timestamp)}
                </span>
                {message.provider && (
                  <span style={{ color: colors.textMuted, fontSize: '10px' }}>
                    | {message.provider}
                  </span>
                )}
              </div>
              {message.role === 'assistant' && message.content ? (
                <MarkdownRenderer content={message.content} />
              ) : (
                <div style={{ color: colors.text, fontSize: '13px', lineHeight: '1.5', whiteSpace: 'pre-wrap' }}>
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
        <div key={message.id} style={{ marginBottom: '12px', color: colors.alert }}>
          Error rendering message
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
          backgroundColor: colors.panel,
          border: `2px solid ${colors.primary}`,
          borderRadius: '4px',
          padding: '10px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
            <Bot size={12} color={colors.primary} />
            <span style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold' }}>
              {t('messages.typing')}
            </span>
          </div>
          <div style={{
            color: colors.text,
            fontSize: '13px',
            lineHeight: '1.5',
            whiteSpace: 'pre-wrap'
          }}>
            {streamingContent}
            <span style={{ color: colors.primary }}>▊</span>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      fontWeight: fontWeight,
      fontStyle: fontStyle,
      display: 'flex',
      flexDirection: 'column',
      fontSize: fontSize.body
    }}>
      <style>{`
        /* Transparent/hidden scrollbar styling */
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: transparent;
        }
        *::-webkit-scrollbar-thumb {
          background: rgba(255, 165, 0, 0.2);
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: rgba(255, 165, 0, 0.4);
        }
        /* For Firefox */
        * {
          scrollbar-width: thin;
          scrollbar-color: rgba(255, 165, 0, 0.2) transparent;
        }
      `}</style>
      {/* Compact Header */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '6px 10px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <span style={{ color: colors.primary, fontWeight: 'bold', fontSize: '12px' }}>{t('title')}</span>
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ color: colors.warning, fontSize: '10px' }}>
            {activeLLMConfig?.provider.toUpperCase() || 'NO PROVIDER'}
          </span>
          <span style={{ color: colors.textMuted }}>|</span>
          <span style={{ color: colors.text, fontSize: '10px' }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => onNavigateToSettings?.()}
            style={{
              backgroundColor: colors.background,
              border: `1px solid ${colors.textMuted}`,
              color: colors.text,
              padding: '4px 8px',
              fontSize: '10px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
            title="Go to Settings tab to configure LLM providers"
          >
            <Settings size={12} />
            {t('header.settings')}
          </button>
          <button
            onClick={createNewSession}
            style={{
              backgroundColor: colors.primary,
              border: 'none',
              color: 'black',
              padding: '4px 8px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Plus size={12} />
            {t('header.newChat')}
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Sessions */}
        <div style={{
          width: '250px',
          backgroundColor: colors.panel,
          borderRight: `1px solid ${colors.textMuted}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          <div style={{ padding: '8px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
              <div style={{ color: colors.warning, fontSize: '11px', fontWeight: 'bold' }}>
                {t('history.sessions')} ({statistics.totalSessions})
              </div>
              {sessions.length > 0 && (
                <button
                  onClick={deleteAllSessions}
                  style={{
                    backgroundColor: colors.alert,
                    color: colors.text,
                    border: 'none',
                    padding: '2px 6px',
                    fontSize: '8px',
                    cursor: 'pointer',
                    fontWeight: 'bold'
                  }}
                  title={t('history.deleteAll')}
                >
                  {t('history.clearAll')}
                </button>
              )}
            </div>

            {/* Search */}
            <div style={{ position: 'relative', marginBottom: '8px' }}>
              <Search size={12} color={colors.textMuted} style={{ position: 'absolute', left: '6px', top: '6px' }} />
              <input
                id="session-search"
                type="text"
                placeholder={t('input.placeholder')}
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  padding: '4px 4px 4px 24px',
                  fontSize: '10px'
                }}
              />
            </div>

            {/* Stats */}
            <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '6px' }}>
              Messages: {statistics.totalMessages} | Tokens: {statistics.totalTokens.toLocaleString()}
            </div>
          </div>

          {/* Session List with scrollbar */}
          <div
            ref={sessionsListRef}
            style={{
              flex: 1,
              overflow: 'auto',
              padding: '0 8px 8px'
            }}
          >
            {filteredSessions.map(session => (
              <div
                key={session.session_uuid}
                style={{
                  backgroundColor: currentSessionUuid === session.session_uuid ? colors.primary + '30' : colors.background,
                  border: `1px solid ${currentSessionUuid === session.session_uuid ? colors.primary : colors.textMuted}`,
                  padding: '6px',
                  marginBottom: '4px',
                  cursor: renamingSessionId === session.session_uuid ? 'default' : 'pointer'
                }}
                onClick={() => renamingSessionId !== session.session_uuid && selectSession(session.session_uuid)}
              >
                {/* Title or Rename Input */}
                {renamingSessionId === session.session_uuid ? (
                  <div style={{ marginBottom: '3px', display: 'flex', gap: '4px', alignItems: 'center' }}>
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
                        backgroundColor: colors.background,
                        border: `1px solid ${colors.primary}`,
                        color: colors.text,
                        fontSize: '11px',
                        padding: '2px 4px',
                        fontFamily: 'Consolas, monospace'
                      }}
                    />
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        saveRename(session.session_uuid);
                      }}
                      style={{
                        backgroundColor: 'transparent',
                        color: colors.secondary,
                        border: 'none',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex'
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
                        color: colors.alert,
                        border: 'none',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex'
                      }}
                    >
                      <X size={12} />
                    </button>
                  </div>
                ) : (
                  <div style={{ color: colors.text, fontSize: '11px', marginBottom: '3px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {session.title}
                  </div>
                )}

                {/* Actions Row */}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: colors.textMuted, fontSize: '9px' }}>
                    {session.message_count} msgs • {formatSessionTime(session.updated_at)}
                  </span>
                  <div style={{ display: 'flex', gap: '6px' }}>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        startRenaming(session.session_uuid, session.title);
                      }}
                      style={{
                        backgroundColor: 'transparent',
                        color: colors.warning,
                        border: 'none',
                        fontSize: '9px',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex'
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
                        color: colors.alert,
                        border: 'none',
                        fontSize: '9px',
                        cursor: 'pointer',
                        padding: '0',
                        display: 'flex'
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
          backgroundColor: colors.panel,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Chat Header */}
          <div style={{
            padding: '6px 10px',
            borderBottom: `1px solid ${colors.textMuted}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <span style={{ color: colors.text, fontSize: '11px' }}>
              {currentSessionUuid ? sessions.find(s => s.session_uuid === currentSessionUuid)?.title || 'Chat' : 'No Active Session'}
            </span>
            {currentSessionUuid && (
              <button
                onClick={clearCurrentChat}
                style={{
                  backgroundColor: colors.alert,
                  color: colors.text,
                  border: 'none',
                  padding: '3px 8px',
                  fontSize: '9px',
                  cursor: 'pointer'
                }}
              >
                CLEAR
              </button>
            )}
          </div>

          {/* Messages Area with scrollbar */}
          <div style={{
            flex: 1,
            padding: '10px',
            overflow: 'auto',
            backgroundColor: colors.background
          }}>
            {messages.length === 0 && !streamingContent ? renderWelcomeScreen() : (
              <div>
                {messages.map(renderMessage)}
                {streamingContent && renderStreamingMessage()}
                {isTyping && !streamingContent && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: colors.textMuted, fontSize: '11px' }}>
                    <Bot size={12} />
                    <span>{t('messages.thinking')}</span>
                  </div>
                )}
                <div ref={messagesEndRef} />
              </div>
            )}
          </div>

          {/* Input Area */}
          <div style={{ padding: '8px', borderTop: `1px solid ${colors.textMuted}` }}>
            <div style={{ display: 'flex', gap: '6px' }}>
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
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.textMuted}`,
                  color: colors.text,
                  padding: '6px',
                  fontSize: '11px',
                  resize: 'none',
                  height: '60px',
                  fontFamily: 'Consolas, monospace'
                }}
              />
              <button
                onClick={handleSendMessage}
                disabled={!messageInput.trim() || isTyping}
                style={{
                  backgroundColor: messageInput.trim() && !isTyping ? colors.primary : colors.textMuted,
                  color: 'black',
                  border: 'none',
                  padding: '6px 12px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: messageInput.trim() && !isTyping ? 'pointer' : 'not-allowed',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                <Send size={12} />
                {t('input.send')}
              </button>
            </div>
            <div style={{ color: colors.textMuted, fontSize: '9px', marginTop: '4px' }}>
              {messageInput.length > 0 ? `${messageInput.length} chars` : systemStatus}
            </div>
          </div>
        </div>

        {/* Right Panel - Quick Actions & Context Selector */}
        <div style={{
          width: '250px',
          backgroundColor: colors.panel,
          borderLeft: `1px solid ${colors.textMuted}`,
          padding: '8px',
          overflow: 'auto',
          display: 'flex',
          flexDirection: 'column',
          gap: '12px'
        }}>
          {/* Context Selector */}
          <div>
            <div style={{ color: colors.warning, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              {t('panel.dataContexts')}
            </div>
            <ContextSelector
              chatSessionUuid={currentSessionUuid}
              onContextsChange={handleContextsChange}
            />
          </div>

          {/* Quick Prompts */}
          <div>
            <div style={{ color: colors.warning, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
              {t('panel.quickPrompts')}
            </div>
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
                  backgroundColor: colors.background,
                  color: colors.text,
                  border: `1px solid ${colors.textMuted}`,
                  padding: '5px',
                  fontSize: '9px',
                  textAlign: 'left',
                  cursor: 'pointer',
                  marginBottom: '4px'
                }}
              >
                {item.cmd}
              </button>
            ))}
          </div>

          {/* System Info */}
          <div>
            <div style={{ color: colors.warning, fontSize: '11px', fontWeight: 'bold', marginBottom: '6px' }}>
              {t('panel.systemInfo')}
            </div>
            {activeLLMConfig ? (
              <>
                <div style={{ color: colors.text, fontSize: '9px', marginBottom: '2px' }}>
                  Provider: {activeLLMConfig.provider.toUpperCase()}
                </div>
                <div style={{ color: colors.text, fontSize: '9px', marginBottom: '2px' }}>
                  Model: {activeLLMConfig.model}
                </div>
                <div style={{ color: colors.text, fontSize: '9px', marginBottom: '2px' }}>
                  Temp: {llmGlobalSettings.temperature}
                </div>
                <div style={{ color: colors.secondary, fontSize: '9px', marginBottom: '2px' }}>
                  Streaming: Enabled
                </div>
              </>
            ) : (
              <div style={{ color: colors.alert, fontSize: '9px', marginBottom: '6px' }}>
                No LLM provider configured
              </div>
            )}
            <div style={{
              marginTop: '6px',
              paddingTop: '6px',
              borderTop: `1px solid ${colors.textMuted}`
            }}>
              <div style={{
                color: mcpToolsCount > 0 ? colors.secondary : colors.textMuted,
                fontSize: '9px',
                marginBottom: '6px'
              }}>
                MCP Tools: {mcpToolsCount > 0 ? `${mcpToolsCount} Available [OK]` : 'None'}
              </div>
              <button
                onClick={() => onNavigateToTab?.('mcp')}
                style={{
                  width: '100%',
                  backgroundColor: colors.primary,
                  color: 'black',
                  border: 'none',
                  padding: '5px 8px',
                  fontSize: '9px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  marginBottom: '6px',
                  borderRadius: '2px'
                }}
                title="Configure MCP servers and tools"
              >
                🔧 MCP INTEGRATION
              </button>
              {mcpToolsCount > 0 && !providerSupportsMCP() && activeLLMConfig && (
                <div style={{
                  color: colors.alert,
                  fontSize: '8px',
                  marginTop: '4px',
                  padding: '4px',
                  backgroundColor: colors.background,
                  border: `1px solid ${colors.alert}`,
                  borderRadius: '2px'
                }}>
                  [WARN] {activeLLMConfig.provider.toUpperCase()} doesn't support MCP tools
                </div>
              )}
            </div>
          </div>
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="AI CHAT"
        leftInfo={[
          { label: `Provider: ${activeLLMConfig?.provider.toUpperCase() || 'NONE'}`, color: colors.textMuted },
          { label: `Session: ${currentSessionUuid ? 'Active' : 'None'}`, color: colors.textMuted },
        ]}
        statusInfo={`Messages: ${messages.length} | ${streamingContent ? 'Streaming...' : 'Ready'}`}
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />
    </div>
  );
};

export default ChatTab;
