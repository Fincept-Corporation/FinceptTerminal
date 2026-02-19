/**
 * Agent Config Tab - Shared Types
 */

import type {
  AgentCard,
  ExecutionPlan,
  RoutingResult,
} from '@/services/agentService';
import type { LLMConfig as DbLLMConfig } from '@/services/core/sqliteService';

// Re-export service types used across views
export type { AgentCard, ExecutionPlan, RoutingResult, DbLLMConfig };

// ─── View Mode ────────────────────────────────────────────────────────────────

export type ViewMode = 'agents' | 'teams' | 'workflows' | 'planner' | 'tools' | 'system' | 'chat' | 'create';

// ─── Chat ─────────────────────────────────────────────────────────────────────

export interface ChatMessage {
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: Date;
  agentName?: string;
  routingInfo?: RoutingResult;
}

// ─── Agent Config Editor ──────────────────────────────────────────────────────

export interface EditedConfig {
  provider: string;
  model_id: string;
  temperature: number;
  max_tokens: number;
  tools: string[];
  instructions: string;
  enable_memory: boolean;
  enable_reasoning: boolean;
  enable_knowledge: boolean;
  enable_guardrails: boolean;
  enable_agentic_memory: boolean;
  enable_storage: boolean;
  enable_tracing: boolean;
  enable_compression: boolean;
  enable_hooks: boolean;
  enable_evaluation: boolean;
  // Knowledge config
  knowledge_type: 'url' | 'pdf' | 'text' | 'combined';
  knowledge_urls: string;
  knowledge_vector_db: string;
  knowledge_embedder: string;
  // Guardrails config
  guardrails_pii: boolean;
  guardrails_injection: boolean;
  guardrails_compliance: boolean;
  // Storage config
  storage_type: 'sqlite' | 'postgres';
  storage_db_path: string;
  storage_table: string;
  // Agentic Memory config
  agentic_memory_user_id: string;
  // Memory config
  memory_db_path: string;
  memory_table: string;
  memory_create_user_memories: boolean;
  memory_create_session_summary: boolean;
}

// ─── Tab Session ──────────────────────────────────────────────────────────────

export interface AgentTabSessionState {
  viewMode: ViewMode;
  selectedCategory: string;
  testQuery: string;
  useAutoRouting: boolean;
  teamMode: 'coordinate' | 'route' | 'collaborate';
  workflowSymbol: string;
  selectedOutputModel: string;
}

// ─── Constants ────────────────────────────────────────────────────────────────

export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
} as const;

export const TERMINAL_STYLES = `
  .agent-terminal * {
    font-family: "IBM Plex Mono", "Consolas", monospace;
  }
  .agent-terminal::-webkit-scrollbar { width: 6px; height: 6px; }
  .agent-terminal::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
  .agent-terminal::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
  .agent-terminal::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
  .agent-hover:hover { background-color: ${FINCEPT.HOVER} !important; }
  .agent-selected {
    background-color: ${FINCEPT.ORANGE}15 !important;
    border-left: 2px solid ${FINCEPT.ORANGE} !important;
  }
  @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
  .agent-pulse { animation: pulse 2s infinite; }
`;

export const VIEW_MODES = [
  { id: 'agents' as ViewMode, name: 'AGENTS' },
  { id: 'teams' as ViewMode, name: 'TEAMS' },
  { id: 'workflows' as ViewMode, name: 'WORKFLOWS' },
  { id: 'planner' as ViewMode, name: 'PLANNER' },
  { id: 'tools' as ViewMode, name: 'TOOLS' },
  { id: 'chat' as ViewMode, name: 'CHAT' },
  { id: 'system' as ViewMode, name: 'SYSTEM' },
] as const;

export const OUTPUT_MODELS = [
  { id: 'trade_signal', name: 'Trade Signal', desc: 'Buy/Sell signals with confidence' },
  { id: 'stock_analysis', name: 'Stock Analysis', desc: 'Comprehensive stock report' },
  { id: 'portfolio_analysis', name: 'Portfolio Analysis', desc: 'Portfolio metrics & allocation' },
  { id: 'risk_assessment', name: 'Risk Assessment', desc: 'Risk metrics & warnings' },
  { id: 'market_analysis', name: 'Market Analysis', desc: 'Market conditions & trends' },
] as const;

export const AGENT_CACHE_KEYS = {
  DISCOVERED_AGENTS: 'agents:discovered-agents',
  SYSTEM_INFO: 'agents:system-info',
  TOOLS_INFO: 'agents:tools-info',
} as const;

export const DEFAULT_EDITED_CONFIG: EditedConfig = {
  provider: 'fincept',
  model_id: 'fincept-llm',
  temperature: 0.7,
  max_tokens: 4096,
  tools: [],
  instructions: '',
  enable_memory: false,
  enable_reasoning: false,
  enable_knowledge: false,
  enable_guardrails: false,
  enable_agentic_memory: false,
  enable_storage: false,
  enable_tracing: false,
  enable_compression: false,
  enable_hooks: false,
  enable_evaluation: false,
  knowledge_type: 'url',
  knowledge_urls: '',
  knowledge_vector_db: 'lancedb',
  knowledge_embedder: 'openai',
  guardrails_pii: true,
  guardrails_injection: true,
  guardrails_compliance: false,
  storage_type: 'sqlite',
  storage_db_path: '',
  storage_table: 'agent_sessions',
  agentic_memory_user_id: '',
  memory_db_path: '',
  memory_table: 'agent_memory',
  memory_create_user_memories: true,
  memory_create_session_summary: true,
};

export const DEFAULT_TAB_SESSION: AgentTabSessionState = {
  viewMode: 'agents',
  selectedCategory: 'all',
  testQuery: 'Analyze AAPL stock and provide your investment thesis.',
  useAutoRouting: true,
  teamMode: 'coordinate',
  workflowSymbol: 'AAPL',
  selectedOutputModel: '',
};

/**
 * Extract the actual readable text from an agent response string.
 */
export function extractAgentResponseText(raw: string): string {
  if (!raw || typeof raw !== 'string') return raw || '';

  try {
    const parsed = JSON.parse(raw);
    if (typeof parsed === 'object' && parsed !== null) {
      if (parsed.data?.response && typeof parsed.data.response === 'string') {
        return parsed.data.response;
      }
      if (parsed.response && typeof parsed.response === 'string') {
        return extractAgentResponseText(parsed.response);
      }
      if (parsed.result && typeof parsed.result === 'string') {
        return parsed.result;
      }
    }
  } catch {
    // Not valid JSON
  }

  try {
    const jsonified = raw
      .replace(/'/g, '"')
      .replace(/\bTrue\b/g, 'true')
      .replace(/\bFalse\b/g, 'false')
      .replace(/\bNone\b/g, 'null');
    const parsed = JSON.parse(jsonified);
    if (typeof parsed === 'object' && parsed !== null) {
      if (parsed.data?.response && typeof parsed.data.response === 'string') {
        return parsed.data.response;
      }
      if (parsed.response && typeof parsed.response === 'string') {
        return parsed.response;
      }
    }
  } catch {
    // Not parseable
  }

  return raw;
}
