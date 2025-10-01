import React, { useState, useEffect, useRef } from 'react';
import { flushSync } from 'react-dom';
import { Settings, Trash2, Bot, User, Clock, Send, Plus, Search, Edit2, Check, X } from 'lucide-react';
import { llmApiService, ChatMessage as APIMessage } from '../../services/llmApi';
import { sqliteService, ChatSession, ChatMessage } from '../../services/sqliteService';
import { llmConfigService } from '../../services/llmConfig';
import LLMSettingsModal from './LLMSettingsModal';
import MarkdownRenderer from '../common/MarkdownRenderer';

const ChatTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [currentSessionUuid, setCurrentSessionUuid] = useState<string | null>(null);
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [messageInput, setMessageInput] = useState('');
  const [sessionSearch, setSessionSearch] = useState('');
  const [isTyping, setIsTyping] = useState(false);
  const [sessions, setSessions] = useState<ChatSession[]>([]);
  const [systemStatus, setSystemStatus] = useState('STATUS: INITIALIZING...');
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [currentProvider, setCurrentProvider] = useState('ollama');
  const [streamingContent, setStreamingContent] = useState('');
  const [statistics, setStatistics] = useState({ totalSessions: 0, totalMessages: 0, totalTokens: 0 });

  // Rename and search functionality
  const [renamingSessionId, setRenamingSessionId] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const [searchQuery, setSearchQuery] = useState('');

  const messagesEndRef = useRef<HTMLDivElement>(null);
  const sessionsListRef = useRef<HTMLDivElement>(null);

  // Bloomberg color scheme
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  // Initialize database on mount
  useEffect(() => {
    const initDatabase = async () => {
      try {
        setSystemStatus('STATUS: INITIALIZING DATABASE...');

        // Clear any old localStorage data from previous versions
        const oldKeys = ['chat-sessions', 'chat-messages', 'fincept-chat-data', 'fincept-llm-settings'];
        oldKeys.forEach(key => localStorage.removeItem(key));

        // Initialize database
        await sqliteService.initialize();

        // Verify database health
        const healthCheck = await sqliteService.healthCheck();
        if (!healthCheck.healthy) {
          throw new Error(healthCheck.message);
        }

        // Load initial data
        await loadSessions();
        await loadStatistics();
        await loadLLMProvider();

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
    }, 1000);
    return () => clearInterval(timer);
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
      const sessions = await sqliteService.getChatSessions(10000);
      const allMessages: ChatMessage[] = [];
      for (const session of sessions) {
        const msgs = await sqliteService.getChatMessages(session.session_uuid);
        allMessages.push(...msgs);
      }
      const totalTokens = allMessages.reduce((sum, msg) => sum + (msg.tokens_used || 0), 0);
      setStatistics({
        totalSessions: sessions.length,
        totalMessages: allMessages.length,
        totalTokens
      });
    } catch (error) {
      console.error('Failed to load statistics:', error);
    }
  };

  const loadLLMProvider = async () => {
    try {
      const activeConfig = await sqliteService.getActiveLLMConfig();
      if (activeConfig) {
        setCurrentProvider(activeConfig.provider);
      }
    } catch (error) {
      console.error('Failed to load LLM provider:', error);
    }
  };

  const loadSessionMessages = async (sessionUuid: string) => {
    try {
      setSystemStatus('STATUS: LOADING MESSAGES...');
      const loadedMessages = await sqliteService.getChatMessages(sessionUuid);
      setMessages(loadedMessages);
      setSystemStatus('STATUS: READY');
    } catch (error) {
      console.error('Failed to load messages:', error);
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

  const createNewSession = async () => {
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
    } catch (error) {
      console.error('Failed to create session:', error);
      setSystemStatus('ERROR: Failed to create session');
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

  const handleSendMessage = async () => {
    if (!messageInput.trim()) return;

    // Create session if none exists
    if (!currentSessionUuid) {
      await createNewSession();
      // Wait a bit for session to be created
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    if (!currentSessionUuid) {
      setSystemStatus('ERROR: No active session');
      return;
    }

    // Validate configuration
    const activeConfig = await sqliteService.getActiveLLMConfig();
    if (!activeConfig) {
      setSystemStatus('ERROR: No active LLM provider configured');
      alert('LLM Configuration Error: No active provider configured\n\nPlease configure your LLM settings first.');
      setIsSettingsOpen(true);
      return;
    }

    if (activeConfig.provider !== 'ollama' && !activeConfig.api_key) {
      setSystemStatus(`ERROR: ${activeConfig.provider} requires API key`);
      alert(`LLM Configuration Error: ${activeConfig.provider} requires an API key\n\nPlease configure your LLM settings first.`);
      setIsSettingsOpen(true);
      return;
    }

    const userContent = messageInput.trim();
    setMessageInput('');
    setIsTyping(true);
    setStreamingContent('');
    setSystemStatus(`STATUS: CALLING ${currentProvider.toUpperCase()} API...`);

    try {
      // Save user message to database
      const userMessage = await sqliteService.addChatMessage({
        session_uuid: currentSessionUuid,
        role: 'user',
        content: userContent
      });

      setMessages(prev => [...prev, userMessage]);

      // Convert conversation history
      const conversationHistory: APIMessage[] = messages.map(msg => ({
        role: msg.role,
        content: msg.content
      }));

      // Call LLM API with streaming
      const response = await llmApiService.chat(
        userContent,
        conversationHistory,
        (chunk: string, done: boolean) => {
          if (!done) {
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
        }
      );

      if (response.error) {
        const errorMessage = await sqliteService.addChatMessage({
          session_uuid: currentSessionUuid,
          role: 'assistant',
          content: `❌ Error: ${response.error}\n\nPlease check your LLM configuration and try again.`
        });
        setMessages(prev => [...prev, errorMessage]);
        setIsTyping(false);
        setStreamingContent('');
        setSystemStatus(`ERROR: ${response.error}`);
      } else {
        const aiMessage = await sqliteService.addChatMessage({
          session_uuid: currentSessionUuid,
          role: 'assistant',
          content: response.content,
          provider: activeConfig.provider,
          model: activeConfig.model,
          tokens_used: response.usage?.totalTokens
        });

        setMessages(prev => [...prev, aiMessage]);
      }

      // Reload sessions and statistics
      await loadSessions();
      await loadStatistics();

    } catch (error) {
      console.error('Chat error:', error);
      const errorMessage = await sqliteService.addChatMessage({
        session_uuid: currentSessionUuid,
        role: 'assistant',
        content: `❌ Unexpected error: ${error instanceof Error ? error.message : 'Unknown error'}`
      });
      setMessages(prev => [...prev, errorMessage]);
      setIsTyping(false);
      setStreamingContent('');
      setSystemStatus('STATUS: ERROR');
    }
  };

  // Filter sessions by search query (searches in both title and message content)
  const filteredSessions = searchQuery
    ? sessions.filter(session =>
        session.title.toLowerCase().includes(searchQuery.toLowerCase())
      )
    : sessions;

  const formatTime = (date: Date | string) => {
    const d = typeof date === 'string' ? new Date(date) : date;
    return d.toLocaleTimeString('en-US', {
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  };

  const formatSessionTime = (dateString: string) => {
    const date = new Date(dateString);
    const now = new Date();
    const diffHours = Math.floor((now.getTime() - date.getTime()) / (1000 * 60 * 60));

    if (diffHours < 24) {
      return date.toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit' });
    } else if (diffHours < 168) { // Less than a week
      return date.toLocaleDateString('en-US', { weekday: 'short', hour: '2-digit', minute: '2-digit' });
    } else {
      return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
    }
  };

  const renderWelcomeScreen = () => (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100%',
      backgroundColor: BLOOMBERG_PANEL_BG
    }}>
      <div style={{ textAlign: 'center', padding: '24px' }}>
        <Bot size={48} color={BLOOMBERG_ORANGE} style={{ marginBottom: '16px' }} />
        <div style={{ color: BLOOMBERG_ORANGE, fontSize: '20px', fontWeight: 'bold', marginBottom: '8px' }}>
          FINCEPT AI ASSISTANT
        </div>
        <div style={{ color: BLOOMBERG_WHITE, fontSize: '14px', marginBottom: '16px' }}>
          Financial Intelligence System
        </div>
        <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', marginBottom: '16px' }}>
          Provider: {currentProvider.toUpperCase()} | Model: {llmConfigService.getActiveConfig().model}
        </div>
        <button
          onClick={createNewSession}
          style={{
            backgroundColor: BLOOMBERG_ORANGE,
            color: 'black',
            border: 'none',
            padding: '10px 20px',
            fontSize: '12px',
            fontWeight: 'bold',
            borderRadius: '4px',
            cursor: 'pointer'
          }}
        >
          START NEW CHAT
        </button>
      </div>
    </div>
  );

  const renderMessage = (message: ChatMessage) => (
    <div key={message.id} style={{ marginBottom: '12px' }}>
      <div style={{
        display: 'flex',
        justifyContent: message.role === 'user' ? 'flex-end' : 'flex-start',
        marginBottom: '4px'
      }}>
        <div style={{
          maxWidth: '85%',
          minWidth: '120px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          border: `1px solid ${message.role === 'user' ? BLOOMBERG_YELLOW : BLOOMBERG_ORANGE}`,
          borderRadius: '4px',
          padding: '10px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
            {message.role === 'user' ? (
              <User size={12} color={BLOOMBERG_YELLOW} />
            ) : (
              <Bot size={12} color={BLOOMBERG_ORANGE} />
            )}
            <span style={{
              color: message.role === 'user' ? BLOOMBERG_YELLOW : BLOOMBERG_ORANGE,
              fontSize: '11px',
              fontWeight: 'bold'
            }}>
              {message.role === 'user' ? 'YOU' : 'AI'}
            </span>
            <Clock size={10} color={BLOOMBERG_GRAY} />
            <span style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>
              {formatTime(message.timestamp)}
            </span>
            {message.provider && (
              <span style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>
                | {message.provider}
              </span>
            )}
          </div>
          {message.role === 'assistant' ? (
            <MarkdownRenderer content={message.content} />
          ) : (
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '13px', lineHeight: '1.5', whiteSpace: 'pre-wrap' }}>
              {message.content}
            </div>
          )}
        </div>
      </div>
    </div>
  );

  const renderStreamingMessage = () => {
    if (!streamingContent) return null;

    return (
      <div style={{ marginBottom: '12px', display: 'flex', justifyContent: 'flex-start' }}>
        <div style={{
          maxWidth: '85%',
          backgroundColor: BLOOMBERG_PANEL_BG,
          border: `2px solid ${BLOOMBERG_ORANGE}`,
          borderRadius: '4px',
          padding: '10px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
            <Bot size={12} color={BLOOMBERG_ORANGE} />
            <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
              AI TYPING...
            </span>
          </div>
          <div style={{
            color: BLOOMBERG_WHITE,
            fontSize: '13px',
            lineHeight: '1.5',
            whiteSpace: 'pre-wrap'
          }}>
            {streamingContent}
            <span style={{ color: BLOOMBERG_ORANGE }}>▊</span>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px'
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
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '6px 10px',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold', fontSize: '12px' }}>FINCEPT AI</span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{ color: BLOOMBERG_YELLOW, fontSize: '10px' }}>
            {currentProvider.toUpperCase()}
          </span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE, fontSize: '10px' }}>
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => setIsSettingsOpen(true)}
            style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '4px 8px',
              fontSize: '10px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <Settings size={12} />
            CONFIG
          </button>
          <button
            onClick={createNewSession}
            style={{
              backgroundColor: BLOOMBERG_ORANGE,
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
            NEW
          </button>
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Sessions */}
        <div style={{
          width: '250px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG_GRAY}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          <div style={{ padding: '8px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '11px', fontWeight: 'bold' }}>
                SESSIONS ({statistics.totalSessions})
              </div>
              {sessions.length > 0 && (
                <button
                  onClick={deleteAllSessions}
                  style={{
                    backgroundColor: BLOOMBERG_RED,
                    color: BLOOMBERG_WHITE,
                    border: 'none',
                    padding: '2px 6px',
                    fontSize: '8px',
                    cursor: 'pointer',
                    fontWeight: 'bold'
                  }}
                  title="Delete all sessions"
                >
                  DELETE ALL
                </button>
              )}
            </div>

            {/* Search */}
            <div style={{ position: 'relative', marginBottom: '8px' }}>
              <Search size={12} color={BLOOMBERG_GRAY} style={{ position: 'absolute', left: '6px', top: '6px' }} />
              <input
                id="session-search"
                type="text"
                placeholder="Search chats..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{
                  width: '100%',
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '4px 4px 4px 24px',
                  fontSize: '10px'
                }}
              />
            </div>

            {/* Stats */}
            <div style={{ fontSize: '9px', color: BLOOMBERG_GRAY, marginBottom: '6px' }}>
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
                  backgroundColor: currentSessionUuid === session.session_uuid ? BLOOMBERG_ORANGE + '30' : BLOOMBERG_DARK_BG,
                  border: `1px solid ${currentSessionUuid === session.session_uuid ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY}`,
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
                        backgroundColor: BLOOMBERG_DARK_BG,
                        border: `1px solid ${BLOOMBERG_ORANGE}`,
                        color: BLOOMBERG_WHITE,
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
                        color: BLOOMBERG_GREEN,
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
                        color: BLOOMBERG_RED,
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
                  <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '3px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                    {session.title}
                  </div>
                )}

                {/* Actions Row */}
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <span style={{ color: BLOOMBERG_GRAY, fontSize: '9px' }}>
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
                        color: BLOOMBERG_YELLOW,
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
                        color: BLOOMBERG_RED,
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
          backgroundColor: BLOOMBERG_PANEL_BG,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Chat Header */}
          <div style={{
            padding: '6px 10px',
            borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <span style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
              {currentSessionUuid ? sessions.find(s => s.session_uuid === currentSessionUuid)?.title || 'Chat' : 'No Active Session'}
            </span>
            {currentSessionUuid && (
              <button
                onClick={clearCurrentChat}
                style={{
                  backgroundColor: BLOOMBERG_RED,
                  color: BLOOMBERG_WHITE,
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
            backgroundColor: BLOOMBERG_DARK_BG
          }}>
            {messages.length === 0 && !streamingContent ? renderWelcomeScreen() : (
              <div>
                {messages.map(renderMessage)}
                {streamingContent && renderStreamingMessage()}
                {isTyping && !streamingContent && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: BLOOMBERG_GRAY, fontSize: '11px' }}>
                    <Bot size={12} />
                    <span>AI is thinking...</span>
                  </div>
                )}
                <div ref={messagesEndRef} />
              </div>
            )}
          </div>

          {/* Input Area */}
          <div style={{ padding: '8px', borderTop: `1px solid ${BLOOMBERG_GRAY}` }}>
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
                placeholder="Type your message... (Shift+Enter for new line)"
                style={{
                  flex: 1,
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
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
                  backgroundColor: messageInput.trim() && !isTyping ? BLOOMBERG_ORANGE : BLOOMBERG_GRAY,
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
                SEND
              </button>
            </div>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', marginTop: '4px' }}>
              {messageInput.length > 0 ? `${messageInput.length} chars` : systemStatus}
            </div>
          </div>
        </div>

        {/* Right Panel - Quick Actions */}
        <div style={{
          width: '200px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          borderLeft: `1px solid ${BLOOMBERG_GRAY}`,
          padding: '8px',
          overflow: 'auto'
        }}>
          <div style={{ color: BLOOMBERG_YELLOW, fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>
            QUICK PROMPTS
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
                backgroundColor: BLOOMBERG_DARK_BG,
                color: BLOOMBERG_WHITE,
                border: `1px solid ${BLOOMBERG_GRAY}`,
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

          <div style={{ color: BLOOMBERG_YELLOW, fontSize: '11px', fontWeight: 'bold', marginTop: '12px', marginBottom: '6px' }}>
            SYSTEM INFO
          </div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '9px', marginBottom: '2px' }}>
            Provider: {currentProvider.toUpperCase()}
          </div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '9px', marginBottom: '2px' }}>
            Model: {llmConfigService.getActiveConfig().model}
          </div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '9px', marginBottom: '2px' }}>
            Temp: {llmConfigService.getActiveConfig().temperature}
          </div>
          <div style={{ color: BLOOMBERG_GREEN, fontSize: '9px' }}>
            Streaming: Enabled
          </div>
        </div>
      </div>

      {/* Settings Modal */}
      <LLMSettingsModal
        isOpen={isSettingsOpen}
        onClose={() => setIsSettingsOpen(false)}
        onSave={() => {
          setCurrentProvider(llmConfigService.getActiveProvider());
          setSystemStatus('STATUS: SETTINGS UPDATED');
        }}
      />
    </div>
  );
};

export default ChatTab;
