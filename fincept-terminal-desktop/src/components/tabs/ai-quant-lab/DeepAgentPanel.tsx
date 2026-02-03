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
import { deepAgentService, ExecuteTaskRequest, Todo } from '@/services/aiQuantLab/deepAgentService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  PANEL_BG: '#0F0F0F',
  DARK_BG: '#000000',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface AgentType {
  id: string;
  name: string;
  description: string;
  icon: React.ComponentType<any>;
  color: string;
  subagents: string[];
}

const AGENT_TYPES: AgentType[] = [
  {
    id: 'research',
    name: 'Research Agent',
    description: 'Autonomous research, data analysis, and report generation',
    icon: FileText,
    color: FINCEPT.CYAN,
    subagents: ['data_analyst', 'reporter']
  },
  {
    id: 'trading_strategy',
    name: 'Strategy Builder',
    description: 'Design, backtest, and validate trading strategies',
    icon: Zap,
    color: FINCEPT.YELLOW,
    subagents: ['data_analyst', 'trading', 'backtester', 'risk_analyzer', 'reporter']
  },
  {
    id: 'portfolio_management',
    name: 'Portfolio Manager',
    description: 'Optimize portfolios with risk management',
    icon: GitBranch,
    color: FINCEPT.GREEN,
    subagents: ['data_analyst', 'portfolio_optimizer', 'risk_analyzer', 'reporter']
  },
  {
    id: 'risk_assessment',
    name: 'Risk Analyzer',
    description: 'Comprehensive risk assessment and stress testing',
    icon: AlertCircle,
    color: FINCEPT.RED,
    subagents: ['data_analyst', 'risk_analyzer', 'reporter']
  },
  {
    id: 'general',
    name: 'General Agent',
    description: 'Full-featured agent with all specialists',
    icon: Brain,
    color: FINCEPT.PURPLE,
    subagents: ['research', 'data_analyst', 'trading', 'risk_analyzer', 'portfolio_optimizer', 'backtester', 'reporter']
  }
];

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

