/**
 * BotDeployView - Single-screen no-scroll bot deployment form
 * All config visible at once in a compact terminal-style grid layout
 */

import React, { useState, useEffect } from 'react';
import { Bot, Play, Brain, AlertCircle } from 'lucide-react';
import {
  polymarketBotService,
  BotStrategy, RiskConfig, BotLLMConfig,
  DEFAULT_STRATEGY, DEFAULT_RISK_CONFIG, DEFAULT_LLM_CONFIG,
  StrategyType,
} from '@/services/polymarket/polymarketBotService';
import { getLLMConfigs } from '@/services/core/sqliteService';

// ─── Constants ────────────────────────────────────────────────────────────────

const C = {
  orange: '#FF8800', green: '#00D66F', red: '#FF3B3B',
  cyan: '#00E5FF', purple: '#9D4EDD', white: '#FFFFFF',
  bg: '#000000', panel: '#0A0A0A', header: '#060606',
  border: '#1A1A1A', dim: '#2A2A2A', muted: '#555555', text: '#CCCCCC',
};

const S = {
  label: { fontSize: '8px', color: C.muted, fontWeight: 'bold', marginBottom: '3px', letterSpacing: '0.06em', display: 'block' } as React.CSSProperties,
  input: {
    width: '100%', boxSizing: 'border-box' as const,
    backgroundColor: C.header, color: C.white,
    border: `1px solid ${C.dim}`, padding: '5px 8px',
    fontSize: '10px', outline: 'none', fontFamily: 'inherit',
  } as React.CSSProperties,
  select: {
    width: '100%', boxSizing: 'border-box' as const,
    backgroundColor: C.header, color: C.white,
    border: `1px solid ${C.dim}`, padding: '5px 8px',
    fontSize: '10px', outline: 'none', fontFamily: 'inherit', cursor: 'pointer',
  } as React.CSSProperties,
};

const STRATEGY_INFO: Record<StrategyType, { label: string; color: string; desc: string }> = {
  analyst:    { label: 'ANALYST',    color: C.orange,  desc: 'Mispricing vs base rate' },
  contrarian: { label: 'CONTRARIAN', color: C.purple,  desc: 'Bet against extremes' },
  momentum:   { label: 'MOMENTUM',   color: C.green,   desc: 'Follow price trends' },
  arbitrage:  { label: 'ARBITRAGE',  color: C.cyan,    desc: 'Exploit inconsistencies' },
  custom:     { label: 'CUSTOM',     color: C.white,   desc: 'Custom instructions' },
};

// ─── Toggle ───────────────────────────────────────────────────────────────────

const Toggle: React.FC<{ label: string; value: boolean; onChange: (v: boolean) => void }> = ({ label, value, onChange }) => (
  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '4px 0' }}>
    <span style={{ fontSize: '9px', color: C.text }}>{label}</span>
    <button
      onClick={() => onChange(!value)}
      style={{
        width: '32px', height: '16px', borderRadius: '8px', border: 'none', cursor: 'pointer',
        backgroundColor: value ? C.orange : C.dim, position: 'relative', flexShrink: 0,
        transition: 'background-color 0.15s',
      }}
    >
      <div style={{
        position: 'absolute', top: '2px', width: '12px', height: '12px', borderRadius: '50%',
        backgroundColor: C.white, transition: 'left 0.15s',
        left: value ? '18px' : '2px',
      }} />
    </button>
  </div>
);

// ─── Section Header ───────────────────────────────────────────────────────────

const SectionHeader: React.FC<{ label: string }> = ({ label }) => (
  <div style={{
    fontSize: '8px', fontWeight: 'bold', color: C.orange, letterSpacing: '0.08em',
    padding: '5px 8px', backgroundColor: C.header, borderBottom: `1px solid ${C.border}`,
  }}>
    {label}
  </div>
);

// ─── Main Component ───────────────────────────────────────────────────────────

interface Props {
  onDeployed: (botId: string) => void;
}

