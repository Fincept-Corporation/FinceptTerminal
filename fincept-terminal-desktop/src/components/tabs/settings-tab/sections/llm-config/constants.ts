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

// Providers that require base URL configuration
export const PROVIDERS_WITH_BASE_URL = [
  LLM_PROVIDERS.OLLAMA,
  LLM_PROVIDERS.DEEPSEEK,
  LLM_PROVIDERS.OPENROUTER,
] as const;

// Provider options for the Add Model form dropdown
export const PROVIDER_OPTIONS = [
  { value: LLM_PROVIDERS.OPENAI, label: 'OpenAI' },
  { value: LLM_PROVIDERS.ANTHROPIC, label: 'Anthropic' },
  { value: LLM_PROVIDERS.GOOGLE, label: 'Google' },
  { value: LLM_PROVIDERS.META_LLAMA, label: 'Meta (Llama)' },
  { value: LLM_PROVIDERS.MISTRAL, label: 'Mistral AI' },
  { value: LLM_PROVIDERS.XAI, label: 'xAI (Grok)' },
  { value: LLM_PROVIDERS.COHERE, label: 'Cohere' },
  { value: LLM_PROVIDERS.DEEPSEEK, label: 'DeepSeek' },
  { value: LLM_PROVIDERS.QWEN, label: 'Qwen (Alibaba)' },
  { value: LLM_PROVIDERS.NVIDIA, label: 'NVIDIA' },
  { value: LLM_PROVIDERS.MICROSOFT, label: 'Microsoft' },
  { value: LLM_PROVIDERS.PERPLEXITY, label: 'Perplexity' },
  { value: LLM_PROVIDERS.GROQ, label: 'Groq' },
  { value: LLM_PROVIDERS.AI21, label: 'AI21 Labs' },
  { value: LLM_PROVIDERS.NOUS_RESEARCH, label: 'Nous Research' },
  { value: LLM_PROVIDERS.COGNITIVE_COMPUTATIONS, label: 'Cognitive Computations' },
] as const;

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
