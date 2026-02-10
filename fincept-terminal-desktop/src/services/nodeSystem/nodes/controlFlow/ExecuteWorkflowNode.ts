import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';
import { workflowService } from '@/services/core/workflowService';
import { nodeExecutionManager } from '@/services/core/nodeExecutionManager';

export class ExecuteWorkflowNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Run Workflow',
    name: 'executeWorkflow',
    icon: 'fa:play-circle',
    group: ['controlFlow'],
    version: 1,
    description: 'Execute another workflow and return its results',
    defaults: { name: 'Run Workflow' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Workflow Selection',
        name: 'workflowSelection',
        type: NodePropertyType.Options,
        options: [
          { name: 'By ID', value: 'id' },
          { name: 'By Name', value: 'name' },
        ],
        default: 'id',
      },
      {
        displayName: 'Workflow ID',
        name: 'workflowId',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'ID of the workflow to execute',
        displayOptions: {
          show: {
            workflowSelection: ['id'],
          },
        },
      },
      {
        displayName: 'Workflow Name',
        name: 'workflowName',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Name of the workflow to execute',
        displayOptions: {
          show: {
            workflowSelection: ['name'],
          },
        },
      },
      {
        displayName: 'Pass Input Data',
        name: 'passInputData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Pass input data to the child workflow',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const workflowSelection = this.getNodeParameter('workflowSelection', i) as string;
        const passInputData = this.getNodeParameter('passInputData', i) as boolean;

        let workflow: any;

        if (workflowSelection === 'id') {
          const workflowId = this.getNodeParameter('workflowId', i) as string;
          const workflows = await workflowService.listWorkflows();
          workflow = workflows.find((w: any) => w.id === workflowId);
        } else {
          const workflowName = this.getNodeParameter('workflowName', i) as string;
          const workflows = await workflowService.listWorkflows();
          workflow = workflows.find((w: any) => w.name === workflowName);
        }

        if (!workflow) {
          throw new Error('Workflow not found');
        }

        // Execute the workflow
        const results: any[] = [];
        const statusCallback = (nodeId: string, status: string, result?: any) => {
          if (status === 'completed' && result) {
            results.push(result);
          }
        };

        await nodeExecutionManager.executeWorkflow(
          workflow.nodes,
          workflow.edges,
          statusCallback
        );

        // Flatten results from all nodes
        const flatResults = results.flat();

        if (flatResults.length > 0) {
          flatResults.forEach((result) => {
            returnData.push({ json: result, pairedItem: { item: i } });
          });
        } else {
          returnData.push({
            json: { message: 'Workflow executed successfully', workflowId: workflow.id },
            pairedItem: { item: i },
          });
        }
      } catch (error: any) {
        returnData.push({ json: { error: error.message }, pairedItem: { item: i } });
      }
    }

    return [returnData];
  }
}
