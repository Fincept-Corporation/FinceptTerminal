/**
 * AI Quant Lab - Type Definitions
 *
 * Shared types and interfaces for the Node Editor and workflow system
 */

import { Node, Edge } from 'reactflow';
import type { INodeProperties, NodeParameterValue, INodeTypeDescription } from '@/services/nodeSystem';
import { Workflow as WorkflowType } from '@/services/core/workflowService';
import { MCPNodeConfig } from './nodes/mcpNodeConfigs';
import { AgentMetadata } from '@/services/chat/pythonAgentService';

// Node configuration for builtin nodes
export interface NodeConfig {
  type: string;
  label: string;
  color: string;
  category: string;
  hasInput: boolean;
  hasOutput: boolean;
  description: string;
}

// Node palette item representation
export interface PaletteNodeItem {
  id: string;
  type: string;
  label: string;
  category: string;
  color: string;
  description: string;
  source: 'builtin' | 'registry' | 'mcp' | 'agent';
  data?: any;
}

// Node palette props
export interface NodePaletteProps {
  onNodeAdd: (nodeType: string, nodeData: any) => void;
  isCollapsed: boolean;
  onToggleCollapse: () => void;
  mcpNodeConfigs: MCPNodeConfig[];
  agentConfigs: AgentMetadata[];
}

// Toolbar props
export interface NodeEditorToolbarProps {
  activeView: 'editor' | 'workflows';
  setActiveView: (view: 'editor' | 'workflows') => void;
  showNodeMenu: boolean;
  setShowNodeMenu: (show: boolean) => void;
  isExecuting: boolean;
  nodes: Node[];
  edges: Edge[];
  selectedNodes: string[];
  workflowMode: 'new' | 'draft' | 'deployed';
  editingDraftId: string | null;
  workflowName: string;
  workflowDescription: string;
  onExecute: () => Promise<void>;
  onClearWorkflow: () => void;
  onSaveWorkflow: () => void;
  onImportWorkflow: () => void;
  onExportWorkflow: () => void;
  onDeleteSelectedNodes: () => void;
  onShowDeployDialog: () => void;
  onQuickSaveDraft: () => Promise<void>;
  onShowTemplates: () => void;
}

// Deploy dialog props
export interface DeployDialogProps {
  isOpen: boolean;
  onClose: () => void;
  workflowName: string;
  setWorkflowName: (name: string) => void;
  workflowDescription: string;
  setWorkflowDescription: (description: string) => void;
  onDeploy: () => void;
  onSaveDraft: () => void;
}

// Node config panel props
export interface NodeConfigPanelProps {
  selectedNode: Node;
  onLabelChange: (nodeId: string, newLabel: string) => void;
  onParameterChange: (paramName: string, value: NodeParameterValue) => void;
  onClose: () => void;
  onDelete: () => void;
  onDuplicate: () => void;
  setNodes: React.Dispatch<React.SetStateAction<Node[]>>;
}

// Custom node data
export interface CustomNodeData {
  label: string;
  nodeType?: string;
  color: string;
  hasInput: boolean;
  hasOutput: boolean;
  status?: 'idle' | 'running' | 'completed' | 'error';
  result?: any;
  error?: string;
  parameters?: Record<string, any>;
  registryData?: INodeTypeDescription;
  nodeTypeName?: string;
  onLabelChange: (nodeId: string, newLabel: string) => void;
}

// Workflow management hook return type
export interface WorkflowManagementState {
  nodes: Node[];
  edges: Edge[];
  setNodes: React.Dispatch<React.SetStateAction<Node[]>>;
  setEdges: React.Dispatch<React.SetStateAction<Edge[]>>;
  onNodesChange: any;
  onEdgesChange: any;
  selectedNodes: string[];
  setSelectedNodes: React.Dispatch<React.SetStateAction<string[]>>;
  isExecuting: boolean;
  setIsExecuting: React.Dispatch<React.SetStateAction<boolean>>;
  workflowMode: 'new' | 'draft' | 'deployed';
  setWorkflowMode: React.Dispatch<React.SetStateAction<'new' | 'draft' | 'deployed'>>;
  currentWorkflowId: string | null;
  setCurrentWorkflowId: React.Dispatch<React.SetStateAction<string | null>>;
  editingDraftId: string | null;
  setEditingDraftId: React.Dispatch<React.SetStateAction<string | null>>;
  workflowName: string;
  setWorkflowName: React.Dispatch<React.SetStateAction<string>>;
  workflowDescription: string;
  setWorkflowDescription: React.Dispatch<React.SetStateAction<string>>;
}

// Category configuration
export interface CategoryConfig {
  icon: React.ReactNode;
  color: string;
}

export type { WorkflowType, MCPNodeConfig, AgentMetadata, INodeProperties, NodeParameterValue, INodeTypeDescription };