export function DeepAgentPanel() {
  const [selectedAgent, setSelectedAgent] = useState<string>('research');
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

    addLog(`Starting ${selectedAgentInfo?.name} execution...`);
    addLog(`Task: ${task.substring(0, 60)}${task.length > 60 ? '...' : ''}`);

    try {
      addLog('Creating agent configuration...');
      const request: ExecuteTaskRequest = {
        agent_type: selectedAgent,
        task: task,
        thread_id: newThreadId,
        config: {
          enable_checkpointing: true,
          enable_summarization: true,
          recursion_limit: 100
        }
      };

      addLog('Invoking DeepAgent backend...');
      const response = await deepAgentService.executeTask(request);

      if (response.success) {
        addLog('✓ Task completed successfully');
        setResult(response.result || 'Task completed successfully');
        setTodos(response.todos || []);
        setThreadId(response.thread_id || newThreadId);
        if (response.todos && response.todos.length > 0) {
          addLog(`Generated ${response.todos.length} task(s)`);
        }
      } else {
        addLog(`✗ Error: ${response.error}`);
        setError(response.error || 'Unknown error occurred');
      }
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
      pending: FINCEPT.MUTED,
      in_progress: FINCEPT.YELLOW,
      completed: FINCEPT.GREEN
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
            padding: '6px 10px',
            backgroundColor: FINCEPT.DARK_BG,
            border: `1px solid ${statusColors[todo.status]}`,
            marginBottom: '4px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            cursor: hasSubtasks ? 'pointer' : 'default',
            fontSize: '11px',
            fontFamily: 'monospace'
          }}
          onClick={() => hasSubtasks && toggleTodo(todo.id)}
        >
          {hasSubtasks && (
            isExpanded ?
              <ChevronDown size={10} style={{ color: FINCEPT.ORANGE }} /> :
              <ChevronRight size={10} style={{ color: FINCEPT.ORANGE }} />
          )}
          <span style={{ color: statusColors[todo.status] }}>{statusIcon}</span>
          <span style={{ flex: 1, color: FINCEPT.WHITE }}>
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
      backgroundColor: FINCEPT.DARK_BG
    }}>
      {/* Terminal-style Header */}
      <div style={{
        padding: '12px 16px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.PANEL_BG,
        display: 'flex',
        alignItems: 'center',
        gap: '12px'
      }}>
        <Network size={16} color={FINCEPT.ORANGE} />
        <span style={{
          color: FINCEPT.ORANGE,
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
          backgroundColor: FINCEPT.GREEN + '20',
          border: `1px solid ${FINCEPT.GREEN}`,
          color: FINCEPT.GREEN
        }}>
          READY
        </div>
      </div>

      <div style={{ display: 'flex', flex: 1, minHeight: 0 }}>
        {/* Left Sidebar - Agent Selection */}
        <div style={{
          width: '240px',
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.PANEL_BG,
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '10px 12px',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            fontSize: '10px',
            fontWeight: 700,
            color: FINCEPT.MUTED,
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
                    backgroundColor: isSelected ? FINCEPT.HOVER : 'transparent',
                    border: `1px solid ${isSelected ? agent.color : FINCEPT.BORDER}`,
                    cursor: 'pointer',
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                      e.currentTarget.style.borderColor = agent.color;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isSelected) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                    <Icon size={14} style={{ color: agent.color }} />
                    <span style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 600, fontFamily: 'monospace' }}>
                      {agent.name}
                    </span>
                  </div>
                  <p style={{ color: FINCEPT.MUTED, fontSize: '9px', margin: '0 0 6px 0', lineHeight: '1.3' }}>
                    {agent.description}
                  </p>
                  <div style={{ display: 'flex', flexWrap: 'wrap', gap: '3px' }}>
                    {agent.subagents.slice(0, 2).map(sub => (
                      <span
                        key={sub}
                        style={{
                          padding: '1px 4px',
                          backgroundColor: FINCEPT.DARK_BG,
                          fontSize: '8px',
                          color: agent.color,
                          fontFamily: 'monospace'
                        }}
                      >
                        {sub}
                      </span>
                    ))}
                    {agent.subagents.length > 2 && (
                      <span style={{ fontSize: '8px', color: FINCEPT.MUTED, fontFamily: 'monospace' }}>
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
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            gap: '12px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              {AgentIcon && <AgentIcon size={14} style={{ color: selectedAgentInfo?.color }} />}
              <span style={{ fontSize: '11px', fontWeight: 600, color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
                {selectedAgentInfo?.name.toUpperCase()}
              </span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>•</span>
              <span style={{ fontSize: '10px', color: FINCEPT.MUTED }}>
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
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.WHITE,
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
                backgroundColor: selectedAgentInfo?.color || FINCEPT.ORANGE,
                border: 'none',
                color: FINCEPT.DARK_BG,
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
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: FINCEPT.PANEL_BG
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.MUTED, marginBottom: '8px', fontFamily: 'monospace' }}>
              EXAMPLE TASKS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {examples.map((example, idx) => (
                <button
                  key={idx}
                  onClick={() => loadExample(example)}
                  style={{
                    padding: '6px 8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: selectedAgentInfo?.color || FINCEPT.CYAN,
                    fontSize: '10px',
                    textAlign: 'left',
                    cursor: 'pointer',
                    fontFamily: 'monospace',
                    transition: 'all 0.15s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.borderColor = selectedAgentInfo?.color || FINCEPT.CYAN;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.DARK_BG;
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
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
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.MUTED,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  EXECUTION LOG
                </div>
                <div style={{
                  padding: '8px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  maxHeight: '150px',
                  overflowY: 'auto',
                  fontSize: '10px',
                  fontFamily: 'monospace',
                  color: FINCEPT.CYAN
                }}>
                  {executionLog.map((log, idx) => (
                    <div key={idx} style={{ marginBottom: '2px' }}>
                      {log}
                    </div>
                  ))}
                </div>
              </div>
            )}

            {error && (
              <div style={{
                padding: '10px',
                backgroundColor: FINCEPT.RED + '15',
                border: `1px solid ${FINCEPT.RED}`,
                color: FINCEPT.RED,
                fontSize: '11px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                fontFamily: 'monospace',
                marginBottom: '12px'
              }}>
                <AlertCircle size={14} />
                {error}
              </div>
            )}

            {todos.length > 0 && (
              <div style={{ marginBottom: '16px' }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.MUTED,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  TASK BREAKDOWN
                </div>
                {todos.map(todo => renderTodo(todo))}
              </div>
            )}

            {result && (
              <div>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.MUTED,
                  marginBottom: '8px',
                  fontFamily: 'monospace',
                  letterSpacing: '0.5px'
                }}>
                  RESULT
                </div>
                <div
                  ref={resultRef}
                  style={{
                    padding: '12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.WHITE,
                    fontSize: '11px',
                    lineHeight: '1.6',
                    whiteSpace: 'pre-wrap',
                    fontFamily: 'monospace'
                  }}
                >
                  {result}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
