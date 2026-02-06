// Text-to-Speech Service with multiple providers
// Supports: Browser native, Edge TTS, ElevenLabs, OpenAI TTS

import { invoke } from '@tauri-apps/api/core';

export type TTSProvider = 'browser' | 'edge' | 'elevenlabs' | 'openai';

export interface TTSVoice {
  id: string;
  name: string;
  provider: TTSProvider;
  language: string;
  gender?: 'male' | 'female' | 'neutral';
  preview?: string;
}

export interface TTSConfig {
  provider: TTSProvider;
  voiceId: string;
  rate: number;
  // API keys stored in settings
  elevenLabsApiKey?: string;
  openaiApiKey?: string;
}

// Edge TTS voices (free, high quality)
export const EDGE_VOICES: TTSVoice[] = [
  { id: 'en-US-GuyNeural', name: 'Guy (US Male)', provider: 'edge', language: 'en-US', gender: 'male' },
  { id: 'en-US-JennyNeural', name: 'Jenny (US Female)', provider: 'edge', language: 'en-US', gender: 'female' },
  { id: 'en-US-AriaNeural', name: 'Aria (US Female)', provider: 'edge', language: 'en-US', gender: 'female' },
  { id: 'en-US-DavisNeural', name: 'Davis (US Male)', provider: 'edge', language: 'en-US', gender: 'male' },
  { id: 'en-US-TonyNeural', name: 'Tony (US Male)', provider: 'edge', language: 'en-US', gender: 'male' },
  { id: 'en-US-SaraNeural', name: 'Sara (US Female)', provider: 'edge', language: 'en-US', gender: 'female' },
  { id: 'en-GB-RyanNeural', name: 'Ryan (UK Male)', provider: 'edge', language: 'en-GB', gender: 'male' },
  { id: 'en-GB-SoniaNeural', name: 'Sonia (UK Female)', provider: 'edge', language: 'en-GB', gender: 'female' },
  { id: 'en-AU-NatashaNeural', name: 'Natasha (AU Female)', provider: 'edge', language: 'en-AU', gender: 'female' },
  { id: 'en-AU-WilliamNeural', name: 'William (AU Male)', provider: 'edge', language: 'en-AU', gender: 'male' },
  { id: 'en-IN-NeerjaNeural', name: 'Neerja (India Female)', provider: 'edge', language: 'en-IN', gender: 'female' },
  { id: 'en-IN-PrabhatNeural', name: 'Prabhat (India Male)', provider: 'edge', language: 'en-IN', gender: 'male' },
];

// ElevenLabs default voices
export const ELEVENLABS_VOICES: TTSVoice[] = [
  { id: 'EXAVITQu4vr4xnSDxMaL', name: 'Sarah (Female)', provider: 'elevenlabs', language: 'en', gender: 'female' },
  { id: 'TX3LPaxmHKxFdv7VOQHJ', name: 'Liam (Male)', provider: 'elevenlabs', language: 'en', gender: 'male' },
  { id: 'XB0fDUnXU5powFXDhCwa', name: 'Charlotte (Female)', provider: 'elevenlabs', language: 'en', gender: 'female' },
  { id: 'pFZP5JQG7iQjIQuC4Bku', name: 'Lily (Female)', provider: 'elevenlabs', language: 'en', gender: 'female' },
  { id: 'onwK4e9ZLuTAKqWW03F9', name: 'Daniel (Male)', provider: 'elevenlabs', language: 'en', gender: 'male' },
  { id: '21m00Tcm4TlvDq8ikWAM', name: 'Rachel (Female)', provider: 'elevenlabs', language: 'en', gender: 'female' },
];

// OpenAI TTS voices
export const OPENAI_VOICES: TTSVoice[] = [
  { id: 'alloy', name: 'Alloy (Neutral)', provider: 'openai', language: 'en', gender: 'neutral' },
  { id: 'echo', name: 'Echo (Male)', provider: 'openai', language: 'en', gender: 'male' },
  { id: 'fable', name: 'Fable (Male)', provider: 'openai', language: 'en', gender: 'male' },
  { id: 'onyx', name: 'Onyx (Male)', provider: 'openai', language: 'en', gender: 'male' },
  { id: 'nova', name: 'Nova (Female)', provider: 'openai', language: 'en', gender: 'female' },
  { id: 'shimmer', name: 'Shimmer (Female)', provider: 'openai', language: 'en', gender: 'female' },
];

