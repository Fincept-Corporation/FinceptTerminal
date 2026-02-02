/**
 * useWorkflowManagement Hook
 *
 * Custom hook for managing workflow state, persistence, and operations
 */

import { useState, useCallback, useEffect } from 'react';
import { Node, Edge, useNodesState, useEdgesState } from 'reactflow';
import { saveSetting, getSetting } from '@/services/core/sqliteService';
import { workflowService, Workflow as WorkflowType } from '@/services/core/workflowService';
import { workflowExecutor } from '../nodes/WorkflowExecutor';
import { showWarning, showError, showConfirm } from '@/utils/notifications';

export interface UseWorkflowManagementReturn {
  // Node and edge state
  nodes: Node[];
  edges: Edge[];
  setNodes: React.Dispatch<React.SetStateAction<Node[]>>;
  setEdges: React.Dispatch<React.SetStateAction<Edge[]>>;
  onNodesChange: any;
  onEdgesChange: any;

  // Selection state
  selectedNodes: string[];
  setSelectedNodes: React.Dispatch<React.SetStateAction<string[]>>;

  // Execution state
  isExecuting: boolean;
  setIsExecuting: React.Dispatch<React.SetStateAction<boolean>>;

  // Workflow metadata
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

  // Operations
  saveWorkflow: () => Promise<void>;
  exportWorkflow: () => void;
  importWorkflow: () => void;
  clearWorkflow: () => Promise<void>;
  deployWorkflow: (statusCallback: (nodeId: string, status: string, result?: any) => void) => Promise<void>;
  saveDraft: () => Promise<void>;
  handleLoadWorkflow: (loadedNodes: Node[], loadedEdges: Edge[], workflowId: string, workflow: WorkflowType) => void;
  handleViewResults: (workflow: WorkflowType) => Promise<{ showModal: boolean; data: any; name: string } | { navigate: boolean }>;
  handleEditDraft: (workflow: WorkflowType) => void;
  deleteSelectedNodes: () => void;
  deleteSelectedEdges: () => void;
  onSelectionChange: (params: any) => void;
}

