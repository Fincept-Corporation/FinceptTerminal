// Agent Types and Interfaces

export type AgentCategory = 'hedge-fund' | 'trader' | 'analysis' | 'data' | 'execution' | 'monitoring';

export type AgentStatus = 'idle' | 'running' | 'completed' | 'error' | 'paused';

export interface AgentConfig {
  id: string;
  name: string;
  type: string;
  category: AgentCategory;
  icon: string;
  color: string;
  description: string;
  hasInput: boolean;
  hasOutput: boolean;
  parameters: AgentParameter[];
  requiredDataSources?: string[];
}

export interface AgentParameter {
  name: string;
  label: string;
  type: 'text' | 'number' | 'select' | 'boolean' | 'data-source' | 'slider' | 'multi-select';
  defaultValue?: any;
  options?: { label: string; value: string }[];
  min?: number;
  max?: number;
  step?: number;
  required?: boolean;
  description?: string;
}

export interface AgentNode {
  id: string;
  type: string;
  position: { x: number; y: number };
  data: {
    label: string;
    agentType: string;
    category: AgentCategory;
    color: string;
    hasInput: boolean;
    hasOutput: boolean;
    parameters: Record<string, any>;
    status: AgentStatus;
    lastRun?: string;
    error?: string;
    onLabelChange: (nodeId: string, newLabel: string) => void;
    onParameterChange: (nodeId: string, paramName: string, value: any) => void;
  };
}

export interface AgentWorkflow {
  id: string;
  name: string;
  description: string;
  nodes: any[];
  edges: any[];
  createdAt: string;
  updatedAt: string;
  schedule?: WorkflowSchedule;
  isActive: boolean;
}

export interface WorkflowSchedule {
  type: 'interval' | 'cron' | 'manual';
  interval?: number; // in minutes
  cronExpression?: string;
  nextRun?: string;
}

export interface AgentExecutionResult {
  agentId: string;
  agentType: string;
  status: 'success' | 'error';
  output: any;
  error?: string;
  executionTime: number;
  timestamp: string;
}

export interface WorkflowExecution {
  id: string;
  workflowId: string;
  startTime: string;
  endTime?: string;
  status: 'running' | 'completed' | 'failed' | 'cancelled';
  results: AgentExecutionResult[];
  totalExecutionTime?: number;
}
