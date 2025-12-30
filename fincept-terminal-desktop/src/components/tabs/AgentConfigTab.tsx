// File: src/components/tabs/AgentConfigTab.tsx
// Professional Bloomberg Terminal-Grade Agent Configuration Interface

import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import ReactMarkdown from 'react-markdown';
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
  Key,
} from 'lucide-react';
import { sqliteService, type LLMConfig } from '@/services/sqliteService';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';
import { useAuth } from '@/contexts/AuthContext';

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
  book_source?: string;
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
  const { t } = useTranslation('agentConfig');
  const { session } = useAuth();
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
  const [llmConfigs, setLlmConfigs] = useState<LLMConfig[]>([]);
  const [apiKeys, setApiKeys] = useState<Record<string, string>>({});
  const [testQuery, setTestQuery] = useState('Analyze China-Russia alliance dynamics');
  const [expandedBooks, setExpandedBooks] = useState<Set<string>>(new Set());
  const [teamName, setTeamName] = useState<string | null>(null);
  const [selectedBook, setSelectedBook] = useState<string | null>(null);

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
    loadLLMConfigs();
  }, [selectedCategory, session]);

  const loadLLMConfigs = async () => {
    try {
      let configs = await sqliteService.getLLMConfigs();
      console.log('[AgentConfigTab] Loaded LLM configs from DB:', configs);

      // Always ensure Fincept config exists with session's API key
      const userApiKey = session?.api_key || '';
      console.log('[AgentConfigTab] Session API key:', userApiKey ? `${userApiKey.substring(0, 8)}...` : 'MISSING');

      const finceptExists = configs.some(c => c.provider === 'fincept');

      if (!finceptExists && userApiKey) {
        console.log('[AgentConfigTab] Creating new Fincept config with user API key');
        const now = new Date().toISOString();
        const finceptConfig = {
          provider: 'fincept',
          api_key: userApiKey,
          base_url: 'https://finceptbackend.share.zrok.io/research/llm',
          model: 'fincept-llm',
          is_active: true,
          created_at: now,
          updated_at: now,
        };
        await sqliteService.saveLLMConfig(finceptConfig);
        configs = await sqliteService.getLLMConfigs();
      } else if (finceptExists && userApiKey) {
        // Update existing Fincept config with latest user API key
        const finceptConfig = configs.find(c => c.provider === 'fincept');
        console.log('[AgentConfigTab] Existing Fincept config API key:', finceptConfig?.api_key ? `${finceptConfig.api_key.substring(0, 8)}...` : 'MISSING');
        if (finceptConfig && finceptConfig.api_key !== userApiKey) {
          console.log('[AgentConfigTab] Updating Fincept config with new user API key');
          finceptConfig.api_key = userApiKey;
          await sqliteService.saveLLMConfig(finceptConfig);
          configs = await sqliteService.getLLMConfigs();
        }
      }

      setLlmConfigs(configs);

      // Build API keys map - always prioritize user's Fincept key
      const keys: Record<string, string> = {};
      configs.forEach(config => {
        if (config.api_key && config.provider !== 'fincept') {
          // Skip fincept - we'll add it from user.api_key below
          keys[`${config.provider.toUpperCase()}_API_KEY`] = config.api_key;
        }
      });
      // Always add user's Fincept key (highest priority)
      if (userApiKey) {
        keys['FINCEPT_API_KEY'] = userApiKey;
      }
      console.log('[AgentConfigTab] Built API keys map:', Object.keys(keys));
      setApiKeys(keys);
    } catch (err) {
      console.error('Failed to load LLM configs:', err);
    }
  };

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
      // New simplified command - follows yfinance pattern
      const resultStr = await invoke<string>('list_agents', {
        category: selectedCategory,
      });

      const data = JSON.parse(resultStr);
      if (data.success) {
        setAgents(data.agents || []);
        setTeamName(data.team_name || null);
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
      // New simplified command
      const resultStr = await invoke<string>('get_agent_providers');
      const data = JSON.parse(resultStr);

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

      // New simplified command
      const resultStr = await invoke<string>('save_agent_config', {
        category: selectedCategory,
        agentId: selectedAgent.id,
        configJson: JSON.stringify(updatedConfig),
      });

      const data = JSON.parse(resultStr);
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
    if (!testQuery.trim()) {
      setError('Please enter a test query');
      return;
    }

    // Get active LLM config from Settings
    const activeLLM = llmConfigs.find(c => c.is_active);
    if (!activeLLM) {
      setError('No active LLM provider configured in Settings. Please configure and activate an LLM provider.');
      return;
    }

    console.log('Testing agent with API keys:', apiKeys);
    console.log('Session API key:', session?.api_key?.substring(0, 8));

    setLoading(true);
    setError(null);
    setTestResult(null);
    try {
      // New simplified command
      const resultStr = await invoke<string>('execute_single_agent', {
        category: selectedCategory,
        agentId: selectedAgent.id,
        query: testQuery,
        llmConfigJson: JSON.stringify({
          provider: activeLLM.provider,
          model: activeLLM.model,
        }),
        apiKeysJson: JSON.stringify(apiKeys),
      });

      const data = JSON.parse(resultStr);
      setTestResult(data);
    } catch (err: any) {
      console.error('Failed to test agent:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const testTeamCollaboration = async () => {
    if (!testQuery.trim()) {
      setError('Please enter a test query');
      return;
    }

    const activeLLM = llmConfigs.find(c => c.is_active);
    if (!activeLLM) {
      setError('No active LLM provider configured in Settings.');
      return;
    }

    // Get agents for selected book only
    const bookAgents = selectedBook
      ? agents.filter(a => a.book_source === selectedBook)
      : agents;

    const agentIds = bookAgents.map(a => a.id);

    setLoading(true);
    setError(null);
    setTestResult({
      success: true,
      discussion: [],
      total_agents: bookAgents.length,
      rounds: 2,
      query: testQuery
    });

    try {
      // New simplified command
      const resultStr = await invoke<string>('execute_agent_team', {
        category: selectedCategory,
        query: testQuery,
        llmConfigJson: JSON.stringify({
          provider: activeLLM.provider,
          model: activeLLM.model,
        }),
        apiKeysJson: JSON.stringify(apiKeys),
        agentIdsJson: agentIds.length > 0 ? JSON.stringify(agentIds) : null,
      });

      const data = JSON.parse(resultStr);
      setTestResult(data);

      if (!data.success) {
        setError(data.error || 'Team collaboration failed');
      }
    } catch (err: any) {
      console.error('Team collaboration failed:', err);
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
              {t('title')}
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
          {/* API Keys Indicator */}
          <div style={{
            padding: '4px 10px',
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `1px solid ${Object.keys(apiKeys).length > 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
            fontSize: '9px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            color: Object.keys(apiKeys).length > 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
          }}>
            <Key size={10} />
            {Object.keys(apiKeys).length} API Keys
          </div>

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
          width: '260px',
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
            {teamName ? `TEAM: ${teamName.toUpperCase()} (${agents.length})` : `AVAILABLE AGENTS (${agents.length})`}
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
              <>
                {(() => {
                  // Group agents by book_source
                  const grouped = agents.reduce((acc, agent) => {
                    const book = agent.book_source || 'Other';
                    if (!acc[book]) acc[book] = [];
                    acc[book].push(agent);
                    return acc;
                  }, {} as Record<string, AgentConfig[]>);

                  const books = Object.keys(grouped);

                  // If only one agent or no books, show flat list
                  if (books.length <= 1 && !teamName) {
                    return agents.map((agent) => (
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
                    ));
                  }

                  // Show book groups
                  return books.map(bookName => {
                    const bookAgents = grouped[bookName];
                    const isExpanded = expandedBooks.has(bookName);

                    return (
                      <div key={bookName} style={{ marginBottom: '8px' }}>
                        {/* Book Header */}
                        <button
                          onClick={() => {
                            const newExpanded = new Set(expandedBooks);
                            if (isExpanded) {
                              newExpanded.delete(bookName);
                            } else {
                              newExpanded.add(bookName);
                            }
                            setExpandedBooks(newExpanded);
                            setSelectedBook(isExpanded ? null : bookName);
                          }}
                          style={{
                            width: '100%',
                            padding: '10px',
                            backgroundColor: isExpanded ? BLOOMBERG.ORANGE + '20' : BLOOMBERG.HEADER_BG,
                            border: `1px solid ${isExpanded ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                            color: BLOOMBERG.WHITE,
                            cursor: 'pointer',
                            textAlign: 'left',
                            fontSize: '10px',
                            fontWeight: 600,
                            marginBottom: isExpanded ? '6px' : '0'
                          }}
                        >
                          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                              <span style={{ fontSize: '14px' }}>📖</span>
                              <span style={{ color: BLOOMBERG.ORANGE }}>{bookName}</span>
                            </div>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                              <span style={{ color: BLOOMBERG.GRAY, fontSize: '9px' }}>
                                {bookAgents.length} agents
                              </span>
                              <ChevronDown
                                size={14}
                                style={{
                                  transform: isExpanded ? 'rotate(180deg)' : 'rotate(0deg)',
                                  transition: 'transform 0.2s'
                                }}
                              />
                            </div>
                          </div>
                        </button>

                        {/* Expanded Agents */}
                        {isExpanded && bookAgents.map((agent) => (
                          <button
                            key={agent.id}
                            onClick={() => setSelectedAgent(agent)}
                            style={{
                              width: 'calc(100% - 12px)',
                              padding: '10px',
                              marginBottom: '6px',
                              marginLeft: '12px',
                              backgroundColor: selectedAgent?.id === agent.id ? BLOOMBERG.HOVER : 'transparent',
                              border: `1px solid ${selectedAgent?.id === agent.id ? BLOOMBERG.CYAN : BLOOMBERG.BORDER}`,
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
                            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px' }}>
                              {agent.role}
                            </div>
                          </button>
                        ))}
                      </div>
                    );
                  });
                })()}
              </>
            )}
          </div>
        </div>

        {/* Middle Panel - Configuration Editor */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderRight: testResult ? `1px solid ${BLOOMBERG.BORDER}` : 'none' }}>
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

                {/* Test Query Input */}
                <div style={{ marginBottom: '8px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '4px' }}>
                    TEST QUERY
                  </div>
                  <textarea
                    value={testQuery}
                    onChange={(e) => setTestQuery(e.target.value)}
                    placeholder="Enter your test query here..."
                    style={{
                      width: '100%',
                      minHeight: '60px',
                      backgroundColor: BLOOMBERG.DARK_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      color: BLOOMBERG.WHITE,
                      padding: '6px 8px',
                      fontSize: '10px',
                      fontFamily: 'inherit',
                      resize: 'vertical',
                      outline: 'none'
                    }}
                  />
                </div>

                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
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
                    {loading ? 'Testing...' : 'Test Agent'}
                  </button>

                  {(teamName || (selectedBook && agents.filter(a => a.book_source === selectedBook).length > 1)) && (
                    <button
                      onClick={testTeamCollaboration}
                      disabled={loading}
                      style={{
                        padding: '6px 12px',
                        backgroundColor: BLOOMBERG.PURPLE,
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
                      <Brain size={12} />
                      {loading ? 'Collaborating...' : '🤝 Team Discussion'}
                    </button>
                  )}
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
                      <input
                        type="text"
                        value={selectedAgent.role}
                        onChange={(e) => {
                          const updated = { ...selectedAgent, role: e.target.value };
                          setSelectedAgent(updated);
                          setJsonText(JSON.stringify(updated, null, 2));
                        }}
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: BLOOMBERG.DARK_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`,
                          fontSize: '11px',
                          color: BLOOMBERG.YELLOW,
                          outline: 'none',
                          fontFamily: 'inherit'
                        }}
                      />
                    </div>

                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        GOAL
                      </div>
                      <textarea
                        value={selectedAgent.goal}
                        onChange={(e) => {
                          const updated = { ...selectedAgent, goal: e.target.value };
                          setSelectedAgent(updated);
                          setJsonText(JSON.stringify(updated, null, 2));
                        }}
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: BLOOMBERG.DARK_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`,
                          fontSize: '11px',
                          color: BLOOMBERG.WHITE,
                          outline: 'none',
                          resize: 'vertical',
                          minHeight: '60px',
                          fontFamily: 'inherit',
                          lineHeight: '1.5'
                        }}
                      />
                    </div>

                    {/* LLM Config - Show ACTIVE from Settings */}
                    <div>
                      <div style={{ fontSize: '10px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '6px' }}>
                        LLM CONFIGURATION (ACTIVE FROM SETTINGS)
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
                        {(() => {
                          const activeLLM = llmConfigs.find(c => c.is_active);
                          if (!activeLLM) {
                            return (
                              <div style={{ gridColumn: '1 / -1', color: BLOOMBERG.RED, textAlign: 'center' }}>
                                ⚠️ No active LLM configured in Settings
                              </div>
                            );
                          }
                          // Check apiKeys map for actual runtime API key (not DB config)
                          const apiKeyName = `${activeLLM.provider.toUpperCase()}_API_KEY`;
                          const hasApiKey = !!apiKeys[apiKeyName];

                          return (
                            <>
                              <div>
                                <span style={{ color: BLOOMBERG.GRAY }}>Provider:</span>{' '}
                                <span style={{ color: BLOOMBERG.CYAN }}>{activeLLM.provider}</span>
                              </div>
                              <div>
                                <span style={{ color: BLOOMBERG.GRAY }}>Model:</span>{' '}
                                <span style={{ color: BLOOMBERG.PURPLE }}>{activeLLM.model}</span>
                              </div>
                              <div>
                                <span style={{ color: BLOOMBERG.GRAY }}>API Key:</span>{' '}
                                <span style={{ color: hasApiKey ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
                                  {hasApiKey ? '✓ Configured' : '✗ Missing'}
                                </span>
                              </div>
                              <div>
                                <span style={{ color: BLOOMBERG.GRAY }}>Status:</span>{' '}
                                <span style={{ color: BLOOMBERG.GREEN }}>● ACTIVE</span>
                              </div>
                            </>
                          );
                        })()}
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
                      <textarea
                        value={selectedAgent.instructions}
                        onChange={(e) => {
                          const updated = { ...selectedAgent, instructions: e.target.value };
                          setSelectedAgent(updated);
                          setJsonText(JSON.stringify(updated, null, 2));
                        }}
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: BLOOMBERG.DARK_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`,
                          fontSize: '10px',
                          color: BLOOMBERG.WHITE,
                          outline: 'none',
                          resize: 'vertical',
                          minHeight: '150px',
                          fontFamily: 'inherit',
                          lineHeight: '1.6'
                        }}
                      />
                    </div>

                    {/* Save Button */}
                    <div style={{ display: 'flex', gap: '8px', marginTop: '12px' }}>
                      <button
                        onClick={saveConfig}
                        disabled={loading}
                        style={{
                          flex: 1,
                          padding: '10px',
                          backgroundColor: BLOOMBERG.ORANGE,
                          border: `1px solid ${BLOOMBERG.ORANGE}`,
                          color: BLOOMBERG.DARK_BG,
                          cursor: loading ? 'not-allowed' : 'pointer',
                          fontSize: '11px',
                          fontWeight: 700,
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          gap: '6px',
                          opacity: loading ? 0.6 : 1
                        }}
                      >
                        <Save size={14} />
                        {loading ? 'SAVING...' : 'SAVE CHANGES'}
                      </button>
                    </div>
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

        {/* Right Panel - Test Results */}
        {testResult && (
          <div style={{
            width: '400px',
            display: 'flex',
            flexDirection: 'column',
            backgroundColor: BLOOMBERG.PANEL_BG,
            overflow: 'hidden'
          }}>
            {/* Header */}
            <div style={{
              padding: '8px 12px',
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
              backgroundColor: BLOOMBERG.HEADER_BG,
              fontSize: '11px',
              fontWeight: 600,
              color: BLOOMBERG.GRAY,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between'
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <Zap size={12} color={testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED} />
                <span>TEST RESULT</span>
              </div>
              <button
                onClick={() => setTestResult(null)}
                style={{
                  padding: '2px 6px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.GRAY,
                  cursor: 'pointer',
                  fontSize: '9px'
                }}
              >
                Close
              </button>
            </div>

            {/* Result Content */}
            <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
              {/* Status Banner */}
              <div style={{
                padding: '8px',
                backgroundColor: testResult.success ? BLOOMBERG.GREEN + '20' : BLOOMBERG.RED + '20',
                border: `1px solid ${testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
                marginBottom: '12px',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                fontSize: '10px',
                color: testResult.success ? BLOOMBERG.GREEN : BLOOMBERG.RED
              }}>
                {testResult.success ? <CheckCircle size={14} /> : <AlertCircle size={14} />}
                <span style={{ fontWeight: 600 }}>
                  {testResult.success ? 'SUCCESS' : 'FAILED'}
                </span>
              </div>

              {/* Agent Info */}
              {testResult.agent_name && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '4px' }}>
                    AGENT
                  </div>
                  <div style={{
                    padding: '6px 8px',
                    backgroundColor: BLOOMBERG.DARK_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '10px',
                    color: BLOOMBERG.CYAN
                  }}>
                    {testResult.agent_name}
                  </div>
                </div>
              )}

              {/* Provider & Model */}
              {testResult.provider && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '4px' }}>
                    PROVIDER & MODEL
                  </div>
                  <div style={{
                    padding: '6px 8px',
                    backgroundColor: BLOOMBERG.DARK_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '10px',
                    display: 'flex',
                    gap: '8px'
                  }}>
                    <span style={{ color: BLOOMBERG.YELLOW }}>{testResult.provider}</span>
                    <span style={{ color: BLOOMBERG.GRAY }}>•</span>
                    <span style={{ color: BLOOMBERG.PURPLE }}>{testResult.model}</span>
                  </div>
                </div>
              )}

              {/* Team Discussion */}
              {testResult.discussion && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '4px' }}>
                    TEAM DISCUSSION ({testResult.total_agents} AGENTS, {testResult.rounds} ROUNDS)
                  </div>
                  <div style={{ maxHeight: '500px', overflow: 'auto' }}>
                    {testResult.discussion.map((entry: any, idx: number) => (
                      <div key={idx} style={{
                        padding: '8px',
                        backgroundColor: BLOOMBERG.DARK_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        marginBottom: '8px',
                        fontSize: '10px'
                      }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                          <span style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>{entry.agent}</span>
                          <span style={{ color: BLOOMBERG.GRAY, fontSize: '9px' }}>Round {entry.round}</span>
                        </div>
                        <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '4px' }}>
                          {entry.role}
                        </div>
                        <div style={{ color: BLOOMBERG.WHITE, lineHeight: '1.6' }}>
                          <ReactMarkdown
                            components={{
                              p: ({ node, ...props }) => <p style={{ margin: '0.5em 0' }} {...props} />,
                              strong: ({ node, ...props }) => <strong style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }} {...props} />,
                              em: ({ node, ...props }) => <em style={{ color: BLOOMBERG.YELLOW }} {...props} />,
                              ul: ({ node, ...props }) => <ul style={{ margin: '0.5em 0', paddingLeft: '1.5em' }} {...props} />,
                              ol: ({ node, ...props }) => <ol style={{ margin: '0.5em 0', paddingLeft: '1.5em' }} {...props} />,
                              li: ({ node, ...props }) => <li style={{ margin: '0.25em 0' }} {...props} />,
                            }}
                          >
                            {entry.response}
                          </ReactMarkdown>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}

              {/* Response */}
              {testResult.response && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '4px' }}>
                    RESPONSE
                  </div>
                  <div style={{
                    padding: '12px',
                    backgroundColor: BLOOMBERG.DARK_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '10px',
                    maxHeight: '400px',
                    overflow: 'auto',
                    lineHeight: '1.8',
                    color: BLOOMBERG.WHITE,
                    whiteSpace: 'pre-wrap'
                  }}>
                    <ReactMarkdown
                      components={{
                        p: ({ node, ...props }) => <p style={{ margin: '0.8em 0', lineHeight: '1.8' }} {...props} />,
                        strong: ({ node, ...props }) => <strong style={{ color: BLOOMBERG.CYAN, fontWeight: 700 }} {...props} />,
                        em: ({ node, ...props }) => <em style={{ color: BLOOMBERG.YELLOW, fontStyle: 'italic' }} {...props} />,
                        ul: ({ node, ...props }) => <ul style={{ margin: '0.8em 0', paddingLeft: '2em', listStyleType: 'disc' }} {...props} />,
                        ol: ({ node, ...props }) => <ol style={{ margin: '0.8em 0', paddingLeft: '2em', listStyleType: 'decimal' }} {...props} />,
                        li: ({ node, ...props }) => <li style={{ margin: '0.5em 0', lineHeight: '1.8' }} {...props} />,
                        h1: ({ node, ...props }) => <h1 style={{ color: BLOOMBERG.ORANGE, fontSize: '14px', fontWeight: 700, margin: '1em 0 0.5em' }} {...props} />,
                        h2: ({ node, ...props }) => <h2 style={{ color: BLOOMBERG.CYAN, fontSize: '12px', fontWeight: 700, margin: '1em 0 0.5em' }} {...props} />,
                        h3: ({ node, ...props }) => <h3 style={{ color: BLOOMBERG.YELLOW, fontSize: '11px', fontWeight: 600, margin: '0.8em 0 0.4em' }} {...props} />,
                        code: ({ node, inline, ...props }: any) => inline
                          ? <code style={{ backgroundColor: BLOOMBERG.HEADER_BG, padding: '2px 4px', color: BLOOMBERG.GREEN, fontFamily: 'IBM Plex Mono, monospace', fontSize: '9px' }} {...props} />
                          : <code style={{ display: 'block', backgroundColor: BLOOMBERG.HEADER_BG, padding: '8px', color: BLOOMBERG.GREEN, fontFamily: 'IBM Plex Mono, monospace', fontSize: '9px', margin: '0.5em 0', overflow: 'auto' }} {...props} />,
                        blockquote: ({ node, ...props }) => <blockquote style={{ borderLeft: `3px solid ${BLOOMBERG.ORANGE}`, paddingLeft: '12px', margin: '0.8em 0', color: BLOOMBERG.GRAY, fontStyle: 'italic' }} {...props} />,
                      }}
                    >
                      {testResult.response}
                    </ReactMarkdown>
                  </div>
                </div>
              )}

              {/* Tokens Used */}
              {testResult.tokens_used && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.GRAY, marginBottom: '4px' }}>
                    TOKENS USED
                  </div>
                  <div style={{
                    padding: '6px 8px',
                    backgroundColor: BLOOMBERG.DARK_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '10px',
                    color: BLOOMBERG.CYAN
                  }}>
                    {testResult.tokens_used.toLocaleString()}
                  </div>
                </div>
              )}

              {/* Error Message */}
              {testResult.error && (
                <div style={{ marginBottom: '12px' }}>
                  <div style={{ fontSize: '9px', fontWeight: 600, color: BLOOMBERG.RED, marginBottom: '4px' }}>
                    ERROR
                  </div>
                  <div style={{
                    padding: '8px',
                    backgroundColor: BLOOMBERG.DARK_BG,
                    border: `1px solid ${BLOOMBERG.RED}`,
                    fontSize: '10px',
                    color: BLOOMBERG.RED,
                    lineHeight: '1.6',
                    whiteSpace: 'pre-wrap'
                  }}>
                    {testResult.error}
                  </div>
                </div>
              )}

              {/* Raw JSON (collapsed by default) */}
              <details style={{ marginTop: '12px' }}>
                <summary style={{
                  fontSize: '9px',
                  fontWeight: 600,
                  color: BLOOMBERG.GRAY,
                  cursor: 'pointer',
                  marginBottom: '4px',
                  padding: '4px',
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`
                }}>
                  RAW JSON
                </summary>
                <div style={{
                  padding: '8px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  fontSize: '9px',
                  fontFamily: '"IBM Plex Mono", monospace',
                  maxHeight: '200px',
                  overflow: 'auto',
                  marginTop: '4px'
                }}>
                  <pre style={{ margin: 0, color: BLOOMBERG.CYAN }}>
                    {JSON.stringify(testResult, null, 2)}
                  </pre>
                </div>
              </details>
            </div>
          </div>
        )}
      </div>

      <TabFooter
        tabName="AGENT CONFIGURATION"
        leftInfo={[
          { label: `Agents: ${agents.length}`, color: BLOOMBERG.GRAY },
          { label: `LLM Configs: ${llmConfigs.length}`, color: BLOOMBERG.GRAY },
          { label: `Active: ${llmConfigs.filter(c => c.is_active).length}`, color: BLOOMBERG.GREEN },
        ]}
        statusInfo={testResult ? (testResult.success ? 'Test Passed' : 'Test Failed') : 'Ready'}
        backgroundColor={BLOOMBERG.PANEL_BG}
        borderColor={BLOOMBERG.BORDER}
      />
    </div>
  );
};

export default AgentConfigTab;
