/**
 * CreateAgentView - Build and save custom agents to SQLite
 *
 * Uses the existing agent_configs table + db_save_agent_config Rust command.
 * Custom agents appear immediately in the Agents view after creation.
 */

import React, { useState, useEffect, useCallback, useRef } from 'react';
import {
  Plus, Save, Trash2, Bot, RefreshCw, CheckCircle,
  AlertCircle, Brain, Cpu, Wrench, ChevronDown, ChevronRight,
  Edit3, Loader2, Upload, Download,
} from 'lucide-react';
import {
  saveAgentConfig,
  getAgentConfigsByCategory,
  deleteAgentConfig,
  getLLMConfigs,
  getMCPServers,
  type AgentConfig,
  type AgentConfigData,
  type LLMConfig,
  type MCPServer,
} from '@/services/core/sqliteService';
import { getTools } from '@/services/agentService';
import { FINCEPT } from '../types';

const CATEGORIES = ['custom', 'trader', 'hedge-fund', 'economic', 'geopolitics', 'research', 'general'];

// ─── Default form state ───────────────────────────────────────────────────────

const DEFAULT_FORM = {
  name: '',
  description: '',
  category: 'custom',
  provider: '',
  model_id: '',
  instructions: '',
  temperature: 0.7,
  max_tokens: 4096,
  enable_memory: false,
  enable_agentic_memory: false,
  tools: [] as string[],
  mcp_server_ids: [] as string[],
};

type FormState = typeof DEFAULT_FORM;

// ─── Component ────────────────────────────────────────────────────────────────