class TTSService {
  private currentAudio: HTMLAudioElement | null = null;
  private isSpeaking = false;

  // Get all available voices
  getAllVoices(): TTSVoice[] {
    const browserVoices = this.getBrowserVoices();
    return [...browserVoices, ...EDGE_VOICES, ...ELEVENLABS_VOICES, ...OPENAI_VOICES];
  }

  // Get browser native voices
  getBrowserVoices(): TTSVoice[] {
    if (!window.speechSynthesis) return [];

    const voices = window.speechSynthesis.getVoices();
    return voices
      .filter(v => v.lang.startsWith('en'))
      .map(v => ({
        id: v.name,
        name: v.name,
        provider: 'browser' as TTSProvider,
        language: v.lang,
      }));
  }

  // Speak text with selected voice
  async speak(text: string, config: TTSConfig): Promise<void> {
    // Stop any current speech
    this.stop();

    // Clean text
    const cleanText = this.cleanText(text);
    if (!cleanText) return;

    switch (config.provider) {
      case 'browser':
        return this.speakBrowser(cleanText, config);
      case 'edge':
        return this.speakEdge(cleanText, config);
      case 'elevenlabs':
        return this.speakElevenLabs(cleanText, config);
      case 'openai':
        return this.speakOpenAI(cleanText, config);
      default:
        return this.speakBrowser(cleanText, config);
    }
  }

  // Stop speaking
  stop(): void {
    if (this.currentAudio) {
      this.currentAudio.pause();
      this.currentAudio = null;
    }
    if (window.speechSynthesis) {
      window.speechSynthesis.cancel();
    }
    this.isSpeaking = false;
  }

  // Check if speaking
  getIsSpeaking(): boolean {
    return this.isSpeaking;
  }

