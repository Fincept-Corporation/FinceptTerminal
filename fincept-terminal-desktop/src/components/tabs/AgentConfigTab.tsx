// File: src/components/tabs/AgentConfigTab.tsx
// Professional Bloomberg Terminal-Grade Agent Configuration Interface

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { resolveResource } from '@tauri-apps/api/path';
import { readTextFile } from '@tauri-apps/plugin-fs';
import {
  Bot,
  RefreshCw,
  Play,
  Code,
  AlertCircle,
  CheckCircle,
  Eye,
  EyeOff,
  Users,
  TrendingUp,
  Globe,
  Building2,
} from 'lucide-react';
import { getLLMConfigs } from '@/services/sqliteService';
import { TabFooter } from '@/components/common/TabFooter';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface AgentConfig {
  id: string;
  name: string;
  role: string;
  goal: string;
  description: string;
  llm_config: {
    provider: string;
    model_id: string;
    temperature: number;
    max_tokens: number;
  };
  tools: string[];
  enable_memory: boolean;
  enable_agentic_memory: boolean;
  instructions: string;
}

const AgentConfigTab: React.FC = () => {
  const [selectedCategory, setSelectedCategory] = useState('TraderInvestorsAgent');
  const [agents, setAgents] = useState<AgentConfig[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<AgentConfig | null>(null);
  const [jsonText, setJsonText] = useState('');
  const [loading, setLoading] = useState(false);
  const [testResult, setTestResult] = useState<any>(null);
  const [showJsonEditor, setShowJsonEditor] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const [testQuery, setTestQuery] = useState('Analyze AAPL stock and provide your investment thesis.');

  const categories = [
    { id: 'TraderInvestorsAgent', name: 'TRADERS', icon: TrendingUp, configFile: 'agent_definitions.json' },
    { id: 'GeopoliticsAgents', name: 'GEOPOLITICS', icon: Globe, configFile: 'agent_definitions.json' },
    { id: 'EconomicAgents', name: 'ECONOMICS', icon: Building2, configFile: 'agent_definitions.json' },
    { id: 'hedgeFundAgents', name: 'HEDGE FUNDS', icon: Users, configFile: 'team_config.json' },
  ];

  // Load agents on mount and category change
  useEffect(() => {
    loadAgents();
  }, [selectedCategory]);

  // Update JSON when selected agent changes
  useEffect(() => {
    if (selectedAgent) {
      setJsonText(JSON.stringify(selectedAgent, null, 2));
    }
  }, [selectedAgent]);

  const loadAgents = async () => {
    setLoading(true);
    setError(null);
    setAgents([]);
    setSelectedAgent(null);

    try {
      const category = categories.find(c => c.id === selectedCategory);
      if (!category) {
        setError('Invalid category');
        setLoading(false);
        return;
      }

      // Try to load from resource path
      const configPath = `resources/scripts/agents/${selectedCategory}/configs/${category.configFile}`;

      try {
        const resourcePath = await resolveResource(configPath);
        const content = await readTextFile(resourcePath);
        const data = JSON.parse(content);

        const agentList = data.agents || [];
        setAgents(agentList);
        if (agentList.length > 0) {
          setSelectedAgent(agentList[0]);
        }
      } catch (fileError) {
        // Fallback: try invoke command to read file
        try {
          const result = await invoke<string>('read_agent_config', {
            category: selectedCategory,
            configFile: category.configFile,
          });
          const data = JSON.parse(result);
          const agentList = data.agents || [];
          setAgents(agentList);
          if (agentList.length > 0) {
            setSelectedAgent(agentList[0]);
          }
        } catch (invokeError) {
          console.error('Failed to load agents via invoke:', invokeError);
          setError(`Failed to load agents: ${fileError}`);
        }
      }
    } catch (err: any) {
      console.error('Failed to load agents:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  // Fetch API keys from Settings LLM configurations
  const fetchApiKeysFromSettings = async (): Promise<Record<string, string>> => {
    try {
      const llmConfigs = await getLLMConfigs();
      const apiKeys: Record<string, string> = {};

      for (const config of llmConfigs) {
        if (config.api_key) {
          const providerUpper = config.provider.toUpperCase();
          apiKeys[`${providerUpper}_API_KEY`] = config.api_key;
          apiKeys[config.provider] = config.api_key;
          apiKeys[providerUpper] = config.api_key;
        }
      }

      return apiKeys;
    } catch (err) {
      console.error('Failed to fetch API keys from settings:', err);
      return {};
    }
  };

  const testAgent = async () => {
    if (!selectedAgent || !testQuery.trim()) return;

    setLoading(true);
    setError(null);
    setTestResult(null);
    setSuccess(null);

    try {
      const apiKeys = await fetchApiKeysFromSettings();
      const provider = selectedAgent.llm_config.provider;

      if (!apiKeys[provider] && !apiKeys[`${provider.toUpperCase()}_API_KEY`] && provider !== 'ollama' && provider !== 'fincept') {
        setError(`No API key found for ${provider}. Configure it in Settings > LLM Config.`);
        setLoading(false);
        return;
      }

      // Build config from selected agent
      const config = {
        model: {
          provider: selectedAgent.llm_config.provider,
          model_id: selectedAgent.llm_config.model_id,
          temperature: selectedAgent.llm_config.temperature,
          max_tokens: selectedAgent.llm_config.max_tokens,
        },
        instructions: selectedAgent.instructions,
        tools: selectedAgent.tools,
        memory: { enabled: selectedAgent.enable_memory },
        debug: false,
      };

      const result = await invoke<string>('execute_core_agent', {
        queryJson: testQuery,
        configJson: JSON.stringify(config),
        apiKeysJson: JSON.stringify(apiKeys),
      });

      const parsed = JSON.parse(result);
      setTestResult(parsed);
      if (parsed.success) {
        setSuccess('Agent executed successfully');
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      console.error('Failed to test agent:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const CategoryIcon = categories.find(c => c.id === selectedCategory)?.icon || Bot;

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${BLOOMBERG.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${BLOOMBERG.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${BLOOMBERG.MUTED}; }
        @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
        .animate-spin { animation: spin 1s linear infinite; }
      `}</style>

      {/* TOP NAVIGATION BAR */}
      <div style={{
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Bot size={18} color={BLOOMBERG.ORANGE} />
            <span style={{ color: BLOOMBERG.ORANGE, fontWeight: 700, fontSize: '14px', letterSpacing: '0.5px' }}>
              AGENT CONFIGURATION
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Category Selector */}
          <div style={{ display: 'flex', gap: '4px' }}>
            {categories.map((cat) => {
              const Icon = cat.icon;
              return (
                <button
                  key={cat.id}
                  onClick={() => setSelectedCategory(cat.id)}
                  style={{
                    padding: '4px 10px',
                    backgroundColor: selectedCategory === cat.id ? BLOOMBERG.ORANGE : BLOOMBERG.PANEL_BG,
                    border: `1px solid ${selectedCategory === cat.id ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                    color: selectedCategory === cat.id ? BLOOMBERG.DARK_BG : BLOOMBERG.CYAN,
                    cursor: 'pointer',
                    fontSize: '9px',
                    fontWeight: 600,
                    letterSpacing: '0.5px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                  }}
                >
                  <Icon size={10} />
                  {cat.name}
                </button>
              );
            })}
          </div>
        </div>

        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <button
            onClick={() => setShowJsonEditor(!showJsonEditor)}
            style={{
              padding: '4px 10px',
              backgroundColor: showJsonEditor ? BLOOMBERG.ORANGE : BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: showJsonEditor ? BLOOMBERG.DARK_BG : BLOOMBERG.CYAN,
              cursor: 'pointer',
              fontSize: '10px',
              fontWeight: 600,
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            {showJsonEditor ? <EyeOff size={12} /> : <Eye size={12} />}
            JSON
          </button>

          <button
            onClick={loadAgents}
            disabled={loading}
            style={{
              padding: '4px 10px',
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.CYAN,
              cursor: loading ? 'not-allowed' : 'pointer',
              fontSize: '10px',
              fontWeight: 600,
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
            Refresh
          </button>
        </div>
      </div>

      {/* MAIN CONTENT */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Agent List */}
        <div style={{
          width: '280px',
          borderRight: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          backgroundColor: BLOOMBERG.PANEL_BG
        }}>
          <div style={{
            padding: '8px 12px',
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            backgroundColor: BLOOMBERG.HEADER_BG,
            fontSize: '11px',
            fontWeight: 600,
            color: BLOOMBERG.GRAY
          }}>
            AVAILABLE AGENTS ({agents.length})
          </div>

          <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
            {loading && agents.length === 0 ? (
              <div style={{ textAlign: 'center', padding: '20px', color: BLOOMBERG.GRAY }}>
                <RefreshCw size={20} className="animate-spin" style={{ margin: '0 auto' }} />
                <div style={{ marginTop: '8px', fontSize: '10px' }}>Loading agents...</div>
              </div>
            ) : agents.length === 0 ? (
              <div style={{ textAlign: 'center', padding: '20px', color: BLOOMBERG.GRAY }}>
                <AlertCircle size={20} style={{ margin: '0 auto' }} />
                <div style={{ marginTop: '8px', fontSize: '10px' }}>No agents found</div>
              </div>
            ) : (
              agents.map((agent) => (
                <button
                  key={agent.id}
                  onClick={() => setSelectedAgent(agent)}
                  style={{
                    width: '100%',
                    padding: '10px',
                    marginBottom: '6px',
                    backgroundColor: selectedAgent?.id === agent.id ? BLOOMBERG.HOVER : 'transparent',
                    border: `1px solid ${selectedAgent?.id === agent.id ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                    color: BLOOMBERG.WHITE,
                    cursor: 'pointer',
                    textAlign: 'left',
                    fontSize: '10px',
                    transition: 'all 0.15s'
                  }}
                >
                  <div style={{ fontWeight: 600, color: BLOOMBERG.CYAN, marginBottom: '4px' }}>
                    {agent.name}
                  </div>
                  <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '4px', lineHeight: 1.3 }}>
                    {agent.role?.substring(0, 60)}{agent.role?.length > 60 ? '...' : ''}
                  </div>
                  <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                    <span style={{
                      padding: '2px 6px',
                      backgroundColor: BLOOMBERG.DARK_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      fontSize: '8px',
                      color: BLOOMBERG.YELLOW
                    }}>
                      {agent.llm_config?.provider || 'N/A'}
                    </span>
                    <span style={{
                      padding: '2px 6px',
                      backgroundColor: BLOOMBERG.DARK_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      fontSize: '8px',
                      color: BLOOMBERG.PURPLE
                    }}>
                      {agent.llm_config?.model_id?.substring(0, 20) || 'N/A'}
                    </span>
                  </div>
                </button>
              ))
            )}
          </div>
        </div>

        {/* Right Panel - Configuration Editor */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Status Messages */}
          {(error || success) && (
            <div style={{
              padding: '8px 12px',
              backgroundColor: error ? BLOOMBERG.RED + '20' : BLOOMBERG.GREEN + '20',
              borderBottom: `1px solid ${error ? BLOOMBERG.RED : BLOOMBERG.GREEN}`,
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '10px'
            }}>
              {error ? <AlertCircle size={14} color={BLOOMBERG.RED} /> : <CheckCircle size={14} color={BLOOMBERG.GREEN} />}
              <span style={{ color: error ? BLOOMBERG.RED : BLOOMBERG.GREEN, flex: 1 }}>
                {error || success}
              </span>
              <button
                onClick={() => { setError(null); setSuccess(null); }}
                style={{ background: 'none', border: 'none', color: 'inherit', cursor: 'pointer', fontSize: '14px' }}
              >
                x
              </button>
            </div>
          )}

          {selectedAgent ? (
            <>
              {/* Agent Info Header */}
              <div style={{
                padding: '12px',
                borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                backgroundColor: BLOOMBERG.HEADER_BG
              }}>
                <div style={{ fontSize: '16px', fontWeight: 700, color: BLOOMBERG.CYAN, marginBottom: '6px' }}>
                  {selectedAgent.name}
                </div>
                <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '8px', lineHeight: 1.4 }}>
                  {selectedAgent.description}
                </div>

                {/* Test Query Input */}
                <div style={{ marginBottom: '8px' }}>
                  <input
                    type="text"
                    value={testQuery}
                    onChange={(e) => setTestQuery(e.target.value)}
                    placeholder="Enter test query..."
                    style={{
                      width: '100%',
                      padding: '6px 10px',
                      backgroundColor: BLOOMBERG.DARK_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      color: BLOOMBERG.WHITE,
                      fontSize: '10px',
                      outline: 'none',
                    }}
                  />
                </div>

                <div style={{ display: 'flex', gap: '8px' }}>
                  <button
                    onClick={testAgent}
                    disabled={loading || !testQuery.trim()}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: BLOOMBERG.BLUE,
                      border: 'none',
                      color: BLOOMBERG.WHITE,
                      cursor: loading ? 'not-allowed' : 'pointer',
                      fontSize: '10px',
                      fontWeight: 600,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px',
                      opacity: loading || !testQuery.trim() ? 0.5 : 1,
                    }}
                  >
                    <Play size={12} />
                    {loading ? 'Running...' : 'Run Agent'}
                  </button>
                </div>
              </div>

              {/* Content Area */}
              <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
                {showJsonEditor ? (
                  <div style={{ height: '100%' }}>
                    <div style={{
                      fontSize: '10px',
                      fontWeight: 600,
                      color: BLOOMBERG.GRAY,
                      marginBottom: '8px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}>
                      <Code size={12} />
                      JSON CONFIGURATION
                    </div>
                    <textarea
                      value={jsonText}
                      onChange={(e) => setJsonText(e.target.value)}
                      style={{
                        width: '100%',
                        height: 'calc(100% - 30px)',
                        backgroundColor: BLOOMBERG.DARK_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        color: BLOOMBERG.CYAN,
                        padding: '12px',
                        fontSize: '11px',
                        fontFamily: '"IBM Plex Mono", monospace',
                        resize: 'none',
                        outline: 'none'
                      }}
                      spellCheck={false}
                    />
                  </div>
                ) : (
                  <div style={{ display: 'grid', gap: '12px' }}>
                    {/* Role & Goal */}
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>ROLE</div>
                        <div style={{ padding: '8px', backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}`, fontSize: '10px', color: BLOOMBERG.YELLOW }}>
                          {selectedAgent.role}
                        </div>
                      </div>
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>GOAL</div>
                        <div style={{ padding: '8px', backgroundColor: BLOOMBERG.PANEL_BG, border: `1px solid ${BLOOMBERG.BORDER}`, fontSize: '10px' }}>
                          {selectedAgent.goal}
                        </div>
                      </div>
                    </div>

                    {/* LLM Config */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>LLM CONFIGURATION</div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        display: 'grid',
                        gridTemplateColumns: 'repeat(4, 1fr)',
                        gap: '8px',
                        fontSize: '10px'
                      }}>
                        <div><span style={{ color: BLOOMBERG.GRAY }}>Provider:</span> <span style={{ color: BLOOMBERG.CYAN }}>{selectedAgent.llm_config?.provider}</span></div>
                        <div><span style={{ color: BLOOMBERG.GRAY }}>Model:</span> <span style={{ color: BLOOMBERG.PURPLE }}>{selectedAgent.llm_config?.model_id}</span></div>
                        <div><span style={{ color: BLOOMBERG.GRAY }}>Temp:</span> <span style={{ color: BLOOMBERG.YELLOW }}>{selectedAgent.llm_config?.temperature}</span></div>
                        <div><span style={{ color: BLOOMBERG.GRAY }}>Max Tokens:</span> <span style={{ color: BLOOMBERG.YELLOW }}>{selectedAgent.llm_config?.max_tokens}</span></div>
                      </div>
                    </div>

                    {/* Tools */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        TOOLS ({selectedAgent.tools?.length || 0})
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        display: 'flex',
                        gap: '6px',
                        flexWrap: 'wrap'
                      }}>
                        {(selectedAgent.tools || []).map((tool, idx) => (
                          <span key={idx} style={{
                            padding: '4px 8px',
                            backgroundColor: BLOOMBERG.DARK_BG,
                            border: `1px solid ${BLOOMBERG.BORDER}`,
                            color: BLOOMBERG.GREEN,
                            fontSize: '9px'
                          }}>
                            {tool}
                          </span>
                        ))}
                      </div>
                    </div>

                    {/* Instructions */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>INSTRUCTIONS</div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        fontSize: '10px',
                        maxHeight: '150px',
                        overflow: 'auto',
                        lineHeight: '1.5',
                        whiteSpace: 'pre-wrap',
                      }}>
                        {selectedAgent.instructions}
                      </div>
                    </div>

                    {/* Test Result */}
                    {testResult && (
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>TEST RESULT</div>
                        <div style={{
                          padding: '8px',
                          backgroundColor: BLOOMBERG.DARK_BG,
                          border: `1px solid ${testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
                          fontSize: '10px',
                          maxHeight: '200px',
                          overflow: 'auto',
                          fontFamily: 'monospace'
                        }}>
                          <pre style={{ margin: 0, color: testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED, whiteSpace: 'pre-wrap' }}>
                            {testResult.response || testResult.content || JSON.stringify(testResult, null, 2)}
                          </pre>
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            </>
          ) : (
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: BLOOMBERG.GRAY,
              fontSize: '11px'
            }}>
              <div style={{ textAlign: 'center' }}>
                <CategoryIcon size={48} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
                <div>Select an agent to view configuration</div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="AGENT CONFIG"
        leftInfo={[
          { label: `${agents.length} agents`, color: BLOOMBERG.CYAN },
          { label: selectedCategory, color: BLOOMBERG.YELLOW },
        ]}
        statusInfo="API keys configured in Settings > LLM Config"
      />
    </div>
  );
};

export default AgentConfigTab;
