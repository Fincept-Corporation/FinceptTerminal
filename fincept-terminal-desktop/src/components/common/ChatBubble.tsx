// Chat Bubble - Floating AI assistant with voice mode
// Syncs with AI Chat tab, connected to Internal MCP
// Voice settings configured in Settings > AI Chat Bubble
// Supports multiple TTS providers: Browser, Edge TTS, ElevenLabs, OpenAI

import React, { useReducer, useEffect, useRef, useCallback } from 'react';
import { X, Send, Plus, Minimize2, Bot, User, Mic, MicOff, Volume2, VolumeX, Square } from 'lucide-react';
import { sqliteService, ChatMessage } from '@/services/core/sqliteService';
import { llmApiService } from '@/services/chat/llmApi';
import { sanitizeInput } from '@/services/core/validators';
import MarkdownRenderer from '@/components/common/MarkdownRenderer';
import { invoke } from '@tauri-apps/api/core';
import { ttsService, TTSProvider, TTSConfig } from '@/services/voice/ttsService';

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
  MUTED: '#4A4A4A',
};

// State types
interface BubbleState {
  isOpen: boolean;
  messages: ChatMessage[];
  input: string;
  isTyping: boolean;
  streamingContent: string;
  currentSessionUuid: string | null;
  unreadCount: number;
  isListening: boolean;
  isSpeaking: boolean;
  voiceMode: boolean;
  activeToolCalls: Array<{ name: string; status: string }>;
  // TTS settings
  ttsProvider: TTSProvider;
  selectedVoice: string;
  speechRate: number;
  autoStartVoice: boolean;
  elevenLabsApiKey: string;
  openaiApiKey: string;
}

type BubbleAction =
  | { type: 'SET_OPEN'; payload: boolean }
  | { type: 'SET_MESSAGES'; payload: ChatMessage[] }
  | { type: 'ADD_MESSAGE'; payload: ChatMessage }
  | { type: 'SET_INPUT'; payload: string }
  | { type: 'SET_TYPING'; payload: boolean }
  | { type: 'SET_STREAMING'; payload: string }
  | { type: 'SET_SESSION'; payload: string | null }
  | { type: 'SET_UNREAD'; payload: number }
  | { type: 'SET_LISTENING'; payload: boolean }
  | { type: 'SET_SPEAKING'; payload: boolean }
  | { type: 'SET_VOICE_MODE'; payload: boolean }
  | { type: 'SET_TOOL_CALLS'; payload: Array<{ name: string; status: string }> }
  | { type: 'SET_TTS_SETTINGS'; payload: { provider: TTSProvider; voice: string; rate: number; autoStart: boolean; elevenLabsKey: string; openaiKey: string } }
  | { type: 'RESET_CHAT' };

const initialState: BubbleState = {
  isOpen: false,
  messages: [],
  input: '',
  isTyping: false,
  streamingContent: '',
  currentSessionUuid: null,
  unreadCount: 0,
  isListening: false,
  isSpeaking: false,
  voiceMode: false,
  activeToolCalls: [],
  ttsProvider: 'browser',
  selectedVoice: '',
  speechRate: 1.4,
  autoStartVoice: false,
  elevenLabsApiKey: '',
  openaiApiKey: '',
};

function bubbleReducer(state: BubbleState, action: BubbleAction): BubbleState {
  switch (action.type) {
    case 'SET_OPEN': return { ...state, isOpen: action.payload, unreadCount: action.payload ? 0 : state.unreadCount };
    case 'SET_MESSAGES': return { ...state, messages: action.payload };
    case 'ADD_MESSAGE': return { ...state, messages: [...state.messages, action.payload] };
    case 'SET_INPUT': return { ...state, input: action.payload };
    case 'SET_TYPING': return { ...state, isTyping: action.payload };
    case 'SET_STREAMING': return { ...state, streamingContent: action.payload };
    case 'SET_SESSION': return { ...state, currentSessionUuid: action.payload };
    case 'SET_UNREAD': return { ...state, unreadCount: action.payload };
    case 'SET_LISTENING': return { ...state, isListening: action.payload };
    case 'SET_SPEAKING': return { ...state, isSpeaking: action.payload };
    case 'SET_VOICE_MODE': return { ...state, voiceMode: action.payload };
    case 'SET_TOOL_CALLS': return { ...state, activeToolCalls: action.payload };
    case 'SET_TTS_SETTINGS': return { ...state, ttsProvider: action.payload.provider, selectedVoice: action.payload.voice, speechRate: action.payload.rate, autoStartVoice: action.payload.autoStart, elevenLabsApiKey: action.payload.elevenLabsKey, openaiApiKey: action.payload.openaiKey };
    case 'RESET_CHAT': return { ...state, messages: [], currentSessionUuid: null, input: '' };
    default: return state;
  }
}

