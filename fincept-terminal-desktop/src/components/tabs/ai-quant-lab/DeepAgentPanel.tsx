/**
 * DeepAgent Panel - Hierarchical Agentic Automation
 * Autonomous multi-step financial workflows with task delegation
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Brain,
  Sparkles,
  Play,
  CheckCircle2,
  Clock,
  AlertCircle,
  ChevronRight,
  ChevronDown,
  GitBranch,
  Zap,
  FileText,
  Loader2,
  Cpu,
  Network
} from 'lucide-react';
import { deepAgentService, ExecuteTaskRequest, Todo, PlanTodo, AgentConfig } from '@/services/aiQuantLab/deepAgentService';
import { useAuth } from '@/contexts/AuthContext';
import { getActiveLLMConfig } from '@/services/core/sqliteService';
import { useTerminalTheme } from '@/contexts/ThemeContext';

interface AgentType {
  id: string;
  name: string;
  description: string;
  icon: React.ComponentType<any>;
  color: string;
  subagents: string[];
}

// Agent types are defined inside the component to use theme colors
// See getAgentTypes() function inside DeepAgentPanel

const EXAMPLE_TASKS = {
  research: [
    'Analyze the semiconductor sector: major players, trends, and investment outlook',
    'Research AI chip manufacturers and their competitive positioning',
    'Compare cloud computing providers: AWS, Azure, GCP - market share and growth'
  ],
  trading_strategy: [
    'Design a momentum strategy for S&P 500 stocks with monthly rebalancing',
    'Create mean reversion strategy for tech sector with risk controls',
    'Build factor-based strategy combining value and quality signals'
  ],
  portfolio_management: [
    'Optimize portfolio: 40% equities, 30% bonds, 20% alternatives, 10% cash',
    'Rebalance portfolio to target risk level while minimizing transaction costs',
    'Construct sector-neutral portfolio with factor tilts'
  ],
  risk_assessment: [
    'Assess tail risk for current portfolio under various market stress scenarios',
    'Calculate VaR and CVaR for multi-asset portfolio',
    'Analyze factor exposures and concentration risks'
  ],
  general: [
    'Full analysis: Research FAANG stocks, build portfolio, assess risks, generate report',
    'Comprehensive workflow: Design strategy → Backtest → Optimize → Risk assess → Report'
  ]
};

/** Parse simple markdown: **bold**, *italic*, ## headings, --- hr, - bullets */
export function formatResultText(text: string): React.ReactNode[] {
  const lines = text.split('\n');
  const nodes: React.ReactNode[] = [];

  lines.forEach((line, i) => {
    const trimmed = line.trim();

    // Horizontal rule
    if (/^-{3,}$/.test(trimmed) || /^\*{3,}$/.test(trimmed)) {
      nodes.push(<hr key={i} style={{ border: 'none', borderTop: '1px solid var(--ft-border-color, #2A2A2A)', margin: '12px 0' }} />);
      return;
    }

    // Headings
    const headingMatch = trimmed.match(/^(#{1,3})\s+(.+)/);
    if (headingMatch) {
      const level = headingMatch[1].length;
      const sizes = { 1: '16px', 2: '14px', 3: '13px' } as Record<number, string>;
      nodes.push(
        <div key={i} style={{ fontSize: sizes[level] || '13px', fontWeight: 700, color: 'var(--ft-color-primary, #FF8800)', margin: '14px 0 6px 0' }}>
          {renderInlineFormatting(headingMatch[2])}
        </div>
      );
      return;
    }

    // Bullet points
    if (/^[-*]\s+/.test(trimmed)) {
      nodes.push(
        <div key={i} style={{ paddingLeft: '16px', position: 'relative', marginBottom: '2px' }}>
          <span style={{ position: 'absolute', left: '4px', color: 'var(--ft-color-primary, #FF8800)' }}>•</span>
          {renderInlineFormatting(trimmed.replace(/^[-*]\s+/, ''))}
        </div>
      );
      return;
    }

    // Regular line with inline formatting
    nodes.push(
      <div key={i} style={{ minHeight: trimmed === '' ? '8px' : undefined }}>
        {renderInlineFormatting(line)}
      </div>
    );
  });

  return nodes;
}

/** Handle **bold** and *italic* inline */
function renderInlineFormatting(text: string): React.ReactNode {
  const parts: React.ReactNode[] = [];
  let remaining = text;
  let key = 0;

  while (remaining.length > 0) {
    // Bold: **text**
    const boldMatch = remaining.match(/\*\*(.+?)\*\*/);
    if (boldMatch && boldMatch.index !== undefined) {
      if (boldMatch.index > 0) {
        parts.push(remaining.slice(0, boldMatch.index));
      }
      parts.push(<span key={key++} style={{ fontWeight: 700, color: 'var(--ft-color-text, #FFFFFF)' }}>{boldMatch[1]}</span>);
      remaining = remaining.slice(boldMatch.index + boldMatch[0].length);
      continue;
    }
    // No more matches
    parts.push(remaining);
    break;
  }

  return parts.length === 1 ? parts[0] : <>{parts}</>;
}

export interface DeepAgentOutput {
  result: string;
  error: string;
  executionLog: string[];
  isExecuting: boolean;
}

interface DeepAgentPanelProps {
  onOutputChange?: (output: DeepAgentOutput) => void;
}

export function DeepAgentPanel({ onOutputChange }: DeepAgentPanelProps) {
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const { session } = useAuth();
  const [selectedAgent, setSelectedAgent] = useState<string>('research');

  // Agent types using theme colors
  const AGENT_TYPES: AgentType[] = [
    {
      id: 'research',
      name: 'Research Agent',
      description: 'Autonomous research, data analysis, and report generation',
      icon: FileText,
      color: colors.accent,
      subagents: ['data_analyst', 'reporter']
    },
    {
      id: 'trading_strategy',
      name: 'Strategy Builder',
      description: 'Design, backtest, and validate trading strategies',
      icon: Zap,
      color: colors.warning,
      subagents: ['data_analyst', 'trading', 'backtester', 'risk_analyzer', 'reporter']
    },
    {
      id: 'portfolio_management',
      name: 'Portfolio Manager',
      description: 'Optimize portfolios with risk management',
      icon: GitBranch,
      color: colors.success,
      subagents: ['data_analyst', 'portfolio_optimizer', 'risk_analyzer', 'reporter']
    },
    {
      id: 'risk_assessment',
      name: 'Risk Analyzer',
      description: 'Comprehensive risk assessment and stress testing',
      icon: AlertCircle,
      color: colors.alert,
      subagents: ['data_analyst', 'risk_analyzer', 'reporter']
    },
    {
      id: 'general',
      name: 'General Agent',
      description: 'Full-featured agent with all specialists',
      icon: Brain,
      color: colors.purple,
      subagents: ['research', 'data_analyst', 'trading', 'risk_analyzer', 'portfolio_optimizer', 'backtester', 'reporter']
    }
  ];
  const [task, setTask] = useState<string>('');
  const [isExecuting, setIsExecuting] = useState(false);
  const [result, setResult] = useState<string>('');
  const [todos, setTodos] = useState<Todo[]>([]);
  const [threadId, setThreadId] = useState<string>('');
  const [error, setError] = useState<string>('');
  const [expandedTodos, setExpandedTodos] = useState<Set<string>>(new Set());
  const [status, setStatus] = useState({ available: false });
  const [executionLog, setExecutionLog] = useState<string[]>([]);

  const resultRef = useRef<HTMLDivElement>(null);

  // Notify parent of output changes
  useEffect(() => {
    onOutputChange?.({ result, error, executionLog, isExecuting });
  }, [result, error, executionLog, isExecuting, onOutputChange]);

  const addLog = (message: string) => {
    const timestamp = new Date().toLocaleTimeString();
    setExecutionLog(prev => [...prev, `[${timestamp}] ${message}`]);
  };

  const handleExecute = async () => {
    if (!task.trim()) return;

    setIsExecuting(true);
    setError('');
    setResult('');
    setTodos([]);
    setExecutionLog([]);

    const newThreadId = `thread-${Date.now()}`;
    setThreadId(newThreadId);

    // Fetch active LLM config from SQLite settings
    let llmProvider = 'fincept';
    let llmApiKey = session?.api_key || undefined;
    let llmBaseUrl: string | undefined;
    let llmModel: string | undefined;
    try {
      const activeLLM = await getActiveLLMConfig();
      if (activeLLM) {
        llmProvider = activeLLM.provider;
        llmApiKey = activeLLM.api_key || llmApiKey;
        llmBaseUrl = activeLLM.base_url;
        llmModel = activeLLM.model;
      }
    } catch (e) {
      console.warn('[DeepAgent] Could not fetch LLM config, defaulting to fincept:', e);
    }

    const agentConfig: AgentConfig = {
      api_key: session?.api_key || undefined,
      llm_provider: llmProvider,
      llm_api_key: llmApiKey,
      llm_base_url: llmBaseUrl,
      llm_model: llmModel,
    };

    addLog(`Starting ${selectedAgentInfo?.name} execution...`);
    addLog(`Task: ${task.substring(0, 60)}${task.length > 60 ? '...' : ''}`);

    try {
      // Phase 1: Create plan
      addLog('Creating execution plan...');
      const planResponse = await deepAgentService.createPlan({
        agent_type: selectedAgent,
        task: task,
        config: agentConfig
      });

      if (!planResponse.success || !planResponse.todos) {
        addLog(`✗ Planning failed: ${planResponse.error}`);
        setError(planResponse.error || 'Failed to create plan');
        setIsExecuting(false);
        addLog('Execution finished');
        return;
      }

      const planTodos = planResponse.todos;
      addLog(`✓ Plan created with ${planTodos.length} step(s)`);

      // Initialize todos in UI
      const uiTodos: Todo[] = planTodos.map(t => ({
        id: t.id,
        task: t.task,
        status: 'pending' as const,
        subtasks: []
      }));
      setTodos([...uiTodos]);

      // Phase 2: Execute each step incrementally
      const stepResults: { step: string; specialist: string; result: string }[] = [];

      for (let i = 0; i < planTodos.length; i++) {
        const todo = planTodos[i];

        // Mark current step as in_progress
        uiTodos[i].status = 'in_progress';
        setTodos([...uiTodos]);
        addLog(`▶ Step ${i + 1}/${planTodos.length}: ${todo.task}`);

        const stepResponse = await deepAgentService.executeStep({
          task: task,
          step_prompt: todo.prompt,
          specialist: todo.specialist,
          config: agentConfig,
          previous_results: stepResults.length > 0 ? stepResults : undefined
        });

        if (stepResponse.success && stepResponse.result) {
          uiTodos[i].status = 'completed';
          setTodos([...uiTodos]);
          stepResults.push({
            step: todo.task,
            specialist: todo.specialist,
            result: stepResponse.result
          });
          addLog(`✓ Completed: ${todo.task}`);

          // Show intermediate result immediately
          const intermediateOutput = stepResults.map(sr =>
            `## ${sr.step}\n\n${sr.result}`
          ).join('\n\n---\n\n');
          setResult(intermediateOutput);
        } else {
          addLog(`✗ Step failed: ${stepResponse.error}`);
          uiTodos[i].status = 'completed'; // mark done even if failed
          setTodos([...uiTodos]);
        }
      }

      // Phase 3: Synthesize final report
      if (stepResults.length > 1) {
        addLog('Synthesizing final report...');
        const synthesisResponse = await deepAgentService.synthesizeResults({
          task: task,
          step_results: stepResults,
          config: agentConfig
        });

        if (synthesisResponse.success && synthesisResponse.result) {
          setResult(synthesisResponse.result);
          addLog('✓ Final report generated');
        } else {
          addLog('⚠ Synthesis failed, showing individual step results');
        }
      }

      addLog('✓ All steps completed');
    } catch (err) {
      addLog(`✗ Exception: ${String(err)}`);
      setError(String(err));
    } finally {
      setIsExecuting(false);
      addLog('Execution finished');
    }
  };

  const loadExample = (example: string) => {
    setTask(example);
  };

  const toggleTodo = (todoId: string) => {
    setExpandedTodos(prev => {
      const next = new Set(prev);
      if (next.has(todoId)) {
        next.delete(todoId);
      } else {
        next.add(todoId);
      }
      return next;
    });
  };

  const renderTodo = (todo: Todo, depth: number = 0) => {
    const isExpanded = expandedTodos.has(todo.id);
    const hasSubtasks = todo.subtasks && todo.subtasks.length > 0;

    const statusColors = {
      pending: colors.textMuted,
      in_progress: colors.warning,
      completed: colors.success
    };

    const statusIcon = {
      pending: <Clock size={12} />,
      in_progress: <Loader2 size={12} className="animate-spin" />,
      completed: <CheckCircle2 size={12} />
    }[todo.status];

    return (
      <div key={todo.id} style={{ marginLeft: depth > 0 ? '16px' : '0' }}>
        <div
          style={{
            padding: '8px 12px',
            backgroundColor: colors.background,
            border: `1px solid ${statusColors[todo.status]}`,
            borderLeft: `3px solid ${statusColors[todo.status]}`,
            marginBottom: '5px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            cursor: hasSubtasks ? 'pointer' : 'default',
            fontSize: '13px',
            fontFamily: 'monospace'
          }}
          onClick={() => hasSubtasks && toggleTodo(todo.id)}
        >
          {hasSubtasks && (
            isExpanded ?
              <ChevronDown size={10} style={{ color: colors.primary }} /> :
              <ChevronRight size={10} style={{ color: colors.primary }} />
          )}
          <span style={{ color: statusColors[todo.status] }}>{statusIcon}</span>
          <span style={{ flex: 1, color: colors.text }}>
            {todo.task}
          </span>
        </div>
        {hasSubtasks && isExpanded && (
          <div style={{ marginLeft: '12px' }}>
            {todo.subtasks!.map(subtask => renderTodo(subtask, depth + 1))}
          </div>
        )}
      </div>
    );
  };

  const selectedAgentInfo = AGENT_TYPES.find(a => a.id === selectedAgent);
  const examples = EXAMPLE_TASKS[selectedAgent as keyof typeof EXAMPLE_TASKS] || [];
  const AgentIcon = selectedAgentInfo?.icon || Brain;

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: colors.background
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
        backgroundColor: colors.panel,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Network size={16} color={colors.primary} />
        <span style={{
          color: colors.primary,
          fontSize: '12px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          fontFamily: 'monospace'
        }}>
          DEEPAGENT AUTOMATION
        </span>
        <div style={{ flex: 1 }} />
        <div style={{
          fontSize: '10px',
          fontFamily: 'monospace',
          padding: '3px 8px',
          backgroundColor: colors.success + '20',
          border: `1px solid ${colors.success}`,
          color: colors.success
        }}>
          READY
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Agent Selection */}
        <div style={{
          width: '240px',
          borderRight: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
          backgroundColor: colors.panel,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            fontSize: '10px',
            fontWeight: 700,
            color: colors.textMuted,
            fontFamily: 'monospace',
            letterSpacing: '0.5px'
          }}>
            AGENT TYPE
          </div>
          <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '6px', overflowY: 'auto' }}>
            {AGENT_TYPES.map(agent => {
              const Icon = agent.icon;
              const isSelected = selectedAgent === agent.id;
              return (
                <div
                  key={agent.id}
                  onClick={() => setSelectedAgent(agent.id)}
                  style={{
                    padding: '10px',
                    backgroundColor: isSelected ? '#1F1F1F' : 'transparent',
                    border: `1px solid ${isSelected ? agent.color : 'var(--ft-border-color, #2A2A2A)'}`,
                    cursor: 'pointer',
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = colors.background;
                      e.currentTarget.style.borderColor = agent.color;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Icon size={14} style={{ color: agent.color }} />
                    <span style={{ color: colors.text, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {agent.name}
                    </span>
                  </div>
                  <p style={{ color: colors.textMuted, fontSize: '9px', margin: '0 0 6px 0', lineHeight: '1.3' }}>
                    {agent.description}
                  </p>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '3px' }}>
                    {agent.subagents.slice(0, 2).map(sub => (
                      <span
                        key={sub}
                        style={{
                          padding: '1px 4px',
                          backgroundColor: colors.background,
                          fontSize: '8px',
                          color: agent.color,
                          fontFamily: 'monospace'
                        }}
                      >
                        {sub}
                      </span>
                    ))}
                    {agent.subagents.length > 2 && (
                      <span style={{ fontSize: '8px', color: colors.textMuted, fontFamily: 'monospace' }}>
                        +{agent.subagents.length - 2}
                      </span>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Main Content Area */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
          {/* Task Input Section */}
          <div style={{
            padding: '16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              {AgentIcon && <AgentIcon size={14} style={{ color: selectedAgentInfo?.color }} />}
              <span style={{ fontSize: '11px', fontWeight: 600, color: colors.text, fontFamily: 'monospace' }}>
                {selectedAgentInfo?.name.toUpperCase()}
              </span>
              <span style={{ fontSize: '10px', color: colors.textMuted }}>•</span>
              <span style={{ fontSize: '10px', color: colors.textMuted }}>
                {selectedAgentInfo?.subagents.length} SUBAGENTS
              </span>
            </div>

            <textarea
              value={task}
              onChange={(e) => setTask(e.target.value)}
              placeholder={`> Enter task for ${selectedAgentInfo?.name}...\n\nThe agent will autonomously break down the task, delegate to specialists, and execute until completion.`}
              style={{
                width: '100%',
                height: '100px',
                padding: '10px',
                backgroundColor: colors.background,
                border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                color: colors.text,
                fontSize: '11px',
                fontFamily: 'monospace',
                resize: 'vertical'
              }}
            />

            <button
              onClick={handleExecute}
              disabled={isExecuting || !task.trim()}
              style={{
                padding: '8px 16px',
                backgroundColor: selectedAgentInfo?.color || colors.primary,
                border: 'none',
                color: colors.background,
                fontSize: '11px',
                fontWeight: 700,
                cursor: isExecuting ? 'not-allowed' : 'pointer',
                opacity: isExecuting || !task.trim() ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                fontFamily: 'monospace',
                letterSpacing: '0.5px'
              }}
            >
              {isExecuting ? (
                <>
                  <Loader2 size={12} className="animate-spin" />
                  EXECUTING...
                </>
              ) : (
                <>
                  <Play size={12} />
                  EXECUTE TASK
                </>
              )}
            </button>
          </div>

          {/* Examples Section */}
          <div style={{
            padding: '12px 16px',
            borderBottom: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
            backgroundColor: colors.panel
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: colors.textMuted, marginBottom: '8px', fontFamily: 'monospace' }}>
              EXAMPLE TASKS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {examples.map((example, idx) => (
                <button
                  key={idx}
                  onClick={() => loadExample(example)}
                  style={{
                    padding: '6px 8px',
                    backgroundColor: colors.background,
                    border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                    color: selectedAgentInfo?.color || colors.accent,
                    fontSize: '10px',
                    textAlign: 'left',
                    cursor: 'pointer',
                    fontFamily: 'monospace',
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = '#1F1F1F';
                    e.currentTarget.style.borderColor = selectedAgentInfo?.color || colors.accent;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = colors.background;
                    e.currentTarget.style.borderColor = 'var(--ft-border-color, #2A2A2A)';
                  }}
                >
                  › {example}
                </button>
              ))}
            </div>
          </div>

          {/* Results Area */}
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
            {/* Execution Log */}
            {executionLog.length > 0 && (
              <div style={{ marginBottom: '16px' }}>
                <div style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: colors.accent,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px'
                }}>
                  <Cpu size={13} color={colors.accent} />
                  EXECUTION LOG
                </div>
                <div style={{
                  padding: '12px 14px',
                  backgroundColor: colors.panel,
                  border: `1px solid ${'var(--ft-border-color, #2A2A2A)'}`,
                  borderLeft: `3px solid ${colors.accent}`,
                  maxHeight: '180px',
                  overflowY: 'auto',
                  fontSize: '13px',
                  fontFamily: 'monospace',
                  color: colors.accent,
                  lineHeight: '1.7'
                }}>
                  {executionLog.map((log, idx) => (
                    <div key={idx} style={{ marginBottom: '3px' }}>
                      {log}
                    </div>
                  ))}
                </div>
              </div>
            )}

            {error && (
              <div style={{
                padding: '12px 14px',
                backgroundColor: colors.alert + '15',
                border: `1px solid ${colors.alert}`,
                borderLeft: `3px solid ${colors.alert}`,
                color: colors.alert,
                fontSize: '13px',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                fontFamily: 'monospace',
                marginBottom: '16px',
                lineHeight: '1.5'
              }}>
                <AlertCircle size={16} style={{ flexShrink: 0 }} />
                {error}
              </div>
            )}

            {todos.length > 0 && (
              <div style={{ marginBottom: '16px' }}>
                <div style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: colors.warning,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px'
                }}>
                  <GitBranch size={13} color={colors.warning} />
                  TASK BREAKDOWN
                </div>
                {todos.map(todo => renderTodo(todo))}
              </div>
            )}

          </div>
        </div>
      </div>
    </div>
  );
}