export const CreateAgentView: React.FC = () => {
  const [form, setForm] = useState<FormState>(DEFAULT_FORM);
  const [savedAgents, setSavedAgents] = useState<AgentConfig[]>([]);
  const [configuredLLMs, setConfiguredLLMs] = useState<LLMConfig[]>([]);
  const [mcpServers, setMcpServers] = useState<MCPServer[]>([]);
  const [toolCatalog, setToolCatalog] = useState<Record<string, string[]>>({});
  const [saving, setSaving] = useState(false);
  const [loading, setLoading] = useState(false);
  const [deletingId, setDeletingId] = useState<string | null>(null);
  const [notification, setNotification] = useState<{ type: 'success' | 'error'; msg: string } | null>(null);
  const [expandedSection, setExpandedSection] = useState<'basic' | 'model' | 'tools' | 'advanced'>('basic');
  const [editingId, setEditingId] = useState<string | null>(null);
  const importInputRef = useRef<HTMLInputElement>(null);

  // ─── Load LLM configs from Settings ──────────────────────────────────────

  const loadLLMConfigs = useCallback(async () => {
    try {
      const configs = await getLLMConfigs();
      setConfiguredLLMs(configs);
      // Auto-select: fincept first, then active, then first available
      if (configs.length > 0 && !form.provider) {
        const fincept = configs.find(c => c.provider === 'fincept');
        const active = configs.find(c => c.is_active);
        const preferred = fincept || active || configs[0];
        setForm(prev => ({ ...prev, provider: preferred.provider, model_id: preferred.model }));
      }
    } catch {
      // Settings not configured yet — user will see the empty state
    }
  }, []);

  // ─── Load saved custom agents ─────────────────────────────────────────────

  const loadSavedAgents = useCallback(async () => {
    setLoading(true);
    try {
      const agents = await getAgentConfigsByCategory('custom');
      // Also load other user-created categories
      const allCustom = await Promise.all(
        CATEGORIES.slice(1).map(cat => getAgentConfigsByCategory(cat))
      );
      const all = [...agents];
      for (const catAgents of allCustom) {
        for (const a of catAgents) {
          if (!all.find(x => x.id === a.id)) all.push(a);
        }
      }
      // Only show agents where config_json has "user_created": true
      setSavedAgents(all.filter(a => {
        try {
          const cfg = JSON.parse(a.config_json);
          return cfg.user_created === true;
        } catch { return false; }
      }));
    } catch (e: any) {
      // Silently ignore — no agents yet
    } finally {
      setLoading(false);
    }
  }, []);

  const loadMcpServers = useCallback(async () => {
    try {
      const servers = await getMCPServers();
      setMcpServers(servers.filter(s => s.enabled));
    } catch {
      // MCP tab not set up yet — ignore
    }
  }, []);

  const loadToolCatalog = useCallback(async () => {
    try {
      const info = await getTools();
      if (info?.tools) setToolCatalog(info.tools);
    } catch {
      // finagent_core not available — leave empty
    }
  }, []);

  useEffect(() => {
    loadSavedAgents();
    loadLLMConfigs();
    loadMcpServers();
    loadToolCatalog();
  }, [loadSavedAgents, loadLLMConfigs, loadMcpServers, loadToolCatalog]);

  // ─── Notify helper ────────────────────────────────────────────────────────

  const notify = (type: 'success' | 'error', msg: string) => {
    setNotification({ type, msg });
    setTimeout(() => setNotification(null), 4000);
  };

  // ─── Save agent ───────────────────────────────────────────────────────────

  const handleSave = async () => {
    if (!form.name.trim()) { notify('error', 'Agent name is required'); return; }
    if (!form.instructions.trim()) { notify('error', 'Instructions are required'); return; }
    if (!form.provider) { notify('error', 'Select an LLM provider in Model Configuration'); return; }

    setSaving(true);
    try {
      // Build MCP server configs for selected servers
      const selectedMcpServers = form.mcp_server_ids.length > 0
        ? mcpServers
            .filter(s => form.mcp_server_ids.includes(s.id))
            .map(s => ({
              id: s.id,
              name: s.name,
              command: s.command,
              args: (() => { try { return JSON.parse(s.args || '[]'); } catch { return []; } })(),
              env: (() => { try { return JSON.parse(s.env || '{}'); } catch { return {}; } })(),
              transport: 'stdio' as const,
            }))
        : undefined;

      const configData: AgentConfigData & { user_created: boolean } = {
        user_created: true,
        model: {
          provider: form.provider,
          model_id: form.model_id,
          temperature: form.temperature,
          max_tokens: form.max_tokens,
        },
        instructions: form.instructions,
        tools: form.tools,
        memory: form.enable_memory ? { enabled: true } : undefined,
        mcp_servers: selectedMcpServers,
      };

      const id = editingId || `custom_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`;

      await saveAgentConfig({
        id,
        name: form.name.trim(),
        description: form.description.trim() || undefined,
        config_json: JSON.stringify(configData),
        category: form.category,
        is_default: false,
        is_active: false,
      });

      notify('success', editingId ? `Agent "${form.name}" updated` : `Agent "${form.name}" created and saved`);
      setForm(DEFAULT_FORM);
      setEditingId(null);
      await loadSavedAgents();
    } catch (e: any) {
      notify('error', `Failed to save: ${e.message || e}`);
    } finally {
      setSaving(false);
    }
  };

  // ─── Delete agent ─────────────────────────────────────────────────────────

  const handleDelete = async (id: string, name: string) => {
    setDeletingId(id);
    try {
      await deleteAgentConfig(id);
      notify('success', `Agent "${name}" deleted`);
      await loadSavedAgents();
    } catch (e: any) {
      notify('error', `Failed to delete: ${e.message || e}`);
    } finally {
      setDeletingId(null);
    }
  };

  // ─── Edit agent ───────────────────────────────────────────────────────────

  const handleEdit = (agent: AgentConfig) => {
    try {
      const cfg = JSON.parse(agent.config_json) as AgentConfigData & { user_created?: boolean };
      setForm({
        name: agent.name,
        description: agent.description || '',
        category: agent.category,
        provider: cfg.model?.provider || '',
        model_id: cfg.model?.model_id || '',
        instructions: cfg.instructions || '',
        temperature: cfg.model?.temperature ?? 0.7,
        max_tokens: cfg.model?.max_tokens ?? 4096,
        enable_memory: cfg.memory?.enabled ?? false,
        enable_agentic_memory: false,
        tools: cfg.tools || [],
        mcp_server_ids: ((cfg as any).mcp_servers || []).map((s: any) => s.id).filter(Boolean),
      });
      setEditingId(agent.id);
      setExpandedSection('model');
    } catch {}
  };

  // ─── Import JSON ──────────────────────────────────────────────────────────

  const handleImport = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (ev) => {
      try {
        const raw = JSON.parse(ev.target?.result as string);

        // Accept two shapes:
        //  1. Full agent config_json: { model, instructions, tools, ... }
        //  2. Wrapped export: { name, description, category, config_json: {...} }
        let cfg: AgentConfigData & { user_created?: boolean } = {};
        let meta = { name: '', description: '', category: 'custom' };

        if (raw.config_json) {
          // Wrapped export format
          cfg = typeof raw.config_json === 'string' ? JSON.parse(raw.config_json) : raw.config_json;
          meta.name = raw.name || '';
          meta.description = raw.description || '';
          meta.category = raw.category || 'custom';
        } else if (raw.model || raw.instructions) {
          // Bare config_json format
          cfg = raw;
        } else {
          notify('error', 'Unrecognized JSON format. Expected agent config with "model" or "instructions" fields.');
          return;
        }

        setForm({
          name: meta.name,
          description: meta.description,
          category: CATEGORIES.includes(meta.category) ? meta.category : 'custom',
          provider: cfg.model?.provider || '',
          model_id: cfg.model?.model_id || '',
          temperature: cfg.model?.temperature ?? 0.7,
          max_tokens: cfg.model?.max_tokens ?? 4096,
          instructions: cfg.instructions || '',
          tools: cfg.tools || [],
          enable_memory: cfg.memory?.enabled ?? false,
          enable_agentic_memory: false,
          mcp_server_ids: ((cfg as any).mcp_servers || []).map((s: any) => s.id).filter(Boolean),
        });

        setEditingId(null);
        setExpandedSection('basic');
        notify('success', 'Agent imported — review and click CREATE & SAVE AGENT');
      } catch {
        notify('error', 'Invalid JSON file');
      }
    };
    reader.readAsText(file);
    // Reset input so same file can be re-imported
    e.target.value = '';
  };

  // ─── Export JSON ──────────────────────────────────────────────────────────

  const handleExportAgent = (agent: AgentConfig) => {
    // Export the full wrapped format — same shape that handleImport accepts
    let cfg: Record<string, any> = {};
    try { cfg = JSON.parse(agent.config_json); } catch {}

    const exportData = {
      name: agent.name,
      description: agent.description || '',
      category: agent.category,
      config_json: cfg,
    };

    const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${agent.name.replace(/\s+/g, '_').toLowerCase()}_agent.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleExportForm = () => {
    if (!form.name.trim()) { notify('error', 'Fill in Agent Name before exporting'); return; }
    const configData = {
      user_created: true,
      model: { provider: form.provider, model_id: form.model_id, temperature: form.temperature, max_tokens: form.max_tokens },
      instructions: form.instructions,
      tools: form.tools,
      memory: form.enable_memory ? { enabled: true } : undefined,
    };
    const exportData = {
      name: form.name.trim(),
      description: form.description.trim(),
      category: form.category,
      config_json: configData,
    };
    const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${form.name.replace(/\s+/g, '_').toLowerCase()}_agent.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  // ─── Helpers ──────────────────────────────────────────────────────────────

  const toggleTool = (toolId: string) => {
    setForm(prev => ({
      ...prev,
      tools: prev.tools.includes(toolId)
        ? prev.tools.filter(t => t !== toolId)
        : [...prev.tools, toolId],
    }));
  };

  const sectionHeader = (
    id: 'basic' | 'model' | 'tools' | 'advanced',
    label: string,
    icon: React.ReactNode,
  ) => (
    <button
      onClick={() => setExpandedSection(expandedSection === id ? 'basic' : id)}
      style={{
        width: '100%', display: 'flex', alignItems: 'center', gap: '8px',
        padding: '8px 12px', backgroundColor: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
        cursor: 'pointer', marginBottom: '8px', color: FINCEPT.WHITE,
      }}
    >
      {icon}
      <span style={{ flex: 1, fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px', textAlign: 'left' }}>{label}</span>
      {expandedSection === id ? <ChevronDown size={12} style={{ color: FINCEPT.GRAY }} /> : <ChevronRight size={12} style={{ color: FINCEPT.GRAY }} />}
    </button>
  );

  const labelStyle: React.CSSProperties = { fontSize: '9px', color: FINCEPT.GRAY, letterSpacing: '0.5px', display: 'block', marginBottom: '4px' };
  const inputStyle: React.CSSProperties = {
    width: '100%', padding: '6px 8px', backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px', fontSize: '11px', outline: 'none', boxSizing: 'border-box',
  };
  const selectStyle: React.CSSProperties = { ...inputStyle, cursor: 'pointer' };

  // ─── Render ───────────────────────────────────────────────────────────────

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>

      {/* LEFT: Builder form */}
      <div style={{ width: '420px', minWidth: '420px', display: 'flex', flexDirection: 'column', borderRight: `1px solid ${FINCEPT.BORDER}`, overflow: 'hidden' }}>

        {/* Hidden file input for import */}
        <input
          ref={importInputRef}
          type="file"
          accept=".json,application/json"
          style={{ display: 'none' }}
          onChange={handleImport}
        />

        {/* Header */}
        <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, flexShrink: 0 }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '6px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Plus size={14} style={{ color: FINCEPT.ORANGE }} />
              <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>
                {editingId ? 'EDIT AGENT' : 'CREATE CUSTOM AGENT'}
              </span>
            </div>
            {/* Import / Export current form */}
            <div style={{ display: 'flex', gap: '6px' }}>
              <button
                onClick={() => importInputRef.current?.click()}
                title="Import agent from JSON file"
                style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.CYAN}60`, color: FINCEPT.CYAN, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px' }}
              >
                <Upload size={10} /> IMPORT
              </button>
              <button
                onClick={handleExportForm}
                title="Export current form as JSON"
                style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.GREEN}60`, color: FINCEPT.GREEN, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px' }}
              >
                <Download size={10} /> EXPORT
              </button>
            </div>
          </div>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
            Agents are saved to SQLite and available immediately in the Agents tab
          </span>
        </div>

        {/* Form */}
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>

          {/* BASIC INFO */}
          {sectionHeader('basic', 'BASIC INFO', <Bot size={12} style={{ color: FINCEPT.ORANGE }} />)}
          {expandedSection === 'basic' && (
            <div style={{ marginBottom: '16px', display: 'flex', flexDirection: 'column', gap: '10px' }}>
              <div>
                <label style={labelStyle}>AGENT NAME *</label>
                <input
                  style={inputStyle}
                  placeholder="e.g. My Value Investor Agent"
                  value={form.name}
                  onChange={e => setForm(p => ({ ...p, name: e.target.value }))}
                />
              </div>
              <div>
                <label style={labelStyle}>DESCRIPTION</label>
                <input
                  style={inputStyle}
                  placeholder="Brief description of what this agent does"
                  value={form.description}
                  onChange={e => setForm(p => ({ ...p, description: e.target.value }))}
                />
              </div>
              <div>
                <label style={labelStyle}>CATEGORY</label>
                <select style={selectStyle} value={form.category} onChange={e => setForm(p => ({ ...p, category: e.target.value }))}>
                  {CATEGORIES.map(c => <option key={c} value={c}>{c.toUpperCase()}</option>)}
                </select>
              </div>
              <div>
                <label style={labelStyle}>INSTRUCTIONS / SYSTEM PROMPT *</label>
                <textarea
                  style={{ ...inputStyle, height: '120px', resize: 'vertical', fontFamily: 'inherit' }}
                  placeholder="You are a financial analyst specializing in value investing. Analyze stocks using fundamental analysis principles..."
                  value={form.instructions}
                  onChange={e => setForm(p => ({ ...p, instructions: e.target.value }))}
                />
              </div>
            </div>
          )}

          {/* MODEL CONFIG */}
          {sectionHeader('model', 'MODEL CONFIGURATION', <Cpu size={12} style={{ color: FINCEPT.CYAN }} />)}
          {expandedSection === 'model' && (
            <div style={{ marginBottom: '16px', display: 'flex', flexDirection: 'column', gap: '10px' }}>

              {/* LLM selector — mirrors AgentsView pattern, pulls from Settings */}
              {configuredLLMs.length > 0 ? (
                <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.CYAN}40`, borderRadius: '2px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px' }}>
                    <CheckCircle size={10} style={{ color: FINCEPT.CYAN }} />
                    <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>SELECT LLM PROVIDER</span>
                  </div>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                    {configuredLLMs.map((llm, idx) => {
                      const hasKey = !!llm.api_key && llm.api_key.length > 0;
                      const isSelected = form.provider === llm.provider && form.model_id === llm.model;
                      return (
                        <button
                          key={idx}
                          onClick={() => setForm(p => ({ ...p, provider: llm.provider, model_id: llm.model }))}
                          style={{
                            padding: '6px 10px', cursor: 'pointer', borderRadius: '2px',
                            backgroundColor: isSelected ? FINCEPT.ORANGE : FINCEPT.BORDER,
                            border: `1px solid ${isSelected ? FINCEPT.ORANGE : FINCEPT.MUTED}`,
                            display: 'flex', alignItems: 'center', gap: '5px',
                          }}
                        >
                          {llm.is_active && (
                            <span style={{ color: isSelected ? FINCEPT.DARK_BG : FINCEPT.GREEN, fontSize: '8px' }}>●</span>
                          )}
                          <span style={{ fontSize: '9px', fontWeight: 700, color: isSelected ? FINCEPT.DARK_BG : FINCEPT.WHITE }}>
                            {llm.provider.toUpperCase()}
                          </span>
                          <span style={{ fontSize: '9px', color: isSelected ? FINCEPT.DARK_BG : FINCEPT.GRAY }}>
                            / {llm.model}
                          </span>
                          {!hasKey && <AlertCircle size={9} style={{ color: FINCEPT.RED }} />}
                        </button>
                      );
                    })}
                  </div>
                  {/* Show selected provider/model */}
                  {form.provider && (
                    <div style={{ marginTop: '8px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                      <div>
                        <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '3px' }}>PROVIDER</div>
                        <div style={{ padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', color: FINCEPT.CYAN }}>{form.provider}</div>
                      </div>
                      <div>
                        <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginBottom: '3px' }}>MODEL</div>
                        <div style={{ padding: '5px 8px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px', color: FINCEPT.CYAN }}>{form.model_id}</div>
                      </div>
                    </div>
                  )}
                </div>
              ) : (
                <div style={{ padding: '12px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.RED}40`, borderRadius: '2px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '6px' }}>
                    <AlertCircle size={10} style={{ color: FINCEPT.RED }} />
                    <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED }}>NO LLM CONFIGURED</span>
                  </div>
                  <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                    Go to Settings → LLM Configuration to add API keys, then return here.
                  </span>
                </div>
              )}

              {/* Temperature + Max Tokens */}
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                <div>
                  <label style={labelStyle}>TEMPERATURE ({form.temperature})</label>
                  <input
                    type="range" min="0" max="2" step="0.1"
                    value={form.temperature}
                    onChange={e => setForm(p => ({ ...p, temperature: parseFloat(e.target.value) }))}
                    style={{ width: '100%', accentColor: FINCEPT.ORANGE }}
                  />
                </div>
                <div>
                  <label style={labelStyle}>MAX TOKENS</label>
                  <input
                    style={inputStyle} type="number" min="256" max="32000" step="256"
                    value={form.max_tokens}
                    onChange={e => setForm(p => ({ ...p, max_tokens: parseInt(e.target.value) || 4096 }))}
                  />
                </div>
              </div>
            </div>
          )}

          {/* TOOLS */}
          {sectionHeader('tools', 'TOOLS', <Wrench size={12} style={{ color: FINCEPT.GREEN }} />)}
          {expandedSection === 'tools' && (
            <div style={{ marginBottom: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>

              {/* Built-in tools from ToolsRegistry — grouped by category */}
              <div>
                <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px', paddingLeft: '2px' }}>
                  BUILT-IN TOOLS ({Object.values(toolCatalog).flat().length})
                </div>
                {Object.keys(toolCatalog).length === 0 ? (
                  <div style={{ fontSize: '9px', color: FINCEPT.MUTED, padding: '8px' }}>Loading...</div>
                ) : (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                    {Object.entries(toolCatalog).map(([category, toolIds]) => (
                      <div key={category}>
                        <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px', marginBottom: '4px', textTransform: 'uppercase' }}>
                          {category.replace(/_/g, ' ')}
                        </div>
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                          {toolIds.map(toolId => (
                            <div
                              key={toolId}
                              onClick={() => toggleTool(toolId)}
                              style={{
                                display: 'flex', alignItems: 'center', gap: '8px',
                                padding: '6px 10px', cursor: 'pointer',
                                backgroundColor: form.tools.includes(toolId) ? `${FINCEPT.GREEN}15` : FINCEPT.DARK_BG,
                                border: `1px solid ${form.tools.includes(toolId) ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                                borderRadius: '2px',
                              }}
                            >
                              <div style={{
                                width: '12px', height: '12px', borderRadius: '2px', flexShrink: 0,
                                backgroundColor: form.tools.includes(toolId) ? FINCEPT.GREEN : 'transparent',
                                border: `1px solid ${form.tools.includes(toolId) ? FINCEPT.GREEN : FINCEPT.MUTED}`,
                                display: 'flex', alignItems: 'center', justifyContent: 'center',
                              }}>
                                {form.tools.includes(toolId) && <span style={{ color: FINCEPT.DARK_BG, fontSize: '9px', fontWeight: 700 }}>✓</span>}
                              </div>
                              <span style={{ fontSize: '10px', color: form.tools.includes(toolId) ? FINCEPT.WHITE : FINCEPT.GRAY, fontWeight: 500 }}>
                                {toolId}
                              </span>
                            </div>
                          ))}
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>

              {/* MCP Servers from the MCP tab */}
              <div>
                <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', marginBottom: '6px', paddingLeft: '2px', display: 'flex', alignItems: 'center', gap: '6px' }}>
                  MCP SERVERS
                  <span style={{ fontSize: '8px', color: FINCEPT.MUTED, fontWeight: 400 }}>(configured in MCP tab)</span>
                </div>
                {mcpServers.length === 0 ? (
                  <div style={{ padding: '10px', backgroundColor: FINCEPT.DARK_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', textAlign: 'center' }}>
                    <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>No MCP servers configured</div>
                    <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '2px' }}>Add servers in the MCP tab to use them here</div>
                  </div>
                ) : (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
                    {mcpServers.map(srv => {
                      const selected = form.mcp_server_ids.includes(srv.id);
                      const isRunning = srv.status === 'running';
                      return (
                        <div
                          key={srv.id}
                          onClick={() => setForm(p => ({
                            ...p,
                            mcp_server_ids: selected
                              ? p.mcp_server_ids.filter(id => id !== srv.id)
                              : [...p.mcp_server_ids, srv.id],
                          }))}
                          style={{
                            display: 'flex', alignItems: 'center', gap: '8px',
                            padding: '8px 10px', cursor: 'pointer',
                            backgroundColor: selected ? `${FINCEPT.CYAN}15` : FINCEPT.DARK_BG,
                            border: `1px solid ${selected ? FINCEPT.CYAN : FINCEPT.BORDER}`,
                            borderRadius: '2px',
                          }}
                        >
                          <div style={{
                            width: '14px', height: '14px', borderRadius: '2px', flexShrink: 0,
                            backgroundColor: selected ? FINCEPT.CYAN : 'transparent',
                            border: `1px solid ${selected ? FINCEPT.CYAN : FINCEPT.MUTED}`,
                            display: 'flex', alignItems: 'center', justifyContent: 'center',
                          }}>
                            {selected && <span style={{ color: FINCEPT.DARK_BG, fontSize: '10px', fontWeight: 700 }}>✓</span>}
                          </div>
                          <div style={{ flex: 1 }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}>
                              {srv.icon && <span style={{ fontSize: '10px' }}>{srv.icon}</span>}
                              <span style={{ fontSize: '10px', color: selected ? FINCEPT.WHITE : FINCEPT.GRAY, fontWeight: 600 }}>{srv.name}</span>
                              <span style={{
                                fontSize: '7px', padding: '1px 4px', borderRadius: '2px', fontWeight: 700,
                                backgroundColor: isRunning ? `${FINCEPT.GREEN}25` : `${FINCEPT.MUTED}25`,
                                color: isRunning ? FINCEPT.GREEN : FINCEPT.MUTED,
                              }}>
                                {srv.status.toUpperCase()}
                              </span>
                            </div>
                            <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>{srv.category} · {srv.command}</div>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                )}
              </div>

            </div>
          )}

          {/* ADVANCED */}
          {sectionHeader('advanced', 'ADVANCED', <Brain size={12} style={{ color: FINCEPT.PURPLE }} />)}
          {expandedSection === 'advanced' && (
            <div style={{ marginBottom: '16px', display: 'flex', flexDirection: 'column', gap: '10px' }}>
              {[
                { key: 'enable_memory' as const, label: 'ENABLE MEMORY', desc: 'Store conversation history between sessions' },
                { key: 'enable_agentic_memory' as const, label: 'ENABLE AGENTIC MEMORY', desc: 'AI builds persistent user memories over time' },
              ].map(({ key, label, desc }) => (
                <div
                  key={key}
                  onClick={() => setForm(p => ({ ...p, [key]: !p[key] }))}
                  style={{
                    display: 'flex', alignItems: 'center', gap: '10px',
                    padding: '10px', cursor: 'pointer',
                    backgroundColor: form[key] ? `${FINCEPT.PURPLE}15` : FINCEPT.DARK_BG,
                    border: `1px solid ${form[key] ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                    borderRadius: '2px',
                  }}
                >
                  <div style={{
                    width: '32px', height: '16px', borderRadius: '8px',
                    backgroundColor: form[key] ? FINCEPT.PURPLE : FINCEPT.BORDER,
                    position: 'relative', flexShrink: 0, transition: 'background 0.2s',
                  }}>
                    <div style={{
                      position: 'absolute', top: '2px',
                      left: form[key] ? '18px' : '2px',
                      width: '12px', height: '12px', borderRadius: '50%',
                      backgroundColor: FINCEPT.WHITE, transition: 'left 0.2s',
                    }} />
                  </div>
                  <div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: form[key] ? FINCEPT.WHITE : FINCEPT.GRAY }}>{label}</div>
                    <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>{desc}</div>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Footer actions */}
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, display: 'flex', gap: '8px', flexShrink: 0 }}>
          {editingId && (
            <button
              onClick={() => { setForm(DEFAULT_FORM); setEditingId(null); }}
              style={{ padding: '8px 12px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px' }}
            >
              CANCEL
            </button>
          )}
          <button
            onClick={handleSave}
            disabled={saving}
            style={{
              flex: 1, padding: '10px', backgroundColor: FINCEPT.ORANGE,
              border: 'none', color: FINCEPT.DARK_BG, fontSize: '10px', fontWeight: 700,
              cursor: saving ? 'not-allowed' : 'pointer', borderRadius: '2px',
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
              opacity: saving ? 0.7 : 1, letterSpacing: '0.5px',
            }}
          >
            {saving ? <Loader2 size={12} className="animate-spin" /> : <Save size={12} />}
            {saving ? 'SAVING...' : editingId ? 'UPDATE AGENT' : 'CREATE & SAVE AGENT'}
          </button>
        </div>
      </div>

      {/* RIGHT: Saved agents list */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>

        {/* Header */}
        <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexShrink: 0 }}>
          <div>
            <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE, letterSpacing: '0.5px' }}>SAVED CUSTOM AGENTS</span>
            <span style={{ marginLeft: '8px', fontSize: '9px', color: FINCEPT.GRAY }}>({savedAgents.length})</span>
          </div>
          <button
            onClick={loadSavedAgents}
            disabled={loading}
            style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer' }}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} style={{ color: FINCEPT.ORANGE }} />
          </button>
        </div>

        {/* Notification */}
        {notification && (
          <div style={{
            margin: '8px 16px', padding: '10px 12px',
            backgroundColor: notification.type === 'success' ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
            border: `1px solid ${notification.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED}40`,
            borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '8px',
          }}>
            {notification.type === 'success'
              ? <CheckCircle size={12} style={{ color: FINCEPT.GREEN }} />
              : <AlertCircle size={12} style={{ color: FINCEPT.RED }} />
            }
            <span style={{ fontSize: '10px', color: notification.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED }}>
              {notification.msg}
            </span>
          </div>
        )}

        {/* Agent list */}
        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          {loading ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', padding: '48px', gap: '12px' }}>
              <Loader2 size={24} className="animate-spin" style={{ color: FINCEPT.ORANGE }} />
              <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Loading saved agents...</span>
            </div>
          ) : savedAgents.length === 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', padding: '48px', textAlign: 'center', gap: '12px' }}>
              <Bot size={48} style={{ color: FINCEPT.MUTED, opacity: 0.4 }} />
              <div>
                <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '4px' }}>NO CUSTOM AGENTS YET</div>
                <div style={{ fontSize: '10px', color: FINCEPT.MUTED }}>Create your first agent using the builder on the left</div>
              </div>
            </div>
          ) : (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {savedAgents.map(agent => {
                let cfg: AgentConfigData & { user_created?: boolean } = {};
                try { cfg = JSON.parse(agent.config_json); } catch {}

                return (
                  <div key={agent.id} style={{
                    padding: '14px', backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${editingId === agent.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    borderRadius: '2px',
                  }}>
                    <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', gap: '8px', marginBottom: '8px' }}>
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px', flexWrap: 'wrap' }}>
                          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{agent.name}</span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, fontSize: '8px', borderRadius: '2px' }}>
                            {agent.category.toUpperCase()}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.PURPLE}20`, color: FINCEPT.PURPLE, fontSize: '8px', borderRadius: '2px' }}>
                            {cfg.model?.provider?.toUpperCase() || 'UNKNOWN'}
                          </span>
                        </div>
                        {agent.description && (
                          <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '4px' }}>{agent.description}</div>
                        )}
                        <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
                          Model: {cfg.model?.model_id || 'N/A'} · Temp: {cfg.model?.temperature ?? 'N/A'}
                          {(cfg.tools?.length ?? 0) > 0 && ` · Tools: ${cfg.tools!.join(', ')}`}
                        </div>
                      </div>
                      <div style={{ display: 'flex', gap: '4px', flexShrink: 0 }}>
                        <button
                          onClick={() => handleEdit(agent)}
                          style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.WHITE, fontSize: '9px', cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px' }}
                        >
                          <Edit3 size={10} /> EDIT
                        </button>
                        <button
                          onClick={() => handleExportAgent(agent)}
                          title="Export agent as JSON"
                          style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.GREEN}60`, color: FINCEPT.GREEN, fontSize: '9px', cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px' }}
                        >
                          <Download size={10} /> JSON
                        </button>
                        <button
                          onClick={() => handleDelete(agent.id, agent.name)}
                          disabled={deletingId === agent.id}
                          style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.RED}40`, color: FINCEPT.RED, fontSize: '9px', cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px', opacity: deletingId === agent.id ? 0.5 : 1 }}
                        >
                          {deletingId === agent.id ? <Loader2 size={10} className="animate-spin" /> : <Trash2 size={10} />}
                          DELETE
                        </button>
                      </div>
                    </div>

                    {/* Instructions preview */}
                    {cfg.instructions && (
                      <div style={{
                        padding: '8px', backgroundColor: FINCEPT.DARK_BG,
                        border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px',
                        fontSize: '9px', color: FINCEPT.MUTED,
                        overflow: 'hidden', maxHeight: '40px',
                        display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical',
                      }}>
                        {cfg.instructions}
                      </div>
                    )}

                    <div style={{ marginTop: '8px', fontSize: '8px', color: FINCEPT.MUTED }}>
                      Created: {new Date(agent.created_at).toLocaleDateString()} · ID: {agent.id.slice(0, 24)}...
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Footer hint */}
        <div style={{ padding: '8px 16px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.BORDER}`, flexShrink: 0 }}>
          <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
            Custom agents are saved to SQLite and automatically appear in AGENTS tab under their category. Use them in the Node Editor, Teams, and Chat.
          </span>
        </div>
      </div>
    </div>
  );
};
