/**
 * Polymarket Bot Service
 *
 * Orchestrates autonomous trading bots on Polymarket prediction markets.
 * Each bot runs an agent polling loop: analyse → decide → (approve) → execute.
 *
 * Key design choices:
 * - Risk checks are enforced here in TypeScript, never trusted to the LLM prompt.
 * - Trade execution requires explicit user approval by default (requireApproval: true).
 * - All decisions are persisted via storageService so they survive restarts.
 * - Bots leverage the full finagent_core stack: memory, reasoning, MCP tools.
 */

import { invoke } from '@tauri-apps/api/core';
import { storageService } from '@/services/core/storageService';
import { buildApiKeysMap } from '@/services/core/sqliteService';
import { runAgent, buildAgentConfig, saveMemoryRepo, recallMemories, searchMemoriesRepo } from '@/services/agentService';
import { polymarketApiService } from './polymarketApiService';
import type { AgentConfig } from '@/services/agentService';

// ─── Storage Keys ─────────────────────────────────────────────────────────────

const STORAGE_KEY_BOTS = 'polymarket:bots';
const STORAGE_KEY_DECISIONS_PREFIX = 'polymarket:decisions:';

// ─── Types ────────────────────────────────────────────────────────────────────

export type BotStatus = 'idle' | 'running' | 'paused' | 'stopped' | 'error';

export type StrategyType = 'analyst' | 'contrarian' | 'momentum' | 'arbitrage' | 'custom';

export interface BotStrategy {
  /** Broad strategy archetype */
  type: StrategyType;
  /** Custom system-prompt additions; merged with base instructions */
  customInstructions: string;
  /** Gamma API category slugs to focus on, e.g. ['politics', 'crypto'] */
  focusCategories: string[];
  /** Milliseconds between analysis cycles (min 30 000) */
  scanIntervalMs: number;
  /** Maximum number of simultaneous open positions */
  maxOpenPositions: number;
  /** Agent confidence threshold 0–100; recommendations below this are ignored */
  minConfidence: number;
  /** Maximum markets to scan per cycle */
  marketsPerScan: number;
}

export interface RiskConfig {
  /** Total USDC capital the bot may have at risk at any point */
  maxExposureUsdc: number;
  /** Maximum USDC per single market position */
  maxPerMarketUsdc: number;
  /** Exit if position drops below entry by this % (e.g. 30 = stop at -30%) */
  stopLossPct: number;
  /** Exit if position gains this % (e.g. 50 = take profit at +50%) */
  takeProfitPct: number;
  /** Cumulative daily loss limit in USDC; bot pauses if hit */
  maxDailyLossUsdc: number;
  /**
   * When true (default), trades are queued for user approval before execution.
   * When false, trades execute immediately if risk checks pass.
   */
  requireApproval: boolean;
}

export interface BotLLMConfig {
  provider: string;
  modelId: string;
  temperature: number;
  maxTokens: number;
  enableMemory: boolean;
  enableReasoning: boolean;
  enableGuardrails: boolean;
}

export interface BotPosition {
  id: string;
  botId: string;
  marketId: string;
  marketQuestion: string;
  /** YES or NO */
  outcome: 'YES' | 'NO';
  tokenId: string;
  entryPrice: number;
  currentPrice: number;
  sizeUsdc: number;
  orderId?: string;
  openedAt: string;
  closedAt?: string;
  closeReason?: 'take_profit' | 'stop_loss' | 'manual' | 'expired';
  realizedPnlUsdc?: number;
  unrealizedPnlPct: number;
}

export type DecisionAction = 'buy_yes' | 'buy_no' | 'sell_yes' | 'sell_no' | 'hold' | 'skip';

export interface BotDecision {
  id: string;
  botId: string;
  timestamp: string;
  marketId: string;
  marketQuestion: string;
  action: DecisionAction;
  outcome: 'YES' | 'NO' | null;
  tokenId: string | null;
  confidence: number;
  reasoning: string;
  riskFactors: string[];
  priceAtDecision: number;
  assessedProbability: number;
  recommendedSizeUsdc: number;
  /** Whether the bot service executed or queued this decision */
  executed: boolean;
  /** Awaiting user approval */
  pendingApproval: boolean;
  /** User approved/rejected (null = pending) */
  approved: boolean | null;
  orderId?: string;
}

