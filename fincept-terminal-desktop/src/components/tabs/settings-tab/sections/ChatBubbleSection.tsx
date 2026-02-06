// Chat Bubble Settings Section
// Supports multiple TTS providers: Browser, Edge TTS, ElevenLabs, OpenAI

import React, { useState, useEffect } from 'react';
import { MessageCircle, ToggleLeft, ToggleRight, Info, Volume2, Mic, Gauge, RefreshCw, Key, ChevronDown, ChevronUp, Eye, EyeOff } from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import type { SettingsColors } from '../types';
import { ttsService, TTSProvider, EDGE_VOICES, ELEVENLABS_VOICES, OPENAI_VOICES, TTSVoice } from '@/services/voice/ttsService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

// Provider info for display
const PROVIDERS = [
  { id: 'browser' as TTSProvider, name: 'Browser Native', description: 'Uses your system voices (free)', requiresKey: false },
  { id: 'edge' as TTSProvider, name: 'Edge TTS', description: 'High-quality Microsoft neural voices (free)', requiresKey: false },
  { id: 'elevenlabs' as TTSProvider, name: 'ElevenLabs', description: 'Ultra-realistic AI voices (API key required)', requiresKey: true },
  { id: 'openai' as TTSProvider, name: 'OpenAI TTS', description: 'OpenAI text-to-speech voices (API key required)', requiresKey: true },
];

interface ChatBubbleSectionProps {
  colors: SettingsColors;
  showMessage: (type: 'success' | 'error', text: string) => void;
}

