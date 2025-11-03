import React from 'react';
import { Node, Edge } from 'reactflow';
import { Workflow } from '@/services/workflowService';

interface WorkflowManagerProps {
  onLoadWorkflow: (nodes: Node[], edges: Edge[], workflowId: string, workflow: Workflow) => void;
  onViewResults: (workflow: Workflow) => void;
  onEditDraft: (workflow: Workflow) => void;
}

const WorkflowManager: React.FC<WorkflowManagerProps> = ({ onLoadWorkflow, onViewResults, onEditDraft }) => {
  return (
    <div style={{
      padding: '20px',
      color: '#a3a3a3'
    }}>
      <h2>Workflow Manager</h2>
      <p>No workflows available</p>
    </div>
  );
};

export default WorkflowManager;