interface ChatBubbleProps {
  onNavigateToTab?: (tabName: string) => void;
}

const ChatBubble: React.FC<ChatBubbleProps> = ({ onNavigateToTab }) => {
  const [state, dispatch] = useReducer(bubbleReducer, initialState);
  const { isOpen, messages, input, isTyping, streamingContent, currentSessionUuid, unreadCount, isListening, isSpeaking, voiceMode, activeToolCalls, ttsProvider, selectedVoice, speechRate, autoStartVoice, elevenLabsApiKey, openaiApiKey } = state;

  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const streamedContentRef = useRef('');
  const recognitionRef = useRef<any>(null);
  const finalTranscriptRef = useRef('');

  // CRITICAL: Use refs for voice state to avoid stale closures
  const voiceModeRef = useRef(false);
  const isListeningRef = useRef(false);
  const isSpeakingRef = useRef(false);
  const isTypingRef = useRef(false);
  const sessionUuidRef = useRef<string | null>(null);
  const messagesRef = useRef<ChatMessage[]>([]);
  // TTS settings refs
  const ttsProviderRef = useRef<TTSProvider>('browser');
  const selectedVoiceRef = useRef('');
  const speechRateRef = useRef(1.4);
  const elevenLabsApiKeyRef = useRef('');
  const openaiApiKeyRef = useRef('');

  // Keep refs in sync with state
  useEffect(() => { voiceModeRef.current = voiceMode; }, [voiceMode]);
  useEffect(() => { isListeningRef.current = isListening; }, [isListening]);
  useEffect(() => { isSpeakingRef.current = isSpeaking; }, [isSpeaking]);
  useEffect(() => { isTypingRef.current = isTyping; }, [isTyping]);
  useEffect(() => { sessionUuidRef.current = currentSessionUuid; }, [currentSessionUuid]);
  useEffect(() => { messagesRef.current = messages; }, [messages]);
  useEffect(() => { ttsProviderRef.current = ttsProvider; }, [ttsProvider]);
  useEffect(() => { selectedVoiceRef.current = selectedVoice; }, [selectedVoice]);
  useEffect(() => { speechRateRef.current = speechRate; }, [speechRate]);
  useEffect(() => { elevenLabsApiKeyRef.current = elevenLabsApiKey; }, [elevenLabsApiKey]);
  useEffect(() => { openaiApiKeyRef.current = openaiApiKey; }, [openaiApiKey]);

  // Load session and settings
  useEffect(() => {
    const load = async () => {
      await sqliteService.initialize();

      // Load chat session
      const sessions = await sqliteService.getChatSessions(1);
      if (sessions.length > 0) {
        dispatch({ type: 'SET_SESSION', payload: sessions[0].session_uuid });
        const msgs = await sqliteService.getChatMessages(sessions[0].session_uuid);
        dispatch({ type: 'SET_MESSAGES', payload: msgs.slice(-50) });
      }

      // Load TTS settings from DB
      try {
        const [provider, voice, rate, autoVoice, elevenKey, openaiKey] = await Promise.all([
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_tts_provider' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_voice' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_speech_rate' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_auto_voice' }),
          invoke<string | null>('db_get_setting', { key: 'elevenlabs_api_key' }),
          invoke<string | null>('db_get_setting', { key: 'openai_tts_api_key' }),
        ]);

        dispatch({
          type: 'SET_TTS_SETTINGS',
          payload: {
            provider: (provider as TTSProvider) || 'browser',
            voice: voice || '',
            rate: rate ? parseFloat(rate) : 1.4,
            autoStart: autoVoice === 'true',
            elevenLabsKey: elevenKey || '',
            openaiKey: openaiKey || '',
          }
        });
      } catch (error) {
        console.error('[ChatBubble] Failed to load TTS settings:', error);
      }
    };
    load();
  }, []);

  // Check session exists when opening
  useEffect(() => {
    if (isOpen && currentSessionUuid) {
      sqliteService.getChatMessages(currentSessionUuid).then(msgs => {
        if (msgs.length === 0) {
          dispatch({ type: 'RESET_CHAT' });
        } else {
          dispatch({ type: 'SET_MESSAGES', payload: msgs.slice(-50) });
        }
      });
    }
  }, [isOpen, currentSessionUuid]);

  // Auto-scroll
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages, streamingContent]);

  // Create session
  const createSession = async (): Promise<string | null> => {
    try {
      const session = await sqliteService.createChatSession(`Chat ${new Date().toLocaleTimeString()}`);
      dispatch({ type: 'SET_SESSION', payload: session.session_uuid });
      dispatch({ type: 'SET_MESSAGES', payload: [] });
      return session.session_uuid;
    } catch { return null; }
  };

  // ttsService handles all voice management internally

  // Start listening - uses refs to avoid stale closures
  const startListening = useCallback(() => {
    console.log('[Voice] startListening called');

    const SR = (window as any).SpeechRecognition || (window as any).webkitSpeechRecognition;
    if (!SR) {
      console.error('[Voice] SpeechRecognition not supported');
      return;
    }

    if (isListeningRef.current || isSpeakingRef.current) return;

    const recognition = new SR();
    recognition.continuous = false;
    recognition.interimResults = true;
    recognition.lang = 'en-US';
    recognition.maxAlternatives = 1;
    finalTranscriptRef.current = '';

    recognition.onstart = () => {
      console.log('[Voice] Recognition started');
      isListeningRef.current = true;
      dispatch({ type: 'SET_LISTENING', payload: true });
    };

    recognition.onend = () => {
      console.log('[Voice] Recognition ended, transcript:', finalTranscriptRef.current, 'voiceMode:', voiceModeRef.current);
      isListeningRef.current = false;
      dispatch({ type: 'SET_LISTENING', payload: false });

      const text = finalTranscriptRef.current.trim();

      // Only auto-send if voice mode is active
      if (voiceModeRef.current) {
        if (text) {
          dispatch({ type: 'SET_INPUT', payload: '' });
          sendMessageDirect(text);
        } else {
          // No speech detected, restart listening in voice mode
          setTimeout(() => {
            if (voiceModeRef.current && !isSpeakingRef.current && !isTypingRef.current) {
              startListening();
            }
          }, 500);
        }
      }
      // If voice mode is OFF, just leave the text in the input field - user can manually send
    };

    recognition.onerror = (e: any) => {
      console.error('[Voice] Recognition error:', e.error);
      isListeningRef.current = false;
      dispatch({ type: 'SET_LISTENING', payload: false });

      // Only auto-restart if voice mode is active
      if (e.error === 'no-speech' && voiceModeRef.current) {
        setTimeout(() => {
          if (voiceModeRef.current && !isSpeakingRef.current && !isTypingRef.current) {
            startListening();
          }
        }, 500);
      }
    };

    recognition.onresult = (e: any) => {
      let final = '';
      let interim = '';
      for (let i = 0; i < e.results.length; i++) {
        const transcript = e.results[i][0].transcript;
        if (e.results[i].isFinal) {
          final += transcript;
        } else {
          interim += transcript;
        }
      }
      // Remove trailing period that speech recognition often adds
      if (final) {
        final = final.replace(/\.\s*$/, '').trim();
        finalTranscriptRef.current = final;
      }
      if (interim) {
        interim = interim.replace(/\.\s*$/, '').trim();
      }
      dispatch({ type: 'SET_INPUT', payload: final || interim });
    };

    recognitionRef.current = recognition;

    try {
      recognition.start();
    } catch (err) {
      console.error('[Voice] Failed to start recognition:', err);
    }
  }, []);

  // Direct send message - doesn't depend on state
  const sendMessageDirect = useCallback(async (text: string) => {
    const sanitized = sanitizeInput(text).trim();
    if (!sanitized || isTypingRef.current) return;

    let sessionUuid = sessionUuidRef.current;
    if (!sessionUuid) {
      const session = await sqliteService.createChatSession(`Chat ${new Date().toLocaleTimeString()}`);
      sessionUuid = session.session_uuid;
      dispatch({ type: 'SET_SESSION', payload: sessionUuid });
      dispatch({ type: 'SET_MESSAGES', payload: [] });
    }

    isTypingRef.current = true;
    dispatch({ type: 'SET_TYPING', payload: true });
    dispatch({ type: 'SET_STREAMING', payload: '' });
    streamedContentRef.current = '';

    try {
      const userMsg = await sqliteService.addChatMessage({ session_uuid: sessionUuid, role: 'user', content: sanitized });
      dispatch({ type: 'ADD_MESSAGE', payload: userMsg });

      const { mcpToolService } = await import('@/services/mcp/mcpToolService');
      const tools = await mcpToolService.getAllTools();
      const history = messagesRef.current.slice(-10).map(m => ({ role: m.role as 'user' | 'assistant', content: m.content }));

      const onStream = (chunk: string) => {
        streamedContentRef.current += chunk;
        dispatch({ type: 'SET_STREAMING', payload: streamedContentRef.current });
      };

      const response = tools.length > 0
        ? await llmApiService.chatWithTools(sanitized, history, tools, onStream, async () => {})
        : await llmApiService.chat(sanitized, history, onStream);

      const content = response.content || streamedContentRef.current || '(No response)';
      const assistantMsg = await sqliteService.addChatMessage({ session_uuid: sessionUuid, role: 'assistant', content });
      dispatch({ type: 'ADD_MESSAGE', payload: assistantMsg });

      isTypingRef.current = false;
      dispatch({ type: 'SET_TYPING', payload: false });
      dispatch({ type: 'SET_STREAMING', payload: '' });

      // Text-to-speech in voice mode using ttsService
      if (voiceModeRef.current && content) {
        isSpeakingRef.current = true;
        dispatch({ type: 'SET_SPEAKING', payload: true });

        try {
          const ttsConfig: TTSConfig = {
            provider: ttsProviderRef.current,
            voiceId: selectedVoiceRef.current,
            rate: speechRateRef.current,
            elevenLabsApiKey: elevenLabsApiKeyRef.current || undefined,
            openaiApiKey: openaiApiKeyRef.current || undefined,
          };

          await ttsService.speak(content, ttsConfig);

          isSpeakingRef.current = false;
          dispatch({ type: 'SET_SPEAKING', payload: false });

          // Auto-restart listening after speech completes
          if (voiceModeRef.current) {
            setTimeout(() => {
              if (voiceModeRef.current && !isListeningRef.current && !isSpeakingRef.current) {
                startListening();
              }
            }, 400);
          }
        } catch (error) {
          console.error('[TTS] Error:', error);
          isSpeakingRef.current = false;
          dispatch({ type: 'SET_SPEAKING', payload: false });

          // Still restart listening even if TTS fails
          if (voiceModeRef.current) {
            setTimeout(() => startListening(), 400);
          }
        }
      }
    } catch (error) {
      console.error('[Chat] Error:', error);
      if (sessionUuid) {
        const errorMsg = await sqliteService.addChatMessage({ session_uuid: sessionUuid, role: 'assistant', content: `Error: ${error}` });
        dispatch({ type: 'ADD_MESSAGE', payload: errorMsg });
      }
      isTypingRef.current = false;
      dispatch({ type: 'SET_TYPING', payload: false });
      dispatch({ type: 'SET_STREAMING', payload: '' });

      if (voiceModeRef.current) setTimeout(() => startListening(), 400);
    }
  }, [startListening]);

  // Toggle voice mode
  const toggleVoiceMode = useCallback(() => {
    if (!voiceModeRef.current) {
      voiceModeRef.current = true;
      dispatch({ type: 'SET_VOICE_MODE', payload: true });
      setTimeout(() => {
        if (voiceModeRef.current && !isListeningRef.current && !isSpeakingRef.current) {
          startListening();
        }
      }, 300);
    } else {
      voiceModeRef.current = false;
      if (recognitionRef.current) {
        try { recognitionRef.current.abort(); } catch {}
      }
      ttsService.stop(); // Stop any ongoing TTS
      isListeningRef.current = false;
      isSpeakingRef.current = false;
      dispatch({ type: 'SET_VOICE_MODE', payload: false });
      dispatch({ type: 'SET_LISTENING', payload: false });
      dispatch({ type: 'SET_SPEAKING', payload: false });
    }
  }, [startListening]);

  // Toggle mic (manual mode)
  const toggleMic = useCallback(() => {
    if (isListeningRef.current) {
      if (recognitionRef.current) {
        try { recognitionRef.current.stop(); } catch {}
      }
    } else {
      startListening();
    }
  }, [startListening]);

  // Stop speech and allow new query
  const stopSpeech = useCallback(() => {
    console.log('[Voice] Stopping speech');
    ttsService.stop();
    isSpeakingRef.current = false;
    dispatch({ type: 'SET_SPEAKING', payload: false });

    // Restart listening so user can provide new query
    if (voiceModeRef.current) {
      setTimeout(() => {
        if (voiceModeRef.current && !isListeningRef.current && !isSpeakingRef.current && !isTypingRef.current) {
          startListening();
        }
      }, 200);
    }
  }, [startListening]);

  // Handle send button
  const handleSend = useCallback(() => {
    const text = input.trim();
    if (text && !isTyping) {
      dispatch({ type: 'SET_INPUT', payload: '' });
      sendMessageDirect(text);
    }
  }, [input, isTyping, sendMessageDirect]);

  // Handle enter key
  const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  }, [handleSend]);

  // Handle open - auto-start voice if enabled
  const handleOpen = useCallback(() => {
    dispatch({ type: 'SET_OPEN', payload: true });
    if (autoStartVoice && !voiceModeRef.current) {
      setTimeout(() => toggleVoiceMode(), 500);
    }
  }, [autoStartVoice, toggleVoiceMode]);

  return (
    <>
      <style>{`
        @keyframes morphGlow {
          0%, 100% { box-shadow: 0 0 20px ${FINCEPT.ORANGE}40, inset 0 0 15px ${FINCEPT.ORANGE}30; }
          50% { box-shadow: 0 0 30px ${FINCEPT.ORANGE}60, inset 0 0 25px ${FINCEPT.ORANGE}50; transform: scale(1.05); }
        }
        @keyframes pulse { 0%, 100% { transform: scale(1); } 50% { transform: scale(1.1); } }
        @keyframes slideUp { from { opacity: 0; transform: translateY(20px); } to { opacity: 1; transform: translateY(0); } }
        .bubble-scroll::-webkit-scrollbar { width: 4px; }
        .bubble-scroll::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 2px; }
      `}</style>

      {/* Chat Panel */}
      {isOpen && (
        <div style={{
          position: 'fixed', bottom: '100px', right: '20px', width: '380px', height: '420px',
          backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.ORANGE}`, borderRadius: '8px',
          display: 'flex', flexDirection: 'column', zIndex: 9999, animation: 'slideUp 0.25s ease-out',
          boxShadow: `0 8px 32px ${FINCEPT.DARK_BG}CC`, fontFamily: '"IBM Plex Mono", monospace', fontSize: '10px',
        }}>
          {/* Header */}
          <div style={{ padding: '10px 12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.ORANGE}`, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Bot size={14} color={FINCEPT.ORANGE} />
              <span style={{ color: FINCEPT.ORANGE, fontWeight: 700 }}>FINCEPT AI</span>
            </div>
            <div style={{ display: 'flex', gap: '6px' }}>
              <button onClick={toggleVoiceMode} style={{ background: voiceMode ? `${FINCEPT.GREEN}20` : 'transparent', border: `1px solid ${voiceMode ? FINCEPT.GREEN : FINCEPT.BORDER}`, color: voiceMode ? FINCEPT.GREEN : FINCEPT.GRAY, padding: '4px 8px', borderRadius: '2px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', fontSize: '8px', fontWeight: 700 }} title="Voice Mode">
                {voiceMode ? <Volume2 size={10} /> : <VolumeX size={10} />} VOICE
              </button>
              <button onClick={() => createSession()} style={{ background: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, padding: '4px 8px', borderRadius: '2px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', fontSize: '8px', fontWeight: 700 }} title="New Chat">
                <Plus size={10} /> NEW
              </button>
              <button onClick={() => dispatch({ type: 'SET_OPEN', payload: false })} style={{ background: 'transparent', border: 'none', color: FINCEPT.GRAY, padding: '4px', cursor: 'pointer' }} title="Minimize">
                <Minimize2 size={14} />
              </button>
            </div>
          </div>

          {/* Messages */}
          <div className="bubble-scroll" style={{ flex: 1, overflow: 'auto', padding: '12px', backgroundColor: FINCEPT.PANEL_BG }}>
            {messages.length === 0 && !streamingContent ? (
              <div style={{ height: '100%', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', color: FINCEPT.MUTED, textAlign: 'center' }}>
                <Bot size={28} style={{ marginBottom: '10px', opacity: 0.6 }} />
                <div style={{ fontWeight: 700, color: FINCEPT.GRAY }}>How can I help?</div>
                <div style={{ fontSize: '9px', marginTop: '4px' }}>Ask me anything about markets or trading.</div>
              </div>
            ) : (
              <>
                {messages.map((msg) => (
                  <div key={msg.id} style={{ marginBottom: '10px', display: 'flex', flexDirection: 'column', alignItems: msg.role === 'user' ? 'flex-end' : 'flex-start' }}>
                    <div style={{ maxWidth: '90%', padding: '8px 10px', backgroundColor: msg.role === 'user' ? `${FINCEPT.ORANGE}20` : FINCEPT.DARK_BG, border: `1px solid ${msg.role === 'user' ? FINCEPT.ORANGE : FINCEPT.BORDER}`, borderRadius: '4px' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '4px', opacity: 0.7 }}>
                        {msg.role === 'user' ? <User size={8} /> : <Bot size={8} />}
                        <span style={{ fontSize: '8px', fontWeight: 700 }}>{msg.role === 'user' ? 'YOU' : 'AI'}</span>
                      </div>
                      {msg.role === 'assistant' ? <MarkdownRenderer content={msg.content} /> : <div style={{ color: FINCEPT.WHITE, lineHeight: 1.4, whiteSpace: 'pre-wrap' }}>{msg.content}</div>}
                    </div>
                  </div>
                ))}
                {(streamingContent || isTyping) && (
                  <div style={{ marginBottom: '10px' }}>
                    <div style={{ maxWidth: '90%', padding: '8px 10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.ORANGE}`, borderRadius: '4px' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '4px' }}>
                        <Bot size={8} />
                        <span style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE }}>TYPING...</span>
                      </div>
                      {streamingContent && <div style={{ color: FINCEPT.WHITE }}>{streamingContent}<span style={{ color: FINCEPT.ORANGE, animation: 'pulse 0.5s infinite' }}>▊</span></div>}
                    </div>
                  </div>
                )}
                <div ref={messagesEndRef} />
              </>
            )}
          </div>

          {/* Input */}
          <div style={{ padding: '10px 12px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, display: 'flex', gap: '8px' }}>
            <textarea ref={inputRef} value={input} onChange={(e) => dispatch({ type: 'SET_INPUT', payload: e.target.value })} onKeyDown={handleKeyDown} placeholder={voiceMode ? "Voice mode active - speak to send" : "Ask anything..."} style={{ flex: 1, padding: '8px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${isListening ? FINCEPT.GREEN : FINCEPT.BORDER}`, borderRadius: '4px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', resize: 'none', height: '36px', outline: 'none' }} />
            <button onClick={toggleMic} style={{ padding: '8px', backgroundColor: isListening ? FINCEPT.RED : 'transparent', color: isListening ? FINCEPT.WHITE : FINCEPT.GRAY, border: `1px solid ${isListening ? FINCEPT.RED : FINCEPT.BORDER}`, borderRadius: '4px', cursor: 'pointer', animation: isListening ? 'pulse 1s infinite' : 'none' }} title={isListening ? 'Stop' : 'Voice input'}>
              {isListening ? <MicOff size={14} /> : <Mic size={14} />}
            </button>
            <button id="chat-bubble-send-btn" onClick={handleSend} disabled={!input.trim() || isTyping} style={{ padding: '8px 12px', backgroundColor: input.trim() && !isTyping ? FINCEPT.ORANGE : FINCEPT.MUTED, color: FINCEPT.DARK_BG, border: 'none', borderRadius: '4px', cursor: input.trim() && !isTyping ? 'pointer' : 'not-allowed', opacity: input.trim() && !isTyping ? 1 : 0.5 }}>
              <Send size={14} />
            </button>
          </div>

          {/* Voice Status Bar */}
          {voiceMode && (
            <div style={{
              padding: '8px 12px',
              backgroundColor: isListening ? `${FINCEPT.GREEN}15` : isSpeaking ? `${FINCEPT.ORANGE}15` : `${FINCEPT.GRAY}10`,
              borderTop: `1px solid ${isListening ? FINCEPT.GREEN : isSpeaking ? FINCEPT.ORANGE : FINCEPT.GRAY}40`,
              display: 'flex',
              justifyContent: 'center',
              alignItems: 'center',
              gap: '8px',
              fontSize: '9px',
              color: isListening ? FINCEPT.GREEN : isSpeaking ? FINCEPT.ORANGE : FINCEPT.GRAY,
              fontWeight: 700
            }}>
              {isSpeaking ? (
                <>
                  <Volume2 size={12} style={{ animation: 'pulse 0.5s infinite' }} /> AI SPEAKING...
                  <button
                    onClick={stopSpeech}
                    style={{
                      marginLeft: '8px',
                      padding: '4px 8px',
                      backgroundColor: FINCEPT.RED,
                      color: FINCEPT.WHITE,
                      border: 'none',
                      borderRadius: '2px',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px',
                      fontSize: '8px',
                      fontWeight: 700
                    }}
                    title="Stop speaking and ask new query"
                  >
                    <Square size={8} fill={FINCEPT.WHITE} /> STOP
                  </button>
                </>
              ) : isListening ? (
                <><Mic size={12} style={{ animation: 'pulse 0.5s infinite' }} /> LISTENING...</>
              ) : isTyping ? (
                <><Bot size={12} style={{ animation: 'pulse 0.5s infinite' }} /> PROCESSING...</>
              ) : (
                <><Volume2 size={12} /> VOICE MODE</>
              )}
            </div>
          )}
        </div>
      )}

      {/* Floating Bubble - shows voice status when minimized */}
      <div style={{ position: 'fixed', bottom: '50px', right: '20px', zIndex: 10000, display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: '8px' }}>
        {/* Voice status indicator when minimized */}
        {!isOpen && voiceMode && (
          <div style={{
            padding: '6px 12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${isListening ? FINCEPT.GREEN : isSpeaking ? FINCEPT.ORANGE : FINCEPT.GRAY}`,
            borderRadius: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontSize: '9px',
            fontWeight: 700,
            color: isListening ? FINCEPT.GREEN : isSpeaking ? FINCEPT.ORANGE : FINCEPT.GRAY,
            animation: 'slideUp 0.2s ease-out'
          }}>
            {isSpeaking ? (
              <>
                <Volume2 size={10} style={{ animation: 'pulse 0.5s infinite' }} /> SPEAKING
                <button
                  onClick={(e) => { e.stopPropagation(); stopSpeech(); }}
                  style={{
                    marginLeft: '4px',
                    padding: '2px 6px',
                    backgroundColor: FINCEPT.RED,
                    color: FINCEPT.WHITE,
                    border: 'none',
                    borderRadius: '2px',
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '2px',
                    fontSize: '7px',
                    fontWeight: 700
                  }}
                  title="Stop speaking"
                >
                  <Square size={6} fill={FINCEPT.WHITE} />
                </button>
              </>
            ) : isListening ? (
              <><Mic size={10} style={{ animation: 'pulse 0.5s infinite' }} /> LISTENING</>
            ) : isTyping ? (
              <><Bot size={10} style={{ animation: 'pulse 0.5s infinite' }} /> PROCESSING</>
            ) : (
              <><Volume2 size={10} /> VOICE ON</>
            )}
          </div>
        )}

        <button onClick={() => isOpen ? dispatch({ type: 'SET_OPEN', payload: false }) : handleOpen()} style={{ width: '40px', height: '40px', borderRadius: '50%', backgroundColor: FINCEPT.DARK_BG, border: `2px solid ${voiceMode ? FINCEPT.GREEN : FINCEPT.ORANGE}`, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', animation: isOpen ? 'none' : voiceMode && isListening ? 'pulse 1s infinite' : 'morphGlow 4s ease-in-out infinite', position: 'relative' }} title={isOpen ? 'Close' : 'AI Assistant'}>
          {isOpen ? <X size={18} color={FINCEPT.ORANGE} /> : voiceMode ? <Mic size={18} color={isListening ? FINCEPT.GREEN : FINCEPT.ORANGE} /> : <Bot size={18} color={FINCEPT.ORANGE} />}
          {!isOpen && unreadCount > 0 && (
            <div style={{ position: 'absolute', top: '-4px', right: '-4px', width: '18px', height: '18px', borderRadius: '50%', backgroundColor: FINCEPT.RED, color: FINCEPT.WHITE, fontSize: '9px', fontWeight: 700, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
              {unreadCount > 9 ? '9+' : unreadCount}
            </div>
          )}
        </button>
      </div>
    </>
  );
};

export default ChatBubble;
