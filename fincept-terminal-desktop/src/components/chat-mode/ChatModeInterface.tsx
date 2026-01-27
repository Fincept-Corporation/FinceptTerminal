// File: src/components/chat-mode/ChatModeInterface.tsx
// Fincept-style Chat Mode interface with professional terminal design

import React, { useState, useEffect, useRef } from 'react';
import { Send, Terminal, Activity, Zap, Database, TrendingUp, Search, Trash2, Download, FileText, Plus } from 'lucide-react';
import { chatModeApiService, ChatModeMessage, ChatSession } from '@/services/data-sources/chatModeApi';
import ChatMessageBubble from './components/ChatMessageBubble';
import ChatSuggestions from './components/ChatSuggestions';

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

const ChatModeInterface: React.FC = () => {
  const [messages, setMessages] = useState<ChatModeMessage[]>([]);
  const [input, setInput] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [isInitializing, setIsInitializing] = useState(true);
  const [isLoadingMessages, setIsLoadingMessages] = useState(false);
  const [currentSessionUuid, setCurrentSessionUuid] = useState<string | null>(null);
  const [sessions, setSessions] = useState<ChatSession[]>([]);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<any[]>([]);
  const [showSearch, setShowSearch] = useState(false);
  const [stats, setStats] = useState<any>(null);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const [currentTime, setCurrentTime] = useState(new Date());
  const pollIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const currentSessionUuidRef = useRef<string | null>(null);
  const isLoadingRef = useRef(false);

  // Keep refs in sync with state
  useEffect(() => { currentSessionUuidRef.current = currentSessionUuid; }, [currentSessionUuid]);
  useEffect(() => { isLoadingRef.current = isLoading; }, [isLoading]);

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  useEffect(() => {
    // Initialize: load or create session
    initializeSession();

    // Cleanup polling on unmount
    return () => {
      if (pollIntervalRef.current) {
        clearInterval(pollIntervalRef.current);
      }
    };
  }, []);

  const sortSessionsNewestFirst = (sessionList: ChatSession[]): ChatSession[] => {
    return [...sessionList].sort((a, b) => {
      const dateA = new Date(a.updated_at || a.created_at).getTime();
      const dateB = new Date(b.updated_at || b.created_at).getTime();
      return dateB - dateA;
    });
  };

  const initializeSession = async () => {
    setIsInitializing(true);
    try {
      // Get all sessions from API
      const sessionsResponse = await chatModeApiService.getSessions();

      if (sessionsResponse.success && sessionsResponse.data?.length > 0) {
        const sorted = sortSessionsNewestFirst(sessionsResponse.data);
        setSessions(sorted);

        // Find active session or use most recent (first after sort)
        const activeSession = sorted.find((s: any) => s.is_active) || sorted[0];
        setCurrentSessionUuid(activeSession.session_uuid);

        // Load messages for this session
        await loadSessionMessages(activeSession.session_uuid);
      } else {
        // Create new session
        await createNewSession();
      }

      // Load stats
      await loadStats();

      // Start polling for background responses
      startPolling();
    } catch (error) {
      console.error('Failed to initialize session:', error);
      await createNewSession();
    } finally {
      setIsInitializing(false);
    }
  };

  const loadSessionMessages = async (sessionUuid: string) => {
    setIsLoadingMessages(true);
    try {
      const sessionData = await chatModeApiService.getSession(sessionUuid);
      if (sessionData.success && sessionData.data?.messages) {
        const loadedMessages: ChatModeMessage[] = sessionData.data.messages.map((msg: any) => ({
          id: msg.message_uuid || msg.id,
          role: msg.role,
          content: msg.content,
          timestamp: new Date(msg.created_at || msg.timestamp),
          message_uuid: msg.message_uuid
        }));
        setMessages(loadedMessages);
      }
    } catch (error) {
      console.error('Failed to load messages:', error);
    } finally {
      setIsLoadingMessages(false);
    }
  };

  const loadStats = async () => {
    try {
      const response = await chatModeApiService.getStats();
      if (response.success) {
        setStats(response.data);
      }
    } catch (error) {
      console.error('Failed to load stats:', error);
    }
  };

  const createNewSession = async () => {
    try {
      const response = await chatModeApiService.createSession('New Chat');
      if (response.success && response.data) {
        setCurrentSessionUuid(response.data.session_uuid);
        setMessages([]);
        setSessions(prev => [response.data, ...prev]);
        await chatModeApiService.activateSession(response.data.session_uuid);
      }
    } catch (error) {
      console.error('Failed to create session:', error);
    }
  };

  const switchSession = async (sessionUuid: string) => {
    if (sessionUuid === currentSessionUuid) return;
    setCurrentSessionUuid(sessionUuid);
    setMessages([]);
    await loadSessionMessages(sessionUuid);
    chatModeApiService.activateSession(sessionUuid).catch(() => {});
  };

  const deleteSession = async (sessionUuid: string) => {
    try {
      await chatModeApiService.deleteSession(sessionUuid);
      setSessions(prev => prev.filter(s => s.session_uuid !== sessionUuid));
      if (currentSessionUuid === sessionUuid) {
        if (sessions.length > 1) {
          const nextSession = sessions.find(s => s.session_uuid !== sessionUuid);
          if (nextSession) {
            await switchSession(nextSession.session_uuid);
          }
        } else {
          await createNewSession();
        }
      }
    } catch (error) {
      console.error('Failed to delete session:', error);
    }
  };

  const handleSearch = async () => {
    if (!searchQuery.trim()) return;

    try {
      const response = await chatModeApiService.searchMessages(searchQuery);
      if (response.success) {
        setSearchResults(response.data || []);
      }
    } catch (error) {
      console.error('Search failed:', error);
    }
  };

  const exportChats = async () => {
    try {
      const response = await chatModeApiService.exportSessions();
      if (response.success && response.data) {
        const blob = new Blob([JSON.stringify(response.data, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `fincept-chat-export-${new Date().toISOString()}.json`;
        a.click();
        URL.revokeObjectURL(url);
      }
    } catch (error) {
      console.error('Export failed:', error);
    }
  };

  const startPolling = () => {
    // Poll every 3 seconds for new messages (handles background responses)
    pollIntervalRef.current = setInterval(async () => {
      if (currentSessionUuidRef.current && isLoadingRef.current) {
        await refreshMessages();
      }
    }, 3000);
  };

  const refreshMessages = async () => {
    const sessionUuid = currentSessionUuidRef.current;
    if (!sessionUuid) return;

    try {
      const sessionData = await chatModeApiService.getSession(sessionUuid);
      if (sessionData.success && sessionData.data?.messages) {
        const loadedMessages: ChatModeMessage[] = sessionData.data.messages.map((msg: any) => ({
          id: msg.message_uuid || msg.id,
          role: msg.role,
          content: msg.content,
          timestamp: new Date(msg.created_at || msg.timestamp),
          message_uuid: msg.message_uuid
        }));

        setMessages(loadedMessages);

        // If we got a response, stop loading
        const lastMessage = loadedMessages[loadedMessages.length - 1];
        if (lastMessage && lastMessage.role === 'assistant') {
          setIsLoading(false);
        }
      }
    } catch (error) {
      console.error('Failed to refresh messages:', error);
    }
  };

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const handleSendMessage = async () => {
    if (!input.trim() || isLoading || !currentSessionUuid) return;

    const messageContent = input.trim();
    setInput('');

    // Reset textarea height
    if (inputRef.current) {
      inputRef.current.style.height = '20px';
    }

    // Immediately add user message to UI
    const userMessage: ChatModeMessage = {
      id: `user-${Date.now()}`,
      role: 'user',
      content: messageContent,
      timestamp: new Date()
    };
    setMessages(prev => [...prev, userMessage]);
    setIsLoading(true);

    try {
      // Send message to session
      const response = await chatModeApiService.sendMessageToSession(currentSessionUuid, messageContent);

      if (response.success && response.data) {
        // Use the AI message directly from the response instead of re-fetching
        const aiData = response.data.ai_message;
        if (aiData) {
          const aiMessage: ChatModeMessage = {
            id: aiData.message_uuid || aiData.id || `ai-${Date.now()}`,
            role: 'assistant',
            content: aiData.content,
            timestamp: new Date(aiData.created_at || new Date()),
            message_uuid: aiData.message_uuid,
            tokens_used: aiData.tokens_used,
            response_time_ms: aiData.response_time_ms
          };
          setMessages(prev => [...prev, aiMessage]);
        }
        setIsLoading(false);

        // Refresh sessions list to update message counts, sorted newest first
        try {
          const sessionsResponse = await chatModeApiService.getSessions();
          if (sessionsResponse.success && sessionsResponse.data?.length > 0) {
            setSessions(sortSessionsNewestFirst(sessionsResponse.data));
          }
        } catch {
          // Non-critical, ignore
        }
      } else {
        throw new Error(response.error || 'Failed to get response');
      }
    } catch (error) {
      console.error('Chat error:', error);
      setIsLoading(false);

      // Add error message locally
      const errorMessage: ChatModeMessage = {
        id: `error-${Date.now()}`,
        role: 'assistant',
        content: `**ERROR:** ${error instanceof Error ? error.message : 'Unable to process request. System encountered an exception.'} Please retry or reformulate query.`,
        timestamp: new Date()
      };
      setMessages(prev => [...prev, errorMessage]);
    }
  };

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSendMessage();
    }
  };

  const handleSuggestionClick = (suggestion: string) => {
    setInput(suggestion);
    inputRef.current?.focus();
  };

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: FINCEPT.DARK_BG,
        color: FINCEPT.WHITE,
        fontFamily: '"IBM Plex Mono", "Consolas", monospace',
        overflow: 'hidden'
      }}
    >
      {/* Top Navigation Bar */}
      <div
        style={{
          padding: '8px 16px',
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          backgroundColor: FINCEPT.HEADER_BG,
          flexShrink: 0,
          boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Terminal size={16} style={{ color: FINCEPT.ORANGE }} />
            <span style={{
              fontSize: '11px',
              fontWeight: 700,
              color: FINCEPT.ORANGE,
              letterSpacing: '0.5px'
            }}>
              FINCEPT AI TERMINAL
            </span>
          </div>
          <span style={{ color: FINCEPT.GRAY }}>|</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '9px', letterSpacing: '0.5px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Activity size={12} style={{ color: FINCEPT.GREEN }} />
              <span style={{ color: FINCEPT.GREEN, fontWeight: 700 }}>ONLINE</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Database size={12} style={{ color: FINCEPT.CYAN }} />
              <span style={{ color: FINCEPT.GRAY, fontWeight: 700 }}>REALTIME</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Zap size={12} style={{ color: FINCEPT.YELLOW }} />
              <span style={{ color: FINCEPT.GRAY, fontWeight: 700 }}>AI READY</span>
            </div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '9px', letterSpacing: '0.5px' }}>
          <div style={{ color: FINCEPT.GRAY }}>
            SESSION: <span style={{ color: FINCEPT.CYAN }}>{currentSessionUuid ? currentSessionUuid.substring(0, 8) : 'NONE'}</span>
          </div>
          <div style={{ color: FINCEPT.GRAY }}>
            MESSAGES: <span style={{ color: FINCEPT.CYAN }}>{messages.length}</span>
          </div>
          <div style={{ color: FINCEPT.GRAY }}>
            TIME: <span style={{ color: FINCEPT.WHITE }}>{currentTime.toLocaleTimeString()}</span>
          </div>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Sidebar - Sessions & Tools */}
        <div
          style={{
            width: '280px',
            borderRight: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: FINCEPT.PANEL_BG,
            display: 'flex',
            flexDirection: 'column',
            flexShrink: 0
          }}
        >
          {/* Session Controls */}
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
              SESSIONS ({sessions.length})
            </span>
            <div style={{ display: 'flex', gap: '6px' }}>
              <button
                onClick={createNewSession}
                style={{
                  padding: '4px 8px',
                  backgroundColor: FINCEPT.ORANGE,
                  border: 'none',
                  borderRadius: '2px',
                  color: FINCEPT.DARK_BG,
                  fontSize: '8px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  letterSpacing: '0.5px'
                }}
                title="New Chat"
              >
                <Plus size={10} />
              </button>
              <button
                onClick={exportChats}
                style={{
                  padding: '4px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  color: FINCEPT.GRAY,
                  fontSize: '8px',
                  fontWeight: 700,
                  cursor: 'pointer'
                }}
                title="Export Chats"
              >
                <Download size={10} />
              </button>
            </div>
          </div>

          {/* Search */}
          <div style={{ padding: '8px' }}>
            <div style={{ display: 'flex', gap: '4px' }}>
              <input
                type="text"
                placeholder="SEARCH..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && handleSearch()}
                style={{
                  flex: 1,
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  color: FINCEPT.WHITE,
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace'
                }}
              />
              <button
                onClick={handleSearch}
                style={{
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.ORANGE,
                  border: 'none',
                  borderRadius: '2px',
                  cursor: 'pointer'
                }}
              >
                <Search size={10} color={FINCEPT.DARK_BG} />
              </button>
            </div>
          </div>

          {/* Sessions List */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '0 8px' }}>
            {isInitializing && sessions.length === 0 && (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '24px 12px',
                gap: '10px'
              }}>
                <div className="terminal-spinner" />
                <span style={{
                  color: FINCEPT.GRAY,
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px'
                }}>
                  LOADING SESSIONS...
                </span>
              </div>
            )}
            {sessions.map((session) => (
              <div
                key={session.session_uuid}
                onClick={() => switchSession(session.session_uuid)}
                style={{
                  padding: '10px 12px',
                  backgroundColor: currentSessionUuid === session.session_uuid ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: currentSessionUuid === session.session_uuid ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  marginBottom: '4px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}
                onMouseEnter={(e) => {
                  if (currentSessionUuid !== session.session_uuid) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }
                }}
                onMouseLeave={(e) => {
                  if (currentSessionUuid !== session.session_uuid) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                  }
                }}
              >
                <div style={{ flex: 1, overflow: 'hidden' }}>
                  <div style={{
                    color: FINCEPT.WHITE,
                    fontSize: '10px',
                    fontWeight: 700,
                    marginBottom: '2px',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap'
                  }}>
                    {session.title}
                  </div>
                  <div style={{
                    color: FINCEPT.GRAY,
                    fontSize: '8px',
                    letterSpacing: '0.5px'
                  }}>
                    {session.message_count} MSGS
                  </div>
                </div>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    deleteSession(session.session_uuid);
                  }}
                  style={{
                    padding: '4px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    color: FINCEPT.RED,
                    cursor: 'pointer',
                    display: 'flex'
                  }}
                >
                  <Trash2 size={10} />
                </button>
              </div>
            ))}
          </div>

          {/* Stats Footer */}
          {stats && (
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '9px',
              letterSpacing: '0.5px'
            }}>
              <div style={{ color: FINCEPT.GRAY, marginBottom: '6px', fontWeight: 700 }}>USAGE STATS</div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                <span style={{ color: FINCEPT.GRAY }}>TOTAL MSGS:</span>
                <span style={{ color: FINCEPT.CYAN, fontWeight: 700 }}>{stats.total_messages || 0}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                <span style={{ color: FINCEPT.GRAY }}>TOTAL TOKENS:</span>
                <span style={{ color: FINCEPT.CYAN, fontWeight: 700 }}>{stats.total_tokens?.toLocaleString() || 0}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: FINCEPT.GRAY }}>AVG RESPONSE:</span>
                <span style={{ color: FINCEPT.GREEN, fontWeight: 700 }}>{stats.avg_response_time_ms || 0}MS</span>
              </div>
            </div>
          )}
        </div>

        {/* Messages Area */}
        <div
          style={{
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}
        >
          {/* Messages Container */}
          <div
            style={{
              flex: 1,
              overflowY: 'auto',
              padding: '16px',
              backgroundColor: FINCEPT.DARK_BG
            }}
            className="custom-scrollbar"
          >
            {(isInitializing || isLoadingMessages) ? (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                gap: '16px'
              }}>
                <div className="terminal-spinner-lg" />
                <span style={{
                  color: FINCEPT.ORANGE,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '1px'
                }}>
                  {isInitializing ? 'CONNECTING TO FINCEPT API...' : 'LOADING MESSAGES...'}
                </span>
                <span style={{
                  color: FINCEPT.GRAY,
                  fontSize: '9px',
                  letterSpacing: '0.5px'
                }}>
                  PLEASE WAIT
                </span>
              </div>
            ) : (
              <>
                {messages.map((message) => (
                  <ChatMessageBubble key={message.id} message={message} />
                ))}

                {isLoading && (
                  <div
                    style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                      color: FINCEPT.GRAY,
                      fontSize: '9px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      padding: '10px 12px',
                      marginTop: '8px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '2px'
                    }}
                  >
                    <div className="terminal-spinner" />
                    <span>PROCESSING QUERY...</span>
                  </div>
                )}

                <div ref={messagesEndRef} />
              </>
            )}
          </div>

          {/* Suggestions */}
          {!isInitializing && !isLoadingMessages && messages.length <= 1 && (
            <div style={{ flexShrink: 0 }}>
              <ChatSuggestions onSuggestionClick={handleSuggestionClick} />
            </div>
          )}

          {/* Input Area */}
          <div
            style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              flexShrink: 0
            }}
          >
            <div
              style={{
                display: 'flex',
                gap: '8px',
                alignItems: 'flex-end',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '8px'
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', paddingLeft: '4px' }}>
                <span style={{ color: FINCEPT.ORANGE, fontSize: '12px', fontWeight: 700 }}>{'>'}</span>
              </div>
              <textarea
                ref={inputRef}
                value={input}
                onChange={(e) => {
                  setInput(e.target.value);
                  const target = e.target as HTMLTextAreaElement;
                  target.style.height = '20px';
                  target.style.height = `${Math.min(target.scrollHeight, 100)}px`;
                }}
                onKeyDown={handleKeyPress}
                placeholder={isInitializing ? "CONNECTING..." : "ENTER QUERY..."}
                disabled={isLoading || isInitializing}
                style={{
                  flex: 1,
                  background: 'transparent',
                  border: 'none',
                  color: FINCEPT.WHITE,
                  fontSize: '11px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  resize: 'none',
                  outline: 'none',
                  height: '20px',
                  maxHeight: '100px',
                  lineHeight: '20px',
                  overflow: 'hidden'
                }}
                rows={1}
              />
              <button
                onClick={handleSendMessage}
                disabled={!input.trim() || isLoading || isInitializing}
                style={{
                  padding: '8px 16px',
                  backgroundColor: input.trim() && !isLoading && !isInitializing ? FINCEPT.ORANGE : FINCEPT.MUTED,
                  border: 'none',
                  borderRadius: '2px',
                  cursor: input.trim() && !isLoading && !isInitializing ? 'pointer' : 'not-allowed',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  color: FINCEPT.DARK_BG,
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                  transition: 'all 0.2s',
                  opacity: input.trim() && !isLoading && !isInitializing ? 1 : 0.5
                }}
              >
                <Send size={12} />
                SEND
              </button>
            </div>
            <div style={{
              fontSize: '9px',
              color: FINCEPT.GRAY,
              marginTop: '6px',
              display: 'flex',
              justifyContent: 'space-between',
              letterSpacing: '0.5px'
            }}>
              <span>ENTER TO SEND | SHIFT+ENTER FOR NEW LINE</span>
              <span>POWERED BY FINCEPT AI ENGINE</span>
            </div>
          </div>
        </div>
      </div>

      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 6px;
          height: 6px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: ${FINCEPT.DARK_BG};
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: ${FINCEPT.BORDER};
          border-radius: 3px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: ${FINCEPT.MUTED};
        }
        .terminal-spinner {
          width: 12px;
          height: 12px;
          border: 2px solid #333;
          border-top-color: #ea580c;
          border-radius: 50%;
          animation: spin 0.8s linear infinite;
        }
        .terminal-spinner-lg {
          width: 32px;
          height: 32px;
          border: 3px solid #333;
          border-top-color: #FF8800;
          border-radius: 50%;
          animation: spin 0.8s linear infinite;
        }
        @keyframes spin {
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

export default ChatModeInterface;