export interface BotStats {
  totalCycles: number;
  totalDecisions: number;
  tradesExecuted: number;
  tradesWon: number;
  tradesLost: number;
  totalPnlUsdc: number;
  dailyPnlUsdc: number;
  winRate: number;
  lastCycleAt: string | null;
  createdAt: string;
}

export interface PolymarketBot {
  id: string;
  name: string;
  status: BotStatus;
  strategy: BotStrategy;
  riskConfig: RiskConfig;
  llmConfig: BotLLMConfig;
  positions: BotPosition[];
  stats: BotStats;
  lastError?: string;
}

// ─── Default Configs ──────────────────────────────────────────────────────────

export const DEFAULT_STRATEGY: BotStrategy = {
  type: 'analyst',
  customInstructions: '',
  focusCategories: [],
  scanIntervalMs: 5 * 60 * 1000, // 5 minutes
  maxOpenPositions: 3,
  minConfidence: 70,
  marketsPerScan: 20,
};

export const DEFAULT_RISK_CONFIG: RiskConfig = {
  maxExposureUsdc: 100,
  maxPerMarketUsdc: 30,
  stopLossPct: 40,
  takeProfitPct: 60,
  maxDailyLossUsdc: 50,
  requireApproval: true,
};

export const DEFAULT_LLM_CONFIG: BotLLMConfig = {
  provider: 'fincept',
  modelId: 'fincept-llm',
  temperature: 0.3,
  maxTokens: 4096,
  enableMemory: true,
  enableReasoning: false,
  enableGuardrails: true,
};

// ─── Agent Instructions ───────────────────────────────────────────────────────

