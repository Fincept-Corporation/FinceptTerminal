export interface Workflow {
  id: string;
  name: string;
  description?: string;
  nodes: any[];
  edges: any[];
  status: 'idle' | 'running' | 'completed' | 'error' | 'draft';
  results?: any;
}

class WorkflowService {
  private workflows: Map<string, Workflow> = new Map();
  private runningWorkflows: Set<string> = new Set();

  async saveWorkflow(workflow: Workflow): Promise<void> {
    this.workflows.set(workflow.id, workflow);
    console.log(`[WorkflowService] Saved workflow: ${workflow.id}`);
  }

  async loadWorkflow(id: string): Promise<Workflow | undefined> {
    return this.workflows.get(id);
  }

  async deleteWorkflow(id: string): Promise<void> {
    this.workflows.delete(id);
    this.runningWorkflows.delete(id);
    console.log(`[WorkflowService] Deleted workflow: ${id}`);
  }

  async listWorkflows(): Promise<Workflow[]> {
    return Array.from(this.workflows.values());
  }

  async executeWorkflow(id: string): Promise<void> {
    const workflow = this.workflows.get(id);
    if (!workflow) {
      throw new Error(`Workflow ${id} not found`);
    }

    this.runningWorkflows.add(id);
    workflow.status = 'running';

    console.log(`[WorkflowService] Executing workflow: ${id}`);

    // Simulate workflow execution
    // In a real implementation, this would execute the nodes

    workflow.status = 'completed';
    this.runningWorkflows.delete(id);
  }

  async stopWorkflow(id: string): Promise<void> {
    this.runningWorkflows.delete(id);
    const workflow = this.workflows.get(id);
    if (workflow) {
      workflow.status = 'idle';
    }
    console.log(`[WorkflowService] Stopped workflow: ${id}`);
  }

  async cleanupRunningWorkflows(): Promise<void> {
    console.log('[WorkflowService] Cleaning up running workflows...');
    for (const workflowId of this.runningWorkflows) {
      await this.stopWorkflow(workflowId);
    }
    this.runningWorkflows.clear();
  }

  isWorkflowRunning(id: string): boolean {
    return this.runningWorkflows.has(id);
  }

  async updateDraft(
    id: string,
    name: string,
    description: string,
    nodes: any[],
    edges: any[]
  ): Promise<void> {
    const workflow = this.workflows.get(id);
    if (workflow) {
      workflow.name = name;
      workflow.description = description;
      workflow.nodes = nodes;
      workflow.edges = edges;
      workflow.status = 'draft';
      this.workflows.set(id, workflow);
      console.log(`[WorkflowService] Updated draft: ${id}`);
    }
  }
}

export const workflowService = new WorkflowService();
