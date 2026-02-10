import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class ErrorHandlerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Handle Errors',
    name: 'errorHandler',
    icon: 'fa:exclamation-triangle',
    group: ['Control Flow'],
    version: 1,
    description: 'Handle and transform errors',
    defaults: {
      name: 'Handle Errors',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      { type: NodeConnectionType.Main, displayName: 'Success' },
      { type: NodeConnectionType.Main, displayName: 'Error' },
    ],
    properties: [
      {
        displayName: 'Retry Count',
        name: 'retryCount',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Number of retries on error',
      },
      {
        displayName: 'Retry Delay (ms)',
        name: 'retryDelay',
        type: NodePropertyType.Number,
        default: 1000,
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const successItems: any[] = [];
    const errorItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      if (item.json.error) {
        errorItems.push({ ...item, pairedItem: i });
      } else {
        successItems.push({ ...item, pairedItem: i });
      }
    }

    return [successItems, errorItems];
  }
}
