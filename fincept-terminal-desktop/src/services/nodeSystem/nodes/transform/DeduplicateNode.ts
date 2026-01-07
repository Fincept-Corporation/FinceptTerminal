import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class DeduplicateNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Deduplicate',
    name: 'deduplicate',
    icon: 'fa:filter',
    group: ['Transform'],
    version: 1,
    description: 'Remove duplicate items',
    defaults: {
      name: 'Deduplicate',
      color: '#ec4899',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Compare Fields',
        name: 'compareFields',
        type: NodePropertyType.String,
        default: '',
        description: 'Comma-separated field names to compare. Leave empty to compare all fields.',
      },
      {
        displayName: 'Keep',
        name: 'keep',
        type: NodePropertyType.Options,
        options: [
          { name: 'First Occurrence', value: 'first' },
          { name: 'Last Occurrence', value: 'last' },
        ],
        default: 'first',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const compareFieldsStr = this.getNodeParameter('compareFields', 0) as string;
    const keep = this.getNodeParameter('keep', 0) as string;

    const compareFields = compareFieldsStr
      ? compareFieldsStr.split(',').map(f => f.trim())
      : null;

    const seen = new Map<string, number>();
    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];

      // Create comparison key
      let key: string;
      if (compareFields) {
        const keyObj: any = {};
        for (const field of compareFields) {
          keyObj[field] = item.json[field];
        }
        key = JSON.stringify(keyObj);
      } else {
        key = JSON.stringify(item.json);
      }

      if (keep === 'first') {
        if (!seen.has(key)) {
          seen.set(key, outputItems.length);
          outputItems.push({ ...item, pairedItem: i });
        }
      } else {
        // Keep last - always update
        const existingIndex = seen.get(key);
        if (existingIndex !== undefined) {
          outputItems[existingIndex] = { ...item, pairedItem: i };
        } else {
          seen.set(key, outputItems.length);
          outputItems.push({ ...item, pairedItem: i });
        }
      }
    }

    return [outputItems];
  }
}
