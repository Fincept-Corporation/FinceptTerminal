// LLM Configuration Constants

// Provider identifiers
export const LLM_PROVIDERS = {
  FINCEPT: 'fincept',
  OLLAMA: 'ollama',
  OPENAI: 'openai',
  ANTHROPIC: 'anthropic',
  GOOGLE: 'google',
  META_LLAMA: 'meta-llama',
  MISTRAL: 'mistralai',
  XAI: 'x-ai',
  COHERE: 'cohere',
  DEEPSEEK: 'deepseek',
  QWEN: 'qwen',
  NVIDIA: 'nvidia',
  MICROSOFT: 'microsoft',
  PERPLEXITY: 'perplexity',
  GROQ: 'groq',
  AI21: 'ai21',
  NOUS_RESEARCH: 'nousresearch',
  COGNITIVE_COMPUTATIONS: 'cognitivecomputations',
  OPENROUTER: 'openrouter',
} as const;

// Special providers that are always visible (don't require API key)
export const SPECIAL_PROVIDERS = [LLM_PROVIDERS.FINCEPT, LLM_PROVIDERS.OLLAMA] as const;

// Default new model configuration
export const DEFAULT_NEW_MODEL_CONFIG = {
  provider: LLM_PROVIDERS.OPENAI,
  model_id: '',
  display_name: '',
  api_key: '',
  base_url: '',
} as const;

// Default Ollama base URL
export const DEFAULT_OLLAMA_BASE_URL = 'http://localhost:11434';

// Special value for custom model entry
export const CUSTOM_MODEL_VALUE = '__custom__';
