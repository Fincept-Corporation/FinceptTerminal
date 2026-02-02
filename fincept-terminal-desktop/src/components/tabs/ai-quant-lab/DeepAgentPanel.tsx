/**
 * DeepAgent Panel - Hierarchical Agentic Automation
 * Autonomous multi-step financial workflows with task delegation
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Brain,
  Sparkles,
  Play,
  Pause,
  CheckCircle2,
  Clock,
  AlertCircle,
  ChevronRight,
  ChevronDown,
  GitBranch,
  Zap,
  FileText,
  RotateCcw,
  Loader2
} from 'lucide-react';
import { deepAgentService, ExecuteTaskRequest, Todo } from '@/services/aiQuantLab/deepAgentService';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  PANEL_BG: '#0F0F0F',
  HOVER: '#1F1F1F'
};

interface AgentType {
  id: string;
  name: string;
  description: string;
  icon: React.ReactNode;
  subagents: string[];
}

const AGENT_TYPES: AgentType[] = [
  {
    id: 'research',
    name: 'Research Agent',
    description: 'Autonomous research, data analysis, and report generation',
    icon: <FileText className="w-5 h-5" />,
    subagents: ['data_analyst', 'reporter']
  },
  {
    id: 'trading_strategy',
    name: 'Strategy Builder',
    description: 'Design, backtest, and validate trading strategies',
    icon: <Zap className="w-5 h-5" />,
    subagents: ['data_analyst', 'trading', 'backtester', 'risk_analyzer', 'reporter']
  },
  {
    id: 'portfolio_management',
    name: 'Portfolio Manager',
    description: 'Optimize portfolios with risk management',
    icon: <GitBranch className="w-5 h-5" />,
    subagents: ['data_analyst', 'portfolio_optimizer', 'risk_analyzer', 'reporter']
  },
  {
    id: 'risk_assessment',
    name: 'Risk Analyzer',
    description: 'Comprehensive risk assessment and stress testing',
    icon: <AlertCircle className="w-5 h-5" />,
    subagents: ['data_analyst', 'risk_analyzer', 'reporter']
  },
  {
    id: 'general',
    name: 'General Agent',
    description: 'Full-featured agent with all specialists',
    icon: <Brain className="w-5 h-5" />,
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
  const [status, setStatus] = useState({ available: false, initialized: false });

  const resultRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    checkStatus();
  }, []);

  const checkStatus = async () => {
    const statusRes = await deepAgentService.checkStatus();
    setStatus({
      available: statusRes.available || false,
      initialized: statusRes.success || false
    });
  };

  const handleExecute = async () => {
    if (!task.trim()) return;

    setIsExecuting(true);
    setError('');
    setResult('');
    setTodos([]);

    const newThreadId = `thread-${Date.now()}`;
    setThreadId(newThreadId);

    try {
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

      const response = await deepAgentService.executeTask(request);

      if (response.success) {
        setResult(response.result || 'Task completed successfully');
        setTodos(response.todos || []);
        setThreadId(response.thread_id || newThreadId);
      } else {
        setError(response.error || 'Unknown error occurred');
      }
    } catch (err) {
      setError(String(err));
    } finally {
      setIsExecuting(false);
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

    const statusIcon = {
      pending: <Clock className="w-4 h-4" style={{ color: FINCEPT.YELLOW }} />,
      in_progress: <Loader2 className="w-4 h-4 animate-spin" style={{ color: FINCEPT.CYAN }} />,
      completed: <CheckCircle2 className="w-4 h-4" style={{ color: FINCEPT.GREEN }} />
    }[todo.status];

    return (
      <div key={todo.id} style={{ marginLeft: depth > 0 ? '20px' : '0' }}>
        <div
          style={{
            padding: '8px 12px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '6px',
            marginBottom: '6px',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            cursor: hasSubtasks ? 'pointer' : 'default'
          }}
          onClick={() => hasSubtasks && toggleTodo(todo.id)}
        >
          {hasSubtasks && (
            isExpanded ?
              <ChevronDown className="w-4 h-4" style={{ color: FINCEPT.ORANGE }} /> :
              <ChevronRight className="w-4 h-4" style={{ color: FINCEPT.ORANGE }} />
          )}
          {statusIcon}
          <span style={{ flex: 1, fontSize: '13px', color: FINCEPT.WHITE }}>
            {todo.task}
          </span>
        </div>
        {hasSubtasks && isExpanded && (
          <div style={{ marginLeft: '20px' }}>
            {todo.subtasks!.map(subtask => renderTodo(subtask, depth + 1))}
          </div>
        )}
      </div>
    );
  };

  const selectedAgentInfo = AGENT_TYPES.find(a => a.id === selectedAgent);
  const examples = EXAMPLE_TASKS[selectedAgent as keyof typeof EXAMPLE_TASKS] || [];

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      padding: '20px',
      gap: '20px',
      overflow: 'auto'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        paddingBottom: '20px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <Sparkles style={{ color: FINCEPT.ORANGE }} className="w-6 h-6" />
        <div>
          <h2 style={{ color: FINCEPT.WHITE, fontSize: '20px', fontWeight: 600, margin: 0 }}>
            DeepAgent Automation
          </h2>
          <p style={{ color: FINCEPT.CYAN, fontSize: '13px', margin: '4px 0 0 0' }}>
            Autonomous multi-step workflows with hierarchical task delegation
          </p>
        </div>
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '8px', alignItems: 'center' }}>
          <div style={{
            padding: '4px 12px',
            backgroundColor: status.available ? FINCEPT.GREEN + '20' : FINCEPT.RED + '20',
            border: `1px solid ${status.available ? FINCEPT.GREEN : FINCEPT.RED}`,
            borderRadius: '12px',
            fontSize: '11px',
            color: status.available ? FINCEPT.GREEN : FINCEPT.RED
          }}>
            {status.available ? '● Available' : '● Unavailable'}
          </div>
        </div>
      </div>

      <div style={{ display: 'flex', gap: '20px', flex: 1, minHeight: 0 }}>
        {/* Left: Agent Selection */}
        <div style={{ width: '300px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <h3 style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: 600, margin: 0 }}>
            Agent Type
          </h3>
          {AGENT_TYPES.map(agent => (
            <div
              key={agent.id}
              onClick={() => setSelectedAgent(agent.id)}
              style={{
                padding: '12px',
                backgroundColor: selectedAgent === agent.id ? FINCEPT.ORANGE + '20' : FINCEPT.PANEL_BG,
                border: `1px solid ${selectedAgent === agent.id ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                borderRadius: '8px',
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                if (selectedAgent !== agent.id) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (selectedAgent !== agent.id) {
                  e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                }
              }}
            >
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                <div style={{ color: selectedAgent === agent.id ? FINCEPT.ORANGE : FINCEPT.CYAN }}>
                  {agent.icon}
                </div>
                <span style={{ color: FINCEPT.WHITE, fontSize: '13px', fontWeight: 500 }}>
                  {agent.name}
                </span>
              </div>
              <p style={{ color: FINCEPT.CYAN, fontSize: '11px', margin: '0 0 8px 0' }}>
                {agent.description}
              </p>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                {agent.subagents.slice(0, 3).map(sub => (
                  <span
                    key={sub}
                    style={{
                      padding: '2px 6px',
                      backgroundColor: FINCEPT.BORDER,
                      borderRadius: '4px',
                      fontSize: '9px',
                      color: FINCEPT.CYAN
                    }}
                  >
                    {sub}
                  </span>
                ))}
                {agent.subagents.length > 3 && (
                  <span style={{ fontSize: '9px', color: FINCEPT.CYAN }}>
                    +{agent.subagents.length - 3} more
                  </span>
                )}
              </div>
            </div>
          ))}
        </div>

        {/* Middle: Task Input */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '12px', minWidth: 0 }}>
          <div>
            <h3 style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: 600, margin: '0 0 8px 0' }}>
              Task Description
            </h3>
            <textarea
              value={task}
              onChange={(e) => setTask(e.target.value)}
              placeholder={`Describe your task for ${selectedAgentInfo?.name}...\n\nThe agent will autonomously break it down, delegate to specialists, and execute until completion.`}
              style={{
                width: '100%',
                height: '120px',
                padding: '12px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '8px',
                color: FINCEPT.WHITE,
                fontSize: '13px',
                fontFamily: 'inherit',
                resize: 'vertical'
              }}
            />
          </div>

          {/* Examples */}
          <div>
            <h4 style={{ color: FINCEPT.CYAN, fontSize: '12px', fontWeight: 500, margin: '0 0 8px 0' }}>
              Example Tasks:
            </h4>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {examples.map((example, idx) => (
                <button
                  key={idx}
                  onClick={() => loadExample(example)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '6px',
                    color: FINCEPT.CYAN,
                    fontSize: '11px',
                    textAlign: 'left',
                    cursor: 'pointer',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  }}
                >
                  {example}
                </button>
              ))}
            </div>
          </div>

          {/* Execute Button */}
          <button
            onClick={handleExecute}
            disabled={isExecuting || !task.trim() || !status.available}
            style={{
              padding: '12px 24px',
              backgroundColor: FINCEPT.ORANGE,
              border: 'none',
              borderRadius: '8px',
              color: FINCEPT.WHITE,
              fontSize: '14px',
              fontWeight: 600,
              cursor: isExecuting || !status.available ? 'not-allowed' : 'pointer',
              opacity: isExecuting || !status.available ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '8px',
              transition: 'all 0.2s'
            }}
          >
            {isExecuting ? (
              <>
                <Loader2 className="w-4 h-4 animate-spin" />
                Executing...
              </>
            ) : (
              <>
                <Play className="w-4 h-4" />
                Execute Task
              </>
            )}
          </button>

          {/* Error */}
          {error && (
            <div style={{
              padding: '12px',
              backgroundColor: FINCEPT.RED + '20',
              border: `1px solid ${FINCEPT.RED}`,
              borderRadius: '8px',
              color: FINCEPT.RED,
              fontSize: '12px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <AlertCircle className="w-4 h-4" />
              {error}
            </div>
          )}

          {/* Todos */}
          {todos.length > 0 && (
            <div>
              <h3 style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: 600, margin: '0 0 8px 0' }}>
                Task Breakdown
              </h3>
              <div style={{
                maxHeight: '200px',
                overflowY: 'auto',
                paddingRight: '8px'
              }}>
                {todos.map(todo => renderTodo(todo))}
              </div>
            </div>
          )}

          {/* Result */}
          {result && (
            <div>
              <h3 style={{ color: FINCEPT.WHITE, fontSize: '14px', fontWeight: 600, margin: '0 0 8px 0' }}>
                Result
              </h3>
              <div
                ref={resultRef}
                style={{
                  padding: '16px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '8px',
                  color: FINCEPT.WHITE,
                  fontSize: '13px',
                  lineHeight: '1.6',
                  maxHeight: '300px',
                  overflowY: 'auto',
                  whiteSpace: 'pre-wrap'
                }}
              >
                {result}
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
