import { Node, Edge } from 'reactflow';

interface ExecuteOptions {
  name: string;
  description: string;
  saveAsDraft: boolean;
}

type StatusCallback = (nodeId: string, status: string, result?: any) => void;

class WorkflowExecutor {
  async executeAndSave(
    nodes: Node[],
    edges: Edge[],
    options: ExecuteOptions,
    statusCallback: StatusCallback
  ): Promise<{ success: boolean; workflowId?: string; error?: string }> {
    console.log('[WorkflowExecutor] Executing workflow:', options.name);

    try {
      // Simulate workflow execution
      for (const node of nodes) {
        statusCallback(node.id, 'running');
        await new Promise(resolve => setTimeout(resolve, 500));
        statusCallback(node.id, 'completed', { message: 'Node executed successfully' });
      }

      return {
        success: true,
        workflowId: `workflow_${Date.now()}`
      };
    } catch (error) {
      return {
        success: false,
        error: String(error)
      };
    }
  }

  async saveDraft(
    name: string,
    description: string,
    nodes: Node[],
    edges: Edge[]
  ): Promise<string> {
    console.log('[WorkflowExecutor] Saving draft:', name);
    return `draft_${Date.now()}`;
  }
}

export const workflowExecutor = new WorkflowExecutor();