const BotDeployView: React.FC<Props> = ({ onDeployed }) => {
  const [botName, setBotName] = useState('');
  const [strategy, setStrategy] = useState<BotStrategy>({ ...DEFAULT_STRATEGY });
  const [riskConfig, setRiskConfig] = useState<RiskConfig>({ ...DEFAULT_RISK_CONFIG });
  const [llmConfig, setLlmConfig] = useState<BotLLMConfig>({ ...DEFAULT_LLM_CONFIG });
  const [llmProviders, setLlmProviders] = useState<Array<{ provider: string; model: string }>>([]);
  const [deploying, setDeploying] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    getLLMConfigs().then((configs) => {
      const providers = configs
        .filter((c) => c.api_key || c.provider === 'ollama' || c.provider === 'fincept')
        .map((c) => ({ provider: c.provider, model: c.model }));
      setLlmProviders(providers);
      if (providers.length > 0) {
        const active = configs.find((c) => c.is_active);
        if (active) setLlmConfig((prev) => ({ ...prev, provider: active.provider, modelId: active.model }));
      }
    }).catch(() => {});
  }, []);

  const handleDeploy = async () => {
    if (!botName.trim()) { setError('Bot name is required'); return; }
    setError(null);
    setDeploying(true);
    try {
      const bot = await polymarketBotService.createBot({ name: botName.trim(), strategy, riskConfig, llmConfig });
      await polymarketBotService.startBot(bot.id);
      onDeployed(bot.id);
    } catch (err: any) {
      setError(err?.message ?? 'Failed to deploy bot');
    } finally {
      setDeploying(false);
    }
  };

  const patchStrategy = (patch: Partial<BotStrategy>) => setStrategy((p) => ({ ...p, ...patch }));
  const patchRisk = (patch: Partial<RiskConfig>) => setRiskConfig((p) => ({ ...p, ...patch }));
  const patchLLM = (patch: Partial<BotLLMConfig>) => setLlmConfig((p) => ({ ...p, ...patch }));

  return (
    <div style={{
      height: '100%', display: 'flex', flexDirection: 'column',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      backgroundColor: C.bg, overflow: 'hidden',
    }}>
      {/* ── Top bar ─────────────────────────────────────────────────────────── */}
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '7px 14px', borderBottom: `2px solid ${C.orange}`,
        backgroundColor: C.header, flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Bot size={14} style={{ color: C.orange }} />
          <span style={{ fontSize: '11px', fontWeight: 'bold', color: C.white, letterSpacing: '0.08em' }}>DEPLOY TRADING BOT</span>
          <span style={{ fontSize: '9px', color: C.muted }}>— configure an autonomous Polymarket agent</span>
        </div>
        {error && (
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: C.red, fontSize: '9px' }}>
            <AlertCircle size={11} />
            {error}
          </div>
        )}
      </div>

      {/* ── Main grid: 3 columns ────────────────────────────────────────────── */}
      <div style={{
        flex: 1, display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr',
        gridTemplateRows: '1fr',
        gap: 0, overflow: 'hidden',
        borderBottom: `1px solid ${C.border}`,
      }}>

        {/* ── Col 1: Identity + Strategy ───────────────────────────────────── */}
        <div style={{ borderRight: `1px solid ${C.border}`, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Identity */}
          <SectionHeader label="BOT IDENTITY" />
          <div style={{ padding: '10px' }}>
            <label style={S.label}>BOT NAME</label>
            <input
              style={S.input}
              placeholder="e.g. Crypto Analyst Bot"
              value={botName}
              onChange={(e) => setBotName(e.target.value)}
              maxLength={50}
            />
          </div>

          {/* Strategy */}
          <SectionHeader label="STRATEGY" />
          <div style={{ padding: '8px 10px', flex: 1 }}>
            {/* Strategy buttons */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '3px', marginBottom: '8px' }}>
              {(Object.keys(STRATEGY_INFO) as StrategyType[]).map((type) => {
                const info = STRATEGY_INFO[type];
                const active = strategy.type === type;
                return (
                  <button
                    key={type}
                    onClick={() => patchStrategy({ type })}
                    style={{
                      padding: '5px 2px', border: `1px solid ${active ? info.color : C.dim}`,
                      backgroundColor: active ? `${info.color}18` : C.panel,
                      color: active ? info.color : C.muted,
                      fontSize: '7px', fontWeight: 'bold', cursor: 'pointer', fontFamily: 'inherit',
                      letterSpacing: '0.04em',
                    }}
                  >
                    {info.label}
                  </button>
                );
              })}
            </div>
            <div style={{
              fontSize: '9px', color: C.text, padding: '5px 8px', marginBottom: '8px',
              backgroundColor: `${STRATEGY_INFO[strategy.type].color}10`,
              border: `1px solid ${STRATEGY_INFO[strategy.type].color}30`,
            }}>
              {STRATEGY_INFO[strategy.type].desc}
            </div>

            {/* Strategy params 2-col grid */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
              <div>
                <label style={S.label}>SCAN INTERVAL</label>
                <select style={S.select} value={strategy.scanIntervalMs} onChange={(e) => patchStrategy({ scanIntervalMs: Number(e.target.value) })}>
                  <option value={30000}>30 SEC</option>
                  <option value={60000}>1 MIN</option>
                  <option value={300000}>5 MIN</option>
                  <option value={900000}>15 MIN</option>
                  <option value={1800000}>30 MIN</option>
                  <option value={3600000}>1 HOUR</option>
                </select>
              </div>
              <div>
                <label style={S.label}>MKTS / SCAN</label>
                <select style={S.select} value={strategy.marketsPerScan} onChange={(e) => patchStrategy({ marketsPerScan: Number(e.target.value) })}>
                  <option value={10}>10</option>
                  <option value={20}>20</option>
                  <option value={30}>30</option>
                  <option value={50}>50</option>
                </select>
              </div>
              <div>
                <label style={S.label}>MAX POSITIONS</label>
                <input style={S.input} type="number" min={1} max={20}
                  value={strategy.maxOpenPositions}
                  onChange={(e) => patchStrategy({ maxOpenPositions: Number(e.target.value) })} />
              </div>
              <div>
                <label style={S.label}>MIN CONF {strategy.minConfidence}%</label>
                <input type="range" min={50} max={95} step={5}
                  value={strategy.minConfidence}
                  onChange={(e) => patchStrategy({ minConfidence: Number(e.target.value) })}
                  style={{ width: '100%', accentColor: C.orange, marginTop: '6px' }} />
              </div>
            </div>

            {strategy.type === 'custom' && (
              <div style={{ marginTop: '6px' }}>
                <label style={S.label}>CUSTOM INSTRUCTIONS</label>
                <textarea
                  style={{ ...S.input, height: '48px', resize: 'none' }}
                  placeholder="Describe your strategy..."
                  value={strategy.customInstructions}
                  onChange={(e) => patchStrategy({ customInstructions: e.target.value })}
                />
              </div>
            )}

            <div style={{ marginTop: '6px' }}>
              <label style={S.label}>FOCUS CATEGORIES (comma-sep, blank = all)</label>
              <input
                style={S.input}
                placeholder="e.g. politics, crypto, sports"
                value={strategy.focusCategories.join(', ')}
                onChange={(e) => patchStrategy({ focusCategories: e.target.value.split(',').map((s) => s.trim()).filter(Boolean) })}
              />
            </div>
          </div>
        </div>

        {/* ── Col 2: AI Model ──────────────────────────────────────────────── */}
        <div style={{ borderRight: `1px solid ${C.border}`, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <SectionHeader label="AI MODEL" />
          <div style={{ padding: '10px', flex: 1 }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', marginBottom: '6px' }}>
              <div>
                <label style={S.label}>PROVIDER</label>
                <select style={S.select} value={llmConfig.provider} onChange={(e) => patchLLM({ provider: e.target.value })}>
                  {llmProviders.length > 0 ? (
                    llmProviders.map((p) => (
                      <option key={`${p.provider}:${p.model}`} value={p.provider}>{p.provider.toUpperCase()}</option>
                    ))
                  ) : (
                    <option value="fincept">FINCEPT</option>
                  )}
                </select>
              </div>
              <div>
                <label style={S.label}>MODEL ID</label>
                <input
                  style={S.input}
                  placeholder="e.g. gpt-4o"
                  value={llmConfig.modelId}
                  onChange={(e) => patchLLM({ modelId: e.target.value })}
                />
              </div>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', marginBottom: '10px' }}>
              <div>
                <label style={S.label}>TEMPERATURE {llmConfig.temperature.toFixed(2)}</label>
                <input
                  type="range" min={0} max={1} step={0.05}
                  value={llmConfig.temperature}
                  onChange={(e) => patchLLM({ temperature: Number(e.target.value) })}
                  style={{ width: '100%', accentColor: C.orange, marginTop: '6px' }}
                />
              </div>
              <div>
                <label style={S.label}>MAX TOKENS</label>
                <select style={S.select} value={llmConfig.maxTokens} onChange={(e) => patchLLM({ maxTokens: Number(e.target.value) })}>
                  <option value={2048}>2048</option>
                  <option value={4096}>4096</option>
                  <option value={8192}>8192</option>
                </select>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${C.border}`, paddingTop: '8px' }}>
              <Toggle label="ENABLE MEMORY — remembers past decisions" value={llmConfig.enableMemory} onChange={(v) => patchLLM({ enableMemory: v })} />
              <Toggle label="ENABLE REASONING — chain-of-thought (slower)" value={llmConfig.enableReasoning} onChange={(v) => patchLLM({ enableReasoning: v })} />
              <Toggle label="ENABLE GUARDRAILS — compliance protection" value={llmConfig.enableGuardrails} onChange={(v) => patchLLM({ enableGuardrails: v })} />
            </div>
          </div>
        </div>

        {/* ── Col 3: Risk + Deploy ─────────────────────────────────────────── */}
        <div style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <SectionHeader label="RISK MANAGEMENT" />
          <div style={{ padding: '10px', flex: 1 }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', marginBottom: '6px' }}>
              <div>
                <label style={S.label}>MAX EXPOSURE (USDC)</label>
                <input style={S.input} type="number" min={1}
                  value={riskConfig.maxExposureUsdc}
                  onChange={(e) => patchRisk({ maxExposureUsdc: Number(e.target.value) })} />
              </div>
              <div>
                <label style={S.label}>MAX PER MARKET (USDC)</label>
                <input style={S.input} type="number" min={1}
                  value={riskConfig.maxPerMarketUsdc}
                  onChange={(e) => patchRisk({ maxPerMarketUsdc: Number(e.target.value) })} />
              </div>
              <div>
                <label style={S.label}>STOP LOSS -{riskConfig.stopLossPct}%</label>
                <input type="range" min={5} max={80} step={5}
                  value={riskConfig.stopLossPct}
                  onChange={(e) => patchRisk({ stopLossPct: Number(e.target.value) })}
                  style={{ width: '100%', accentColor: C.red, marginTop: '6px' }} />
              </div>
              <div>
                <label style={S.label}>TAKE PROFIT +{riskConfig.takeProfitPct}%</label>
                <input type="range" min={10} max={200} step={10}
                  value={riskConfig.takeProfitPct}
                  onChange={(e) => patchRisk({ takeProfitPct: Number(e.target.value) })}
                  style={{ width: '100%', accentColor: C.green, marginTop: '6px' }} />
              </div>
            </div>

            <div style={{ marginBottom: '8px' }}>
              <label style={S.label}>MAX DAILY LOSS (USDC)</label>
              <input style={{ ...S.input, width: '60%' }} type="number" min={1}
                value={riskConfig.maxDailyLossUsdc}
                onChange={(e) => patchRisk({ maxDailyLossUsdc: Number(e.target.value) })} />
            </div>

            <div style={{ borderTop: `1px solid ${C.border}`, paddingTop: '8px', marginBottom: '8px' }}>
              <Toggle
                label="REQUIRE TRADE APPROVAL (recommended)"
                value={riskConfig.requireApproval}
                onChange={(v) => patchRisk({ requireApproval: v })}
              />
            </div>

            {!riskConfig.requireApproval && (
              <div style={{
                backgroundColor: '#1A0800', border: `1px solid ${C.orange}`,
                padding: '6px 8px', marginBottom: '8px', fontSize: '8px', color: C.orange,
              }}>
                ⚠ AUTO-EXECUTE: orders placed without confirmation
              </div>
            )}
          </div>

          {/* ── Summary strip ──────────────────────────────────────────────── */}
          <div style={{
            borderTop: `1px solid ${C.border}`, borderBottom: `1px solid ${C.border}`,
            display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)',
            backgroundColor: C.header, flexShrink: 0,
          }}>
            {[
              { label: 'STRATEGY', value: STRATEGY_INFO[strategy.type].label, color: STRATEGY_INFO[strategy.type].color },
              { label: 'MODEL',    value: llmConfig.provider.toUpperCase(),     color: C.cyan },
              { label: 'EXPOSURE', value: `${riskConfig.maxExposureUsdc} USDC`, color: C.white },
              { label: 'INTERVAL', value: `${strategy.scanIntervalMs / 60000}m`, color: C.white },
            ].map((item) => (
              <div key={item.label} style={{ padding: '6px 8px', borderRight: `1px solid ${C.border}` }}>
                <div style={{ fontSize: '7px', color: C.muted, letterSpacing: '0.06em', marginBottom: '2px' }}>{item.label}</div>
                <div style={{ fontSize: '10px', fontWeight: 'bold', color: item.color }}>{item.value}</div>
              </div>
            ))}
          </div>

          {/* ── Deploy button ───────────────────────────────────────────────── */}
          <button
            onClick={handleDeploy}
            disabled={deploying || !botName.trim()}
            style={{
              padding: '12px', flexShrink: 0,
              backgroundColor: deploying || !botName.trim() ? C.dim : C.orange,
              color: deploying || !botName.trim() ? C.muted : '#000000',
              border: 'none', fontSize: '11px', fontWeight: 'bold',
              cursor: deploying || !botName.trim() ? 'not-allowed' : 'pointer',
              fontFamily: 'inherit', letterSpacing: '0.08em',
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px',
              transition: 'all 0.1s',
            }}
          >
            {deploying ? <><Brain size={14} /> DEPLOYING...</> : <><Play size={14} /> DEPLOY &amp; START BOT</>}
          </button>
        </div>
      </div>
    </div>
  );
};

export default BotDeployView;
