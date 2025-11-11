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
  private readonly STORAGE_KEY = 'fincept_workflows';

  constructor() {
    // Load workflows from localStorage on startup
    this.loadFromStorage();
  }

  private loadFromStorage(): void {
    try {
      const stored = localStorage.getItem(this.STORAGE_KEY);
      if (stored) {
        const workflows: Workflow[] = JSON.parse(stored);
        workflows.forEach(wf => this.workflows.set(wf.id, wf));
        console.log(`[WorkflowService] Loaded ${workflows.length} workflows from storage`);
      }
    } catch (error) {
      console.error('[WorkflowService] Failed to load workflows:', error);
    }
  }

  private saveToStorage(): void {
    try {
      const workflows = Array.from(this.workflows.values());
      localStorage.setItem(this.STORAGE_KEY, JSON.stringify(workflows));
      console.log(`[WorkflowService] Saved ${workflows.length} workflows to storage`);
    } catch (error) {
      console.error('[WorkflowService] Failed to save workflows:', error);
    }
  }

  async saveWorkflow(workflow: Workflow): Promise<void> {
    this.workflows.set(workflow.id, workflow);
    this.saveToStorage();
    console.log(`[WorkflowService] Saved workflow: ${workflow.id}`);
  }

  async saveDraft(
    name: string,
    description: string,
    nodes: any[],
    edges: any[]
  ): Promise<string> {
    const id = `draft_${Date.now()}`;
    const workflow: Workflow = {
      id,
      name,
      description,
      nodes,
      edges,
      status: 'draft'
    };
    await this.saveWorkflow(workflow);
    return id;
  }

  async loadWorkflow(id: string): Promise<Workflow | undefined> {
    return this.workflows.get(id);
  }

  async deleteWorkflow(id: string): Promise<void> {
    this.workflows.delete(id);
    this.runningWorkflows.delete(id);
    this.saveToStorage();
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
    this.saveToStorage(); // Save the 'running' state

    console.log(`[WorkflowService] Executing workflow: ${id}`);

    // Simulate async workflow execution
    setTimeout(() => {
      const completedWorkflow = this.workflows.get(id);
      if (completedWorkflow) {
        completedWorkflow.status = 'completed';
        this.runningWorkflows.delete(id);
        this.saveToStorage(); // Save the 'completed' state
        console.log(`[WorkflowService] Workflow ${id} completed`);
      }
    }, 100); // Simulate a 100ms execution time
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
      this.saveToStorage();
      console.log(`[WorkflowService] Updated draft: ${id}`);
    }
  }
}

export const workflowService = new WorkflowService();
