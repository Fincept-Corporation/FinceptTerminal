// File: src/components/chat-mode/ChatModeInterface.tsx
// Bloomberg-style Chat Mode interface with professional terminal design

import React, { useState, useEffect, useRef } from 'react';
import { Send, Terminal, Activity, Zap, Database, TrendingUp } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { chatModeApiService, ChatModeMessage } from '@/services/data-sources/chatModeApi';
import ChatMessageBubble from './components/ChatMessageBubble';
import ChatSuggestions from './components/ChatSuggestions';

const ChatModeInterface: React.FC = () => {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const [messages, setMessages] = useState<ChatModeMessage[]>([]);
  const [input, setInput] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const [sessionStartTime] = useState(new Date());
  const [currentTime, setCurrentTime] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  useEffect(() => {
    // Initialize with welcome message
    const welcomeMessage: ChatModeMessage = {
      id: 'welcome',
      role: 'assistant',
      content: `**FINCEPT TERMINAL AI ASSISTANT v3.0**

**SYSTEM READY** | All modules operational | Real-time data feeds active

**AVAILABLE FUNCTIONS:**
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
• **MARKET DATA** - Real-time quotes, prices, charts
• **PORTFOLIO ANALYTICS** - Performance, risk, optimization
• **NEWS & RESEARCH** - Latest developments, analysis
• **FUNDAMENTAL DATA** - Financials, ratios, valuations
• **TECHNICAL ANALYSIS** - Indicators, patterns, signals
• **DERIVATIVES** - Options pricing, Greeks, volatility
• **ECONOMICS** - Macro data, indicators, forecasts
• **CUSTOM QUERIES** - Advanced financial modeling

**READY FOR QUERY** | Type your request below`,
      timestamp: new Date()
    };
    setMessages([welcomeMessage]);
  }, []);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const handleSendMessage = async () => {
    if (!input.trim() || isLoading) return;

    const userMessage: ChatModeMessage = {
      id: `user-${Date.now()}`,
      role: 'user',
      content: input.trim(),
      timestamp: new Date()
    };

    setMessages(prev => [...prev, userMessage]);
    setInput('');

    // Reset textarea height
    if (inputRef.current) {
      inputRef.current.style.height = '20px';
    }

    setIsLoading(true);

    try {
      const response = await chatModeApiService.quickChat(userMessage.content);

      if (response.success && response.data) {
        const assistantMessage: ChatModeMessage = {
          id: `assistant-${Date.now()}`,
          role: 'assistant',
          content: response.data.ai_response,
          timestamp: new Date(response.data.timestamp)
        };

        setMessages(prev => [...prev, assistantMessage]);
      } else {
        throw new Error(response.error || 'Failed to get response');
      }
    } catch (error) {
      console.error('Chat error:', error);
      const errorMessage: ChatModeMessage = {
        id: `error-${Date.now()}`,
        role: 'assistant',
        content: '**ERROR:** Unable to process request. System encountered an exception. Please retry or reformulate query.',
        timestamp: new Date()
      };
      setMessages(prev => [...prev, errorMessage]);
    } finally {
      setIsLoading(false);
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
        backgroundColor: '#000000',
        color: colors.text,
        fontFamily: 'Consolas, "Courier New", monospace',
        overflow: 'hidden'
      }}
    >
      {/* Bloomberg-style Header */}
      <div
        style={{
          padding: '8px 16px',
          borderBottom: '2px solid #ea580c',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          backgroundColor: '#0a0a0a',
          flexShrink: 0
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Terminal size={18} style={{ color: '#ea580c' }} />
            <span style={{ fontSize: '13px', fontWeight: 'bold', color: '#ea580c' }}>
              FINCEPT AI TERMINAL
            </span>
          </div>
          <div style={{ height: '16px', width: '1px', backgroundColor: '#333' }} />
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px', fontSize: '10px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Activity size={12} style={{ color: '#22c55e' }} />
              <span style={{ color: '#22c55e' }}>ONLINE</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Database size={12} style={{ color: '#06b6d4' }} />
              <span style={{ color: '#666' }}>REALTIME</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
              <Zap size={12} style={{ color: '#fbbf24' }} />
              <span style={{ color: '#666' }}>AI READY</span>
            </div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '10px' }}>
          <div style={{ color: '#666' }}>
            SESSION: <span style={{ color: colors.text }}>{sessionStartTime.toLocaleTimeString()}</span>
          </div>
          <div style={{ color: '#666' }}>
            QUERIES: <span style={{ color: colors.text }}>{Math.max(0, messages.length - 1)}</span>
          </div>
          <div style={{ color: '#666' }}>
            TIME: <span style={{ color: colors.text }}>{currentTime.toLocaleTimeString()}</span>
          </div>
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Sidebar - Quick Access */}
        <div
          style={{
            width: '220px',
            borderRight: '1px solid #1a1a1a',
            backgroundColor: '#0a0a0a',
            display: 'flex',
            flexDirection: 'column',
            flexShrink: 0
          }}
        >
          <div style={{ padding: '12px', borderBottom: '1px solid #1a1a1a' }}>
            <div style={{ fontSize: '11px', fontWeight: 'bold', color: '#ea580c', marginBottom: '8px' }}>
              QUICK ACCESS
            </div>
          </div>
          <div style={{ flex: 1, overflowY: 'auto' }}>
            {[
              { icon: TrendingUp, label: 'MARKET OVERVIEW', color: '#06b6d4' },
              { icon: Database, label: 'PORTFOLIO STATUS', color: '#22c55e' },
              { icon: Activity, label: 'ANALYTICS SUITE', color: '#fbbf24' },
              { icon: Terminal, label: 'CUSTOM QUERIES', color: '#a855f7' }
            ].map((item, idx) => (
              <div
                key={idx}
                style={{
                  padding: '10px 12px',
                  borderBottom: '1px solid #1a1a1a',
                  cursor: 'pointer',
                  transition: 'background 0.2s',
                  fontSize: '10px'
                }}
                onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1a1a1a'}
                onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <item.icon size={14} style={{ color: item.color }} />
                  <span style={{ color: '#999' }}>{item.label}</span>
                </div>
              </div>
            ))}
          </div>

          <div style={{ padding: '12px', borderTop: '1px solid #1a1a1a', fontSize: '9px' }}>
            <div style={{ color: '#666', marginBottom: '4px' }}>SYSTEM STATUS</div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
              <span style={{ color: '#666' }}>API:</span>
              <span style={{ color: '#22c55e' }}>CONNECTED</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '2px' }}>
              <span style={{ color: '#666' }}>LATENCY:</span>
              <span style={{ color: '#22c55e' }}>42ms</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: '#666' }}>UPTIME:</span>
              <span style={{ color: colors.text }}>99.9%</span>
            </div>
          </div>
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
              backgroundColor: '#000000'
            }}
            className="custom-scrollbar"
          >
            {messages.map((message) => (
              <ChatMessageBubble key={message.id} message={message} />
            ))}

            {isLoading && (
              <div
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '8px',
                  color: '#666',
                  fontSize: '11px',
                  padding: '12px',
                  marginTop: '8px',
                  backgroundColor: '#0a0a0a',
                  border: '1px solid #1a1a1a'
                }}
              >
                <div className="terminal-spinner" />
                <span>PROCESSING QUERY...</span>
              </div>
            )}

            <div ref={messagesEndRef} />
          </div>

          {/* Suggestions */}
          {messages.length <= 1 && (
            <div style={{ flexShrink: 0 }}>
              <ChatSuggestions onSuggestionClick={handleSuggestionClick} />
            </div>
          )}

          {/* Input Area */}
          <div
            style={{
              padding: '12px 16px',
              borderTop: '2px solid #1a1a1a',
              backgroundColor: '#0a0a0a',
              flexShrink: 0
            }}
          >
            <div
              style={{
                display: 'flex',
                gap: '8px',
                alignItems: 'flex-end',
                backgroundColor: '#000000',
                border: '1px solid #333',
                padding: '8px'
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', paddingLeft: '4px' }}>
                <span style={{ color: '#ea580c', fontSize: '12px', fontWeight: 'bold' }}>{'>'}</span>
              </div>
              <textarea
                ref={inputRef}
                value={input}
                onChange={(e) => {
                  setInput(e.target.value);
                  // Auto-resize
                  const target = e.target as HTMLTextAreaElement;
                  target.style.height = '20px';
                  target.style.height = `${Math.min(target.scrollHeight, 100)}px`;
                }}
                onKeyDown={handleKeyPress}
                placeholder="ENTER QUERY..."
                disabled={isLoading}
                style={{
                  flex: 1,
                  background: 'transparent',
                  border: 'none',
                  color: colors.text,
                  fontSize: '12px',
                  fontFamily: 'Consolas, "Courier New", monospace',
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
                disabled={!input.trim() || isLoading}
                style={{
                  background: input.trim() && !isLoading ? '#ea580c' : '#333',
                  border: 'none',
                  padding: '6px 12px',
                  cursor: input.trim() && !isLoading ? 'pointer' : 'not-allowed',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  color: input.trim() && !isLoading ? '#000' : '#666',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  fontFamily: 'Consolas, "Courier New", monospace',
                  transition: 'all 0.2s',
                  opacity: input.trim() && !isLoading ? 1 : 0.7
                }}
              >
                <Send size={12} />
                SEND
              </button>
            </div>
            <div style={{ fontSize: '9px', color: '#666', marginTop: '6px', display: 'flex', justifyContent: 'space-between' }}>
              <span>ENTER TO SEND | SHIFT+ENTER FOR NEW LINE</span>
              <span>POWERED BY FINCEPT AI ENGINE</span>
            </div>
          </div>
        </div>
      </div>

      <style>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 8px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: #0a0a0a;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: #333;
          border-radius: 4px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: #555;
        }
        .terminal-spinner {
          width: 12px;
          height: 12px;
          border: 2px solid #333;
          border-top-color: #ea580c;
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
