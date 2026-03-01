/**
 * SQLite Service — Constants
 *
 * Static API key definitions and LLM provider configurations
 * extracted from sqliteService.ts.
 */

// ==================== API KEYS ====================

export interface ApiKeys {
  FRED_API_KEY?: string;
  ALPHA_VANTAGE_API_KEY?: string;
  OPENAI_API_KEY?: string;
  ANTHROPIC_API_KEY?: string;
  COINGECKO_API_KEY?: string;
  NASDAQ_API_KEY?: string;
  FINANCIAL_MODELING_PREP_API_KEY?: string;
  OPENROUTER_API_KEY?: string;
  DATABENTO_API_KEY?: string;
  WTO_API_KEY?: string;
  EIA_API_KEY?: string;
  BLS_API_KEY?: string;
  BEA_API_KEY?: string;
  [key: string]: string | undefined;
}

// Data API Keys (non-LLM providers)
export const PREDEFINED_API_KEYS = [
  { key: 'FRED_API_KEY', label: 'FRED API Key', description: 'Federal Reserve Economic Data' },
  { key: 'ALPHA_VANTAGE_API_KEY', label: 'Alpha Vantage API Key', description: 'Stock & crypto data' },
  { key: 'COINGECKO_API_KEY', label: 'CoinGecko API Key', description: 'Cryptocurrency data' },
  { key: 'NASDAQ_API_KEY', label: 'NASDAQ API Key', description: 'NASDAQ market data' },
  { key: 'FINANCIAL_MODELING_PREP_API_KEY', label: 'Financial Modeling Prep', description: 'Financial statements & ratios' },
  { key: 'DATABENTO_API_KEY', label: 'Databento API Key', description: 'Institutional-grade market data (Options, Equities, Futures)' },
  { key: 'OPENROUTER_API_KEY', label: 'OpenRouter API Key', description: 'Access 400+ AI models from all providers (Model Library)' },
  { key: 'WTO_API_KEY', label: 'WTO API Key', description: 'World Trade Organization - Trade statistics, restrictions & notifications' },
  { key: 'EIA_API_KEY', label: 'EIA API Key', description: 'U.S. Energy Information Administration - Petroleum, natural gas & energy data' },
  { key: 'BLS_API_KEY', label: 'BLS API Key', description: 'Bureau of Labor Statistics - Employment, wages, CPI & productivity data' },
  { key: 'BEA_API_KEY', label: 'BEA API Key', description: 'Bureau of Economic Analysis - GDP, income, spending & national accounts (NIPA)' },
] as const;

// LLM Provider configurations
export const LLM_PROVIDERS = [
  {
    id: 'fincept',
    name: 'Fincept LLM',
    description: 'Fincept Research API (Default - 5 credits/response)',
    endpoint: 'https://api.fincept.in/research/llm',
    requiresApiKey: true,
    isDefault: true,
    models: ['fincept-llm']
  },
  {
    id: 'openai',
    name: 'OpenAI',
    description: 'GPT models',
    endpoint: 'https://api.openai.com/v1',
    requiresApiKey: true,
    models: ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'o1', 'o1-mini']
  },
  {
    id: 'anthropic',
    name: 'Anthropic',
    description: 'Claude models',
    endpoint: 'https://api.anthropic.com/v1',
    requiresApiKey: true,
    models: ['claude-sonnet-4-20250514', 'claude-3-5-sonnet-20241022', 'claude-3-5-haiku-20241022', 'claude-3-opus-20240229']
  },
  {
    id: 'google',
    name: 'Google Gemini',
    description: 'Gemini models',
    endpoint: 'https://generativelanguage.googleapis.com/v1beta',
    requiresApiKey: true,
    models: ['gemini-2.5-flash-preview-05-20', 'gemini-2.5-pro-preview-05-06', 'gemini-2.0-flash', 'gemini-2.0-flash-lite', 'gemini-1.5-pro', 'gemini-1.5-flash']
  },
  {
    id: 'ollama',
    name: 'Ollama (Local)',
    description: 'Local LLM models',
    endpoint: 'http://localhost:11434',
    requiresApiKey: false,
    models: [] // Dynamically loaded
  },
  {
    id: 'openrouter',
    name: 'OpenRouter',
    description: 'Access to multiple LLM providers',
    endpoint: 'https://openrouter.ai/api/v1',
    requiresApiKey: true,
    supportsCustomModels: true,
    models: []
  },
  {
    id: 'groq',
    name: 'Groq',
    description: 'Fast inference for open models',
    endpoint: 'https://api.groq.com/openai/v1',
    requiresApiKey: true,
    models: ['llama-3.3-70b-versatile', 'llama-3.1-70b-versatile', 'llama-3.1-8b-instant', 'mixtral-8x7b-32768', 'gemma2-9b-it']
  },
  {
    id: 'deepseek',
    name: 'DeepSeek',
    description: 'Advanced reasoning models',
    endpoint: 'https://api.deepseek.com',
    requiresApiKey: true,
    models: ['deepseek-chat', 'deepseek-coder', 'deepseek-reasoner']
  },
] as const;
