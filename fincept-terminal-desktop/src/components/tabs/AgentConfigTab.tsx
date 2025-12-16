// File: src/components/tabs/AgentConfigTab.tsx
// Professional Bloomberg Terminal-Grade Agent Configuration Interface

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  Bot,
  Brain,
  RefreshCw,
  Save,
  Play,
  Settings as SettingsIcon,
  Code,
  ChevronDown,
  AlertCircle,
  CheckCircle,
  Zap,
  Database,
  Eye,
  EyeOff,
} from 'lucide-react';

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

interface ProviderInfo {
  name: string;
  models: string[];
  api_key_required: boolean;
}

const AgentConfigTab: React.FC = () => {
  const [selectedCategory, setSelectedCategory] = useState('TraderInvestorsAgent');
  const [agents, setAgents] = useState<AgentConfig[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<AgentConfig | null>(null);
  const [jsonText, setJsonText] = useState('');
  const [providers, setProviders] = useState<Record<string, ProviderInfo>>({});
  const [loading, setLoading] = useState(false);
  const [testResult, setTestResult] = useState<any>(null);
  const [showJsonEditor, setShowJsonEditor] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  const categories = [
    { id: 'TraderInvestorsAgent', name: 'Trader/Investor Agents', icon: '💰' },
    { id: 'GeopoliticsAgents', name: 'Geopolitics Agents', icon: '🌍' },
    { id: 'EconomicAgents', name: 'Economic Agents', icon: '📊' },
    { id: 'hedgeFundAgents', name: 'Hedge Fund Team', icon: '🏦' },
  ];

  // Load agents on mount and category change
  useEffect(() => {
    loadAgents();
    loadProviders();
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
    try {
      const result = await invoke('execute_python_agent_command', {
        action: 'list_agents',
        parameters: { category: selectedCategory },
        inputs: null,
      });

      const data = result as any;
      if (data.success) {
        setAgents(data.agents || []);
        if (data.agents && data.agents.length > 0) {
          setSelectedAgent(data.agents[0]);
        }
      } else {
        setError(data.error || 'Failed to load agents');
      }
    } catch (err: any) {
      console.error('Failed to load agents:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const loadProviders = async () => {
    try {
      const result = await invoke('execute_python_agent_command', {
        action: 'get_providers',
        parameters: {},
        inputs: null,
      });

      const data = result as any;
      if (data.success && data.providers) {
        setProviders(data.providers);
      }
    } catch (err) {
      console.error('Failed to load providers:', err);
    }
  };

  const saveConfig = async () => {
    if (!selectedAgent) return;

    setLoading(true);
    setError(null);
    setSuccess(null);
    try {
      // Parse the JSON text to validate and get updated config
      const updatedConfig = JSON.parse(jsonText);

      const result = await invoke('execute_python_agent_command', {
        action: 'update_config',
        parameters: {
          agent_id: selectedAgent.id,
          category: selectedCategory,
          updates: updatedConfig,
        },
        inputs: null,
      });

      const data = result as any;
      if (data.success) {
        setSuccess('Configuration saved successfully!');
        setSelectedAgent(updatedConfig);
        setTimeout(() => setSuccess(null), 3000);
      } else {
        setError(data.error || 'Failed to save configuration');
      }
    } catch (err: any) {
      console.error('Failed to save config:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const testAgent = async () => {
    if (!selectedAgent) return;

    setLoading(true);
    setError(null);
    setTestResult(null);
    try {
      const result = await invoke('execute_python_agent_command', {
        action: 'execute_agent',
        parameters: {
          agent_id: selectedAgent.id,
          category: selectedCategory,
          query: 'Test query: Provide a brief summary of your role and capabilities.',
        },
        inputs: {
          api_keys: {}, // User would need to provide API keys
        },
      });

      const data = result as any;
      setTestResult(data);
    } catch (err: any) {
      console.error('Failed to test agent:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

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
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${BLOOMBERG.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${BLOOMBERG.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${BLOOMBERG.MUTED}; }

        .terminal-glow {
          text-shadow: 0 0 10px ${BLOOMBERG.ORANGE}40;
        }
      `}</style>

      {/* ========== TOP NAVIGATION BAR ========== */}
      <div style={{
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${BLOOMBERG.ORANGE}20`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Bot size={18} color={BLOOMBERG.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + BLOOMBERG.ORANGE + ')' }} />
            <span style={{
              color: BLOOMBERG.ORANGE,
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: `0 0 10px ${BLOOMBERG.ORANGE}40`
            }}>
              AGENT CONFIGURATION
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Category Selector */}
          <div style={{ display: 'flex', gap: '8px' }}>
            {categories.map((cat) => (
              <button
                key={cat.id}
                onClick={() => setSelectedCategory(cat.id)}
                style={{
                  padding: '4px 12px',
                  backgroundColor: selectedCategory === cat.id ? BLOOMBERG.ORANGE : BLOOMBERG.PANEL_BG,
                  border: `1px solid ${selectedCategory === cat.id ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                  color: selectedCategory === cat.id ? BLOOMBERG.DARK_BG : BLOOMBERG.CYAN,
                  cursor: 'pointer',
                  fontSize: '10px',
                  fontWeight: 600,
                  transition: 'all 0.2s',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                <span>{cat.icon}</span>
                <span>{cat.name}</span>
              </button>
            ))}
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
            {showJsonEditor ? 'Hide JSON' : 'Show JSON'}
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

      {/* ========== MAIN CONTENT ========== */}
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
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedAgent?.id !== agent.id) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                      e.currentTarget.style.borderColor = BLOOMBERG.MUTED;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (selectedAgent?.id !== agent.id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                    }
                  }}
                >
                  <div style={{ fontWeight: 600, color: BLOOMBERG.CYAN, marginBottom: '4px' }}>
                    {agent.name}
                  </div>
                  <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '4px' }}>
                    {agent.role}
                  </div>
                  <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                    <span style={{
                      padding: '2px 6px',
                      backgroundColor: BLOOMBERG.DARK_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '8px',
                      color: BLOOMBERG.YELLOW
                    }}>
                      {agent.llm_config.provider}
                    </span>
                    <span style={{
                      padding: '2px 6px',
                      backgroundColor: BLOOMBERG.DARK_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      borderRadius: '2px',
                      fontSize: '8px',
                      color: BLOOMBERG.PURPLE
                    }}>
                      {agent.llm_config.model_id}
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
              <span style={{ color: error ? BLOOMBERG.RED : BLOOMBERG.GREEN }}>
                {error || success}
              </span>
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
                <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>
                  {selectedAgent.description}
                </div>
                <div style={{ display: 'flex', gap: '8px' }}>
                  <button
                    onClick={saveConfig}
                    disabled={loading}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: BLOOMBERG.GREEN,
                      border: 'none',
                      color: BLOOMBERG.DARK_BG,
                      cursor: loading ? 'not-allowed' : 'pointer',
                      fontSize: '10px',
                      fontWeight: 600,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px'
                    }}
                  >
                    <Save size={12} />
                    Save Configuration
                  </button>

                  <button
                    onClick={testAgent}
                    disabled={loading}
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
                      gap: '4px'
                    }}
                  >
                    <Play size={12} />
                    Test Agent
                  </button>
                </div>
              </div>

              {/* JSON Editor */}
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
                    {/* Basic Info */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        ROLE
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        fontSize: '11px',
                        color: BLOOMBERG.YELLOW
                      }}>
                        {selectedAgent.role}
                      </div>
                    </div>

                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        GOAL
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        fontSize: '11px'
                      }}>
                        {selectedAgent.goal}
                      </div>
                    </div>

                    {/* LLM Config */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        LLM CONFIGURATION
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        display: 'grid',
                        gridTemplateColumns: '1fr 1fr',
                        gap: '8px',
                        fontSize: '10px'
                      }}>
                        <div>
                          <span style={{ color: BLOOMBERG.GRAY }}>Provider:</span>{' '}
                          <span style={{ color: BLOOMBERG.CYAN }}>{selectedAgent.llm_config.provider}</span>
                        </div>
                        <div>
                          <span style={{ color: BLOOMBERG.GRAY }}>Model:</span>{' '}
                          <span style={{ color: BLOOMBERG.PURPLE }}>{selectedAgent.llm_config.model_id}</span>
                        </div>
                        <div>
                          <span style={{ color: BLOOMBERG.GRAY }}>Temperature:</span>{' '}
                          <span style={{ color: BLOOMBERG.YELLOW }}>{selectedAgent.llm_config.temperature}</span>
                        </div>
                        <div>
                          <span style={{ color: BLOOMBERG.GRAY }}>Max Tokens:</span>{' '}
                          <span style={{ color: BLOOMBERG.YELLOW }}>{selectedAgent.llm_config.max_tokens}</span>
                        </div>
                      </div>
                    </div>

                    {/* Tools */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        TOOLS ({selectedAgent.tools.length})
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        display: 'flex',
                        gap: '6px',
                        flexWrap: 'wrap'
                      }}>
                        {selectedAgent.tools.map((tool, idx) => (
                          <span
                            key={idx}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: BLOOMBERG.DARK_BG,
                              border: `1px solid ${BLOOMBERG.BORDER}`,
                              color: BLOOMBERG.GREEN,
                              fontSize: '9px'
                            }}
                          >
                            {tool}
                          </span>
                        ))}
                      </div>
                    </div>

                    {/* Scoring Weights */}
                    {(selectedAgent as any).scoring_weights && Object.keys((selectedAgent as any).scoring_weights).length > 0 && (
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                          SCORING WEIGHTS (User Configurable)
                        </div>
                        <div style={{
                          padding: '8px',
                          backgroundColor: BLOOMBERG.PANEL_BG,
                          border: `1px solid ${BLOOMBERG.ORANGE}`,
                          display: 'grid',
                          gridTemplateColumns: '1fr 1fr',
                          gap: '8px',
                          fontSize: '10px'
                        }}>
                          {Object.entries((selectedAgent as any).scoring_weights || {}).map(([key, value]) => (
                            <div key={key}>
                              <span style={{ color: BLOOMBERG.GRAY }}>{key}:</span>{' '}
                              <span style={{ color: BLOOMBERG.ORANGE, fontWeight: 600 }}>{((value as number) * 100).toFixed(0)}%</span>
                            </div>
                          ))}
                        </div>
                      </div>
                    )}

                    {/* Thresholds */}
                    {(selectedAgent as any).thresholds && Object.keys((selectedAgent as any).thresholds).length > 0 && (
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                          ANALYSIS THRESHOLDS (User Configurable)
                        </div>
                        <div style={{
                          padding: '8px',
                          backgroundColor: BLOOMBERG.PANEL_BG,
                          border: `1px solid ${BLOOMBERG.CYAN}`,
                          display: 'grid',
                          gridTemplateColumns: '1fr 1fr 1fr',
                          gap: '6px',
                          fontSize: '9px',
                          maxHeight: '150px',
                          overflow: 'auto'
                        }}>
                          {Object.entries((selectedAgent as any).thresholds || {}).map(([key, value]) => (
                            <div key={key}>
                              <span style={{ color: BLOOMBERG.GRAY }}>{key}:</span>{' '}
                              <span style={{ color: BLOOMBERG.CYAN }}>{String(value)}</span>
                            </div>
                          ))}
                        </div>
                      </div>
                    )}

                    {/* Data Sources */}
                    {(selectedAgent as any).data_sources && (
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                          DATA SOURCES (User Configurable)
                        </div>
                        <div style={{
                          padding: '8px',
                          backgroundColor: BLOOMBERG.PANEL_BG,
                          border: `1px solid ${BLOOMBERG.PURPLE}`,
                          fontSize: '10px'
                        }}>
                          {(selectedAgent as any).data_sources?.line_items && Array.isArray((selectedAgent as any).data_sources.line_items) && (
                            <div style={{ marginBottom: '6px' }}>
                              <span style={{ color: BLOOMBERG.GRAY }}>Line Items:</span>{' '}
                              <span style={{ color: BLOOMBERG.PURPLE }}>
                                {(selectedAgent as any).data_sources.line_items.join(', ')}
                              </span>
                            </div>
                          )}
                          {(selectedAgent as any).data_sources?.years_of_data != null && (
                            <div>
                              <span style={{ color: BLOOMBERG.GRAY }}>Years of Data:</span>{' '}
                              <span style={{ color: BLOOMBERG.YELLOW }}>{(selectedAgent as any).data_sources.years_of_data}</span>
                            </div>
                          )}
                        </div>
                      </div>
                    )}

                    {/* Instructions */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        INSTRUCTIONS
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        fontSize: '10px',
                        maxHeight: '200px',
                        overflow: 'auto',
                        lineHeight: '1.5'
                      }}>
                        {selectedAgent.instructions}
                      </div>
                    </div>

                    {/* Test Result */}
                    {testResult && (
                      <div>
                        <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                          TEST RESULT
                        </div>
                        <div style={{
                          padding: '8px',
                          backgroundColor: BLOOMBERG.DARK_BG,
                          border: `1px solid ${testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
                          fontSize: '10px',
                          maxHeight: '200px',
                          overflow: 'auto',
                          fontFamily: 'monospace'
                        }}>
                          <pre style={{ margin: 0, color: testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
                            {JSON.stringify(testResult, null, 2)}
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
                <Bot size={48} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
                <div>Select an agent to view configuration</div>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default AgentConfigTab;
