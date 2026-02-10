/**
 * Manual Trigger Node
 * Starts workflow execution manually when user clicks "Run"
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class ManualTriggerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Start Workflow',
    name: 'manualTrigger',
    icon: 'file:trigger.svg',
    group: ['trigger'],
    version: 1,
    subtitle: 'Start workflow manually',
    description: 'Triggers workflow execution when you click the Run button',
    defaults: {
      name: 'Start Workflow',
      color: '#f59e0b',
    },
    inputs: [],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Notice',
        name: 'notice',
        type: NodePropertyType.Notice,
        default: '',
        description: 'This workflow will start when you click "Execute Workflow"',
      },
      {
        displayName: 'Pass Data',
        name: 'passData',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Pass custom data to the workflow',
      },
      {
        displayName: 'Custom Data',
        name: 'customData',
        type: NodePropertyType.Json,
        default: '{}',
        description: 'JSON data to pass to the workflow',
        displayOptions: {
          show: { passData: [true] },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const passData = this.getNodeParameter('passData', 0) as boolean;
    let customData = {};

    if (passData) {
      try {
        const customDataStr = this.getNodeParameter('customData', 0) as string;
        customData = JSON.parse(customDataStr);
      } catch (error) {
        console.warn('[ManualTrigger] Invalid custom data JSON, using empty object');
      }
    }

    return [[{
      json: {
        trigger: 'manual',
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
        ...customData,
      },
    }]];
  }
}
