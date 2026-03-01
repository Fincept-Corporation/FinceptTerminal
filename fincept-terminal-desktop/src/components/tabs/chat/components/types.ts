export interface QuickPrompt {
  cmd: string;
  prompt: string;
}

// Constants
export const SESSION_CREATION_DELAY_MS = 100;
export const MAX_MESSAGES_IN_MEMORY = 200;
export const MCP_RETRY_INTERVAL_MS = 10000;
export const MCP_MAX_RETRIES = 5;
export const CLOCK_UPDATE_INTERVAL_MS = 1000;
export const MCP_SUPPORTED_PROVIDERS = ['openai', 'anthropic', 'gemini', 'google', 'groq', 'deepseek', 'openrouter', 'fincept'];
export const STREAMING_SUPPORTED_PROVIDERS = ['openai', 'anthropic', 'gemini', 'google', 'groq', 'deepseek', 'openrouter', 'ollama', 'fincept'];
export const NO_API_KEY_PROVIDERS = ['ollama', 'fincept']; // Providers that don't require API key
export const API_TIMEOUT_MS = 30000;
export const DB_TIMEOUT_MS = 10000;

export const DEFAULT_QUICK_PROMPTS: QuickPrompt[] = [
  { cmd: 'MARKET TRENDS', prompt: 'Analyze current market trends and key insights' },
  { cmd: 'PORTFOLIO', prompt: 'Portfolio diversification recommendations' },
  { cmd: 'RISK', prompt: 'Investment risk assessment strategies' },
  { cmd: 'TECHNICAL', prompt: 'Key technical analysis indicators' }
];
