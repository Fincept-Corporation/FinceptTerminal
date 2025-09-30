import React, { useState, useEffect, useRef } from 'react';
import { Send, Plus, Search, Trash2, RefreshCw, MessageSquare, User, Bot, Clock, Hash, CheckCircle, AlertCircle } from 'lucide-react';

interface Message {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: Date;
}

interface ChatSession {
  session_uuid: string;
  title: string;
  message_count: number;
  updated_at: string;
  created_at: string;
}

const ChatTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [currentSessionUuid, setCurrentSessionUuid] = useState<string | null>(null);
  const [messages, setMessages] = useState<Message[]>([]);
  const [messageInput, setMessageInput] = useState('');
  const [sessionSearch, setSessionSearch] = useState('');
  const [isTyping, setIsTyping] = useState(false);
  const [chatCounter, setChatCounter] = useState(1);
  const [sessions, setSessions] = useState<ChatSession[]>([]);
  const [isConnected, setIsConnected] = useState(true);
  const [systemStatus, setSystemStatus] = useState('STATUS: READY');
  const [totalSessions, setTotalSessions] = useState(0);
  const [totalMessages, setTotalMessages] = useState(0);
  const [userType, setUserType] = useState('FREE');
  const [requestCount, setRequestCount] = useState(142);

  const messagesEndRef = useRef<HTMLDivElement>(null);

  // Bloomberg color scheme
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_YELLOW = '#FFFF00';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_BLUE = '#64B4FF';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  useEffect(() => {
    // Simulate loading sessions
    const mockSessions: ChatSession[] = [
      {
        session_uuid: '1',
        title: 'Market Analysis Discussion',
        message_count: 12,
        updated_at: new Date(Date.now() - 3600000).toISOString(),
        created_at: new Date(Date.now() - 7200000).toISOString()
      },
      {
        session_uuid: '2',
        title: 'Portfolio Optimization Help',
        message_count: 8,
        updated_at: new Date(Date.now() - 7200000).toISOString(),
        created_at: new Date(Date.now() - 10800000).toISOString()
      },
      {
        session_uuid: '3',
        title: 'Technical Analysis Guide',
        message_count: 15,
        updated_at: new Date(Date.now() - 86400000).toISOString(),
        created_at: new Date(Date.now() - 172800000).toISOString()
      }
    ];
    setSessions(mockSessions);
    setTotalSessions(mockSessions.length);
    setTotalMessages(mockSessions.reduce((sum, session) => sum + session.message_count, 0));
  }, []);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const handleSendMessage = () => {
    if (!messageInput.trim()) return;

    const userMessage: Message = {
      id: Date.now().toString(),
      role: 'user',
      content: messageInput.trim(),
      timestamp: new Date()
    };

    setMessages(prev => [...prev, userMessage]);
    setMessageInput('');
    setIsTyping(true);
    setSystemStatus('STATUS: SENDING MESSAGE...');

    // Simulate AI response
    setTimeout(() => {
      const aiMessage: Message = {
        id: (Date.now() + 1).toString(),
        role: 'assistant',
        content: generateAIResponse(userMessage.content),
        timestamp: new Date()
      };
      setMessages(prev => [...prev, aiMessage]);
      setIsTyping(false);
      setSystemStatus('STATUS: READY');
      setRequestCount(prev => prev + 1);
    }, 1500);
  };

  const generateAIResponse = (userInput: string): string => {
    const responses = [
      "I can help you with market analysis. Let me analyze the current market conditions and provide insights based on real-time data.",
      "Based on your portfolio composition, I recommend diversifying across different sectors to minimize risk while maximizing returns.",
      "The technical indicators suggest a bullish trend in the technology sector. Consider examining the RSI and MACD indicators for better entry points.",
      "For risk management, I suggest implementing stop-loss orders at 5-10% below your entry price depending on the volatility of the asset.",
      "Current market sentiment indicates increased volatility. It's advisable to reduce position sizes and focus on defensive stocks during this period."
    ];
    return responses[Math.floor(Math.random() * responses.length)];
  };

  const createNewSession = () => {
    const sessionTitle = messageInput.trim() ?
      generateSmartTitle(messageInput) :
      `Session-${chatCounter.toString().padStart(3, '0')}-${currentTime.getHours().toString().padStart(2, '0')}${currentTime.getMinutes().toString().padStart(2, '0')}`;

    const newSession: ChatSession = {
      session_uuid: Date.now().toString(),
      title: sessionTitle,
      message_count: 0,
      updated_at: new Date().toISOString(),
      created_at: new Date().toISOString()
    };

    setSessions(prev => [newSession, ...prev]);
    setCurrentSessionUuid(newSession.session_uuid);
    setMessages([]);
    setChatCounter(prev => prev + 1);
    setSystemStatus('STATUS: NEW SESSION CREATED');
  };

  const generateSmartTitle = (message: string): string => {
    const cleanMsg = message.replace(/[^\w\s]/g, '').trim();
    const words = cleanMsg.split(' ');

    if (words.length <= 2) {
      return cleanMsg.substring(0, 20) || 'New Chat';
    } else {
      return words.slice(0, 2).join(' ') + '...';
    }
  };

  const selectSession = (sessionUuid: string, title: string) => {
    setCurrentSessionUuid(sessionUuid);
    setSystemStatus('STATUS: LOADING SESSION...');

    // Simulate loading session messages
    setTimeout(() => {
      const mockMessages: Message[] = [
        {
          id: '1',
          role: 'user',
          content: 'Can you help me analyze the current market trends?',
          timestamp: new Date(Date.now() - 3600000)
        },
        {
          id: '2',
          role: 'assistant',
          content: 'I\'d be happy to help analyze current market trends. Based on recent data, we\'re seeing increased volatility in tech stocks while defensive sectors like utilities are showing stability.',
          timestamp: new Date(Date.now() - 3500000)
        }
      ];
      setMessages(mockMessages);
      setSystemStatus('STATUS: READY');
    }, 500);
  };

  const deleteSession = (sessionUuid: string) => {
    setSessions(prev => prev.filter(s => s.session_uuid !== sessionUuid));
    if (currentSessionUuid === sessionUuid) {
      setCurrentSessionUuid(null);
      setMessages([]);
    }
    setSystemStatus('STATUS: SESSION DELETED');
  };

  const clearCurrentChat = () => {
    setMessages([]);
    setSystemStatus('STATUS: CHAT CLEARED');
  };

  const filteredSessions = sessions.filter(session =>
    session.title.toLowerCase().includes(sessionSearch.toLowerCase())
  );

  const formatTime = (date: Date) => {
    return date.toLocaleTimeString('en-US', {
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
    } else {
      return date.toLocaleDateString('en-US', { month: '2-digit', day: '2-digit' });
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
      <div style={{ textAlign: 'center', padding: '40px' }}>
        <div style={{ color: BLOOMBERG_ORANGE, fontSize: '24px', fontWeight: 'bold', marginBottom: '8px' }}>
          FINCEPT AI ASSISTANT
        </div>
        <div style={{ color: BLOOMBERG_WHITE, fontSize: '16px', marginBottom: '24px' }}>
          Financial Intelligence System
        </div>

        <div style={{ display: 'flex', gap: '24px', justifyContent: 'center', marginBottom: '24px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <CheckCircle size={16} color={BLOOMBERG_GREEN} />
            <span style={{ color: BLOOMBERG_GREEN, fontSize: '14px' }}>AI Ready</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <CheckCircle size={16} color={BLOOMBERG_GREEN} />
            <span style={{ color: BLOOMBERG_GREEN, fontSize: '14px' }}>API Connected</span>
          </div>
        </div>

        <div style={{ color: userType === 'FREE' ? BLOOMBERG_YELLOW : BLOOMBERG_GREEN, fontSize: '14px', marginBottom: '24px' }}>
          Mode: {userType === 'FREE' ? 'Guest (Limited)' : 'Registered (Full Access)'}
        </div>

        <div style={{ color: BLOOMBERG_YELLOW, fontSize: '14px', marginBottom: '12px' }}>
          Quick Start:
        </div>
        <div style={{ color: BLOOMBERG_WHITE, fontSize: '12px', textAlign: 'left', marginBottom: '24px' }}>
          <div>• Type message below and press Enter</div>
          <div>• Use function keys for quick actions</div>
          <div>• Browse sessions in left panel</div>
        </div>

        <button
          onClick={createNewSession}
          style={{
            backgroundColor: BLOOMBERG_ORANGE,
            color: 'black',
            border: 'none',
            padding: '12px 24px',
            fontSize: '14px',
            fontWeight: 'bold',
            borderRadius: '4px',
            cursor: 'pointer'
          }}
        >
          START CHAT
        </button>
      </div>
    </div>
  );

  const renderMessage = (message: Message) => (
    <div key={message.id} style={{ marginBottom: '8px' }}>
      <div style={{
        display: 'flex',
        justifyContent: message.role === 'user' ? 'flex-end' : 'flex-start',
        marginBottom: '4px'
      }}>
        <div style={{
          maxWidth: '450px',
          minWidth: '120px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          border: `1px solid ${BLOOMBERG_GRAY}`,
          borderRadius: '4px',
          padding: '12px'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
            {message.role === 'user' ? (
              <User size={14} color={BLOOMBERG_YELLOW} />
            ) : (
              <Bot size={14} color={BLOOMBERG_ORANGE} />
            )}
            <span style={{
              color: message.role === 'user' ? BLOOMBERG_YELLOW : BLOOMBERG_ORANGE,
              fontSize: '12px',
              fontWeight: 'bold'
            }}>
              {message.role === 'user' ? 'YOU' : 'AI'}
            </span>
            <Clock size={12} color={BLOOMBERG_GRAY} />
            <span style={{ color: BLOOMBERG_GRAY, fontSize: '11px' }}>
              {formatTime(message.timestamp)}
            </span>
          </div>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '14px', lineHeight: '1.4' }}>
            {message.content}
          </div>
        </div>
      </div>
    </div>
  );

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Terminal Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '8px 12px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT</span>
          <span style={{ color: BLOOMBERG_WHITE }}>AI ASSISTANT</span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{ color: userType === 'FREE' ? BLOOMBERG_YELLOW : BLOOMBERG_GREEN }}>
            👤 {userType === 'FREE' ? 'Guest Mode' : 'User Mode'}
          </span>
          <span style={{ color: BLOOMBERG_GRAY }}>|</span>
          <span style={{ color: BLOOMBERG_WHITE }}>
            {currentTime.toISOString().replace('T', ' ').substring(0, 19)}
          </span>
        </div>
      </div>

      {/* Function Keys */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 8px'
      }}>
        <div style={{ display: 'flex', gap: '4px' }}>
          {[
            { key: 'F1:HELP', action: () => setMessageInput('I need help with using this AI assistant') },
            { key: 'F2:SESSIONS', action: () => document.getElementById('session-search')?.focus() },
            { key: 'F3:NEW', action: createNewSession },
            { key: 'F4:SEARCH', action: () => document.getElementById('session-search')?.focus() },
            { key: 'F5:CLEAR', action: clearCurrentChat },
            { key: 'F6:STATS', action: () => setSystemStatus(`STATS: Sessions: ${totalSessions}, Messages: ${totalMessages}`) }
          ].map(item => (
            <button
              key={item.key}
              onClick={item.action}
              style={{
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '4px 8px',
                fontSize: '10px',
                height: '25px',
                cursor: 'pointer'
              }}
            >
              {item.key}
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Chat Sessions */}
        <div style={{
          width: '350px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG_GRAY}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{ padding: '12px' }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
              CHAT SESSIONS
            </div>

            {/* Controls */}
            <div style={{ display: 'flex', gap: '8px', marginBottom: '12px' }}>
              <button
                onClick={createNewSession}
                style={{
                  backgroundColor: BLOOMBERG_ORANGE,
                  color: 'black',
                  border: 'none',
                  padding: '6px 12px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                NEW
              </button>
              <button
                onClick={() => setSystemStatus('STATUS: SESSIONS REFRESHED')}
                style={{
                  backgroundColor: BLOOMBERG_DARK_BG,
                  color: BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  padding: '6px 12px',
                  fontSize: '11px',
                  cursor: 'pointer'
                }}
              >
                REFRESH
              </button>
              <button
                onClick={() => currentSessionUuid && deleteSession(currentSessionUuid)}
                style={{
                  backgroundColor: BLOOMBERG_RED,
                  color: BLOOMBERG_WHITE,
                  border: 'none',
                  padding: '6px 12px',
                  fontSize: '11px',
                  cursor: 'pointer'
                }}
              >
                DELETE
              </button>
            </div>

            {/* Search */}
            <input
              id="session-search"
              type="text"
              placeholder="Search sessions..."
              value={sessionSearch}
              onChange={(e) => setSessionSearch(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '12px',
                marginBottom: '12px'
              }}
            />

            {/* Stats */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                SESSION STATISTICS
              </div>
              <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>Total Sessions: {totalSessions}</div>
              <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>Total Messages: {totalMessages}</div>
              <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
                Active Session: {currentSessionUuid ? sessions.find(s => s.session_uuid === currentSessionUuid)?.title || 'None' : 'None'}
              </div>
            </div>

            <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              ACTIVE SESSIONS
            </div>
          </div>

          {/* Session List */}
          <div style={{ flex: 1, overflow: 'auto', padding: '0 12px 12px' }}>
            {filteredSessions.map(session => (
              <div
                key={session.session_uuid}
                style={{
                  backgroundColor: currentSessionUuid === session.session_uuid ? BLOOMBERG_ORANGE + '20' : BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  padding: '8px',
                  marginBottom: '4px',
                  cursor: 'pointer'
                }}
                onClick={() => selectSession(session.session_uuid, session.title)}
              >
                <div style={{ color: BLOOMBERG_WHITE, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                  {session.title}
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                  <div style={{ display: 'flex', gap: '12px' }}>
                    <span style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>
                      Msgs: {session.message_count}
                    </span>
                    <span style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>
                      {formatSessionTime(session.updated_at)}
                    </span>
                  </div>
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      deleteSession(session.session_uuid);
                    }}
                    style={{
                      backgroundColor: 'transparent',
                      color: BLOOMBERG_RED,
                      border: 'none',
                      fontSize: '10px',
                      cursor: 'pointer',
                      padding: '2px'
                    }}
                  >
                    DEL
                  </button>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Center Panel - Chat Interface */}
        <div style={{
          width: '850px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG_GRAY}`,
          display: 'flex',
          flexDirection: 'column'
        }}>
          {/* Chat Header */}
          <div style={{ padding: '12px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold' }}>AI CHAT</span>
              <span style={{ color: BLOOMBERG_GRAY }}>|</span>
              <span style={{ color: BLOOMBERG_WHITE, fontSize: '12px' }}>
                {currentSessionUuid ? sessions.find(s => s.session_uuid === currentSessionUuid)?.title || 'No Active Session' : 'No Active Session'}
              </span>
            </div>
          </div>

          {/* Messages Area */}
          <div style={{
            flex: 1,
            padding: '12px',
            overflow: 'auto',
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            margin: '0 12px'
          }}>
            {messages.length === 0 ? renderWelcomeScreen() : (
              <div>
                {messages.map(renderMessage)}
                {isTyping && (
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', color: BLOOMBERG_GRAY }}>
                    <Bot size={14} />
                    <span>AI is typing...</span>
                  </div>
                )}
                <div ref={messagesEndRef} />
              </div>
            )}
          </div>

          {/* Input Area */}
          <div style={{ padding: '12px' }}>
            <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              INPUT
            </div>
            <div style={{ display: 'flex', gap: '8px' }}>
              <textarea
                value={messageInput}
                onChange={(e) => setMessageInput(e.target.value)}
                onKeyPress={(e) => {
                  if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    handleSendMessage();
                  }
                }}
                placeholder="Type message..."
                style={{
                  flex: 1,
                  backgroundColor: BLOOMBERG_DARK_BG,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '8px',
                  fontSize: '12px',
                  resize: 'none',
                  height: '40px'
                }}
              />
              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                <button
                  onClick={handleSendMessage}
                  style={{
                    backgroundColor: BLOOMBERG_ORANGE,
                    color: 'black',
                    border: 'none',
                    padding: '8px 16px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  SEND
                </button>
                <button
                  onClick={() => setMessageInput('')}
                  style={{
                    backgroundColor: BLOOMBERG_DARK_BG,
                    color: BLOOMBERG_WHITE,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    padding: '6px 16px',
                    fontSize: '10px',
                    cursor: 'pointer'
                  }}
                >
                  CLEAR
                </button>
              </div>
            </div>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '11px', marginTop: '4px' }}>
              {messageInput.length > 0 ? `${messageInput.length} chars` : 'Ready'}
            </div>
          </div>
        </div>

        {/* Right Panel - Command Center */}
        <div style={{
          width: '300px',
          backgroundColor: BLOOMBERG_PANEL_BG,
          padding: '12px',
          overflow: 'auto'
        }}>
          <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
            COMMAND CENTER
          </div>

          {/* API Status */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              API STATUS
            </div>
            <div style={{ color: BLOOMBERG_GREEN, fontSize: '11px', marginBottom: '2px' }}>
              ● Connected
            </div>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '2px' }}>
              ● {userType} User
            </div>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
              ● Requests: {requestCount}
            </div>
          </div>

          {/* Quick Commands */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              QUICK COMMANDS
            </div>
            {[
              { cmd: 'NEW_SESSION', desc: 'Create new chat session', action: createNewSession },
              { cmd: 'GET_HELP', desc: 'Get AI assistance', action: () => setMessageInput('I need help with using this AI assistant') },
              { cmd: 'MARKET_ANALYSIS', desc: 'Market insights', action: () => setMessageInput('Can you help me with market analysis?') },
              { cmd: 'PORTFOLIO_HELP', desc: 'Portfolio advice', action: () => setMessageInput('I need advice on portfolio management') }
            ].map(item => (
              <div key={item.cmd} style={{ marginBottom: '8px' }}>
                <button
                  onClick={item.action}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    color: BLOOMBERG_WHITE,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    padding: '6px',
                    fontSize: '10px',
                    textAlign: 'left',
                    cursor: 'pointer'
                  }}
                >
                  {item.cmd}
                </button>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', padding: '2px 0' }}>
                  {item.desc}
                </div>
              </div>
            ))}
          </div>

          {/* System Info */}
          <div>
            <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              SYSTEM INFO
            </div>
            <div style={{ color: BLOOMBERG_GREEN, fontSize: '11px', marginBottom: '2px' }}>
              Chat API: READY
            </div>
            <div style={{ color: BLOOMBERG_GREEN, fontSize: '11px', marginBottom: '2px' }}>
              Response Time: &lt;100ms
            </div>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '11px', marginBottom: '2px' }}>
              Last Update:
            </div>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px' }}>
              {formatTime(currentTime)}
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '16px',
        fontSize: '11px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <div style={{ color: BLOOMBERG_GREEN }}>●</div>
          <span style={{ color: BLOOMBERG_GREEN }}>CONNECTED</span>
        </div>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_ORANGE }}>AI CHAT</span>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_WHITE }}>{systemStatus}</span>
        <span style={{ color: BLOOMBERG_GRAY }}>|</span>
        <span style={{ color: BLOOMBERG_WHITE }}>USER: {userType}</span>
      </div>
    </div>
  );
};

export default ChatTab;