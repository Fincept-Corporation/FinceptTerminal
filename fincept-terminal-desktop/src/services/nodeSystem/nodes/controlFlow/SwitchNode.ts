/**
 * Switch Node
 *
 * Multi-way routing based on field values.
 * Routes items to different outputs based on case matching.
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class SwitchNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Multi-Way Branch',
    name: 'switch',
    icon: 'fa:random',
    group: ['Control Flow'],
    version: 1,
    description: 'Route items based on field value matching',
    defaults: {
      name: 'Multi-Way Branch',
      color: '#6366f1',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      { type: NodeConnectionType.Main, displayName: 'Case 0' },
      { type: NodeConnectionType.Main, displayName: 'Case 1' },
      { type: NodeConnectionType.Main, displayName: 'Case 2' },
      { type: NodeConnectionType.Main, displayName: 'Default' },
    ],
    properties: [
      {
        displayName: 'Field to Match',
        name: 'field',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Field path to match (e.g., "status", "data.type")',
      },
      {
        displayName: 'Cases',
        name: 'cases',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: {},
        options: [
          {
            name: 'case',
            displayName: 'Case',
            values: [
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                description: 'Value to match',
              },
              {
                displayName: 'Output',
                name: 'output',
                type: NodePropertyType.Number,
                default: 0,
                description: 'Output index (0-2)',
              },
            ],
          },
        ],
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const outputs: any[][] = [[], [], [], []]; // 3 cases + default

    const field = this.getNodeParameter('field', 0) as string;
    const casesData = this.getNodeParameter('cases', 0, {}) as any;
    const cases = casesData.case || [];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const fieldValue = getFieldValue(item.json, field);
      let matched = false;

      // Check each case
      for (const caseRule of cases) {
        if (String(fieldValue) === String(caseRule.value)) {
          const outputIndex = Math.min(Math.max(caseRule.output, 0), 2);
          outputs[outputIndex].push({ ...item, pairedItem: i });
          matched = true;
          break;
        }
      }

      // Default output
      if (!matched) {
        outputs[3].push({ ...item, pairedItem: i });
      }
    }

    return outputs;
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
