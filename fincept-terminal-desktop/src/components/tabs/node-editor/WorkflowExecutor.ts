import { Node, Edge } from 'reactflow';
import { workflowService } from '@/services/workflowService';
import { invoke } from '@tauri-apps/api/core';

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
      // Execute workflow nodes
      const results = new Map<string, any>();

      for (const node of nodes) {
        statusCallback(node.id, 'running');

        try {
          let result: any;

          // Execute based on node type
          switch (node.type) {
            case 'technical-indicator':
              result = await this.executeTechnicalIndicatorNode(node, results);
              break;

            case 'python-agent':
              result = await this.executePythonAgentNode(node, results);
              break;

            case 'data-source':
              result = await this.executeDataSourceNode(node, results);
              break;

            default:
              // For other node types, simulate execution
              await new Promise(resolve => setTimeout(resolve, 500));
              result = { message: 'Node executed successfully' };
          }

          results.set(node.id, result);
          statusCallback(node.id, 'completed', result);
        } catch (error) {
          console.error(`[WorkflowExecutor] Error executing node ${node.id}:`, error);
          statusCallback(node.id, 'error', { error: String(error) });
          throw error;
        }
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

  private async executeTechnicalIndicatorNode(node: Node, previousResults: Map<string, any>): Promise<any> {
    console.log('[WorkflowExecutor] Executing technical indicator node:', node.id);

    const { dataSource, symbol, period, csvPath, jsonData, categories } = node.data;

    try {
      let result: string;

      // Build categories string
      const categoriesStr = categories && categories.length > 0 ? categories.join(',') : undefined;

      // Execute based on data source
      switch (dataSource) {
        case 'yfinance':
          result = await invoke('calculate_indicators_yfinance', {
            symbol: symbol || 'AAPL',
            period: period || '1y',
            categories: categoriesStr
          });
          break;

        case 'csv':
          if (!csvPath) {
            throw new Error('CSV file path is required');
          }
          result = await invoke('calculate_indicators_csv', {
            filepath: csvPath,
            categories: categoriesStr
          });
          break;

        case 'json':
          if (!jsonData) {
            throw new Error('JSON data is required');
          }
          result = await invoke('calculate_indicators_json', {
            jsonData: jsonData,
            categories: categoriesStr
          });
          break;

        case 'input':
          // Get data from previous node
          const inputEdges = node.data.inputEdges || [];
          if (inputEdges.length === 0) {
            throw new Error('No input connection found');
          }

          const sourceNodeId = inputEdges[0].source;
          const sourceData = previousResults.get(sourceNodeId);

          if (!sourceData) {
            throw new Error('No data available from input node');
          }

          // Use the data from previous node as JSON input
          result = await invoke('calculate_indicators_json', {
            jsonData: JSON.stringify(sourceData),
            categories: categoriesStr
          });
          break;

        default:
          throw new Error(`Unknown data source: ${dataSource}`);
      }

      // Parse result
      const parsedResult = JSON.parse(result);

      // Check for errors
      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      console.log('[WorkflowExecutor] Technical indicator node completed:', parsedResult);
      return parsedResult;

    } catch (error) {
      console.error('[WorkflowExecutor] Technical indicator node failed:', error);
      throw error;
    }
  }

  private async executePythonAgentNode(node: Node, previousResults: Map<string, any>): Promise<any> {
    console.log('[WorkflowExecutor] Executing Python agent node:', node.id);
    // TODO: Implement Python agent execution
    await new Promise(resolve => setTimeout(resolve, 500));
    return { message: 'Python agent executed' };
  }

  private async executeDataSourceNode(node: Node, previousResults: Map<string, any>): Promise<any> {
    console.log('[WorkflowExecutor] Executing data source node:', node.id);
    // TODO: Implement data source execution
    await new Promise(resolve => setTimeout(resolve, 500));
    return { message: 'Data source executed' };
  }

  async saveDraft(
    name: string,
    description: string,
    nodes: Node[],
    edges: Edge[]
  ): Promise<string> {
    console.log('[WorkflowExecutor] Saving draft:', name);

    // Actually save the draft to workflowService
    const workflowId = await workflowService.saveDraft(
      name,
      description,
      nodes,
      edges
    );

    console.log('[WorkflowExecutor] Draft saved with ID:', workflowId);
    return workflowId;
  }
}

export const workflowExecutor = new WorkflowExecutor();
