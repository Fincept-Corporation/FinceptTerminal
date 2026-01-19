// File: src/components/tabs/AgentConfigTab.tsx
// Professional Fincept Terminal-Grade Agent Configuration Interface
// Extended with Teams, Workflows, Tools, Structured Outputs

import React, { useState, useEffect, useCallback } from 'react';
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
  Wrench,
  Database,
  Brain,
  GitBranch,
  FileText,
  Settings,
  Zap,
  Shield,
  Activity,
  Cpu,
  MessageSquare,
  Trash2,
  Plus,
  ChevronRight,
  ChevronDown,
  Search,
  Copy,
  Check,
} from 'lucide-react';
import { getLLMConfigs } from '@/services/core/sqliteService';
import { TabFooter } from '@/components/common/TabFooter';

// Fincept Professional Color Palette
const FINCEPT = {
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

interface SystemInfo {
  version: string;
  framework: string;
  capabilities: {
    model_providers: number;
    tools: number;
    tool_categories: string[];
    vectordbs: string[];
    embedders: string[];
    output_models: string[];
  };
  features: string[];
}

interface ToolsInfo {
  tools: Record<string, string[]>;
  categories: string[];
  total_count: number;
}

interface ChatMessage {
  role: 'user' | 'assistant';
  content: string;
  timestamp: Date;
  agentName?: string;
  outputModel?: string;
}

type ViewMode = 'agents' | 'teams' | 'workflows' | 'tools' | 'system' | 'chat';

const AgentConfigTab: React.FC = () => {
  // Core state
  const [viewMode, setViewMode] = useState<ViewMode>('agents');
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

  // Extended state
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [toolsInfo, setToolsInfo] = useState<ToolsInfo | null>(null);
  const [selectedOutputModel, setSelectedOutputModel] = useState<string>('');
  const [chatMessages, setChatMessages] = useState<ChatMessage[]>([]);
  const [chatInput, setChatInput] = useState('');
  const [expandedCategories, setExpandedCategories] = useState<Set<string>>(new Set(['finance']));
  const [toolSearch, setToolSearch] = useState('');
  const [selectedTools, setSelectedTools] = useState<string[]>([]);
  const [copiedTool, setCopiedTool] = useState<string | null>(null);

  // Team state
  const [teamMembers, setTeamMembers] = useState<AgentConfig[]>([]);
  const [teamMode, setTeamMode] = useState<'coordinate' | 'route' | 'collaborate'>('coordinate');

  const categories = [
    { id: 'TraderInvestorsAgent', name: 'TRADERS', icon: TrendingUp, configFile: 'agent_definitions.json' },
    { id: 'GeopoliticsAgents', name: 'GEOPOLITICS', icon: Globe, configFile: 'agent_definitions.json' },
    { id: 'EconomicAgents', name: 'ECONOMICS', icon: Building2, configFile: 'agent_definitions.json' },
    { id: 'hedgeFundAgents', name: 'HEDGE FUNDS', icon: Users, configFile: 'team_config.json' },
  ];

  const viewModes = [
    { id: 'agents' as ViewMode, name: 'AGENTS', icon: Bot },
    { id: 'teams' as ViewMode, name: 'TEAMS', icon: Users },
    { id: 'workflows' as ViewMode, name: 'WORKFLOWS', icon: GitBranch },
    { id: 'tools' as ViewMode, name: 'TOOLS', icon: Wrench },
    { id: 'chat' as ViewMode, name: 'CHAT', icon: MessageSquare },
    { id: 'system' as ViewMode, name: 'SYSTEM', icon: Cpu },
  ];

  const outputModels = [
    { id: 'trade_signal', name: 'Trade Signal', icon: TrendingUp, description: 'Buy/sell recommendations with confidence' },
    { id: 'stock_analysis', name: 'Stock Analysis', icon: Activity, description: 'Comprehensive stock analysis report' },
    { id: 'portfolio_analysis', name: 'Portfolio Analysis', icon: Database, description: 'Portfolio metrics and allocation' },
    { id: 'risk_assessment', name: 'Risk Assessment', icon: Shield, description: 'Risk scoring and mitigation strategies' },
    { id: 'market_analysis', name: 'Market Analysis', icon: Globe, description: 'Market trends and sentiment' },
    { id: 'research_report', name: 'Research Report', icon: FileText, description: 'Full investment research report' },
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

  // Load system info on mount
  useEffect(() => {
    loadSystemInfo();
    loadToolsInfo();
  }, []);

  const loadSystemInfo = async () => {
    try {
      const result = await invoke<string>('execute_core_agent', {
        command: 'system_info',
        args: [],
      });
      const parsed = JSON.parse(result);
      if (parsed.success) {
        setSystemInfo(parsed);
      }
    } catch (err) {
      console.error('Failed to load system info:', err);
    }
  };

  const loadToolsInfo = async () => {
    try {
      const result = await invoke<string>('execute_core_agent', {
        command: 'list_tools',
        args: [],
      });
      const parsed = JSON.parse(result);
      if (parsed.success) {
        setToolsInfo(parsed);
      }
    } catch (err) {
      console.error('Failed to load tools info:', err);
    }
  };

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

  const runAgent = async (query: string, outputModel?: string) => {
    if (!selectedAgent || !query.trim()) return;

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

      const config = {
        model: {
          provider: selectedAgent.llm_config.provider,
          model_id: selectedAgent.llm_config.model_id,
          temperature: selectedAgent.llm_config.temperature,
          max_tokens: selectedAgent.llm_config.max_tokens,
        },
        instructions: selectedAgent.instructions,
        tools: selectedTools.length > 0 ? selectedTools : selectedAgent.tools,
        memory: { enabled: selectedAgent.enable_memory },
        debug: false,
      };

      let result: string;

      if (outputModel) {
        const queryJson = JSON.stringify({ query, output_model: outputModel });
        result = await invoke<string>('execute_core_agent', {
          command: 'run_structured',
          args: [queryJson, JSON.stringify(config), JSON.stringify(apiKeys)],
        });
      } else {
        const queryJson = JSON.stringify({ query });
        result = await invoke<string>('execute_core_agent', {
          command: 'run',
          args: [queryJson, JSON.stringify(config), JSON.stringify(apiKeys)],
        });
      }

      const parsed = JSON.parse(result);
      setTestResult(parsed);

      // Add to chat history
      setChatMessages(prev => [
        ...prev,
        { role: 'user', content: query, timestamp: new Date() },
        {
          role: 'assistant',
          content: parsed.response || parsed.error || JSON.stringify(parsed, null, 2),
          timestamp: new Date(),
          agentName: selectedAgent.name,
          outputModel
        },
      ]);

      if (parsed.success) {
        setSuccess('Agent executed successfully');
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      console.error('Failed to run agent:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const runTeam = async (query: string) => {
    if (teamMembers.length === 0 || !query.trim()) return;

    setLoading(true);
    setError(null);
    setTestResult(null);

    try {
      const apiKeys = await fetchApiKeysFromSettings();

      const teamConfig = {
        name: 'Custom Team',
        mode: teamMode,
        members: teamMembers.map(agent => ({
          name: agent.name,
          role: agent.role,
          model: agent.llm_config,
          tools: agent.tools,
          instructions: agent.instructions,
        })),
      };

      const queryJson = JSON.stringify({ query });
      const result = await invoke<string>('execute_core_agent', {
        command: 'run_team',
        args: [queryJson, JSON.stringify(teamConfig), JSON.stringify(apiKeys)],
      });

      const parsed = JSON.parse(result);
      setTestResult(parsed);

      setChatMessages(prev => [
        ...prev,
        { role: 'user', content: query, timestamp: new Date() },
        {
          role: 'assistant',
          content: parsed.response || parsed.error || JSON.stringify(parsed, null, 2),
          timestamp: new Date(),
          agentName: `Team (${teamMembers.length} agents)`,
        },
      ]);

      if (parsed.success) {
        setSuccess('Team executed successfully');
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      console.error('Failed to run team:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const runWorkflow = async (workflowType: string, input: any) => {
    setLoading(true);
    setError(null);
    setTestResult(null);

    try {
      const apiKeys = await fetchApiKeysFromSettings();

      let command = '';
      let queryJson = '';

      switch (workflowType) {
        case 'stock_analysis':
          command = 'stock_analysis';
          queryJson = JSON.stringify({ symbol: input.symbol });
          break;
        case 'portfolio_rebal':
          command = 'portfolio_rebal';
          queryJson = JSON.stringify(input);
          break;
        case 'risk_assessment':
          command = 'risk_assessment';
          queryJson = JSON.stringify(input);
          break;
        default:
          throw new Error(`Unknown workflow: ${workflowType}`);
      }

      const result = await invoke<string>('execute_core_agent', {
        command,
        args: [queryJson, JSON.stringify({}), JSON.stringify(apiKeys)],
      });

      const parsed = JSON.parse(result);
      setTestResult(parsed);

      if (parsed.success) {
        setSuccess(`${workflowType} workflow completed`);
        setTimeout(() => setSuccess(null), 3000);
      }
    } catch (err: any) {
      console.error('Failed to run workflow:', err);
      setError(err.toString());
    } finally {
      setLoading(false);
    }
  };

  const addToTeam = (agent: AgentConfig) => {
    if (!teamMembers.find(m => m.id === agent.id)) {
      setTeamMembers(prev => [...prev, agent]);
    }
  };

  const removeFromTeam = (agentId: string) => {
    setTeamMembers(prev => prev.filter(m => m.id !== agentId));
  };

  const toggleCategory = (category: string) => {
    setExpandedCategories(prev => {
      const newSet = new Set(prev);
      if (newSet.has(category)) {
        newSet.delete(category);
      } else {
        newSet.add(category);
      }
      return newSet;
    });
  };

  const toggleTool = (tool: string) => {
    setSelectedTools(prev => {
      if (prev.includes(tool)) {
        return prev.filter(t => t !== tool);
      }
      return [...prev, tool];
    });
  };

  const copyTool = (tool: string) => {
    navigator.clipboard.writeText(tool);
    setCopiedTool(tool);
    setTimeout(() => setCopiedTool(null), 2000);
  };

  const handleChatSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!chatInput.trim()) return;

    if (viewMode === 'teams' && teamMembers.length > 0) {
      runTeam(chatInput);
    } else if (selectedAgent) {
      runAgent(chatInput, selectedOutputModel || undefined);
    }
    setChatInput('');
  };

  const filteredTools = toolsInfo ? Object.entries(toolsInfo.tools).reduce((acc, [category, tools]) => {
    const filtered = tools.filter(tool =>
      tool.toLowerCase().includes(toolSearch.toLowerCase()) ||
      category.toLowerCase().includes(toolSearch.toLowerCase())
    );
    if (filtered.length > 0) {
      acc[category] = filtered;
    }
    return acc;
  }, {} as Record<string, string[]>) : {};

  const CategoryIcon = categories.find(c => c.id === selectedCategory)?.icon || Bot;

  // Render different views
  const renderAgentsView = () => (
    <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
      {/* Left Panel - Agent List */}
      <div style={{
        width: '280px',
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: FINCEPT.PANEL_BG
      }}>
        {/* Category Tabs */}
        <div style={{
          padding: '8px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexWrap: 'wrap',
          gap: '4px'
        }}>
          {categories.map((cat) => {
            const Icon = cat.icon;
            return (
              <button
                key={cat.id}
                onClick={() => setSelectedCategory(cat.id)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: selectedCategory === cat.id ? FINCEPT.ORANGE : 'transparent',
                  border: `1px solid ${selectedCategory === cat.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  color: selectedCategory === cat.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontSize: '8px',
                  fontWeight: 600,
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

        <div style={{
          padding: '8px 12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.HEADER_BG,
          fontSize: '11px',
          fontWeight: 600,
          color: FINCEPT.GRAY
        }}>
          AVAILABLE AGENTS ({agents.length})
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
          {loading && agents.length === 0 ? (
            <div style={{ textAlign: 'center', padding: '20px', color: FINCEPT.GRAY }}>
              <RefreshCw size={20} className="animate-spin" style={{ margin: '0 auto' }} />
              <div style={{ marginTop: '8px', fontSize: '10px' }}>Loading agents...</div>
            </div>
          ) : agents.length === 0 ? (
            <div style={{ textAlign: 'center', padding: '20px', color: FINCEPT.GRAY }}>
              <AlertCircle size={20} style={{ margin: '0 auto' }} />
              <div style={{ marginTop: '8px', fontSize: '10px' }}>No agents found</div>
            </div>
          ) : (
            agents.map((agent) => (
              <div
                key={agent.id}
                style={{
                  marginBottom: '6px',
                  border: `1px solid ${selectedAgent?.id === agent.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  backgroundColor: selectedAgent?.id === agent.id ? FINCEPT.HOVER : 'transparent',
                }}
              >
                <button
                  onClick={() => setSelectedAgent(agent)}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    color: FINCEPT.WHITE,
                    cursor: 'pointer',
                    textAlign: 'left',
                    fontSize: '10px',
                  }}
                >
                  <div style={{ fontWeight: 600, color: FINCEPT.CYAN, marginBottom: '4px' }}>
                    {agent.name}
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '4px', lineHeight: 1.3 }}>
                    {agent.role?.substring(0, 60)}{agent.role?.length > 60 ? '...' : ''}
                  </div>
                  <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                    <span style={{
                      padding: '2px 6px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      fontSize: '8px',
                      color: FINCEPT.YELLOW
                    }}>
                      {agent.llm_config?.provider || 'N/A'}
                    </span>
                  </div>
                </button>
                <div style={{ padding: '0 10px 8px', display: 'flex', gap: '4px' }}>
                  <button
                    onClick={() => addToTeam(agent)}
                    disabled={teamMembers.some(m => m.id === agent.id)}
                    style={{
                      padding: '2px 6px',
                      backgroundColor: teamMembers.some(m => m.id === agent.id) ? FINCEPT.GREEN + '40' : FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      color: teamMembers.some(m => m.id === agent.id) ? FINCEPT.GREEN : FINCEPT.GRAY,
                      cursor: teamMembers.some(m => m.id === agent.id) ? 'default' : 'pointer',
                      fontSize: '8px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '2px',
                    }}
                  >
                    <Plus size={8} />
                    {teamMembers.some(m => m.id === agent.id) ? 'In Team' : 'Add to Team'}
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Right Panel - Agent Details */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {selectedAgent ? (
          <>
            {/* Agent Header */}
            <div style={{
              padding: '12px',
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              backgroundColor: FINCEPT.HEADER_BG
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'start', marginBottom: '8px' }}>
                <div>
                  <div style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '4px' }}>
                    {selectedAgent.name}
                  </div>
                  <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: 1.4 }}>
                    {selectedAgent.description}
                  </div>
                </div>
                <button
                  onClick={() => setShowJsonEditor(!showJsonEditor)}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: showJsonEditor ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: showJsonEditor ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                    cursor: 'pointer',
                    fontSize: '9px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px'
                  }}
                >
                  <Code size={10} />
                  JSON
                </button>
              </div>

              {/* Output Model Selector */}
              <div style={{ marginBottom: '8px' }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>STRUCTURED OUTPUT (Optional)</div>
                <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                  <button
                    onClick={() => setSelectedOutputModel('')}
                    style={{
                      padding: '3px 8px',
                      backgroundColor: !selectedOutputModel ? FINCEPT.BLUE : FINCEPT.PANEL_BG,
                      border: `1px solid ${!selectedOutputModel ? FINCEPT.BLUE : FINCEPT.BORDER}`,
                      color: !selectedOutputModel ? FINCEPT.WHITE : FINCEPT.GRAY,
                      cursor: 'pointer',
                      fontSize: '8px',
                    }}
                  >
                    None
                  </button>
                  {outputModels.map(model => (
                    <button
                      key={model.id}
                      onClick={() => setSelectedOutputModel(model.id)}
                      title={model.description}
                      style={{
                        padding: '3px 8px',
                        backgroundColor: selectedOutputModel === model.id ? FINCEPT.PURPLE : FINCEPT.PANEL_BG,
                        border: `1px solid ${selectedOutputModel === model.id ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                        color: selectedOutputModel === model.id ? FINCEPT.WHITE : FINCEPT.GRAY,
                        cursor: 'pointer',
                        fontSize: '8px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '3px',
                      }}
                    >
                      {model.name}
                    </button>
                  ))}
                </div>
              </div>

              {/* Test Query Input */}
              <div style={{ display: 'flex', gap: '8px' }}>
                <input
                  type="text"
                  value={testQuery}
                  onChange={(e) => setTestQuery(e.target.value)}
                  placeholder="Enter test query..."
                  style={{
                    flex: 1,
                    padding: '6px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '10px',
                    outline: 'none',
                  }}
                />
                <button
                  onClick={() => runAgent(testQuery, selectedOutputModel || undefined)}
                  disabled={loading || !testQuery.trim()}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: FINCEPT.BLUE,
                    border: 'none',
                    color: FINCEPT.WHITE,
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
                  {loading ? 'Running...' : 'Run'}
                </button>
              </div>
            </div>

            {/* Content Area */}
            <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
              {showJsonEditor ? (
                <div style={{ height: '100%' }}>
                  <textarea
                    value={jsonText}
                    onChange={(e) => setJsonText(e.target.value)}
                    style={{
                      width: '100%',
                      height: '100%',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      color: FINCEPT.CYAN,
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
                  {/* LLM Config */}
                  <div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GRAY, marginBottom: '6px' }}>LLM CONFIGURATION</div>
                    <div style={{
                      padding: '8px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      display: 'grid',
                      gridTemplateColumns: 'repeat(4, 1fr)',
                      gap: '8px',
                      fontSize: '10px'
                    }}>
                      <div><span style={{ color: FINCEPT.GRAY }}>Provider:</span> <span style={{ color: FINCEPT.CYAN }}>{selectedAgent.llm_config?.provider}</span></div>
                      <div><span style={{ color: FINCEPT.GRAY }}>Model:</span> <span style={{ color: FINCEPT.PURPLE }}>{selectedAgent.llm_config?.model_id}</span></div>
                      <div><span style={{ color: FINCEPT.GRAY }}>Temp:</span> <span style={{ color: FINCEPT.YELLOW }}>{selectedAgent.llm_config?.temperature}</span></div>
                      <div><span style={{ color: FINCEPT.GRAY }}>Max Tokens:</span> <span style={{ color: FINCEPT.YELLOW }}>{selectedAgent.llm_config?.max_tokens}</span></div>
                    </div>
                  </div>

                  {/* Tools */}
                  <div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GRAY, marginBottom: '6px' }}>
                      TOOLS ({selectedAgent.tools?.length || 0})
                      {selectedTools.length > 0 && <span style={{ color: FINCEPT.GREEN }}> + {selectedTools.length} custom</span>}
                    </div>
                    <div style={{
                      padding: '8px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      display: 'flex',
                      gap: '6px',
                      flexWrap: 'wrap'
                    }}>
                      {(selectedAgent.tools || []).map((tool, idx) => (
                        <span key={idx} style={{
                          padding: '4px 8px',
                          backgroundColor: FINCEPT.DARK_BG,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          color: FINCEPT.GREEN,
                          fontSize: '9px'
                        }}>
                          {tool}
                        </span>
                      ))}
                      {selectedTools.filter(t => !selectedAgent.tools?.includes(t)).map((tool, idx) => (
                        <span key={`custom-${idx}`} style={{
                          padding: '4px 8px',
                          backgroundColor: FINCEPT.PURPLE + '30',
                          border: `1px solid ${FINCEPT.PURPLE}`,
                          color: FINCEPT.PURPLE,
                          fontSize: '9px',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                        }}>
                          {tool}
                          <button
                            onClick={() => toggleTool(tool)}
                            style={{ background: 'none', border: 'none', color: FINCEPT.PURPLE, cursor: 'pointer', padding: 0 }}
                          >
                            <Trash2 size={8} />
                          </button>
                        </span>
                      ))}
                    </div>
                  </div>

                  {/* Instructions */}
                  <div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GRAY, marginBottom: '6px' }}>INSTRUCTIONS</div>
                    <div style={{
                      padding: '8px',
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      fontSize: '10px',
                      maxHeight: '120px',
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
                      <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GRAY, marginBottom: '6px' }}>
                        RESULT {selectedOutputModel && <span style={{ color: FINCEPT.PURPLE }}>({selectedOutputModel})</span>}
                      </div>
                      <div style={{
                        padding: '8px',
                        backgroundColor: FINCEPT.DARK_BG,
                        border: `1px solid ${testResult.success ? FINCEPT.GREEN : FINCEPT.RED}`,
                        fontSize: '10px',
                        maxHeight: '200px',
                        overflow: 'auto',
                        fontFamily: 'monospace'
                      }}>
                        <pre style={{ margin: 0, color: testResult.success ? FINCEPT.GREEN : FINCEPT.RED, whiteSpace: 'pre-wrap' }}>
                          {testResult.response || testResult.error || JSON.stringify(testResult, null, 2)}
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
            color: FINCEPT.GRAY,
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
  );

  const renderTeamsView = () => (
    <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
      {/* Left - Team Builder */}
      <div style={{
        width: '300px',
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: FINCEPT.PANEL_BG
      }}>
        <div style={{
          padding: '12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.HEADER_BG,
        }}>
          <div style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.CYAN, marginBottom: '8px' }}>
            TEAM MEMBERS ({teamMembers.length})
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
            Add agents from the Agents view to build a team
          </div>
          <div style={{ display: 'flex', gap: '4px' }}>
            {(['coordinate', 'route', 'collaborate'] as const).map(mode => (
              <button
                key={mode}
                onClick={() => setTeamMode(mode)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: teamMode === mode ? FINCEPT.PURPLE : FINCEPT.DARK_BG,
                  border: `1px solid ${teamMode === mode ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                  color: teamMode === mode ? FINCEPT.WHITE : FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontSize: '8px',
                  textTransform: 'uppercase',
                }}
              >
                {mode}
              </button>
            ))}
          </div>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
          {teamMembers.length === 0 ? (
            <div style={{ textAlign: 'center', padding: '20px', color: FINCEPT.GRAY }}>
              <Users size={24} style={{ margin: '0 auto 8px', opacity: 0.3 }} />
              <div style={{ fontSize: '10px' }}>No team members yet</div>
              <div style={{ fontSize: '9px', marginTop: '4px' }}>Go to Agents view to add members</div>
            </div>
          ) : (
            teamMembers.map((member, idx) => (
              <div
                key={member.id}
                style={{
                  padding: '10px',
                  marginBottom: '6px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'start' }}>
                  <div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.CYAN }}>
                      {idx + 1}. {member.name}
                    </div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginTop: '2px' }}>
                      {member.role?.substring(0, 50)}...
                    </div>
                  </div>
                  <button
                    onClick={() => removeFromTeam(member.id)}
                    style={{
                      background: 'none',
                      border: 'none',
                      color: FINCEPT.RED,
                      cursor: 'pointer',
                      padding: '2px',
                    }}
                  >
                    <Trash2 size={12} />
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Right - Team Execution */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
        <div style={{
          padding: '12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.HEADER_BG,
        }}>
          <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '8px' }}>
            Run Multi-Agent Team
          </div>
          <form onSubmit={(e) => { e.preventDefault(); runTeam(testQuery); }} style={{ display: 'flex', gap: '8px' }}>
            <input
              type="text"
              value={testQuery}
              onChange={(e) => setTestQuery(e.target.value)}
              placeholder="Enter query for the team..."
              style={{
                flex: 1,
                padding: '8px 12px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
                fontSize: '11px',
                outline: 'none',
              }}
            />
            <button
              type="submit"
              disabled={loading || teamMembers.length === 0 || !testQuery.trim()}
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.PURPLE,
                border: 'none',
                color: FINCEPT.WHITE,
                cursor: loading || teamMembers.length === 0 ? 'not-allowed' : 'pointer',
                fontSize: '11px',
                fontWeight: 600,
                opacity: loading || teamMembers.length === 0 ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              <Users size={14} />
              {loading ? 'Running...' : 'Run Team'}
            </button>
          </form>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          {testResult && (
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${testResult.success ? FINCEPT.GREEN : FINCEPT.RED}`,
            }}>
              <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GRAY, marginBottom: '8px' }}>
                TEAM RESULT
              </div>
              <pre style={{
                margin: 0,
                fontSize: '11px',
                color: testResult.success ? FINCEPT.GREEN : FINCEPT.RED,
                whiteSpace: 'pre-wrap',
                fontFamily: 'monospace',
              }}>
                {testResult.response || testResult.error || JSON.stringify(testResult, null, 2)}
              </pre>
            </div>
          )}
        </div>
      </div>
    </div>
  );

  const renderWorkflowsView = () => (
    <div style={{ flex: 1, padding: '16px', overflow: 'auto' }}>
      <div style={{ fontSize: '14px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '16px' }}>
        Financial Workflows
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
        {[
          { id: 'stock_analysis', name: 'Stock Analysis', icon: TrendingUp, description: 'Comprehensive stock analysis pipeline', color: FINCEPT.GREEN },
          { id: 'portfolio_rebal', name: 'Portfolio Rebalancing', icon: Database, description: 'Optimize portfolio allocation', color: FINCEPT.BLUE },
          { id: 'risk_assessment', name: 'Risk Assessment', icon: Shield, description: 'Evaluate portfolio risks', color: FINCEPT.RED },
        ].map(workflow => {
          const Icon = workflow.icon;
          return (
            <div
              key={workflow.id}
              style={{
                padding: '16px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '8px' }}>
                <Icon size={20} color={workflow.color} />
                <span style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.WHITE }}>{workflow.name}</span>
              </div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '12px' }}>
                {workflow.description}
              </div>
              {workflow.id === 'stock_analysis' && (
                <div style={{ display: 'flex', gap: '8px' }}>
                  <input
                    type="text"
                    placeholder="Symbol (e.g., AAPL)"
                    style={{
                      flex: 1,
                      padding: '6px 10px',
                      backgroundColor: FINCEPT.DARK_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      color: FINCEPT.WHITE,
                      fontSize: '10px',
                      outline: 'none',
                    }}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') {
                        runWorkflow('stock_analysis', { symbol: (e.target as HTMLInputElement).value });
                      }
                    }}
                  />
                  <button
                    onClick={() => {
                      const input = document.querySelector(`input[placeholder="Symbol (e.g., AAPL)"]`) as HTMLInputElement;
                      if (input?.value) runWorkflow('stock_analysis', { symbol: input.value });
                    }}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: workflow.color,
                      border: 'none',
                      color: FINCEPT.WHITE,
                      cursor: 'pointer',
                      fontSize: '10px',
                    }}
                  >
                    Run
                  </button>
                </div>
              )}
            </div>
          );
        })}
      </div>

      {testResult && (
        <div style={{ marginTop: '16px' }}>
          <div style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.GRAY, marginBottom: '8px' }}>
            WORKFLOW RESULT
          </div>
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${testResult.success ? FINCEPT.GREEN : FINCEPT.RED}`,
            maxHeight: '300px',
            overflow: 'auto',
          }}>
            <pre style={{
              margin: 0,
              fontSize: '11px',
              color: testResult.success ? FINCEPT.GREEN : FINCEPT.RED,
              whiteSpace: 'pre-wrap',
            }}>
              {JSON.stringify(testResult, null, 2)}
            </pre>
          </div>
        </div>
      )}
    </div>
  );

  const renderToolsView = () => (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Search */}
      <div style={{
        padding: '12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <Search size={14} color={FINCEPT.GRAY} />
          <input
            type="text"
            value={toolSearch}
            onChange={(e) => setToolSearch(e.target.value)}
            placeholder="Search tools..."
            style={{
              flex: 1,
              padding: '6px 10px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              fontSize: '11px',
              outline: 'none',
            }}
          />
          <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
            {toolsInfo?.total_count || 0} tools available
          </span>
        </div>
        {selectedTools.length > 0 && (
          <div style={{ marginTop: '8px', display: 'flex', gap: '4px', flexWrap: 'wrap', alignItems: 'center' }}>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>Selected:</span>
            {selectedTools.map(tool => (
              <span
                key={tool}
                style={{
                  padding: '2px 6px',
                  backgroundColor: FINCEPT.PURPLE + '30',
                  border: `1px solid ${FINCEPT.PURPLE}`,
                  color: FINCEPT.PURPLE,
                  fontSize: '8px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                }}
              >
                {tool}
                <button
                  onClick={() => toggleTool(tool)}
                  style={{ background: 'none', border: 'none', color: FINCEPT.PURPLE, cursor: 'pointer', padding: 0 }}
                >
                  x
                </button>
              </span>
            ))}
            <button
              onClick={() => setSelectedTools([])}
              style={{
                padding: '2px 6px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
                cursor: 'pointer',
                fontSize: '8px',
              }}
            >
              Clear All
            </button>
          </div>
        )}
      </div>

      {/* Tools List */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {Object.entries(filteredTools).map(([category, tools]) => (
          <div key={category} style={{ marginBottom: '8px' }}>
            <button
              onClick={() => toggleCategory(category)}
              style={{
                width: '100%',
                padding: '8px 12px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.CYAN,
                cursor: 'pointer',
                fontSize: '11px',
                fontWeight: 600,
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                textAlign: 'left',
              }}
            >
              {expandedCategories.has(category) ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
              {category.toUpperCase()}
              <span style={{ color: FINCEPT.GRAY, fontWeight: 400 }}>({tools.length})</span>
            </button>
            {expandedCategories.has(category) && (
              <div style={{
                padding: '8px',
                backgroundColor: FINCEPT.DARK_BG,
                borderLeft: `1px solid ${FINCEPT.BORDER}`,
                borderRight: `1px solid ${FINCEPT.BORDER}`,
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                display: 'flex',
                gap: '6px',
                flexWrap: 'wrap',
              }}>
                {tools.map(tool => (
                  <button
                    key={tool}
                    onClick={() => toggleTool(tool)}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: selectedTools.includes(tool) ? FINCEPT.PURPLE + '40' : FINCEPT.PANEL_BG,
                      border: `1px solid ${selectedTools.includes(tool) ? FINCEPT.PURPLE : FINCEPT.BORDER}`,
                      color: selectedTools.includes(tool) ? FINCEPT.PURPLE : FINCEPT.GREEN,
                      cursor: 'pointer',
                      fontSize: '9px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px',
                    }}
                  >
                    {selectedTools.includes(tool) && <Check size={10} />}
                    {tool}
                    <button
                      onClick={(e) => { e.stopPropagation(); copyTool(tool); }}
                      style={{ background: 'none', border: 'none', color: FINCEPT.GRAY, cursor: 'pointer', padding: 0 }}
                    >
                      {copiedTool === tool ? <Check size={10} color={FINCEPT.GREEN} /> : <Copy size={10} />}
                    </button>
                  </button>
                ))}
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );

  const renderChatView = () => (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Chat Messages */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {chatMessages.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '40px', color: FINCEPT.GRAY }}>
            <MessageSquare size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
            <div style={{ fontSize: '11px' }}>No messages yet</div>
            <div style={{ fontSize: '10px', marginTop: '4px' }}>Select an agent and start chatting</div>
          </div>
        ) : (
          chatMessages.map((msg, idx) => (
            <div
              key={idx}
              style={{
                marginBottom: '12px',
                padding: '10px',
                backgroundColor: msg.role === 'user' ? FINCEPT.BLUE + '20' : FINCEPT.PANEL_BG,
                borderLeft: `3px solid ${msg.role === 'user' ? FINCEPT.BLUE : FINCEPT.GREEN}`,
              }}
            >
              <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>
                {msg.role === 'user' ? 'You' : msg.agentName || 'Agent'}
                {msg.outputModel && <span style={{ color: FINCEPT.PURPLE }}> ({msg.outputModel})</span>}
                <span style={{ marginLeft: '8px' }}>{msg.timestamp.toLocaleTimeString()}</span>
              </div>
              <div style={{ fontSize: '11px', color: FINCEPT.WHITE, whiteSpace: 'pre-wrap' }}>
                {msg.content}
              </div>
            </div>
          ))
        )}
      </div>

      {/* Chat Input */}
      <div style={{
        padding: '12px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
          Agent: {selectedAgent?.name || 'None selected'} | Team: {teamMembers.length} members
        </div>
        <form onSubmit={handleChatSubmit} style={{ display: 'flex', gap: '8px' }}>
          <input
            type="text"
            value={chatInput}
            onChange={(e) => setChatInput(e.target.value)}
            placeholder="Type a message..."
            style={{
              flex: 1,
              padding: '8px 12px',
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              fontSize: '11px',
              outline: 'none',
            }}
          />
          <button
            type="submit"
            disabled={loading || (!selectedAgent && teamMembers.length === 0)}
            style={{
              padding: '8px 16px',
              backgroundColor: FINCEPT.BLUE,
              border: 'none',
              color: FINCEPT.WHITE,
              cursor: loading ? 'not-allowed' : 'pointer',
              fontSize: '11px',
              fontWeight: 600,
              opacity: loading ? 0.5 : 1,
            }}
          >
            {loading ? 'Sending...' : 'Send'}
          </button>
        </form>
      </div>
    </div>
  );

  const renderSystemView = () => (
    <div style={{ flex: 1, padding: '16px', overflow: 'auto' }}>
      {systemInfo ? (
        <div style={{ display: 'grid', gap: '16px' }}>
          {/* Header */}
          <div style={{
            padding: '16px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.ORANGE}`,
          }}>
            <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              FinAgent Core v{systemInfo.version}
            </div>
            <div style={{ fontSize: '11px', color: FINCEPT.GRAY }}>
              Powered by {systemInfo.framework} framework
            </div>
          </div>

          {/* Capabilities */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px' }}>
            <div style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, textAlign: 'center' }}>
              <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.CYAN }}>{systemInfo.capabilities.model_providers}</div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Model Providers</div>
            </div>
            <div style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, textAlign: 'center' }}>
              <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.GREEN }}>{systemInfo.capabilities.tools}</div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Tools Available</div>
            </div>
            <div style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, textAlign: 'center' }}>
              <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.PURPLE }}>{systemInfo.capabilities.vectordbs.length}</div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Vector DBs</div>
            </div>
            <div style={{ padding: '12px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, textAlign: 'center' }}>
              <div style={{ fontSize: '24px', fontWeight: 700, color: FINCEPT.YELLOW }}>{systemInfo.capabilities.embedders.length}</div>
              <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>Embedders</div>
            </div>
          </div>

          {/* Features */}
          <div>
            <div style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '8px' }}>Features</div>
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              {systemInfo.features.map(feature => (
                <span
                  key={feature}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.GREEN,
                    fontSize: '9px',
                  }}
                >
                  {feature.replace(/_/g, ' ')}
                </span>
              ))}
            </div>
          </div>

          {/* Output Models */}
          <div>
            <div style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '8px' }}>Structured Output Models</div>
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              {systemInfo.capabilities.output_models.map(model => (
                <span
                  key={model}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: FINCEPT.PURPLE + '30',
                    border: `1px solid ${FINCEPT.PURPLE}`,
                    color: FINCEPT.PURPLE,
                    fontSize: '9px',
                  }}
                >
                  {model.replace(/_/g, ' ')}
                </span>
              ))}
            </div>
          </div>

          {/* Tool Categories */}
          <div>
            <div style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.WHITE, marginBottom: '8px' }}>Tool Categories</div>
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              {systemInfo.capabilities.tool_categories.map(cat => (
                <span
                  key={cat}
                  style={{
                    padding: '4px 8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.CYAN,
                    fontSize: '9px',
                  }}
                >
                  {cat}
                </span>
              ))}
            </div>
          </div>
        </div>
      ) : (
        <div style={{ textAlign: 'center', padding: '40px', color: FINCEPT.GRAY }}>
          <RefreshCw size={24} className="animate-spin" style={{ margin: '0 auto 12px' }} />
          <div>Loading system info...</div>
        </div>
      )}
    </div>
  );

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
        @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
        .animate-spin { animation: spin 1s linear infinite; }
      `}</style>

      {/* TOP NAVIGATION BAR */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Bot size={18} color={FINCEPT.ORANGE} />
            <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '14px', letterSpacing: '0.5px' }}>
              AGENT STUDIO
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

          {/* View Mode Selector */}
          <div style={{ display: 'flex', gap: '4px' }}>
            {viewModes.map((mode) => {
              const Icon = mode.icon;
              return (
                <button
                  key={mode.id}
                  onClick={() => setViewMode(mode.id)}
                  style={{
                    padding: '4px 10px',
                    backgroundColor: viewMode === mode.id ? FINCEPT.ORANGE : FINCEPT.PANEL_BG,
                    border: `1px solid ${viewMode === mode.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                    color: viewMode === mode.id ? FINCEPT.DARK_BG : FINCEPT.CYAN,
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
                  {mode.name}
                </button>
              );
            })}
          </div>
        </div>

        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <button
            onClick={() => { loadAgents(); loadSystemInfo(); loadToolsInfo(); }}
            disabled={loading}
            style={{
              padding: '4px 10px',
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.CYAN,
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

      {/* Status Messages */}
      {(error || success) && (
        <div style={{
          padding: '8px 12px',
          backgroundColor: error ? FINCEPT.RED + '20' : FINCEPT.GREEN + '20',
          borderBottom: `1px solid ${error ? FINCEPT.RED : FINCEPT.GREEN}`,
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          fontSize: '10px'
        }}>
          {error ? <AlertCircle size={14} color={FINCEPT.RED} /> : <CheckCircle size={14} color={FINCEPT.GREEN} />}
          <span style={{ color: error ? FINCEPT.RED : FINCEPT.GREEN, flex: 1 }}>
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

      {/* MAIN CONTENT */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {viewMode === 'agents' && renderAgentsView()}
        {viewMode === 'teams' && renderTeamsView()}
        {viewMode === 'workflows' && renderWorkflowsView()}
        {viewMode === 'tools' && renderToolsView()}
        {viewMode === 'chat' && renderChatView()}
        {viewMode === 'system' && renderSystemView()}
      </div>

      {/* Footer */}
      <TabFooter
        tabName="AGENT STUDIO"
        leftInfo={[
          { label: `${agents.length} agents`, color: FINCEPT.CYAN },
          { label: `${teamMembers.length} in team`, color: FINCEPT.PURPLE },
          { label: `${selectedTools.length} tools`, color: FINCEPT.GREEN },
        ]}
        statusInfo={systemInfo ? `v${systemInfo.version} | ${systemInfo.capabilities.tools} tools | ${systemInfo.capabilities.model_providers} providers` : 'Loading...'}
      />
    </div>
  );
};

export default AgentConfigTab;