export function ChatBubbleSection({ colors, showMessage }: ChatBubbleSectionProps) {
  const [enabled, setEnabled] = useState(true);
  const [loading, setLoading] = useState(false);

  // TTS provider settings
  const [selectedProvider, setSelectedProvider] = useState<TTSProvider>('browser');
  const [selectedVoice, setSelectedVoice] = useState('');
  const [speechRate, setSpeechRate] = useState(1.4);
  const [autoStartVoice, setAutoStartVoice] = useState(false);

  // API Keys
  const [elevenLabsApiKey, setElevenLabsApiKey] = useState('');
  const [openaiApiKey, setOpenaiApiKey] = useState('');
  const [showElevenLabsKey, setShowElevenLabsKey] = useState(false);
  const [showOpenaiKey, setShowOpenaiKey] = useState(false);
  const [apiKeysExpanded, setApiKeysExpanded] = useState(false);

  // Available voices
  const [browserVoices, setBrowserVoices] = useState<SpeechSynthesisVoice[]>([]);
  const [voicesLoaded, setVoicesLoaded] = useState(false);

  // Load browser voices
  useEffect(() => {
    const loadVoices = () => {
      if (!window.speechSynthesis) return;

      const voices = window.speechSynthesis.getVoices();
      const englishVoices = voices.filter(v => v.lang.startsWith('en'));
      setBrowserVoices(englishVoices);
      setVoicesLoaded(englishVoices.length > 0);

      console.log('[ChatBubbleSection] Loaded browser voices:', englishVoices.length);
    };

    loadVoices();
    if (window.speechSynthesis) {
      window.speechSynthesis.onvoiceschanged = loadVoices;
    }

    return () => {
      if (window.speechSynthesis) {
        window.speechSynthesis.onvoiceschanged = null;
      }
    };
  }, []);

  // Load settings on mount
  useEffect(() => {
    const load = async () => {
      try {
        const [bubbleEnabled, provider, voice, rate, autoVoice, elevenKey, openaiKey] = await Promise.all([
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_enabled' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_tts_provider' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_voice' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_speech_rate' }),
          invoke<string | null>('db_get_setting', { key: 'chat_bubble_auto_voice' }),
          invoke<string | null>('db_get_setting', { key: 'elevenlabs_api_key' }),
          invoke<string | null>('db_get_setting', { key: 'openai_tts_api_key' }),
        ]);

        setEnabled(bubbleEnabled !== 'false');
        if (provider) setSelectedProvider(provider as TTSProvider);
        if (voice) setSelectedVoice(voice);
        if (rate) setSpeechRate(parseFloat(rate));
        setAutoStartVoice(autoVoice === 'true');
        if (elevenKey) setElevenLabsApiKey(elevenKey);
        if (openaiKey) setOpenaiApiKey(openaiKey);
      } catch (error) {
        console.error('[ChatBubbleSection] Load error:', error);
      }
    };
    load();
  }, []);

  const handleToggle = async () => {
    setLoading(true);
    try {
      const newValue = !enabled;
      await invoke('db_save_setting', { key: 'chat_bubble_enabled', value: String(newValue), category: 'ui' });
      setEnabled(newValue);
      showMessage('success', `Chat bubble ${newValue ? 'enabled' : 'disabled'}. Restart for changes to take effect.`);
    } catch (error) {
      console.error('[ChatBubbleSection] Save error:', error);
      showMessage('error', 'Failed to save setting');
    } finally {
      setLoading(false);
    }
  };

  // Get voices for current provider
  const getCurrentVoices = (): TTSVoice[] => {
    switch (selectedProvider) {
      case 'browser':
        return browserVoices.map(v => ({
          id: v.name,
          name: v.name,
          provider: 'browser' as TTSProvider,
          language: v.lang,
        }));
      case 'edge':
        return EDGE_VOICES;
      case 'elevenlabs':
        return ELEVENLABS_VOICES;
      case 'openai':
        return OPENAI_VOICES;
      default:
        return [];
    }
  };

  const handleProviderChange = async (provider: TTSProvider) => {
    try {
      await invoke('db_save_setting', { key: 'chat_bubble_tts_provider', value: provider, category: 'ui' });
      setSelectedProvider(provider);
      // Reset voice selection when provider changes
      setSelectedVoice('');
      showMessage('success', `TTS provider changed to ${PROVIDERS.find(p => p.id === provider)?.name}`);
    } catch (error) {
      showMessage('error', 'Failed to save provider setting');
    }
  };

  const handleVoiceChange = async (voiceId: string) => {
    try {
      await invoke('db_save_setting', { key: 'chat_bubble_voice', value: voiceId, category: 'ui' });
      setSelectedVoice(voiceId);
      showMessage('success', 'Voice updated');
    } catch (error) {
      showMessage('error', 'Failed to save voice setting');
    }
  };

  const handleRateChange = async (rate: number) => {
    setSpeechRate(rate);
    try {
      await invoke('db_save_setting', { key: 'chat_bubble_speech_rate', value: String(rate), category: 'ui' });
    } catch (error) {
      console.error('[ChatBubbleSection] Rate save error:', error);
    }
  };

  const handleAutoVoiceToggle = async () => {
    try {
      const newValue = !autoStartVoice;
      await invoke('db_save_setting', { key: 'chat_bubble_auto_voice', value: String(newValue), category: 'ui' });
      setAutoStartVoice(newValue);
      showMessage('success', `Auto voice mode ${newValue ? 'enabled' : 'disabled'}`);
    } catch (error) {
      showMessage('error', 'Failed to save setting');
    }
  };

  const handleSaveApiKey = async (keyType: 'elevenlabs' | 'openai', value: string) => {
    try {
      const key = keyType === 'elevenlabs' ? 'elevenlabs_api_key' : 'openai_tts_api_key';
      await invoke('db_save_setting', { key, value, category: 'api' });
      showMessage('success', `${keyType === 'elevenlabs' ? 'ElevenLabs' : 'OpenAI'} API key saved`);
    } catch (error) {
      showMessage('error', 'Failed to save API key');
    }
  };

  const refreshVoices = () => {
    if (!window.speechSynthesis) return;
    const voices = window.speechSynthesis.getVoices();
    const englishVoices = voices.filter(v => v.lang.startsWith('en'));
    setBrowserVoices(englishVoices);
    setVoicesLoaded(englishVoices.length > 0);
    showMessage('success', `Found ${englishVoices.length} browser voices`);
  };

  const testVoice = async () => {
    const text = 'Hello, I am your Fincept AI assistant. How can I help you today?';

    try {
      await ttsService.speak(text, {
        provider: selectedProvider,
        voiceId: selectedVoice,
        rate: speechRate,
        elevenLabsApiKey: elevenLabsApiKey || undefined,
        openaiApiKey: openaiApiKey || undefined,
      });
    } catch (error) {
      console.error('[TestVoice] Error:', error);
      showMessage('error', `Failed to test voice: ${error}`);
    }
  };

  const currentVoices = getCurrentVoices();

  return (
    <div>
      {/* Header */}
      <div style={{
        marginBottom: '20px',
        paddingBottom: '12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <MessageCircle size={16} color={FINCEPT.ORANGE} />
          <h2 style={{
            fontSize: '11px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            margin: 0,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            AI CHAT BUBBLE
          </h2>
        </div>
        <p style={{
          fontSize: '10px',
          color: FINCEPT.GRAY,
          marginTop: '8px',
          lineHeight: 1.5
        }}>
          Configure the floating AI chat assistant bubble and voice settings.
        </p>
      </div>

      {/* Enable/Disable Toggle */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '16px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}>
          <div>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.WHITE,
              marginBottom: '4px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px'
            }}>
              ENABLE CHAT BUBBLE
            </div>
            <div style={{
              fontSize: '9px',
              color: FINCEPT.GRAY,
              lineHeight: 1.4
            }}>
              Show floating AI assistant button in bottom-right corner
            </div>
          </div>

          <button
            onClick={handleToggle}
            disabled={loading}
            style={{
              background: 'transparent',
              border: 'none',
              cursor: loading ? 'not-allowed' : 'pointer',
              opacity: loading ? 0.5 : 1,
              padding: '4px',
              display: 'flex',
              alignItems: 'center'
            }}
          >
            {enabled ? (
              <ToggleRight size={32} color={FINCEPT.GREEN} />
            ) : (
              <ToggleLeft size={32} color={FINCEPT.MUTED} />
            )}
          </button>
        </div>

        {/* Status indicator */}
        <div style={{
          marginTop: '12px',
          padding: '8px 10px',
          backgroundColor: enabled ? `${FINCEPT.GREEN}10` : `${FINCEPT.MUTED}10`,
          border: `1px solid ${enabled ? FINCEPT.GREEN : FINCEPT.MUTED}`,
          borderRadius: '2px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <div style={{
            width: '6px',
            height: '6px',
            borderRadius: '50%',
            backgroundColor: enabled ? FINCEPT.GREEN : FINCEPT.MUTED
          }} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: enabled ? FINCEPT.GREEN : FINCEPT.MUTED,
            textTransform: 'uppercase',
            letterSpacing: '0.5px'
          }}>
            {enabled ? 'ACTIVE' : 'DISABLED'}
          </span>
        </div>
      </div>

      {/* Voice Settings Section */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '16px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Volume2 size={14} color={FINCEPT.ORANGE} />
            <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
              TEXT-TO-SPEECH
            </span>
          </div>
          {selectedProvider === 'browser' && (
            <button
              onClick={refreshVoices}
              style={{
                background: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '3px',
                padding: '4px 8px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                color: FINCEPT.GRAY,
                fontSize: '8px'
              }}
              title="Refresh browser voices"
            >
              <RefreshCw size={10} />
              REFRESH
            </button>
          )}
        </div>

        {/* TTS Provider Selection */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px', textTransform: 'uppercase' }}>
            TTS PROVIDER
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
            {PROVIDERS.map(provider => (
              <button
                key={provider.id}
                onClick={() => handleProviderChange(provider.id)}
                style={{
                  padding: '10px',
                  backgroundColor: selectedProvider === provider.id ? `${FINCEPT.ORANGE}15` : FINCEPT.DARK_BG,
                  border: `1px solid ${selectedProvider === provider.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  cursor: 'pointer',
                  textAlign: 'left'
                }}
              >
                <div style={{ fontSize: '9px', fontWeight: 700, color: selectedProvider === provider.id ? FINCEPT.ORANGE : FINCEPT.WHITE, marginBottom: '2px' }}>
                  {provider.name}
                </div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED, lineHeight: 1.3 }}>
                  {provider.description}
                </div>
              </button>
            ))}
          </div>
        </div>

        {/* Voice Selection */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '8px', textTransform: 'uppercase' }}>
            {selectedProvider === 'browser' ? `BROWSER VOICES (${currentVoices.length})` :
             selectedProvider === 'edge' ? 'EDGE TTS VOICES' :
             selectedProvider === 'elevenlabs' ? 'ELEVENLABS VOICES' : 'OPENAI VOICES'}
          </div>

          {selectedProvider === 'browser' && !voicesLoaded ? (
            <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', color: FINCEPT.GRAY, fontSize: '9px', textAlign: 'center' }}>
              Loading voices... Click REFRESH if voices don't appear.
            </div>
          ) : currentVoices.length === 0 ? (
            <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '4px', color: FINCEPT.GRAY, fontSize: '9px', textAlign: 'center' }}>
              No voices available for this provider.
            </div>
          ) : (
            <div style={{ maxHeight: '160px', overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {currentVoices.map(voice => (
                <button
                  key={voice.id}
                  onClick={() => handleVoiceChange(voice.id)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    padding: '8px 10px',
                    backgroundColor: selectedVoice === voice.id ? `${FINCEPT.ORANGE}15` : FINCEPT.DARK_BG,
                    border: `1px solid ${selectedVoice === voice.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    borderRadius: '3px',
                    cursor: 'pointer',
                    textAlign: 'left'
                  }}
                >
                  <div>
                    <div style={{ fontSize: '9px', fontWeight: 600, color: selectedVoice === voice.id ? FINCEPT.ORANGE : FINCEPT.WHITE }}>
                      {voice.name}
                    </div>
                    <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '2px' }}>
                      {voice.language} {voice.gender ? `• ${voice.gender}` : ''}
                    </div>
                  </div>
                  {selectedVoice === voice.id && (
                    <div style={{ width: '6px', height: '6px', borderRadius: '50%', backgroundColor: FINCEPT.ORANGE }} />
                  )}
                </button>
              ))}
            </div>
          )}
        </div>

        {/* Speech Rate */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Gauge size={12} color={FINCEPT.GRAY} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, textTransform: 'uppercase' }}>
                SPEECH SPEED
              </span>
            </div>
            <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.ORANGE }}>
              {speechRate.toFixed(1)}x
            </span>
          </div>
          <input
            type="range"
            min="0.5"
            max="2.0"
            step="0.1"
            value={speechRate}
            onChange={(e) => handleRateChange(parseFloat(e.target.value))}
            style={{
              width: '100%',
              accentColor: FINCEPT.ORANGE,
              height: '4px',
              cursor: 'pointer'
            }}
          />
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px', color: FINCEPT.MUTED, marginTop: '4px' }}>
            <span>0.5x (Slow)</span>
            <span>1.0x</span>
            <span>2.0x (Fast)</span>
          </div>
        </div>

        {/* Test Voice Button */}
        <button
          onClick={testVoice}
          disabled={currentVoices.length === 0 || !selectedVoice}
          style={{
            width: '100%',
            padding: '10px',
            backgroundColor: currentVoices.length > 0 && selectedVoice ? FINCEPT.DARK_BG : FINCEPT.MUTED,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '4px',
            color: currentVoices.length > 0 && selectedVoice ? FINCEPT.WHITE : FINCEPT.GRAY,
            fontSize: '10px',
            fontWeight: 600,
            cursor: currentVoices.length > 0 && selectedVoice ? 'pointer' : 'not-allowed',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '8px'
          }}
        >
          <Volume2 size={14} />
          TEST VOICE
        </button>
      </div>

      {/* API Keys Section */}
      {(selectedProvider === 'elevenlabs' || selectedProvider === 'openai') && (
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '4px',
          padding: '16px',
          marginBottom: '16px'
        }}>
          <button
            onClick={() => setApiKeysExpanded(!apiKeysExpanded)}
            style={{
              width: '100%',
              background: 'transparent',
              border: 'none',
              padding: 0,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              marginBottom: apiKeysExpanded ? '16px' : 0
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Key size={14} color={FINCEPT.ORANGE} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.WHITE, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                API KEYS
              </span>
            </div>
            {apiKeysExpanded ? <ChevronUp size={14} color={FINCEPT.GRAY} /> : <ChevronDown size={14} color={FINCEPT.GRAY} />}
          </button>

          {apiKeysExpanded && (
            <>
              {/* ElevenLabs API Key */}
              {selectedProvider === 'elevenlabs' && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', textTransform: 'uppercase' }}>
                    ELEVENLABS API KEY
                  </div>
                  <div style={{ display: 'flex', gap: '6px' }}>
                    <div style={{ flex: 1, position: 'relative' }}>
                      <input
                        type={showElevenLabsKey ? 'text' : 'password'}
                        value={elevenLabsApiKey}
                        onChange={(e) => setElevenLabsApiKey(e.target.value)}
                        placeholder="Enter ElevenLabs API key..."
                        style={{
                          width: '100%',
                          padding: '8px 32px 8px 10px',
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '3px',
                          color: FINCEPT.WHITE,
                          fontSize: '9px',
                          fontFamily: 'monospace'
                        }}
                      />
                      <button
                        onClick={() => setShowElevenLabsKey(!showElevenLabsKey)}
                        style={{
                          position: 'absolute',
                          right: '6px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'transparent',
                          border: 'none',
                          cursor: 'pointer',
                          padding: '2px',
                          color: FINCEPT.GRAY
                        }}
                      >
                        {showElevenLabsKey ? <EyeOff size={12} /> : <Eye size={12} />}
                      </button>
                    </div>
                    <button
                      onClick={() => handleSaveApiKey('elevenlabs', elevenLabsApiKey)}
                      style={{
                        padding: '8px 12px',
                        backgroundColor: FINCEPT.ORANGE,
                        border: 'none',
                        borderRadius: '3px',
                        color: FINCEPT.DARK_BG,
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer'
                      }}
                    >
                      SAVE
                    </button>
                  </div>
                  <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                    Get your API key from <a href="https://elevenlabs.io" target="_blank" rel="noopener" style={{ color: FINCEPT.ORANGE }}>elevenlabs.io</a>
                  </div>
                </div>
              )}

              {/* OpenAI API Key */}
              {selectedProvider === 'openai' && (
                <div>
                  <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', textTransform: 'uppercase' }}>
                    OPENAI API KEY
                  </div>
                  <div style={{ display: 'flex', gap: '6px' }}>
                    <div style={{ flex: 1, position: 'relative' }}>
                      <input
                        type={showOpenaiKey ? 'text' : 'password'}
                        value={openaiApiKey}
                        onChange={(e) => setOpenaiApiKey(e.target.value)}
                        placeholder="Enter OpenAI API key..."
                        style={{
                          width: '100%',
                          padding: '8px 32px 8px 10px',
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '3px',
                          color: FINCEPT.WHITE,
                          fontSize: '9px',
                          fontFamily: 'monospace'
                        }}
                      />
                      <button
                        onClick={() => setShowOpenaiKey(!showOpenaiKey)}
                        style={{
                          position: 'absolute',
                          right: '6px',
                          top: '50%',
                          transform: 'translateY(-50%)',
                          background: 'transparent',
                          border: 'none',
                          cursor: 'pointer',
                          padding: '2px',
                          color: FINCEPT.GRAY
                        }}
                      >
                        {showOpenaiKey ? <EyeOff size={12} /> : <Eye size={12} />}
                      </button>
                    </div>
                    <button
                      onClick={() => handleSaveApiKey('openai', openaiApiKey)}
                      style={{
                        padding: '8px 12px',
                        backgroundColor: FINCEPT.ORANGE,
                        border: 'none',
                        borderRadius: '3px',
                        color: FINCEPT.DARK_BG,
                        fontSize: '9px',
                        fontWeight: 700,
                        cursor: 'pointer'
                      }}
                    >
                      SAVE
                    </button>
                  </div>
                  <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                    Get your API key from <a href="https://platform.openai.com/api-keys" target="_blank" rel="noopener" style={{ color: FINCEPT.ORANGE }}>platform.openai.com</a>
                  </div>
                </div>
              )}
            </>
          )}
        </div>
      )}

      {/* Auto Voice Mode Toggle */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '16px'
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}>
          <div style={{ display: 'flex', alignItems: 'flex-start', gap: '10px' }}>
            <Mic size={14} color={FINCEPT.ORANGE} style={{ marginTop: '2px' }} />
            <div>
              <div style={{
                fontSize: '10px',
                fontWeight: 700,
                color: FINCEPT.WHITE,
                marginBottom: '4px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px'
              }}>
                AUTO-START VOICE MODE
              </div>
              <div style={{
                fontSize: '9px',
                color: FINCEPT.GRAY,
                lineHeight: 1.4
              }}>
                Automatically enable voice mode when opening chat bubble
              </div>
            </div>
          </div>

          <button
            onClick={handleAutoVoiceToggle}
            style={{
              background: 'transparent',
              border: 'none',
              cursor: 'pointer',
              padding: '4px',
              display: 'flex',
              alignItems: 'center'
            }}
          >
            {autoStartVoice ? (
              <ToggleRight size={28} color={FINCEPT.GREEN} />
            ) : (
              <ToggleLeft size={28} color={FINCEPT.MUTED} />
            )}
          </button>
        </div>
      </div>

      {/* Info Card */}
      <div style={{
        backgroundColor: `${FINCEPT.ORANGE}10`,
        border: `1px solid ${FINCEPT.ORANGE}40`,
        borderRadius: '4px',
        padding: '12px'
      }}>
        <div style={{ display: 'flex', gap: '10px' }}>
          <Info size={14} color={FINCEPT.ORANGE} style={{ flexShrink: 0, marginTop: '2px' }} />
          <div>
            <div style={{
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.ORANGE,
              marginBottom: '6px',
              textTransform: 'uppercase',
              letterSpacing: '0.5px'
            }}>
              VOICE MODE FEATURES
            </div>
            <ul style={{
              margin: 0,
              padding: '0 0 0 14px',
              fontSize: '9px',
              color: FINCEPT.GRAY,
              lineHeight: 1.6
            }}>
              <li>Hands-free conversation with AI assistant</li>
              <li>4 TTS providers: Browser, Edge TTS, ElevenLabs, OpenAI</li>
              <li>Edge TTS: Free high-quality Microsoft neural voices</li>
              <li>ElevenLabs/OpenAI: Premium AI voices (API key required)</li>
              <li>Works even when chat bubble is minimized</li>
              <li>Click VOICE button in chat to activate</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
}