  // Clean text for TTS
  private cleanText(text: string): string {
    return text
      .replace(/```[\s\S]*?```/g, '') // Remove code blocks
      .replace(/[`*#\[\](){}]/g, '')  // Remove markdown
      .replace(/\n+/g, '. ')          // Replace newlines
      .replace(/[\u{1F300}-\u{1F9FF}]/gu, '') // Remove emojis
      .replace(/[\u{2600}-\u{26FF}]/gu, '')
      .replace(/[\u{2700}-\u{27BF}]/gu, '')
      .replace(/[\u{1F600}-\u{1F64F}]/gu, '')
      .replace(/[\u{1F680}-\u{1F6FF}]/gu, '')
      .replace(/[\u{1F1E0}-\u{1F1FF}]/gu, '')
      .replace(/\s+/g, ' ')
      .trim()
      .slice(0, 1000);
  }

  // Browser native TTS
  private speakBrowser(text: string, config: TTSConfig): Promise<void> {
    return new Promise((resolve, reject) => {
      if (!window.speechSynthesis) {
        reject(new Error('Speech synthesis not supported'));
        return;
      }

      const utterance = new SpeechSynthesisUtterance(text);
      utterance.rate = config.rate;

      const voices = window.speechSynthesis.getVoices();
      const voice = voices.find(v => v.name === config.voiceId);
      if (voice) utterance.voice = voice;

      utterance.onstart = () => { this.isSpeaking = true; };
      utterance.onend = () => { this.isSpeaking = false; resolve(); };
      utterance.onerror = (e) => { this.isSpeaking = false; reject(e); };

      window.speechSynthesis.cancel();
      window.speechSynthesis.speak(utterance);
    });
  }

  // Edge TTS (via Python backend)
  private async speakEdge(text: string, config: TTSConfig): Promise<void> {
    try {
      this.isSpeaking = true;

      // Call Rust/Python backend to generate audio
      const audioBase64 = await invoke<string>('tts_edge_speak', {
        text,
        voice: config.voiceId,
        rate: config.rate,
      });

      // Play audio
      const audio = new Audio(`data:audio/mp3;base64,${audioBase64}`);
      this.currentAudio = audio;

      return new Promise((resolve, reject) => {
        audio.onended = () => {
          this.isSpeaking = false;
          this.currentAudio = null;
          resolve();
        };
        audio.onerror = (e) => {
          this.isSpeaking = false;
          this.currentAudio = null;
          reject(e);
        };
        audio.play().catch(reject);
      });
    } catch (error) {
      this.isSpeaking = false;
      console.error('[TTS] Edge TTS error:', error);
      // Fallback to browser
      return this.speakBrowser(text, { ...config, provider: 'browser' });
    }
  }

  // ElevenLabs TTS
  private async speakElevenLabs(text: string, config: TTSConfig): Promise<void> {
    if (!config.elevenLabsApiKey) {
      console.warn('[TTS] ElevenLabs API key not set, falling back to browser');
      return this.speakBrowser(text, { ...config, provider: 'browser' });
    }

    try {
      this.isSpeaking = true;

      const response = await fetch(
        `https://api.elevenlabs.io/v1/text-to-speech/${config.voiceId}`,
        {
          method: 'POST',
          headers: {
            'Accept': 'audio/mpeg',
            'Content-Type': 'application/json',
            'xi-api-key': config.elevenLabsApiKey,
          },
          body: JSON.stringify({
            text,
            model_id: 'eleven_monolingual_v1',
            voice_settings: {
              stability: 0.5,
              similarity_boost: 0.75,
            },
          }),
        }
      );

      if (!response.ok) {
        throw new Error(`ElevenLabs API error: ${response.status}`);
      }

      const audioBlob = await response.blob();
      const audioUrl = URL.createObjectURL(audioBlob);
      const audio = new Audio(audioUrl);
      this.currentAudio = audio;

      // Adjust playback rate
      audio.playbackRate = config.rate;

      return new Promise((resolve, reject) => {
        audio.onended = () => {
          this.isSpeaking = false;
          this.currentAudio = null;
          URL.revokeObjectURL(audioUrl);
          resolve();
        };
        audio.onerror = (e) => {
          this.isSpeaking = false;
          this.currentAudio = null;
          URL.revokeObjectURL(audioUrl);
          reject(e);
        };
        audio.play().catch(reject);
      });
    } catch (error) {
      this.isSpeaking = false;
      console.error('[TTS] ElevenLabs error:', error);
      return this.speakBrowser(text, { ...config, provider: 'browser' });
    }
  }

  // OpenAI TTS
  private async speakOpenAI(text: string, config: TTSConfig): Promise<void> {
    if (!config.openaiApiKey) {
      console.warn('[TTS] OpenAI API key not set, falling back to browser');
      return this.speakBrowser(text, { ...config, provider: 'browser' });
    }

    try {
      this.isSpeaking = true;

      const response = await fetch('https://api.openai.com/v1/audio/speech', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${config.openaiApiKey}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          model: 'tts-1',
          input: text,
          voice: config.voiceId,
          speed: config.rate,
        }),
      });

      if (!response.ok) {
        throw new Error(`OpenAI API error: ${response.status}`);
      }

      const audioBlob = await response.blob();
      const audioUrl = URL.createObjectURL(audioBlob);
      const audio = new Audio(audioUrl);
      this.currentAudio = audio;

      return new Promise((resolve, reject) => {
        audio.onended = () => {
          this.isSpeaking = false;
          this.currentAudio = null;
          URL.revokeObjectURL(audioUrl);
          resolve();
        };
        audio.onerror = (e) => {
          this.isSpeaking = false;
          this.currentAudio = null;
          URL.revokeObjectURL(audioUrl);
          reject(e);
        };
        audio.play().catch(reject);
      });
    } catch (error) {
      this.isSpeaking = false;
      console.error('[TTS] OpenAI error:', error);
      return this.speakBrowser(text, { ...config, provider: 'browser' });
    }
  }
}

export const ttsService = new TTSService();
