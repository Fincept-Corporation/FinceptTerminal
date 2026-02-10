/**
 * Loop Node
 *
 * Iterate over arrays and execute downstream nodes for each item.
 * Supports for-each loops and index-based iteration.
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class LoopNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Repeat Items',
    name: 'loop',
    icon: 'fa:sync',
    group: ['Control Flow'],
    version: 1,
    description: 'Iterate over arrays and execute for each item',
    defaults: {
      name: 'Repeat Items',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'For Each Item', value: 'forEach' },
          { name: 'For Each Field in Item', value: 'forEachField' },
          { name: 'Repeat N Times', value: 'repeat' },
        ],
        default: 'forEach',
        description: 'Loop iteration mode',
      },
      {
        displayName: 'Field to Iterate',
        name: 'field',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            mode: ['forEachField'],
          },
        },
        description: 'Array field to iterate over',
      },
      {
        displayName: 'Number of Iterations',
        name: 'iterations',
        type: NodePropertyType.Number,
        default: 10,
        displayOptions: {
          show: {
            mode: ['repeat'],
          },
        },
        description: 'Number of times to repeat',
      },
      {
        displayName: 'Include Index',
        name: 'includeIndex',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include iteration index in output',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    const mode = this.getNodeParameter('mode', 0) as string;
    const includeIndex = this.getNodeParameter('includeIndex', 0) as boolean;

    if (mode === 'forEach') {
      // Iterate over each item
      items.forEach((item, index) => {
        returnData.push({
          json: {
            ...item.json,
            ...(includeIndex ? { _loopIndex: index } : {}),
          },
          pairedItem: index,
        });
      });
    } else if (mode === 'forEachField') {
      // Iterate over array field
      const field = this.getNodeParameter('field', 0) as string;

      for (let i = 0; i < items.length; i++) {
        const item = items[i];
        const arrayValue = getFieldValue(item.json, field);

        if (Array.isArray(arrayValue)) {
          arrayValue.forEach((value, idx) => {
            returnData.push({
              json: {
                value,
                ...(includeIndex ? { _loopIndex: idx } : {}),
                _originalItem: item.json,
              },
              pairedItem: i,
            });
          });
        }
      }
    } else if (mode === 'repeat') {
      // Repeat N times
      const iterations = this.getNodeParameter('iterations', 0) as number;

      for (let i = 0; i < items.length; i++) {
        const item = items[i];
        for (let j = 0; j < iterations; j++) {
          returnData.push({
            json: {
              ...item.json,
              ...(includeIndex ? { _loopIndex: j } : {}),
            },
            pairedItem: i,
          });
        }
      }
    }

    return [returnData];
  }
}

function getFieldValue(obj: any, path: string): any {
  const keys = path.split('.');
  let value = obj;
  for (const key of keys) {
    if (value === null || value === undefined) return undefined;
    value = value[key];
  }
  return value;
}