export function useWorkflowManagement(): UseWorkflowManagementReturn {
  // Node and edge state
  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);

  // Selection state
  const [selectedNodes, setSelectedNodes] = useState<string[]>([]);

  // Execution state
  const [isExecuting, setIsExecuting] = useState(false);

  // Workflow metadata
  const [workflowMode, setWorkflowMode] = useState<'new' | 'draft' | 'deployed'>('new');
  const [currentWorkflowId, setCurrentWorkflowId] = useState<string | null>(null);
  const [editingDraftId, setEditingDraftId] = useState<string | null>(null);
  const [workflowName, setWorkflowName] = useState('');
  const [workflowDescription, setWorkflowDescription] = useState('');

  // Load saved workflow from storage
  const loadSavedWorkflow = useCallback(async () => {
    try {
      const saved = await getSetting('nodeEditorWorkflow');
      if (saved) {
        const workflow = JSON.parse(saved);
        return workflow;
      }
    } catch (error) {
      console.error('[useWorkflowManagement] Failed to load saved workflow:', error);
    }
    return { nodes: [], edges: [] };
  }, []);

  // Load workflow on mount
  useEffect(() => {
    const initWorkflow = async () => {
      const workflow = await loadSavedWorkflow();
      setNodes(workflow.nodes);
      setEdges(workflow.edges);
    };
    initWorkflow();
  }, [loadSavedWorkflow, setNodes, setEdges]);

  // Auto-save workflow to storage whenever nodes or edges change
  useEffect(() => {
    const saveWorkflowAuto = async () => {
      try {
        const workflow = { nodes, edges };
        await saveSetting('nodeEditorWorkflow', JSON.stringify(workflow), 'node_editor');
      } catch (error) {
        console.error('[useWorkflowManagement] Failed to auto-save workflow:', error);
      }
    };

    // Debounce saves
    const timeoutId = setTimeout(saveWorkflowAuto, 500);
    return () => clearTimeout(timeoutId);
  }, [nodes, edges]);

  // Save workflow manually
  const saveWorkflow = useCallback(async () => {
    try {
      const workflow = { nodes, edges };
      await saveSetting('nodeEditorWorkflow', JSON.stringify(workflow), 'node_editor');
    } catch (error) {
      console.error('[useWorkflowManagement] Failed to save workflow:', error);
    }
  }, [nodes, edges]);

  // Export workflow to JSON file
  const exportWorkflow = useCallback(() => {
    const workflow = {
      nodes: nodes,
      edges: edges,
    };
    const dataStr = JSON.stringify(workflow, null, 2);
    const dataUri = `data:application/json;charset=utf-8,${encodeURIComponent(dataStr)}`;
    const exportFileDefaultName = `workflow_${Date.now()}.json`;

    const linkElement = document.createElement('a');
    linkElement.setAttribute('href', dataUri);
    linkElement.setAttribute('download', exportFileDefaultName);
    linkElement.click();
  }, [nodes, edges]);

  // Import workflow from JSON file
  const importWorkflow = useCallback(() => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = (e: any) => {
      const file = e.target.files[0];
      if (file) {
        const reader = new FileReader();
        reader.onload = (event: any) => {
          try {
            const workflow = JSON.parse(event.target.result);
            setNodes(workflow.nodes || []);
            setEdges(workflow.edges || []);
          } catch (error) {
            console.error('[useWorkflowManagement] Failed to import workflow:', error);
          }
        };
        reader.readAsText(file);
      }
    };
    input.click();
  }, [setNodes, setEdges]);

  // Clear workflow
  const clearWorkflow = useCallback(async () => {
    setNodes([]);
    setEdges([]);
    await saveSetting('nodeEditorWorkflow', '', 'node_editor');
    setWorkflowMode('new');
    setCurrentWorkflowId(null);
    setEditingDraftId(null);
    setWorkflowName('');
    setWorkflowDescription('');
  }, [setNodes, setEdges]);

  // Deploy workflow
  const deployWorkflow = useCallback(
    async (statusCallback: (nodeId: string, status: string, result?: any) => void) => {
      if (!workflowName.trim()) {
        showWarning('Please enter a workflow name');
        return;
      }

      if (isExecuting) {
        showWarning('A workflow is already executing. Please wait for it to complete.');
        return;
      }

      const name = workflowName;
      const description = workflowDescription;
      const currentNodes = nodes;
      const currentEdges = edges;

      setIsExecuting(true);

      try {
        const result = await workflowExecutor.executeAndSave(
          currentNodes,
          currentEdges,
          {
            name: name,
            description: description,
            saveAsDraft: false,
          },
          statusCallback
        );

        if (result.success) {
          setNodes([]);
          setEdges([]);
          await saveSetting('nodeEditorWorkflow', '', 'node_editor');
          setCurrentWorkflowId(null);
          setWorkflowMode('new');
          setEditingDraftId(null);
        } else {
          console.error('[useWorkflowManagement] Deployment failed:', result.error);
          showError('Deployment failed', [
            { label: 'ERROR', value: result.error || 'Unknown error' }
          ]);
        }
      } catch (error: any) {
        console.error('[useWorkflowManagement] Deploy failed:', error);
        showError('Deployment failed', [
          { label: 'ERROR', value: error.message }
        ]);
      } finally {
        setIsExecuting(false);
      }
    },
    [nodes, edges, workflowName, workflowDescription, isExecuting, setNodes, setEdges]
  );

  // Save as draft
  const saveDraft = useCallback(async () => {
    if (!workflowName.trim()) {
      showWarning('Please enter a workflow name');
      return;
    }

    try {
      let workflowId: string;
      if (editingDraftId) {
        await workflowService.updateDraft(
          editingDraftId,
          workflowName,
          workflowDescription,
          nodes,
          edges
        );
        workflowId = editingDraftId;
        setEditingDraftId(null);
      } else {
        workflowId = await workflowExecutor.saveDraft(workflowName, workflowDescription, nodes, edges);
      }
      setCurrentWorkflowId(workflowId);
      setWorkflowMode('draft');
    } catch (error: any) {
      console.error('[useWorkflowManagement] Save draft failed:', error);
      showError('Save failed', [
        { label: 'ERROR', value: error.message }
      ]);
    }
  }, [nodes, edges, workflowName, workflowDescription, editingDraftId]);

  // Load workflow from manager
  const handleLoadWorkflow = useCallback(
    (loadedNodes: Node[], loadedEdges: Edge[], workflowId: string, workflow: WorkflowType) => {
      setNodes(loadedNodes);
      setEdges(loadedEdges);
      setCurrentWorkflowId(workflowId);
      setWorkflowMode(workflow.status === 'draft' ? 'draft' : 'deployed');
    },
    [setNodes, setEdges]
  );

  // View workflow results
  const handleViewResults = useCallback(
    async (workflow: WorkflowType): Promise<{ showModal: boolean; data: any; name: string } | { navigate: boolean }> => {
      const choice = await showConfirm(
        'Click OK to view in modal window, or Cancel to navigate to workflow editor with results',
        {
          title: `View results for: ${workflow.name}`,
          type: 'info'
        }
      );

      if (choice) {
        return { showModal: true, data: workflow.results, name: workflow.name };
      } else {
        setNodes(workflow.nodes);
        setEdges(workflow.edges);
        setCurrentWorkflowId(workflow.id);
        setWorkflowMode('deployed');
        return { navigate: true };
      }
    },
    [setNodes, setEdges]
  );

  // Edit draft workflow
  const handleEditDraft = useCallback(
    (workflow: WorkflowType) => {
      setNodes(workflow.nodes);
      setEdges(workflow.edges);
      setWorkflowName(workflow.name);
      setWorkflowDescription(workflow.description || '');
      setEditingDraftId(workflow.id);
      setCurrentWorkflowId(workflow.id);
      setWorkflowMode('draft');
    },
    [setNodes, setEdges]
  );

  // Delete selected nodes
  const deleteSelectedNodes = useCallback(() => {
    if (selectedNodes.length > 0) {
      setNodes((nds) => nds.filter((node) => !selectedNodes.includes(node.id)));
      setEdges((eds) =>
        eds.filter(
          (edge) => !selectedNodes.includes(edge.source) && !selectedNodes.includes(edge.target)
        )
      );
      setSelectedNodes([]);
    }
  }, [selectedNodes, setNodes, setEdges]);

  // Delete selected edges
  const deleteSelectedEdges = useCallback(() => {
    setEdges((eds) => eds.filter((edge) => !edge.selected));
  }, [setEdges]);

  // Handle selection change
  const onSelectionChange = useCallback((params: any) => {
    setSelectedNodes(params.nodes.map((node: Node) => node.id));
  }, []);

  return {
    nodes,
    edges,
    setNodes,
    setEdges,
    onNodesChange,
    onEdgesChange,
    selectedNodes,
    setSelectedNodes,
    isExecuting,
    setIsExecuting,
    workflowMode,
    setWorkflowMode,
    currentWorkflowId,
    setCurrentWorkflowId,
    editingDraftId,
    setEditingDraftId,
    workflowName,
    setWorkflowName,
    workflowDescription,
    setWorkflowDescription,
    saveWorkflow,
    exportWorkflow,
    importWorkflow,
    clearWorkflow,
    deployWorkflow,
    saveDraft,
    handleLoadWorkflow,
    handleViewResults,
    handleEditDraft,
    deleteSelectedNodes,
    deleteSelectedEdges,
    onSelectionChange,
  };
}
