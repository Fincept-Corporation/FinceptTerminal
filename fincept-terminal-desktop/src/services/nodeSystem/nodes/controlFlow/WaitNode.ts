/**
 * Wait/Delay Node
 *
 * Pause workflow execution for a specified duration.
 * Useful for rate limiting, scheduled delays, and time-based operations.
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class WaitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Delay Execution',
    name: 'wait',
    icon: 'fa:clock',
    group: ['Control Flow'],
    version: 1,
    description: 'Pause execution for a specified duration',
    defaults: {
      name: 'Delay Execution',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Wait Time',
        name: 'waitTime',
        type: NodePropertyType.Number,
        default: 1,
        description: 'How long to wait',
      },
      {
        displayName: 'Unit',
        name: 'unit',
        type: NodePropertyType.Options,
        options: [
          { name: 'Seconds', value: 'seconds' },
          { name: 'Minutes', value: 'minutes' },
          { name: 'Hours', value: 'hours' },
        ],
        default: 'seconds',
        description: 'Time unit',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const waitTime = this.getNodeParameter('waitTime', 0) as number;
    const unit = this.getNodeParameter('unit', 0) as string;

    // Convert to milliseconds
    let ms = waitTime * 1000;
    if (unit === 'minutes') ms *= 60;
    if (unit === 'hours') ms *= 3600;

    // Wait
    await new Promise(resolve => setTimeout(resolve, ms));

    return [items];
  }
}