function buildAgentInstructions(bot: PolymarketBot): string {
  const strategyBlocks: Record<StrategyType, string> = {
    analyst: `Evaluate markets objectively. Look for mispricings where the crowd probability deviates significantly from the true base rate. Use news and fundamentals.`,
    contrarian: `Look for overcrowded positions. Markets priced above 0.85 or below 0.15 that may be driven by sentiment rather than fundamentals. Bet against the crowd when justified.`,
    momentum: `Identify markets with strong directional price movement in the last 1-7 days. Follow momentum but set tight stop-losses.`,
    arbitrage: `Look for related markets with inconsistent pricing (e.g., candidate A wins + candidate B wins sum to > 1.0). Flag arbitrage spreads.`,
    custom: ``,
  };

  const focusNote = bot.strategy.focusCategories.length
    ? `Focus on these market categories: ${bot.strategy.focusCategories.join(', ')}.`
    : 'Scan all available market categories.';

  return `You are a Polymarket trading agent running in autonomous mode.

${strategyBlocks[bot.strategy.type]}
${focusNote}

Your task each cycle:
1. Use polymarket_get_markets to fetch active markets (limit ${bot.strategy.marketsPerScan}, order by volume, active: true)
2. Recall your previous trade decisions and outcomes using recall_memories
3. Identify markets where current price deviates from your assessed true probability by at least 5%
4. For top 3–5 candidates: use polymarket_get_order_book to check liquidity
5. Use polymarket_get_price_history (interval: 1w) for trend context
6. Only recommend if confidence >= ${bot.strategy.minConfidence}

Output ONLY a JSON array of trade recommendations. Each item:
{
  "market_id": "string",
  "market_question": "string",
  "action": "buy_yes" | "buy_no" | "hold" | "skip",
  "outcome": "YES" | "NO" | null,
  "token_id": "string or null",
  "current_price": 0.0,
  "assessed_probability": 0.0,
  "confidence": 0,
  "recommended_size_usdc": 0.0,
  "reasoning": "string (max 200 chars)",
  "risk_factors": ["string"],
  "time_horizon": "string"
}

If no opportunities meet the threshold, output: []

${bot.strategy.customInstructions ? `\nAdditional instructions:\n${bot.strategy.customInstructions}` : ''}`;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

function generateId(): string {
  return `bot_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
}

function generateDecisionId(): string {
  return `dec_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
}

function createInitialStats(): BotStats {
  return {
    totalCycles: 0,
    totalDecisions: 0,
    tradesExecuted: 0,
    tradesWon: 0,
    tradesLost: 0,
    totalPnlUsdc: 0,
    dailyPnlUsdc: 0,
    winRate: 0,
    lastCycleAt: null,
    createdAt: new Date().toISOString(),
  };
}

// ─── Parse Agent Response ─────────────────────────────────────────────────────

function parseAgentDecisions(raw: string): Array<Record<string, any>> {
  if (!raw) return [];
  try {
    // Strip markdown code fences if present
    const cleaned = raw
      .replace(/```json\s*/gi, '')
      .replace(/```\s*/g, '')
      .trim();
    const parsed = JSON.parse(cleaned);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    // Try to extract JSON array from surrounding text
    const match = raw.match(/\[[\s\S]*\]/);
    if (match) {
      try {
        const parsed = JSON.parse(match[0]);
        return Array.isArray(parsed) ? parsed : [];
      } catch {
        return [];
      }
    }
    return [];
  }
}

// ─── Risk Checks ──────────────────────────────────────────────────────────────

interface RiskCheckResult {
  passed: boolean;
  reason?: string;
}

function checkRiskLimits(
  bot: PolymarketBot,
  recommendedSizeUsdc: number,
): RiskCheckResult {
  const { riskConfig, positions, stats } = bot;

  // Max exposure check
  const currentExposure = positions
    .filter((p) => !p.closedAt)
    .reduce((sum, p) => sum + p.sizeUsdc, 0);
  if (currentExposure + recommendedSizeUsdc > riskConfig.maxExposureUsdc) {
    return {
      passed: false,
      reason: `Max exposure ${riskConfig.maxExposureUsdc} USDC would be exceeded (current: ${currentExposure.toFixed(2)})`,
    };
  }

  // Per-market size check
  if (recommendedSizeUsdc > riskConfig.maxPerMarketUsdc) {
    return {
      passed: false,
      reason: `Recommended size ${recommendedSizeUsdc} USDC exceeds per-market limit ${riskConfig.maxPerMarketUsdc}`,
    };
  }

  // Max open positions check
  const openPositions = positions.filter((p) => !p.closedAt).length;
  if (openPositions >= bot.strategy.maxOpenPositions) {
    return {
      passed: false,
      reason: `Max open positions (${bot.strategy.maxOpenPositions}) already reached`,
    };
  }

  // Daily loss check
  if (stats.dailyPnlUsdc <= -riskConfig.maxDailyLossUsdc) {
    return {
      passed: false,
      reason: `Daily loss limit ${riskConfig.maxDailyLossUsdc} USDC reached`,
    };
  }

  return { passed: true };
}

// ─── SQLite Sync Helpers ──────────────────────────────────────────────────────

async function syncBotToDb(bot: PolymarketBot): Promise<void> {
  await invoke('pm_bot_upsert', {
    bot: {
      id: bot.id,
      name: bot.name,
      status: bot.status,
      strategy_json: JSON.stringify(bot.strategy),
      risk_config_json: JSON.stringify(bot.riskConfig),
      llm_config_json: JSON.stringify(bot.llmConfig),
      stats_json: JSON.stringify(bot.stats),
      last_error: bot.lastError ?? null,
      created_at: bot.stats.createdAt,
    },
  }).catch((e) => console.warn('[BotService] DB sync failed:', e));
}

async function syncDecisionToDb(decision: BotDecision): Promise<void> {
  await invoke('pm_decision_insert', {
    d: {
      id: decision.id,
      bot_id: decision.botId,
      timestamp: decision.timestamp,
      market_id: decision.marketId,
      market_question: decision.marketQuestion,
      action: decision.action,
      outcome: decision.outcome,
      token_id: decision.tokenId,
      confidence: decision.confidence,
      reasoning: decision.reasoning,
      risk_factors: JSON.stringify(decision.riskFactors),
      price_at_decision: decision.priceAtDecision,
      assessed_probability: decision.assessedProbability,
      recommended_size_usdc: decision.recommendedSizeUsdc,
      executed: decision.executed,
      pending_approval: decision.pendingApproval,
      approved: decision.approved,
      order_id: decision.orderId ?? null,
    },
  }).catch((e) => console.warn('[BotService] Decision DB sync failed:', e));
}

async function syncPositionToDb(position: BotPosition): Promise<void> {
  await invoke('pm_position_upsert', {
    p: {
      id: position.id,
      bot_id: position.botId,
      market_id: position.marketId,
      market_question: position.marketQuestion,
      outcome: position.outcome,
      token_id: position.tokenId,
      entry_price: position.entryPrice,
      current_price: position.currentPrice,
      size_usdc: position.sizeUsdc,
      order_id: position.orderId ?? null,
      opened_at: position.openedAt,
      closed_at: position.closedAt ?? null,
      close_reason: position.closeReason ?? null,
      realized_pnl_usdc: position.realizedPnlUsdc ?? null,
      unrealized_pnl_pct: position.unrealizedPnlPct,
    },
  }).catch((e) => console.warn('[BotService] Position DB sync failed:', e));
}

// ─── Service Class ────────────────────────────────────────────────────────────

class PolymarketBotService {
  private bots: Map<string, PolymarketBot> = new Map();
  private timers: Map<string, ReturnType<typeof setInterval>> = new Map();
  private listeners: Set<() => void> = new Set();
  private loaded = false;

  // ── Persistence ─────────────────────────────────────────────────────────────

  private async load(): Promise<void> {
    if (this.loaded) return;
    const stored = await storageService.getJSON<PolymarketBot[]>(STORAGE_KEY_BOTS);
    if (stored) {
      for (const bot of stored) {
        // Restore bots as idle on load — timers are not persisted
        this.bots.set(bot.id, { ...bot, status: bot.status === 'running' ? 'idle' : bot.status });
      }
    }
    this.loaded = true;
  }

  private async save(): Promise<void> {
    const bots = Array.from(this.bots.values());
    await storageService.setJSON(STORAGE_KEY_BOTS, bots);
    // Mirror to dedicated SQLite table for queryability
    await Promise.all(bots.map((b) => syncBotToDb(b)));
  }

  private async loadDecisions(botId: string): Promise<BotDecision[]> {
    return (await storageService.getJSON<BotDecision[]>(`${STORAGE_KEY_DECISIONS_PREFIX}${botId}`)) ?? [];
  }

  private async saveDecisions(botId: string, decisions: BotDecision[]): Promise<void> {
    // Keep last 200 decisions per bot to avoid unbounded growth
    const trimmed = decisions.slice(-200);
    await storageService.setJSON(`${STORAGE_KEY_DECISIONS_PREFIX}${botId}`, trimmed);
  }

  private notify(): void {
    this.listeners.forEach((fn) => fn());
  }

  // ── Public State ─────────────────────────────────────────────────────────────

  subscribe(listener: () => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  async getBots(): Promise<PolymarketBot[]> {
    await this.load();
    return Array.from(this.bots.values());
  }

  async getBot(id: string): Promise<PolymarketBot | null> {
    await this.load();
    return this.bots.get(id) ?? null;
  }

  async getDecisions(botId: string): Promise<BotDecision[]> {
    return this.loadDecisions(botId);
  }

  // ── Lifecycle ────────────────────────────────────────────────────────────────

  async createBot(params: {
    name: string;
    strategy?: Partial<BotStrategy>;
    riskConfig?: Partial<RiskConfig>;
    llmConfig?: Partial<BotLLMConfig>;
  }): Promise<PolymarketBot> {
    await this.load();

    const bot: PolymarketBot = {
      id: generateId(),
      name: params.name,
      status: 'idle',
      strategy: { ...DEFAULT_STRATEGY, ...params.strategy },
      riskConfig: { ...DEFAULT_RISK_CONFIG, ...params.riskConfig },
      llmConfig: { ...DEFAULT_LLM_CONFIG, ...params.llmConfig },
      positions: [],
      stats: createInitialStats(),
    };

    this.bots.set(bot.id, bot);
    await this.save();
    this.notify();
    return bot;
  }

  async updateBot(
    id: string,
    updates: Partial<Pick<PolymarketBot, 'name' | 'strategy' | 'riskConfig' | 'llmConfig'>>,
  ): Promise<PolymarketBot | null> {
    await this.load();
    const bot = this.bots.get(id);
    if (!bot) return null;
    if (bot.status === 'running') {
      throw new Error('Cannot update a running bot. Stop it first.');
    }
    const updated: PolymarketBot = {
      ...bot,
      ...(updates.name !== undefined && { name: updates.name }),
      ...(updates.strategy && { strategy: { ...bot.strategy, ...updates.strategy } }),
      ...(updates.riskConfig && { riskConfig: { ...bot.riskConfig, ...updates.riskConfig } }),
      ...(updates.llmConfig && { llmConfig: { ...bot.llmConfig, ...updates.llmConfig } }),
    };
    this.bots.set(id, updated);
    await this.save();
    this.notify();
    return updated;
  }

  async startBot(id: string): Promise<void> {
    await this.load();
    const bot = this.bots.get(id);
    if (!bot) throw new Error(`Bot ${id} not found`);
    if (bot.status === 'running') return;
    if (bot.strategy.scanIntervalMs < 30_000) {
      throw new Error('Scan interval must be at least 30 seconds');
    }

    this.updateBotStatus(id, 'running');

    // Run immediately, then on interval
    this.runCycle(id).catch((err) => {
      console.error(`[BotService] Initial cycle error for ${id}:`, err);
    });

    const timer = setInterval(() => {
      const current = this.bots.get(id);
      if (current?.status !== 'running') {
        clearInterval(timer);
        this.timers.delete(id);
        return;
      }
      this.runCycle(id).catch((err) => {
        console.error(`[BotService] Cycle error for ${id}:`, err);
      });
    }, bot.strategy.scanIntervalMs);

    this.timers.set(id, timer);
  }

  async stopBot(id: string): Promise<void> {
    const timer = this.timers.get(id);
    if (timer) {
      clearInterval(timer);
      this.timers.delete(id);
    }
    this.updateBotStatus(id, 'stopped');
  }

  async pauseBot(id: string): Promise<void> {
    const timer = this.timers.get(id);
    if (timer) {
      clearInterval(timer);
      this.timers.delete(id);
    }
    this.updateBotStatus(id, 'paused');
  }

  async deleteBot(id: string): Promise<void> {
    await this.stopBot(id);
    this.bots.delete(id);
    await this.save();
    // Clean up decisions from key-value storage
    await storageService.remove(`${STORAGE_KEY_DECISIONS_PREFIX}${id}`);
    // Delete from SQLite (CASCADE removes decisions + positions)
    await invoke('pm_bot_delete', { botId: id }).catch(() => {});
    this.notify();
  }

  // ── Trade Approval ───────────────────────────────────────────────────────────

  async approveDecision(botId: string, decisionId: string): Promise<void> {
    const decisions = await this.loadDecisions(botId);
    const decision = decisions.find((d) => d.id === decisionId);
    if (!decision || decision.action === 'hold' || decision.action === 'skip') return;

    decision.approved = true;
    decision.pendingApproval = false;

    await this.executeDecision(botId, decision);
    await this.saveDecisions(botId, decisions);

    // Mirror approval to SQLite
    await invoke('pm_decision_update_approval', {
      decisionId,
      approved: true,
      orderId: decision.orderId ?? null,
    }).catch(() => {});

    this.notify();
  }

  async rejectDecision(botId: string, decisionId: string): Promise<void> {
    const decisions = await this.loadDecisions(botId);
    const decision = decisions.find((d) => d.id === decisionId);
    if (!decision) return;
    decision.approved = false;
    decision.pendingApproval = false;
    await this.saveDecisions(botId, decisions);

    // Mirror rejection to SQLite
    await invoke('pm_decision_update_approval', {
      decisionId,
      approved: false,
      orderId: null,
    }).catch(() => {});

    this.notify();
  }

  // ── Core Cycle ───────────────────────────────────────────────────────────────

  private async runCycle(botId: string): Promise<void> {
    const bot = this.bots.get(botId);
    if (!bot || bot.status !== 'running') return;

    console.log(`[BotService] Running cycle for bot "${bot.name}" (${botId})`);

    try {
      // 1. Build agent config from bot's LLM config
      const agentConfig = this.buildBotAgentConfig(bot);

      // 2. Get API keys
      const apiKeys = await buildApiKeysMap();

      // 3. Build context-aware query (async — recalls memories)
      const query = await this.buildCycleQuery(bot);

      // 4. Run the agent
      const response = await runAgent(query, agentConfig, apiKeys, `bot_session_${botId}`);

      if (!response.success) {
        this.setBotError(botId, response.error ?? 'Agent returned failure');
        return;
      }

      // 5. Extract text response
      const rawText: string =
        typeof response.response === 'string'
          ? response.response
          : typeof response.result === 'string'
          ? response.result
          : JSON.stringify(response.data ?? '');

      // 6. Parse structured decisions from agent output
      const rawDecisions = parseAgentDecisions(rawText);

      // 7. Process each recommendation
      const decisions = await this.loadDecisions(botId);
      let newDecisionCount = 0;

      for (const raw of rawDecisions) {
        const action = (raw.action ?? 'skip') as DecisionAction;
        if (action === 'hold' || action === 'skip') continue;
        if ((raw.confidence ?? 0) < bot.strategy.minConfidence) continue;
        if (!raw.token_id || !raw.market_id) continue;

        const sizeUsdc = Math.min(
          raw.recommended_size_usdc ?? 10,
          bot.riskConfig.maxPerMarketUsdc,
        );

        const decision: BotDecision = {
          id: generateDecisionId(),
          botId,
          timestamp: new Date().toISOString(),
          marketId: raw.market_id,
          marketQuestion: raw.market_question ?? '',
          action,
          outcome: raw.outcome ?? null,
          tokenId: raw.token_id ?? null,
          confidence: raw.confidence ?? 0,
          reasoning: (raw.reasoning ?? '').slice(0, 300),
          riskFactors: Array.isArray(raw.risk_factors) ? raw.risk_factors : [],
          priceAtDecision: raw.current_price ?? 0,
          assessedProbability: raw.assessed_probability ?? 0,
          recommendedSizeUsdc: sizeUsdc,
          executed: false,
          pendingApproval: false,
          approved: null,
        };

        // Risk gate
        const riskCheck = checkRiskLimits(bot, sizeUsdc);
        if (!riskCheck.passed) {
          decision.action = 'skip';
          decision.reasoning = `Risk blocked: ${riskCheck.reason}`;
          decisions.push(decision);
          continue;
        }

        if (bot.riskConfig.requireApproval) {
          // Queue for user approval
          decision.pendingApproval = true;
          decisions.push(decision);
        } else {
          // Auto-execute
          decisions.push(decision);
          await this.executeDecision(botId, decision);
        }

        // Mirror decision to SQLite
        await syncDecisionToDb(decision);

        newDecisionCount++;

        // Save memory of this decision for future cycles
        await saveMemoryRepo(
          `Polymarket trade decision: ${action} on "${decision.marketQuestion}" at price ${decision.priceAtDecision} with assessed probability ${decision.assessedProbability} (confidence: ${decision.confidence}%, reasoning: ${decision.reasoning}, risk factors: ${decision.riskFactors.join('; ')})`,
          {
            agent_id: `polymarket_bot_${botId}`,
            type: 'trade_decision',
            importance: Math.round(decision.confidence / 10), // 0–10 scale
            metadata: {
              market_id: decision.marketId,
              action,
              price: decision.priceAtDecision,
              size_usdc: decision.recommendedSizeUsdc,
            },
          },
        ).catch(() => {/* non-fatal */});
      }

      // 8. Update stats
      const updatedBot = this.bots.get(botId);
      if (updatedBot) {
        updatedBot.stats.totalCycles += 1;
        updatedBot.stats.totalDecisions += newDecisionCount;
        updatedBot.stats.lastCycleAt = new Date().toISOString();
        updatedBot.lastError = undefined;
        this.bots.set(botId, updatedBot);
      }

      await this.saveDecisions(botId, decisions);
      await this.save();
      this.notify();
    } catch (error: any) {
      console.error(`[BotService] Cycle error for ${botId}:`, error);
      this.setBotError(botId, error?.message ?? 'Unknown error during cycle');
    }
  }

  // ── Execution ────────────────────────────────────────────────────────────────

  private async executeDecision(botId: string, decision: BotDecision): Promise<void> {
    const bot = this.bots.get(botId);
    if (!bot || !decision.tokenId) return;

    const isBuy = decision.action === 'buy_yes' || decision.action === 'buy_no';

    try {
      if (isBuy) {
        // Place limit order slightly above best ask for faster fill
        const orderBook = await polymarketApiService.getOrderBook(decision.tokenId).catch(() => null);
        const bestAsk = orderBook?.asks?.[0]?.price ?? String(decision.priceAtDecision + 0.01);
        const price = Math.min(parseFloat(bestAsk) + 0.005, 0.999).toFixed(3);
        const size = (decision.recommendedSizeUsdc / parseFloat(price)).toFixed(2);

        const orderResp = await polymarketApiService.createOrder({
          token_id: decision.tokenId,
          price,
          size,
          side: 'BUY',
        });

        decision.orderId = orderResp.order_id;
        decision.executed = true;

        // Track position
        const position: BotPosition = {
          id: `pos_${Date.now()}_${Math.random().toString(36).slice(2, 6)}`,
          botId,
          marketId: decision.marketId,
          marketQuestion: decision.marketQuestion,
          outcome: decision.outcome ?? 'YES',
          tokenId: decision.tokenId,
          entryPrice: parseFloat(price),
          currentPrice: parseFloat(price),
          sizeUsdc: decision.recommendedSizeUsdc,
          orderId: orderResp.order_id,
          openedAt: new Date().toISOString(),
          unrealizedPnlPct: 0,
        };

        bot.positions.push(position);
        bot.stats.tradesExecuted += 1;
        this.bots.set(botId, bot);

        // Sync new position to SQLite
        await syncPositionToDb(position);
        console.log(`[BotService] Order placed for bot ${botId}: ${orderResp.order_id}`);
      }
      // Sell actions would follow a similar pattern with the existing cancelOrder / createOrder(SELL)
    } catch (error: any) {
      console.error(`[BotService] Execution error for ${botId}:`, error);
      decision.executed = false;
    }
  }

  // ── Position Monitoring ──────────────────────────────────────────────────────

  /**
   * Called externally (e.g. from a React effect on price updates) to update
   * position P&L and trigger stop-loss / take-profit if thresholds are hit.
   */
  async updatePositionPrice(botId: string, tokenId: string, currentPrice: number): Promise<void> {
    const bot = this.bots.get(botId);
    if (!bot) return;

    let changed = false;
    for (const position of bot.positions) {
      if (position.closedAt || position.tokenId !== tokenId) continue;

      const pnlPct = ((currentPrice - position.entryPrice) / position.entryPrice) * 100;
      position.currentPrice = currentPrice;
      position.unrealizedPnlPct = pnlPct;
      changed = true;

      // Take-profit trigger
      if (pnlPct >= bot.riskConfig.takeProfitPct) {
        await this.closePosition(botId, position, 'take_profit', currentPrice);
      }
      // Stop-loss trigger
      else if (pnlPct <= -bot.riskConfig.stopLossPct) {
        await this.closePosition(botId, position, 'stop_loss', currentPrice);
      }
    }

    if (changed) {
      this.bots.set(botId, bot);
      await this.save();
      this.notify();
    }
  }

  private async closePosition(
    botId: string,
    position: BotPosition,
    reason: BotPosition['closeReason'],
    exitPrice: number,
  ): Promise<void> {
    const bot = this.bots.get(botId);
    if (!bot) return;

    // Attempt to cancel any open order for this position
    if (position.orderId) {
      await polymarketApiService.cancelOrder(position.orderId).catch(() => {/* order may already be filled */});
    }

    position.closedAt = new Date().toISOString();
    position.closeReason = reason;
    position.realizedPnlUsdc = ((exitPrice - position.entryPrice) / position.entryPrice) * position.sizeUsdc;

    // Update daily P&L stats
    bot.stats.dailyPnlUsdc += position.realizedPnlUsdc;
    bot.stats.totalPnlUsdc += position.realizedPnlUsdc;
    if (position.realizedPnlUsdc > 0) {
      bot.stats.tradesWon += 1;
    } else {
      bot.stats.tradesLost += 1;
    }
    const total = bot.stats.tradesWon + bot.stats.tradesLost;
    bot.stats.winRate = total > 0 ? (bot.stats.tradesWon / total) * 100 : 0;

    // Pause bot if daily loss limit hit
    if (bot.stats.dailyPnlUsdc <= -bot.riskConfig.maxDailyLossUsdc && bot.status === 'running') {
      await this.pauseBot(botId);
      this.setBotError(botId, `Daily loss limit of ${bot.riskConfig.maxDailyLossUsdc} USDC hit. Bot paused.`);
    }

    // Sync closed position to SQLite
    await syncPositionToDb(position);

    // Save outcome to memory so future cycles learn from results
    const pnl = position.realizedPnlUsdc ?? 0;
    const outcomeLabel = pnl > 0 ? 'PROFITABLE' : 'LOSS';
    await saveMemoryRepo(
      `Polymarket position closed (${reason}): "${position.marketQuestion}" (${position.outcome}) — entry ${position.entryPrice.toFixed(3)}, exit ${exitPrice.toFixed(3)}, PnL ${pnl.toFixed(2)} USDC (${outcomeLabel}). ${reason === 'take_profit' ? 'Take-profit triggered.' : reason === 'stop_loss' ? 'Stop-loss triggered — review entry criteria.' : ''}`,
      {
        agent_id: `polymarket_bot_${botId}`,
        type: 'trade_outcome',
        importance: pnl > 0 ? 7 : 9, // Losses are more important to remember
        metadata: {
          market_id: position.marketId,
          outcome: position.outcome,
          entry_price: position.entryPrice,
          exit_price: exitPrice,
          pnl_usdc: pnl,
          close_reason: reason,
        },
      },
    ).catch(() => {/* non-fatal */});

    console.log(`[BotService] Position closed (${reason}) for bot ${botId}: PnL ${position.realizedPnlUsdc?.toFixed(2)} USDC`);
  }

  // ── Helpers ──────────────────────────────────────────────────────────────────

  private buildBotAgentConfig(bot: PolymarketBot): AgentConfig {
    return buildAgentConfig(
      bot.llmConfig.provider,
      bot.llmConfig.modelId,
      buildAgentInstructions(bot),
      {
        tools: [
          'polymarket_get_markets',
          'polymarket_search_markets',
          'polymarket_get_market_detail',
          'polymarket_get_order_book',
          'polymarket_get_price_history',
          'polymarket_get_events',
          'polymarket_get_trades',
          'calculator',
        ],
        temperature: bot.llmConfig.temperature,
        maxTokens: bot.llmConfig.maxTokens,
        memory: bot.llmConfig.enableMemory
          ? {
              enabled: true,
              table_name: `bot_memory_${bot.id}`,
              create_session_summary: true,
              create_user_memories: false,
            }
          : undefined,
        reasoning: bot.llmConfig.enableReasoning || undefined,
        guardrails: bot.llmConfig.enableGuardrails
          ? { enabled: true, pii_detection: false, injection_check: true, financial_compliance: true }
          : undefined,
        agentic_memory: bot.llmConfig.enableMemory
          ? { enabled: true, user_id: `bot_${bot.id}` }
          : undefined,
      },
    );
  }

  private async buildCycleQuery(bot: PolymarketBot): Promise<string> {
    const openPositions = bot.positions.filter((p) => !p.closedAt);

    // ── Recall past trade decisions and outcomes ──────────────────────────────
    let memoryContext = '';
    if (bot.llmConfig.enableMemory) {
      const [tradeMemories, outcomeMemories] = await Promise.all([
        searchMemoriesRepo('polymarket trade decision', `polymarket_bot_${bot.id}`, 5).catch(() => null),
        searchMemoriesRepo('polymarket position closed', `polymarket_bot_${bot.id}`, 5).catch(() => null),
      ]);

      const tradeLines: string[] = [];
      for (const mem of tradeMemories?.data?.memories ?? []) {
        const content: string = mem.content ?? mem.memory ?? '';
        if (content) tradeLines.push(`- ${content}`);
      }
      for (const mem of outcomeMemories?.data?.memories ?? []) {
        const content: string = mem.content ?? mem.memory ?? '';
        if (content) tradeLines.push(`- ${content}`);
      }

      if (tradeLines.length > 0) {
        memoryContext = `\n\nYour recent trade history and outcomes (learn from these):\n${tradeLines.join('\n')}`;
      }
    }

    // ── Performance snapshot ──────────────────────────────────────────────────
    const { stats } = bot;
    const perfContext =
      stats.tradesExecuted > 0
        ? `\n\nYour performance so far: ${stats.tradesExecuted} trades, win rate ${stats.winRate.toFixed(1)}%, total PnL ${stats.totalPnlUsdc.toFixed(2)} USDC, daily PnL ${stats.dailyPnlUsdc.toFixed(2)} USDC.`
        : '';

    // ── Open positions ────────────────────────────────────────────────────────
    const positionContext =
      openPositions.length > 0
        ? `\n\nCurrent open positions (do NOT recommend buying into these markets again):\n${openPositions
            .map(
              (p) =>
                `- "${p.marketQuestion}" (${p.outcome} @ ${p.entryPrice.toFixed(3)}, current: ${p.currentPrice.toFixed(3)}, PnL: ${p.unrealizedPnlPct.toFixed(1)}%)`,
            )
            .join('\n')}`
        : '';

    return `Run your Polymarket market analysis cycle now. Scan for trading opportunities and return your structured recommendations as a JSON array.${memoryContext}${perfContext}${positionContext}`;
  }

  private updateBotStatus(id: string, status: BotStatus): void {
    const bot = this.bots.get(id);
    if (!bot) return;
    bot.status = status;
    this.bots.set(id, bot);
    this.save().catch(() => {});
    this.notify();
  }

  private setBotError(id: string, message: string): void {
    const bot = this.bots.get(id);
    if (!bot) return;
    bot.status = 'error';
    bot.lastError = message;
    this.bots.set(id, bot);
    this.save().catch(() => {});
    this.notify();
  }
}

// ─── Singleton Export ─────────────────────────────────────────────────────────

export const polymarketBotService = new PolymarketBotService();
export default polymarketBotService;
