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

  // Clean and preprocess text for TTS - makes content more natural to listen to
  private cleanText(text: string): string {
    let result = text;

    // 1. Handle LaTeX formulas - convert to spoken math
    result = this.convertFormulasToSpeech(result);

    // 2. Handle markdown tables - convert to spoken format
    result = this.convertTablesToSpeech(result);

    // 3. Handle dates and times
    result = this.convertDatesToSpeech(result);

    // 4. Handle numbers and financial data
    result = this.convertNumbersToSpeech(result);

    // 5. Handle common abbreviations
    result = this.expandAbbreviations(result);

    // 6. Clean remaining markdown
    result = result
      .replace(/```[\s\S]*?```/g, '') // Remove code blocks
      .replace(/`([^`]+)`/g, '$1')    // Keep inline code content
      .replace(/\*\*([^*]+)\*\*/g, '$1') // Bold
      .replace(/\*([^*]+)\*/g, '$1')  // Italic
      .replace(/#{1,6}\s*/g, '')      // Headers
      .replace(/\[([^\]]+)\]\([^)]+\)/g, '$1') // Links - keep text
      .replace(/[[\](){}]/g, '')      // Remove brackets
      .replace(/\n+/g, '. ')          // Replace newlines
      .replace(/[\u{1F300}-\u{1F9FF}]/gu, '') // Remove emojis
      .replace(/[\u{2600}-\u{26FF}]/gu, '')
      .replace(/[\u{2700}-\u{27BF}]/gu, '')
      .replace(/[\u{1F600}-\u{1F64F}]/gu, '')
      .replace(/[\u{1F680}-\u{1F6FF}]/gu, '')
      .replace(/[\u{1F1E0}-\u{1F1FF}]/gu, '')
      .replace(/\.+/g, '.')           // Multiple periods to single
      .replace(/\s+/g, ' ')           // Multiple spaces to single
      .trim();

    // Limit length for TTS (can be longer since we're summarizing)
    return result.slice(0, 2000);
  }

  // Convert LaTeX formulas to spoken math
  private convertFormulasToSpeech(text: string): string {
    // Block formulas: \[ ... \] or $$ ... $$
    text = text.replace(/\\\[([\s\S]*?)\\\]|\$\$([\s\S]*?)\$\$/g, (_, b1, b2) => {
      const formula = b1 || b2;
      return '. The formula is: ' + this.latexToSpeech(formula) + '. ';
    });

    // Inline formulas: \( ... \) or $ ... $
    text = text.replace(/\\\((.*?)\\\)|\$([^\$\n]+?)\$/g, (_, i1, i2) => {
      const formula = i1 || i2;
      return ' ' + this.latexToSpeech(formula) + ' ';
    });

    return text;
  }

  // Convert LaTeX to speakable text
  private latexToSpeech(latex: string): string {
    let speech = latex.trim();

    // Fractions
    speech = speech.replace(/\\frac\{([^}]+)\}\{([^}]+)\}/g, '$1 divided by $2');
    speech = speech.replace(/\\dfrac\{([^}]+)\}\{([^}]+)\}/g, '$1 divided by $2');

    // Square roots
    speech = speech.replace(/\\sqrt\[(\d+)\]\{([^}]+)\}/g, 'the $1th root of $2');
    speech = speech.replace(/\\sqrt\{([^}]+)\}/g, 'the square root of $1');

    // Exponents and subscripts
    speech = speech.replace(/\^{([^}]+)}/g, ' to the power of $1');
    speech = speech.replace(/\^(\d)/g, ' to the power of $1');
    speech = speech.replace(/_{([^}]+)}/g, ' subscript $1');
    speech = speech.replace(/_(\w)/g, ' subscript $1');

    // Summation and integrals
    speech = speech.replace(/\\sum_\{([^}]+)\}\^\{([^}]+)\}/g, 'the sum from $1 to $2 of');
    speech = speech.replace(/\\sum/g, 'the sum of');
    speech = speech.replace(/\\int_\{([^}]+)\}\^\{([^}]+)\}/g, 'the integral from $1 to $2 of');
    speech = speech.replace(/\\int/g, 'the integral of');
    speech = speech.replace(/\\prod/g, 'the product of');

    // Limits
    speech = speech.replace(/\\lim_\{([^}]+)\}/g, 'the limit as $1 of');

    // Greek letters
    const greekLetters: Record<string, string> = {
      '\\alpha': 'alpha', '\\beta': 'beta', '\\gamma': 'gamma', '\\delta': 'delta',
      '\\epsilon': 'epsilon', '\\zeta': 'zeta', '\\eta': 'eta', '\\theta': 'theta',
      '\\iota': 'iota', '\\kappa': 'kappa', '\\lambda': 'lambda', '\\mu': 'mu',
      '\\nu': 'nu', '\\xi': 'xi', '\\pi': 'pi', '\\rho': 'rho',
      '\\sigma': 'sigma', '\\tau': 'tau', '\\upsilon': 'upsilon', '\\phi': 'phi',
      '\\chi': 'chi', '\\psi': 'psi', '\\omega': 'omega',
      '\\Delta': 'Delta', '\\Sigma': 'Sigma', '\\Pi': 'Pi', '\\Omega': 'Omega',
    };
    for (const [latex, spoken] of Object.entries(greekLetters)) {
      speech = speech.replace(new RegExp(latex.replace(/\\/g, '\\\\'), 'g'), spoken);
    }

    // Operators
    speech = speech.replace(/\\times/g, ' times ');
    speech = speech.replace(/\\div/g, ' divided by ');
    speech = speech.replace(/\\pm/g, ' plus or minus ');
    speech = speech.replace(/\\mp/g, ' minus or plus ');
    speech = speech.replace(/\\cdot/g, ' times ');
    speech = speech.replace(/\\leq/g, ' less than or equal to ');
    speech = speech.replace(/\\geq/g, ' greater than or equal to ');
    speech = speech.replace(/\\neq/g, ' not equal to ');
    speech = speech.replace(/\\approx/g, ' approximately equal to ');
    speech = speech.replace(/\\equiv/g, ' is equivalent to ');
    speech = speech.replace(/\\rightarrow/g, ' approaches ');
    speech = speech.replace(/\\to/g, ' to ');
    speech = speech.replace(/\\infty/g, ' infinity ');

    // Text commands
    speech = speech.replace(/\\text\{([^}]+)\}/g, '$1');
    speech = speech.replace(/\\textbf\{([^}]+)\}/g, '$1');
    speech = speech.replace(/\\mathrm\{([^}]+)\}/g, '$1');

    // Trig functions
    speech = speech.replace(/\\(sin|cos|tan|cot|sec|csc|log|ln|exp)/g, ' $1 of ');

    // Clean up remaining backslashes and braces
    speech = speech.replace(/\\[a-zA-Z]+/g, ' ');
    speech = speech.replace(/[{}]/g, '');
    speech = speech.replace(/\s+/g, ' ');

    return speech.trim();
  }

  // Convert markdown tables to spoken format
  private convertTablesToSpeech(text: string): string {
    // Match markdown tables
    const tableRegex = /\|(.+)\|\n\|[-:\s|]+\|\n((?:\|.+\|\n?)+)/g;

    return text.replace(tableRegex, (match) => {
      const lines = match.trim().split('\n');
      if (lines.length < 3) return match;

      // Parse header
      const headers = lines[0].split('|').filter(h => h.trim()).map(h => h.trim());

      // Parse rows (skip header and separator)
      const rows = lines.slice(2).map(line =>
        line.split('|').filter(c => c.trim()).map(c => c.trim())
      );

      // Build spoken table
      let spoken = '. Here is a table with columns: ' + headers.join(', ') + '. ';

      rows.forEach((row, idx) => {
        spoken += `Row ${idx + 1}: `;
        row.forEach((cell, cellIdx) => {
          if (headers[cellIdx]) {
            spoken += `${headers[cellIdx]} is ${cell}. `;
          } else {
            spoken += `${cell}. `;
          }
        });
      });

      return spoken;
    });
  }

  // Convert dates and times to spoken format
  private convertDatesToSpeech(text: string): string {
    // ISO dates: 2024-01-15 or 2024/01/15
    text = text.replace(/(\d{4})[-/](\d{2})[-/](\d{2})/g, (_, year, month, day) => {
      const months = ['', 'January', 'February', 'March', 'April', 'May', 'June',
        'July', 'August', 'September', 'October', 'November', 'December'];
      const m = parseInt(month, 10);
      const d = parseInt(day, 10);
      return `${months[m]} ${d}, ${year}`;
    });

    // US dates: 01/15/2024 or 01-15-2024
    text = text.replace(/(\d{2})[-/](\d{2})[-/](\d{4})/g, (_, month, day, year) => {
      const months = ['', 'January', 'February', 'March', 'April', 'May', 'June',
        'July', 'August', 'September', 'October', 'November', 'December'];
      const m = parseInt(month, 10);
      const d = parseInt(day, 10);
      if (m > 0 && m <= 12) {
        return `${months[m]} ${d}, ${year}`;
      }
      return `${month}/${day}/${year}`;
    });

    // Times: 14:30 or 14:30:00
    text = text.replace(/(\d{1,2}):(\d{2})(?::(\d{2}))?(?!\d)/g, (_, hours, minutes, seconds) => {
      const h = parseInt(hours, 10);
      const m = parseInt(minutes, 10);
      const ampm = h >= 12 ? 'PM' : 'AM';
      const h12 = h % 12 || 12;
      let spoken = `${h12}:${minutes} ${ampm}`;
      if (seconds) {
        spoken += ` and ${seconds} seconds`;
      }
      return spoken;
    });

    return text;
  }

  // Convert numbers to spoken format
  private convertNumbersToSpeech(text: string): string {
    // Percentages
    text = text.replace(/(\d+\.?\d*)%/g, '$1 percent');

    // Currency with large numbers: $1,234,567.89
    text = text.replace(/\$(\d{1,3}(?:,\d{3})*(?:\.\d+)?)/g, (_, num) => {
      const value = parseFloat(num.replace(/,/g, ''));
      return this.numberToWords(value, 'dollars');
    });

    // Large numbers with commas: 1,234,567
    text = text.replace(/\b(\d{1,3}(?:,\d{3})+)\b/g, (_, num) => {
      const value = parseFloat(num.replace(/,/g, ''));
      return this.numberToWords(value);
    });

    // Basis points
    text = text.replace(/(\d+)\s*(?:bps|basis points)/gi, '$1 basis points');

    // Ratios: 1:2 or 3:1
    text = text.replace(/(\d+):(\d+)/g, '$1 to $2');

    return text;
  }

  // Convert number to spoken words for large numbers
  private numberToWords(num: number, unit?: string): string {
    if (num >= 1e12) {
      const val = (num / 1e12).toFixed(2);
      return `${val} trillion${unit ? ' ' + unit : ''}`;
    } else if (num >= 1e9) {
      const val = (num / 1e9).toFixed(2);
      return `${val} billion${unit ? ' ' + unit : ''}`;
    } else if (num >= 1e6) {
      const val = (num / 1e6).toFixed(2);
      return `${val} million${unit ? ' ' + unit : ''}`;
    } else if (num >= 1e3) {
      const val = (num / 1e3).toFixed(1);
      return `${val} thousand${unit ? ' ' + unit : ''}`;
    }
    return unit ? `${num} ${unit}` : num.toString();
  }

  // Expand common abbreviations
  private expandAbbreviations(text: string): string {
    const abbreviations: Record<string, string> = {
      // Financial
      'P/E': 'price to earnings ratio',
      'EPS': 'earnings per share',
      'ROI': 'return on investment',
      'ROE': 'return on equity',
      'ROA': 'return on assets',
      'ROCE': 'return on capital employed',
      'EBIT': 'earnings before interest and taxes',
      'EBITDA': 'earnings before interest, taxes, depreciation and amortization',
      'DCF': 'discounted cash flow',
      'NPV': 'net present value',
      'IRR': 'internal rate of return',
      'WACC': 'weighted average cost of capital',
      'CAGR': 'compound annual growth rate',
      'YoY': 'year over year',
      'QoQ': 'quarter over quarter',
      'MoM': 'month over month',
      'YTD': 'year to date',
      'TTM': 'trailing twelve months',
      'AUM': 'assets under management',
      'NAV': 'net asset value',
      'IPO': 'initial public offering',
      'M&A': 'mergers and acquisitions',
      'PE': 'private equity',
      'VC': 'venture capital',
      'ETF': 'exchange traded fund',
      'GDP': 'gross domestic product',
      'CPI': 'consumer price index',
      'PPI': 'producer price index',
      'Fed': 'Federal Reserve',
      'SEC': 'Securities and Exchange Commission',
      'NYSE': 'New York Stock Exchange',
      'NASDAQ': 'NASDAQ stock exchange',
      // Technical
      'API': 'A P I',
      'URL': 'U R L',
      'AI': 'A I',
      'ML': 'machine learning',
      'NLP': 'natural language processing',
    };

    for (const [abbr, full] of Object.entries(abbreviations)) {
      // Match whole words only
      const regex = new RegExp(`\\b${abbr}\\b`, 'g');
      text = text.replace(regex, full);
    }

    return text;
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
